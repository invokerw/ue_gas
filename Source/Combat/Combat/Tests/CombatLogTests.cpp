#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include <limits>
#include "Misc/AutomationTest.h"
#include "Combat/Log/CombatLogTypes.h"
#include "Combat/Log/CombatLogComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Combat/CombatHealSubsystem.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "CombatPlayerController.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Combat/UI/CombatLogWidget.h"
#include "Combat/UI/CombatPlayerHUD.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBoxSlot.h"

namespace CombatLogTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	/** 生成通过正式定义初始化的测试单位，不直接改写资源。 */
	ACombatUnitCharacter* SpawnUnit(UWorld& World, const FName Name)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACombatUnitCharacter* Unit = World.SpawnActor<ACombatUnitCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!Unit) return nullptr;
		UCombatUnitData* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = Name;
		Data->InitialTeamId = FCombatTeamId(1);
		Data->BaseStats.MaxHealth = 100.0f;
		return Unit && Unit->InitializeFromUnitData(Data) ? Unit : nullptr;
	}
}

/** 不同 DPI/面板缩放下均贴合视口边缘；重复应用及视口缩小不会累积漂移。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatLogDragGeometryTest, "Combat.UI.Log.WindowDragGeometry", CombatLogTests::Flags)
bool FCombatLogDragGeometryTest::RunTest(const FString& Parameters)
{
	for (const float Dpi : { 0.75f, 1.0f, 1.5f, 2.0f })
	{
		const FGeometry Viewport = FGeometry::MakeRoot(FVector2f(1280, 720), FSlateLayoutTransform(Dpi, FVector2f(200, 100)));
		const FGeometry Parent = Viewport.MakeChild(FVector2f(840, 540), FSlateLayoutTransform(0.75f, FVector2f(16, 62)));
		const FVector2D Unconstrained(100, 80);
		TestTrue(TEXT("Interior translation is preserved without snapping"), CombatLogWindowLayout::ClampTranslation(Viewport, Parent, Unconstrained).Equals(Unconstrained, 0.01));
		const FVector2D TopLeft = CombatLogWindowLayout::ClampTranslation(Viewport, Parent, FVector2D(-10000, -10000));
		TestTrue(TEXT("Top and left edges remain visible at every DPI"), Viewport.AbsoluteToLocal(Parent.LocalToAbsolute(TopLeft)).Equals(FVector2D::ZeroVector, 0.01f));
		const FVector2D BottomRight = CombatLogWindowLayout::ClampTranslation(Viewport, Parent, FVector2D(10000, 10000));
		TestTrue(TEXT("Right and bottom edges remain visible at every DPI"), Viewport.AbsoluteToLocal(Parent.LocalToAbsolute(BottomRight + FVector2D(840, 540))).Equals(FVector2D(1280, 720), 0.01f));
		TestTrue(TEXT("Repeated layout does not cause drift"), CombatLogWindowLayout::ClampTranslation(Viewport, Parent, BottomRight).Equals(BottomRight, 0.01));
		const FGeometry Smaller = FGeometry::MakeRoot(FVector2f(900, 600), FSlateLayoutTransform(Dpi, FVector2f(200, 100)));
		const FVector2D Resized = CombatLogWindowLayout::ClampTranslation(Smaller, Parent, BottomRight);
		TestTrue(TEXT("Viewport shrink recovers the entire panel"), Smaller.AbsoluteToLocal(Parent.LocalToAbsolute(Resized + FVector2D(840, 540))).Equals(FVector2D(900, 600), 0.01f));
	}
	const FGeometry Empty = FGeometry::MakeRoot(FVector2f::ZeroVector, FSlateLayoutTransform());
	TestTrue(TEXT("Unarranged geometry keeps the current position"), CombatLogWindowLayout::ClampTranslation(Empty, Empty, FVector2D(20, 30)).Equals(FVector2D(20, 30)));
	return true;
}

/** 筛选采用事件发生时的身份与时间，开关组合不会改变权威记录。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatLogFilterTest, "Combat.UI.Log.FilterAndFormatting", CombatLogTests::Flags)
bool FCombatLogFilterTest::RunTest(const FString& Parameters)
{
	FCombatLogEntry Entry;
	Entry.Sequence = 1;
	Entry.ServerTime = 40.0;
	Entry.EventType = CombatTags::Event_Combat_DamageApplied;
	Entry.Category = ECombatLogCategory::Damage;
	Entry.SourceActorId = 11;
	Entry.TargetActorId = 22;
	Entry.bSourceHero = true;
	Entry.Amount = 25.0f;
	Entry.bHasHealthChange = true;
	Entry.PreviousHealth = 100.0f;
	Entry.NewHealth = 75.0f;
	FCombatLogFilter Filter;
	TestTrue(TEXT("Thirty-second boundary included"), Filter.Matches(Entry, 70.0));
	TestFalse(TEXT("Expired row hidden"), Filter.Matches(Entry, 70.01));
	Filter.TimeWindowSeconds = 0.0;
	Filter.SourceActorId = 12;
	TestFalse(TEXT("Same definition cannot substitute another instance"), Filter.Matches(Entry, 100.0));
	Filter.SourceActorId = 11;
	Filter.TargetActorId = 22;
	Filter.bIncludeNonHeroes = false;
	TestTrue(TEXT("Hero against a dummy remains visible"), Filter.Matches(Entry, 100.0));
	Entry.bSourceHero = false;
	TestFalse(TEXT("Both non-hero units can be hidden"), Filter.Matches(Entry, 100.0));
	Entry.bSourceHero = true;
	Filter.bDamage = false;
	TestFalse(TEXT("Category can be disabled"), Filter.Matches(Entry, 100.0));
	Filter.bDamage = true;
	Entry.ServerTime = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("Invalid timestamp rejected"), Filter.Matches(Entry, 100.0));
	Entry.ServerTime = 40.0;
	const FString Rich = CombatLogPresentation::BuildRichText(Entry, TEXT("<hero>&"), TEXT("木桩"), TEXT(""));
	TestTrue(TEXT("Definition text cannot inject rich markup"), Rich.Contains(TEXT("&lt;hero&gt;&amp;")));
	TestTrue(TEXT("Transaction endpoints displayed"), Rich.Contains(TEXT("100 → 75")));
	TestTrue(TEXT("Actual applied amount displayed"), Rich.Contains(TEXT("25")));
	TestEqual(TEXT("Millisecond clock"), CombatLogPresentation::FormatTimestamp(79.366), FString(TEXT("[01:19.366]")));
	ECombatLogCategory Category;
	TestFalse(TEXT("Internal projectile diagnostics are omitted"), CombatLogPresentation::Classify(CombatTags::Event_Combat_ProjectileFinished, Category));
	TestTrue(TEXT("Modifier events are supported"), CombatLogPresentation::Classify(CombatTags::Event_Combat_ModifierApplied, Category));
	TestEqual(TEXT("Modifier category"), Category, ECombatLogCategory::Status);
	TMap<FString, int32> Options = { { TEXT("全部"), 0 }, { TEXT("木桩 · #22"), 33 } };
	const FString AllNamedUnit = CombatLogPresentation::BuildUnitOptionLabel(TEXT("全部"), 11, false, Options);
	TestFalse(TEXT("Unit name must not replace the all-units filter"), Options.Contains(AllNamedUnit));
	const FString DuplicateNamedUnit = CombatLogPresentation::BuildUnitOptionLabel(TEXT("木桩"), 22, true, Options);
	TestFalse(TEXT("Generated instance suffix cannot collide with a literal unit name"), Options.Contains(DuplicateNamedUnit));
	TestEqual(TEXT("Distinct ordinary name remains readable"),
		CombatLogPresentation::BuildUnitOptionLabel(TEXT("卓尔游侠"), 44, false, Options), FString(TEXT("卓尔游侠")));
	for (const FPrimaryAssetId& Id : {
		FPrimaryAssetId(TEXT("CombatModifier"), TEXT("frost_arrow_slow")),
		FPrimaryAssetId(TEXT("CombatModifier"), TEXT("frost_arrows_intrinsic")),
		FPrimaryAssetId(TEXT("CombatProjectile"), TEXT("frost_arrows_projectile")) })
	{
		TestTrue(TEXT("Demo effect identity resolves to a loadable asset for client names"), UAssetManager::Get().GetPrimaryAssetPath(Id).IsValid());
	}
	return true;
}

/** 真实蓝图具备入口与筛选，重建只绑定一次，关闭窗口期间仍记录战斗。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatLogWidgetTest, "Combat.UI.Log.BlueprintControlsAndLifecycle", CombatLogTests::Flags)
bool FCombatLogWidgetTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld* World = Fixture.GetWorld();
	ACombatPlayerController* Player = World->SpawnActor<ACombatPlayerController>();
	if (!Player->HasActorBegunPlay()) Player->DispatchBeginPlay();
	UCombatLogComponent* Log = Player->FindComponentByClass<UCombatLogComponent>();
	ACombatUnitCharacter* Hero = CombatLogTests::SpawnUnit(*World, TEXT("widget_hero"));
	ACombatUnitCharacter* Dummy = CombatLogTests::SpawnUnit(*World, TEXT("widget_dummy"));
	if (!Hero || !Dummy || !Log) return false;
	Player->SetCommandedUnitAuthority(Hero);
	UClass* WidgetClass = LoadClass<UCombatLogWidget>(nullptr, TEXT("/Game/Combat/Demo/UI/WBP_CombatLog.WBP_CombatLog_C"));
	if (!TestNotNull(TEXT("Log Blueprint loads with native parent"), WidgetClass)) return false;
	UCombatLogWidget* Widget = NewObject<UCombatLogWidget>(Player, WidgetClass);
	Widget->Initialize();
	Widget->SetOwningPlayer(Player);
	TSharedPtr<SWidget> Slate = Widget->TakeWidget();
	Widget->InitializeForLog(Log);
	Widget->InitializeForLog(Log);
	TestTrue(TEXT("Subscribed"), Log->OnHistoryChanged().IsBoundToObject(Widget));
	TestFalse(TEXT("Panel initially closed"), Widget->IsLogOpen());
	UWidget* TitleBar = Widget->WidgetTree->FindWidget(TEXT("LogTitleBar"));
	UWidget* Panel = Widget->WidgetTree->FindWidget(TEXT("LogPanel"));
	UWidget* Window = Widget->WidgetTree->FindWidget(TEXT("LogScale"));
	if (!TestNotNull(TEXT("Designer supplies a title drag region"), TitleBar) || !TestNotNull(TEXT("Movable panel exists"), Panel)) return false;
	if (!TestNotNull(TEXT("Drag moves the outer window including hit-test ancestors"), Window)) return false;
	TestEqual(TEXT("Title drag region advertises movement"), TitleBar->GetCursor(), EMouseCursor::CardinalCross);
	const UCanvasPanelSlot* TitleSlot = Cast<UCanvasPanelSlot>(TitleBar->Slot);
	if (!TestNotNull(TEXT("Title region belongs to the panel canvas"), TitleSlot)) return false;
	TestTrue(TEXT("Title region stays behind close and follow controls"), TitleSlot->GetZOrder() < 0);
	const USizeBoxSlot* PanelSlot = Cast<USizeBoxSlot>(Panel->Slot);
	if (!TestNotNull(TEXT("Panel fills its stable size container"), PanelSlot)) return false;
	TestEqual(TEXT("Movement geometry has no unaccounted slot padding"), PanelSlot->GetPadding(), FMargin(0.0f));
	Window->SetRenderTranslation(FVector2D(100, 80));
	FCombatDamageRequest Damage;
	Damage.Source = Hero;
	Damage.Target = Dummy;
	Damage.Amount = 12.0f;
	Damage.DamageType = ECombatDamageType::Pure;
	World->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Damage);
	UButton* Entry = Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("LogEntryButton")));
	UButton* Close = Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("CloseLogButton")));
	UCheckBox* DamageCheck = Cast<UCheckBox>(Widget->WidgetTree->FindWidget(TEXT("DamageCheck")));
	UCheckBox* Items = Cast<UCheckBox>(Widget->WidgetTree->FindWidget(TEXT("ItemCheck")));
	UComboBoxString* Sources = Cast<UComboBoxString>(Widget->WidgetTree->FindWidget(TEXT("AttackerCombo")));
	if (!Entry || !Close || !DamageCheck || !Items || !Sources) { AddError(TEXT("Missing required Blueprint controls")); return false; }
	const UCanvasPanelSlot* EntrySlot = Cast<UCanvasPanelSlot>(Entry->Slot);
	if (!TestNotNull(TEXT("Entry has Designer placement"), EntrySlot)) return false;
	TestEqual(TEXT("Entry anchored at upper left"), EntrySlot->GetAnchors().Minimum, FVector2D::ZeroVector);
	TestTrue(TEXT("Entry has usable click width"), EntrySlot->GetSize().X >= 100.0);
	Entry->OnClicked.Broadcast();
	TestTrue(TEXT("Entry opens panel"), Widget->IsLogOpen());
	TestEqual(TEXT("Hidden period retained actual damage"), Widget->GetVisibleEntryCount(), 1);
	TestTrue(TEXT("Source options include units"), Sources->GetOptionCount() >= 3);
	TestFalse(TEXT("Items explicitly unavailable"), Items->GetIsEnabled());
	DamageCheck->SetIsChecked(false);
	DamageCheck->OnCheckStateChanged.Broadcast(false);
	TestEqual(TEXT("Checkbox filters actual rows"), Widget->GetVisibleEntryCount(), 0);
	DamageCheck->SetIsChecked(true);
	DamageCheck->OnCheckStateChanged.Broadcast(true);
	TestEqual(TEXT("Re-enable restores history"), Widget->GetVisibleEntryCount(), 1);
	Close->OnClicked.Broadcast();
	TestFalse(TEXT("Close control works"), Widget->IsLogOpen());
	TestTrue(TEXT("Closing retains the local window position"), Window->GetRenderTransform().Translation.Equals(FVector2D(100, 80)));
	Widget->ReleaseSlateResources(true);
	Slate.Reset();
	TestFalse(TEXT("Destruct removes history subscription"), Log->OnHistoryChanged().IsBoundToObject(Widget));
	Slate = Widget->TakeWidget();
	Widget->SetLogOpen(true);
	TestTrue(TEXT("Slate rebuild retains the local window position"), Window->GetRenderTransform().Translation.Equals(FVector2D(100, 80)));
	TestEqual(TEXT("Rebuild restores historical rows once"), Widget->GetVisibleEntryCount(), 1);
	Widget->ReleaseSlateResources(true);
	Slate.Reset();
	return true;
}

/** 环形历史按提交顺序去重并淘汰最旧记录，防止长局无限增长。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatLogCapacityTest, "Combat.UI.Log.BoundedHistory", CombatLogTests::Flags)
bool FCombatLogCapacityTest::RunTest(const FString& Parameters)
{
	FCombatLogArray History;
	for (int64 Sequence = 1; Sequence <= CombatLogPresentation::MaxEntries + 2; ++Sequence)
	{
		FCombatLogEntry Entry;
		Entry.Sequence = Sequence;
		History.Append(Entry);
	}
	TestEqual(TEXT("Bounded history"), History.Items.Num(), CombatLogPresentation::MaxEntries);
	TestEqual(TEXT("Oldest evicted"), History.Items[0].Sequence, int64(3));
	const FCombatLogEntry Last = History.Items.Last();
	TestFalse(TEXT("Same event is not appended twice"), History.Append(Last));
	FCombatLogEntry Old;
	Old.Sequence = 1;
	TestFalse(TEXT("Stale event rejected"), History.Append(Old));
	return true;
}

/** 玩家历史读取实际事务结果，过量治疗不虚报；销毁组件解绑服务器观察者。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatLogTransactionTest, "Combat.UI.Log.AuthoritativeTransactionsAndTeardown", CombatLogTests::Flags)
bool FCombatLogTransactionTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld* World = Fixture.GetWorld();
	ACombatPlayerController* Player = World->SpawnActor<ACombatPlayerController>();
	UCombatLogComponent* Log = Player ? Player->FindComponentByClass<UCombatLogComponent>() : nullptr;
	if (!TestNotNull(TEXT("Player owns log component"), Log)) return false;
	if (!Player->HasActorBegunPlay()) Player->DispatchBeginPlay();
	ACombatUnitCharacter* Hero = CombatLogTests::SpawnUnit(*World, TEXT("log_hero"));
	ACombatUnitCharacter* Dummy = CombatLogTests::SpawnUnit(*World, TEXT("log_dummy"));
	if (!Hero || !Dummy || !Player->SetCommandedUnitAuthority(Hero)) return false;
	FCombatDamageRequest Damage;
	Damage.Source = Hero;
	Damage.Target = Dummy;
	Damage.Amount = 25.0f;
	Damage.DamageType = ECombatDamageType::Pure;
	TestTrue(TEXT("Damage pipeline"), World->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Damage).bSuccess);
	if (!TestTrue(TEXT("Damage recorded"), !Log->GetEntries().IsEmpty())) return false;
	const FCombatLogEntry DamageRow = Log->GetEntries().Last();
	TestEqual(TEXT("Health before"), DamageRow.PreviousHealth, 100.0f);
	TestEqual(TEXT("Health after"), DamageRow.NewHealth, 75.0f);
	TestEqual(TEXT("Applied damage"), DamageRow.Amount, 25.0f);
	FCombatHealRequest Heal;
	Heal.Source = Hero;
	Heal.Target = Dummy;
	Heal.Amount = 100.0f;
	World->GetSubsystem<UCombatHealSubsystem>()->Heal(Heal);
	TestEqual(TEXT("Overheal excluded"), Log->GetEntries().Last().Amount, 25.0f);
	TestEqual(TEXT("Healed endpoint"), Log->GetEntries().Last().NewHealth, 100.0f);
	TestEqual(TEXT("Older endpoint remains immutable"), Log->GetEntries()[Log->GetEntries().Num() - 2].NewHealth, 75.0f);
	const int32 BeforeZeroHeal = Log->GetEntries().Num();
	World->GetSubsystem<UCombatHealSubsystem>()->Heal(Heal);
	TestEqual(TEXT("Zero regeneration does not flood UI"), Log->GetEntries().Num(), BeforeZeroHeal);
	UClass* RangerClass = LoadClass<ACombatUnitCharacter>(nullptr, TEXT("/Game/Combat/Demo/Heros/DrowRanger/BP_DrowRanger.BP_DrowRanger_C"));
	if (!TestNotNull(TEXT("Real Demo unit class"), RangerClass)) return false;
	FActorSpawnParameters RangerParams;
	RangerParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACombatUnitCharacter* Ranger = World->SpawnActor<ACombatUnitCharacter>(RangerClass, FVector(500.0, 0.0, 0.0), FRotator::ZeroRotator, RangerParams);
	if (!TestNotNull(TEXT("Demo unit with intrinsic effect"), Ranger)) return false;
	if (!Ranger->HasActorBegunPlay()) Ranger->DispatchBeginPlay();
	const FCombatLogEntry* Intrinsic = Log->GetEntries().FindByPredicate([Ranger](const FCombatLogEntry& Entry)
	{
		return Entry.TargetActorId == static_cast<int32>(Ranger->GetUniqueID()) && Entry.EventType == CombatTags::Event_Combat_ModifierApplied;
	});
	if (TestNotNull(TEXT("Birth intrinsic effect recorded"), Intrinsic))
	{
		TestTrue(TEXT("Intrinsic source name survives initialization ordering"), Intrinsic->SourceDefinitionId.IsValid());
		TestEqual(TEXT("Intrinsic target snapshots the real unit definition"), Intrinsic->TargetDefinitionId, Ranger->GetUnitDefinitionId());
	}
	Log->DestroyComponent();
	TestFalse(TEXT("Component EndPlay removes presentation delegate"), World->GetSubsystem<UCombatEventSubsystem>()->OnPresentationRecord().IsBoundToObject(Log));
	return true;
}
#endif
