#include "Combat/Performance/CombatPerformanceBudget.h"

FCombatPerformanceBudgetResult FCombatPerformanceBudgetEvaluator::Evaluate(
	const FCombatRuntimeMetrics& Metrics,
	const FCombatPerformanceBudget& Budget,
	const float ServerFrameP95Ms,
	const float ServerFrameP99Ms,
	const float PerConnectionBandwidthKiBps)
{
	FCombatPerformanceBudgetResult Result;
	auto Check = [&Result](const bool bWithinBudget, const TCHAR* Name, const double Actual, const double Limit)
	{
		if (!bWithinBudget)
		{
			Result.Violations.Add(FString::Printf(TEXT("%s 超预算：实际 %.3f，上限 %.3f"), Name, Actual, Limit));
		}
	};
	Check(Metrics.Units <= Budget.MaxUnits, TEXT("单位"), Metrics.Units, Budget.MaxUnits);
	Check(Metrics.Modifiers <= Budget.MaxModifiers, TEXT("Modifier"), Metrics.Modifiers, Budget.MaxModifiers);
	Check(Metrics.Projectiles <= Budget.MaxProjectiles, TEXT("Projectile"), Metrics.Projectiles, Budget.MaxProjectiles);
	Check(Metrics.Thinkers <= Budget.MaxThinkers, TEXT("Thinker"), Metrics.Thinkers, Budget.MaxThinkers);
	Check(Metrics.Auras <= Budget.MaxAuras, TEXT("Aura"), Metrics.Auras, Budget.MaxAuras);
	Check(Metrics.AuraChildren <= Budget.MaxAuraChildren, TEXT("Aura child"), Metrics.AuraChildren, Budget.MaxAuraChildren);
	Check(Metrics.SchedulerSlots <= Budget.MaxSchedulerSlots, TEXT("Scheduler slot"), Metrics.SchedulerSlots, Budget.MaxSchedulerSlots);
	Check(Metrics.SchedulerCallbacks <= Budget.MaxSchedulerCallbacksPerFrame, TEXT("Scheduler callback/frame"), Metrics.SchedulerCallbacks, Budget.MaxSchedulerCallbacksPerFrame);
	const float EffectiveP95Ms = ServerFrameP95Ms >= 0.0f ? ServerFrameP95Ms
		: (Metrics.ServerFrameSamples > 0 ? Metrics.ServerFrameP95Ms : -1.0f);
	const float EffectiveP99Ms = ServerFrameP99Ms >= 0.0f ? ServerFrameP99Ms
		: (Metrics.ServerFrameSamples > 0 ? Metrics.ServerFrameP99Ms : -1.0f);
	const float EffectiveBandwidthKiBps = PerConnectionBandwidthKiBps >= 0.0f ? PerConnectionBandwidthKiBps
		: (Metrics.NetworkConnections > 0 ? Metrics.MaxConnectionOutKiBps : -1.0f);
	if (EffectiveP95Ms >= 0.0f) Check(EffectiveP95Ms <= Budget.MaxServerFrameP95Ms, TEXT("Server frame P95 ms"), EffectiveP95Ms, Budget.MaxServerFrameP95Ms);
	if (EffectiveP99Ms >= 0.0f) Check(EffectiveP99Ms <= Budget.MaxServerFrameP99Ms, TEXT("Server frame P99 ms"), EffectiveP99Ms, Budget.MaxServerFrameP99Ms);
	if (EffectiveBandwidthKiBps >= 0.0f) Check(EffectiveBandwidthKiBps <= Budget.MaxPerConnectionBandwidthKiBps, TEXT("Per-connection KiB/s"), EffectiveBandwidthKiBps, Budget.MaxPerConnectionBandwidthKiBps);
	Result.bPassed = Result.Violations.IsEmpty();
	return Result;
}
