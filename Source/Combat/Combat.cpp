// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Combat/Editor/CombatMinimapEditor.h"
#endif

/** Combat 主模块；编辑器构建额外注册小地图生成菜单，运行时目标不加载任何 Editor API。 */
class FCombatModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
#if WITH_EDITOR
		CombatMinimapEditor::Startup();
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		CombatMinimapEditor::Shutdown();
#endif
		FDefaultGameModuleImpl::ShutdownModule();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FCombatModule, Combat, "Combat");

DEFINE_LOG_CATEGORY(LogCombatGame)

