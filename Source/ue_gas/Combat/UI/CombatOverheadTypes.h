#pragma once

#include "CoreMinimal.h"

#include "CombatOverheadTypes.generated.h"

/** 头顶战斗跳字的表现分类；数值本身始终来自服务器实际落账结果。 */
UENUM(BlueprintType)
enum class ECombatFloatingTextType : uint8
{
	PhysicalDamage UMETA(DisplayName="物理伤害"),
	MagicalDamage UMETA(DisplayName="魔法伤害"),
	PureDamage UMETA(DisplayName="纯粹伤害"),
	Healing UMETA(DisplayName="治疗")
};
