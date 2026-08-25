#include "Combat/Targeting/CombatTeamSubsystem.h"

#include "Combat/Core/CombatTags.h"

ECombatTeamRelation UCombatTeamSubsystem::GetRelation(const FCombatTeamId SourceTeam, const FCombatTeamId TargetTeam) const
{
	if (!SourceTeam.IsValid() || !TargetTeam.IsValid())
	{
		return ECombatTeamRelation::Invalid;
	}
	// 相同紧凑值固定为 Friendly；不同队伍再查有方向的初始化关系。
	if (SourceTeam == TargetTeam)
	{
		return ECombatTeamRelation::Friendly;
	}
	if (const ECombatTeamRelation* Override = InitialRelations.Find(MakeRelationKey(SourceTeam, TargetTeam)))
	{
		return *Override;
	}
	return ECombatTeamRelation::Hostile;
}

bool UCombatTeamSubsystem::IsTargetTeamAllowed(
	const FCombatTeamId SourceTeam,
	const FCombatTeamId TargetTeam,
	const FGameplayTag& TargetTeamTag) const
{
	const ECombatTeamRelation Relation = GetRelation(SourceTeam, TargetTeam);
	if (TargetTeamTag == CombatTags::TargetTeam_Friendly)
	{
		return Relation == ECombatTeamRelation::Friendly;
	}
	if (TargetTeamTag == CombatTags::TargetTeam_Enemy)
	{
		return Relation == ECombatTeamRelation::Hostile;
	}
	if (TargetTeamTag == CombatTags::TargetTeam_Both)
	{
		return Relation == ECombatTeamRelation::Friendly || Relation == ECombatTeamRelation::Hostile;
	}
	return false;
}

bool UCombatTeamSubsystem::AddInitialRelation(
	const FCombatTeamId SourceTeam,
	const FCombatTeamId TargetTeam,
	const ECombatTeamRelation Relation)
{
	if (!SourceTeam.IsValid() || !TargetTeam.IsValid() || SourceTeam == TargetTeam || Relation == ECombatTeamRelation::Invalid)
	{
		return false;
	}
	InitialRelations.Add(MakeRelationKey(SourceTeam, TargetTeam), Relation);
	return true;
}

uint16 UCombatTeamSubsystem::MakeRelationKey(const FCombatTeamId SourceTeam, const FCombatTeamId TargetTeam)
{
	return static_cast<uint16>((static_cast<uint16>(SourceTeam.Value) << 8) | TargetTeam.Value);
}
