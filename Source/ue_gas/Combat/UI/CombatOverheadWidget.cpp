#include "Combat/UI/CombatOverheadWidget.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Targeting/CombatTeamSubsystem.h"
#include "Combat/UI/CombatFloatingTextWidget.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "ue_gasPlayerController.h"

void UCombatOverheadWidget::InitializeForUnit(ACombatUnitCharacter* InUnit)
{
	if (IsDesignTime())
	{
		return;
	}
	if (BoundUnit.Get() == InUnit && BoundView.IsValid())
	{
		HandleViewChanged();
		return;
	}
	UnbindView();
	ResetPresentation();
	BoundUnit = InUnit;
	BoundView = InUnit ? InUnit->GetCombatUnitViewComponent() : nullptr;
	if (BoundView.IsValid())
	{
		BoundView->OnUnitViewChanged.AddUniqueDynamic(this, &ThisClass::HandleViewChanged);
		BoundView->OnModifierViewsChanged.AddUniqueDynamic(this, &ThisClass::HandleViewChanged);
	}
	HandleViewChanged();
}

void UCombatOverheadWidget::UnbindView()
{
	++BindingRevision;
	if (BoundView.IsValid())
	{
		BoundView->OnUnitViewChanged.RemoveDynamic(this, &ThisClass::HandleViewChanged);
		BoundView->OnModifierViewsChanged.RemoveDynamic(this, &ThisClass::HandleViewChanged);
	}
	BoundView.Reset();
	if (DefinitionLoadHandle)
	{
		DefinitionLoadHandle->CancelHandle();
		DefinitionLoadHandle.Reset();
	}
	RequestedUnitId = FPrimaryAssetId();
	RequestedAbilityId = FPrimaryAssetId();
}

void UCombatOverheadWidget::ResetPresentation()
{
	// 重生后即使定义 ID 相同，也必须重新发起名称请求，避免旧生命的回调被丢弃后永久保留占位名。
	++BindingRevision;
	if (DefinitionLoadHandle)
	{
		DefinitionLoadHandle->CancelHandle();
		DefinitionLoadHandle.Reset();
	}
	RequestedUnitId = FPrimaryAssetId();
	RequestedAbilityId = FPrimaryAssetId();
	for (UCombatFloatingTextWidget* Widget : FloatingWidgets)
	{
		if (Widget)
		{
			Widget->RemoveFromParent();
		}
	}
	FloatingWidgets.Reset();
	FloatingSequence = 0;
	DisplayData = FCombatOverheadDisplayData();
	if (bReadyForEvents)
	{
		OnPresentationReset();
	}
}

void UCombatOverheadWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (IsDesignTime())
	{
		OnDisplayDataChanged(PreviewData);
		OnProgressChanged(ComputeProgress(PreviewData, 0.0));
	}
}

void UCombatOverheadWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bReadyForEvents = true;
	InitializeForUnit(BoundUnit.Get());
}

void UCombatOverheadWidget::NativeDestruct()
{
	ResetPresentation();
	bReadyForEvents = false;
	UnbindView();
	// 保留 Unit 弱引用以支持同一 Widget 的 Slate 重建；组件 EndPlay 会显式置空。
	Super::NativeDestruct();
}

void UCombatOverheadWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bReadyForEvents || IsDesignTime())
	{
		return;
	}
	if (!BoundUnit.IsValid() || !BoundView.IsValid())
	{
		if (DisplayData.bReady)
		{
			InitializeForUnit(nullptr);
		}
		return;
	}
	// 观察者的主控 Unit/队伍可能晚于目标 View 到达；只在关系改变时重新推送快照。
	if (DisplayData.Relation != ResolveViewerRelation())
	{
		HandleViewChanged();
	}
	if (DisplayData.bShowStatus || DisplayData.bShowAbility)
	{
		OnProgressChanged(ComputeProgress(DisplayData, BoundView->GetEstimatedServerTimeSeconds()));
	}
}

void UCombatOverheadWidget::HandleViewChanged()
{
	if (!BoundView.IsValid())
	{
		DisplayData = FCombatOverheadDisplayData();
		if (bReadyForEvents)
		{
			OnDisplayDataChanged(DisplayData);
			OnProgressChanged(FCombatOverheadProgressData());
		}
		return;
	}
	const FCombatUnitView& View = BoundView->GetUnitView();
	if (DisplayData.LifeGeneration != View.LifeGeneration)
	{
		ResetPresentation();
	}
	DisplayData = BuildDisplayData(View, BoundView->GetVisibleModifiers(), ControlRules,
		ResolveViewerRelation(), ResolveDefinitionName(View.UnitDefinitionId),
		ResolveDefinitionName(View.ActiveAbilityDefinitionId));
	if (bReadyForEvents)
	{
		OnDisplayDataChanged(DisplayData);
		OnProgressChanged(ComputeProgress(DisplayData, BoundView->GetEstimatedServerTimeSeconds()));
	}
	RequestDefinitionNames(View);
}

FCombatOverheadDisplayData UCombatOverheadWidget::BuildDisplayData(
	const FCombatUnitView& View, const TArray<FCombatModifierView>& Modifiers,
	const TArray<FCombatControlPresentationRule>& Rules, const ECombatTeamRelation Relation,
	const FText& UnitName, const FText& AbilityName)
{
	FCombatOverheadDisplayData Data;
	Data.bReady = View.UnitDefinitionId.IsValid() && View.LifeGeneration > 0;
	Data.LifeGeneration = View.LifeGeneration;
	Data.UnitName = UnitName;
	Data.AbilityName = AbilityName;
	Data.Relation = Relation;
	Data.bShowInfo = Data.bReady && View.LifeState == ECombatLifeState::Alive
		&& !View.VisibleStatusTags.HasTagExact(CombatTags::State_NoHealthBar);
	Data.Health = FMath::IsFinite(View.Health) ? FMath::Max(0.0f, View.Health) : 0.0f;
	Data.MaxHealth = FMath::IsFinite(View.MaxHealth) ? FMath::Max(0.0f, View.MaxHealth) : 0.0f;
	Data.HealthPercent = Data.MaxHealth > KINDA_SMALL_NUMBER
		? FMath::Clamp(Data.Health / Data.MaxHealth, 0.0f, 1.0f) : 0.0f;
	Data.bShowMana = FMath::IsFinite(View.MaxMana) && View.MaxMana > KINDA_SMALL_NUMBER;
	Data.ManaPercent = Data.bShowMana && FMath::IsFinite(View.Mana)
		? FMath::Clamp(View.Mana / View.MaxMana, 0.0f, 1.0f) : 0.0f;

	TArray<const FCombatControlPresentationRule*> VisibleRules;
	for (const FCombatControlPresentationRule& Rule : Rules)
	{
		if (Rule.Tag.IsValid() && View.VisibleStatusTags.HasTagExact(Rule.Tag)
			&& !VisibleRules.ContainsByPredicate([&Rule](const FCombatControlPresentationRule* Existing)
				{ return Existing->Tag == Rule.Tag; }))
		{
			VisibleRules.Add(&Rule);
		}
	}
	VisibleRules.Sort([](const FCombatControlPresentationRule& A, const FCombatControlPresentationRule& B)
	{
		return A.Priority != B.Priority ? A.Priority > B.Priority : A.Tag.ToString() < B.Tag.ToString();
	});
	if (!VisibleRules.IsEmpty())
	{
		Data.bShowStatus = true;
		Data.PrimaryStatusTag = VisibleRules[0]->Tag;
		Data.StatusColor = VisibleRules[0]->Color;
		for (const FCombatControlPresentationRule* Rule : VisibleRules)
		{
			Data.StatusLabel = Data.StatusLabel.IsEmpty() ? Rule->Label
				: FText::Format(NSLOCTEXT("CombatUI", "StatusJoin", "{0} · {1}"), Data.StatusLabel, Rule->Label);
		}
		// 任一无限来源支配有限来源，结果不受 FastArray 条目顺序影响。
		for (const FCombatModifierView& Modifier : Modifiers)
		{
			if (!Modifier.ControlTags.HasTagExact(Data.PrimaryStatusTag))
			{
				continue;
			}
			if (Modifier.ServerEndTime <= 0.0)
			{
				Data.StatusStartTime = 0.0;
				Data.StatusEndTime = 0.0;
				break;
			}
			if (FMath::IsFinite(Modifier.ServerStartTime) && FMath::IsFinite(Modifier.ServerEndTime)
				&& Modifier.ServerEndTime > Modifier.ServerStartTime
				&& Modifier.ServerEndTime > Data.StatusEndTime)
			{
				Data.StatusStartTime = Modifier.ServerStartTime;
				Data.StatusEndTime = Modifier.ServerEndTime;
			}
		}
	}
	// EventId 的序号仅供服务器匹配开始/结束，客户端投影不包含该内部字段；
	// 展示就绪只依赖实际复制的定义、阶段和时间窗，避免客户端永远隐藏施法条。
	Data.bShowAbility = View.ActiveAbilityDefinitionId.IsValid()
		&& View.AbilityPhase != ECombatAbilityViewPhase::None
		&& FMath::IsFinite(View.AbilityServerStartTime) && FMath::IsFinite(View.AbilityServerEndTime)
		&& View.AbilityServerEndTime > View.AbilityServerStartTime;
	Data.bIsChanneling = View.AbilityPhase == ECombatAbilityViewPhase::Channeling;
	Data.AbilityStartTime = View.AbilityServerStartTime;
	Data.AbilityEndTime = View.AbilityServerEndTime;
	return Data;
}

FCombatOverheadProgressData UCombatOverheadWidget::ComputeProgress(
	const FCombatOverheadDisplayData& Data, const double ServerTime)
{
	FCombatOverheadProgressData Progress;
	const double Now = FMath::IsFinite(ServerTime) ? ServerTime : 0.0;
	if (Data.bShowStatus && Data.StatusEndTime > Data.StatusStartTime)
	{
		Progress.StatusRemaining = static_cast<float>(FMath::Max(0.0, Data.StatusEndTime - Now));
		Progress.StatusPercent = FMath::Clamp(
			static_cast<float>((Data.StatusEndTime - Now) / (Data.StatusEndTime - Data.StatusStartTime)), 0.0f, 1.0f);
	}
	if (Data.bShowAbility && Data.AbilityEndTime > Data.AbilityStartTime)
	{
		Progress.AbilityRemaining = static_cast<float>(FMath::Max(0.0, Data.AbilityEndTime - Now));
		Progress.AbilityPercent = FMath::Clamp(
			static_cast<float>((Data.AbilityEndTime - Now) / (Data.AbilityEndTime - Data.AbilityStartTime)), 0.0f, 1.0f);
		Progress.bShowAbility = Progress.AbilityRemaining > 0.0f;
	}
	return Progress;
}

ECombatTeamRelation UCombatOverheadWidget::ResolveViewerRelation() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Player = GetOwningPlayer();
	if (!Player && World)
	{
		Player = World->GetFirstPlayerController();
	}
	const Aue_gasPlayerController* Commander = Cast<Aue_gasPlayerController>(Player);
	const ACombatUnitCharacter* Observer = Commander && Commander->IsLocalController()
		? Commander->GetCommandedUnit() : nullptr;
	const UCombatUnitViewComponent* ObserverView = Observer ? Observer->GetCombatUnitViewComponent() : nullptr;
	const UCombatTeamSubsystem* Teams = World ? World->GetSubsystem<UCombatTeamSubsystem>() : nullptr;
	return ObserverView && BoundView.IsValid() && Teams
		? Teams->GetRelation(ObserverView->GetUnitView().TeamId, BoundView->GetUnitView().TeamId)
		: ECombatTeamRelation::Invalid;
}

FText UCombatOverheadWidget::ResolveDefinitionName(const FPrimaryAssetId& DefinitionId)
{
	if (const UCombatDefinitionData* Definition = Cast<UCombatDefinitionData>(
		UAssetManager::Get().GetPrimaryAssetObject(DefinitionId)))
	{
		if (!Definition->DisplayNameText.IsEmpty())
		{
			return Definition->DisplayNameText;
		}
	}
	return DefinitionId.IsValid() ? FText::FromName(DefinitionId.PrimaryAssetName)
		: NSLOCTEXT("CombatUI", "UnknownDefinition", "未知");
}

void UCombatOverheadWidget::RequestDefinitionNames(const FCombatUnitView& View)
{
	if (RequestedUnitId == View.UnitDefinitionId && RequestedAbilityId == View.ActiveAbilityDefinitionId)
	{
		return;
	}
	++BindingRevision;
	if (DefinitionLoadHandle)
	{
		DefinitionLoadHandle->CancelHandle();
		DefinitionLoadHandle.Reset();
	}
	RequestedUnitId = View.UnitDefinitionId;
	RequestedAbilityId = View.ActiveAbilityDefinitionId;
	TArray<FSoftObjectPath> Paths;
	UAssetManager& Assets = UAssetManager::Get();
	for (const FPrimaryAssetId& Id : { RequestedUnitId, RequestedAbilityId })
	{
		const FSoftObjectPath Path = Assets.GetPrimaryAssetPath(Id);
		if (Path.IsValid() && !Assets.GetPrimaryAssetObject(Id))
		{
			Paths.AddUnique(Path);
		}
	}
	if (Paths.IsEmpty())
	{
		return;
	}
	const TWeakObjectPtr<UCombatOverheadWidget> WeakThis(this);
	const uint64 Revision = BindingRevision;
	const int64 Life = View.LifeGeneration;
	DefinitionLoadHandle = Assets.GetStreamableManager().RequestAsyncLoad(Paths,
		FStreamableDelegate::CreateLambda([WeakThis, Revision, Life]()
		{
			UCombatOverheadWidget* Widget = WeakThis.Get();
			if (Widget && Widget->BindingRevision == Revision && Widget->BoundView.IsValid()
				&& Widget->BoundView->GetUnitView().LifeGeneration == Life)
			{
				Widget->HandleViewChanged();
			}
		}));
}

void UCombatOverheadWidget::AddFloatingText(
	const float Amount, const ECombatFloatingTextType Type, const int64 LifeGeneration)
{
	if (!bReadyForEvents || !BoundView.IsValid() || !FMath::IsFinite(Amount)
		|| Amount <= KINDA_SMALL_NUMBER || LifeGeneration <= 0
		|| LifeGeneration != BoundView->GetUnitView().LifeGeneration)
	{
		return;
	}
	FCombatFloatingTextPayload Data;
	Data.Amount = Amount;
	Data.Type = Type;
	Data.LifeGeneration = LifeGeneration;
	Data.Sequence = FloatingSequence;
	FloatingSequence = (FloatingSequence + 1) % 10000;
	OnFloatingTextRequested(Data);
}

UCombatFloatingTextWidget* UCombatOverheadWidget::CreateFloatingText(const FCombatFloatingTextPayload& Data)
{
	if (!FloatingTextWidgetClass || !BoundView.IsValid() || !FMath::IsFinite(Data.Amount)
		|| Data.Amount <= KINDA_SMALL_NUMBER || Data.LifeGeneration <= 0
		|| Data.LifeGeneration != BoundView->GetUnitView().LifeGeneration
		|| !GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	FloatingWidgets.RemoveAll([](const UCombatFloatingTextWidget* Widget)
	{
		return !IsValid(Widget) || !Widget->GetParent();
	});
	while (FloatingWidgets.Num() >= 12)
	{
		FloatingWidgets[0]->RemoveFromParent();
		FloatingWidgets.RemoveAt(0);
	}
	UCombatFloatingTextWidget* Widget = CreateWidget<UCombatFloatingTextWidget>(this, FloatingTextWidgetClass);
	if (Widget)
	{
		Widget->InitializeFloatingText(Data);
		FloatingWidgets.Add(Widget);
	}
	return Widget;
}
