#pragma once

#include "CoreMinimal.h"

#include "Combat/Attack/CombatAttackTypes.h"

/** 集中实现 M4 冻结的 BAT、IAS、attack point 与动画投影公式。 */
struct UE_GAS_API FCombatAttackTimingPolicyV1
{
	/** Numeric Policy v1 接受的最小有效攻击速度。 */
	static constexpr float MinAttackSpeed = 20.0f;
	/** Numeric Policy v1 接受的最大有效攻击速度。 */
	static constexpr float MaxAttackSpeed = 700.0f;
	/** 防止极端攻速产生零间隔的下限。 */
	static constexpr float MinAttackInterval = 0.20f;
	/** 防止非法低攻速把 Order 永久挂起的上限。 */
	static constexpr float MaxAttackInterval = 10.0f;

	/** 对有限输入计算完整时序；任何非法输入返回 false 且不写半成品。 */
	static bool Calculate(float BaseAttackTime, float AttackSpeed, float BaseAttackPoint, FCombatAttackTiming& OutTiming);
};
