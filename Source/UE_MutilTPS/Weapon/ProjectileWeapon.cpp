// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileWeapon.h"

#include "Projectile.h"
#include "Engine/StaticMeshSocket.h"

void AProjectileWeapon::Fire(FVector& HitTarget)
{
	Super::Fire(HitTarget);
	UE_LOG(LogTemp, Warning, TEXT("Fire"));
	// 生成子弹
	if (ProjectileClass && HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("Spawn Projectile"));
		UStaticMeshSocket const* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName(FName("MuzzleFlash"));
		if (MuzzleFlashSocket)
		{
			UE_LOG(LogTemp, Warning, TEXT("Get Socket Transform"));
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
