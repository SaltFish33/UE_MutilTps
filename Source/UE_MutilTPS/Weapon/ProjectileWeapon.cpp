// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileWeapon.h"

#include "Projectile.h"
#include "Engine/StaticMeshSocket.h"

void AProjectileWeapon::Fire(FVector& HitTarget)
{
	Super::Fire(HitTarget);
	// 生成子弹
	if (ProjectileClass && HasAuthority())
	{
		UStaticMeshSocket const* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName(FName("MuzzleFlash"));
		if (MuzzleFlashSocket)
		{
			FTransform SocketTransform;
			MuzzleFlashSocket->GetSocketTransform(SocketTransform,GetWeaponMesh());
			FVector ToTarget = HitTarget - SocketTransform.GetLocation();
			FRotator TargetRotation = ToTarget.Rotation();

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = GetOwner();
			SpawnParams.Instigator = Cast<APawn>(GetOwner());

			GetWorld()->SpawnActor<AProjectile>(
				ProjectileClass,
				SocketTransform.GetLocation(),
				TargetRotation,
				SpawnParams
			);
		}
	}
}
