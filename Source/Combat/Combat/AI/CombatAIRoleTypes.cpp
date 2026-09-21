#include "Combat/AI/CombatAIRoleTypes.h"

bool FCombatAIAssignment::IsValid() const
{
	if (Home.ContainsNaN() || Route.Num() > 64) return false;
	for (const FVector& Point : Route) if (Point.ContainsNaN()) return false;
	return true;
}

void FCombatAITargetSelection::Sort(TArray<FCombatAIKnownTarget>& Candidates)
{
	Candidates.Sort([](const FCombatAIKnownTarget& A, const FCombatAIKnownTarget& B)
	{
		if (A.Priority != B.Priority) return A.Priority > B.Priority;
		if (A.Score != B.Score) return A.Score > B.Score;
		return A.StableId < B.StableId;
	});
}

int32 FCombatAITargetSelection::Select(const TArray<FCombatAIKnownTarget>& Candidates,
	TWeakObjectPtr<ACombatUnitCharacter> Current, uint32 Life, double HeldSeconds, float MinHold, float SwitchMargin)
{
	if (Candidates.IsEmpty()) return INDEX_NONE;
	const int32 Kept = Candidates.IndexOfByPredicate([&](const auto& Item) { return Item.Unit == Current && Item.Life == Life; });
	if (Kept == INDEX_NONE || Kept == 0) return 0;
	if (HeldSeconds < MinHold) return Kept;
	if (Candidates[0].Priority == Candidates[Kept].Priority && Candidates[0].Score < Candidates[Kept].Score + SwitchMargin) return Kept;
	return 0;
}
