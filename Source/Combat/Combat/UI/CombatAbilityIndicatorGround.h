#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

/** 技能指示器的地面接收约定；只用于本地输入和渲染，不替代服务器目标规则。 */
namespace CombatAbilityIndicatorGround
{
	/** 地图地面显式 Block，其他组件默认 Ignore；不复用选单位和普通移动的 Visibility。 */
	constexpr ECollisionChannel TraceChannel = ECC_GameTraceChannel5;
	/** CustomStencil 最高位供地面指示器使用，低七位保留给其他表现。 */
	constexpr int32 StencilBit = 128;
	/** 与 M_CombatAbilityIndicator 一致：允许最大 60 度坡面，拒绝平台侧壁。 */
	constexpr float MinNormalZ = 0.5f;
	/** 只接受同时配置查询/渲染标记的向上地面；缺失组件、无穷坐标或角色命中均失败。 */
	COMBAT_API bool IsGroundHit(const FHitResult& Hit);
}
