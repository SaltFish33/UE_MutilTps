// 文件说明：
// ALobbyGameMode 负责大厅场景的规则与玩家进入时的处理，重载 PostLogin 以便在达到条件时触发场景切换。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "LobbyGameMode.generated.h"

/**
 * 
 */
UCLASS()
class UE_MUTILTPS_API ALobbyGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	// PostLogin 在有新的 PlayerController 登录并且已创建其 PlayerState 后被调用
	// 参数 NewPlayer：刚刚登录的玩家的 PlayerController 指针
	virtual void PostLogin(APlayerController* NewPlayer) override;
};

