#include "Combat/Editor/CombatMinimapEditor.h"

#if WITH_EDITOR

#include "Combat.h"
#include "Combat/UI/CombatMinimapWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace CombatMinimapEditorPrivate
{
	bool bMenusRegistered = false;
	FTSTicker::FDelegateHandle MenuRegistrationTicker;
	FDelegateHandle ModulesChangedHandle;

	struct FGeometryCandidate
	{
		FVector2D WorldMin = FVector2D::ZeroVector;
		FVector2D WorldMax = FVector2D::ZeroVector;
		float MinZ = 0.0f;
		float MaxZ = 0.0f;
		FString Label;
		float Area = 0.0f;
		bool bNamedSurface = false;
	};

	const FLinearColor BoundaryFill(0.06f, 0.08f, 0.12f, 0.96f);
	const FLinearColor BoundaryOutline(0.34f, 0.48f, 0.58f, 0.92f);
	const FLinearColor BlockerFill(0.08f, 0.19f, 0.25f, 0.94f);
	const FLinearColor BlockerOutline(0.22f, 0.64f, 0.72f, 0.88f);
	const FLinearColor LandmarkFill(0.34f, 0.20f, 0.08f, 0.96f);
	const FLinearColor LandmarkOutline(1.0f, 0.68f, 0.18f, 0.94f);

	FString ActorLabel(const AActor* Actor)
	{
		if (!Actor) return FString();
		const FString Label = Actor->GetActorLabel();
		return Label.IsEmpty() ? Actor->GetName() : Label;
	}

	bool ContainsAny(const FString& Value, const TArray<FString>& Needles)
	{
		for (const FString& Needle : Needles)
		{
			if (Value.Contains(Needle, ESearchCase::IgnoreCase)) return true;
		}
		return false;
	}

	bool IsNamedSurface(const FString& Label)
	{
		return ContainsAny(Label, { TEXT("floor"), TEXT("ground"), TEXT("terrain"), TEXT("landscape") });
	}

	bool IsNamedLandmark(const FString& Label)
	{
		return ContainsAny(Label, { TEXT("center"), TEXT("spawn"), TEXT("supply"), TEXT("platform"), TEXT("objective"), TEXT("landmark") });
	}

	void SetMessage(FString* OutMessage, const FString& Message)
	{
		if (OutMessage) *OutMessage = Message;
	}

	void Notify(const bool bSuccess, const FString& Message)
	{
		FNotificationInfo Info(FText::FromString(Message));
		Info.ExpireDuration = bSuccess ? 4.0f : 8.0f;
		Info.bUseThrobber = false;
		Info.bFireAndForget = true;
		Info.Image = FAppStyle::GetBrush(bSuccess ? TEXT("Icons.SuccessWithColor") : TEXT("Icons.ErrorWithColor"));
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	bool CollectCandidates(UWorld* World, TArray<FGeometryCandidate>& OutCandidates, FString* OutMessage)
	{
		OutCandidates.Reset();
		if (!World)
		{
			SetMessage(OutMessage, TEXT("没有可生成小地图的编辑器关卡"));
			return false;
		}
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			AStaticMeshActor* Actor = *It;
			if (!Actor || Actor->IsTemplate() || Actor->IsHiddenEd()) continue;
			TInlineComponentArray<UStaticMeshComponent*> Components;
			Actor->GetComponents(Components);
			const FString Label = ActorLabel(Actor);
			for (UStaticMeshComponent* Component : Components)
			{
				// Mobility 只描述运行时是否允许移动；编辑器生成要覆盖可见的地面和场景网格，
				// 因此不能因为原型地面是 Movable 就把它排除在地图范围之外。
				if (!Component || !Component->GetStaticMesh()) continue;
				const FBox Bounds = Component->Bounds.GetBox();
				if (!Bounds.IsValid || !FMath::IsFinite(Bounds.Min.X) || !FMath::IsFinite(Bounds.Min.Y)
					|| !FMath::IsFinite(Bounds.Max.X) || !FMath::IsFinite(Bounds.Max.Y)
					|| !FMath::IsFinite(Bounds.Min.Z) || !FMath::IsFinite(Bounds.Max.Z)) continue;
				const float Width = Bounds.Max.X - Bounds.Min.X;
				const float Depth = Bounds.Max.Y - Bounds.Min.Y;
				const float Area = Width * Depth;
				if (Width <= 1.0f || Depth <= 1.0f || Area <= 1.0f) continue;
				FGeometryCandidate Candidate;
				Candidate.WorldMin = FVector2D(Bounds.Min.X, Bounds.Min.Y);
				Candidate.WorldMax = FVector2D(Bounds.Max.X, Bounds.Max.Y);
				Candidate.MinZ = Bounds.Min.Z;
				Candidate.MaxZ = Bounds.Max.Z;
				Candidate.Label = Label;
				Candidate.Area = Area;
				Candidate.bNamedSurface = IsNamedSurface(Label);
				OutCandidates.Add(MoveTemp(Candidate));
			}
		}
		if (OutCandidates.Num() == 0)
		{
			SetMessage(OutMessage, TEXT("当前关卡没有可用的静态网格几何"));
			return false;
		}
		return true;
	}

	bool BuildMap(const FString& MapPackage, const TArray<FGeometryCandidate>& Candidates,
		FCombatMinimapMap& OutMap, FString* OutMessage)
	{
		int32 SurfaceIndex = INDEX_NONE;
		float SurfaceArea = 0.0f;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const FGeometryCandidate& Candidate = Candidates[Index];
			const bool bHorizontal = Candidate.MaxZ - Candidate.MinZ <= 250.0f;
			if ((Candidate.bNamedSurface && bHorizontal && Candidate.Area > SurfaceArea)
				|| (SurfaceIndex == INDEX_NONE && bHorizontal && Candidate.Area > SurfaceArea))
			{
				SurfaceIndex = Index;
				SurfaceArea = Candidate.Area;
			}
		}
		if (SurfaceIndex == INDEX_NONE)
		{
			for (int32 Index = 0; Index < Candidates.Num(); ++Index)
			{
				if (Candidates[Index].Area > SurfaceArea)
				{
					SurfaceIndex = Index;
					SurfaceArea = Candidates[Index].Area;
				}
			}
		}
		if (!Candidates.IsValidIndex(SurfaceIndex))
		{
			SetMessage(OutMessage, TEXT("无法从静态几何确定地图范围"));
			return false;
		}

		const FGeometryCandidate& Surface = Candidates[SurfaceIndex];
		OutMap = FCombatMinimapMap();
		OutMap.MapPackage = FName(*MapPackage);
		OutMap.WorldMin = Surface.WorldMin;
		OutMap.WorldMax = Surface.WorldMax;
		OutMap.GroundZ = Surface.MaxZ;
		const FVector2D Range = OutMap.WorldMax - OutMap.WorldMin;
		const float MapArea = Range.X * Range.Y;
		if (!OutMap.IsValid() || MapArea <= 1.0f)
		{
			SetMessage(OutMessage, TEXT("地图范围无效或面积过小"));
			return false;
		}

		const float MinimumFeatureArea = FMath::Max(1600.0f, MapArea * 0.00008f);
		const float EdgeTolerance = FMath::Max(50.0f, FMath::Min(Range.X, Range.Y) * 0.075f);
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			if (Index == SurfaceIndex) continue;
			const FGeometryCandidate& Candidate = Candidates[Index];
			if (Candidate.Area < MinimumFeatureArea || Candidate.Area >= MapArea * 0.90f) continue;
			const FVector2D Min(FMath::Max(Candidate.WorldMin.X, OutMap.WorldMin.X), FMath::Max(Candidate.WorldMin.Y, OutMap.WorldMin.Y));
			const FVector2D Max(FMath::Min(Candidate.WorldMax.X, OutMap.WorldMax.X), FMath::Min(Candidate.WorldMax.Y, OutMap.WorldMax.Y));
			if (Min.X >= Max.X || Min.Y >= Max.Y) continue;
			const bool bNearEdge = Min.X <= OutMap.WorldMin.X + EdgeTolerance || Min.Y <= OutMap.WorldMin.Y + EdgeTolerance
				|| Max.X >= OutMap.WorldMax.X - EdgeTolerance || Max.Y >= OutMap.WorldMax.Y - EdgeTolerance;
			const bool bLandmark = IsNamedLandmark(Candidate.Label);
			FCombatMinimapTerrainFeature Feature;
			Feature.WorldMin = Min;
			Feature.WorldMax = Max;
			Feature.Type = bNearEdge ? ECombatMinimapTerrainType::Boundary
				: (bLandmark ? ECombatMinimapTerrainType::Landmark : ECombatMinimapTerrainType::Blocker);
			Feature.FillColor = Feature.Type == ECombatMinimapTerrainType::Boundary ? BoundaryFill
				: (Feature.Type == ECombatMinimapTerrainType::Landmark ? LandmarkFill : BlockerFill);
			Feature.OutlineColor = Feature.Type == ECombatMinimapTerrainType::Boundary ? BoundaryOutline
				: (Feature.Type == ECombatMinimapTerrainType::Landmark ? LandmarkOutline : BlockerOutline);
			// 文字已经从小地图契约移除；Actor Label 只参与类别判断，不写入表现配置。
			Feature.Label = FText::GetEmpty();
			OutMap.TerrainFeatures.Add(MoveTemp(Feature));
		}

		if (OutMap.TerrainFeatures.Num() == 0)
		{
			const float Border = FMath::Max(20.0f, FMath::Min(Range.X, Range.Y) * 0.02f);
			const auto AddBorder = [&OutMap](const FVector2D& Min, const FVector2D& Max)
			{
				FCombatMinimapTerrainFeature Feature;
				Feature.WorldMin = Min;
				Feature.WorldMax = Max;
				Feature.Type = ECombatMinimapTerrainType::Boundary;
				Feature.FillColor = BoundaryFill;
				Feature.OutlineColor = BoundaryOutline;
				OutMap.TerrainFeatures.Add(MoveTemp(Feature));
			};
			AddBorder(OutMap.WorldMin, FVector2D(OutMap.WorldMax.X, OutMap.WorldMin.Y + Border));
			AddBorder(FVector2D(OutMap.WorldMin.X, OutMap.WorldMax.Y - Border), OutMap.WorldMax);
			AddBorder(FVector2D(OutMap.WorldMin.X, OutMap.WorldMin.Y + Border), FVector2D(OutMap.WorldMin.X + Border, OutMap.WorldMax.Y - Border));
			AddBorder(FVector2D(OutMap.WorldMax.X - Border, OutMap.WorldMin.Y + Border), FVector2D(OutMap.WorldMax.X, OutMap.WorldMax.Y - Border));
		}

		OutMap.TerrainFeatures.Sort([](const FCombatMinimapTerrainFeature& A, const FCombatMinimapTerrainFeature& B)
		{
			if (!FMath::IsNearlyEqual(A.WorldMin.X, B.WorldMin.X)) return A.WorldMin.X < B.WorldMin.X;
			if (!FMath::IsNearlyEqual(A.WorldMin.Y, B.WorldMin.Y)) return A.WorldMin.Y < B.WorldMin.Y;
			if (!FMath::IsNearlyEqual(A.WorldMax.X, B.WorldMax.X)) return A.WorldMax.X < B.WorldMax.X;
			return A.WorldMax.Y < B.WorldMax.Y;
		});
		return true;
	}

	void MergeMap(TArray<FCombatMinimapMap>& Maps, FCombatMinimapMap&& Map)
	{
		for (FCombatMinimapMap& Existing : Maps)
		{
			if (Existing.MapPackage == Map.MapPackage)
			{
				Existing = MoveTemp(Map);
				return;
			}
		}
		Maps.Add(MoveTemp(Map));
	}

	void ExecuteGenerateCommand()
	{
		FString Message;
		const bool bSuccess = CombatMinimapEditor::GenerateCurrentMap(&Message);
		if (bSuccess)
		{
			UE_LOG(LogCombatGame, Display, TEXT("CombatMinimapGenerate Success=1 %s"), *Message);
		}
		else
		{
			UE_LOG(LogCombatGame, Warning, TEXT("CombatMinimapGenerate Success=0 %s"), *Message);
		}
	}

	void OnGenerateMenuEntry()
	{
		FString Message;
		const bool bSuccess = CombatMinimapEditor::GenerateCurrentMap(&Message);
		Notify(bSuccess, Message);
	}

	void RegisterMenus()
	{
		if (bMenusRegistered) return;
		UToolMenus* ToolMenus = UToolMenus::Get();
		if (!ToolMenus) return;
		FToolMenuOwnerScoped OwnerScoped(TEXT("CombatMinimapEditor"));
		bool bAdded = false;
		if (UToolMenu* Menu = ToolMenus->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")))
		{
			FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("CombatMinimap"), FText::FromString(TEXT("Combat 小地图")));
			Section.AddEntry(FToolMenuEntry::InitMenuEntry(
				TEXT("CombatMinimapGenerate"),
				FText::FromString(TEXT("保存并生成小地图")),
				FText::FromString(TEXT("先保存当前关卡，再按静态网格 bounds 生成对应的小地图配置。")),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Toolbar.Save")),
				FUIAction(FExecuteAction::CreateStatic(&OnGenerateMenuEntry))));
			bAdded = true;
		}
		if (UToolMenu* Toolbar = ToolMenus->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.User")))
		{
			FToolMenuSection& Section = Toolbar->FindOrAddSection(TEXT("CombatMinimap"));
			Section.AddEntry(FToolMenuEntry::InitToolBarButton(
				TEXT("CombatMinimapGenerateToolbar"),
				FUIAction(FExecuteAction::CreateStatic(&OnGenerateMenuEntry)),
				FText::FromString(TEXT("生成小地图")),
				FText::FromString(TEXT("保存当前关卡并生成小地图配置。")),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Toolbar.Save"))));
			bAdded = true;
		}
		if (bAdded)
		{
			bMenusRegistered = true;
			if (MenuRegistrationTicker.IsValid())
				FTSTicker::GetCoreTicker().RemoveTicker(MenuRegistrationTicker);
			UE_LOG(LogCombatGame, Display, TEXT("CombatMinimap editor menu registered"));
		}
	}

	bool RetryRegisterMenus(float)
	{
		RegisterMenus();
		return !bMenusRegistered;
	}

	void OnModulesChanged(FName ModuleName, EModuleChangeReason)
	{
		if (ModuleName == TEXT("LevelEditor"))
		{
			UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterMenus));
			RegisterMenus();
		}
	}

	void OnPostEngineInit()
	{
		RegisterMenus();
	}

	void OnEditorInitialized(double)
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterMenus));
	}

	static FAutoConsoleCommand GenerateCommand(
		TEXT("combat.Editor.GenerateMinimap"),
		TEXT("保存当前编辑器关卡并生成 Config/CombatMinimap.generated.json。"),
		FConsoleCommandDelegate::CreateStatic(&ExecuteGenerateCommand));
}

namespace CombatMinimapEditor
{
	void Startup()
	{
		UE_LOG(LogCombatGame, Display, TEXT("CombatMinimap editor startup"));
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&CombatMinimapEditorPrivate::RegisterMenus));
		CombatMinimapEditorPrivate::ModulesChangedHandle = FModuleManager::Get().OnModulesChanged().AddStatic(&CombatMinimapEditorPrivate::OnModulesChanged);
		FCoreDelegates::GetOnPostEngineInit().AddStatic(&CombatMinimapEditorPrivate::OnPostEngineInit);
		CombatMinimapEditorPrivate::MenuRegistrationTicker = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateStatic(&CombatMinimapEditorPrivate::RetryRegisterMenus), 0.0f);
		if (GEditor)
		{
			CombatMinimapEditorPrivate::RegisterMenus();
		}
		FEditorDelegates::OnEditorInitialized.AddStatic(&CombatMinimapEditorPrivate::OnEditorInitialized);
	}

	void Shutdown()
	{
		UE_LOG(LogCombatGame, Display, TEXT("CombatMinimap editor shutdown"));
		if (CombatMinimapEditorPrivate::MenuRegistrationTicker.IsValid())
			FTSTicker::GetCoreTicker().RemoveTicker(CombatMinimapEditorPrivate::MenuRegistrationTicker);
		if (CombatMinimapEditorPrivate::ModulesChangedHandle.IsValid())
			FModuleManager::Get().OnModulesChanged().Remove(CombatMinimapEditorPrivate::ModulesChangedHandle);
		FCoreDelegates::GetOnPostEngineInit().RemoveAll(&CombatMinimapEditorPrivate::OnPostEngineInit);
		CombatMinimapEditorPrivate::bMenusRegistered = false;
		FEditorDelegates::OnEditorInitialized.RemoveAll(&CombatMinimapEditorPrivate::OnEditorInitialized);
		if (UToolMenus::TryGet())
		{
			UToolMenus::Get()->UnregisterOwnerByName(TEXT("CombatMinimapEditor"));
		}
	}

	bool GenerateCurrentMap(FString* OutMessage)
	{
		if (!GEditor)
		{
			CombatMinimapEditorPrivate::SetMessage(OutMessage, TEXT("编辑器实例不可用"));
			return false;
		}
		if (GEditor->PlayWorld)
		{
			CombatMinimapEditorPrivate::SetMessage(OutMessage, TEXT("请先停止 PIE，再生成小地图"));
			return false;
		}
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (!World || !World->PersistentLevel)
		{
			CombatMinimapEditorPrivate::SetMessage(OutMessage, TEXT("当前没有可保存的编辑器关卡"));
			return false;
		}
		const FString MapPackage = World->PersistentLevel->GetOutermost()->GetName();
		if (MapPackage.IsEmpty() || MapPackage.StartsWith(TEXT("/Engine/Transient")))
		{
			CombatMinimapEditorPrivate::SetMessage(OutMessage, TEXT("当前关卡不是可保存的项目地图"));
			return false;
		}
		if (!UEditorLoadingAndSavingUtils::SaveMap(World, MapPackage)
			|| !FEditorFileUtils::SaveMapDataPackages(World, true))
		{
			CombatMinimapEditorPrivate::SetMessage(OutMessage, FString::Printf(TEXT("保存地图失败：%s"), *MapPackage));
			return false;
		}

		TArray<CombatMinimapEditorPrivate::FGeometryCandidate> Candidates;
		FString BuildError;
		if (!CombatMinimapEditorPrivate::CollectCandidates(World, Candidates, &BuildError))
		{
			CombatMinimapEditorPrivate::SetMessage(OutMessage, BuildError);
			return false;
		}
		FCombatMinimapMap Map;
		if (!CombatMinimapEditorPrivate::BuildMap(MapPackage, Candidates, Map, &BuildError))
		{
			CombatMinimapEditorPrivate::SetMessage(OutMessage, BuildError);
			return false;
		}
		const int32 GeneratedFeatureCount = Map.TerrainFeatures.Num();

		TArray<FCombatMinimapMap> GeneratedMaps;
		FString LoadError;
		CombatMinimapConfig::Load(GeneratedMaps, &LoadError);
		CombatMinimapEditorPrivate::MergeMap(GeneratedMaps, MoveTemp(Map));
		FString SaveError;
		if (!CombatMinimapConfig::Save(GeneratedMaps, &SaveError))
		{
			CombatMinimapEditorPrivate::SetMessage(OutMessage, SaveError);
			return false;
		}
		CombatMinimapEditorPrivate::SetMessage(OutMessage, FString::Printf(TEXT("已生成 %d 个静态特征：%s"), GeneratedFeatureCount, *CombatMinimapConfig::GetGeneratedConfigPath()));
		return true;
	}
}

#endif
