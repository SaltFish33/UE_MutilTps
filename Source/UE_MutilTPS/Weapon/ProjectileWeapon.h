// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Projectile.h"
#include "WeaponBase.h"
#include "ProjectileWeapon.generated.h"

/**
 * 
 */
UCLASS()
class UE_MUTILTPS_API AProjectileWeapon : public AWeaponBase
{
	GENERATED_BODY()

public:
	virtual void Fire(const FVector_NetQuantize& HitTarget) override;

private:
	UPROPERTY(EditAnywhere, Category="Weapon Projectile")
	TSubclassOf<AProjectile> ProjectileClass;

};
