// 文件说明：
// UPlayerAnimInstance 是角色的动画实例，用于把运行时的角色状态（速度、是否在空中等）同步到动画蓝图变量上。
// 关键点：缓存 Owner Pawn（APlayerCharacter）提高效率；在 NativeUpdateAnimation 中实时从 MovementComponent 读取状态，
// 而不是依赖输入事件，以保证在跳跃或物理影响下动画状态依旧准确。

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PlayerAnimInstance.generated.h"

class APlayerCharacter;
/**
 * 
 */
UCLASS()
class UE_MUTILTPS_API UPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// 在这里获取并缓存 Owning Pawn，以便在每帧更新时使用（避免多次 TryGetPawnOwner 调用）
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	// 缓存的角色指针：用于访问速度、MovementComponent 等
	UPROPERTY(BlueprintReadOnly, Category="Character", meta=(AllowPrivateAccess=true))
	TObjectPtr<APlayerCharacter> PlayerCharacter;

	// 动画蓝图读取的参数（BlueprintReadOnly：供动画蓝图读取）
	// Speed: 水平移动速度（仅 X/Y 分量）
	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float Speed = 0.f;

	// bIsInAir: 是否处于空中（跳跃/掉落），通过 MovementComponent->IsFalling() 获取更可靠
	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bIsInAir = false;

	// bIsRunning: 是否正在输入移动（由加速度判断）
	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bIsRunning = false;
	
	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bIsEquipWeapon = false;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bIsCrouch = false;
	
	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bIsAiming = false;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float YawOffset = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float Lean = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float AO_Yaw = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float AO_Pitch = 0.f;
	
	FRotator DeltaRotation;
	FRotator PlayerRotationLastFrame;
	FRotator PlayerRotation;
};

