#include "Combat/View/CombatUnitViewComponent.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Unit/CombatProgressionComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "GameFramework/PlayerController.h"

bool FCombatHUDAbilityView::operator==(const FCombatHUDAbilityView& Other) const
{
	return SpecHandle == Other.SpecHandle && DefinitionId == Other.DefinitionId && Level == Other.Level
		&& MaxLevel == Other.MaxLevel && ManaCost == Other.ManaCost && CooldownEndTime == Other.CooldownEndTime
		&& CooldownDuration == Other.CooldownDuration && bIgnoreSilence == Other.bIgnoreSilence
		&& bUsesAutoCastToggleInput == Other.bUsesAutoCastToggleInput
		&& bAutoCastEnabled == Other.bAutoCastEnabled && bCanUpgrade == Other.bCanUpgrade;
}

bool FCombatHUDOwnerView::operator==(const FCombatHUDOwnerView& Other) const
{
	return UnitDefinitionId == Other.UnitDefinitionId && LifeGeneration == Other.LifeGeneration
		&& Strength == Other.Strength && Agility == Other.Agility && Intelligence == Other.Intelligence
		&& PrimaryAttribute == Other.PrimaryAttribute && Health == Other.Health && MaxHealth == Other.MaxHealth
		&& Mana == Other.Mana && MaxMana == Other.MaxMana
		&& AttackDamage == Other.AttackDamage && Armor == Other.Armor && MagicResist == Other.MagicResist
		&& MoveSpeed == Other.MoveSpeed && HealthRegen == Other.HealthRegen && ManaRegen == Other.ManaRegen
		&& CastRangeBonus == Other.CastRangeBonus && AttackRange == Other.AttackRange
		&& Evasion == Other.Evasion && AttackSpeed == Other.AttackSpeed && BaseAttackTime == Other.BaseAttackTime
		&& LifestealPct == Other.LifestealPct && SpellAmplifyPct == Other.SpellAmplifyPct
		&& CooldownReductionPct == Other.CooldownReductionPct && StatusResistancePct == Other.StatusResistancePct
		&& HealAmplifyPct == Other.HealAmplifyPct && HealReceivedPct == Other.HealReceivedPct
		&& Level == Other.Level && Experience == Other.Experience
		&& ExperienceIntoLevel == Other.ExperienceIntoLevel && ExperienceToNextLevel == Other.ExperienceToNextLevel
		&& ExperienceProgress == Other.ExperienceProgress && UnspentAbilityPoints == Other.UnspentAbilityPoints
		&& Abilities == Other.Abilities && Items == Other.Items && InventoryRevision == Other.InventoryRevision;
}

namespace CombatHUDView
{
	/** 与 PlayerController 的 Q/W/E/R 规则一致，保留主动技能和可切换 AutoCast 被动的授予顺序。 */
	TArray<const FGameplayAbilitySpec*> GetSlots(const UCombatAbilitySystemComponent& Asc)
	{
		TArray<const FGameplayAbilitySpec*> Result;
		for (const FGameplayAbilitySpec& Spec : Asc.GetActivatableAbilities())
		{
			const UCombatGameplayAbility* Ability = Cast<UCombatGameplayAbility>(Spec.Ability);
			const UCombatAbilityData* Data = Ability ? Ability->GetAbilityData() : nullptr;
			if (!Data || Asc.IsItemAbility(Spec.Handle) || !Data->ShouldOccupyPlayerAbilitySlot()) continue;
			Result.Add(&Spec);
			if (Result.Num() == 4) break;
		}
		return Result;
	}
}

FCombatHUDOwnerView UCombatUnitViewComponent::GetHUDOwnerView() const
{
	FCombatHUDOwnerView Result = HUDOwnerView;
	const ACombatUnitCharacter* Unit = GetOwnerUnit();
	const UCombatAbilitySystemComponent* Asc = Unit ? Unit->GetCombatAbilitySystemComponent() : nullptr;
	if (Unit && !Unit->HasAuthority())
	{
		const APlayerController* Player = Unit->GetCommandingPlayerController();
		// 失去拥有权后，旧客户端可能仍缓存最后一次 owner-only 属性；公共展示入口立即屏蔽旧快照。
		if (!Player || !Player->IsLocalController()) return FCombatHUDOwnerView();
		// Spec 与 View 是独立复制流；按句柄匹配，缺少任一流时显示空槽而非错误的技能快捷键。
		Result.Abilities.Reset();
		if (Asc)
		{
			for (const FGameplayAbilitySpec* Spec : CombatHUDView::GetSlots(*Asc))
			{
				const FCombatHUDAbilityView* Item = HUDOwnerView.Abilities.FindByPredicate(
					[Spec](const FCombatHUDAbilityView& Candidate) { return Candidate.SpecHandle == Spec->Handle; });
				Result.Abilities.Add(Item ? *Item : FCombatHUDAbilityView());
			}
		}
	}
	return Result;
}

void UCombatUnitViewComponent::RefreshHUDOwnerView()
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !Unit->HasAuthority()) return;
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	FCombatHUDOwnerView Next;
	if (Asc && Unit->GetCommandingPlayerController())
	{
		Next.UnitDefinitionId = Unit->GetUnitDefinitionId();
		Next.LifeGeneration = Unit->GetLifeGeneration();
		Next.PrimaryAttribute = Unit->GetCombatAttributeSet()
			? Unit->GetCombatAttributeSet()->GetPrimaryAttribute() : ECombatPrimaryAttribute::Strength;
		if (const UCombatInventoryComponent* Inventory = Unit->GetCombatInventoryComponent())
		{
			Inventory->BuildViews(Next.Items);
			Next.InventoryRevision = Inventory->GetRevision();
		}
		Next.Strength = Asc->GetNumericAttribute(UCombatAttributeSet::GetStrengthAttribute());
		Next.Agility = Asc->GetNumericAttribute(UCombatAttributeSet::GetAgilityAttribute());
		Next.Intelligence = Asc->GetNumericAttribute(UCombatAttributeSet::GetIntelligenceAttribute());
		Next.Health = Asc->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute());
		Next.MaxHealth = Asc->GetNumericAttribute(UCombatAttributeSet::GetMaxHealthAttribute());
		Next.Mana = Asc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute());
		Next.MaxMana = Asc->GetNumericAttribute(UCombatAttributeSet::GetMaxManaAttribute());
		Next.AttackDamage = Asc->GetNumericAttribute(UCombatAttributeSet::GetAttackDamageAttribute());
		Next.Armor = Asc->GetNumericAttribute(UCombatAttributeSet::GetArmorAttribute());
		Next.MagicResist = Asc->GetNumericAttribute(UCombatAttributeSet::GetMagicResistAttribute());
		Next.Evasion = Asc->GetNumericAttribute(UCombatAttributeSet::GetEvasionAttribute());
		Next.AttackSpeed = Asc->GetNumericAttribute(UCombatAttributeSet::GetAttackSpeedAttribute());
		Next.BaseAttackTime = Asc->GetNumericAttribute(UCombatAttributeSet::GetBaseAttackTimeAttribute());
		Next.MoveSpeed = Asc->GetNumericAttribute(UCombatAttributeSet::GetMoveSpeedAttribute());
		Next.HealthRegen = Asc->GetNumericAttribute(UCombatAttributeSet::GetHealthRegenAttribute());
		Next.ManaRegen = Asc->GetNumericAttribute(UCombatAttributeSet::GetManaRegenAttribute());
		Next.LifestealPct = Asc->GetNumericAttribute(UCombatAttributeSet::GetLifestealPctAttribute());
		Next.SpellAmplifyPct = Asc->GetNumericAttribute(UCombatAttributeSet::GetSpellAmplifyPctAttribute());
		Next.CooldownReductionPct = Asc->GetNumericAttribute(UCombatAttributeSet::GetCooldownReductionPctAttribute());
		Next.CastRangeBonus = Asc->GetNumericAttribute(UCombatAttributeSet::GetCastRangeBonusAttribute());
		Next.StatusResistancePct = Asc->GetNumericAttribute(UCombatAttributeSet::GetStatusResistancePctAttribute());
		Next.AttackRange = Asc->GetNumericAttribute(UCombatAttributeSet::GetAttackRangeAttribute());
		Next.HealAmplifyPct = Asc->GetNumericAttribute(UCombatAttributeSet::GetHealAmplifyPctAttribute());
		Next.HealReceivedPct = Asc->GetNumericAttribute(UCombatAttributeSet::GetHealReceivedPctAttribute());
		if (const UCombatProgressionComponent* Progression = Unit->GetCombatProgressionComponent())
		{
			Next.Level = Progression->GetLevel();
			Next.Experience = Progression->GetExperience();
			Next.ExperienceIntoLevel = Progression->GetExperienceIntoLevel();
			Next.ExperienceToNextLevel = Progression->GetExperienceToNextLevel();
			Next.ExperienceProgress = Progression->GetExperienceProgress();
			Next.UnspentAbilityPoints = Progression->GetUnspentAbilityPoints();
		}
		for (const FGameplayAbilitySpec* Spec : CombatHUDView::GetSlots(*Asc))
		{
			const UCombatAbilityData* Data = Asc->GetCombatAbilityData(Spec->Handle);
			if (!Data) continue;
			FCombatHUDAbilityView& Item = Next.Abilities.AddDefaulted_GetRef();
			Item.SpecHandle = Spec->Handle;
			Item.DefinitionId = Data->GetPrimaryAssetId();
			Item.Level = Spec->Level;
			Item.MaxLevel = Data->MaxLevel;
			Item.ManaCost = Data->GetSpecialValue(TEXT("mana_cost"), Spec->Level);
			Item.bIgnoreSilence = Data->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_IgnoreSilence);
			Item.bUsesAutoCastToggleInput = Data->UsesAutoCastToggleInput();
			Item.bAutoCastEnabled = Item.bUsesAutoCastToggleInput && Asc->IsAutoCastEnabled(Spec->Handle);
			Item.bCanUpgrade = Next.UnspentAbilityPoints > 0 && Spec->Level < Data->MaxLevel
				&& Spec->Level < Next.Level;
			Asc->GetCombatAbilityCooldownWindow(Spec->Handle, Item.CooldownEndTime, Item.CooldownDuration);
		}
	}
	if (!(Next == HUDOwnerView))
	{
		HUDOwnerView = MoveTemp(Next);
		OnHUDOwnerViewChanged.Broadcast();
		Unit->ForceNetUpdate();
	}
}

void UCombatUnitViewComponent::OnRep_HUDOwnerView()
{
	OnHUDOwnerViewChanged.Broadcast();
}
