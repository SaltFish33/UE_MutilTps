// 文件说明：
// CombatComponent 负责角色的战斗相关逻辑（当前简化为装备武器的功能）。
// - 本组件绑定到玩家角色（PlayerCharacter）上，保存当前已装备武器指针（EquippedWeapon）。
// - 设计原则：武器的“装备/卸下”逻辑需要处理视觉 Attach（Socket）、状态切换、以及 UI（拾取提示）变化。
// - 注意：实际的网络同步（例如谁可以调用 EquipWeapon）在调用端（如 PlayerCharacter）应由权威性（HasAuthority）控制；本组件本身标记为可复制，但 EquipWeapon 的调用位置决定了是否需要额外的 RPC。

#include "CombatComponent.h"

#include "PlayerCharacter.h"
#include "PlayerCharacterController.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Camera/CameraComponent.h"
#include "UE_MutilTPS/Animation/PlayerAnimInstance.h"
#include "UE_MutilTPS/Weapon/WeaponBase.h"
#include "UE_MutilTPS/Widget/PlayerHUD.h"
#include "UE_MutilTPS/Interfaces/InteractWithCrosshairInterface.h"

#define TRACE_LENGTH 8000.f

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	NormalMaxWalkSpeed = 600.f;
	AimingMaxWalkSpeed = 450.f;
	bIsFiring = false;
	CurrentTargetFOV = DefaultFOV;
	CurrentCrosshairShootingFactor = 0.0f;
	bIsAimingAtInteractable = false;
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	DOREPLIFETIME(UCombatComponent, bIsAiming);
	DOREPLIFETIME(UCombatComponent, TraceHitTarget);
}


void UCombatComponent::CustomTick(float DeltaTime)
{
	this->SetPlayerHUD(DeltaTime);
	this->UpdateCameraFOV(DeltaTime);
	
	// 只在本地控制的客户端执行射线检测（需要相机和视口信息）
	if (this->PlayerCharacter->IsLocallyControlled())
	{
		FHitResult HitResult;
		this->TickGetTraceHitRaycast(HitResult);
		
		// 如果是在服务器上（Listen Server），直接更新TraceHitTarget
		// 如果是客户端，需要通过Server RPC发送到服务器，但本地也立即更新以避免延迟
		if (this->PlayerCharacter->HasAuthority())
		{
			this->TraceHitTarget = HitResult.ImpactPoint;
		}
		else
		{
			// 客户端：立即更新本地值（用于本地显示），同时通过Server RPC发送到服务器（用于网络同步）
			this->TraceHitTarget = HitResult.ImpactPoint;
			this->ServerUpdateTraceHitTarget(HitResult.ImpactPoint);
		}
		
		// 检测是否瞄准到实现了接口的对象
		if (HitResult.bBlockingHit && HitResult.GetActor())
		{
			// 检查命中的Actor是否实现了IInteractWithCrosshairInterface接口
			this->bIsAimingAtInteractable = HitResult.GetActor()->GetClass()->ImplementsInterface(UInteractWithCrosshairInterface::StaticClass());
		}
		else
		{
			this->bIsAimingAtInteractable = false;
		}
	}
}


void UCombatComponent::SetPlayerHUD(float DeltaTime)
{
	if (this->PlayerCharacter == nullptr || this->PlayerCharacter->Controller == nullptr) return;
	if (this->PlayerCharacterController == nullptr)
	{
		this->PlayerCharacterController = Cast<APlayerCharacterController>(this->PlayerCharacter->Controller);
	}
	if (this->PlayerHUD == nullptr)
	{
		this->PlayerHUD = Cast<APlayerHUD>(this->PlayerCharacterController->GetHUD());
	}
	if (this->PlayerHUD)
	{
		FCrosshairData HUDPackage;
		HUDPackage.CrosshairsCenter = this->EquippedWeapon ? this->EquippedWeapon->CrosshairsCenter : nullptr;
		HUDPackage.CrosshairsLeft = this->EquippedWeapon ? this->EquippedWeapon->CrosshairsLeft : nullptr;
		HUDPackage.CrosshairsRight = this->EquippedWeapon ? this->EquippedWeapon->CrosshairsRight : nullptr;
		HUDPackage.CrosshairsTop = this->EquippedWeapon ? this->EquippedWeapon->CrosshairsTop : nullptr;
		HUDPackage.CrosshairsBottom = this->EquippedWeapon ? this->EquippedWeapon->CrosshairsBottom : nullptr;
		
		// 计算准星扩散
		// 移动时，通过速度计算，拿到玩家最大速度，映射到[0,1]之间，然后设置
		FVector2D VelocityFactor = FVector2D(0.f, PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed);
		FVector2D TargetRange = FVector2D(0.f, 1.f);
		FVector PlayerVelocity = PlayerCharacter->GetVelocity();
		PlayerVelocity.Z = 0;
		this->CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(VelocityFactor, TargetRange, PlayerVelocity.Size());
		
		// 空中时，插值计算
		if (PlayerCharacter->GetCharacterMovement()->IsFalling())
		{
			this->CrosshairInAirFactor = FMath::FInterpTo(this->CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f);
		} else
		{
			this->CrosshairInAirFactor = FMath::FInterpTo(this->CrosshairInAirFactor, 0.f, DeltaTime, 30.f);
		}
		
		// ========== 瞄准时的准星缩小 ==========
		// 瞄准时，准星会变得更紧凑（扩散值减小），提供更精确的瞄准体验
		// 原理：将基础扩散值乘以瞄准因子（小于1的值），使准星缩小
		// 例如：CrosshairAimingFactor = 0.5，则瞄准时扩散值减半
		float AimingFactor = 1.0f;
		if (this->bIsAiming)
		{
			AimingFactor = this->CrosshairAimingFactor;
		}
		
		// ========== 开火时的准星扩大 ==========
		// 开火时，准星会扩大（扩散值增大），模拟后坐力和射击精度下降
		// 原理：在基础扩散值上增加开火因子，开火时快速增加，停止开火后逐渐恢复
		if (this->bIsFiring)
		{
			// 开火时，将开火因子快速插值到目标值（CrosshairShootingFactor）
			// 使用2倍恢复速度，使开火效果更明显
			this->CurrentCrosshairShootingFactor = FMath::FInterpTo(
				this->CurrentCrosshairShootingFactor, 
				this->CrosshairShootingFactor, 
				DeltaTime, 
				this->CrosshairShootingRecoverySpeed * 2.0f
			);
		}
		else
		{
			// 停止开火后，逐渐恢复准星（插值回0）
			// 使用正常的恢复速度，提供平滑的恢复效果
			this->CurrentCrosshairShootingFactor = FMath::FInterpTo(
				this->CurrentCrosshairShootingFactor, 
				0.0f, 
				DeltaTime, 
				this->CrosshairShootingRecoverySpeed
			);
		}
		
		// ========== 计算最终准星扩散值 ==========
		// 公式：最终扩散 = (速度因子 + 空中因子) * 瞄准因子 + 开火因子
		// - 速度因子：移动速度越大，扩散越大
		// - 空中因子：在空中时，扩散增大
		// - 瞄准因子：瞄准时，基础扩散值减小（乘以小于1的因子，使准星更紧凑）
		// - 开火因子：开火时，在基础扩散上增加开火扩散值（使准星扩大）
		float BaseSpread = (this->CrosshairVelocityFactor + this->CrosshairInAirFactor) * AimingFactor;
		HUDPackage.CrosshairSpread = BaseSpread + this->CurrentCrosshairShootingFactor;

		// 设置是否瞄准到可交互对象（用于准星变红）
		HUDPackage.bIsAimingAtInteractable = this->bIsAimingAtInteractable;

		this->PlayerHUD->SetHUDPackage(HUDPackage);
	}
	
}
void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	this->SetComponentTickEnabled(true);
	if (PlayerCharacter)
	{
		PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed = NormalMaxWalkSpeed;
		// 初始化FOV为默认值
		if (PlayerCharacter->IsLocallyControlled() && PlayerCharacter->Camera)
		{
			CurrentTargetFOV = DefaultFOV;
			PlayerCharacter->Camera->SetFieldOfView(DefaultFOV);
		}
	}
	
}

void UCombatComponent::OnRep_EquippedWeapon()
{
	if (this->EquippedWeapon && PlayerCharacter)
	{
		// 客户端复制武器后，需要重新附着武器到Socket，确保旋转同步
		// 这是因为AttachActor操作只在服务器执行，客户端需要通过RepNotify重新执行
		const USkeletalMeshSocket* RightHandSocket = PlayerCharacter->GetMesh()->GetSocketByName(FName("RightHandSocket"));
		if (RightHandSocket)
		{
			// 在客户端重新附着武器，确保武器旋转与服务器同步
			RightHandSocket->AttachActor(EquippedWeapon, PlayerCharacter->GetMesh());
		}
		
		PlayerCharacter->GetCharacterMovement()->bOrientRotationToMovement = false;
		PlayerCharacter->bUseControllerRotationYaw = true;
		// 客户端复制武器后，根据当前瞄准状态更新FOV
		if (PlayerCharacter->IsLocallyControlled())
		{
			this->SetTargetFOV(this->bIsAiming);
		}
	}
	else
	{
		// 卸下武器后，恢复默认FOV
		if (PlayerCharacter && PlayerCharacter->IsLocallyControlled())
		{
			this->SetTargetFOV(false);
		}
	}
}


void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// EquipWeapon 说明：
// 参数：Weapon - 要装备的武器指针（AWeaponBase*），外部（通常是服务器）传入。
// 行为：
// 1. 如果已有武器，则先将其状态标记为 Dropped（EWS_Dropped），以便在游戏逻辑/外观上区分“已卸下”的武器。
// 2. 将成员 EquippedWeapon 指向新武器，并设置状态为 Equipped（EWS_Equipped）。
// 3. 查找角色 Mesh 上的名为 "RightHandSocket" 的插槽，并把 Weapon Actor 以该插槽为父附着，使武器随骨骼动画移动。
// 4. 调用 SetOwner 将武器的 Owner 指向角色（这有助于权限判断及后续的复制/销毁行为）。
// 5. 隐藏武器上的拾取提示 Widget（因为装备后不应再提示可拾取）。
// 设计理由与注意事项：
// - AttachActor 使用 Socket 可以保证武器与骨骼动画同步（例如手部握持），而不是简单移动 Actor 的世界位置。
// - SetWeaponState 的转换用于在其它逻辑中判断武器当前行为（可被拾取、在手中、掉落物理存在等）。
// - Widget 的显示仅作视觉提示，通常由客户端展示，但在服务器驱动的流程中，服务器修改状态并通过复制/通知使客户端更新 UI。
// - 为避免空指针崩溃，函数入口对 PlayerCharacter 与 Weapon 做了空指针校验。
// - 如果需要支持客户端请求装备（比如按键触发），应通过 Server RPC 在服务器端执行 EquipWeapon，从而保证服务器为权威来源。
void UCombatComponent::EquipWeapon(AWeaponBase* Weapon)
{
	if (PlayerCharacter == nullptr || Weapon == nullptr)
	{
		return;
	}

	if (this->EquippedWeapon)
	{
		this->EquippedWeapon->SetWeaponState(EWeaponState::EWS_Dropped);
	}
	this->EquippedWeapon = Weapon;
	this->EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
	const USkeletalMeshSocket* RightHandSocket = this->PlayerCharacter->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if (RightHandSocket)
	{
		RightHandSocket->AttachActor(EquippedWeapon, this->PlayerCharacter->GetMesh());
	}
	this->EquippedWeapon->SetOwner(PlayerCharacter);
	PlayerCharacter->GetCharacterMovement()->bOrientRotationToMovement = false;
	PlayerCharacter->bUseControllerRotationYaw = true;
	
	// 装备武器后，根据当前瞄准状态更新FOV（仅在本地控制的客户端执行）
	if (PlayerCharacter->IsLocallyControlled())
	{
		this->SetTargetFOV(this->bIsAiming);
	}
}

void UCombatComponent::SetAiming(bool IsAiming)
{
	// 检查是否装备了武器，未装备武器时不允许瞄准
	if (IsAiming && (!EquippedWeapon || EquippedWeapon == nullptr))
	{
		return;
	}
	
	this->bIsAiming = IsAiming;
	if (!PlayerCharacter->HasAuthority())
	{
		this->ServerSetAiming(IsAiming);
	}
	if (PlayerCharacter)
	{
		PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed = this->bIsAiming ? AimingMaxWalkSpeed : NormalMaxWalkSpeed;
	}
	// 设置目标FOV（仅在本地控制的客户端执行，因为FOV是本地视觉效果）
	if (PlayerCharacter && PlayerCharacter->IsLocallyControlled())
	{
		this->SetTargetFOV(IsAiming);
	}
}

void UCombatComponent::ServerSetAiming_Implementation(bool IsAiming)
{
	// 检查是否装备了武器，未装备武器时不允许瞄准
	if (IsAiming && (!EquippedWeapon || EquippedWeapon == nullptr))
	{
		return;
	}
	
	this->bIsAiming = IsAiming;
	if (PlayerCharacter)
	{
		PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed = this->bIsAiming ? AimingMaxWalkSpeed : NormalMaxWalkSpeed;
	}
}

void UCombatComponent::ServerUpdateTraceHitTarget_Implementation(const FVector_NetQuantize& HitTarget)
{
	// 服务器接收客户端发送的TraceHitTarget并更新，然后复制到所有客户端
	this->TraceHitTarget = HitTarget;
}

void UCombatComponent::FireButtonPressed(bool bIsPressed)
{
	this->bIsFiring = bIsPressed;
	if (bIsPressed && this->EquippedWeapon)
	{
		FHitResult HitResult;
		this->TickGetTraceHitRaycast(HitResult);
		this->ServerFireButtonPressed(HitResult.ImpactPoint);
	}
}

void UCombatComponent::TickGetTraceHitRaycast(FHitResult& OutHitResult)
{
	FVector2D ViewCenterPos;
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	ViewCenterPos.X = (ViewportSize.X / 2.f);
	ViewCenterPos.Y = (ViewportSize.Y / 2.f);
	FVector WorldPosition;
	FVector WorldDirection;
	bool bSceneToWorld = UGameplayStatics::DeprojectScreenToWorld(UGameplayStatics::GetPlayerController(GetWorld(), 0), ViewCenterPos, WorldPosition, WorldDirection);
	if (bSceneToWorld)
	{
		// 将起始点从相机位置向前推进50厘米，避免射线打到玩家身后的对象
		const float TraceStartOffset = 50.0f;
		FVector Start = WorldPosition + WorldDirection * TraceStartOffset;
		FVector End = Start + WorldDirection * TRACE_LENGTH;
		
		// 忽略玩家自身，避免射线打到玩家自己的碰撞体
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(PlayerCharacter);
		
		GetWorld()->LineTraceSingleByChannel(OutHitResult, Start, End, ECC_Visibility, QueryParams);
		if (!OutHitResult.bBlockingHit)
		{
			OutHitResult.ImpactPoint = End;
		}
	}
}

void UCombatComponent::ServerFireButtonPressed_Implementation(const FVector_NetQuantize& HitTarget)
{
	this->MulticastFire(HitTarget);
}

void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& HitTarget)
{
	if (this->EquippedWeapon && this->EquippedWeapon->FireMontage && PlayerAnimInstance)
	{
		PlayerAnimInstance->Montage_Play(this->EquippedWeapon->FireMontage);
		FName FireSection = this->bIsAiming ? FName("Rifle_Aim") : FName("Rifle_Hip");
		PlayerAnimInstance->Montage_JumpToSection(FireSection);
		
		this->EquippedWeapon->Fire(HitTarget);
	}
}

// SetTargetFOV 说明：
// 根据瞄准状态和当前装备的武器设置目标FOV。
// - 如果武器设置了自定义FOV，则使用武器的FOV值
// - 如果武器没有设置FOV（值为0），则使用CombatComponent的默认FOV值
// - 仅在本地控制的客户端执行，因为FOV是本地视觉效果
void UCombatComponent::SetTargetFOV(bool bCurIsAiming)
{
	if (!PlayerCharacter || !PlayerCharacter->IsLocallyControlled())
	{
		return;
	}

	float TargetFOV = DefaultFOV;

	if (bCurIsAiming)
	{
		// 瞄准状态：优先使用武器的瞄准FOV，如果为0则使用默认瞄准FOV
		if (EquippedWeapon && EquippedWeapon->AimingFOV > 0.0f)
		{
			TargetFOV = EquippedWeapon->AimingFOV;
		}
		else
		{
			TargetFOV = DefaultAimingFOV;
		}
	}
	else
	{
		// 非瞄准状态：优先使用武器的默认FOV，如果为0则使用CombatComponent的默认FOV
		if (EquippedWeapon && EquippedWeapon->DefaultFOV > 0.0f)
		{
			TargetFOV = EquippedWeapon->DefaultFOV;
		}
		else
		{
			TargetFOV = DefaultFOV;
		}
	}

	CurrentTargetFOV = TargetFOV;
}

// UpdateCameraFOV 说明：
// 每帧调用，通过插值平滑地将相机FOV过渡到目标FOV。
// - 使用FInterpTo进行平滑插值，避免FOV突然变化
// - 仅在本地控制的客户端执行，因为FOV是本地视觉效果
void UCombatComponent::UpdateCameraFOV(float DeltaTime)
{
	if (!PlayerCharacter || !PlayerCharacter->IsLocallyControlled() || !PlayerCharacter->Camera)
	{
		return;
	}

	float CurrentFOV = PlayerCharacter->Camera->FieldOfView;
	float NewFOV = FMath::FInterpTo(CurrentFOV, CurrentTargetFOV, DeltaTime, FOVInterpSpeed);
	PlayerCharacter->Camera->SetFieldOfView(NewFOV);
}
