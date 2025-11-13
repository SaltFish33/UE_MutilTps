// 文件说明：
// Menu.cpp 包含 UMenu 的实现。UMenu 是一个 UUserWidget，负责主菜单 UI（Host/Join）和与
// UMultiplayerSessionsSubsystem 的交互。此处添加注释用于说明每个方法的职责与执行流程。

#include "Menu.h"
#include "Components/Button.h"
#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"

// UMenu::MenuSetup
// 说明：初始化并显示菜单 Widget，同时：
//  - 记录大厅地图路径（用于 ServerTravel）
//  - 设置要创建会话的玩家数量与比赛类型（MatchType）
//  - 将 Widget 添加到视口并使其可聚焦，切换输入模式为 UIOnly 并显示鼠标
//  - 获取 GameInstance 的 MultiplayerSessionsSubsystem 并绑定订阅其事件（创建/查找/加入/销毁/开始）
// 参数：
//  - NumberOfPublicConnections: 本次会话的公开玩家数量（可加入的玩家数）
//  - TypeOfMatch: 自定义字符串标识比赛类型（用于会话查询/过滤）
//  - LobbyPath: 地图路径，当创建会话并作为服务器时，调用 ServerTravel 到该路径
// 注意：PathToLobby 字符串会附加 "?listen" 以便作为服务器开放监听
void UMenu::MenuSetup(int32 NumberOfPublicConnections, FString TypeOfMatch, FString LobbyPath)
{
	PathToLobby = FString::Printf(TEXT("%s?listen"), *LobbyPath);
	NumPublicConnections = NumberOfPublicConnections;
	MatchType = TypeOfMatch;
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	bIsFocusable = true;

	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
	}

	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->MultiplayerOnCreateSessionComplete.AddDynamic(this, &ThisClass::OnCreateSession);
		MultiplayerSessionsSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &ThisClass::OnFindSessions);
		MultiplayerSessionsSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSession);
		MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &ThisClass::OnDestroySession);
		MultiplayerSessionsSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &ThisClass::OnStartSession);
	}
}

// UMenu::Initialize
// 说明：在 Widget 初始化时绑定 Host 与 Join 按钮的回调（OnClicked）。
//  返回：如果父类 Initialize 失败则返回 false。
bool UMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &ThisClass::HostButtonClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &ThisClass::JoinButtonClicked);
	}

	return true;
}

// UMenu::NativeDestruct
// 说明：当 Widget 销毁时清理（MenuTearDown），恢复输入模式等。
void UMenu::NativeDestruct()
{
	MenuTearDown();
	Super::NativeDestruct();
}

// UMenu::OnCreateSession
// 说明：当会话创建完成时被调用（由子系统广播）。如果成功则作为服务器进行 ServerTravel 到大厅地图。
// 失败时打印调试信息并重新启用 Host 按钮以允许重试。
// 参数：bWasSuccessful - 会话创建是否成功
void UMenu::OnCreateSession(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			World->ServerTravel(PathToLobby);
		}
	}
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Red,
				FString(TEXT("Failed to create session!"))
			);
		}
		HostButton->SetIsEnabled(true);
	}
}

// UMenu::OnFindSessions
// 说明：当查找会话完成时被调用，遍历搜索结果并查找匹配 MatchType 的会话，然后调用子系统的 JoinSession。
// 如果查找失败或未找到会话，则重新启用 Join 按钮。
// 参数：SessionResults - 查询结果数组；bWasSuccessful - 查找是否成功
void UMenu::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		return;
	}

	for (auto Result : SessionResults)
	{
		FString SettingsValue;
		Result.Session.SessionSettings.Get(FName("MatchType"), SettingsValue);
		if (SettingsValue == MatchType)
		{
			MultiplayerSessionsSubsystem->JoinSession(Result);
			return;
		}
	}
	if (!bWasSuccessful || SessionResults.Num() == 0)
	{
		JoinButton->SetIsEnabled(true);
	}
}

// UMenu::OnJoinSession
// 说明：当加入会话完成时被调用。通过 OnlineSubsystem 获取连接地址（ResolvedConnectString）并让本地玩家ClientTravel到该地址。
// 参数：Result - 加入会话的结果枚举
// 备注：需要 IOnlineSubsystem 与会话接口有效
void UMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			FString Address;
			SessionInterface->GetResolvedConnectString(NAME_GameSession, Address);

			APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
			if (PlayerController)
			{
				PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
			}
		}
	}
}

// UMenu::OnDestroySession / OnStartSession
// 说明：占位回调，可在需要时扩展处理对应事件结果。
void UMenu::OnDestroySession(bool bWasSuccessful)
{
}

void UMenu::OnStartSession(bool bWasSuccessful)
{
}

// UMenu::HostButtonClicked
// 说明：Host 按钮回调，禁止按钮并请求子系统创建会话（CreateSession）
void UMenu::HostButtonClicked()
{
	HostButton->SetIsEnabled(false);
	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->CreateSession(NumPublicConnections, MatchType);
	}
}

// UMenu::JoinButtonClicked
// 说明：Join 按钮回调，禁止按钮并请求子系统查找会话（FindSessions）
void UMenu::JoinButtonClicked()
{
	JoinButton->SetIsEnabled(false);
	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->FindSessions(10000);
	}
}

// UMenu::MenuTearDown
// 说明：从父节点移除 Widget 并恢复输入模式为 GameOnly，隐藏鼠标光标。
void UMenu::MenuTearDown()
{
	RemoveFromParent();
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}
}
