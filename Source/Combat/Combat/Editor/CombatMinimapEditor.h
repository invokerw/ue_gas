#pragma once

#include "CoreMinimal.h"

/**
 * 编辑器侧小地图生成入口；只读取当前编辑器 World 的静态几何并写入表现配置，
 * 不修改关卡 Actor、碰撞、导航、网络或战斗 Order。
 */
namespace CombatMinimapEditor
{
	/** 注册 Level Editor 菜单和控制台命令；只在 Editor target 调用。 */
	COMBAT_API void Startup();
	/** 撤销菜单、通知和编辑器委托，避免热重载后留下悬挂回调。 */
	COMBAT_API void Shutdown();
	/** 保存当前地图并按其静态网格 bounds 生成或替换对应的小地图配置。 */
	COMBAT_API bool GenerateCurrentMap(FString* OutMessage = nullptr);
}
