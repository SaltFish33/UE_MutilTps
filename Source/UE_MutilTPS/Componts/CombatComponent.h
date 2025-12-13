// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"


class UPlayerAnimInstance;
class AWeaponBase;
class APlayerCharacterController;
class APlayerHUD;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UE_MUTILTPS_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCombatComponent();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	friend class APlayerCharacter;

	UFUNCTION()
	void EquipWeapon(AWeaponBase* Weapon);

	UFUNCTION()
	void SetAiming(bool IsAiming);

	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool IsAiming);

	void CustomTick(float DeltaTime);

	FORCEINLINE FVector& GetTraceHitTarget() { return this->TraceHitTarget; }
protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_EquippedWeapon();

	UFUNCTION()
	void FireButtonPressed(bool bIsPressed);

	UFUNCTION(Server, Reliable)
	void ServerFireButtonPressed(const FVector_NetQuantize& HitTarget);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastFire(const FVector_NetQuantize& HitTarget);

	TObjectPtr<APlayerCharacterController> PlayerCharacterController;
	TObjectPtr<APlayerHUD> PlayerHUD;

private:
	UPROPERTY(ReplicatedUsing=OnRep_EquippedWeapon)
	TObjectPtr<AWeaponBase> EquippedWeapon;
	
	TObjectPtr<APlayerCharacter> PlayerCharacter;
	UPROPERTY(Replicated)
	bool bIsAiming;

	UPROPERTY(EditDefaultsOnly)
	float NormalMaxWalkSpeed;

	UPROPERTY(EditDefaultsOnly)
	float AimingMaxWalkSpeed;

	UPROPERTY(Replicated)
	bool bIsFiring;

	TObjectPtr<UPlayerAnimInstance> PlayerAnimInstance;
	void TickGetTraceHitRaycast(FHitResult& OutHitResult);

	void SetPlayerHUD(float DeltaTime);

	UPROPERTY(EditDefaultsOnly)
	float CrosshairVelocityFactor;

	UPROPERTY(EditDefaultsOnly)
	float CrosshairInAirFactor;

	// 准星扩散相关因子
	// 瞄准时的准星缩小因子（瞄准时扩散值会减小，使准星更紧凑）
	UPROPERTY(EditDefaultsOnly, Category="Crosshair", meta=(ClampMin="0.0", ClampMax="1.0"))
	float CrosshairAimingFactor = 0.5f;

	// 开火时的准星扩大因子（开火时扩散值会增大，使准星展开）
	UPROPERTY(EditDefaultsOnly, Category="Crosshair", meta=(ClampMin="0.0"))
	float CrosshairShootingFactor = 2.5f;

	// 开火后准星恢复速度（插值速度）
	UPROPERTY(EditDefaultsOnly, Category="Crosshair", meta=(ClampMin="0.0"))
	float CrosshairShootingRecoverySpeed = 8.0f;

	// 当前开火导致的准星扩散值（用于平滑恢复）
	float CurrentCrosshairShootingFactor = 0.0f;

	// 是否瞄准到实现了接口的对象（用于准星变红）
	bool bIsAimingAtInteractable = false;

	FVector TraceHitTarget;

	// 相机FOV相关参数
	// 默认FOV（正常视角），当武器没有设置FOV时使用此值
	UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(ClampMin="1.0", ClampMax="170.0"))
	float DefaultFOV = 90.0f;

	// 瞄准时的默认FOV（更小的FOV提供更窄的视野），当武器没有设置FOV时使用此值
	UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(ClampMin="1.0", ClampMax="170.0"))
	float DefaultAimingFOV = 60.0f;

	// FOV插值速度（度/秒），用于平滑过渡
	UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(ClampMin="0.0"))
	float FOVInterpSpeed = 20.0f;

	// 当前目标FOV（用于插值计算）
	float CurrentTargetFOV;

	// 更新相机FOV（在CustomTick中调用）
	void UpdateCameraFOV(float DeltaTime);

	// 根据瞄准状态设置目标FOV
	void SetTargetFOV(bool bIsAiming);
};
