#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseAugmentData.h"
#include "Combat/Defense/DefenseChoiceComponent.h"
#include "Combat/Defense/DefenseHeroSelectionComponent.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Combat/CombatHealSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Combat/Core/CombatRngSubsystem.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/AI/CombatAIProfileData.h"
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
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

UDefenseDirectorComponent::UDefenseDirectorComponent() { PrimaryComponentTick.bCanEverTick = false; }

void UDefenseDirectorComponent::BuildDefaultEvents()
{
	FrozenEvents.Reset();
	if (!RunData) return;
	RunData->Events.Reset();
	auto* Vault = NewObject<UDefenseEventData>(RunData);
	Vault->DefinitionName = TEXT("defense_event_vault"); Vault->Kind = EDefenseEventKind::Vault; Vault->TriggerAfterWave = 2;
	Vault->Title = FText::FromString(TEXT("北侧宝库"));
	Vault->SafeDescription = FText::FromString(TEXT("安全取出补给：全队获得240金币与60经验。"));
	Vault->RiskDescription = FText::FromString(TEXT("冒险开启宝库：下一波增加一名敌人，成功后全队获得300金币，Boss奖励提高10%。"));
	Vault->SafeGold = 240; Vault->SafeExperience = 60; Vault->RiskBudgetBonus = 12; Vault->RiskGold = 300; Vault->BossRewardBonusFraction = 0.1f;
	auto* Oath = NewObject<UDefenseEventData>(RunData);
	Oath->DefinitionName = TEXT("defense_event_oath"); Oath->Kind = EDefenseEventKind::Oath; Oath->TriggerAfterWave = 4;
	Oath->Title = FText::FromString(TEXT("神殿誓约"));
	Oath->SafeDescription = FText::FromString(TEXT("修复神殿核心：立即恢复核心最大生命的12%。"));
	Oath->RiskDescription = FText::FromString(TEXT("接受悬赏：下一波增加一名敌人，成功后全队获得300金币，Boss奖励提高10%。"));
	Oath->SafeCoreHealFraction = 0.12f; Oath->RiskBudgetBonus = 12; Oath->RiskGold = 300; Oath->BossRewardBonusFraction = 0.1f;
	RunData->Events.Reset(); RunData->Events.Add(Vault); RunData->Events.Add(Oath); FrozenEvents = RunData->Events;
}

bool UDefenseDirectorComponent::InitializeForRun(UDefenseRunData* Data, ADefenseObjective* Core, ADefenseGameState* State, const bool bWaitForStart)
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
		for (auto& Group : Copy->SpawnGroups)
		{
			Group.UnitData = DuplicateObject<UCombatUnitData>(Group.UnitData, Copy);
			if (Group.RoleProfile) Group.RoleProfile = DuplicateObject<UCombatAIProfileData>(Group.RoleProfile, Copy);
		}
		if (Copy->FixedAugment) Copy->FixedAugment = DuplicateObject<UCombatModifierData>(Copy->FixedAugment, Copy);
		RunData->Waves.Add(Copy);
	}
	RunData->Events.Reset();
	if (!Data->Events.IsEmpty())
	{
		for (const UDefenseEventData* Event : Data->Events) if (Event) RunData->Events.Add(DuplicateObject<UDefenseEventData>(Event, RunData));
	}
	if (RunData->bEnablePrototypeEvents)
	{
		if (RunData->Events.IsEmpty()) BuildDefaultEvents(); else FrozenEvents = RunData->Events;
	}
	else FrozenEvents.Reset();
	if (Data->AugmentPool)
	{
		RunData->AugmentPool=DuplicateObject<UDefenseAugmentPoolData>(Data->AugmentPool,RunData);
		RunData->AugmentPool->Augments.Reset();
		for(UDefenseAugmentData* A:Data->AugmentPool->Augments)
		{
			auto* Copy=DuplicateObject<UDefenseAugmentData>(A,RunData->AugmentPool);
			Copy->ModifierData=DuplicateObject<UCombatModifierData>(A->ModifierData,Copy);
			RunData->AugmentPool->Augments.Add(Copy);
		}
	}
	SeedState = {};
	SeedState.RunSeed=GetWorld()->GetSubsystem<UCombatRngSubsystem>()->GetMatchSeed();
#if !UE_BUILD_SHIPPING
	if(RunData->DevelopmentSeedOverride>0) SeedState.RunSeed=static_cast<uint64>(RunData->DevelopmentSeedOverride);
	FString SeedArgument;
	uint64 ParsedSeed=0;
	if(FParse::Value(FCommandLine::Get(),TEXT("DefenseSeed="),SeedArgument) && ParseSeed(SeedArgument,ParsedSeed)) SeedState.RunSeed=ParsedSeed;
#endif
	if (SeedState.RunSeed == 0) SeedState.RunSeed = 1;
	Objective = Core; CoreLife = Core->GetLifeGeneration(); GameState = State;
	View.RunId = FGuid::NewGuid(); View.TotalWaves = RunData->Waves.Num(); View.Objective = Core;
	bInitialized = true;
#if !UE_BUILD_SHIPPING
	View.bWaitingForStart=bWaitForStart;
#endif
	Core->GetCombatLifecycleComponent()->OnDied().AddUObject(this, &ThisClass::OnCoreDied);
	EnterPhase(EDefensePhase::Preparation, INDEX_NONE, RunData->PreparationDuration);
	auto* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>();
	MonitorSchedule = Scheduler->ScheduleRepeating(this, 0.1, 0.1, -100, ECombatCatchUpPolicy::Coalesce,
		FCombatScheduledDelegate::CreateUObject(this, &ThisClass::Monitor));
	if(!View.bWaitingForStart)
	{
		UE_LOG(LogCombat,Display,TEXT("DefenseSeedLocked Run=%s Seed=%llu"),*View.RunId.ToString(),SeedState.RunSeed);
		SchedulePhase(RunData->PreparationDuration);
	}
	else { View.Deadline=0; Publish(); }
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
	ExpireTeamShop();
	PublishTeamShop();
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
	if (View.Phase == EDefensePhase::EventChoice)
	{
		if (EventView.State == EDefenseEventState::Offered && Tick.ActualTime >= EventView.Deadline)
		{
			ResolveEvent(EDefenseEventOption::Safe, EDefenseEventState::Expired, nullptr);
		}
		else if (EventView.State == EDefenseEventState::PendingRisk && Tick.ActualTime >= EventView.Deadline)
		{
			ResolveEvent(EDefenseEventOption::Cancel, EDefenseEventState::Expired, nullptr);
		}
		return;
	}
	if (View.Phase != EDefensePhase::WaveCombat)
	{
		ExpireChoices(Tick.ActualTime,true);
		if(Terminal.bTerminal) return;
		const int32 Next = View.WaveIndex + 1;
		if (!RunData->Waves.IsValidIndex(Next)) { Finish(false, TEXT("MissingWave")); return; }
		GroupIndex = SpawnIndex = SpawnFailures = 0;
		RiskExtraSpawnsRemaining = ConsumeRiskBonusForWave(Next);
		View.PendingSpawns = RiskExtraSpawnsRemaining;
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
		if (View.WaveIndex == 1 || View.WaveIndex == 3) { OpenEventAfterWave(View.WaveIndex); return; }
		EnterPhase(View.WaveIndex == 4 ? EDefensePhase::BossPreparation : EDefensePhase::Intermission,
			View.WaveIndex, RunData->IntermissionDuration);
		OpenChoiceWindows();
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
	if (!Groups.IsValidIndex(GroupIndex) && RiskExtraSpawnsRemaining > 0)
	{
		const FDefenseSpawnGroup* Extra = Groups.IsValidIndex(0) ? &Groups[0] : nullptr;
		if (!Extra || !SpawnEnemy(*Extra)) { if (++SpawnFailures > RunData->SpawnRetryLimit) Finish(false, TEXT("SpawnFailed")); else SchedulePhase(0.5f); return; }
		--RiskExtraSpawnsRemaining; --View.PendingSpawns; SpawnFailures=0; Publish();
		if (RiskExtraSpawnsRemaining > 0) SchedulePhase(1.0f); else QueueWaveCompletion(); return;
	}
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
	if (G.RoleProfile)
	{
		// 先登记敌人再授予职责，角色树是此组唯一命令来源，不能继续排入导演的人工队列。
		auto* Brain = Enemy->GetCombatAIBrainComponent();
		FCombatAIAssignment Duty;
		Duty.Home = Position;
		Duty.Route = RunData->Routes[G.RouteIndex].Waypoints;
		if (!Brain->SetAssignment(Duty)) { Finish(false, TEXT("RoleAssignmentRejected")); return true; }
		Brain->ConfigureProfile(G.RoleProfile);
		if (!Brain->IsRunning()) { Finish(false, TEXT("RoleStartupFailed")); return true; }
		UE_LOG(LogCombat, Display, TEXT("DefenseRoleSpawn Wave=%d Route=%d Unit=%s Definition=%s Profile=%s Life=%u"),
			View.WaveIndex, G.RouteIndex, *Enemy->GetName(), *Enemy->GetUnitDefinitionId().ToString(), *G.RoleProfile->GetPrimaryAssetId().ToString(), Enemy->GetLifeGeneration());
		return true;
	}
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
	ExpireChoices(Tick.ActualTime);
	if(Terminal.bTerminal) return;
	ExpireTeamShop();
	ServicePlayers(Tick.ActualTime);
	PublishChoices();
	PublishTeamShop();
	Publish();
}
void UDefenseDirectorComponent::Publish()
{
	View.Event = EventView;
	View.ObjectiveShield=ReadObjectiveShield();
	if (Objective.IsValid())
	{
		View.ObjectiveHealth = Objective->GetCombatAttributeSet()->GetHealth();
		View.ObjectiveMaxHealth = Objective->GetCombatAttributeSet()->GetMaxHealth();
	}
	if(SeedState.RunSeed)
	{
		FCombatRngSubjectId Subject;
		View.SeedSummary=FString::Printf(TEXT("%08x"),uint32(UCombatRngSubsystem::HashSeededTextV1(SeedState.RunSeed,TEXT("Run.Summary"),Subject,0)));
		View.OfferDrawCount=SeedState.DomainOrdinals.FindRef(TEXT("Run.AugmentOffer"));
	}
	if (GameState.IsValid()) GameState->Publish(View);
	const auto* Mode=GetWorld()?GetWorld()->GetAuthGameMode<ADefenseGameMode>():nullptr;
	for(const auto& Pair:Players) if(auto* PC=Pair.Key.Get())
		if(auto* Selection=PC->FindComponentByClass<UDefenseHeroSelectionComponent>())
			Selection->PublishEligibility(Mode&&Mode->GetGuardianClass()&&CanSelectStartingHero(PC),Pair.Value.bStartingHeroSelected);
}
void UDefenseDirectorComponent::Finish(bool bVictory, const FName Reason)
{
	if (Terminal.bTerminal || bEnding) return;
	if (bVictory && !IsCoreAlive()) { Finish(false, TEXT("ObjectiveLost")); return; }
	Terminal = {true, bVictory, Reason};
	View.TerminalReason = Reason;
	View.bWaitingForStart = false;
	View.Phase = bVictory ? EDefensePhase::Victory : EDefensePhase::Defeat;
	++View.PhaseGeneration; View.Deadline = 0; EventView = {}; View.Event = EventView; View.PendingSpawns = 0;
	GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>()->CancelAllForOwner(this);
	PhaseSchedule = MonitorSchedule = {};
	if (Objective.IsValid()) Objective->GetCombatLifecycleComponent()->OnDied().RemoveAll(this);
	ClearEnemies();
	for(const auto& Pair:Players) UnbindHeroLifecycle(Pair.Value.Hero.Get());
	ClearTeamShopEffects();
	// 结果先冻结；终局不再授予死亡期间的奖励或恢复英雄。
	for (auto& Pair : Players) { Pair.Value.PendingExperience = 0; Pair.Value.PendingAugments.Reset(); Pair.Value.PendingCards.Reset(); Pair.Value.CurrentOffer.bCancelled=true; }
	PublishChoices();
	PublishTeamShop();
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
	for(const auto& Pair:Players) if(Pair.Value.Hero.Get()==Hero) return false;
	FDefensePlayerRecord Record; Record.Hero = Hero; Record.PlayerOrdinal = NextPlayerOrdinal++;
	Players.Add(Player,Record);
	Hero->GetCombatLifecycleComponent()->OnDied().AddUObject(this,&UDefenseDirectorComponent::OnHeroDied);
	Hero->GetCombatLifecycleComponent()->OnRespawned().AddUObject(this,&UDefenseDirectorComponent::OnHeroRespawned);
	if(!View.bWaitingForStart && RunData->AugmentPool) GenerateAugmentOffer(Players.FindChecked(Player),View.WaveIndex);
	if(!ApplyTeamShopEffects(Hero)) { Finish(false,TEXT("TeamEffectFailed")); return false; }
	PublishChoices();
	PublishTeamShop();
	return true;
}
void UDefenseDirectorComponent::RemovePlayer(ACombatPlayerController* Player)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || GetNetMode() == NM_Client) return;
	FDefensePlayerRecord Removed;
	if(Players.RemoveAndCopyValue(Player,Removed))
	{
		if(auto* Hero=Removed.Hero.Get())
		{
			UnbindHeroLifecycle(Hero); RemoveHeroReinforcement(Hero);
			for(auto Handle:Removed.AugmentHandles) Hero->GetCombatModifierComponent()->RemoveModifier(Handle);
		}
		++TeamShopRevision;
	}
	if(IsValid(Player))
	{
		if(auto* Choice=Player->FindComponentByClass<UDefenseChoiceComponent>()) Choice->ClearAuthority();
		Player->GetCombatEconomyComponent()->RefreshShopOffers();
	}

	HighEventRequestIds.Remove(Player);
	EventReceipts.RemoveAll([Player](const FDefenseEventReceipt& R){return R.Player.Get()==Player;});
	if(EventProposer.Get()==Player&&EventView.State==EDefenseEventState::PendingRisk)
	{
		// 发起人离线仅退回安全选项，不允许离线身份留下自动风险确认。
		ResolveEvent(EDefenseEventOption::Cancel,EDefenseEventState::Cancelled,nullptr);
		EventProposer.Reset();
	}
	ExpireTeamShop(); PublishTeamShop();
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
		const int32 WaveGold = Wave->bBossWave ? FMath::RoundToInt(Wave->GoldReward * (1.0f + EarnedBossRewardBonus)) : Wave->GoldReward;
		if (WaveGold > 0 && !Player->GetCombatEconomyComponent()->AddGold(WaveGold, TEXT("DefenseWave"))) return false;
		Record.PendingExperience += Wave->ExperienceReward;

		if (!RunData->AugmentPool && Wave->FixedAugment && !Record.OwnedAugments.Contains(Wave->FixedAugment->GetPrimaryAssetId()))
		{
			Record.OwnedAugments.Add(Wave->FixedAugment->GetPrimaryAssetId());
			Record.PendingAugments.Add(Wave->FixedAugment);
		}
	}
	if (!RewardRiskEvent(View.WaveIndex)) return false;
	ServicePlayers(GetWorld()->GetTimeSeconds());
	return !Terminal.bTerminal;
}
void UDefenseDirectorComponent::ServicePlayers(const double Now)
{
	TArray<TWeakObjectPtr<ACombatPlayerController>> Keys; Players.GetKeys(Keys);
	for(const auto& Key:Keys)
	{
		if(!IsCurrent(View.RunId,GetPhaseGeneration())) return;
		auto* R=Players.Find(Key); auto* Hero=R?R->Hero.Get():nullptr;
		if(!Key.IsValid()||!IsValid(Hero)||Hero->IsActorBeingDestroyed()) continue;
		if(Hero->GetLifeState()==ECombatLifeState::Dead)
		{
			// 已经死亡后才登记的英雄没有本局死亡事件，按普通等待处理，不追溯援军。
			if(R->DeadLife!=Hero->GetLifeGeneration())
			{ R->DeadLife=Hero->GetLifeGeneration(); R->RespawnAt=Now+RunData->RespawnDelay; R->RespawnRetries=0; }
			if(Now<R->RespawnAt) continue;
			FVector Safe;
			const bool Respawned=FindSafeLocation(*Hero,RunData->RespawnLocation,Safe)&&Hero->GetCombatLifecycleComponent()->RespawnAtLocation(Safe);
			// 复活委托可以清空整局或移除当前玩家，禁止继续使用之前的容器引用。
			if(!IsCurrent(View.RunId,GetPhaseGeneration())) return;
			R=Players.Find(Key); if(!R||!IsValid(Hero)||Hero->IsActorBeingDestroyed()||R->Hero.Get()!=Hero) continue;
			if(!Respawned)
			{
				if(++R->RespawnRetries>10) { Finish(false,TEXT("RespawnBlocked")); return; }
				R->RespawnAt=Now+0.5; continue;
			}
			if(Hero->GetLifeState()==ECombatLifeState::Alive) R->DeadLife=0;
		}
		if(Hero->GetLifeState()!=ECombatLifeState::Alive) continue;
		if(!ApplyPendingReinforcement(Hero)) { Finish(false,TEXT("ReinforcementEffectFailed")); return; }
		if(!IsCurrent(View.RunId,GetPhaseGeneration())) return;
		R=Players.Find(Key); if(!R||R->Hero.Get()!=Hero) continue;
		if(!ApplyTeamShopEffects(Hero)) { Finish(false,TEXT("TeamEffectFailed")); return; }
		if(!IsCurrent(View.RunId,GetPhaseGeneration())) return;
		R=Players.Find(Key); if(!R||R->Hero.Get()!=Hero) continue;
		if(!ApplyPendingCards(*R)) { Finish(false,TEXT("AugmentFailed")); return; }
		if(!IsCurrent(View.RunId,GetPhaseGeneration())) return;
		R=Players.Find(Key); if(!R||R->Hero.Get()!=Hero) continue;
		if(R->PendingExperience>0)
		{
			const int32 XP=R->PendingExperience; R->PendingExperience=0;
			if(!Hero->GetCombatProgressionComponent()->AddExperience(XP)) { Finish(false,TEXT("ExperienceFailed")); return; }
			if(!IsCurrent(View.RunId,GetPhaseGeneration())) return;
			R=Players.Find(Key); if(!R||R->Hero.Get()!=Hero) continue;
		}
		const auto Pending=MoveTemp(R->PendingAugments); R->PendingAugments.Reset();
		for(const auto& Augment:Pending)
		{
			FCombatModifierApplyRequest Request; Request.ModifierData=Augment.Get(); Request.Source=Hero;
			const auto Applied=Hero->GetCombatModifierComponent()->ApplyModifier(Request);
			R=Players.Find(Key);
			if(!IsCurrent(View.RunId,GetPhaseGeneration())||!R||R->Hero.Get()!=Hero)
			{ if(IsValid(Hero)) Hero->GetCombatModifierComponent()->RemoveModifier(Applied.Handle); return; }
			if(!Applied.bSuccess) { Finish(false,TEXT("AugmentFailed")); return; }
			R->AugmentHandles.Add(Applied.Handle);
		}
	}
}

void UDefenseDirectorComponent::OpenEventAfterWave(const int32 CompletedWaveIndex)
{
	if (!IsCurrent(View.RunId, GetPhaseGeneration())) return;
	UDefenseEventData* Definition = nullptr;
	for (UDefenseEventData* Candidate : FrozenEvents)
		if (Candidate && Candidate->TriggerAfterWave == CompletedWaveIndex + 1) { Definition = Candidate; break; }
	if (!Definition)
	{
		EnterPhase(CompletedWaveIndex == 4 ? EDefensePhase::BossPreparation : EDefensePhase::Intermission,
			CompletedWaveIndex, RunData->IntermissionDuration);
		OpenChoiceWindows(); SchedulePhase(RunData->IntermissionDuration); return;
	}
	EnterPhase(EDefensePhase::EventChoice, CompletedWaveIndex, 20.0f);
	EventView = {};
	EventView.EventId = FGuid::NewGuid(); EventView.Revision = 1; EventView.DefinitionId = Definition->GetPrimaryAssetId();
	EventView.Title = Definition->Title; EventView.SafeDescription = Definition->SafeDescription; EventView.RiskDescription = Definition->RiskDescription;
	EventView.State = EDefenseEventState::Offered; EventView.Deadline = GetWorld()->GetTimeSeconds() + 20.0;
	RiskLedger = {}; RiskLedger.EventId = EventView.EventId; RiskLedger.Revision = EventView.Revision; RiskLedger.DefinitionId = Definition->GetPrimaryAssetId();
	RiskLedger.TriggerAfterWave = CompletedWaveIndex + 1; RiskLedger.State = EDefenseEventState::Offered;
	PublishEvent(); SchedulePhase(20.0f);
	UE_LOG(LogCombat, Display, TEXT("DefenseEventOpened Run=%s Event=%s Wave=%d Definition=%s Schema=%d"), *View.RunId.ToString(), *EventView.EventId.ToString(), CompletedWaveIndex, *EventView.DefinitionId.ToString(), View.SchemaVersion);
}

FDefenseEventView UDefenseDirectorComponent::BuildEventView() const { return EventView; }

int32 UDefenseDirectorComponent::InspectEventReplay(ACombatPlayerController* Player, const FDefenseEventRequest& Request) const
{
	if (!Player) return 0;
	for (const FDefenseEventReceipt& Receipt : EventReceipts)
	{
		if (Receipt.Player.Get() != Player || Receipt.Request.RequestId != Request.RequestId) continue;
		const bool Same = Receipt.Request.RunId == Request.RunId && Receipt.Request.PhaseGeneration == Request.PhaseGeneration
			&& Receipt.Request.EventId == Request.EventId && Receipt.Request.EventRevision == Request.EventRevision
			&& Receipt.Request.BindingGeneration == Request.BindingGeneration && Receipt.Request.LifeGeneration == Request.LifeGeneration
			&& Receipt.Request.Option == Request.Option;
		return Same ? 1 : -1;
	}
	return 0;
}

bool UDefenseDirectorComponent::ResolveEvent(const EDefenseEventOption Option, const EDefenseEventState State, ACombatPlayerController* Proposer)
{
	if (!IsCurrent(View.RunId, GetPhaseGeneration()) || View.Phase != EDefensePhase::EventChoice || bEventMutating || !EventView.EventId.IsValid()) return false;
	UDefenseEventData* Definition = nullptr;
	for (UDefenseEventData* Candidate : FrozenEvents) if (Candidate && Candidate->GetPrimaryAssetId() == EventView.DefinitionId) { Definition = Candidate; break; }
	if (!Definition) return false;
	TGuardValue<bool> Guard(bEventMutating, true);
	if (Option == EDefenseEventOption::Risk && EventView.State == EDefenseEventState::Offered)
	{
		EventProposer=Proposer;
		EventView.State = EDefenseEventState::PendingRisk; EventView.ViewRevision++;
		EventView.ProposerName = Proposer && Proposer->PlayerState ? Proposer->PlayerState->GetPlayerName() : TEXT("");
		EventView.Deadline = GetWorld()->GetTimeSeconds() + 10.0; RiskLedger.State = EDefenseEventState::PendingRisk; RiskLedger.Option = Option; ++RiskLedger.Revision; EventView.Revision=RiskLedger.Revision;
		PublishEvent(); SchedulePhase(10.0f); return true;
	}
	if (EventView.State == EDefenseEventState::PendingRisk && Option == EDefenseEventOption::Risk)
	{
		if(Proposer!=EventProposer.Get()) return false;
		UDefenseEventData* RiskDefinition = Definition; RiskLedger.State = EDefenseEventState::ResolvedRisk; RiskLedger.Option = EDefenseEventOption::Risk; RiskLedger.TargetWave = View.WaveIndex + 1; RiskLedger.RiskBudgetBonus = RiskDefinition->RiskBudgetBonus; RiskLedger.BossRewardBonusFraction = RiskDefinition->BossRewardBonusFraction;
		++RiskLedger.Revision; EventView.State = EDefenseEventState::ResolvedRisk; EventView.Revision = RiskLedger.Revision; EventView.ViewRevision++; PublishEvent(); FinishEventAndResume(); return true;
	}
	const bool bSafe = Option == EDefenseEventOption::Safe || Option == EDefenseEventOption::Cancel;
	if (!bSafe || (EventView.State != EDefenseEventState::Offered && EventView.State != EDefenseEventState::PendingRisk)) return false;
	if (Definition->Kind == EDefenseEventKind::Vault)
	{
		for (auto& Pair : Players)
		{
			auto* PC = Pair.Key.Get(); auto& Record = Pair.Value; auto* Hero = Record.Hero.Get();
			if (!PC) continue;
			if (Definition->SafeGold > 0 && !PC->GetCombatEconomyComponent()->AddGold(Definition->SafeGold, TEXT("DefenseEventSafe"))) { Finish(false, TEXT("EventRewardFailed")); return false; }
			Record.PendingExperience += Definition->SafeExperience;
		}
		ServicePlayers(GetWorld()->GetTimeSeconds());
	}
	else if (Definition->SafeCoreHealFraction > 0.0f)
	{
		FCombatHealRequest Heal; Heal.Source = Objective.Get(); Heal.Target = Objective.Get(); Heal.Amount = Objective->GetCombatAttributeSet()->GetMaxHealth() * Definition->SafeCoreHealFraction;
		if (!GetWorld()->GetSubsystem<UCombatHealSubsystem>()->Heal(Heal).bSuccess) { Finish(false, TEXT("EventCoreHealFailed")); return false; }
	}
	EventView.State = State == EDefenseEventState::Expired ? EDefenseEventState::Expired : (Option == EDefenseEventOption::Cancel ? EDefenseEventState::Cancelled : EDefenseEventState::ResolvedSafe);
	RiskLedger.Option = EDefenseEventOption::Safe; RiskLedger.State = EventView.State; ++RiskLedger.Revision; EventView.Revision = RiskLedger.Revision; EventView.ViewRevision++;
	PublishEvent(); FinishEventAndResume(); return true;
}

void UDefenseDirectorComponent::FinishEventAndResume()
{
	if (!IsCurrent(View.RunId, GetPhaseGeneration())) return;
	GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>()->Cancel(PhaseSchedule); PhaseSchedule = {};
	const int32 CompletedWave = View.WaveIndex;
	EnterPhase(CompletedWave == 4 ? EDefensePhase::BossPreparation : EDefensePhase::Intermission, CompletedWave, RunData->IntermissionDuration);
	OpenChoiceWindows(); SchedulePhase(RunData->IntermissionDuration); PublishEvent();
}

FDefenseEventResult UDefenseDirectorComponent::ProcessDefenseEvent(ACombatPlayerController* Player, const FDefenseEventRequest& Request)
{
	FDefenseEventResult Result; Result.RequestId = Request.RequestId; Result.FailureReason = TEXT("InvalidEvent");
	for (const FDefenseEventReceipt& Receipt : EventReceipts)
	{
		if (Receipt.Player.Get() == Player && Receipt.Request.RequestId == Request.RequestId)
		{ if(InspectEventReplay(Player,Request)<0) { Result.FailureReason=TEXT("RequestPayloadMismatch"); return Result; } return Receipt.Result; }
	}
	if (!IsCurrent(View.RunId, GetPhaseGeneration()) || View.Phase != EDefensePhase::EventChoice || !Player || !Players.Contains(Player)
		|| Request.RunId != View.RunId || Request.PhaseGeneration != View.PhaseGeneration || Request.EventId != EventView.EventId
		|| Request.EventRevision != EventView.Revision || !Player->GetCommandedUnit()
		|| Player->GetCommandedUnit()->GetLifeState()!=ECombatLifeState::Alive || Player->GetCommandedUnit()->GetLifeGeneration() != Request.LifeGeneration
		|| Player->GetCommandBindingGeneration() != Request.BindingGeneration)
	{ return Result; }
	if (Request.RequestId <= 0 || Request.RequestId <= HighEventRequestIds.FindRef(Player)) { Result.FailureReason = TEXT("StaleRequest"); return Result; }
	HighEventRequestIds.FindOrAdd(Player) = Request.RequestId;
	const bool Accepted = ResolveEvent(Request.Option, Request.Option == EDefenseEventOption::Risk ? EDefenseEventState::PendingRisk : EDefenseEventState::ResolvedSafe, Player);
	Result.bSuccess = Accepted; Result.FailureReason = Accepted ? NAME_None : FName(TEXT("EventClosed"));
	EventReceipts.Add({Player, Request, Result}); if (EventReceipts.Num() > 32) EventReceipts.RemoveAt(0);
	return Result;
}

bool UDefenseDirectorComponent::RewardRiskEvent(const int32 CompletedWaveIndex)
{
	if (RiskLedger.State != EDefenseEventState::ResolvedRisk || RiskLedger.TargetWave != CompletedWaveIndex || RiskLedger.bRewardGranted) return true;
	UDefenseEventData* Definition = nullptr;
	for (UDefenseEventData* Candidate : FrozenEvents) if (Candidate && Candidate->GetPrimaryAssetId() == RiskLedger.DefinitionId) { Definition = Candidate; break; }
	if (!Definition) return false;
	RiskLedger.bRewardGranted = true; RiskLedger.RewardBatchId = FGuid::NewGuid();
	EarnedBossRewardBonus += RiskLedger.BossRewardBonusFraction;
	for (auto& Pair : Players)
	{
		auto* PC = Pair.Key.Get(); auto* Hero = Pair.Value.Hero.Get(); if (!PC) continue;
		if (Definition->RiskGold > 0 && !PC->GetCombatEconomyComponent()->AddGold(Definition->RiskGold, TEXT("DefenseEventRisk"))) return false;
		if (Hero) Pair.Value.PendingExperience += Definition->RiskExperience;
	}
	UE_LOG(LogCombat, Display, TEXT("DefenseEventRiskRewarded Run=%s Event=%s Wave=%d Batch=%s"), *View.RunId.ToString(), *RiskLedger.EventId.ToString(), CompletedWaveIndex, *RiskLedger.RewardBatchId.ToString());
	return true;
}

int32 UDefenseDirectorComponent::ConsumeRiskBonusForWave(const int32 WaveIndex)
{
	if (RiskLedger.State != EDefenseEventState::ResolvedRisk || RiskLedger.TargetWave != WaveIndex || RiskLedger.RiskBudgetBonus <= 0) return 0;
	const int32 Bonus = FMath::Max(1, RiskLedger.RiskBudgetBonus / 12); RiskLedger.RiskBudgetBonus = 0; return Bonus;
}

void UDefenseDirectorComponent::PublishEvent()
{
	if(View.Phase==EDefensePhase::EventChoice) View.Deadline=EventView.Deadline;
	EventView.ViewRevision = EventView.ViewRevision == MAX_int32 ? 1 : EventView.ViewRevision + 1;
	Publish();
}

void UDefenseDirectorComponent::Shutdown()
{
	if (bEnding) return;
	bEnding = true;
	if (auto* S = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr) S->CancelAllForOwner(this);
	PhaseSchedule = MonitorSchedule = {};
	if (Objective.IsValid()) Objective->GetCombatLifecycleComponent()->OnDied().RemoveAll(this);
	ClearEnemies();
	for(const auto& Pair:Players) UnbindHeroLifecycle(Pair.Value.Hero.Get());
	ClearTeamShopEffects(true);
	TArray<TWeakObjectPtr<ACombatPlayerController>> RemovedPlayers;
	Players.GetKeys(RemovedPlayers);
	for (const auto& Player : RemovedPlayers) RemovePlayer(Player.Get());
	Players.Reset(); HighEventRequestIds.Reset(); EventProposer.Reset(); EarnedBossRewardBonus=0; EventReceipts.Reset(); FrozenEvents.Reset(); EventView = {}; RiskLedger = {}; bInitialized = false;
	Publish();
}
void UDefenseDirectorComponent::EndPlay(const EEndPlayReason::Type Reason) { Shutdown(); Super::EndPlay(Reason); }
