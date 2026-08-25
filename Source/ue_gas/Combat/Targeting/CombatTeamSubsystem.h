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
	/** 返回两个队伍的初始外交关系；无效队伍返回 Invalid。 */
	ECombatTeamRelation GetRelation(FCombatTeamId SourceTeam, FCombatTeamId TargetTeam) const;
	/** 检查 TargetTeamTag 是否允许 SourceTeam 选择 TargetTeam。 */
	bool IsTargetTeamAllowed(FCombatTeamId SourceTeam, FCombatTeamId TargetTeam, const FGameplayTag& TargetTeamTag) const;

	/** 添加 World 初始化关系；v1 故意不支持运行时外交变化。 */
	bool AddInitialRelation(FCombatTeamId SourceTeam, FCombatTeamId TargetTeam, ECombatTeamRelation Relation);

private:
	/** 将两个 8 位 TeamId 组合成有方向的 16 位 Map 键。 */
	static uint16 MakeRelationKey(FCombatTeamId SourceTeam, FCombatTeamId TargetTeam);
	/** World 初始化阶段配置的有方向外交关系表。 */
	TMap<uint16, ECombatTeamRelation> InitialRelations;
};
