// 头文件说明：
// 该头文件声明 UMultiplayerSessionsSubsystem。类职责：
//  - 封装对 IOnlineSession 的调用（创建/查找/加入/销毁/开始）
//  - 提供一组可被 UI（例如 Menu）绑定的自定义广播委托，以便 UI 对会话事件做出响应
//  - 管理委托句柄与会话设置/搜索对象的生命周期，避免在线子系统回调泄露

// 自定义委托说明：
//  - FMultiplayerOnCreateSessionComplete: 动态多播委托（Blueprint 可绑定），用于通知创建会话结果（成功或失败）
//  - FMultiplayerOnFindSessionsComplete: 非动态委托，包含搜索结果数组与成功标志（传输大量数据时避免动态委托开销）
//  - FMultiplayerOnJoinSessionComplete: 非动态委托，传递 EOnJoinSessionCompleteResult 枚举
//  - FMultiplayerOnDestroySessionComplete / FMultiplayerOnStartSessionComplete: 动态多播委托，用于 Blueprint 可绑定的销毁/开始事件

// 类说明：UMultiplayerSessionsSubsystem 继承自 UGameInstanceSubsystem，意味着其实例与 UGameInstance 绑定，
// 生命周期与 GameInstance 保持一致，适合管理跨关卡的在线会话逻辑与状态。

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"

#include "MultiplayerSessionsSubsystem.generated.h"

//
// Delcaring our own custom delegates for the Menu class to bind callbacks to
//
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnCreateSessionComplete, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnJoinSessionComplete, EOnJoinSessionCompleteResult::Type Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnDestroySessionComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnStartSessionComplete, bool, bWasSuccessful);

/**
 * 
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMultiplayerSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UMultiplayerSessionsSubsystem();

	//
	// To handle session functionality. The Menu class will call these
	//
	void CreateSession(int32 NumPublicConnections, FString MatchType);
	void FindSessions(int32 MaxSearchResults);
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);
	void DestroySession();
	void StartSession();

	bool IsValidSessionInterface();

	//
	// Our own custom delegates for the Menu class to bind callbacks to
	//
	FMultiplayerOnCreateSessionComplete MultiplayerOnCreateSessionComplete;
	FMultiplayerOnFindSessionsComplete MultiplayerOnFindSessionsComplete;
	FMultiplayerOnJoinSessionComplete MultiplayerOnJoinSessionComplete;
	FMultiplayerOnDestroySessionComplete MultiplayerOnDestroySessionComplete;
	FMultiplayerOnStartSessionComplete MultiplayerOnStartSessionComplete;

protected:

	//
	// Internal callbacks for the delegates we'll add to the Online Session Interface delegate list.
	// Thise don't need to be called outside this class.
	//
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);

private:
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;

	//
	// To add to the Online Session Interface delegate list.
	// We'll bind our MultiplayerSessionsSubsystem internal callbacks to these.
	//
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;
	FDelegateHandle DestroySessionCompleteDelegateHandle;
	FOnStartSessionCompleteDelegate StartSessionCompleteDelegate;
	FDelegateHandle StartSessionCompleteDelegateHandle;

	bool bCreateSessionOnDestroy{ false };
	int32 LastNumPublicConnections;
	FString LastMatchType;
};

// 方法说明（公共接口）：
//  - CreateSession(int32 NumPublicConnections, FString MatchType)
//      创建会话并将 MatchType 写入 SessionSettings（用于后续查找过滤），如果当前已存在会话会先销毁后重建。
//  - FindSessions(int32 MaxSearchResults)
//      使用 SEARCH_PRESENCE 查询会话，结果会通过 MultiplayerOnFindSessionsComplete 广播回调。
//  - JoinSession(const FOnlineSessionSearchResult& SessionResult)
//      尝试加入找到的会话。结果通过 MultiplayerOnJoinSessionComplete 广播。
//  - DestroySession()
//      销毁当前会话。若在销毁后需要重建（bCreateSessionOnDestroy 为 true）则在 OnDestroySessionComplete 中处理。
//  - StartSession()
//      启动会话（占位，可扩展）。

// 成员变量说明（私有）：
//  - SessionInterface: 缓存的 IOnlineSessionPtr，延迟初始化以适配 OnlineSubsystem 的运行时可用性。
//  - LastSessionSettings: 在 CreateSession 时保存的会话设置，若创建失败或需重建时可重用。
//  - LastSessionSearch: 在 FindSessions 时保存的搜索对象与结果。
//  - FOn...Delegate 与 FDelegateHandle: 用于在向 SessionInterface 注册回调时保存句柄，以便在回调后移除。
//  - bCreateSessionOnDestroy / LastNumPublicConnections / LastMatchType: 用于在“已存在会话需要先销毁再创建”场景时保存重建信息。

