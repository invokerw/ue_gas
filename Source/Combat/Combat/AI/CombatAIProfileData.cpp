#include "Combat/AI/CombatAIProfileData.h"
#include "Combat/AI/CombatAIStateTreeSchema.h"
#include "Combat/AI/CombatAIRoleTasks.h"
#include "StateTree.h"
#include "Misc/DataValidation.h"
#if WITH_EDITOR
#include "Combat/Validation/CombatAIAssetBuilder.h"
#endif

FPrimaryAssetType UCombatAIProfileData::GetCombatPrimaryAssetType() const { return FPrimaryAssetType(TEXT("CombatAIProfile")); }

namespace CombatAIProfileValidation
{
	/** 从已编译数据检查链接闭包，Cook 后也能拒绝依赖感知但关闭提供者的角色配置。 */
	bool RequiresPerception(const UStateTree& Tree, TSet<const UStateTree*>& Visited)
	{
		if (Visited.Contains(&Tree)) return false;
		Visited.Add(&Tree);
		for (int32 Index = 0; Index < Tree.GetNodes().Num(); ++Index)
		{
			const auto* Type = Tree.GetNode(Index).GetScriptStruct();
			if (Type && (Type->IsChildOf(FCombatAIPrepareRoleTask::StaticStruct()) || Type->IsChildOf(FCombatAIExecuteRoleTask::StaticStruct())
				|| Type->IsChildOf(FCombatAIRoleWaitTask::StaticStruct()) || Type->IsChildOf(FCombatAIRoleCondition::StaticStruct()))) return true;
		}
		for (const auto& State : Tree.GetStates())
			if (State.LinkedAsset && RequiresPerception(*State.LinkedAsset, Visited)) return true;
		return false;
	}
}

bool UCombatAIProfileData::ValidateRuntime(FString& Diagnostic) const
{
	if (AIProfileVersion != 1 || !GetPrimaryAssetId().IsValid() || !RootTree || !RootTree->IsReadyToRun()
		|| !RootTree->GetSchema() || !RootTree->GetSchema()->IsA<UCombatAIStateTreeSchema>()
		|| !FMath::IsFinite(IntentLifetime) || IntentLifetime <= 0 || !FMath::IsFinite(BoundaryHoldSeconds)
		|| BoundaryHoldSeconds < 0.01f || BoundaryHoldSeconds > 5.0f)
	{
		Diagnostic = TEXT("AI Profile requires valid identity/version, compiled Combat AI tree and finite positive timing parameters");
		return false;
	}
	Diagnostic.Reset();
	TSet<const UStateTree*> Visited;
	if (!bEnablePerception && CombatAIProfileValidation::RequiresPerception(*RootTree, Visited))
	{ Diagnostic = TEXT("Role StateTree requires the configured perception provider"); return false; }
	if (bEnablePerception)
	{
		const auto InRange = [](float Value, float Min, float Max) { return FMath::IsFinite(Value) && Value >= Min && Value <= Max; };
		if (!InRange(Perception.Radius, 1, 10000) || !InRange(Perception.ActiveInterval, 0.05f, 5)
			|| !InRange(Perception.IdleInterval, 0.05f, 5) || !InRange(Perception.MemorySeconds, 0, 60)
			|| Perception.VisibilityPolicy != ECombatVisibilityPolicy::None || Perception.MaxCandidates < 1 || Perception.MaxCandidates > 64
			|| Perception.MaxMemories < Perception.MaxCandidates || Perception.MaxMemories > 128
			|| !InRange(DistanceWeight, 0, 100) || !InRange(ThreatWeight, 0, 100) || !InRange(MinTargetHold, 0, 30)
			|| !InRange(TargetSwitchMargin, 0, 10000) || !InRange(LeashDistance, 80, 10000) || !InRange(MaxEngagementSeconds, 0.1f, 120)
			|| !InRange(ArrivalTolerance, 1, 500) || !InRange(RetryDelay, 0.05f, 10) || !InRange(RoutePause, 0.05f, 10)
			|| MaxAttempts < 1 || MaxAttempts > 10 || TargetPriorities.Num() > 64)
		{
			Diagnostic = TEXT("AI role requires bounded perception/memory, supported visibility, finite duty/selection/retry parameters");
			return false;
		}
		TSet<FPrimaryAssetId> Seen;
		for (const auto& Entry : TargetPriorities)
		{
			if (!Entry.Definition.IsValid() || Entry.Definition.PrimaryAssetType != FPrimaryAssetType(TEXT("CombatUnit"))
				|| Seen.Contains(Entry.Definition) || Entry.Priority < -100 || Entry.Priority > 100)
			{ Diagnostic = TEXT("AI target priorities require unique CombatUnit identities and priorities in [-100,100]"); return false; }
			Seen.Add(Entry.Definition);
		}
	}
#if WITH_EDITOR
	if (!FCombatAIAssetBuilder::ValidateRootTree(RootTree, Diagnostic)) return false;
#endif
	return true;
}

#if WITH_EDITOR
EDataValidationResult UCombatAIProfileData::IsDataValid(FDataValidationContext& Context) const
{
	const auto Parent = Super::IsDataValid(Context);
	FString Diagnostic;
	if (!ValidateRuntime(Diagnostic)) { Context.AddError(FText::FromString(Diagnostic)); return EDataValidationResult::Invalid; }
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif
