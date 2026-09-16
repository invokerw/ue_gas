#include "Combat/Log/CombatLogComponent.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

namespace CombatLogIdentity
{
	/** 固有被动在初始化完成标志前产生事件；此时读取已校验并赋值的定义，不修改单位初始化顺序。 */
	FPrimaryAssetId ResolveDefinition(const ACombatUnitCharacter* Unit)
	{
		if (!Unit) return FPrimaryAssetId();
		const FPrimaryAssetId Initialized = Unit->GetUnitDefinitionId();
		return Initialized.IsValid() || !Unit->GetUnitData() ? Initialized : Unit->GetUnitData()->GetPrimaryAssetId();
	}
}

UCombatLogComponent::UCombatLogComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
	History.Owner = this;
}

void UCombatLogComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UCombatLogComponent, History, COND_OwnerOnly);
}

void UCombatLogComponent::BeginPlay()
{
	Super::BeginPlay();
	History.Owner = this;
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Cast<APlayerController>(GetOwner())) return;
	BoundEvents = GetWorld()->GetSubsystem<UCombatEventSubsystem>();
	if (BoundEvents.IsValid())
	{
		BoundEvents->OnPresentationRecord().RemoveAll(this);
		BoundEvents->OnPresentationRecord().AddUObject(this, &UCombatLogComponent::HandleRecord);
	}
}

void UCombatLogComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (BoundEvents.IsValid()) BoundEvents->OnPresentationRecord().RemoveAll(this);
	BoundEvents.Reset();
	UnitCache.Reset();
	History.Items.Reset();
	NotifyHistoryChanged();
	HistoryChanged.Clear();
	Super::EndPlay(Reason);
}

ACombatUnitCharacter* UCombatLogComponent::FindUnit(const int32 ActorId)
{
	if (ActorId == 0) return nullptr;
	if (ACombatUnitCharacter* Unit = UnitCache.FindRef(ActorId).Get()) return Unit;
	// 缓存不持有 Actor；在发生未见过的单位事件时重建，顺便清除已销毁对象。
	UnitCache.Reset();
	for (TActorIterator<ACombatUnitCharacter> It(GetWorld()); It; ++It)
		UnitCache.Add(static_cast<int32>(It->GetUniqueID()), *It);
	return UnitCache.FindRef(ActorId).Get();
}

void UCombatLogComponent::HandleRecord(const FCombatLogRecord& Record, const FCombatLogResourceChange& ResourceChange)
{
	APlayerController* Player = Cast<APlayerController>(GetOwner());
	if (!Player || !Player->HasAuthority() || !FMath::IsFinite(Record.AppliedAmount)
		|| !FMath::IsFinite(Record.ServerTime) || Record.AppliedAmount < 0.0f) return;
	FCombatLogEntry Entry;
	if (!CombatLogPresentation::Classify(Record.EventType, Entry.Category)) return;
	if (Record.EventType == CombatTags::Event_Combat_ItemChanged
		&& (Record.ItemAction == TEXT("Cooldown") || Record.ItemAction == TEXT("Removed"))) return;
	if (Entry.Category == ECombatLogCategory::Healing && Record.AppliedAmount <= KINDA_SMALL_NUMBER) return;
	ACombatUnitCharacter* Source = FindUnit(Record.SourceActorId);
	ACombatUnitCharacter* Target = FindUnit(Record.TargetActorId);
	if ((!Source && Record.SourceActorId != 0) || (!Target && Record.TargetActorId != 0)) return;
	if (!Source && !Target) return;
	if (GetNetMode() != NM_Standalone)
	{
		const AActor* ViewTarget = Player->GetViewTarget();
		const FVector ViewLocation = Player->GetFocalLocation();
		// 双方都必须对该连接相关，避免通过战斗记录暴露远端不可见的另一方。
		if ((Source && !Source->IsNetRelevantFor(Player, ViewTarget, ViewLocation))
			|| (Target && !Target->IsNetRelevantFor(Player, ViewTarget, ViewLocation))) return;
	}
	Entry.Sequence = static_cast<int64>(Record.Sequence);
	Entry.ServerTime = Record.ServerTime;
	Entry.EventType = Record.EventType;
	Entry.SourceActorId = Record.SourceActorId;
	Entry.TargetActorId = Record.TargetActorId;
	Entry.SourceDefinitionId = CombatLogIdentity::ResolveDefinition(Source);
	Entry.TargetDefinitionId = CombatLogIdentity::ResolveDefinition(Target);
	Entry.bSourceHero = Source && Source->GetCommandingPlayerController();
	Entry.bTargetHero = Target && Target->GetCommandingPlayerController();
	Entry.EffectDefinitionId = Entry.Category == ECombatLogCategory::Status ? Record.Source.ModifierDefinitionId : Record.Source.AbilityDefinitionId;
	if (!Entry.EffectDefinitionId.IsValid()) Entry.EffectDefinitionId = Record.Source.ModifierDefinitionId;
	if (!Entry.EffectDefinitionId.IsValid()) Entry.EffectDefinitionId = Record.Source.ProjectileDefinitionId;
	Entry.ItemDefinitionId = Record.Source.ItemDefinitionId;
	Entry.ItemHandle = Record.Source.ItemHandle;
	Entry.ItemAction = Record.ItemAction;
	Entry.ItemQuantity = Record.ItemQuantity;
	Entry.ItemCharges = Record.ItemCharges;
	Entry.GoldDelta = Record.GoldDelta;
	Entry.GoldBalance = Record.GoldBalance;
	if (Entry.ItemDefinitionId.IsValid()) Entry.EffectDefinitionId = Entry.ItemDefinitionId;
	Entry.Amount = Record.AppliedAmount;
	Entry.bHasHealthChange = ResourceChange.bHasHealthChange && FMath::IsFinite(ResourceChange.PreviousHealth)
		&& FMath::IsFinite(ResourceChange.NewHealth);
	if (Entry.bHasHealthChange)
	{
		Entry.PreviousHealth = ResourceChange.PreviousHealth;
		Entry.NewHealth = ResourceChange.NewHealth;
	}
	if (Source && Entry.EventType == CombatTags::Event_Combat_AutoCastChanged)
	{
		UCombatAbilitySystemComponent* Asc = Source->GetCombatAbilitySystemComponent();
		const FGameplayAbilitySpec* Spec = Asc->FindCombatAbilitySpecByDefinitionId(Entry.EffectDefinitionId);
		Entry.bAutoCastEnabled = Spec && Asc->IsAutoCastEnabled(Spec->Handle);
	}
	if (History.Append(Entry))
	{
		NotifyHistoryChanged();
		Player->ForceNetUpdate();
	}
}

void UCombatLogComponent::NotifyHistoryChanged()
{
	OrderedEntries = History.Items;
	OrderedEntries.Sort([](const FCombatLogEntry& A, const FCombatLogEntry& B) { return A.Sequence < B.Sequence; });
	HistoryChanged.Broadcast();
}
