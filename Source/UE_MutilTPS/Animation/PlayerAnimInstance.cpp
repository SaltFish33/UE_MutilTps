// 文件说明：
// 在 NativeInitializeAnimation 中缓存 Owning Pawn（尽量减少每帧查找）；
// 在 NativeUpdateAnimation 中读取 Velocity、CharacterMovementComponent 的 IsFalling() 与当前加速度来设置动画变量。
// 之所以用 IsFalling 而不是输入事件，是因为输入事件不能涵盖所有导致空中状态的情况（例如被击飞、斜坡滑动等）。

// filepath: e:\UEProject\Ue_MutilTps\UE_MutilTPS\Source\UE_MutilTPS\Animation\PlayerAnimInstance.cpp
// 本文件：PlayerAnimInstance
// 责任：为角色的动画蓝图提供每帧更新的数据（例如速度、是否空中、Aim Offset、Lean 等）。
// 设计目标：
// - 尽量把昂贵的运行时类型转换/获取操作缓存（例如 Owning Pawn -> APlayerCharacter）。
// - 使用 CharacterMovementComponent 提供的物理/运动信息（速度、加速度、IsFalling）来驱动 BlendSpace / 状态机。
// - 计算用于动画层 blending 的偏移量：YawOffset（角色朝向与视角差）和 AO（AimOffset 的 Yaw/Pitch），并保证在网络代理上稳健。

#include "PlayerAnimInstance.h"

#include "PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "UE_MutilTPS/Weapon/WeaponBase.h"


void UPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 尝试获取并缓存 Owning Pawn（只做一次），避免每帧都做 TryGetPawnOwner + Cast 的开销。
	// 注意：在编辑器中热重载或某些生命周期时点上，OwningPawn 可能为空，因此 NativeUpdateAnimation 中有二次尝试逻辑。
	APawn* OwningPawn = TryGetPawnOwner();
	if (OwningPawn)
	{
		// 这里缓存为 APlayerCharacter，因为后续我们会调用角色特定的方法（例如 GetAO_Yaw/GetAO_Pitch）
		PlayerCharacter = Cast<APlayerCharacter>(OwningPawn);
	}
}

void UPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 如果尚未缓存 PlayerCharacter，那么再尝试一次（这覆盖了初始化时未就绪的情况）。
	// 这比每帧都做 Cast 更高效，同时对运行时动态生成/销毁 Pawn 更有容错性。
	if (!this->PlayerCharacter)
	{
		APawn* OwningPawn = TryGetPawnOwner();
		if (OwningPawn)
		{
			PlayerCharacter = Cast<APlayerCharacter>(OwningPawn);
		} else
		{
			// 如果连 OwningPawn 都为空，记录日志以便调试：可能是动画资产在预览或角色尚未绑定时被调用。
			UE_LOG(LogTemp, Warning, TEXT("PlayerAnimInstance::NativeUpdateAnimation - Failed to cast OwningPawn (%s) to APlayerCharacter"), *GetNameSafe(OwningPawn));
			return;
		}

	}

	// ----------------------------
	// 1) 速度相关（用于 BlendSpace）
	// ----------------------------
	// 我们只使用水平分量（X，Y），忽略垂直速度（Z），因为行走/跑步动画通常由水平速度驱动。
	FVector Velocity = this->PlayerCharacter->GetVelocity();
	FVector Lateral = FVector(Velocity.X, Velocity.Y, 0.f);

	// Speed 用于 BlendSpace 的输入：速度越大，Blend 越偏向奔跑动画
	Speed = Lateral.Size();

	// ----------------------------
	// 2) Movement Component 信息
	// ----------------------------
	UCharacterMovementComponent* MoveComp =  PlayerCharacter->GetCharacterMovement();
	if (MoveComp == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to Get MovementComponent"));
		return;
	}

	// 空中状态：使用 MovementComponent 的 IsFalling 更可靠（包括跳跃、被击飞、斜坡滑落等）
	bIsInAir = MoveComp->IsFalling();
	
	// 是否有输入推动（通常被用来判断是否在 run 动画或站立）：通过当前加速度判断
	// 注意：加速度为 0 不代表没有移动（角色可能在滑动或被外力推动），但通常用于判定是否主动输入
	bIsRunning = MoveComp->GetCurrentAcceleration().Size() > 0.f;

	// 角色装备/下蹲/瞄准状态，直接从 PlayerCharacter 读取网络/本地同步的状态
	bIsEquipWeapon = PlayerCharacter->IsEquipWeapon();

	bIsCrouch = PlayerCharacter->bIsCrouched;

	bIsAiming = PlayerCharacter->IsAiming();

	// ----------------------------
	// 3) YawOffset：角色朝向与 Aim（控制器）之间的差，用于上半身与骨骼层的 Blend
	// ----------------------------
	// AimRotation: 控制器当前视角（代表玩家视线/瞄准方向）
	FRotator AimRotation = PlayerCharacter->GetBaseAimRotation();
	// MovementRotation: 角色当前移动方向构造的旋转（即期望面向的旋转）
	FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(MoveComp->Velocity);

	// DeltaRot 表示 MovementRotation 相对于 AimRotation 的差值（带符号、-180..180）
	// 这在动画层里经常用来让上半身朝向 Aim 而下半身维持行走方向
	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation);

	this->TurningInPlace = PlayerCharacter->GetTurningInPlaceType();

	// DeltaRotation 使用插值（RInterpTo）来平滑过渡，避免瞬时抖动（视觉上更平滑）
	// DeltaRotation 存储累计的旋转差，随后我们把 Yaw 分量传递给动画蓝图的 YawOffset
	DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaSeconds, 6.f);
	YawOffset = DeltaRotation.Yaw;

	// ----------------------------
	// 4) Lean：根据每帧角色实际旋转速率来计算倾斜，用于转弯时上半身或骨盆的侧倾动画
	// ----------------------------
	// 保存上帧朝向并获取当前帧朝向以计算真实旋转速率（非移动方向的旋转）
	PlayerRotationLastFrame = PlayerRotation;
	PlayerRotation = PlayerCharacter->GetActorRotation();
	// 使用 NormalizedDeltaRotator 来正确处理角度环绕（避免 359 -> 0 导致的大跳变）
	FRotator DeltaRealRotation = UKismetMathLibrary::NormalizedDeltaRotator(PlayerRotation, PlayerRotationLastFrame);
	// 旋转速率（度/秒）
	float RotateSpeed = DeltaRealRotation.Yaw / DeltaSeconds;
	// 把当前 Lean 值插值到目标速率以获得平滑变化
	float InterpRotateSpeed = FMath::FInterpTo(Lean, RotateSpeed, DeltaSeconds, 6.f);
	// 限制 Lean 范围（避免动画被极端数值破坏）
	Lean = FMath::Clamp(InterpRotateSpeed, -90.f, 90.f);

	// ----------------------------
	// 5) Aim Offsets：AO 用于上半身瞄准姿态（通常在动画蓝图的 AimOffset BlendSpace 中使用）
	// ----------------------------
	// AO 的值由 PlayerCharacter 管理（那里的逻辑负责针对本地/远程代理做角度范围修正与平滑）
	AO_Yaw = PlayerCharacter->GetAO_Yaw();
	AO_Pitch = PlayerCharacter->GetAO_Pitch();

	// ----------------------------
	// 6) 左手 IK 位置（用于将左手贴到武器骨骼/插槽）
	// ----------------------------
	AWeaponBase* EquippedWeapon = PlayerCharacter->GetEquippedWeapon();
	if (!EquippedWeapon) return;
	UStaticMeshComponent* WeaponMesh = EquippedWeapon->GetWeaponMesh();
	if (!WeaponMesh) return;

	// 获取武器上 LeftHandSocket 的世界变换，然后把该位置转换为角色骨骼空间（hand_r）以便动画蓝图使用
	FVector OutLocation;
	FRotator OutRotation;
	this->LeftHandTransform = WeaponMesh->GetSocketTransform(FName("LeftHandSocket"), RTS_World);
	// 将世界空间的左手位置转换到骨骼空间：这使我们得到一个相对于手部骨骼的偏移，方便动画蓝图直接设置骨骼的 IK
	PlayerCharacter->GetMesh()->TransformToBoneSpace(FName("hand_r"), this->LeftHandTransform.GetLocation(),
		FRotator::ZeroRotator, OutLocation, OutRotation);
	// 把转换后的位置信息写回 LeftHandTransform 以供蓝图读取（保持旋转信息为骨骼空间的旋转）
	this->LeftHandTransform.SetLocation(OutLocation);
	this->LeftHandTransform.SetRotation(FQuat(OutRotation));

	// ----------------------------
	// 7) 右手旋转（用于武器瞄准IK）
	// ----------------------------
	// 获取角色骨骼上的右手Socket位置（hand_r），用于计算武器瞄准方向
	// 注意：这里使用角色骨骼的Socket，而不是武器的Socket
	FTransform RightHandSocketTransform = PlayerCharacter->GetMesh()->GetSocketTransform(FName("hand_r"), RTS_World);
	FVector RightHandLocation = RightHandSocketTransform.GetLocation();
	FVector TargetLocation = RightHandSocketTransform.GetLocation() + (RightHandSocketTransform.GetLocation() - PlayerCharacter->GetTraceHitTarget());
	
	// 计算从右手位置到瞄准目标的旋转
	// FindLookAtRotation(起始位置, 目标位置) - 计算从起始位置看向目标位置的旋转
	FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(RightHandLocation, TargetLocation);
	
	// 使用插值平滑过渡，避免旋转突然变化
	RightHandRotation = FMath::RInterpTo(RightHandRotation, LookAtRotation, DeltaSeconds, 4.f);
	
}

