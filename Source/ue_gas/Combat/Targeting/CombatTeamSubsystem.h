#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatTeamSubsystem.generated.h"

/** 集中计算 TeamId 外交关系和 TargetTeam 标签许可，避免业务层直接比较队伍值。 */
UCLASS()
class UE_GAS_API UCombatTeamSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 相同有效队伍固定为友军，不同队伍优先查有方向的配置关系，未配置时视为敌军；任一队伍无效时返回 Invalid。 */
	ECombatTeamRelation GetRelation(FCombatTeamId SourceTeam, FCombatTeamId TargetTeam) const;
	/** Friendly 只接受友军，Enemy 只接受敌军，Both 接受这两者但不包含中立；中立许可由目标规则中的单独开关处理。 */
	bool IsTargetTeamAllowed(FCombatTeamId SourceTeam, FCombatTeamId TargetTeam, const FGameplayTag& TargetTeamTag) const;

	/** 配置从来源队伍到目标队伍的单向关系，重复键会覆盖；调用方应在 World 初始化阶段使用。无效队伍、相同队伍或 Invalid 关系返回 false；此函数不检查当前是否仍处于初始化阶段。 */
	bool AddInitialRelation(FCombatTeamId SourceTeam, FCombatTeamId TargetTeam, ECombatTeamRelation Relation);

private:
	/** 将两个 8 位 TeamId 组合成有方向的 16 位 Map 键。 */
	static uint16 MakeRelationKey(FCombatTeamId SourceTeam, FCombatTeamId TargetTeam);
	/** World 初始化阶段配置的有方向外交关系表。 */
	TMap<uint16, ECombatTeamRelation> InitialRelations;
};
