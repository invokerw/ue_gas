#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Unit/CombatProgressionComponent.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Targeting/CombatTeamSubsystem.h"
#include "CombatPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"

UDefenseDirectorComponent::UDefenseDirectorComponent() { PrimaryComponentTick.bCanEverTick = false; }

bool UDefenseDirectorComponent::InitializeForRun(UDefenseRunData* Data, ADefenseObjective* Core, ADefenseGameState* State)
{
	if (bInitialized || bEnding || !GetOwner() || !GetOwner()->HasAuthority() || !GetWorld() || GetNetMode() == NM_Client
		|| !IsValid(Core) || !IsValid(State) || Core->GetWorld() != GetWorld() || State->GetWorld() != GetWorld()) return false;
	FString Error;
	if (!Data || !Data->ValidateRuntime(Error) || !Core->GetUnitDefinitionId().IsValid()
		|| Core->GetLifeState() != ECombatLifeState::Alive) return false;
	auto* Teams = GetWorld()->GetSubsystem<UCombatTeamSubsystem>();
	for (const UDefenseWaveData* Wave : Data->Waves)
		for (const auto& G : Wave->SpawnGroups)
			if (Teams->GetRelation(G.UnitData->InitialTeamId, Core->GetCombatTeamId()) != ECombatTeamRelation::Hostile) return false;
	// 复制计划及每个被引用的数值定义；编辑原资产不会改变已开始对局。
	RunData = DuplicateObject<UDefenseRunData>(Data, this);
	RunData->Waves.Reset();
	for (UDefenseWaveData* Wave : Data->Waves)
	{
		auto* Copy = DuplicateObject<UDefenseWaveData>(Wave, RunData);
		for (auto& Group : Copy->SpawnGroups) Group.UnitData = DuplicateObject<UCombatUnitData>(Group.UnitData, Copy);
		if (Copy->FixedAugment) Copy->FixedAugment = DuplicateObject<UCombatModifierData>(Copy->FixedAugment, Copy);
		RunData->Waves.Add(Copy);
	}
	Objective = Core; CoreLife = Core->GetLifeGeneration(); GameState = State;
	View.RunId = FGuid::NewGuid(); View.TotalWaves = RunData->Waves.Num(); View.Objective = Core;
	bInitialized = true;
	Core->GetCombatLifecycleComponent()->OnDied().AddUObject(this, &ThisClass::OnCoreDied);
	EnterPhase(EDefensePhase::Preparation, INDEX_NONE, RunData->PreparationDuration);
	auto* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>();
	MonitorSchedule = Scheduler->ScheduleRepeating(this, 0.1, 0.1, -100, ECombatCatchUpPolicy::Coalesce,
		FCombatScheduledDelegate::CreateUObject(this, &ThisClass::Monitor));
	SchedulePhase(RunData->PreparationDuration);
	return !Terminal.bTerminal;
}

bool UDefenseDirectorComponent::IsCurrent(const FGuid ExpectedRun, const uint32 Generation) const
{
	return bInitialized && !bEnding && !Terminal.bTerminal && GetOwner() && GetOwner()->HasAuthority()
		&& GetWorld() && GetNetMode() != NM_Client && View.RunId == ExpectedRun && GetPhaseGeneration() == Generation;
}
bool UDefenseDirectorComponent::IsCoreAlive() const
{
	return Objective.IsValid() && !Objective->IsActorBeingDestroyed() && Objective->GetLifeGeneration() == CoreLife
		&& Objective->GetLifeState() == ECombatLifeState::Alive && Objective->GetCombatAttributeSet()->GetHealth() > 0;
}
void UDefenseDirectorComponent::EnterPhase(const EDefensePhase Phase, const int32 Wave, const float Duration)
{
	GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>()->Cancel(PhaseSchedule); PhaseSchedule = {};
	++View.PhaseGeneration; View.Phase = Phase; View.WaveIndex = Wave;
	View.Deadline = GetWorld()->GetTimeSeconds() + Duration;
	bCompletionPending = false;
	UE_LOG(LogCombat, Display, TEXT("DefensePhase Run=%s Generation=%lld Wave=%d Phase=%d"),
		*View.RunId.ToString(), View.PhaseGeneration, Wave, int32(Phase));
	Publish();
}
void UDefenseDirectorComponent::SchedulePhase(const float Delay)
{
	auto* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>();
	Scheduler->Cancel(PhaseSchedule);
	PhaseSchedule = Scheduler->ScheduleOnce(this, Delay, -50,
		FCombatScheduledDelegate::CreateUObject(this, &ThisClass::Advance, View.RunId, GetPhaseGeneration()));
	if (!PhaseSchedule.IsValid()) Finish(false, TEXT("ScheduleFailed"));
}
void UDefenseDirectorComponent::Advance(const FCombatScheduledTickContext& Tick, const FGuid ExpectedRun, const uint32 Generation)
{
	if (!IsCurrent(ExpectedRun, Generation)) return;
	PhaseSchedule = {};
	if (!IsCoreAlive()) { Finish(false, TEXT("ObjectiveLost")); return; }
	if (View.Phase != EDefensePhase::WaveCombat)
	{
		const int32 Next = View.WaveIndex + 1;
		if (!RunData->Waves.IsValidIndex(Next)) { Finish(false, TEXT("MissingWave")); return; }
		GroupIndex = SpawnIndex = SpawnFailures = 0;
		View.PendingSpawns = 0;
		for (const auto& Group : RunData->Waves[Next]->SpawnGroups) View.PendingSpawns += Group.Count;
		EnterPhase(EDefensePhase::WaveCombat, Next, RunData->Waves[Next]->MaxDuration);
		SchedulePhase(0);
		return;
	}
	if (Tick.ActualTime >= View.Deadline) { Finish(false, TEXT("WaveTimeout")); return; }
	if (bCompletionPending)
	{
		bCompletionPending = false;
		if (View.PendingSpawns != 0 || View.RemainingEnemies != 0) return;
		// 所有死亡已从公共生命周期确认；异常销毁/复活会在此处再次复核。
		for (const auto& E : Enemies)
			if (!E.bDead || !E.Unit.IsValid() || E.Unit->GetLifeGeneration() != E.Life
				|| E.Unit->GetLifeState() != ECombatLifeState::Dead) { Finish(false, TEXT("InvalidClear")); return; }
		if (!RewardWave()) { Finish(false, TEXT("RewardFailed")); return; }
		if (RunData->Waves[View.WaveIndex]->bBossWave) { Finish(true, TEXT("BossDefeated")); return; }
		ClearEnemies();
		EnterPhase(View.WaveIndex == 4 ? EDefensePhase::BossPreparation : EDefensePhase::Intermission,
			View.WaveIndex, RunData->IntermissionDuration);
		SchedulePhase(RunData->IntermissionDuration);
		return;
	}
	SpawnNext();
}
void UDefenseDirectorComponent::SpawnNext()
{
	const auto& Groups = RunData->Waves[View.WaveIndex]->SpawnGroups;
	while (Groups.IsValidIndex(GroupIndex) && SpawnIndex >= Groups[GroupIndex].Count)
	{ ++GroupIndex; SpawnIndex = 0; }
	if (!Groups.IsValidIndex(GroupIndex)) { QueueWaveCompletion(); return; }
	const auto& G = Groups[GroupIndex];
	if (!SpawnEnemy(G))
	{
		UE_LOG(LogCombat, Display, TEXT("DefenseSpawnRetry Wave=%d Group=%d Attempt=%d"), View.WaveIndex, GroupIndex, SpawnFailures + 1);
		if (++SpawnFailures > RunData->SpawnRetryLimit) Finish(false, TEXT("SpawnFailed"));
		else SchedulePhase(0.5f);
		return;
	}
	if (Terminal.bTerminal) return;
	SpawnFailures = 0; ++SpawnIndex; --View.PendingSpawns;
	Publish();
	if (View.PendingSpawns > 0) SchedulePhase(G.SpawnInterval);
	else QueueWaveCompletion();
}

bool UDefenseDirectorComponent::FindSafeLocation(const ACombatUnitCharacter& Unit, const FVector Desired, FVector& Out) const
{
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav) return false;
	const auto& Props = Unit.GetNavAgentPropertiesRef();
	const float Half = Unit.GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Radius = Unit.GetCapsuleComponent()->GetScaledCapsuleRadius();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DefenseSpawn), false, &Unit);
	for (int32 I=0; I<9; ++I)
	{
		const FVector Offset = I == 0 ? FVector::ZeroVector : FVector(FMath::Cos((I-1)*PI/4)*180, FMath::Sin((I-1)*PI/4)*180, 0);
		FNavLocation Projected;
		if (!Nav->ProjectPointToNavigation(Desired + Offset, Projected, FVector(100,100,250), &Props)) continue;
		const FVector Candidate = Projected.Location + FVector(0,0,Half + 3);
		if (!GetWorld()->OverlapBlockingTestByProfile(Candidate, FQuat::Identity, TEXT("CombatUnit"),
			FCollisionShape::MakeCapsule(Radius, Half), Query)) { Out = Candidate; return true; }
	}
	return false;
}
bool UDefenseDirectorComponent::SpawnEnemy(const FDefenseSpawnGroup& G)
{
	FVector Position;
	const auto* Defaults = G.UnitClass->GetDefaultObject<ACombatUnitCharacter>();
	if (!FindSafeLocation(*Defaults, RunData->Routes[G.RouteIndex].SpawnLocation, Position))
	{ UE_LOG(LogCombat, Display, TEXT("DefenseSpawnRejected Reason=NoSafeNavigationPoint Class=%s"), *GetNameSafe(G.UnitClass)); return false; }
	const FTransform Transform(FRotator::ZeroRotator, Position);
	auto* Enemy = GetWorld()->SpawnActorDeferred<ACombatUnitCharacter>(G.UnitClass, Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
	if (!Enemy) { UE_LOG(LogCombat, Display, TEXT("DefenseSpawnRejected Reason=SpawnCollision")); return false; }
	if (!Enemy->InitializeFromUnitData(G.UnitData)) { UE_LOG(LogCombat, Display, TEXT("DefenseSpawnRejected Reason=UnitInitialization")); Enemy->Destroy(); return false; }
	Enemy->FinishSpawning(Transform);
	if (!IsValid(Enemy) || Enemy->IsActorBeingDestroyed()) return false;
	RegisterEnemy(Enemy);
	Enemy->GetCombatAIBrainComponent()->ConfigureProfile(nullptr);
	auto* Orders = Enemy->GetCombatOrderComponent();
	Orders->MaxChaseDuration = RunData->Waves[View.WaveIndex]->MaxDuration;
	bool Queue = false;
	for (const FVector& P : RunData->Routes[G.RouteIndex].Waypoints)
	{
		FCombatOrderRequest Move;
		Move.Type = ECombatOrderType::MoveToPoint; Move.TargetLocation = P; Move.bHasTargetLocation = true;
		if (!Orders->IssueOrder(Move, Queue).bSuccess) { Finish(false, TEXT("RouteRejected")); return true; }
		Queue = true;
	}
	FCombatOrderRequest Attack;
	Attack.Type = ECombatOrderType::AttackTarget; Attack.TargetUnit = Objective.Get();
	if (!Orders->IssueOrder(Attack, Queue).bSuccess) Finish(false, TEXT("AttackRejected"));
	UE_LOG(LogCombat, Display, TEXT("DefenseSpawn Wave=%d Route=%d Boss=%d Unit=%s Definition=%s Life=%u"),
		View.WaveIndex, G.RouteIndex, G.bBoss, *Enemy->GetName(), *Enemy->GetUnitDefinitionId().ToString(), Enemy->GetLifeGeneration());
	return true;
}
void UDefenseDirectorComponent::RegisterEnemy(ACombatUnitCharacter* Enemy)
{
	Enemies.Add({Enemy, Enemy->GetLifeGeneration(), GetPhaseGeneration(), false});
	++View.RemainingEnemies;
	Enemy->GetCombatLifecycleComponent()->OnDied().AddUObject(this, &ThisClass::OnEnemyDied);
}
void UDefenseDirectorComponent::OnEnemyDied(ACombatUnitCharacter* Unit, const FCombatEventContext& Cause)
{
	if (!IsCurrent(View.RunId, GetPhaseGeneration()) || View.Phase != EDefensePhase::WaveCombat) return;
	for (auto& E : Enemies)
	{
		if (E.Unit.Get() != Unit || E.bDead || E.Life != Unit->GetLifeGeneration() || E.Phase != GetPhaseGeneration()
			|| Unit->GetLifeState() != ECombatLifeState::Dead) continue;
		E.bDead = true; --View.RemainingEnemies;
		QueueWaveCompletion(); Publish(); break;
	}
}
void UDefenseDirectorComponent::QueueWaveCompletion()
{
	if (!Terminal.bTerminal && View.PendingSpawns == 0 && View.RemainingEnemies == 0 && !bCompletionPending)
	{ bCompletionPending = true; SchedulePhase(0); }
}
void UDefenseDirectorComponent::OnCoreDied(ACombatUnitCharacter* Unit, const FCombatEventContext& Cause)
{
	if (bInitialized && Unit == Objective.Get() && Unit->GetLifeGeneration() == CoreLife)
		Finish(false, TEXT("ObjectiveDestroyed"));
}
void UDefenseDirectorComponent::Monitor(const FCombatScheduledTickContext& Tick)
{
	if (!IsCurrent(View.RunId, GetPhaseGeneration())) return;
	if (!IsCoreAlive()) { Finish(false, TEXT("ObjectiveLost")); return; }
	for (const auto& E : Enemies)
	{
		if (!E.Unit.IsValid() || E.Unit->IsActorBeingDestroyed() || E.Unit->GetLifeGeneration() != E.Life)
		{ Finish(false, TEXT("EnemyIdentityLost")); return; }
		if (!E.bDead && E.Unit->GetLifeState() == ECombatLifeState::Dead)
		{ Finish(false, TEXT("UnconfirmedDeath")); return; }
	}
	if (View.Phase == EDefensePhase::WaveCombat && Tick.ActualTime >= View.Deadline)
	{ Finish(false, TEXT("WaveTimeout")); return; }
	ServicePlayers(Tick.ActualTime);
	Publish();
}
void UDefenseDirectorComponent::Publish()
{
	if (Objective.IsValid())
	{
		View.ObjectiveHealth = Objective->GetCombatAttributeSet()->GetHealth();
		View.ObjectiveMaxHealth = Objective->GetCombatAttributeSet()->GetMaxHealth();
	}
	if (GameState.IsValid()) GameState->Publish(View);
}
void UDefenseDirectorComponent::Finish(bool bVictory, const FName Reason)
{
	if (Terminal.bTerminal || bEnding) return;
	if (bVictory && !IsCoreAlive()) { Finish(false, TEXT("ObjectiveLost")); return; }
	Terminal = {true, bVictory, Reason};
	View.TerminalReason = Reason;
	View.Phase = bVictory ? EDefensePhase::Victory : EDefensePhase::Defeat;
	++View.PhaseGeneration; View.Deadline = 0; View.PendingSpawns = 0;
	GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>()->CancelAllForOwner(this);
	PhaseSchedule = MonitorSchedule = {};
	if (Objective.IsValid()) Objective->GetCombatLifecycleComponent()->OnDied().RemoveAll(this);
	ClearEnemies();
	// 结果先冻结；终局不再授予死亡期间的奖励或恢复英雄。
	for (auto& Pair : Players) { Pair.Value.PendingExperience = 0; Pair.Value.PendingAugments.Reset(); }
	Publish();
	UE_LOG(LogCombat, Display, TEXT("DefenseResult Run=%s Result=%s Wave=%d Reason=%s"),
		*View.RunId.ToString(), bVictory ? TEXT("Victory") : TEXT("Defeat"), View.WaveIndex, *Reason.ToString());
}
void UDefenseDirectorComponent::ClearEnemies()
{
	TArray<FDefenseEnemyRecord> Removed = MoveTemp(Enemies);
	Enemies.Reset(); View.RemainingEnemies = 0;
	for (const auto& Entry : Removed)
	{
		if (auto* Unit = Entry.Unit.Get())
		{
			Unit->GetCombatLifecycleComponent()->OnDied().RemoveAll(this);
			AController* Controller = Unit->GetController();
			Unit->GetCombatOrderComponent()->StopAllOrders(FGameplayTag());
			Unit->Destroy();
			if (IsValid(Controller)) Controller->Destroy();
		}
	}
}

bool UDefenseDirectorComponent::RegisterPlayer(ACombatPlayerController* Player, ACombatUnitCharacter* Hero)
{
	if (!IsCurrent(View.RunId, GetPhaseGeneration()) || View.Phase != EDefensePhase::Preparation || !IsValid(Player)
		|| !IsValid(Hero) || Hero->GetWorld() != GetWorld() || Player->GetWorld() != GetWorld()
		|| Hero->GetResourceOwnerPlayerController() != Player) return false;
	if (Players.Contains(Player)) return Players.FindChecked(Player).Hero == Hero;
	FDefensePlayerRecord Record; Record.Hero = Hero; Players.Add(Player, Record);
	return true;
}
void UDefenseDirectorComponent::RemovePlayer(ACombatPlayerController* Player)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || GetNetMode() == NM_Client) return;
	if (auto* Record = Players.Find(Player))
	{
		if (auto* Hero = Record->Hero.Get())
			for (auto Handle : Record->AugmentHandles) Hero->GetCombatModifierComponent()->RemoveModifier(Handle);
	}
	Players.Remove(Player);
}
bool UDefenseDirectorComponent::RewardWave()
{
	if (!IsCurrent(View.RunId, GetPhaseGeneration()) || View.Phase != EDefensePhase::WaveCombat) return false;
	const UDefenseWaveData* Wave = RunData->Waves[View.WaveIndex];
	for (auto& Pair : Players)
	{
		auto* Player = Pair.Key.Get(); auto& Record = Pair.Value;
		if (!Player || Record.RewardedWaves.Contains(View.WaveIndex)) continue;
		Record.RewardedWaves.Add(View.WaveIndex);
		if (Wave->GoldReward > 0 && !Player->GetCombatEconomyComponent()->AddGold(Wave->GoldReward, TEXT("DefenseWave"))) return false;
		Record.PendingExperience += Wave->ExperienceReward;
		if (Wave->FixedAugment && !Record.OwnedAugments.Contains(Wave->FixedAugment->GetPrimaryAssetId()))
		{
			Record.OwnedAugments.Add(Wave->FixedAugment->GetPrimaryAssetId());
			Record.PendingAugments.Add(Wave->FixedAugment);
		}
	}
	ServicePlayers(GetWorld()->GetTimeSeconds());
	return !Terminal.bTerminal;
}
void UDefenseDirectorComponent::ServicePlayers(const double Now)
{
	for (auto& Pair : Players)
	{
		auto& R = Pair.Value; auto* Hero = R.Hero.Get();
		if (!Pair.Key.IsValid() || !IsValid(Hero)) continue;
		if (Hero->GetLifeState() == ECombatLifeState::Dead)
		{
			if (R.DeadLife != Hero->GetLifeGeneration())
			{ R.DeadLife = Hero->GetLifeGeneration(); R.RespawnAt = Now + RunData->RespawnDelay; R.RespawnRetries = 0; }
			if (Now < R.RespawnAt) continue;
			FVector Safe;
			if (!FindSafeLocation(*Hero, RunData->RespawnLocation, Safe)
				|| !Hero->GetCombatLifecycleComponent()->RespawnAtLocation(Safe))
			{
				if (++R.RespawnRetries > 10) { Finish(false, TEXT("RespawnBlocked")); return; }
				R.RespawnAt = Now + 0.5; continue;
			}
			R.DeadLife = 0;
		}
		if (Hero->GetLifeState() != ECombatLifeState::Alive) continue;
		if (R.PendingExperience > 0)
		{
			const int32 XP = R.PendingExperience; R.PendingExperience = 0;
			if (!Hero->GetCombatProgressionComponent()->AddExperience(XP)) { Finish(false, TEXT("ExperienceFailed")); return; }
		}
		const auto Pending = MoveTemp(R.PendingAugments);
		R.PendingAugments.Reset();
		for (const auto& Augment : Pending)
		{
			FCombatModifierApplyRequest Request;
			Request.ModifierData = Augment.Get(); Request.Source = Hero;
			const auto Applied = Hero->GetCombatModifierComponent()->ApplyModifier(Request);
			if (!Applied.bSuccess) { Finish(false, TEXT("AugmentFailed")); return; }
			R.AugmentHandles.Add(Applied.Handle);
		}
		R.PendingAugments.Reset();
	}
}
void UDefenseDirectorComponent::Shutdown()
{
	if (bEnding) return;
	bEnding = true;
	if (auto* S = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr) S->CancelAllForOwner(this);
	PhaseSchedule = MonitorSchedule = {};
	if (Objective.IsValid()) Objective->GetCombatLifecycleComponent()->OnDied().RemoveAll(this);
	ClearEnemies();
	TArray<TWeakObjectPtr<ACombatPlayerController>> RemovedPlayers;
	Players.GetKeys(RemovedPlayers);
	for (const auto& Player : RemovedPlayers) RemovePlayer(Player.Get());
	Players.Reset(); bInitialized = false;
}
void UDefenseDirectorComponent::EndPlay(const EEndPlayReason::Type Reason) { Shutdown(); Super::EndPlay(Reason); }
