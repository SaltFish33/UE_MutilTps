// 文件说明：
// 该文件实现插件模块 FMultiplayerSessionsModule。模块的 StartupModule 在插件加载时执行，
// ShutdownModule 在卸载或引擎关闭前执行。这里通常可放置全局注册、日志初始化或资源分配代码。
// 目前留空，因为本插件的大部分初始化在子系统或其他地方处理。

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiplayerSessions.h"

#define LOCTEXT_NAMESPACE "FMultiplayerSessionsModule"

void FMultiplayerSessionsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FMultiplayerSessionsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMultiplayerSessionsModule, MultiplayerSessions)
