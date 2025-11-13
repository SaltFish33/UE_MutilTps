// 头文件说明：
// UMenu 是一个 UUserWidget 派生类，负责处理主菜单的 UI 行为（Host/Join），并通过 UMultiplayerSessionsSubsystem
// 发起或响应在线会话操作。该类具有 BlueprintCallable 的 MenuSetup 接口，便于在 Blueprint 中创建并显示该菜单。
// 注：该 Widget 通过 BindWidget 获取 Designer 中创建的按钮控件（HostButton / JoinButton），并在 Initialize 时绑定点击事件。

// MenuSetup 说明（BlueprintCallable）：
//  - NumberOfPublicConnections（默认 4）：会话可加入的最大公开玩家数量。
//  - TypeOfMatch（默认 "FreeForAll"）：自定义的会话类型标签，作为匹配过滤器使用（写入并读取于 FOnlineSessionSettings）。
//  - LobbyPath（默认 "/Game/ThirdPersonCPP/Maps/Lobby"）：当作为服务器创建会话时，ServerTravel 至此地图并附加 "?listen"。
// 使用场景：在玩家打开主菜单时由游戏逻辑（例如 HUD / PlayerController）调用 MenuSetup 来显示菜单并初始化子系统回调。

// 成员/方法说明：
//  - HostButton / JoinButton：在 UMG Designer 中以相应的 Widget 名称绑定，分别触发 HostButtonClicked / JoinButtonClicked。
//  - HostButtonClicked：禁用按钮并通过 MultiplayerSessionsSubsystem->CreateSession 请求创建会话。
//  - JoinButtonClicked：禁用按钮并通过 MultiplayerSessionsSubsystem->FindSessions 请求查找会话。
//  - MenuTearDown：移除 Widget 并恢复输入为游戏模式（GameOnly），同时隐藏鼠标。
//  - OnCreateSession / OnFindSessions / OnJoinSession / OnDestroySession / OnStartSession：这些方法作为子系统广播的回调。
//    Menu 将根据回调结果执行 UI 更新（例如按钮状态）或进行场景迁移（ClientTravel / ServerTravel）。

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Menu.generated.h"

/**
 * 
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMenu : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	void MenuSetup(int32 NumberOfPublicConnections = 4, FString TypeOfMatch = FString(TEXT("FreeForAll")), FString LobbyPath = FString(TEXT("/Game/ThirdPersonCPP/Maps/Lobby")));

protected:

	virtual bool Initialize() override;
	virtual void NativeDestruct() override;

	//
	// Callbacks for the custom delegates on the MultiplayerSessionsSubsystem
	//
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful);
	void OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);
	UFUNCTION()
	void OnStartSession(bool bWasSuccessful);

private:

	UPROPERTY(meta = (BindWidget))
	class UButton* HostButton;

	UPROPERTY(meta = (BindWidget))
	UButton* JoinButton;

	UFUNCTION()
	void HostButtonClicked();

	UFUNCTION()
	void JoinButtonClicked();

	void MenuTearDown();

	// The subsystem designed to handle all online session functionality
	class UMultiplayerSessionsSubsystem* MultiplayerSessionsSubsystem;

	int32 NumPublicConnections{4};
	FString MatchType{TEXT("FreeForAll")};
	FString PathToLobby{TEXT("")};
};
