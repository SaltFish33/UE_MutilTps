// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PlayerHUD.generated.h"

USTRUCT()
struct FCrosshairData{

	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category="Crosshair Data")
	UTexture2D* CrosshairsCenter;

	UPROPERTY(EditDefaultsOnly, Category="Crosshair Data")
	UTexture2D* CrosshairsLeft;

	UPROPERTY(EditDefaultsOnly, Category="Crosshair Data")
	UTexture2D* CrosshairsRight;

	UPROPERTY(EditDefaultsOnly, Category="Crosshair Data")
	UTexture2D* CrosshairsTop;

	UPROPERTY(EditDefaultsOnly, Category="Crosshair Data")
	UTexture2D* CrosshairsBottom;

	float CrosshairSpread;
};

/**
 * 
 */
UCLASS()
class UE_MUTILTPS_API APlayerHUD : public AHUD
{
	GENERATED_BODY()
	
public:
	virtual void DrawHUD() override;

	FORCEINLINE void SetHUDPackage(FCrosshairData Package) { this->CrosshairData = Package; }
private:
	FCrosshairData CrosshairData;
	float CrosshairSpreadMax = 16.0f;
	void DrawCrosshair(UTexture2D* Texture, FVector2D ViewportCenter, FVector2D Spread);
};
