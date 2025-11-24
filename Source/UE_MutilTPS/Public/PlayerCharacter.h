// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UE_MutilTPS/Componts/CombatComponent.h"
#include "PlayerCharacter.generated.h"

class UCombatComponent;
class AWeaponBase;
class UWidgetComponent;
class UCameraComponent;
class USpringArmComponent;
class UInputMappingContext;
class UInputAction;

// 文件/类说明：
// APlayerCharacter 表示玩家控制的 Character，包括摄像机、输入、与武器交互的基本逻辑。
// 重要约定：
// - 大部分游戏状态（如装备状态）应由服务器维护；客户端通过 Replicated/RepNotify 接收并更新本地 UI。
// - InputMappingContext 与 InputAction 用于 Enhanced Input 系统，需在运行时把 MappingContext 添加到本地子系统。

UCLASS()
class UE_MUTILTPS_API APlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();

	// Tick：如果日后需要帧刷新的战斗逻辑可以开启，当前默认启用。
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Enhanced Input 映射上下文（在编辑器中指定），在运行时通过 AddMappingContext 添加到 LocalPlayer 子系统
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;

	// Enhanced Input Actions：在编辑器中配置具体的输入（例如 Move 为 Vector2）
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> EquipAction;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> AimingAction;

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	// SetOverlappingWeapon 说明：
	// - 由 AWeaponBase::OnSphereOverLap 在服务器上调用，用来让角色记录当前可拾取的武器。
	// - 参数 Weapon：当前重叠的武器 Actor 指针（nullptr 表示离开范围）。
	void SetOverlappingWeapon(AWeaponBase* Weapon);

	virtual void PostInitializeComponents() override;

	FORCEINLINE
	bool IsEquipWeapon() const { return this->CombatComponent && this->CombatComponent->EquippedWeapon != nullptr; }

	FORCEINLINE
	bool IsAiming() const { return this->CombatComponent && this->CombatComponent->bIsAiming; }
	
protected:
	virtual void BeginPlay() override;

private:
	// 摄像机与相机杆
	UPROPERTY(VisibleAnywhere, Category="Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category="Camera")
	TObjectPtr<UCameraComponent> Camera;

	void InitInputMapping();

	// 输入回调
	void Move(const struct FInputActionValue& Value);
	void Look(const struct FInputActionValue& Value);

	// 跳跃回调（由 Enhanced Input 的 Started/Completed 调用）
	void StartJump();
	void StopJump();
	// 本地拾取触发，触发后会在服务器（如果 HasAuthority）执行装备逻辑
	void EquipWeapon();
	void PressCrouch();
	void PressAiming();
	void ReleaseAiming();
	

	// 头顶 Widget（通常用于显示玩家名字/状态）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(AllowPrivateAccess=true))
	TObjectPtr<UWidgetComponent> OverHeadWidget;

	// Replicated Property：重叠武器
	// - 使用 ReplicatedUsing 指定 OnRep_OverlappingWeapon 作 RepNotify 回调。
	// - 复制条件设置为 OwnerOnly（只复制给 Pawn 的拥有者客户端），因为这是一条仅与该玩家 UI 相关的信息。
	UPROPERTY(ReplicatedUsing=OnRep_OverlappingWeapon)
	TObjectPtr<AWeaponBase> OverlappingWeapon;

	// RepNotify 回调：当 OverlappingWeapon 在客户端更新时被调用，用于显示/隐藏拾取提示。
	UFUNCTION()
	void OnRep_OverlappingWeapon(AWeaponBase* LastWeapon);

	UFUNCTION(Server, Reliable)
	void ServerPressEquipWeapon();

	// 战斗组件，用于管理装备逻辑
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess=true))
	TObjectPtr<UCombatComponent> CombatComponent;
};
