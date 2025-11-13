// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// 文件说明：
// 定义插件模块 FMultiplayerSessionsModule 的接口。模块在引擎插件加载/卸载时由模块管理器调用。
// StartupModule 可在插件加载时执行一次性初始化（例如注册自定义 Slate 样式 / 控件或日志），
// ShutdownModule 在卸载时用于清理相关资源。当前模块实现保持最小化，具体扩展可在此处完成。

class FMultiplayerSessionsModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

