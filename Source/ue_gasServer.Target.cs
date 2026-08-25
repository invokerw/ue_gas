// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/// <summary>定义不包含 Editor 与客户端专用代码的独立 ue_gas Dedicated Server Target。</summary>
public class ue_gasServerTarget : TargetRules
{
	/// <summary>配置 UE 5.8 Server 的构建规则并链接 ue_gas 主模块。</summary>
	public ue_gasServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("ue_gas");
	}
}
