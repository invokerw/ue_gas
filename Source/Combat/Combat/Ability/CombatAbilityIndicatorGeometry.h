#pragma once

#include "CoreMinimal.h"

/** 主 Action 的几何语义；只供展示，不构成新的命中或施法范围规则。 */
enum class ECombatIndicatorShape : uint8 { None, Circle, Line };

/** 从当前等级 Action 解析的瞬时形状，不复制，不缓存平衡数值。半径/长度单位为厘米。 */
struct COMBAT_API FCombatAbilityIndicatorGeometry
{
	ECombatIndicatorShape Shape = ECombatIndicatorShape::None;
	float Radius = 0.0f;
	float Length = 0.0f;
	/** 自身 Thinker / 无目标范围以施法者为锚点，直线始终从施法者发出。 */
	bool bCenterOnCaster = false;

	/** 直线弹体按三维方向飞行；返回其最大轨迹在地面 XY 上的长度，零向量沿用技能的朝向回退。 */
	float GetPlanarLineLength(FVector Source, FVector Target, FVector Forward) const
	{
		FVector Travel = Target - Source;
		if (Travel.IsNearlyZero()) Travel = Forward;
		return Length * Travel.GetSafeNormal().Size2D();
	}
};
