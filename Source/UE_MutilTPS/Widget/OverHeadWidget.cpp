// 文件说明：
// 实现了设置文本与根据 Pawn 展示网络角色的逻辑。注意 GetLocalRole 在服务端/客户端上下文中返回的值不同（Authority 表示服务端拥有）。

#include "OverHeadWidget.h"

#include "Components/TextBlock.h"

void UOverHeadWidget::SetDisplayText(const FString& Text)
{
	// 如果绑定的 DisplayText 存在则更新显示；否则记录警告日志，便于定位 UI 未正确绑定的问题。
	if (DisplayText)
	{
		DisplayText->SetText(FText::FromString(Text));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DisplayText is null in UOverHeadWidget::SetDisplayText"));
	}
}

void UOverHeadWidget::ShowPlayerNetRole(APawn* Player)
{
	// 获取本地角色（LocalRole）：在服务器上通常为 ROLE_Authority，在客户端为 Autonomous/Simulated 等
	ENetRole PlayerRole = Player != nullptr ? Player->GetLocalRole() : ROLE_None;
	FString RoleText;
	switch (PlayerRole)
	{
	case ROLE_Authority:
		RoleText = TEXT("Authority");
		break;
	case ROLE_AutonomousProxy:
		RoleText = TEXT("Autonomous Proxy");
		break;
	case ROLE_SimulatedProxy:
		RoleText = TEXT("Simulated Proxy");
		break;
	case ROLE_None:
	default:
		RoleText = TEXT("None");
		break;
	}

	// 将角色文本设置到 UI。这样在多人调试时可以直观看到某个 Pawn 的网络角色。
	this->SetDisplayText(RoleText);
}

void UOverHeadWidget::NativeDestruct()
{
	// 从父控件移除并调用父类析构函数，确保 Widget 清理正确。
	RemoveFromParent();
	Super::NativeDestruct();
}

