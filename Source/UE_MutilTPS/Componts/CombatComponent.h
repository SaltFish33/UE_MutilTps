// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"


class UPlayerAnimInstance;
class AWeaponBase;

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
	
};
