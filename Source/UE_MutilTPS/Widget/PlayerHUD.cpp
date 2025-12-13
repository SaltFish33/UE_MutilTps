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
    // 根据是否瞄准到可交互对象决定准星颜色（红色表示可交互）
    FLinearColor CrosshairColor = this->CrosshairData.bIsAimingAtInteractable ? FLinearColor::Red : FLinearColor::White;
    DrawCrosshair(this->CrosshairData.CrosshairsCenter, ViewportCenter, FVector2D(0.f, 0.f), CrosshairColor);
    DrawCrosshair(this->CrosshairData.CrosshairsLeft, ViewportCenter, FVector2D(-Spread, 0.f), CrosshairColor);
    DrawCrosshair(this->CrosshairData.CrosshairsRight, ViewportCenter, FVector2D(Spread, 0.f), CrosshairColor);
    DrawCrosshair(this->CrosshairData.CrosshairsTop, ViewportCenter, FVector2D(0.f, -Spread), CrosshairColor);
    DrawCrosshair(this->CrosshairData.CrosshairsBottom, ViewportCenter, FVector2D(0.f, Spread), CrosshairColor);
}

// DrawCrosshair 说明：
// 此函数负责在屏幕上绘制准星纹理，并支持以指定中心点进行定位和扩散偏移。
//
// UE 中 ViewPort 的坐标系说明：
// - 原点 (0, 0) 位于屏幕左上角
// - X 轴：从左到右递增（0 在左，ViewportSize.X 在右）
// - Y 轴：从上到下递增（0 在上，ViewportSize.Y 在下）
// - 屏幕中心点坐标 = (ViewportSize.X / 2, ViewportSize.Y / 2)
//
// DrawTextureSimple 的坐标系统：
// - 函数签名：DrawTextureSimple(UTexture2D* Texture, float X, float Y)
// - X, Y 参数表示纹理左上角的屏幕坐标位置（不是中心点）
// - 这意味着纹理会从指定的 (X, Y) 位置向右下方向绘制
//
// 为什么需要计算 TextureSize 的偏移：
// 1. 目标：让纹理以 ViewportCenter 为中心点绘制
// 2. 问题：DrawTextureSimple 需要的是左上角坐标，而不是中心点坐标
// 3. 解决：需要将中心点坐标转换为左上角坐标
//
// 坐标转换公式：
// 左上角X = 中心点X - 纹理宽度 / 2
// 左上角Y = 中心点Y - 纹理高度 / 2
//
// 示例说明：
// 假设 ViewportCenter = (960, 540)，TextureSize = (64, 64)
// - 如果直接使用 ViewportCenter：纹理左上角在 (960, 540)，纹理会向右下偏移
// - 使用转换后：左上角 = (960 - 32, 540 - 32) = (928, 508)，纹理中心正好在 (960, 540)
//
// Spread 参数的作用：
// - Spread.X：水平方向的扩散偏移（正值向右，负值向左）
// - Spread.Y：垂直方向的扩散偏移（正值向下，负值向上）
// - 用于实现准星在射击时的展开效果
//
// TintColor 参数的作用：
// - 用于改变准星的颜色（例如瞄准到可交互对象时变为红色）
// - 默认值为白色（FLinearColor::White），表示正常颜色
void APlayerHUD::DrawCrosshair(UTexture2D* Texture, FVector2D ViewportCenter, FVector2D Spread, FLinearColor TintColor)
{
	if (Texture)
	{
		// 获取纹理的尺寸（宽度和高度）
		FVector2D TextureSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		
		// 计算纹理左上角的屏幕坐标：
		// - ViewportCenter.X - TextureSize.X / 2.f：将中心点X转换为左上角X
		// - ViewportCenter.Y - TextureSize.Y / 2.f：将中心点Y转换为左上角Y
		// - + Spread.X / Spread.Y：添加扩散偏移，实现准星展开效果
		float X = ViewportCenter.X - TextureSize.X / 2.f + Spread.X;
		float Y = ViewportCenter.Y - TextureSize.Y / 2.f + Spread.Y;
		
		// 使用DrawTexture支持颜色tint（DrawTextureSimple不支持颜色）
		// 参数说明：
		// - Texture: 要绘制的纹理
		// - X, Y: 左上角坐标
		// - ScaleX, ScaleY: 缩放比例（1.0表示原始大小）
		// - U, V: UV坐标起始点（0,0表示从纹理左上角开始）
		// - UL, VL: UV坐标范围（1,1表示使用整个纹理）
		// - TintColor: 颜色tint（用于改变纹理颜色）
		// - BlendMode: 混合模式（BLEND_Translucent表示支持透明度）
		DrawTexture(
			Texture,
			X, Y,
			TextureSize.X, TextureSize.Y,
			0.0f, 0.0f,
			1.0f, 1.0f,
			TintColor,
			EBlendMode::BLEND_Translucent
		);
	}
}