// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"


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
protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Replicated)
	TObjectPtr<AWeaponBase> EquippedWeapon;
	TObjectPtr<APlayerCharacter> PlayerCharacter;
};
