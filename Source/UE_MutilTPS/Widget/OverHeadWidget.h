// 文件说明：
// UOverHeadWidget 提供一个简单的头顶 UI，用于显示玩家的网络角色（Role）或其他文本信息。
// DisplayText 通过 BindWidget 与 Widget Blueprint 中的 TextBlock 联动，SetDisplayText 用于更新文本。

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OverHeadWidget.generated.h"

class UTextBlock;
/**
 * 
 */
UCLASS()
class UE_MUTILTPS_API UOverHeadWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// DisplayText：在 UMG Blueprint 中通过 BindWidget 绑定的文本控件
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DisplayText;

	// SetDisplayText: 在 C++/蓝图中调用以设置显示文本
	// 参数 Text: 要显示的字符串
	UFUNCTION()
	void SetDisplayText(const FString& Text);

	// ShowPlayerNetRole: 根据传入的 APawn 获取其网络 Role 并显示（Authority/AutonomousProxy/SimulatedProxy）
	// 参数 Player: 传入的 Pawn，用于读取网络角色
	UFUNCTION(BlueprintCallable)
	void ShowPlayerNetRole(APawn* Player);
protected:
	// 覆盖 NativeDestruct，以确保从父级移除（防止悬挂引用）
	virtual void NativeDestruct() override;
};

