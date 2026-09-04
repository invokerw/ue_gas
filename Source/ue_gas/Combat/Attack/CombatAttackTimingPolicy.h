#pragma once

#include "CoreMinimal.h"

#include "Combat/Attack/CombatAttackTypes.h"

/**
 * 统一计算普攻间隔、前摇及表现播放速率：基础攻击间隔与基础前摇按 100/有效攻速缩放。
 * 例如基础间隔 1.7 秒、前摇 0.3 秒、攻速 200 时，间隔为 0.85 秒、前摇为 0.15 秒，剩余等待 0.70 秒；上下限仍由本类型约束。
 */
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

	/** 计算并一次性写出时序；基础间隔须为有限正数，基础前摇须有限非负，攻速须有限且会限制在 [20,700]。失败保持 OutTiming 原值。 */
	static bool Calculate(float BaseAttackTime, float AttackSpeed, float BaseAttackPoint, FCombatAttackTiming& OutTiming);
};
