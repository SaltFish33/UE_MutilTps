// 文件说明：
// APlayerCharacter 封装玩家控制的角色实体：相机、相机杆、输入绑定、拾取武器的逻辑与网络复制。
// 关键点：
// - 相机杆（SpringArm）附在网格（Mesh）上并使用 Controller 的旋转，使镜头跟随骨骼/角色朝向。
// - 使用 Enhanced Input 系统绑定动作（Move/Look/Jump/Equip），在 InitInputMapping 中将 MappingContext 添加到本地玩家子系统。
// - OverlappingWeapon 使用 ReplicatedUsing（OnRep_OverlappingWeapon）并在服务器上通过武器的 AreaSphere 回调设置，复制条件为 OwnerOnly（只复制给拥有者客户端）。
// - CombatComponent 在 PostInitializeComponents 中设置 PlayerCharacter 指针，以便组件引用角色进行 Attach 等操作。

#include "PlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "UE_MutilTPS/Componts/CombatComponent.h"
#include "UE_MutilTPS/PlayerCharacter/TurningInPlace.h"
#include "UE_MutilTPS/Weapon/WeaponBase.h"

// Constructor 注释：创建并初始化摄像机、摄像杆、Widget、CombatComponent 等组件。
// - SpringArm: 挂在 Mesh 上并以 Controller 旋转为基准，这样镜头朝向随控制器旋转，但角色本身通过 OrientRotationToMovement 控制朝向。
// - Camera: 附到 SpringArm，使其跟随相机杆。
// - OverHeadWidget: 用于显示在角色头顶（例如名字 / 状态），通过 UWidgetComponent 在蓝图/编辑器中设置 WidgetClass。
// - CombatComponent: 作为子组件创建，并设置为复制（SetIsReplicated(true)），但组件内部仍需配合角色逻辑保证服务器为状态权威来源。
APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 创建相机杆，设置长度，随Controller旋转
	this->SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	this->SpringArm->SetupAttachment(GetMesh());
	this->SpringArm->TargetArmLength = 600.0f;
	this->SpringArm->bUsePawnControlRotation = true;

	this->Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	this->Camera->SetupAttachment(this->SpringArm, USpringArmComponent::SocketName);

	this->bUseControllerRotationYaw = false;
	this->GetCharacterMovement()->bOrientRotationToMovement = true;

	this->OverHeadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverHeadWidget"));
	this->OverHeadWidget->SetupAttachment(RootComponent);

	this->CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	this->CombatComponent->SetIsReplicated(true);

	this->GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	this->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	this->TurningInPlaceType = ETurningInPlace::ETP_InPlace;

	SetNetUpdateFrequency(66.f);
	SetMinNetUpdateFrequency(33.f);
}

void APlayerCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// ReplicatedUsing = OnRep_OverlappingWeapon 在头文件中声明
	// COND_OwnerOnly：表示只把 OverlappingWeapon 的变化复制给拥有该 Pawn 的客户端
	// 为什么用 OwnerOnly：OverlappingWeapon 只是关于“这个玩家能否看到拾取提示”的局部信息，没必要广播给所有客户端以减少网络带宽。
	DOREPLIFETIME_CONDITION(APlayerCharacter, OverlappingWeapon, COND_OwnerOnly);
}

// SetOverlappingWeapon 说明：
// 目的：在玩家进入/离开武器拾取范围时更新 OverlappingWeapon 并在本地展示/隐藏拾取 Widget。
// 参数：Weapon - 要设置为重叠武器的指针（如果为 nullptr 表示离开范围）。
// 设计注意：
// - AreaSphere 的重叠回调在服务器上触发（由 AWeaponBase::OnSphereOverLap 调用），因此服务器会设置 OverlappingWeapon 并通过 RepNotify 将变化传给拥有者客户端。
// - 为提升单机/ListenServer 体验（服务器同时为本地玩家），这里也在 IsLocallyControlled() 为 true 时立即显示 Widget。
// - 在客户端本地路径（如果被错误调用）也提供容错处理：仅改变本地 UI，不修改服务器状态。
void APlayerCharacter::SetOverlappingWeapon(AWeaponBase* Weapon)
{
	if (this->OverlappingWeapon)
	{
		this->OverlappingWeapon->ShowPickUpWidget(false);
	}
	this->OverlappingWeapon = Weapon;
	if (IsLocallyControlled())
	{
		if (Weapon)
		{
			Weapon->ShowPickUpWidget(true);
		}
	}
}

// PostInitializeComponents 说明：
// 在组件初始化后（但尚未开始播放），将 PlayerCharacter 指针传给 CombatComponent。
// 这样组件可以安全使用角色的 Mesh、Socket 等资源去 Attach 武器或访问角色状态。
void APlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (this->CombatComponent)
	{
		this->CombatComponent->PlayerCharacter = this;
	}
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	this->InitInputMapping();
}

void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	this->SetAimOffset(DeltaTime);
}

// Input 绑定说明（SetupPlayerInputComponent / InitInputMapping）：
// - 使用 Enhanced Input：需先把 MappingContext 添加到 UEnhancedInputLocalPlayerSubsystem（InitInputMapping）。
// - BindAction 的 ETriggerEvent：Move/Look 使用 Triggered（持续触发以便获取轴输入），Jump 使用 Started/Completed 模拟按下/松开事件。
// - EquipAction 使用 Triggered（一次）。如果 Equip 行为应当由服务器验证（例如拾取时要确保服务器权威），应在 EquipWeapon 调用前做权限检查或通过 Server RPC 执行。
void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);
		}
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);
		}

		// 绑定跳跃动作（按下开始跳跃，松开停止跳跃）
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &APlayerCharacter::StartJump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &APlayerCharacter::StopJump);
		}
		if (EquipAction)
		{
			EnhancedInputComponent->BindAction(EquipAction, ETriggerEvent::Triggered, this, &APlayerCharacter::EquipWeapon);
		}
		if (CrouchAction)
		{
			EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &APlayerCharacter::PressCrouch);
		}
		if (AimingAction)
		{
			EnhancedInputComponent->BindAction(AimingAction, ETriggerEvent::Started, this, &APlayerCharacter::PressAiming);
			EnhancedInputComponent->BindAction(AimingAction, ETriggerEvent::Completed, this, &APlayerCharacter::ReleaseAiming);
		}
	}

}

void APlayerCharacter::InitInputMapping()
{
	// 仅在本地控制的客户端执行
	if (!IsLocallyControlled() || !InputMappingContext) return;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	if (!LocalPlayer) return;

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (Subsystem)
	{
		Subsystem->AddMappingContext(InputMappingContext, 0);
	}
}

void APlayerCharacter::Move(const struct FInputActionValue& Value)
{
	FVector2D MoveVector = Value.Get<FVector2D>();
	if (Controller)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MoveVector.Y);
		AddMovementInput(RightDirection, MoveVector.X);
	}
}

void APlayerCharacter::Look(const struct FInputActionValue& Value)
{
	FVector2D LookVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookVector.X);
	AddControllerPitchInput(LookVector.Y);
}

// 跳跃回调的简单说明：StartJump / StopJump 只是转发给 Character 的 Jump / StopJumping。
// 使用 Enhanced Input 的 Started/Completed 事件分别映射到开始与结束跳跃。
void APlayerCharacter::StopJump()
{
    StopJumping();
}

// EquipWeapon 说明：
// - 该函数在玩家按下拾取键时被调用（客户端输入触发）。为了保证游戏状态由服务器维护，实际装备逻辑由服务器执行：
//   在当前实现中，EquipWeapon() 只会在 HasAuthority() 为 true 时调用 CombatComponent->EquipWeapon。
// - 如果你需要客户端请求服务器拾取，应实现一个 Server RPC（例如 ServerEquipWeapon）在服务器端校验并调用组件 EquipWeapon。
// - 参数：无（使用 OverlappingWeapon 成员做目标），注意 OverlappingWeapon 由服务器通过重叠回调设置并复制到拥有者客户端。
void APlayerCharacter::EquipWeapon()
{
	if (OverlappingWeapon && CombatComponent)
	{
		if (HasAuthority())
		{
			CombatComponent->EquipWeapon(OverlappingWeapon);
		} else
		{
			this->ServerPressEquipWeapon();
		}
		
	}
}

void APlayerCharacter::PressCrouch()
{
	if (bIsCrouched)
	{
		UnCrouch();
	} else
	{
		Crouch();
	}
}

void APlayerCharacter::PressAiming()
{
	if (this->CombatComponent)
	{
		this->CombatComponent->SetAiming(true);
	}
}

void APlayerCharacter::ReleaseAiming()
{
	if (this->CombatComponent)
	{
		this->CombatComponent->SetAiming(false);
	}
}

void APlayerCharacter::SetAimOffset(float DeltaTime)
{
	FVector Velocity = this->GetVelocity();
	FVector Lateral = FVector(Velocity.X, Velocity.Y, 0.f);
	float Speed = Lateral.Size();
	bool IsInAir = this->GetCharacterMovement()->IsFalling();
	
	if (Speed != 0.f || IsInAir)
	{
		AO_Yaw = 0.f;
		this->LastFrameRotation = FRotator(0.f, this->GetBaseAimRotation().Yaw, 0.f);
		this->bUseControllerRotationYaw = true;
		//this->GetCharacterMovement()->bOrientRotationToMovement = false;
		this->TurningInPlaceType = ETurningInPlace::ETP_InPlace;
	}
	else 
	{
		FRotator AimRotation = FRotator(0.f, this->GetBaseAimRotation().Yaw, 0.f);
		float TargetAOYaw = UKismetMathLibrary::NormalizedDeltaRotator(AimRotation, this->LastFrameRotation).Yaw;
		// TargetAOYaw = FMath::Clamp(TargetAOYaw, -90.f, 90.f);
		// AO_Yaw = FMath::FInterpTo(AO_Yaw, TargetAOYaw, DeltaTime, 6.f);
		AO_Yaw = TargetAOYaw;
		if (TurningInPlaceType == ETurningInPlace::ETP_InPlace)
		{
			InterpAO_Yaw = AO_Yaw;
		}
		SetTurningInPlace(DeltaTime);
		this->bUseControllerRotationYaw = false;
		this->GetCharacterMovement()->bOrientRotationToMovement = true;
	}

	AO_Pitch = this->GetBaseAimRotation().Pitch;
	if (AO_Pitch > 90.f && !IsLocallyControlled())
	{
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = UKismetMathLibrary::MapRangeClamped(AO_Pitch, InRange.X, InRange.Y, OutRange.X, OutRange.Y);
	}
	
}

void APlayerCharacter::SetTurningInPlace(float DeltaTime)
{
	if (AO_Yaw > 90.f)
	{
		this->TurningInPlaceType = ETurningInPlace::ETP_TurnRight;
	}
	else if (AO_Yaw < -90.f)
	{
		this->TurningInPlaceType = ETurningInPlace::ETP_TurnLeft;
	}
	if (this->TurningInPlaceType != ETurningInPlace::ETP_InPlace)
	{
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 6.f);
		AO_Yaw = InterpAO_Yaw;
		if (FMath::Abs(AO_Yaw) < 15.f)
		{
			this->TurningInPlaceType = ETurningInPlace::ETP_InPlace;
			this->LastFrameRotation = FRotator(0.f, this->GetBaseAimRotation().Yaw, 0.f);
		}
	}
}

void APlayerCharacter::ServerPressEquipWeapon_Implementation()
{
	CombatComponent->EquipWeapon(OverlappingWeapon);
}

// OnRep_OverlappingWeapon 说明：
// - RepNotify 回调：当 OverlappingWeapon 在拥有者客户端发生变化时触发。
// - 参数 LastWeapon：用于在通知时隐藏上一个武器的拾取提示（如果有）。
// - 设计：服务器更新 OverlappingWeapon（OwnerOnly），客户端接收到后仅用于本地 UI 更新（例如显示/隐藏 PickUpWidget）。
void APlayerCharacter::OnRep_OverlappingWeapon(AWeaponBase* LastWeapon)
{
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickUpWidget(true);
	}
	if (LastWeapon)
	{
		LastWeapon->ShowPickUpWidget(false);
	}
}

void APlayerCharacter::StartJump()
{
    Jump();
}
