// 文件说明：
// 在 NativeInitializeAnimation 中缓存 Owning Pawn（尽量减少每帧查找）；
// 在 NativeUpdateAnimation 中读取 Velocity、CharacterMovementComponent 的 IsFalling() 与当前加速度来设置动画变量。
// 之所以用 IsFalling 而不是输入事件，是因为输入事件不能涵盖所有导致空中状态的情况（例如被击飞、斜坡滑动等）。

#include "PlayerAnimInstance.h"

#include "PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"


void UPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 尝试获取 Owning Pawn 并缓存为 APlayerCharacter，避免每帧都做复杂的类型检查
	APawn* OwningPawn = TryGetPawnOwner();
	if (OwningPawn)
	{
		PlayerCharacter = Cast<APlayerCharacter>(OwningPawn);
	}
}

void UPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 如果尚未缓存 PlayerCharacter，则再次尝试获取（可能在初始化时未能获取）
	if (!this->PlayerCharacter)
	{
		APawn* OwningPawn = TryGetPawnOwner();
		if (OwningPawn)
		{
			PlayerCharacter = Cast<APlayerCharacter>(OwningPawn);
		} else
		{
			// 记录日志以便调试为何没有 OwningPawn
			UE_LOG(LogTemp, Warning, TEXT("PlayerAnimInstance::NativeUpdateAnimation - Failed to cast OwningPawn (%s) to APlayerCharacter"), *GetNameSafe(OwningPawn));
			return;
		}

	}

	// 使用角色速度的水平分量来驱动奔跑/慢走动画
	FVector Velocity = this->PlayerCharacter->GetVelocity();
	FVector Lateral = FVector(Velocity.X, Velocity.Y, 0.f);

	// 水平速度用于动画 BlendSpace 的输入
	Speed = Lateral.Size();

	// 获取 CharacterMovementComponent：该组件提供 IsFalling()、GetCurrentAcceleration() 等便捷方法
	UCharacterMovementComponent* MoveComp =  PlayerCharacter->GetCharacterMovement();
	if (MoveComp == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to Get MovementComponent"));
		return;
	}
	// 是否在空中：使用 MovementComponent->IsFalling() 更加可靠（包含跳跃与被抛出等情况）
	bIsInAir = MoveComp->IsFalling();
	
	// 是否正在运行 / 有输入：通过当前加速度来判断（加速度为 0 时通常表示没有输入推动）
	bIsRunning = MoveComp->GetCurrentAcceleration().Size() > 0.f;

	bIsEquipWeapon = PlayerCharacter->IsEquipWeapon();

	bIsCrouch = PlayerCharacter->bIsCrouched;

	bIsAiming = PlayerCharacter->IsAiming();

	// 计算Controller的旋转值，和实际要转向的旋转值的差值
	FRotator AimRotation = PlayerCharacter->GetBaseAimRotation();
	FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(MoveComp->Velocity);
	// 差值就是在指定时间内需要旋转多少
	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation);
	// 再去计算在DeltaSecond内，需要插值多少来旋转，速度越小，越精确
	DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaSeconds, 6.f);
	YawOffset = DeltaRotation.Yaw;

	// Lean是计算单位时间内的旋转速率，根据旋转速率来调整倾斜角度
	// 旋转速率就是通过当前帧和上一真旋转的差值来计算
	PlayerRotationLastFrame = PlayerRotation;
	PlayerRotation = PlayerCharacter->GetActorRotation();
	FRotator DeltaRealRotation = UKismetMathLibrary::NormalizedDeltaRotator(PlayerRotation, PlayerRotationLastFrame);
	// 差值除以时间等于速度
	float RotateSpeed = DeltaRealRotation.Yaw / DeltaSeconds;
	// 把当前的速率插值到目标速率
	float InterpRotateSpeed = FMath::FInterpTo(Lean, RotateSpeed, DeltaSeconds, 6.f);
	Lean = FMath::Clamp(InterpRotateSpeed, -90.f, 90.f);

	AO_Yaw = PlayerCharacter->GetAO_Yaw();
	AO_Pitch = PlayerCharacter->GetAO_Pitch();
}

