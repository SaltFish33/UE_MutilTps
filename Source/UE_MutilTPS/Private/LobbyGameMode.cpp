// 文件说明：
// LobbyGameMode 用于处理大厅逻辑：当有玩家登录达到一定人数时触发地图传送到游戏地图。
// 这里使用 bUseSeamlessTravel 以尽量保留玩家状态并实现无缝传送（需要配合 GameSession 等配置）。

#include "LobbyGameMode.h"

#include "GameFramework/GameStateBase.h"

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// 获取当前连接的玩家数量：GameState->PlayerArray 保存当前所有已登录的 PlayerState
	int32 PlayerCounts = GameState.Get()->PlayerArray.Num();
	// 当达到至少 1 个玩家时（示范逻辑，实际项目中应按设计人数判断）触发关卡切换
	if (PlayerCounts >= 1)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			// 允许无缝传送，以在 ServerTravel 时尽量保留 PlayerController/PlayerState 等信息
			bUseSeamlessTravel = true;
			// ServerTravel 会让服务器切换到目标地图并带上 ?listen 以开启监听
			World->ServerTravel(FString("/Game/Mutil_TPS/Levels/GameMap?listen"));
		}
	}
}

