#include "Combat/Unit/CombatUnitCharacter.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Aura/CombatAuraSubsystem.h"
#include "Combat/Combat/CombatEffectUtilities.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Motion/CombatMotionComponent.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/UI/CombatOverheadWidgetComponent.h"
#include "Combat/Unit/CombatRegenerationComponent.h"
#include "Combat/Unit/CombatUnitAIController.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffectTypes.h"
#include "Net/UnrealNetwork.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "ue_gasPlayerController.h"

ACombatUnitCharacter::ACombatUnitCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);

	CombatAbilitySystemComponent = CreateDefaultSubobject<UCombatAbilitySystemComponent>(TEXT("CombatAbilitySystem"));
	CombatAttributeSet = CreateDefaultSubobject<UCombatAttributeSet>(TEXT("CombatAttributes"));
	CombatAbilitySystemComponent->AddAttributeSetSubobject(CombatAttributeSet.Get());
	CombatModifierComponent = CreateDefaultSubobject<UCombatModifierComponent>(TEXT("CombatModifiers"));
	CombatLifecycleComponent = CreateDefaultSubobject<UCombatUnitLifecycleComponent>(TEXT("CombatLifecycle"));
	CombatRegenerationComponent = CreateDefaultSubobject<UCombatRegenerationComponent>(TEXT("CombatRegeneration"));
	CombatAttackComponent = CreateDefaultSubobject<UCombatAttackComponent>(TEXT("CombatAttack"));
	CombatOrderComponent = CreateDefaultSubobject<UCombatOrderComponent>(TEXT("CombatOrders"));
	CombatMotionComponent = CreateDefaultSubobject<UCombatMotionComponent>(TEXT("CombatMotion"));
	CombatUnitViewComponent = CreateDefaultSubobject<UCombatUnitViewComponent>(TEXT("CombatUnitView"));
	CombatOverheadWidgetComponent = CreateDefaultSubobject<UCombatOverheadWidgetComponent>(TEXT("CombatOverheadUI"));
	CombatOverheadWidgetComponent->SetupAttachment(GetRootComponent());
	CombatOverheadWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 145.0f));
	AIControllerClass = ACombatUnitAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("CombatUnit"));
	// AI 路径使用输入加速度驱动，确保 PathFollowing 通过 PawnMovement 正式消费移动请求。
	GetCharacterMovement()->GetNavMovementProperties()->bUseAccelerationForPaths = true;
	// 所有客户端都只消费服务器移动；禁止 SimulatedProxy 因本地 Pawn 重叠改写其他 Unit 的表现位置。
	GetCharacterMovement()->MaxDepenetrationWithPawnAsProxy = 0.0f;
	// 普通移动只由 Detour Crowd 计算避让方向，关闭 CharacterMovement 的 RVO，避免两套算法争用速度。
	GetCharacterMovement()->bUseRVOAvoidance = false;
}

bool ACombatUnitCharacter::SetCommandingPlayerController(APlayerController* NewController)
{
	if (!HasAuthority() || (NewController && NewController->GetWorld() != GetWorld()))
	{
		return false;
	}
	if (NewController && Cast<APlayerController>(GetController()))
	{
		UE_LOG(LogCombat, Error,
			TEXT("SAMTopologyRejected Unit=%s Reason=PlayerControllerDirectPossess Controller=%s"),
			*GetName(), *GetNameSafe(GetController()));
		return false;
	}
	if (NewController && !Cast<ACombatUnitAIController>(GetController()))
	{
		SpawnDefaultController();
	}
	if (NewController && !Cast<ACombatUnitAIController>(GetController()))
	{
		UE_LOG(LogCombat, Error,
			TEXT("SAMTopologyRejected Unit=%s Reason=MissingCombatAIController Controller=%s"),
			*GetName(), *GetNameSafe(GetController()));
		return false;
	}
	if (GetOwner() == NewController && CommandingPlayerController.Get() == NewController)
	{
		RefreshCombatReplicationPolicy();
		RefreshServerMovementState();
		return true;
	}
	CommandingPlayerController = NewController;
	SetOwner(NewController);
	// Owner 只建立 RPC/ASC owning connection；移动 RemoteRole 保持 SimulatedProxy。
	RefreshCombatReplicationPolicy();
	RefreshAbilityActorInfo();
	RefreshServerMovementState();
	ForceNetUpdate();
	return true;
}

APlayerController* ACombatUnitCharacter::GetCommandingPlayerController() const
{
	if (HasAuthority() && CommandingPlayerController.IsValid())
	{
		return CommandingPlayerController.Get();
	}
	return Cast<APlayerController>(GetOwner());
}

bool ACombatUnitCharacter::ValidateServerMovementTopology(FString& OutDiagnostic) const
{
	const ACombatUnitAIController* CombatController = Cast<ACombatUnitAIController>(GetController());
	const UCrowdFollowingComponent* Crowd = CombatController ? CombatController->GetCombatCrowdFollowing() : nullptr;
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const APlayerController* CommandingController = GetCommandingPlayerController();
	int32 BindingGeneration = 0;
	if (const Aue_gasPlayerController* CombatPlayer = Cast<Aue_gasPlayerController>(CommandingController))
	{
		BindingGeneration = CombatPlayer->GetCommandBindingGeneration();
	}
	OutDiagnostic = FString::Printf(
		TEXT("Unit=%s ControllerClass=%s CommandingPlayerController=%s CommandBindingGeneration=%d LocalRole=%d RemoteRole=%d PathFollowingClass=%s CrowdSimulationState=%d CrowdActive=%d CollisionProfile=%s LifeGeneration=%u"),
		*GetName(), *GetNameSafe(GetController() ? GetController()->GetClass() : nullptr),
		*GetNameSafe(CommandingController), BindingGeneration,
		static_cast<int32>(GetLocalRole()), static_cast<int32>(GetRemoteRole()),
		*GetNameSafe(CombatController && CombatController->GetPathFollowingComponent()
			? CombatController->GetPathFollowingComponent()->GetClass() : nullptr),
		Crowd ? static_cast<int32>(Crowd->GetCrowdSimulationState()) : -1,
		Crowd && Crowd->IsCrowdSimulationActive() ? 1 : 0,
		*GetCapsuleComponent()->GetCollisionProfileName().ToString(), LifeGeneration);

	if (!HasAuthority())
	{
		return GetLocalRole() == ROLE_SimulatedProxy;
	}
	const bool bAliveTopologyValid = LifeState != ECombatLifeState::Alive
		|| (CombatController && CombatController->GetPathFollowingComponent());
	return bAliveTopologyValid
		&& !Cast<APlayerController>(GetController())
		&& GetRemoteRole() != ROLE_AutonomousProxy
		&& (!Movement || !Movement->bUseRVOAvoidance);
}

void ACombatUnitCharacter::LogServerMovementTopology(const TCHAR* Context) const
{
	FString Diagnostic;
	const bool bValid = ValidateServerMovementTopology(Diagnostic);
	UE_LOG(LogCombat, Log, TEXT("SAMTopology Context=%s Valid=%s %s"),
		Context ? Context : TEXT("Unknown"), bValid ? TEXT("Yes") : TEXT("No"), *Diagnostic);
	if (HasAuthority() && !bValid)
	{
		UE_LOG(LogCombat, Error, TEXT("SAMTopologyInvariantFailed Context=%s %s"),
			Context ? Context : TEXT("Unknown"), *Diagnostic);
	}
}

void ACombatUnitCharacter::RefreshServerMovementState()
{
	if (HasAuthority())
	{
		if (ACombatUnitAIController* CombatController = Cast<ACombatUnitAIController>(GetController()))
		{
			CombatController->RefreshCrowdParticipation();
		}
	}
}

const AActor* ACombatUnitCharacter::GetNetOwner() const
{
	// APawn 默认总是把自己作为 NetOwner；AIController 驱动的单位必须显式暴露真正的指挥玩家。
	if (const APlayerController* CommandingController = Cast<APlayerController>(GetOwner()))
	{
		return CommandingController;
	}
	return Super::GetNetOwner();
}

UNetConnection* ACombatUnitCharacter::GetNetConnection() const
{
	// APawn 默认优先查询 AIController，而 AIController 没有客户端连接，因此这里改走显式 Owner。
	if (const APlayerController* CommandingController = Cast<APlayerController>(GetOwner()))
	{
		return CommandingController->GetNetConnection();
	}
	return Super::GetNetConnection();
}

UPlayer* ACombatUnitCharacter::GetNetOwningPlayer()
{
	if (APlayerController* CommandingController = Cast<APlayerController>(GetOwner()))
	{
		return CommandingController->GetNetOwningPlayer();
	}
	return Super::GetNetOwningPlayer();
}

UPlayer* ACombatUnitCharacter::GetNetOwningPlayerAnyRole()
{
	if (APlayerController* CommandingController = Cast<APlayerController>(GetOwner()))
	{
		return CommandingController->GetNetOwningPlayerAnyRole();
	}
	return Super::GetNetOwningPlayerAnyRole();
}

void ACombatUnitCharacter::ServerIssueOrderBatch_Implementation(FCombatOrderBatchRequest Request)
{
	APlayerController* RequestingController = Cast<APlayerController>(GetOwner());
	UE_LOG(LogCombat, Display, TEXT("M7OrderBatchServerReceived Unit=%s RequestId=%d Owner=%s Count=%d"),
		*GetName(), Request.RequestId, RequestingController ? *RequestingController->GetName() : TEXT("None"), Request.Orders.Num());
	const FCombatOrderBatchResult Result = ProcessOrderBatchForConnection(RequestingController, Request);
	ClientReceiveOrderBatchResult(Result);
}

FCombatOrderBatchResult ACombatUnitCharacter::ProcessOrderBatchForConnection(
	APlayerController* RequestingController,
	const FCombatOrderBatchRequest& Request)
{
	FCombatOrderBatchResult Result;
	Result.RequestId = Request.RequestId;
	UCombatNetworkSecuritySubsystem* Security = GetWorld()
		? GetWorld()->GetSubsystem<UCombatNetworkSecuritySubsystem>() : nullptr;
	FString Diagnostic;
	if (!Security || !Security->ValidateAndConsumeOrderRequest(
		RequestingController, this, Request, Result.FailureTag, Diagnostic))
	{
		if (!Result.FailureTag.IsValid())
		{
			Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		}
		return Result;
	}
	Result.bAccepted = true;
	if (!CombatOrderComponent)
	{
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}
	for (int32 Index = 0; Index < Request.Orders.Num(); ++Index)
	{
		const bool bQueue = Request.bAppendToExistingQueue || Index > 0;
		FCombatOrderResult& OrderResult = Result.OrderResults.Add_GetRef(
			CombatOrderComponent->IssueOrder(Request.Orders[Index], bQueue));
		Result.AcceptedOrderCount += OrderResult.bSuccess ? 1 : 0;
	}
	return Result;
}

void ACombatUnitCharacter::ClientReceiveOrderBatchResult_Implementation(FCombatOrderBatchResult Result)
{
	LastOrderBatchResult = MoveTemp(Result);
	UE_LOG(LogCombat, Display, TEXT("M7OrderBatchResult Unit=%s RequestId=%d Accepted=%s Orders=%d Failure=%s"),
		*GetName(), LastOrderBatchResult.RequestId,
		LastOrderBatchResult.bAccepted ? TEXT("true") : TEXT("false"),
		LastOrderBatchResult.AcceptedOrderCount, *LastOrderBatchResult.FailureTag.ToString());
	OnOrderBatchResult.Broadcast(LastOrderBatchResult);
}

UAbilitySystemComponent* ACombatUnitCharacter::GetAbilitySystemComponent() const
{
	return CombatAbilitySystemComponent;
}

bool ACombatUnitCharacter::SetCombatTeamId(const FCombatTeamId NewTeamId)
{
	if (!HasAuthority() || !NewTeamId.IsValid() || TeamId == NewTeamId)
	{
		return false;
	}
	const FCombatTeamId PreviousTeamId = TeamId;
	TeamId = NewTeamId;
	OnRep_TeamId(PreviousTeamId);
	ForceNetUpdate();
	return true;
}

bool ACombatUnitCharacter::InitializeFromUnitData(UCombatUnitData* InUnitData)
{
	if (!HasAuthority() || !InUnitData || !InUnitData->GetPrimaryAssetId().IsValid()
		|| !UCombatDefinitionData::IsValidDefinitionName(InUnitData->DefinitionName))
	{
		return false;
	}
	const FPrimaryAssetId RequestedId = InUnitData->GetPrimaryAssetId();
	if (InitializedUnitDefinitionId.IsValid())
	{
		return InitializedUnitDefinitionId == RequestedId;
	}
	FString StatsDiagnostic;
	if (!InUnitData->BaseStats.IsValid(&StatsDiagnostic) || !InUnitData->InitialTeamId.IsValid()
		|| !FMath::IsFinite(InUnitData->BaseAttackPoint) || InUnitData->BaseAttackPoint < 0.0f
		|| !FMath::IsFinite(InUnitData->AttackFacingToleranceDegrees)
		|| InUnitData->AttackFacingToleranceDegrees < 0.0f || InUnitData->AttackFacingToleranceDegrees > 180.0f
		|| !FMath::IsFinite(InUnitData->CriticalStrikeChance)
		|| InUnitData->CriticalStrikeChance < 0.0f || InUnitData->CriticalStrikeChance > 1.0f
		|| !FMath::IsFinite(InUnitData->CriticalStrikeMultiplier) || InUnitData->CriticalStrikeMultiplier < 1.0f)
	{
		return false;
	}

	// 先加载并完整校验 AbilitySet，避免属性已写入后才发现授予表非法。
	TArray<UCombatAbilitySet*> LoadedSets;
	TSet<UClass*> PendingAbilityClasses;
	TSet<FPrimaryAssetId> PendingAbilityDefinitions;
	for (const TSoftObjectPtr<UCombatAbilitySet>& SetReference : InUnitData->AbilitySets)
	{
		UCombatAbilitySet* AbilitySet = SetReference.LoadSynchronous();
		if (!AbilitySet)
		{
			return false;
		}
		LoadedSets.Add(AbilitySet);
		for (const FCombatAbilitySetEntry& Entry : AbilitySet->Abilities)
		{
			UClass* AbilityClass = Entry.AbilityClass.Get();
			const UCombatGameplayAbility* AbilityCdo = AbilityClass
				? Cast<UCombatGameplayAbility>(AbilityClass->GetDefaultObject()) : nullptr;
			const UCombatAbilityData* AbilityData = AbilityCdo ? AbilityCdo->GetAbilityData() : nullptr;
			FString AbilityDiagnostic;
			if (!AbilityData || !AbilityData->ValidateRuntime(AbilityDiagnostic)
				|| Entry.InitialLevel < 1 || Entry.InitialLevel > AbilityData->MaxLevel
				|| PendingAbilityClasses.Contains(AbilityClass)
				|| PendingAbilityDefinitions.Contains(AbilityData->GetPrimaryAssetId())
				|| CombatAbilitySystemComponent->FindCombatAbilitySpecByDefinitionId(AbilityData->GetPrimaryAssetId())
				|| (Entry.bAutoCastEnabled
					&& !AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_AutoCast)))
			{
				return false;
			}
			PendingAbilityClasses.Add(AbilityClass);
			PendingAbilityDefinitions.Add(AbilityData->GetPrimaryAssetId());
		}
	}

	UnitData = InUnitData;
	if (TeamId != InUnitData->InitialTeamId)
	{
		SetCombatTeamId(InUnitData->InitialTeamId);
	}
	if (InUnitData->CapsuleRadiusOverride > 0.0f)
	{
		GetCapsuleComponent()->SetCapsuleRadius(InUnitData->CapsuleRadiusOverride);
	}
	const FCombatUnitBaseStats& Stats = InUnitData->BaseStats;
	// 先写最大生命/法力再写当前值，保证同一初始化效果按新的资源上限限制当前值。
	const TArray<TPair<FGameplayAttribute, float>> InitialAttributes = {
		{ UCombatAttributeSet::GetMaxHealthAttribute(), Stats.MaxHealth },
		{ UCombatAttributeSet::GetHealthAttribute(), Stats.MaxHealth },
		{ UCombatAttributeSet::GetMaxManaAttribute(), Stats.MaxMana },
		{ UCombatAttributeSet::GetManaAttribute(), Stats.MaxMana },
		{ UCombatAttributeSet::GetArmorAttribute(), Stats.Armor },
		{ UCombatAttributeSet::GetMagicResistAttribute(), Stats.MagicResist },
		{ UCombatAttributeSet::GetEvasionAttribute(), Stats.Evasion },
		{ UCombatAttributeSet::GetAttackDamageAttribute(), Stats.AttackDamage },
		{ UCombatAttributeSet::GetAttackSpeedAttribute(), Stats.AttackSpeed },
		{ UCombatAttributeSet::GetBaseAttackTimeAttribute(), Stats.BaseAttackTime },
		{ UCombatAttributeSet::GetAttackRangeAttribute(), Stats.AttackRange },
		{ UCombatAttributeSet::GetMoveSpeedAttribute(), Stats.MoveSpeed },
		{ UCombatAttributeSet::GetHealthRegenAttribute(), Stats.HealthRegen },
		{ UCombatAttributeSet::GetManaRegenAttribute(), Stats.ManaRegen },
		{ UCombatAttributeSet::GetLifestealPctAttribute(), Stats.LifestealPct },
		{ UCombatAttributeSet::GetSpellAmplifyPctAttribute(), Stats.SpellAmplifyPct },
		{ UCombatAttributeSet::GetCooldownReductionPctAttribute(), Stats.CooldownReductionPct },
		{ UCombatAttributeSet::GetCastRangeBonusAttribute(), Stats.CastRangeBonus },
		{ UCombatAttributeSet::GetStatusResistancePctAttribute(), Stats.StatusResistancePct },
		{ UCombatAttributeSet::GetHealAmplifyPctAttribute(), Stats.HealAmplifyPct },
		{ UCombatAttributeSet::GetHealReceivedPctAttribute(), Stats.HealReceivedPct }
	};
	if (!CombatEffectUtilities::ApplyAttributeOverrides(this, *CombatAbilitySystemComponent, InitialAttributes))
	{
		return false;
	}

	for (const UCombatAbilitySet* AbilitySet : LoadedSets)
	{
		for (const FCombatAbilitySetEntry& Entry : AbilitySet->Abilities)
		{
			FGameplayAbilitySpecHandle Handle;
			FGameplayTag FailureTag;
			if (!CombatAbilitySystemComponent->GrantCombatAbility(
				Entry.AbilityClass, Entry.InitialLevel, Entry.bAutoCastEnabled, Handle, FailureTag))
			{
				return false;
			}
		}
	}
	InitializedUnitDefinitionId = RequestedId;
	// 动态 Spawn 的最小 World 可能在 BeginPlay 后才具备最终 Authority/Owner；初始化结束再应用一次产品策略。
	RefreshCombatReplicationPolicy();
	if (CombatUnitViewComponent)
	{
		CombatUnitViewComponent->RefreshUnitView();
		CombatUnitViewComponent->RefreshModifierViews();
	}
	// 组件可能先于单位定义开始运行；此处确保恢复任务存在，已有任务保留原节拍，下一次回调会读取新属性。
	if (CombatRegenerationComponent)
	{
		CombatRegenerationComponent->HandleOwnerRespawn();
	}
	RefreshStatusResponse();
	return true;
}

bool ACombatUnitCharacter::IsMovementBlocked() const
{
	return LifeState != ECombatLifeState::Alive || !CombatAbilitySystemComponent
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Stunned)
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Rooted)
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Hexed)
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Frozen);
}

bool ACombatUnitCharacter::IsAttackBlocked() const
{
	return IsMovementBlocked() || CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Disarmed);
}

bool ACombatUnitCharacter::IsAbilityBlocked() const
{
	return LifeState != ECombatLifeState::Alive || !CombatAbilitySystemComponent
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Stunned)
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Silenced)
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Hexed)
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Frozen);
}

void ACombatUnitCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACombatUnitCharacter, TeamId);
	DOREPLIFETIME(ACombatUnitCharacter, LifeState);
	DOREPLIFETIME(ACombatUnitCharacter, LifeGeneration);
}

void ACombatUnitCharacter::PossessedBy(AController* NewController)
{
	APlayerController* PreviousCommandingController = GetCommandingPlayerController();
	Super::PossessedBy(NewController);
	// AIController 重建会由 APawn 临时覆盖 Owner；显式恢复玩家 owning connection。
	if (PreviousCommandingController && Cast<ACombatUnitAIController>(NewController))
	{
		SetOwner(PreviousCommandingController);
	}
	if (Cast<APlayerController>(NewController))
	{
		UE_LOG(LogCombat, Error,
			TEXT("SAMTopologyInvariantFailed Context=DirectPossess Unit=%s Controller=%s"),
			*GetName(), *GetNameSafe(NewController));
	}
	RefreshAbilityActorInfo();
	RefreshServerMovementState();
}

void ACombatUnitCharacter::UnPossessed()
{
	APlayerController* PreviousCommandingController = GetCommandingPlayerController();
	Super::UnPossessed();
	if (PreviousCommandingController)
	{
		SetOwner(PreviousCommandingController);
	}
	RefreshAbilityActorInfo();
}

void ACombatUnitCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	if (CombatOrderComponent)
	{
		CombatOrderComponent->RefreshControllerBinding();
	}
	RefreshServerMovementState();
}

void ACombatUnitCharacter::BeginPlay()
{
	Super::BeginPlay();
	RefreshCombatReplicationPolicy();
	RefreshAbilityActorInfo();
	RefreshLifeStateTag();
	if (CombatAbilitySystemComponent)
	{
		CombatAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetMoveSpeedAttribute())
			.AddUObject(this, &ACombatUnitCharacter::HandleMoveSpeedChanged);
	}
	if (HasAuthority() && UnitData)
	{
		InitializeFromUnitData(UnitData);
	}
	if (HasAuthority() && !GetController())
	{
		SpawnDefaultController();
	}
	RefreshStatusResponse();
	if (HasAuthority())
	{
		LogServerMovementTopology(TEXT("BeginPlay"));
	}
}

void ACombatUnitCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (Aue_gasPlayerController* CommandingController = Cast<Aue_gasPlayerController>(GetCommandingPlayerController()))
		{
			CommandingController->HandleCommandedUnitEndPlay(this);
		}
		if (UCombatAuraSubsystem* Auras = GetWorld() ? GetWorld()->GetSubsystem<UCombatAuraSubsystem>() : nullptr)
		{
			Auras->NotifyUnitEndPlay(this);
		}
	}
	if (CombatAbilitySystemComponent)
	{
		CombatAbilitySystemComponent->ClearCombatActorInfo();
	}
	Super::EndPlay(EndPlayReason);
}

void ACombatUnitCharacter::OnRep_Owner()
{
	Super::OnRep_Owner();
	RefreshAbilityActorInfo();
}

void ACombatUnitCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	RefreshAbilityActorInfo();
}

void ACombatUnitCharacter::OnRep_TeamId(const FCombatTeamId PreviousTeamId)
{
	if (CombatUnitViewComponent)
	{
		CombatUnitViewComponent->RefreshUnitView();
	}
	if (!HasAuthority())
	{
		return;
	}
	if (UCombatAuraSubsystem* Auras = GetWorld() ? GetWorld()->GetSubsystem<UCombatAuraSubsystem>() : nullptr)
	{
		Auras->NotifyUnitChanged(this);
	}
	if (UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr)
	{
		FCombatLogRecord Record;
		Record.Context = Events->CreateRootEvent();
		Record.EventType = CombatTags::Event_Combat_TeamChanged;
		Record.SourceActorId = GetUniqueID();
		Record.TargetActorId = GetUniqueID();
		Record.UnitLifeGeneration = LifeGeneration;
		Record.RequestedAmount = static_cast<float>(PreviousTeamId.Value);
		Record.AppliedAmount = static_cast<float>(TeamId.Value);
		Record.Diagnostic = FString::Printf(TEXT("Team %s -> %s"),
			*PreviousTeamId.ToString(), *TeamId.ToString());
		Events->Emit(Record);
	}
}

void ACombatUnitCharacter::OnRep_LifeState()
{
	RefreshLifeStateTag();
	RefreshStatusResponse();
	if (CombatUnitViewComponent)
	{
		CombatUnitViewComponent->RefreshUnitView();
	}
	if (HasAuthority())
	{
		if (UCombatAuraSubsystem* Auras = GetWorld() ? GetWorld()->GetSubsystem<UCombatAuraSubsystem>() : nullptr)
		{
			Auras->NotifyUnitChanged(this);
		}
	}
}

void ACombatUnitCharacter::RefreshAbilityActorInfo()
{
	// Character 自持 ASC，Owner 与 Avatar 都使用 Unit，可在四种 NetMode 下保持同一复制契约。
	if (CombatAbilitySystemComponent)
	{
		CombatAbilitySystemComponent->InitializeCombatActorInfo(this, this);
	}
}

void ACombatUnitCharacter::RefreshCombatReplicationPolicy()
{
	if (!HasAuthority() || !CombatAbilitySystemComponent)
	{
		return;
	}
	EffectiveAscReplicationPolicy = AscReplicationPolicy;
	if (EffectiveAscReplicationPolicy == ECombatAscReplicationPolicy::Automatic)
	{
		EffectiveAscReplicationPolicy = Cast<APlayerController>(GetOwner())
			? ECombatAscReplicationPolicy::Mixed : ECombatAscReplicationPolicy::Minimal;
	}
	switch (EffectiveAscReplicationPolicy)
	{
	case ECombatAscReplicationPolicy::Minimal:
		CombatAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
		break;
	case ECombatAscReplicationPolicy::Full:
		CombatAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);
		break;
	case ECombatAscReplicationPolicy::Mixed:
	default:
		CombatAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
		break;
	}
}

void ACombatUnitCharacter::RefreshLifeStateTag()
{
	if (!CombatAbilitySystemComponent)
	{
		return;
	}

	// 先移除完整互斥集合，再添加当前状态，避免复制回调留下多个生命标签。
	CombatAbilitySystemComponent->RemoveLooseGameplayTag(CombatTags::State_Alive);
	CombatAbilitySystemComponent->RemoveLooseGameplayTag(CombatTags::State_Dying);
	CombatAbilitySystemComponent->RemoveLooseGameplayTag(CombatTags::State_Dead);
	CombatAbilitySystemComponent->RemoveLooseGameplayTag(CombatTags::State_Respawning);

	switch (LifeState)
	{
	case ECombatLifeState::Alive: CombatAbilitySystemComponent->AddLooseGameplayTag(CombatTags::State_Alive); break;
	case ECombatLifeState::Dying: CombatAbilitySystemComponent->AddLooseGameplayTag(CombatTags::State_Dying); break;
	case ECombatLifeState::Dead: CombatAbilitySystemComponent->AddLooseGameplayTag(CombatTags::State_Dead); break;
	case ECombatLifeState::Respawning: CombatAbilitySystemComponent->AddLooseGameplayTag(CombatTags::State_Respawning); break;
	default: break;
	}
}

void ACombatUnitCharacter::RefreshStatusResponse()
{
	if (!CombatAbilitySystemComponent)
	{
		return;
	}
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement)
	{
		Movement->MaxWalkSpeed = CombatAbilitySystemComponent->GetNumericAttribute(UCombatAttributeSet::GetMoveSpeedAttribute());
		if (IsMovementBlocked())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
		else if (Movement->MovementMode == MOVE_None)
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
	if (LifeState == ECombatLifeState::Dead || LifeState == ECombatLifeState::Dying)
	{
		GetCapsuleComponent()->SetCollisionProfileName(TEXT("CombatCorpse"));
	}
	else if (CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_NoUnitCollision))
	{
		GetCapsuleComponent()->SetCollisionProfileName(TEXT("CombatUnitNoCollision"));
	}
	else
	{
		GetCapsuleComponent()->SetCollisionProfileName(TEXT("CombatUnit"));
	}
	// Dying/Dead 的清理由 LifecycleComponent 以固定屏障顺序调用，避免复制投影重复提升 generation。
	if (LifeState == ECombatLifeState::Alive && CombatAttackComponent)
	{
		CombatAttackComponent->HandleOwnerStatusChanged();
	}
	if (LifeState == ECombatLifeState::Alive && CombatOrderComponent)
	{
		CombatOrderComponent->HandleOwnerStatusChanged();
	}
	RefreshServerMovementState();
}

void ACombatUnitCharacter::HandleMoveSpeedChanged(const FOnAttributeChangeData& ChangeData)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = FMath::Max(0.0f, ChangeData.NewValue);
	}
}

void ACombatUnitCharacter::SetLifeStateFromLifecycle(const ECombatLifeState NewState)
{
	LifeState = NewState;
	OnRep_LifeState();
}

void ACombatUnitCharacter::IncrementLifeGenerationFromLifecycle()
{
	++LifeGeneration;
	if (CombatUnitViewComponent)
	{
		CombatUnitViewComponent->RefreshUnitView();
	}
}
