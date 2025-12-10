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
#include "UE_MutilTPS/Animation/PlayerAnimInstance.h"
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
	
	// 初始化代码驱动旋转相关变量
	this->TargetRotationDelta = 0.0f;
	this->bIsRotatingCharacter = false;

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
		this->CombatComponent->PlayerAnimInstance = Cast<UPlayerAnimInstance>(this->GetMesh()->GetAnimInstance());
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
	if (this->CombatComponent)
	{
		this->CombatComponent->CustomTick(DeltaTime);
	}
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
		if (FireAction)
		{
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &APlayerCharacter::PressFire);
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &APlayerCharacter::ReleaseFire);
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

void APlayerCharacter::PressFire()
{
	if (this->CombatComponent)
	{
		this->CombatComponent->FireButtonPressed(true);
	}
}

void APlayerCharacter::ReleaseFire()
{
	if (this->CombatComponent)
	{
		this->CombatComponent->FireButtonPressed(false);
	}
}

// SetAimOffset 说明：
// 此函数每帧调用，负责计算角色的瞄准偏移（Aim Offset）角度，并触发代码驱动旋转。
//
// 主要功能：
// 1. 计算 AO_Yaw：角色朝向与瞄准方向之间的角度差
// 2. 检测是否需要代码驱动旋转：当 AO_Yaw 超过阈值时触发
// 3. 管理旋转状态：在旋转期间保护旋转过程不被其他系统干扰
//
// 与旋转系统的关系：
// - 此函数计算 AO_Yaw，SetTurningInPlace 检测是否需要旋转
// - RotateCharacterForTurning 执行实际的旋转逻辑
// - 三者配合实现完整的代码驱动旋转系统
//
// AO_Yaw 的计算原理：
// AO_Yaw = NormalizedDeltaRotator(瞄准方向, 角色朝向).Yaw
// - 正值：角色需要向右转才能面向瞄准方向
// - 负值：角色需要向左转才能面向瞄准方向
// - 绝对值越大，需要旋转的角度越大
void APlayerCharacter::SetAimOffset(float DeltaTime)
{
	if (this->CombatComponent->EquippedWeapon == nullptr)
	{
		return;
	}
	// 计算角色在水平面上的移动速度（忽略垂直速度）
	// 这用于判断角色是否在移动，移动时不需要进行原地转向检测
	FVector Velocity = this->GetVelocity();
	FVector Lateral = FVector(Velocity.X, Velocity.Y, 0.f);
	float Speed = Lateral.Size();
	bool IsInAir = this->GetCharacterMovement()->IsFalling();
	
	// ========== 情况1：角色正在移动或处于空中 ==========
	// 当角色移动时，不需要进行原地转向检测，因为：
	// 1. 移动时角色应该跟随移动方向旋转（由 bOrientRotationToMovement 控制）
	// 2. 移动时的转向由移动系统处理，不需要代码驱动旋转
	// 3. 空中时也不适合进行原地转向，因为角色可能处于不稳定状态
	if (Speed != 0.f || IsInAir)
	{
		// 重置 AO_Yaw 相关状态
		AO_Yaw = 0.f;
		// 更新 LastFrameRotation 为当前瞄准方向，为下一帧计算做准备
		this->LastFrameRotation = FRotator(0.f, this->GetBaseAimRotation().Yaw, 0.f);
		// 移动时允许控制器控制角色旋转
		this->bUseControllerRotationYaw = true;
		// 禁用自动朝向移动方向，让控制器控制旋转
		this->GetCharacterMovement()->bOrientRotationToMovement = false;
		this->TurningInPlaceType = ETurningInPlace::ETP_InPlace;
	}
	// ========== 情况2：角色静止在地面上 ==========
	// 这是进行原地转向检测和代码驱动旋转的主要场景
	else 
	{
		// 计算 AO_Yaw：角色朝向与瞄准方向之间的角度差
		// AimRotation：当前瞄准方向（控制器的 Yaw 旋转）
		// LastFrameRotation：上一帧角色的朝向（Yaw）
		// NormalizedDeltaRotator：计算两个旋转之间的差值，结果在 [-180, 180] 范围内
		FRotator AimRotation = FRotator(0.f, this->GetBaseAimRotation().Yaw, 0.f);
		float TargetAOYaw = UKismetMathLibrary::NormalizedDeltaRotator(AimRotation, this->LastFrameRotation).Yaw;
		
		// 直接使用计算出的角度差作为 AO_Yaw（不使用插值，确保响应及时）
		// 注释掉的代码是之前的插值版本，现在改为直接赋值以获得更快的响应
		// TargetAOYaw = FMath::Clamp(TargetAOYaw, -90.f, 90.f);
		// AO_Yaw = FMath::FInterpTo(AO_Yaw, TargetAOYaw, DeltaTime, 6.f);
		AO_Yaw = TargetAOYaw;
		
		// 如果角色处于"原地"状态（不是正在转向），更新 InterpAO_Yaw
		// 这为后续的平滑插值提供基准值
		if (TurningInPlaceType == ETurningInPlace::ETP_InPlace)
		{
			InterpAO_Yaw = AO_Yaw;
		}
		UE_LOG(LogTemp, Log, TEXT("AO_Yaw: %f"), AO_Yaw);
		
		// 调用转向检测和处理函数
		// 如果 AO_Yaw 超过阈值，会触发代码驱动旋转
		SetTurningInPlace(DeltaTime);
		
		// 如果正在通过代码旋转，保持禁用 bOrientRotationToMovement
		// 这样可以避免 CharacterMovementComponent 干扰我们的手动旋转
		// 如果不在旋转，则启用 bOrientRotationToMovement，允许角色正常响应移动
		if (!bIsRotatingCharacter)
		{
			this->bUseControllerRotationYaw = false;
			this->GetCharacterMovement()->bOrientRotationToMovement = true;
		}
	}

	AO_Pitch = this->GetBaseAimRotation().Pitch;
	if (AO_Pitch > 90.f && !IsLocallyControlled())
	{
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = UKismetMathLibrary::MapRangeClamped(AO_Pitch, InRange.X, InRange.Y, OutRange.X, OutRange.Y);
	}
	
}

// SetTurningInPlace 说明：
// 此函数负责检测角色是否需要原地转向，并决定使用代码驱动旋转还是动画系统处理。
// 
// AO_Yaw 的含义：
// - AO_Yaw 表示角色当前朝向（LastFrameRotation）与瞄准方向（Controller旋转）之间的Yaw角度差
// - 正值表示角色需要向右转才能面向瞄准方向，负值表示需要向左转
// - 当角色静止且玩家快速转动视角时，AO_Yaw 会增大
//
// 为什么需要代码驱动旋转：
// - 当 AO_Yaw 超过阈值（默认90度）时，仅靠动画系统的 RotateRootBone 可能无法快速响应
// - 通过代码直接旋转整个 Actor，可以确保角色及时转向，避免"背对瞄准方向"的尴尬情况
// - 这种方式提供了更精确的控制和更快的响应速度
void APlayerCharacter::SetTurningInPlace(float DeltaTime)
{
	// 如果正在通过代码旋转角色，直接执行旋转逻辑，避免重复检测
	// 这样可以确保旋转过程的连续性，不会被其他逻辑打断
	if (bIsRotatingCharacter)
	{
		RotateCharacterForTurning(DeltaTime);
		return;
	}
	
	// 检测是否需要向右转向
	// 当 AO_Yaw 超过正阈值时，说明角色需要向右旋转才能面向瞄准方向
	if (AO_Yaw > TurningThresholdAngle)
	{
		this->TurningInPlaceType = ETurningInPlace::ETP_TurnRight;
		
		// 计算需要旋转的总角度
		// TargetRotationDelta 存储了需要旋转的角度（单位：度）
		// 我们旋转整个 AO_Yaw，使角色完全朝向瞄准方向（AO_Yaw 变为 0）
		// 例如：如果 AO_Yaw = 120度，我们需要旋转 120度使角色面向瞄准方向
		TargetRotationDelta = AO_Yaw;
		
		// 标记开始代码驱动旋转
		bIsRotatingCharacter = true;
		
		// 关键：禁用 CharacterMovementComponent 的自动旋转功能
		// bOrientRotationToMovement = true 时，系统会根据移动方向自动旋转角色
		// 这会与我们的手动旋转产生冲突，导致旋转被覆盖或抖动
		// bUseControllerRotationYaw = false 确保控制器不会直接控制角色旋转
		this->GetCharacterMovement()->bOrientRotationToMovement = false;
		this->bUseControllerRotationYaw = false;
		
		// 立即执行第一帧的旋转
		RotateCharacterForTurning(DeltaTime);
	}
	// 检测是否需要向左转向
	// 当 AO_Yaw 小于负阈值时，说明角色需要向左旋转
	else if (AO_Yaw < -TurningThresholdAngle)
	{
		this->TurningInPlaceType = ETurningInPlace::ETP_TurnLeft;
		
		// 计算需要旋转的总角度（注意 AO_Yaw 此时是负数）
		// 例如：如果 AO_Yaw = -120度，TargetRotationDelta = -120度，表示需要向左旋转 120度
		TargetRotationDelta = AO_Yaw;
		
		// 标记开始代码驱动旋转
		bIsRotatingCharacter = true;
		
		// 同样需要禁用自动旋转功能
		this->GetCharacterMovement()->bOrientRotationToMovement = false;
		this->bUseControllerRotationYaw = false;
		
		// 立即执行第一帧的旋转
		RotateCharacterForTurning(DeltaTime);
	}
	// 如果 AO_Yaw 在阈值范围内，但之前处于转向状态，使用平滑插值回到正常状态
	// 这种情况发生在旋转完成后，AO_Yaw 已经小于阈值，但还没有完全归零
	else if (this->TurningInPlaceType != ETurningInPlace::ETP_InPlace)
	{
		// 使用插值平滑地将 AO_Yaw 归零
		// FInterpTo 会在每帧将 InterpAO_Yaw 向目标值（0）插值，插值速度为 6.0
		// 这提供了平滑的过渡效果，避免突然的数值跳跃
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 6.f);
		AO_Yaw = InterpAO_Yaw;
		
		// 当 AO_Yaw 足够小时（小于15度），认为已经完成转向，重置状态
		if (FMath::Abs(AO_Yaw) < 15.f)
		{
			this->TurningInPlaceType = ETurningInPlace::ETP_InPlace;
			// 更新 LastFrameRotation 为当前瞄准方向的 Yaw，为下一帧的 AO_Yaw 计算做准备
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

// RotateCharacterForTurning 说明：
// 此函数负责执行实际的角色旋转逻辑，通过代码直接控制 Actor 的旋转。
// 
// 工作原理：
// 1. 每帧根据 CharacterRotationSpeed 计算应该旋转的角度
// 2. 直接旋转整个 Actor（而不是只旋转 Mesh），确保角色朝向正确更新
// 3. 同步更新 LastFrameRotation 和 AO_Yaw，保持数据一致性
// 4. 当旋转完成时，恢复 CharacterMovementComponent 的自动旋转功能
//
// 为什么旋转整个 Actor 而不是 Mesh：
// - 旋转 Actor 会改变角色的实际朝向，影响移动、碰撞等系统
// - 只旋转 Mesh 只是视觉上的改变，不会影响角色的实际朝向
// - 我们需要角色真正转向，所以必须旋转整个 Actor
//
// LastFrameRotation 的作用：
// - LastFrameRotation 存储上一帧角色的朝向（Yaw）
// - 在 SetAimOffset 中，AO_Yaw = NormalizedDeltaRotator(AimRotation, LastFrameRotation).Yaw
// - 通过更新 LastFrameRotation，我们"告诉"系统角色已经旋转了，下一帧 AO_Yaw 会自动减少
// - 这是保持数据一致性的关键
void APlayerCharacter::RotateCharacterForTurning(float DeltaTime)
{
	// 检查旋转是否已经完成（剩余角度小于 0.1 度）
	// 这个检查在函数开始和结束时都会执行，确保旋转能够及时结束
	if (FMath::Abs(TargetRotationDelta) < 0.1f)
	{
		// ========== 旋转完成，重置所有状态 ==========
		
		// 清除旋转标志
		bIsRotatingCharacter = false;
		TargetRotationDelta = 0.0f;
		this->TurningInPlaceType = ETurningInPlace::ETP_InPlace;
		
		// 关键：更新 LastFrameRotation 为当前 Actor 的旋转（不是控制器的旋转）
		// 这确保了下一帧计算 AO_Yaw 时，会基于角色新的朝向
		// 如果使用 GetBaseAimRotation()（控制器旋转），会导致数据不一致
		this->LastFrameRotation = FRotator(0.f, GetActorRotation().Yaw, 0.f);
		
		// 重置 AO_Yaw 相关变量
		AO_Yaw = 0.0f;
		InterpAO_Yaw = 0.0f;
		
		// 恢复 CharacterMovementComponent 的自动旋转功能
		// 旋转完成后，角色可以正常响应移动方向的旋转
		this->GetCharacterMovement()->bOrientRotationToMovement = true;
		return;
	}
	
	// ========== 旋转期间的保护措施 ==========
	// 在旋转期间持续禁用自动旋转功能，防止被其他系统干扰
	// 这确保了我们的手动旋转不会被 CharacterMovementComponent 覆盖
	this->GetCharacterMovement()->bOrientRotationToMovement = false;
	this->bUseControllerRotationYaw = false;
	
	// ========== 计算本帧应该旋转的角度 ==========
	// CharacterRotationSpeed 是旋转速度（度/秒），例如 360 度/秒表示每秒转一圈
	// RotationThisFrame = 速度 × 时间，得到本帧应该旋转的角度
	float RotationThisFrame = CharacterRotationSpeed * DeltaTime;
	
	// RotationDirection 表示旋转方向：+1 表示顺时针（向右），-1 表示逆时针（向左）
	// 使用 FMath::Sign 获取 TargetRotationDelta 的符号
	float RotationDirection = FMath::Sign(TargetRotationDelta);
	
	// 防止过度旋转：如果剩余角度小于本帧应该旋转的角度，只旋转剩余角度
	// 例如：剩余 5 度，但本帧应该旋转 10 度，则只旋转 5 度，避免"超调"
	if (FMath::Abs(TargetRotationDelta) < RotationThisFrame)
	{
		RotationThisFrame = FMath::Abs(TargetRotationDelta);
	}
	
	// ========== 应用旋转到 Actor ==========
	// 获取当前 Actor 的旋转
	FRotator CurrentRotation = GetActorRotation();
	FRotator NewRotation = CurrentRotation;
	
	// 在 Yaw 轴上添加旋转角度
	// RotationDirection * RotationThisFrame 确保旋转方向正确
	// 例如：TargetRotationDelta = -120，则 RotationDirection = -1，每帧向左旋转
	NewRotation.Yaw += RotationDirection * RotationThisFrame;
	
	// 应用新的旋转到 Actor
	// SetActorRotation 会立即更新角色的朝向，这是实际产生旋转效果的地方
	SetActorRotation(NewRotation);
	
	// ========== 更新 LastFrameRotation（关键步骤）==========
	// 更新 LastFrameRotation 为角色新的朝向
	// 这是保持数据一致性的关键：下一帧在 SetAimOffset 中计算 AO_Yaw 时，
	// 会使用这个新的 LastFrameRotation，从而正确计算出减少后的 AO_Yaw
	// 
	// 为什么使用 NewRotation.Yaw 而不是 GetBaseAimRotation().Yaw：
	// - NewRotation.Yaw 是角色实际的朝向（我们刚刚设置的）
	// - GetBaseAimRotation().Yaw 是控制器的朝向（瞄准方向）
	// - 我们需要记录角色的朝向，而不是瞄准方向
	this->LastFrameRotation = FRotator(0.f, NewRotation.Yaw, 0.f);
	
	// ========== 更新剩余旋转角度 ==========
	// 从 TargetRotationDelta 中减去本帧旋转的角度
	// 例如：TargetRotationDelta = 120，本帧旋转 10 度，则剩余 110 度
	TargetRotationDelta -= RotationDirection * RotationThisFrame;
	
	// ========== 同步更新 AO_Yaw ==========
	// 手动减少 AO_Yaw，使其与旋转同步
	// 这样可以在旋转过程中立即看到 AO_Yaw 的变化，而不需要等到下一帧
	float AO_YawReduction = RotationDirection * RotationThisFrame;
	AO_Yaw -= AO_YawReduction;
	
	// 角度范围规范化：确保 AO_Yaw 在 [-180, 180] 范围内
	// 如果超出范围，进行规范化处理，避免角度值异常
	// 例如：AO_Yaw = 190 度，应该转换为 -170 度（190 - 360 = -170）
	if (AO_Yaw > 180.f) AO_Yaw -= 360.f;
	if (AO_Yaw < -180.f) AO_Yaw += 360.f;
	
	// 同步更新 InterpAO_Yaw，保持一致性
	InterpAO_Yaw = AO_Yaw;
	
	// ========== 检查旋转是否完成 ==========
	// 再次检查剩余角度（可能在更新后已经小于阈值）
	if (FMath::Abs(TargetRotationDelta) < 0.1f)
	{
		// 旋转完成，执行清理工作
		bIsRotatingCharacter = false;
		TargetRotationDelta = 0.0f;
		this->TurningInPlaceType = ETurningInPlace::ETP_InPlace;
		
		// 更新 LastFrameRotation 为最终旋转后的朝向
		this->LastFrameRotation = FRotator(0.f, NewRotation.Yaw, 0.f);
		
		// 重置 AO_Yaw 相关变量
		AO_Yaw = 0.0f;
		InterpAO_Yaw = 0.0f;
		
		// 恢复 CharacterMovementComponent 的自动旋转功能
		this->GetCharacterMovement()->bOrientRotationToMovement = true;
	}
}
