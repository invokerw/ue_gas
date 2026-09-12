#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include <limits>
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/UI/CombatOverheadWidget.h"
#include "Combat/UI/CombatOverheadWidgetComponent.h"
#include "Combat/UI/CombatFloatingTextWidget.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/View/CombatUnitViewComponent.h"

namespace CombatOverheadTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** 通过正式初始化入口生成测试单位，不直接写最终属性。 */
	ACombatUnitCharacter* SpawnUnit(UWorld& World, const FName Name)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACombatUnitCharacter* Unit = World.SpawnActor<ACombatUnitCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!Unit) return nullptr;
		UCombatUnitData* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = Name;
		Data->InitialTeamId = FCombatTeamId(1);
		if (!Unit->InitializeFromUnitData(Data)) return nullptr;
		// 临时 World 没有 GameMode 驱动 MatchStart；显式进入 Actor 生命周期后才能验证 Destroy/EndPlay。
		if (!Unit->HasActorBegunPlay()) { Unit->DispatchBeginPlay(); }
		return Unit;
	}

	/** 构造明确的展示规则，验证算法与蓝图视觉配置相互独立。 */
	FCombatControlPresentationRule Rule(const FGameplayTag Tag, const TCHAR* Label, const int32 Priority)
	{
		FCombatControlPresentationRule Result;
		Result.Tag = Tag;
		Result.Label = FText::FromString(Label);
		Result.Priority = Priority;
		return Result;
	}
}

/** 多来源状态的无限期限不受顺序影响，主状态与数字投影保持安全。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatOverheadProjectionTest,
	"Combat.UI.Overhead.ProjectionAndControlWindows", CombatOverheadTests::Flags)
bool FCombatOverheadProjectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatUnitView View;
	View.UnitDefinitionId = FPrimaryAssetId(TEXT("CombatUnit"), TEXT("ui_test"));
	View.LifeGeneration = 1;
	View.Health = 75.0f;
	View.MaxHealth = 100.0f;
	View.Mana = 25.0f;
	View.MaxMana = 50.0f;
	View.VisibleStatusTags.AddTag(CombatTags::State_Stunned);
	View.VisibleStatusTags.AddTag(CombatTags::State_Silenced);
	const TArray<FCombatControlPresentationRule> Rules = {
		CombatOverheadTests::Rule(CombatTags::State_Silenced, TEXT("沉默"), 50),
		CombatOverheadTests::Rule(CombatTags::State_Stunned, TEXT("眩晕"), 100) };
	FCombatModifierView Finite;
	Finite.ControlTags.AddTag(CombatTags::State_Stunned);
	Finite.ServerStartTime = 2.0;
	Finite.ServerEndTime = 6.0;
	FCombatModifierView Longer = Finite;
	Longer.ServerEndTime = 10.0;
	FCombatModifierView Infinite = Finite;
	Infinite.ServerEndTime = 0.0;
	const FText Name = FText::FromString(TEXT("预览单位"));
	auto Build = [&](const TArray<FCombatModifierView>& Modifiers)
	{
		return UCombatOverheadWidget::BuildDisplayData(View, Modifiers, Rules,
			ECombatTeamRelation::Friendly, Name, FText::GetEmpty());
	};
	FCombatOverheadDisplayData Data = Build({ Finite, Longer });
	TestTrue(TEXT("Initialized living view displays information"), Data.bReady && Data.bShowInfo);
	TestEqual(TEXT("Health ratio"), Data.HealthPercent, 0.75f);
	TestEqual(TEXT("Mana ratio"), Data.ManaPercent, 0.5f);
	TestEqual(TEXT("Priority selects stun"), Data.PrimaryStatusTag, CombatTags::State_Stunned.GetTag());
	TestEqual(TEXT("Names follow configured priority"), Data.StatusLabel.ToString(), FString(TEXT("眩晕 · 沉默")));
	TestEqual(TEXT("Latest finite source wins"), Data.StatusEndTime, 10.0);
	TestEqual(TEXT("Finite countdown uses server time"),
		UCombatOverheadWidget::ComputeProgress(Data, 4.0).StatusRemaining, 6.0f);
	TestEqual(TEXT("Infinite source first"), Build({ Infinite, Finite }).StatusEndTime, 0.0);
	TestEqual(TEXT("Infinite source last"), Build({ Finite, Infinite }).StatusEndTime, 0.0);
	TestEqual(TEXT("Infinite countdown sentinel"),
		UCombatOverheadWidget::ComputeProgress(Build({ Infinite }), 99.0).StatusRemaining, -1.0f);
	View.VisibleStatusTags.Reset();
	TestFalse(TEXT("Removed control disappears"), Build({}).bShowStatus);
	// 网络投影没有服务器内部 EventId 序号；客户端仍须按明确阶段和时间窗显示进度。
	View.ActiveAbilityDefinitionId = FPrimaryAssetId(TEXT("CombatAbility"), TEXT("ui_cast"));
	View.AbilityPhase = ECombatAbilityViewPhase::Casting;
	View.AbilityServerStartTime = 2.0;
	View.AbilityServerEndTime = 4.0;
	View.bChanneling = true;
	Data = Build({});
	TestTrue(TEXT("Replicated phase displays without server-only event sequence"), Data.bShowAbility);
	TestFalse(TEXT("Channel configuration does not replace casting phase"), Data.bIsChanneling);
	TestEqual(TEXT("Client cast countdown uses replicated window"),
		UCombatOverheadWidget::ComputeProgress(Data, 3.0).AbilityRemaining, 1.0f);
	View.AbilityPhase = ECombatAbilityViewPhase::None;
	TestFalse(TEXT("Cleared phase hides stale window"), Build({}).bShowAbility);
	View.MaxMana = 0.0f;
	View.MaxHealth = 0.0f;
	View.Health = std::numeric_limits<float>::quiet_NaN();
	Data = Build({});
	TestFalse(TEXT("Zero mana maximum hides mana"), Data.bShowMana);
	TestEqual(TEXT("Invalid health cannot reach UMG"), Data.Health, 0.0f);
	TestEqual(TEXT("Zero maximum cannot divide by zero"), Data.HealthPercent, 0.0f);
	View.LifeState = ECombatLifeState::Dead;
	TestFalse(TEXT("Dead view hides information"), Build({}).bShowInfo);
	View.LifeGeneration = 0;
	TestFalse(TEXT("Initial replication is not falsely ready"), Build({}).bReady);
	return true;
}

/** 真实 Ability/Scheduler 进入引导后才切换 UI 阶段，旧激活结束不会抹掉新阶段。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatOverheadPhaseTest,
	"Combat.UI.Overhead.AuthoritativeAbilityPhases", CombatOverheadTests::Flags)
bool FCombatOverheadPhaseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture(NM_DedicatedServer);
	if (!Fixture.IsValid()) return false;
	ACombatUnitCharacter* Unit = CombatOverheadTests::SpawnUnit(*Fixture.GetWorld(), TEXT("ui_phase"));
	if (!TestNotNull(TEXT("Unit"), Unit)) return false;
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	UCombatUnitViewComponent* View = Unit->GetCombatUnitViewComponent();
	UCombatAbilityData* Data = NewObject<UCombatAbilityData>(Unit);
	Data->DefinitionName = TEXT("ui_channel");
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_Channelled);
	Data->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	Data->CastPoint = 0.4f;
	Data->ChannelDuration = 3.0f;
	Data->ChannelInterval = 1.0f;
	TGuardValue<TObjectPtr<UCombatAbilityData>> RestoreData(GetMutableDefault<UCombatChannelProbeAbility>()->AbilityData, Data);
	FGameplayAbilitySpecHandle Handle;
	FGameplayTag Failure;
	TestTrue(TEXT("Grant"), Asc->GrantCombatAbility(UCombatChannelProbeAbility::StaticClass(), 1, false, Handle, Failure));
	TestTrue(TEXT("Activate"), Asc->TryActivateCombatAbility(Handle, FCombatAbilityTargetData(), Failure));
	TestEqual(TEXT("Channelled spell starts in casting phase"), View->GetUnitView().AbilityPhase, ECombatAbilityViewPhase::Casting);
	TestTrue(TEXT("Cast window excludes channel duration"), FMath::IsNearlyEqual(
		View->GetUnitView().AbilityServerEndTime - View->GetUnitView().AbilityServerStartTime, 0.4, 0.001));
	Fixture.GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(Fixture.GetWorld()->GetTimeSeconds() + 0.5);
	TestEqual(TEXT("Scheduler starts channel phase"), View->GetUnitView().AbilityPhase, ECombatAbilityViewPhase::Channeling);
	TestEqual(TEXT("Channel window is phase duration"),
		View->GetUnitView().AbilityServerEndTime - View->GetUnitView().AbilityServerStartTime, 3.0);
	View->NotifyAbilityEnded(FCombatEventId());
	TestEqual(TEXT("Stale end cannot clear current phase"), View->GetUnitView().AbilityPhase, ECombatAbilityViewPhase::Channeling);
	Asc->AddLooseGameplayTag(CombatTags::State_Stunned);
	TestEqual(TEXT("Authoritative interruption clears UI"), View->GetUnitView().AbilityPhase, ECombatAbilityViewPhase::None);
	// 生产资产在 BeginPlay 前配置 WidgetClass；引擎非虚 SetWidgetClass 的运行时重建不经过 InitWidget。
	UCombatOverheadWidgetComponent* ServerComponent = NewObject<UCombatOverheadWidgetComponent>(Unit);
	ServerComponent->SetWidgetClass(UCombatOverheadWidget::StaticClass());
	ServerComponent->RegisterComponent();
	ServerComponent->InitWidget();
	TestNull(TEXT("Dedicated server never creates UMG"), ServerComponent->GetUserWidgetObject());
	ServerComponent->DestroyComponent();
	return true;
}

/** 使用真实头顶蓝图验证控件树、绑定、重建、换生命与 Owner EndPlay。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatOverheadBlueprintLifecycleTest,
	"Combat.UI.Overhead.BlueprintBindingLifecycle", CombatOverheadTests::Flags)
bool FCombatOverheadBlueprintLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UClass* WidgetClass = LoadClass<UCombatOverheadWidget>(nullptr,
		TEXT("/Game/Combat/Demo/UI/WBP_CombatOverhead.WBP_CombatOverhead_C"));
	if (!TestNotNull(TEXT("Head widget blueprint asset loads"), WidgetClass)) return false;
	ACombatUnitCharacter* Unit = CombatOverheadTests::SpawnUnit(*Fixture.GetWorld(), TEXT("ui_lifecycle"));
	if (!TestNotNull(TEXT("Unit"), Unit)) return false;
	UCombatUnitViewComponent* View = Unit->GetCombatUnitViewComponent();
	UCombatOverheadWidgetComponent* Component = Unit->GetCombatOverheadWidgetComponent();
	Component->SetWidgetClass(WidgetClass);
	Component->InitWidget();
	UCombatOverheadWidget* Widget = Cast<UCombatOverheadWidget>(Component->GetUserWidgetObject());
	if (!TestNotNull(TEXT("Component instantiates blueprint"), Widget)) return false;
	TSharedPtr<SWidget> Slate = Widget->TakeWidget();
	TestNotNull(TEXT("Blueprint owns health bar"), Widget->WidgetTree->FindWidget(TEXT("HealthBar")));
	const UProgressBar* HealthBar = Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("HealthBar")));
	if (!TestNotNull(TEXT("Health widget uses a Blueprint progress bar"), HealthBar)) return false;
	TestEqual(TEXT("Blueprint applies initial health snapshot"), HealthBar->GetPercent(), Widget->GetDisplayData().HealthPercent);
	const FSoftObjectPath DemoUnitPath = UAssetManager::Get().GetPrimaryAssetPath(
		FPrimaryAssetId(TEXT("CombatUnit"), TEXT("ranged_combat_player")));
	TestTrue(TEXT("Demo unit is discoverable by replicated definition ID"), DemoUnitPath.IsValid());
	const UCombatUnitData* DemoData = Cast<UCombatUnitData>(DemoUnitPath.TryLoad());
	if (!TestNotNull(TEXT("Demo name definition loads from AssetManager"), DemoData)) return false;
	TestFalse(TEXT("Demo name is localized display text"), DemoData->DisplayNameText.IsEmpty());
	Widget->InitializeForUnit(Unit);
	Widget->InitializeForUnit(Unit);
	TestEqual(TEXT("Repeated initialization binds once"), View->OnUnitViewChanged.GetAllObjects().Num(), 1);
	TestTrue(TEXT("Initial snapshot is available"), Widget->GetDisplayData().bReady);
	const int64 OldLife = Unit->GetLifeGeneration();
	FCombatFloatingTextPayload Payload;
	Payload.Amount = 25.0f;
	Payload.LifeGeneration = OldLife - 1;
	TestNull(TEXT("Old life floating text rejected"), Widget->CreateFloatingText(Payload));
	Widget->ReleaseSlateResources(true);
	Slate.Reset();
	TestEqual(TEXT("Destruct removes subscriptions"), View->OnUnitViewChanged.GetAllObjects().Num(), 0);
	Slate = Widget->TakeWidget();
	TestEqual(TEXT("Rebuild restores one subscription"), View->OnUnitViewChanged.GetAllObjects().Num(), 1);
	TestTrue(TEXT("Rebuild resends full snapshot"), Widget->GetDisplayData().bReady);
	Unit->GetCombatLifecycleComponent()->RequestDeath(FCombatEventContext(), nullptr);
	TestFalse(TEXT("Death hides resource information"), Widget->GetDisplayData().bShowInfo);
	TestTrue(TEXT("Respawn"), Unit->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(500.0, 0.0, 0.0)));
	TestTrue(TEXT("Respawn refreshes life"), Widget->GetDisplayData().LifeGeneration > OldLife);
	Payload.LifeGeneration = OldLife;
	TestNull(TEXT("Previous life cannot create a number after respawn"), Widget->CreateFloatingText(Payload));
	Unit->Destroy();
	TestFalse(TEXT("Owner EndPlay clears snapshot"), Widget->GetDisplayData().bReady);
	TestEqual(TEXT("Owner EndPlay removes subscriptions"), View->OnUnitViewChanged.GetAllObjects().Num(), 0);
	Slate.Reset();
	Widget->ReleaseSlateResources(true);
	return true;
}
#endif
