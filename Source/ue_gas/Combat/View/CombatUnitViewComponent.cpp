#include "Combat/View/CombatUnitViewComponent.h"

#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"

bool FCombatModifierView::HasSamePayload(const FCombatModifierView& Other) const
{
	return Handle == Other.Handle && DefinitionId == Other.DefinitionId
		&& StackCount == Other.StackCount && ServerStartTime == Other.ServerStartTime
		&& ServerEndTime == Other.ServerEndTime && ControlTags == Other.ControlTags
		&& bIsDebuff == Other.bIsDebuff && bDispellable == Other.bDispellable;
}

void FCombatModifierViewArray::PostReplicatedAdd(const TArrayView<int32> AddedIndices, const int32 FinalSize)
{
	(void)AddedIndices;
	(void)FinalSize;
	if (Owner) { Owner->HandleModifierViewsReplicated(); }
}

void FCombatModifierViewArray::PostReplicatedChange(const TArrayView<int32> ChangedIndices, const int32 FinalSize)
{
	(void)ChangedIndices;
	(void)FinalSize;
	if (Owner) { Owner->HandleModifierViewsReplicated(); }
}

void FCombatModifierViewArray::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, const int32 FinalSize)
{
	(void)RemovedIndices;
	(void)FinalSize;
	if (Owner) { Owner->HandleModifierViewsReplicated(); }
}

UCombatUnitViewComponent::UCombatUnitViewComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ModifierViews.Owner = this;
}

float UCombatUnitViewComponent::GetModifierRemainingTime(const FCombatModifierView& View) const
{
	if (View.ServerEndTime <= 0.0)
	{
		return -1.0f;
	}
	const double ServerTime = GetEstimatedServerTimeSeconds();
	return static_cast<float>(FMath::Max(0.0, View.ServerEndTime - ServerTime));
}

double UCombatUnitViewComponent::GetEstimatedServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	return World->GetTimeSeconds();
}

void UCombatUnitViewComponent::RefreshUnitView()
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	UCombatAbilitySystemComponent* Asc = Unit ? Unit->GetCombatAbilitySystemComponent() : nullptr;
	if (!Unit || !Unit->HasAuthority() || !Asc)
	{
		return;
	}
	UnitView.UnitDefinitionId = Unit->GetUnitDefinitionId();
	UnitView.TeamId = Unit->GetCombatTeamId();
	UnitView.LifeGeneration = Unit->GetLifeGeneration();
	UnitView.LifeState = Unit->GetLifeState();
	UnitView.VisibleStatusTags.Reset();
	for (const FGameplayTag& Tag : {
		CombatTags::State_Stunned.GetTag(), CombatTags::State_Silenced.GetTag(),
		CombatTags::State_Rooted.GetTag(), CombatTags::State_Disarmed.GetTag(),
		CombatTags::State_Hexed.GetTag(), CombatTags::State_Frozen.GetTag(),
		CombatTags::State_NoHealthBar.GetTag() })
	{
		if (Asc->HasMatchingGameplayTag(Tag))
		{
			UnitView.VisibleStatusTags.AddTag(Tag);
		}
	}
	UnitView.Health = Asc->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute());
	UnitView.MaxHealth = Asc->GetNumericAttribute(UCombatAttributeSet::GetMaxHealthAttribute());
	UnitView.Mana = Asc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute());
	UnitView.MaxMana = Asc->GetNumericAttribute(UCombatAttributeSet::GetMaxManaAttribute());
	OnUnitViewChanged.Broadcast();
	Unit->ForceNetUpdate();
}

void UCombatUnitViewComponent::RefreshModifierViews()
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	UCombatModifierComponent* Modifiers = Unit ? Unit->GetCombatModifierComponent() : nullptr;
	if (!Unit || !Unit->HasAuthority() || !Modifiers)
	{
		return;
	}
	// Dynamic GE 的 GrantedTag 通知可能早于 Modifier Runtime 入表；在集合稳定后再同步一次聚合状态，
	// 保证同一帧内 UnitView.VisibleStatusTags 与 ModifierViews.ControlTags 一致。
	RefreshUnitView();
	TArray<FCombatModifierView> Desired;
	Modifiers->BuildModifierViews(Desired);
	bool bArrayChanged = false;
	for (int32 Index = ModifierViews.Items.Num() - 1; Index >= 0; --Index)
	{
		if (!Desired.ContainsByPredicate([&](const FCombatModifierView& Item)
		{
			return Item.Handle == ModifierViews.Items[Index].Handle;
		}))
		{
			ModifierViews.Items.RemoveAt(Index, 1, EAllowShrinking::No);
			bArrayChanged = true;
		}
	}
	for (const FCombatModifierView& Wanted : Desired)
	{
		FCombatModifierView* Existing = ModifierViews.Items.FindByPredicate([&](const FCombatModifierView& Item)
		{
			return Item.Handle == Wanted.Handle;
		});
		if (!Existing)
		{
			FCombatModifierView& Added = ModifierViews.Items.Add_GetRef(Wanted);
			ModifierViews.MarkItemDirty(Added);
			continue;
		}
		if (!Existing->HasSamePayload(Wanted))
		{
			const int32 ReplicationId = Existing->ReplicationID;
			const int32 ReplicationKey = Existing->ReplicationKey;
			*Existing = Wanted;
			Existing->ReplicationID = ReplicationId;
			Existing->ReplicationKey = ReplicationKey;
			ModifierViews.MarkItemDirty(*Existing);
		}
	}
	if (bArrayChanged)
	{
		ModifierViews.MarkArrayDirty();
	}
	OnModifierViewsChanged.Broadcast();
	Unit->ForceNetUpdate();
}

void UCombatUnitViewComponent::NotifyAbilityStarted(
	const FPrimaryAssetId& DefinitionId,
	const FCombatEventId ActivationId,
	const double StartTime,
	const double EndTime,
	const bool bChanneling)
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !Unit->HasAuthority())
	{
		return;
	}
	UnitView.ActiveAbilityDefinitionId = DefinitionId;
	UnitView.AbilityActivationId = ActivationId;
	UnitView.AbilityServerStartTime = StartTime;
	UnitView.AbilityServerEndTime = EndTime;
	UnitView.bChanneling = bChanneling;
	OnUnitViewChanged.Broadcast();
	Unit->ForceNetUpdate();
}

void UCombatUnitViewComponent::NotifyAbilityEnded(const FCombatEventId ActivationId)
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !Unit->HasAuthority() || UnitView.AbilityActivationId != ActivationId)
	{
		return;
	}
	UnitView.ActiveAbilityDefinitionId = FPrimaryAssetId();
	UnitView.AbilityActivationId = FCombatEventId();
	UnitView.AbilityServerStartTime = 0.0;
	UnitView.AbilityServerEndTime = 0.0;
	UnitView.bChanneling = false;
	OnUnitViewChanged.Broadcast();
	Unit->ForceNetUpdate();
}

void UCombatUnitViewComponent::HandleModifierViewsReplicated()
{
	OnModifierViewsChanged.Broadcast();
}

void UCombatUnitViewComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCombatUnitViewComponent, UnitView);
	DOREPLIFETIME(UCombatUnitViewComponent, ModifierViews);
}

void UCombatUnitViewComponent::BeginPlay()
{
	Super::BeginPlay();
	ModifierViews.Owner = this;
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	UCombatAbilitySystemComponent* Asc = Unit ? Unit->GetCombatAbilitySystemComponent() : nullptr;
	if (Asc)
	{
		for (const FGameplayAttribute& Attribute : {
			UCombatAttributeSet::GetHealthAttribute(), UCombatAttributeSet::GetMaxHealthAttribute(),
			UCombatAttributeSet::GetManaAttribute(), UCombatAttributeSet::GetMaxManaAttribute() })
		{
			Asc->GetGameplayAttributeValueChangeDelegate(Attribute)
				.AddUObject(this, &UCombatUnitViewComponent::HandleAttributeChanged);
		}
		for (const FGameplayTag& Tag : {
			CombatTags::State_Stunned.GetTag(), CombatTags::State_Silenced.GetTag(),
			CombatTags::State_Rooted.GetTag(), CombatTags::State_Disarmed.GetTag(),
			CombatTags::State_Hexed.GetTag(), CombatTags::State_Frozen.GetTag(),
			CombatTags::State_NoHealthBar.GetTag() })
		{
			Asc->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &UCombatUnitViewComponent::HandleVisibleStatusTagChanged);
		}
	}
	if (UCombatModifierComponent* Modifiers = Unit ? Unit->GetCombatModifierComponent() : nullptr)
	{
		Modifiers->OnModifierCollectionChanged().AddUObject(this, &UCombatUnitViewComponent::RefreshModifierViews);
	}
	RefreshUnitView();
	RefreshModifierViews();
}

void UCombatUnitViewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ACombatUnitCharacter* Unit = GetOwnerUnit())
	{
		if (UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent())
		{
			Asc->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetHealthAttribute()).RemoveAll(this);
			Asc->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
			Asc->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetManaAttribute()).RemoveAll(this);
			Asc->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetMaxManaAttribute()).RemoveAll(this);
			for (const FGameplayTag& Tag : {
				CombatTags::State_Stunned.GetTag(), CombatTags::State_Silenced.GetTag(),
				CombatTags::State_Rooted.GetTag(), CombatTags::State_Disarmed.GetTag(),
				CombatTags::State_Hexed.GetTag(), CombatTags::State_Frozen.GetTag(),
				CombatTags::State_NoHealthBar.GetTag() })
			{
				Asc->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
			}
		}
		if (UCombatModifierComponent* Modifiers = Unit->GetCombatModifierComponent())
		{
			Modifiers->OnModifierCollectionChanged().RemoveAll(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UCombatUnitViewComponent::OnRep_UnitView()
{
	OnUnitViewChanged.Broadcast();
}

void UCombatUnitViewComponent::HandleAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	(void)ChangeData;
	RefreshUnitView();
}

void UCombatUnitViewComponent::HandleVisibleStatusTagChanged(const FGameplayTag Tag, const int32 NewCount)
{
	(void)Tag;
	(void)NewCount;
	RefreshUnitView();
}

ACombatUnitCharacter* UCombatUnitViewComponent::GetOwnerUnit() const
{
	return Cast<ACombatUnitCharacter>(GetOwner());
}
