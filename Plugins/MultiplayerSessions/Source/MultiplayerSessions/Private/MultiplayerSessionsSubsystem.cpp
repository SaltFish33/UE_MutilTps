// Fill out your copyright notice in the Description page of Project Settings.

// 文件说明：
// 本文件实现 UMultiplayerSessionsSubsystem 的功能。该子系统封装了对 IOnlineSession 接口的常用操作：
// CreateSession / FindSessions / JoinSession / DestroySession / StartSession。
// 同时管理与 OnlineSubsystem 的委托（Delegate）注册与清理，并对外通过自定义委托广播结果，供 UI（如 Menu）使用。

#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"

// 构造函数说明：
// 在构造时使用 CreateUObject 来绑定内部回调到各自的 FOnXxxCompleteDelegate。这样当 OnlineSession 接口
// 触发事件时，本类能够接收并进一步广播自定义的 MultiplayOnXxxComplete 委托。
UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem():
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete))
{
	
}

// UMultiplayerSessionsSubsystem::CreateSession
// 说明：创建会话的流程：
//  1. 确保 SessionInterface 有效（IsValidSessionInterface）
//  2. 检查是否已有名为 NAME_GameSession 的现有会话；若存在则先销毁后重建（记录请求以便销毁完成后重建）
//  3. 将本地委托添加到 SessionInterface（保存 Handle 以便后续移除）
//  4. 填充 FOnlineSessionSettings（是否 LAN、公开连接数、是否可发现、MatchType 等）
//  5. 调用 SessionInterface->CreateSession；若失败则清理委托并广播失败
// 注：MatchType 通过 Set 存入 SessionSettings 的自定义键，用于后续 FindSessions 过滤
void UMultiplayerSessionsSubsystem::CreateSession(int32 NumPublicConnections, FString MatchType)
{
	if (!IsValidSessionInterface())
	{
		return;
	}

	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr)
	{
		bCreateSessionOnDestroy = true;
		LastNumPublicConnections = NumPublicConnections;
		LastMatchType = MatchType;

		DestroySession();
	}

	// Store the delegate in a FDelegateHandle so we can later remove it from the delegate list
	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	LastSessionSettings->bIsLANMatch = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	LastSessionSettings->NumPublicConnections = NumPublicConnections;
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->BuildUniqueId = 1;
	LastSessionSettings->bUseLobbiesIfAvailable = true;

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);

		// Broadcast our own custom delegate
		MultiplayerOnCreateSessionComplete.Broadcast(false);
	}
}

// UMultiplayerSessionsSubsystem::FindSessions
// 说明：查找会话的流程：
//  - 创建 FOnlineSessionSearch 并设置 MaxSearchResults、bIsLanQuery、以及查询设置（SEARCH_PRESENCE）
//  - 注册 FindSessions 完成委托并调用 FindSessions；若调用失败则清理委托并广播空结果
//  - 成功时 OnFindSessionsComplete 会读取 LastSessionSearch->SearchResults 并广播给监听者（Menu）
void UMultiplayerSessionsSubsystem::FindSessions(int32 MaxSearchResults)
{
	if (!IsValidSessionInterface())
	{
		return;
	}

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	LastSessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);

		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
}

// UMultiplayerSessionsSubsystem::JoinSession
// 说明：加入指定的 SessionResult（来自 FindSessions 的结果）。步骤：
//  - 检查 SessionInterface 有效；注册 Join 会话完成委托并调用 JoinSession。
//  - 若 JoinSession 调用失败，清理委托并广播错误结果
void UMultiplayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);

		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
	}
}

// UMultiplayerSessionsSubsystem::DestroySession
// 说明：销毁当前会话：注册销毁完成委托并调用 DestroySession；失败时广播 false。
// 注意：如果之前设置了 bCreateSessionOnDestroy（表示用户触发了 CreateSession 但当前已有会话正在占用名），
// 在 OnDestroySessionComplete 收到成功事件后会重建会话。
void UMultiplayerSessionsSubsystem::DestroySession()
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		MultiplayerOnDestroySessionComplete.Broadcast(false);
	}
}

// UMultiplayerSessionsSubsystem::StartSession
// 说明：占位实现，可用于在会话开始时执行额外逻辑（例如统计、通知等）
void UMultiplayerSessionsSubsystem::StartSession()
{
}

// UMultiplayerSessionsSubsystem::IsValidSessionInterface
// 说明：延迟获取并缓存 IOnlineSubsystem::Get()->GetSessionInterface()，便于在后续函数中调用。
// 返回：SessionInterface 是否有效
bool UMultiplayerSessionsSubsystem::IsValidSessionInterface()
{
	if (!SessionInterface)
	{
		IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
		if (Subsystem)
		{
			SessionInterface = Subsystem->GetSessionInterface();
		}
	}
	return SessionInterface.IsValid();
}

// 回调函数说明（OnCreateSessionComplete / OnFindSessionsComplete / OnJoinSessionComplete / OnDestroySessionComplete / OnStartSessionComplete）
// 这些函数为 OnlineSubsystem 的回调入口：
//  - 它们会在收到回调后立即移除对应的委托句柄（DelegateHandle），以避免重复调用或泄露句柄。
//  - 然后通过自定义的广播（MultiplayerOn...）通知外部（如 Menu）操作结果。
//  - 在 Destroy 完成且存在 bCreateSessionOnDestroy 时会触发重建逻辑。
void UMultiplayerSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}

	MultiplayerOnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}

	if (LastSessionSearch->SearchResults.Num() <= 0)
	{
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}

	MultiplayerOnFindSessionsComplete.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}

	MultiplayerOnJoinSessionComplete.Broadcast(Result);
}

void UMultiplayerSessionsSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}
	if (bWasSuccessful && bCreateSessionOnDestroy)
	{
		bCreateSessionOnDestroy = false;
		CreateSession(LastNumPublicConnections, LastMatchType);
	}
	MultiplayerOnDestroySessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
}

