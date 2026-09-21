// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/// <summary>声明 Combat 主运行时模块的依赖与公共头文件搜索路径。</summary>
public class Combat : ModuleRules
{
	/// <summary>配置 Combat/GAS、导航与 UI 所需的编译依赖。</summary>
	public Combat(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Combat 基座在运行时直接使用 GAS、GameplayTag、AssetRegistry、网络与导航模块。
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"NavigationSystem",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"AssetRegistry",
			"Json",
			"NetCore",
			"Niagara",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
		if (Target.bBuildEditor)
		{
			// 树资产构建和编译只在编辑器使用，Server/Client 不链接编辑器模块。
			PrivateDependencyModuleNames.AddRange(new string[] { "StateTreeEditorModule", "PropertyBindingUtils", "UnrealEd" });
		}

		PublicIncludePaths.AddRange(new string[] {
			"Combat"
		});

		// 接入在线会话功能时再启用 OnlineSubsystem，避免当前基座引入无用依赖。
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// 启用 OnlineSubsystemSteam 时还必须同步在 uproject 的 Plugins 列表中开启插件。
	}
}
