// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerHUD.h"

void APlayerHUD::DrawHUD()
{
	Super::DrawHUD();
    FVector2D ViewportSize;
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(ViewportSize);
    }
    FVector2D ViewportCenter(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
    float Spread = this->CrosshairData.CrosshairSpread * this->CrosshairSpreadMax;
    DrawCrosshair(this->CrosshairData.CrosshairsCenter, ViewportCenter, FVector2D(0.f, 0.f));
    DrawCrosshair(this->CrosshairData.CrosshairsLeft, ViewportCenter, FVector2D(-Spread, 0.f));
    DrawCrosshair(this->CrosshairData.CrosshairsRight, ViewportCenter, FVector2D(Spread, 0.f));
    DrawCrosshair(this->CrosshairData.CrosshairsTop, ViewportCenter, FVector2D(0.f, -Spread));
    DrawCrosshair(this->CrosshairData.CrosshairsBottom, ViewportCenter, FVector2D(0.f, Spread));
}

void APlayerHUD::DrawCrosshair(UTexture2D* Texture, FVector2D ViewportCenter, FVector2D Spread)
{
	if (Texture)
	{
		FVector2D TextureSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		DrawTextureSimple(Texture, ViewportCenter.X - TextureSize.X / 2.f + Spread.X, ViewportCenter.Y - TextureSize.Y / 2.f + Spread.Y);
	}
}