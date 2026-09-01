// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/// <summary>声明 ue_gas 主运行时模块的依赖与公共头文件搜索路径。</summary>
public class ue_gas : ModuleRules
{
	/// <summary>配置 Combat/GAS、模板玩法、导航与 UI 所需的编译依赖。</summary>
	public ue_gas(ReadOnlyTargetRules Target) : base(Target)
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
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
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

		PublicIncludePaths.AddRange(new string[] {
			"ue_gas",
			"ue_gas/Variant_Strategy",
			"ue_gas/Variant_Strategy/UI",
			"ue_gas/Variant_TwinStick",
			"ue_gas/Variant_TwinStick/AI",
			"ue_gas/Variant_TwinStick/Gameplay",
			"ue_gas/Variant_TwinStick/UI"
		});

		// 接入在线会话功能时再启用 OnlineSubsystem，避免当前基座引入无用依赖。
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// 启用 OnlineSubsystemSteam 时还必须同步在 uproject 的 Plugins 列表中开启插件。
	}
}
