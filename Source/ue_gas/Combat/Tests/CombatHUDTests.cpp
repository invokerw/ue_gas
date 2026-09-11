#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include <limits>
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Combat/CombatEffectUtilities.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/UI/CombatHUDWidget.h"
#include "Combat/UI/CombatHUDSlotWidget.h"
#include "Combat/UI/CombatRadialProgress.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "ue_gasPlayerController.h"

namespace CombatHUDTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	/** 用正式 Unit 初始化和指挥绑定创建 HUD 观察目标。 */
	ACombatUnitCharacter* SpawnUnit(UWorld& World, Aue_gasPlayerController& Player, FName Name)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACombatUnitCharacter* Unit = World.SpawnActor<ACombatUnitCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!Unit) return nullptr;
		UCombatUnitData* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = Name;
		Data->InitialTeamId = FCombatTeamId(1);
		Data->BaseStats.AttackDamage = 68.0f;
		Data->BaseStats.Armor = -3.0f;
		Data->BaseStats.MaxMana = 500.0f;
		if (!Unit->InitializeFromUnitData(Data)) return nullptr;
		if (!Unit->HasActorBegunPlay()) Unit->DispatchBeginPlay();
		if (!Player.SetCommandedUnitAuthority(Unit)) return nullptr;
		Unit->GetCombatUnitViewComponent()->RefreshHUDOwnerView();
		return Unit;
	}
}

/** 拥有者快照读取真实属性和已提交冷却；后续 CDR 不重算旧时间窗，清空拥有者后不残留。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatHUDOwnerProjectionTest, "Combat.UI.HUD.OwnerProjectionAndFrozenCooldown", CombatHUDTests::Flags)
bool FCombatHUDOwnerProjectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	Aue_gasPlayerController* Player = Fixture.GetWorld()->SpawnActor<Aue_gasPlayerController>();
	if (!TestNotNull(TEXT("Player"), Player)) return false;
	ACombatUnitCharacter* Unit = CombatHUDTests::SpawnUnit(*Fixture.GetWorld(), *Player, TEXT("hud_owner"));
	if (!TestNotNull(TEXT("Unit"), Unit)) return false;
	UCombatUnitViewComponent* View = Unit->GetCombatUnitViewComponent();
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	const FCombatHUDOwnerView Initial = View->GetHUDOwnerView();
	TestEqual(TEXT("Negative armor remains visible"), Initial.Armor, -3.0f);
	TestEqual(TEXT("Attack comes from ASC"), Initial.AttackDamage, 68.0f);
	TestEqual(TEXT("Projection has current life"), Initial.LifeGeneration, Unit->GetLifeGeneration());
	View->RefreshHUDOwnerView();
	TestTrue(TEXT("Unchanged sampling produces identical payload"), Initial == View->GetHUDOwnerView());
	UCombatAbilityData* Data = NewObject<UCombatAbilityData>(Unit);
	Data->DefinitionName = TEXT("hud_cooldown");
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	Data->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	FCombatSpecialValue Cost, Cooldown;
	Cost.Values.Add(40.0f);
	Cooldown.Values.Add(10.0f);
	Data->SpecialValues.Add(TEXT("mana_cost"), Cost);
	Data->SpecialValues.Add(TEXT("cooldown"), Cooldown);
	TGuardValue<TObjectPtr<UCombatAbilityData>> RestoreData(GetMutableDefault<UCombatSelfHealAbility>()->AbilityData, Data);
	FGameplayAbilitySpecHandle Handle;
	FGameplayTag Failure;
	if (!TestTrue(TEXT("Grant through public API"), Asc->GrantCombatAbility(UCombatSelfHealAbility::StaticClass(), 1, false, Handle, Failure))) return false;
	UCombatAbilityData* AutoCastData = NewObject<UCombatAbilityData>(Unit);
	AutoCastData->DefinitionName = TEXT("hud_autocast");
	AutoCastData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	AutoCastData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_Passive);
	AutoCastData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_AutoCast);
	AutoCastData->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	TGuardValue<TObjectPtr<UCombatAbilityData>> RestoreAutoCastData(
		GetMutableDefault<UCombatFrostArrowsAbility>()->AbilityData, AutoCastData);
	FGameplayAbilitySpecHandle AutoCastHandle;
	if (!TestTrue(TEXT("Grant passive AutoCast through public API"), Asc->GrantCombatAbility(
		UCombatFrostArrowsAbility::StaticClass(), 1, true, AutoCastHandle, Failure))) return false;
	bool CostCommitted = false, CooldownCommitted = false;
	TestTrue(TEXT("Commit through public API"), Asc->CommitCombatAbilityStage(Handle, *Data, 1,
		ECombatAbilityCommitStage::SpellStarted, CostCommitted, CooldownCommitted, Failure));
	View->RefreshHUDOwnerView();
	FCombatHUDOwnerView Snapshot = View->GetHUDOwnerView();
	if (!TestEqual(TEXT("Active and passive AutoCast skills are visible"), Snapshot.Abilities.Num(), 2)) return false;
	TestEqual(TEXT("Identity uses actual Spec"), Snapshot.Abilities[0].SpecHandle, Handle);
	TestEqual(TEXT("AutoCast keeps its AbilitySpec slot"), Snapshot.Abilities[1].SpecHandle, AutoCastHandle);
	TestTrue(TEXT("AutoCast slot exposes toggle input semantics"), Snapshot.Abilities[1].bUsesAutoCastToggleInput);
	TestTrue(TEXT("AutoCast slot exposes initial authoritative state"), Snapshot.Abilities[1].bAutoCastEnabled);
	TestEqual(TEXT("Real mana cost"), Snapshot.Abilities[0].ManaCost, 40.0f);
	TestEqual(TEXT("Frozen duration"), Snapshot.Abilities[0].CooldownDuration, 10.0f);
	const double FrozenEnd = Snapshot.Abilities[0].CooldownEndTime;
	CombatEffectUtilities::ApplyAttributeAdditive(Asc, *Asc, UCombatAttributeSet::GetCooldownReductionPctAttribute(), 0.5f);
	View->RefreshHUDOwnerView();
	TestEqual(TEXT("CDR change preserves end"), View->GetHUDOwnerView().Abilities[0].CooldownEndTime, FrozenEnd);
	TestEqual(TEXT("CDR change preserves duration"), View->GetHUDOwnerView().Abilities[0].CooldownDuration, 10.0f);
	TestTrue(TEXT("Disable AutoCast through public API"), Asc->SetAutoCastEnabled(AutoCastHandle, false, Failure));
	View->RefreshHUDOwnerView();
	TestFalse(TEXT("HUD projection follows authoritative AutoCast state"),
		View->GetHUDOwnerView().Abilities[1].bAutoCastEnabled);
	TestTrue(TEXT("Remove through public API"), Asc->RemoveCombatAbility(Handle, Failure));
	TestTrue(TEXT("Remove AutoCast through public API"), Asc->RemoveCombatAbility(AutoCastHandle, Failure));
	View->RefreshHUDOwnerView();
	TestTrue(TEXT("Removed skill no longer appears"), View->GetHUDOwnerView().Abilities.IsEmpty());
	Player->SetCommandedUnitAuthority(nullptr);
	View->RefreshHUDOwnerView();
	TestEqual(TEXT("Unowned snapshot cleared"), View->GetHUDOwnerView().LifeGeneration, int64(0));
	return true;
}

/** 本地进度对过期与异常时间安全；经验进度不写入任何战斗属性。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatHUDProgressTest, "Combat.UI.HUD.ProgressBoundaries", CombatHUDTests::Flags)
bool FCombatHUDProgressTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestEqual(TEXT("Server clock countdown"), UCombatHUDSlotWidget::Remaining(10.0, 3.5), 6.5f);
	TestEqual(TEXT("Expired never negative"), UCombatHUDSlotWidget::Remaining(2.0, 10.0), 0.0f);
	TestEqual(TEXT("Invalid endpoint rejected"), UCombatHUDSlotWidget::Remaining(std::numeric_limits<double>::quiet_NaN(), 1.0), 0.0f);
	UCombatRadialProgress* Ring = NewObject<UCombatRadialProgress>();
	Ring->SetProgress(1.5f);
	TestEqual(TEXT("Experience upper bound"), Ring->Percent, 1.0f);
	Ring->SetProgress(-0.5f);
	TestEqual(TEXT("Experience lower bound"), Ring->Percent, 0.0f);
	Ring->SetProgress(std::numeric_limits<float>::quiet_NaN());
	TestEqual(TEXT("Invalid experience rejected"), Ring->Percent, 0.0f);
	return true;
}

/** 真实 Widget Blueprint 的资源接线、紧凑槽位和观察生命周期回归。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatHUDBlueprintLifecycleTest, "Combat.UI.HUD.BlueprintLayoutAndLifecycle", CombatHUDTests::Flags)
bool FCombatHUDBlueprintLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	Aue_gasPlayerController* Player = Fixture.GetWorld()->SpawnActor<Aue_gasPlayerController>();
	if (!TestNotNull(TEXT("Player"), Player)) return false;
	ACombatUnitCharacter* First = CombatHUDTests::SpawnUnit(*Fixture.GetWorld(), *Player, TEXT("hud_first"));
	if (!TestNotNull(TEXT("First unit"), First)) return false;
	UClass* WidgetClass = LoadClass<UCombatHUDWidget>(nullptr, TEXT("/Game/Combat/Demo/UI/WBP_CombatHUD.WBP_CombatHUD_C"));
	if (!TestNotNull(TEXT("HUD Blueprint loads"), WidgetClass)) return false;
	UCombatHUDWidget* Widget = NewObject<UCombatHUDWidget>(Player, WidgetClass);
	Widget->Initialize();
	Widget->SetOwningPlayer(Player);
	TSharedPtr<SWidget> Slate = Widget->TakeWidget();
	Widget->InitializeForUnit(First);
	Widget->InitializeForUnit(First);
	UCombatUnitViewComponent* FirstView = First->GetCombatUnitViewComponent();
	TestEqual(TEXT("HUD binds once"), FirstView->OnHUDOwnerViewChanged.GetAllObjects().Num(), 1);
	const UProgressBar* Health = Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("HealthBar")));
	if (!TestNotNull(TEXT("Blueprint owns health bar"), Health)) return false;
	TestEqual(TEXT("Resources initialized from real view"), Health->GetPercent(), 1.0f);
	for (const TCHAR* Prefix : { TEXT("ItemSlot"), TEXT("BackpackSlot") })
	{
		const int32 Count = FString(Prefix) == TEXT("ItemSlot") ? 6 : 3;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			UWidget* Item = Widget->WidgetTree->FindWidget(FName(FString::Printf(TEXT("%s%d"), Prefix, Index)));
			if (!TestNotNull(TEXT("Permanent rectangular slot exists"), Item)) return false;
			const UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Item->Slot);
			if (!TestNotNull(TEXT("Slot has Designer geometry"), Slot)) return false;
			TestTrue(TEXT("Item is landscape"), Slot->GetSize().X > Slot->GetSize().Y);
		}
	}
	Widget->ReleaseSlateResources(true);
	Slate.Reset();
	TestEqual(TEXT("Destruct unbinds HUD view"), FirstView->OnHUDOwnerViewChanged.GetAllObjects().Num(), 0);
	Slate = Widget->TakeWidget();
	TestEqual(TEXT("Rebuild restores subscription"), FirstView->OnHUDOwnerViewChanged.GetAllObjects().Num(), 1);
	const int64 OldLife = First->GetLifeGeneration();
	FCombatDamageRequest Lethal;
	Lethal.Source = First;
	Lethal.Target = First;
	Lethal.Amount = 10000.0f;
	Lethal.DamageType = ECombatDamageType::Pure;
	TestTrue(TEXT("Lethal damage uses real pipeline"), Fixture.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Lethal).bSuccess);
	TestEqual(TEXT("Death still shows actual zero health"), Health->GetPercent(), 0.0f);
	TestTrue(TEXT("Respawn"), First->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(500.0, 0.0, 0.0)));
	FirstView->RefreshHUDOwnerView();
	TestTrue(TEXT("Snapshot uses new life"), Widget->GetDisplaySnapshot().LifeGeneration > OldLife);
	ACombatUnitCharacter* Second = CombatHUDTests::SpawnUnit(*Fixture.GetWorld(), *Player, TEXT("hud_second"));
	if (!TestNotNull(TEXT("Second unit"), Second)) return false;
	Widget->InitializeForUnit(Second);
	TestEqual(TEXT("Switch removes old subscription"), FirstView->OnHUDOwnerViewChanged.GetAllObjects().Num(), 0);
	TestEqual(TEXT("Switch changes observer"), Widget->GetObservedUnit(), Second);
	Second->Destroy();
	TestNull(TEXT("Owner EndPlay clears observer"), Widget->GetObservedUnit());
	TestEqual(TEXT("Owner EndPlay clears snapshot"), Widget->GetDisplaySnapshot().LifeGeneration, int64(0));
	Widget->ReleaseSlateResources(true);
	Slate.Reset();
	return true;
}

/** 真实槽蓝图显示缺蓝、沉默、冷却和效果倒计时，不由本地零秒移除权威效果。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatHUDSlotStateTest, "Combat.UI.HUD.SlotStatesAndModifierWindows", CombatHUDTests::Flags)
bool FCombatHUDSlotStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	APlayerController* Player = Fixture.GetWorld()->SpawnActor<APlayerController>();
	UClass* SkillClass = LoadClass<UCombatHUDSlotWidget>(nullptr, TEXT("/Game/Combat/Demo/UI/WBP_CombatHUDSkill.WBP_CombatHUDSkill_C"));
	UClass* BuffClass = LoadClass<UCombatHUDSlotWidget>(nullptr, TEXT("/Game/Combat/Demo/UI/WBP_CombatHUDBuff.WBP_CombatHUDBuff_C"));
	if (!TestNotNull(TEXT("Skill Blueprint"), SkillClass) || !TestNotNull(TEXT("Buff Blueprint"), BuffClass)) return false;
	UCombatHUDSlotWidget* Skill = NewObject<UCombatHUDSlotWidget>(Player, SkillClass);
	UCombatHUDSlotWidget* Buff = NewObject<UCombatHUDSlotWidget>(Player, BuffClass);
	Skill->Initialize();
	Buff->Initialize();
	TSharedPtr<SWidget> SkillSlate = Skill->TakeWidget(), BuffSlate = Buff->TakeWidget();
	UTextBlock* SkillCount = Cast<UTextBlock>(Skill->WidgetTree->FindWidget(TEXT("CountText")));
	UImage* Shade = Cast<UImage>(Skill->WidgetTree->FindWidget(TEXT("CooldownShade")));
	UTextBlock* BuffCount = Cast<UTextBlock>(Buff->WidgetTree->FindWidget(TEXT("CountText")));
	UTextBlock* Stack = Cast<UTextBlock>(Buff->WidgetTree->FindWidget(TEXT("StackText")));
	UCombatRadialProgress* Ring = Cast<UCombatRadialProgress>(Buff->WidgetTree->FindWidget(TEXT("DurationRing")));
	if (!SkillCount || !Shade || !BuffCount || !Stack || !Ring) { AddError(TEXT("Missing required slot visuals")); return false; }
	FCombatHUDAbilityView Ability;
	Ability.DefinitionId = FPrimaryAssetId(TEXT("CombatAbility"), TEXT("hud_skill"));
	Ability.Level = 1;
	Ability.MaxLevel = 4;
	Ability.ManaCost = 40.0f;
	Ability.CooldownEndTime = 15.0;
	Ability.CooldownDuration = 10.0f;
	FCombatUnitView Unit;
	Unit.LifeState = ECombatLifeState::Alive;
	Unit.Mana = 100.0f;
	const FText Name = FText::FromString(TEXT("测试技能")), Key = FText::FromString(TEXT("Q"));
	Skill->ShowAbility(Ability, Unit, 10.0, Name, FText::GetEmpty(), nullptr, Key);
	TestEqual(TEXT("Cooldown uses server clock"), SkillCount->GetText().ToString(), FString(TEXT("5.0")));
	TestEqual(TEXT("Cooldown shade uses frozen window"), Shade->GetRenderTransform().Scale.Y, 0.5);
	Unit.Mana = 0.0f;
	Skill->ShowAbility(Ability, Unit, 10.0, Name, FText::GetEmpty(), nullptr, Key);
	TestEqual(TEXT("Mana block visible"), SkillCount->GetText().ToString(), FString(TEXT("缺蓝")));
	Unit.Mana = 100.0f;
	Unit.VisibleStatusTags.AddTag(CombatTags::State_Silenced);
	Skill->ShowAbility(Ability, Unit, 20.0, Name, FText::GetEmpty(), nullptr, Key);
	TestEqual(TEXT("Silence block visible"), SkillCount->GetText().ToString(), FString(TEXT("沉默")));
	Ability.bIgnoreSilence = true;
	Skill->ShowAbility(Ability, Unit, 20.0, Name, FText::GetEmpty(), nullptr, Key);
	TestTrue(TEXT("Ignore silence follows definition"), SkillCount->GetText().IsEmpty());
	Ability.bUsesAutoCastToggleInput = true;
	Ability.bAutoCastEnabled = true;
	Skill->ShowAbility(Ability, Unit, 20.0, Name, FText::GetEmpty(), nullptr, Key);
	TestEqual(TEXT("Enabled AutoCast is visible"), SkillCount->GetText().ToString(), FString(TEXT("自动")));
	Ability.bAutoCastEnabled = false;
	Skill->ShowAbility(Ability, Unit, 20.0, Name, FText::GetEmpty(), nullptr, Key);
	TestEqual(TEXT("Disabled AutoCast is visible"), SkillCount->GetText().ToString(), FString(TEXT("关闭")));
	FCombatModifierView Modifier;
	Modifier.ServerStartTime = 10.0;
	Modifier.ServerEndTime = 20.0;
	Modifier.StackCount = 3;
	Buff->ShowModifier(Modifier, 15.0, Name, nullptr);
	TestEqual(TEXT("Buff has five seconds"), BuffCount->GetText().ToString(), FString(TEXT("5s")));
	TestEqual(TEXT("Buff stack count"), Stack->GetText().ToString(), FString(TEXT("3")));
	TestEqual(TEXT("Buff duration fraction"), Ring->Percent, 0.5f);
	Buff->ShowModifier(Modifier, 30.0, Name, nullptr);
	TestEqual(TEXT("Expired view remains present at zero"), BuffCount->GetText().ToString(), FString(TEXT("0s")));
	Modifier.ServerEndTime = 0.0;
	Buff->ShowModifier(Modifier, 30.0, Name, nullptr);
	TestEqual(TEXT("Infinite duration"), BuffCount->GetText().ToString(), FString(TEXT("∞")));
	Skill->ReleaseSlateResources(true);
	Buff->ReleaseSlateResources(true);
	SkillSlate.Reset();
	BuffSlate.Reset();
	return true;
}
#endif
