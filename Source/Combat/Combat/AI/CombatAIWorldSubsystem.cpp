#include "Combat/AI/CombatAIWorldSubsystem.h"

#include "Engine/World.h"

namespace CombatAIWorldBudget
{
	constexpr int32 MaxDurationSamples = 1024;
}

void UCombatAIWorldSubsystem::RefreshBudgetSlice()
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (BudgetSliceTime == Now)
	{
		return;
	}
	BudgetSliceTime = Now;
	PerceptionUsed = 0;
	EQSStartsUsed = 0;
}

bool UCombatAIWorldSubsystem::TryAcquirePerception()
{
	RefreshBudgetSlice();
	++Counters.PerceptionRequests;
	if (PerceptionUsed >= PerceptionPerSlice)
	{
		++Counters.PerceptionDeferred;
		return false;
	}
	++PerceptionUsed;
	++Counters.PerceptionGranted;
	return true;
}

uint64 UCombatAIWorldSubsystem::AllocateEQSQueryToken()
{
	do
	{
		++NextQueryToken;
		if (NextQueryToken == 0) ++NextQueryToken;
	}
	while (ActiveQueries.Contains(NextQueryToken));
	return NextQueryToken;
}

bool UCombatAIWorldSubsystem::TryStartEQS(const uint64 QueryToken)
{
	RefreshBudgetSlice();
	++Counters.EQSRequests;
	if (!QueryToken || ActiveQueries.Contains(QueryToken))
	{
		return false;
	}
	if (EQSStartsUsed >= EQSStartsPerSlice)
	{
		++Counters.EQSDeferred;
		return false;
	}
	++EQSStartsUsed;
	++Counters.EQSGranted;
	ActiveQueries.Add(QueryToken);
	Counters.ActiveEQS = ActiveQueries.Num();
	Counters.PeakActiveEQS = FMath::Max(Counters.PeakActiveEQS, Counters.ActiveEQS);
	return true;
}

bool UCombatAIWorldSubsystem::FinishEQS(const uint64 QueryToken, const ECombatAIEQSResult Result,
	const double ElapsedMilliseconds)
{
	if (!ActiveQueries.Remove(QueryToken))
	{
		return false;
	}
	Counters.ActiveEQS = ActiveQueries.Num();
	switch (Result)
	{
	case ECombatAIEQSResult::Succeeded: ++Counters.EQSSucceeded; break;
	case ECombatAIEQSResult::Failed: ++Counters.EQSFailed; break;
	case ECombatAIEQSResult::Cancelled: ++Counters.EQSCancelled; break;
	case ECombatAIEQSResult::Stale: ++Counters.EQSStale; break;
	}
	if (FMath::IsFinite(ElapsedMilliseconds) && ElapsedMilliseconds >= 0.0)
	{
		if (EQSDurationsMilliseconds.Num() == CombatAIWorldBudget::MaxDurationSamples)
		{
			EQSDurationsMilliseconds.RemoveAt(0, 1, EAllowShrinking::No);
		}
		EQSDurationsMilliseconds.Add(static_cast<float>(ElapsedMilliseconds));
	}
	return true;
}

float UCombatAIWorldSubsystem::Percentile(const TArray<float>& SortedSamples, const float Quantile)
{
	if (SortedSamples.IsEmpty())
	{
		return 0.0f;
	}
	const int32 Index = FMath::Clamp(FMath::CeilToInt(Quantile * SortedSamples.Num()) - 1,
		0, SortedSamples.Num() - 1);
	return SortedSamples[Index];
}

FCombatAIWorldBudgetSnapshot UCombatAIWorldSubsystem::GetSnapshot() const
{
	FCombatAIWorldBudgetSnapshot Result = Counters;
	TArray<float> Sorted = EQSDurationsMilliseconds;
	Sorted.Sort();
	Result.EQSP95Milliseconds = Percentile(Sorted, 0.95f);
	Result.EQSP99Milliseconds = Percentile(Sorted, 0.99f);
	return Result;
}

float UCombatAIWorldSubsystem::ComputeStableInitialDelay(const uint32 StableUnitId, const float IntervalSeconds)
{
	if (!FMath::IsFinite(IntervalSeconds) || IntervalSeconds <= 0.0f)
	{
		return 0.0f;
	}
	uint32 Hash = StableUnitId + 0x9e3779b9u;
	Hash ^= Hash >> 16;
	Hash *= 0x7feb352du;
	Hash ^= Hash >> 15;
	Hash *= 0x846ca68bu;
	Hash ^= Hash >> 16;
	const float UnitFraction = static_cast<float>(Hash & 0x00ffffffu) / 16777216.0f;
	return UnitFraction * IntervalSeconds;
}

#if WITH_DEV_AUTOMATION_TESTS
void UCombatAIWorldSubsystem::SetFrameLimitsForTesting(const int32 PerceptionLimit, const int32 EQSStartLimit)
{
	PerceptionPerSlice = FMath::Max(0, PerceptionLimit);
	EQSStartsPerSlice = FMath::Max(0, EQSStartLimit);
	BudgetSliceTime = -DBL_MAX;
	PerceptionUsed = 0;
	EQSStartsUsed = 0;
}
#endif

void UCombatAIWorldSubsystem::Deinitialize()
{
	ActiveQueries.Reset();
	EQSDurationsMilliseconds.Reset();
	Counters.ActiveEQS = 0;
	NextQueryToken = 0;
	Super::Deinitialize();
}
