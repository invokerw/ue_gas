// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/// <summary>定义不包含 Editor 与服务器专用代码的独立 ue_gas Client Target。</summary>
public class ue_gasClientTarget : TargetRules
{
	/// <summary>配置 UE 5.8 Client 的构建规则并链接 ue_gas 主模块。</summary>
	public ue_gasClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Client;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("ue_gas");
	}
}
