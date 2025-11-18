// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerAnimInstance.h"

#include "PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"


void UPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	APawn* OwningPawn = TryGetPawnOwner();
	if (OwningPawn)
	{
		PlayerCharacter = Cast<APlayerCharacter>(OwningPawn);
	}
}

void UPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!this->PlayerCharacter)
	{
		APawn* OwningPawn = TryGetPawnOwner();
		if (OwningPawn)
		{
			PlayerCharacter = Cast<APlayerCharacter>(OwningPawn);
		} else
		{
			UE_LOG(LogTemp, Warning, TEXT("PlayerAnimInstance::NativeUpdateAnimation - Failed to cast OwningPawn (%s) to APlayerCharacter"), *GetNameSafe(OwningPawn));
			return;
		}

	}

	FVector Velocity = this->PlayerCharacter->GetVelocity();
	FVector Lateral = FVector(Velocity.X, Velocity.Y, 0.f);

	// 水平速度
	Speed = Lateral.Size();

	UCharacterMovementComponent* MoveComp =  PlayerCharacter->GetCharacterMovement();
	if (MoveComp == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to Get MovementComponent"));
		return;
	}
	// 是否在空中
	bIsInAir = MoveComp->IsFalling();
	
	bIsRunning = MoveComp->GetCurrentAcceleration().Size() > 0.f;
}
