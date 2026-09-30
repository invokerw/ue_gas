#include "Combat/UI/CombatMinimapWidget.h"

#include "Combat.h"
#include "Combat/UI/CombatAbilityIndicatorGround.h"
#include "Combat/Targeting/CombatTeamSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "CombatCharacter.h"
#include "CombatPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Rendering/DrawElements.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/SlateBrush.h"

namespace CombatMinimapPrivate
{
	constexpr float Border = 5.0f;
	constexpr float MapGap = 12.0f;
	constexpr float MarkerRadius = 5.0f;
	constexpr float ViewDragThreshold = 6.0f;

	FString PackageName(const UWorld* World)
	{
		if (!World || !World->PersistentLevel) return FString();
		// PIE 会给复制出来的 World 包加上 UEDPIE 前缀；先还原到保存的完整包路径，
		// 这样 Demo 与测试地图仍按完整路径精确区分，而不是靠当前关卡顺序猜测。
		return UWorld::RemovePIEPrefix(World->PersistentLevel->GetOutermost()->GetName());
	}

	bool Finite(const FVector2D& Value)
	{
		return !Value.ContainsNaN() && FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
	}

	bool GroundPoint(UWorld* World, const FVector2D& XY, const float GroundZ, FVector& OutPoint)
	{
		if (!World || !Finite(XY) || !FMath::IsFinite(GroundZ)) return false;
		const FVector Start(XY.X, XY.Y, GroundZ + 10000.0f);
		const FVector End(XY.X, XY.Y, GroundZ - 10000.0f);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatMinimapGround), false);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Start, End, CombatAbilityIndicatorGround::TraceChannel, Params)
			|| !CombatAbilityIndicatorGround::IsGroundHit(Hit)) return false;
		OutPoint = Hit.ImpactPoint;
		return !OutPoint.ContainsNaN();
	}

	void AddTerrainFeature(FCombatMinimapMap& Map, const FVector2D& WorldMin, const FVector2D& WorldMax,
		const ECombatMinimapTerrainType Type, const FLinearColor& Fill,
		const FLinearColor& Outline)
	{
		FCombatMinimapTerrainFeature Feature;
		Feature.WorldMin = WorldMin;
		Feature.WorldMax = WorldMax;
		Feature.Type = Type;
		Feature.FillColor = Fill;
		Feature.OutlineColor = Outline;
		Map.TerrainFeatures.Add(MoveTemp(Feature));
	}

	/**
	 * 允许用世界范围直接得到 UV 范围；与单点投影相比不会因为矩形的一角在边界上而丢弃整个特征。
	 */
	bool ProjectWorldBoundsToMap(const FCombatMinimapTerrainFeature& Feature, const FCombatMinimapMap& Map,
		FVector2D& OutMin, FVector2D& OutMax)
	{
		if (!Feature.IsValid() || !Map.IsValid()) return false;
		const FVector2D Range = Map.WorldMax - Map.WorldMin;
		const FVector2D A((Feature.WorldMin.Y - Map.WorldMin.Y) / Range.Y,
			1.0f - (Feature.WorldMax.X - Map.WorldMin.X) / Range.X);
		const FVector2D B((Feature.WorldMax.Y - Map.WorldMin.Y) / Range.Y,
			1.0f - (Feature.WorldMin.X - Map.WorldMin.X) / Range.X);
		OutMin = FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
		OutMax = FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
		return Finite(OutMin) && Finite(OutMax) && OutMax.X > OutMin.X && OutMax.Y > OutMin.Y;
	}
}

namespace CombatMinimapConfigPrivate
{
	void SetError(FString* OutError, const FString& Message)
	{
		if (OutError) *OutError = Message;
	}

	TArray<TSharedPtr<FJsonValue>> VectorArray(const FVector2D& Value)
	{
		return {
			MakeShared<FJsonValueNumber>(static_cast<double>(Value.X)),
			MakeShared<FJsonValueNumber>(static_cast<double>(Value.Y))
		};
	}

	TArray<TSharedPtr<FJsonValue>> ColorArray(const FLinearColor& Value)
	{
		return {
			MakeShared<FJsonValueNumber>(static_cast<double>(Value.R)),
			MakeShared<FJsonValueNumber>(static_cast<double>(Value.G)),
			MakeShared<FJsonValueNumber>(static_cast<double>(Value.B)),
			MakeShared<FJsonValueNumber>(static_cast<double>(Value.A))
		};
	}

	bool ReadVector2(const TSharedPtr<FJsonObject>& Object, const FStringView FieldName,
		FVector2D& OutValue, FString* OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values || Values->Num() != 2)
		{
			SetError(OutError, FString::Printf(TEXT("字段 %s 必须是两个数字的数组"), FieldName.GetData()));
			return false;
		}
		double X = 0.0, Y = 0.0;
		if (!(*Values)[0].IsValid() || !(*Values)[1].IsValid()
			|| !(*Values)[0]->TryGetNumber(X) || !(*Values)[1]->TryGetNumber(Y)
			|| !FMath::IsFinite(X) || !FMath::IsFinite(Y))
		{
			SetError(OutError, FString::Printf(TEXT("字段 %s 包含无效数字"), FieldName.GetData()));
			return false;
		}
		OutValue = FVector2D(static_cast<float>(X), static_cast<float>(Y));
		return true;
	}

	bool ReadColor(const TSharedPtr<FJsonObject>& Object, const FStringView FieldName,
		FLinearColor& OutValue, FString* OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values || Values->Num() != 4)
		{
			SetError(OutError, FString::Printf(TEXT("字段 %s 必须是四个数字的颜色数组"), FieldName.GetData()));
			return false;
		}
		double Channels[4] = {};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetNumber(Channels[Index])
				|| !FMath::IsFinite(Channels[Index]))
			{
				SetError(OutError, FString::Printf(TEXT("字段 %s 包含无效颜色"), FieldName.GetData()));
				return false;
			}
		}
		OutValue = FLinearColor(static_cast<float>(Channels[0]), static_cast<float>(Channels[1]),
			static_cast<float>(Channels[2]), static_cast<float>(Channels[3]));
		return true;
	}

	FString TypeToString(const ECombatMinimapTerrainType Type)
	{
		switch (Type)
		{
		case ECombatMinimapTerrainType::Boundary: return TEXT("boundary");
		case ECombatMinimapTerrainType::Landmark: return TEXT("landmark");
		default: return TEXT("blocker");
		}
	}

	bool StringToType(const FString& Value, ECombatMinimapTerrainType& OutType)
	{
		if (Value.Equals(TEXT("boundary"), ESearchCase::IgnoreCase)) OutType = ECombatMinimapTerrainType::Boundary;
		else if (Value.Equals(TEXT("landmark"), ESearchCase::IgnoreCase)) OutType = ECombatMinimapTerrainType::Landmark;
		else if (Value.Equals(TEXT("blocker"), ESearchCase::IgnoreCase)) OutType = ECombatMinimapTerrainType::Blocker;
		else return false;
		return true;
	}
}

namespace CombatMinimapConfig
{
	FString GetGeneratedConfigPath()
	{
		return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("CombatMinimap.generated.json"));
	}

	bool Serialize(const TArray<FCombatMinimapMap>& Maps, FString& OutJson, FString* OutError)
	{
		OutJson.Reset();
		if (Maps.Num() == 0)
		{
			CombatMinimapConfigPrivate::SetError(OutError, TEXT("生成配置至少需要一张地图"));
			return false;
		}
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("schemaVersion"), 1);
		Root->SetStringField(TEXT("generatedBy"), TEXT("CombatMinimapEditor"));
		Root->SetStringField(TEXT("generatedAtUtc"), FDateTime::UtcNow().ToIso8601());
		TArray<TSharedPtr<FJsonValue>> MapValues;
		MapValues.Reserve(Maps.Num());
		for (const FCombatMinimapMap& Map : Maps)
		{
			if (!Map.IsValid())
			{
				CombatMinimapConfigPrivate::SetError(OutError, TEXT("地图配置范围或包路径无效"));
				return false;
			}
			TSharedRef<FJsonObject> MapObject = MakeShared<FJsonObject>();
			MapObject->SetStringField(TEXT("mapPackage"), Map.MapPackage.ToString());
			MapObject->SetArrayField(TEXT("worldMin"), CombatMinimapConfigPrivate::VectorArray(Map.WorldMin));
			MapObject->SetArrayField(TEXT("worldMax"), CombatMinimapConfigPrivate::VectorArray(Map.WorldMax));
			MapObject->SetNumberField(TEXT("groundZ"), static_cast<double>(Map.GroundZ));
			TArray<TSharedPtr<FJsonValue>> FeatureValues;
			FeatureValues.Reserve(Map.TerrainFeatures.Num());
			for (const FCombatMinimapTerrainFeature& Feature : Map.TerrainFeatures)
			{
				if (!Feature.IsValid())
				{
					CombatMinimapConfigPrivate::SetError(OutError, TEXT("地形特征包含无效范围或颜色"));
					return false;
				}
				TSharedRef<FJsonObject> FeatureObject = MakeShared<FJsonObject>();
				FeatureObject->SetArrayField(TEXT("worldMin"), CombatMinimapConfigPrivate::VectorArray(Feature.WorldMin));
				FeatureObject->SetArrayField(TEXT("worldMax"), CombatMinimapConfigPrivate::VectorArray(Feature.WorldMax));
				FeatureObject->SetStringField(TEXT("type"), CombatMinimapConfigPrivate::TypeToString(Feature.Type));
				FeatureObject->SetArrayField(TEXT("fill"), CombatMinimapConfigPrivate::ColorArray(Feature.FillColor));
				FeatureObject->SetArrayField(TEXT("outline"), CombatMinimapConfigPrivate::ColorArray(Feature.OutlineColor));
				FeatureValues.Add(MakeShared<FJsonValueObject>(FeatureObject));
			}
			MapObject->SetArrayField(TEXT("features"), MoveTemp(FeatureValues));
			MapValues.Add(MakeShared<FJsonValueObject>(MapObject));
		}
		Root->SetArrayField(TEXT("maps"), MoveTemp(MapValues));
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
		if (!FJsonSerializer::Serialize(Root, Writer))
		{
			CombatMinimapConfigPrivate::SetError(OutError, TEXT("生成配置 JSON 序列化失败"));
			OutJson.Reset();
			return false;
		}
		return true;
	}

	bool Deserialize(const FString& Json, TArray<FCombatMinimapMap>& OutMaps, FString* OutError)
	{
		OutMaps.Reset();
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			CombatMinimapConfigPrivate::SetError(OutError, TEXT("生成配置不是有效 JSON"));
			return false;
		}
		int32 SchemaVersion = 0;
		if (!Root->TryGetNumberField(TEXT("schemaVersion"), SchemaVersion) || SchemaVersion != 1)
		{
			CombatMinimapConfigPrivate::SetError(OutError, TEXT("生成配置 schemaVersion 不受支持"));
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* MapValues = nullptr;
		if (!Root->TryGetArrayField(TEXT("maps"), MapValues) || !MapValues || MapValues->Num() == 0)
		{
			CombatMinimapConfigPrivate::SetError(OutError, TEXT("生成配置缺少 maps 数组"));
			return false;
		}
		for (const TSharedPtr<FJsonValue>& MapValue : *MapValues)
		{
			const TSharedPtr<FJsonObject> MapObject = MapValue.IsValid() ? MapValue->AsObject() : nullptr;
			if (!MapObject.IsValid())
			{
				CombatMinimapConfigPrivate::SetError(OutError, TEXT("地图条目不是对象"));
				OutMaps.Reset();
				return false;
			}
			FString Package;
			FCombatMinimapMap Map;
			if (!MapObject->TryGetStringField(TEXT("mapPackage"), Package) || Package.IsEmpty()
				|| !CombatMinimapConfigPrivate::ReadVector2(MapObject, TEXT("worldMin"), Map.WorldMin, OutError)
				|| !CombatMinimapConfigPrivate::ReadVector2(MapObject, TEXT("worldMax"), Map.WorldMax, OutError)
				|| !MapObject->TryGetNumberField(TEXT("groundZ"), Map.GroundZ))
			{
				CombatMinimapConfigPrivate::SetError(OutError, OutError && !OutError->IsEmpty() ? *OutError : TEXT("地图条目字段无效"));
				OutMaps.Reset();
				return false;
			}
			Map.MapPackage = FName(*Package);
			const TArray<TSharedPtr<FJsonValue>>* FeatureValues = nullptr;
			if (!MapObject->TryGetArrayField(TEXT("features"), FeatureValues) || !FeatureValues)
			{
				CombatMinimapConfigPrivate::SetError(OutError, TEXT("地图条目缺少 features 数组"));
				OutMaps.Reset();
				return false;
			}
			for (const TSharedPtr<FJsonValue>& FeatureValue : *FeatureValues)
			{
				const TSharedPtr<FJsonObject> FeatureObject = FeatureValue.IsValid() ? FeatureValue->AsObject() : nullptr;
				if (!FeatureObject.IsValid())
				{
					CombatMinimapConfigPrivate::SetError(OutError, TEXT("地形特征不是对象"));
					OutMaps.Reset();
					return false;
				}
				FCombatMinimapTerrainFeature Feature;
				FString Type;
				if (!FeatureObject->TryGetStringField(TEXT("type"), Type)
					|| !CombatMinimapConfigPrivate::StringToType(Type, Feature.Type)
					|| !CombatMinimapConfigPrivate::ReadVector2(FeatureObject, TEXT("worldMin"), Feature.WorldMin, OutError)
					|| !CombatMinimapConfigPrivate::ReadVector2(FeatureObject, TEXT("worldMax"), Feature.WorldMax, OutError)
					|| !CombatMinimapConfigPrivate::ReadColor(FeatureObject, TEXT("fill"), Feature.FillColor, OutError)
					|| !CombatMinimapConfigPrivate::ReadColor(FeatureObject, TEXT("outline"), Feature.OutlineColor, OutError)
					|| !Feature.IsValid())
				{
					CombatMinimapConfigPrivate::SetError(OutError, OutError && !OutError->IsEmpty() ? *OutError : TEXT("地形特征字段无效"));
					OutMaps.Reset();
					return false;
				}
				Map.TerrainFeatures.Add(MoveTemp(Feature));
			}
			if (!Map.IsValid())
			{
				CombatMinimapConfigPrivate::SetError(OutError, TEXT("地图条目范围无效"));
				OutMaps.Reset();
				return false;
			}
			OutMaps.Add(MoveTemp(Map));
		}
		return OutMaps.Num() > 0;
	}

	bool Load(TArray<FCombatMinimapMap>& OutMaps, FString* OutError)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *GetGeneratedConfigPath()))
		{
			CombatMinimapConfigPrivate::SetError(OutError, TEXT("生成配置文件不存在"));
			return false;
		}
		return Deserialize(Json, OutMaps, OutError);
	}

#if WITH_EDITOR
	bool Save(const TArray<FCombatMinimapMap>& Maps, FString* OutError)
	{
		FString Json;
		if (!Serialize(Maps, Json, OutError)) return false;
		const FString Path = GetGeneratedConfigPath();
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		const FString TempPath = Path + TEXT(".tmp");
		if (!FFileHelper::SaveStringToFile(Json, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
			|| !IFileManager::Get().Move(*Path, *TempPath, true, true, true))
		{
			IFileManager::Get().Delete(*TempPath, false, true);
			CombatMinimapConfigPrivate::SetError(OutError, FString::Printf(TEXT("无法写入生成配置：%s"), *Path));
			return false;
		}
		return true;
	}
#endif
}

bool FCombatMinimapTerrainFeature::IsValid() const
{
	return !WorldMin.ContainsNaN() && !WorldMax.ContainsNaN()
		&& FMath::IsFinite(WorldMin.X) && FMath::IsFinite(WorldMin.Y)
		&& FMath::IsFinite(WorldMax.X) && FMath::IsFinite(WorldMax.Y)
		&& WorldMin.X < WorldMax.X && WorldMin.Y < WorldMax.Y
		&& FMath::IsFinite(FillColor.R) && FMath::IsFinite(FillColor.G)
		&& FMath::IsFinite(FillColor.B) && FMath::IsFinite(FillColor.A)
		&& FMath::IsFinite(OutlineColor.R) && FMath::IsFinite(OutlineColor.G)
		&& FMath::IsFinite(OutlineColor.B) && FMath::IsFinite(OutlineColor.A);
}

bool FCombatMinimapMap::IsValid() const
{
	return !MapPackage.IsNone() && !WorldMin.ContainsNaN() && !WorldMax.ContainsNaN()
		&& FMath::IsFinite(WorldMin.X) && FMath::IsFinite(WorldMin.Y)
		&& FMath::IsFinite(WorldMax.X) && FMath::IsFinite(WorldMax.Y)
		&& WorldMin.X < WorldMax.X && WorldMin.Y < WorldMax.Y && FMath::IsFinite(GroundZ);
}

UCombatMinimapWidget::UCombatMinimapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The Demo floor bounds are read from Floor_0; the per-package table keeps other maps from reusing them.
	FCombatMinimapMap Demo;
	Demo.MapPackage = TEXT("/Game/Combat/Demo/Maps/L_CombatDemo");
	Demo.WorldMin = FVector2D(-2000.0f, -2000.0f);
	Demo.WorldMax = FVector2D(2000.0f, 2000.0f);
	Demo.GroundZ = 0.0f;
	const FLinearColor BoundaryFill(0.06f, 0.08f, 0.12f, 0.96f);
	const FLinearColor BoundaryOutline(0.34f, 0.48f, 0.58f, 0.92f);
	const FLinearColor BlockerFill(0.08f, 0.19f, 0.25f, 0.94f);
	const FLinearColor BlockerOutline(0.22f, 0.64f, 0.72f, 0.88f);
	const FLinearColor LandmarkFill(0.34f, 0.20f, 0.08f, 0.96f);
	const FLinearColor LandmarkOutline(1.0f, 0.68f, 0.18f, 0.94f);
	// 这些范围来自 Demo/Test 的真实 StaticMeshActor bounds：四面边界墙、中心掩体和四角掩体。
	// 小地图只把它们转成只读图形；关卡几何仍是碰撞、导航和移动的唯一来源。
	// 这些向量只作为生成 JSON 缺失或非法时的兼容回退；改图后应在 Editor 使用生成按钮，
	// 由 CombatMinimapEditor 扫描当前关卡并更新 Config/CombatMinimap.generated.json。
	// 运行时不扫描关卡，也不生成纹理资产，避免把表现层变成第二份 gameplay 事实源。
	const auto AddPrototypeFeatures = [&](FCombatMinimapMap& Map)
	{
		CombatMinimapPrivate::AddTerrainFeature(Map, FVector2D(-2000, -2000), FVector2D(2000, -1800), ECombatMinimapTerrainType::Boundary, BoundaryFill, BoundaryOutline);
		CombatMinimapPrivate::AddTerrainFeature(Map, FVector2D(-2000, 1800), FVector2D(2000, 2000), ECombatMinimapTerrainType::Boundary, BoundaryFill, BoundaryOutline);
		CombatMinimapPrivate::AddTerrainFeature(Map, FVector2D(-2000, -1800), FVector2D(-1800, 1800), ECombatMinimapTerrainType::Boundary, BoundaryFill, BoundaryOutline);
		CombatMinimapPrivate::AddTerrainFeature(Map, FVector2D(1800, -1800), FVector2D(2000, 1800), ECombatMinimapTerrainType::Boundary, BoundaryFill, BoundaryOutline);

		// 四个方向的中段掩体，保留了原型场地的十字形识别度。
		CombatMinimapPrivate::AddTerrainFeature(Map, FVector2D(-1900, -260), FVector2D(-1650, 240), ECombatMinimapTerrainType::Blocker, BlockerFill, BlockerOutline);
		CombatMinimapPrivate::AddTerrainFeature(Map, FVector2D(1650, -260), FVector2D(1900, 240), ECombatMinimapTerrainType::Blocker, BlockerFill, BlockerOutline);
		CombatMinimapPrivate::AddTerrainFeature(Map, FVector2D(-260, 1650), FVector2D(240, 1900), ECombatMinimapTerrainType::Blocker, BlockerFill, BlockerOutline);
		CombatMinimapPrivate::AddTerrainFeature(Map, FVector2D(-260, -1900), FVector2D(240, -1650), ECombatMinimapTerrainType::Blocker, BlockerFill, BlockerOutline);

		// 四角的方块组合按实际障碍范围逐块绘制；低优先级障碍只显示形状，避免小地图拥挤。
		const auto AddCover = [&](const FVector2D& Min, const FVector2D& Max)
		{
			CombatMinimapPrivate::AddTerrainFeature(Map, Min, Max, ECombatMinimapTerrainType::Blocker, BlockerFill, BlockerOutline);
		};
		AddCover(FVector2D(-1800, 1200), FVector2D(-1500, 1800));
		AddCover(FVector2D(-1500, 1200), FVector2D(-1200, 1500));
		AddCover(FVector2D(-1800, 800), FVector2D(-1500, 1200));
		AddCover(FVector2D(800, 1500), FVector2D(1200, 1800));
		AddCover(FVector2D(1200, 1500), FVector2D(1800, 1800));
		AddCover(FVector2D(1500, 800), FVector2D(1800, 1500));
		AddCover(FVector2D(-1800, -1800), FVector2D(-1500, -1500));
		AddCover(FVector2D(-1500, -1800), FVector2D(-1200, -1500));
		AddCover(FVector2D(-1800, -1500), FVector2D(-1500, -1200));
		AddCover(FVector2D(800, -1800), FVector2D(1200, -1500));
		AddCover(FVector2D(1200, -1800), FVector2D(1800, -1500));
		AddCover(FVector2D(1500, -1500), FVector2D(1800, -1200));

		// 中央十字掩体与高台来自中心一组实际原型方块，作为主要导航识别点；十字本身不加文字。
		AddCover(FVector2D(-900, -200), FVector2D(900, 200));
		AddCover(FVector2D(-200, -900), FVector2D(200, 900));
		CombatMinimapPrivate::AddTerrainFeature(Map, FVector2D(-350, -350), FVector2D(350, 350), ECombatMinimapTerrainType::Landmark, LandmarkFill, LandmarkOutline);
		AddCover(FVector2D(-525, -525), FVector2D(-200, -200));
		AddCover(FVector2D(-525, 200), FVector2D(-200, 535));
		AddCover(FVector2D(200, -525), FVector2D(525, -200));
		AddCover(FVector2D(200, 210), FVector2D(525, 535));

		CombatMinimapPrivate::AddTerrainFeature(Map, FVector2D(1080, 880), FVector2D(1320, 1120), ECombatMinimapTerrainType::Landmark, LandmarkFill, LandmarkOutline);
		CombatMinimapPrivate::AddTerrainFeature(Map, FVector2D(700, 700), FVector2D(1550, 850), ECombatMinimapTerrainType::Landmark, LandmarkFill, LandmarkOutline);
	};
	AddPrototypeFeatures(Demo);
	Maps.Add(Demo);
	FCombatMinimapMap Test;
	Test.MapPackage = TEXT("/Game/Combat/Tests/L_CombatTest");
	Test.WorldMin = Demo.WorldMin;
	Test.WorldMax = Demo.WorldMax;
	Test.GroundZ = Demo.GroundZ;
	Test.TerrainFeatures = Demo.TerrainFeatures;
	Maps.Add(Test);
	// 编辑器生成配置优先于旧向量表，但只替换同包地图；文件缺失或非法时继续使用内置 Demo/Test 回退。
	TArray<FCombatMinimapMap> GeneratedMaps;
	FString GeneratedError;
	if (CombatMinimapConfig::Load(GeneratedMaps, &GeneratedError))
	{
		for (FCombatMinimapMap& GeneratedMap : GeneratedMaps)
		{
			const int32 ExistingIndex = FindMap(Maps, GeneratedMap.MapPackage.ToString());
			if (ExistingIndex == INDEX_NONE) Maps.Add(MoveTemp(GeneratedMap));
			else Maps[ExistingIndex] = MoveTemp(GeneratedMap);
		}
	}
	else if (!GeneratedError.IsEmpty() && !GeneratedError.Equals(TEXT("生成配置文件不存在")))
	{
		UE_LOG(LogCombatGame, Warning, TEXT("CombatMinimap generated config ignored: %s"), *GeneratedError);
	}
	SetIsFocusable(true);
}

bool UCombatMinimapWidget::ProjectWorldToMap(const FVector2D& World, const FVector2D& Min,
	const FVector2D& Max, FVector2D& UV)
{
	if (!CombatMinimapPrivate::Finite(World) || !CombatMinimapPrivate::Finite(Min)
		|| !CombatMinimapPrivate::Finite(Max) || Min.X >= Max.X || Min.Y >= Max.Y) return false;
	const FVector2D Range = Max - Min;
	UV = FVector2D((World.Y - Min.Y) / Range.Y, 1.0f - (World.X - Min.X) / Range.X);
	return UV.X >= 0.0f && UV.X <= 1.0f && UV.Y >= 0.0f && UV.Y <= 1.0f;
}

bool UCombatMinimapWidget::UnprojectMapToWorld(const FVector2D& UV, const FVector2D& Min,
	const FVector2D& Max, FVector2D& World)
{
	if (!CombatMinimapPrivate::Finite(UV) || UV.X < 0.0f || UV.X > 1.0f || UV.Y < 0.0f || UV.Y > 1.0f
		|| !CombatMinimapPrivate::Finite(Min) || !CombatMinimapPrivate::Finite(Max)
		|| Min.X >= Max.X || Min.Y >= Max.Y) return false;
	const FVector2D Range = Max - Min;
	World = FVector2D(Min.X + (1.0f - UV.Y) * Range.X, Min.Y + UV.X * Range.Y);
	return CombatMinimapPrivate::Finite(World);
}

FVector2D UCombatMinimapWidget::ClampViewCenter(FVector2D CenterUV, float Zoom)
{
	Zoom = FMath::Max(1.0f, FMath::IsFinite(Zoom) ? Zoom : 1.0f);
	const float HalfSpan = 0.5f / Zoom;
	return FVector2D(
		FMath::Clamp(FMath::IsFinite(CenterUV.X) ? CenterUV.X : 0.5f, HalfSpan, 1.0f - HalfSpan),
		FMath::Clamp(FMath::IsFinite(CenterUV.Y) ? CenterUV.Y : 0.5f, HalfSpan, 1.0f - HalfSpan));
}

int32 UCombatMinimapWidget::FindMap(const TArray<FCombatMinimapMap>& Configs, const FString& WorldPackage)
{
	if (WorldPackage.IsEmpty()) return INDEX_NONE;
	const FString NormalizedWorldPackage = UWorld::RemovePIEPrefix(WorldPackage);
	int32 Match = INDEX_NONE;
	for (int32 Index = 0; Index < Configs.Num(); ++Index)
	{
		const FString ConfigPackage = Configs[Index].MapPackage.ToString();
		if (!Configs[Index].IsValid() || ConfigPackage != NormalizedWorldPackage) continue;
		if (Match != INDEX_NONE) return INDEX_NONE;
		Match = Index;
	}
	return Match;
}

void UCombatMinimapWidget::ConfigureViewportLayout(const float StandardSize, const float CompactSize,
	const float InMargin, const float InGap)
{
	StandardLayoutSize = FMath::Max(64.0f, FMath::IsFinite(StandardSize) ? StandardSize : 220.0f);
	CompactLayoutSize = FMath::Clamp(FMath::IsFinite(CompactSize) ? CompactSize : 184.0f, 64.0f, StandardLayoutSize);
	LayoutMargin = FMath::Max(0.0f, FMath::IsFinite(InMargin) ? InMargin : 22.0f);
	LayoutGap = FMath::Max(0.0f, FMath::IsFinite(InGap) ? InGap : 12.0f);
}

FCombatMinimapLayout UCombatMinimapWidget::ResolveLayout(const FVector2D ViewSize,
	const FSlateRect& HeroRect, const bool bHeroVisible, const float StandardSize,
	const float CompactSize, const float Margin, const float Gap)
{
	FCombatMinimapLayout Result;
	if (ViewSize.X <= 0.0f || ViewSize.Y <= 0.0f) return Result;
	const float Sizes[] = { StandardSize, CompactSize };
	// UUserWidget 的根通常覆盖整个 PlayerScreen，只是承载子面板；把它当作实体面板会让
	// 左下角候选位置永远冲突并被抬到屏幕外。真正覆盖整个视口时不参与避让。
	const float HeroWidth = HeroRect.Right - HeroRect.Left;
	const bool bHeroIsViewportContainer = bHeroVisible
		&& (HeroWidth >= ViewSize.X * 0.75f || HeroRect.Left <= Margin + StandardSize);
	const bool bUseHeroAvoidance = bHeroVisible && !bHeroIsViewportContainer
		&& !(HeroRect.Left <= 0.0f && HeroRect.Top <= 0.0f
			&& HeroRect.Right >= ViewSize.X && HeroRect.Bottom >= ViewSize.Y);
	for (const float Candidate : Sizes)
	{
		FVector2D Position(Margin, ViewSize.Y - Margin - Candidate);
		const bool bHorizontalOverlap = bUseHeroAvoidance && Position.X + Candidate > HeroRect.Left && Position.X < HeroRect.Right;
		const bool bVerticalOverlap = bUseHeroAvoidance && Position.Y + Candidate > HeroRect.Top && Position.Y < HeroRect.Bottom;
		if (bHorizontalOverlap && bVerticalOverlap) Position.Y = HeroRect.Top - Gap - Candidate;
		if (Position.X >= 0.0f && Position.Y >= 0.0f && Position.X + Candidate <= ViewSize.X
			&& Position.Y + Candidate <= ViewSize.Y)
		{
			Result.Position = Position;
			Result.Size = Candidate;
			Result.bVisible = true;
			return Result;
		}
	}
	return Result;
}

void UCombatMinimapWidget::UpdateViewportLayout()
{
	ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwningPlayer());
	if (!PC || ActiveMap == INDEX_NONE) { SetVisibility(ESlateVisibility::Collapsed); return; }
	int32 Width = 0, Height = 0;
	PC->GetViewportSize(Width, Height);
	// 小地图是左下角固定锚点；底部主 HUD 居中，任何承载层几何都不能把左角抬离视口。
	Layout = ResolveLayout(FVector2D(Width, Height), FSlateRect(), false,
		StandardLayoutSize, CompactLayoutSize, LayoutMargin, LayoutGap);
	SetVisibility(Layout.bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Layout.bVisible)
	{
		SetDesiredSizeInViewport(FVector2D(Layout.Size, Layout.Size));
		// Anchor to the viewport's lower-left corner instead of subtracting the
		// unscaled design size from the pixel viewport height.  In PIE and at
		// high-DPI display scales the visual size is smaller than Layout.Size;
		// the bottom anchor keeps the rendered square flush with the same margin.
		SetPositionInViewport(FVector2D(Layout.Position.X, -LayoutMargin), true);
		SetAnchorsInViewport(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
		SetAlignmentInViewport(FVector2D(0.0f, 1.0f));
	}
}

void UCombatMinimapWidget::RefreshMapConfiguration()
{
	ActiveMap = FindMap(Maps, CombatMinimapPrivate::PackageName(GetWorld()));
	TrackedUnits.Reset();
	Markers.Reset();
	LastPrimary.Reset();
	LastBinding = INDEX_NONE;
	LastLife = 0;
	bLastReady = false;
	PingRemaining = 0.0f;
	ViewCenterUV = ClampViewCenter(FVector2D(0.5f, 0.5f), ViewZoom);
	ResetPointerState();
	if (MapStatus)
	{
		MapStatus->SetText(FText::GetEmpty());
		MapStatus->SetVisibility(ESlateVisibility::Collapsed);
	}
	UpdateViewportLayout();
}

void UCombatMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 地图操作不使用悬停说明条，避免鼠标进入小地图时在其下方弹出一行文字。
	SetToolTipText(FText::GetEmpty());
	RefreshMapConfiguration();
	if (WidgetTree)
	{
		// 旧 HUD 蓝图可能保留未绑定的 Image 或 TextBlock。原生地形层是唯一底图来源时，
		// 收起这些装饰控件，避免子控件在 NativePaint 后覆盖地形或重新引入文字。
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			if (UImage* Image = Cast<UImage>(Widget))
				Image->SetVisibility(Image == MapSurface.Get() ? Image->GetVisibility() : ESlateVisibility::Collapsed);
			if (UTextBlock* Text = Cast<UTextBlock>(Widget))
			{
				Text->SetText(FText::GetEmpty());
				Text->SetVisibility(ESlateVisibility::Collapsed);
			}
		});
	}
	if (MapStatus) MapStatus->SetVisibility(ESlateVisibility::Collapsed);
	if (MapSurface)
	{
		if (ActiveMap != INDEX_NONE && Maps[ActiveMap].Texture)
		{
			MapSurface->SetBrushFromTexture(Maps[ActiveMap].Texture);
			MapSurface->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			// Designer 中常见的 Image 默认是白色刷子；没有真实纹理时让原生地形层独占表面，
			// 避免这个可选绑定把暗色地形和标识覆盖成白块。
			MapSurface->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (ActiveMap != INDEX_NONE) DiscoverUnits();
}

void UCombatMinimapWidget::NativeDestruct()
{
	ResetPointerState();
	TrackedUnits.Reset();
	Markers.Reset();
	PingRemaining = 0.0f;
	HoveredUnit.Reset();
	Super::NativeDestruct();
}

void UCombatMinimapWidget::DiscoverUnits()
{
	TrackedUnits.Reset();
	if (!GetWorld() || ActiveMap == INDEX_NONE) return;
	for (TActorIterator<ACombatUnitCharacter> It(GetWorld()); It; ++It)
		if (CanDisplayUnit(*It)) TrackedUnits.Add(*It);
}

bool UCombatMinimapWidget::CanDisplayUnit(ACombatUnitCharacter* Unit) const
{
	return IsValid(Unit) && Unit->GetWorld() == GetWorld() && !Unit->IsHidden()
		&& Unit->GetLifeState() == ECombatLifeState::Alive;
}

void UCombatMinimapWidget::ClearTransientPresentation()
{
	HoveredUnit.Reset();
	PingRemaining = 0.0f;
	Markers.Reset();
}

void UCombatMinimapWidget::RefreshPresentation(const float DeltaTime)
{
	if (ActiveMap == INDEX_NONE) return;
	const ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwningPlayer());
	ACombatUnitCharacter* Primary = PC ? PC->GetCommandedUnit() : nullptr;
	const int32 Binding = PC ? PC->GetCommandBindingGeneration() : 0;
	const uint32 Life = Primary ? Primary->GetLifeGeneration() : 0;
	const bool bReady = PC && Primary && PC->CanControlUnit(Primary) && PC->CanOperateInspectedUnit();
	if (LastPrimary.Get() != Primary || LastBinding != Binding || LastLife != Life || bLastReady != bReady)
	{
		ClearTransientPresentation();
		LastPrimary = Primary;
		LastBinding = Binding;
		LastLife = Life;
		bLastReady = bReady;
	}
	Markers.Reset();
	const UCombatTeamSubsystem* Teams = GetWorld() ? GetWorld()->GetSubsystem<UCombatTeamSubsystem>() : nullptr;
	const FCombatTeamId ViewerTeam = Primary ? Primary->GetCombatTeamId() : FCombatTeamId();
	for (const TWeakObjectPtr<ACombatUnitCharacter>& WeakUnit : TrackedUnits)
	{
		ACombatUnitCharacter* Unit = WeakUnit.Get();
		if (!CanDisplayUnit(Unit)) continue;
		FVector2D UV;
		if (!ProjectWorldToMap(FVector2D(Unit->GetActorLocation()), Maps[ActiveMap].WorldMin, Maps[ActiveMap].WorldMax, UV)) continue;
		FCombatMinimapMarker Marker;
		Marker.Unit = Unit;
		Marker.LifeGeneration = Unit->GetLifeGeneration();
		Marker.UV = UV;
		Marker.bPrimary = Unit == Primary && bReady;
		Marker.Relation = Teams && ViewerTeam.IsValid() && Unit->GetCombatTeamId().IsValid()
			? Teams->GetRelation(ViewerTeam, Unit->GetCombatTeamId()) : ECombatTeamRelation::Invalid;
		Markers.Add(Marker);
	}
	if (PingRemaining > 0.0f) PingRemaining = FMath::Max(0.0f, PingRemaining - FMath::Max(0.0f, DeltaTime));
	// 小地图只显示图形；即使旧 Blueprint 仍绑定状态文本，也强制清空并隐藏。
	if (MapStatus)
	{
		MapStatus->SetText(FText::GetEmpty());
		MapStatus->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UCombatMinimapWidget::IsScreenPositionOverUI(const FVector2D ScreenPosition) const
{
	return ActiveMap != INDEX_NONE && Layout.bVisible && GetCachedGeometry().IsUnderLocation(ScreenPosition);
}

bool UCombatMinimapWidget::PointerToUV(const FVector2D Position, FVector2D& UV) const
{
	if (ActiveMap == INDEX_NONE || !Layout.bVisible) return false;
	const FVector2D Local = GetCachedGeometry().AbsoluteToLocal(Position);
	const FVector2D MapSize(Layout.Size - CombatMinimapPrivate::Border * 2.0f);
	const FVector2D MapLocal = Local - FVector2D(CombatMinimapPrivate::Border);
	if (MapLocal.X < 0.0f || MapLocal.Y < 0.0f || MapLocal.X > MapSize.X || MapLocal.Y > MapSize.Y) return false;
	UV = MapLocal / MapSize;
	return CombatMinimapPrivate::Finite(UV);
}

bool UCombatMinimapWidget::GlobalToViewUV(const FVector2D& GlobalUV, FVector2D& ViewUV) const
{
	const float SafeZoom = FMath::Max(1.0f, FMath::IsFinite(ViewZoom) ? ViewZoom : 1.0f);
	const FVector2D ViewSpan(1.0f / SafeZoom, 1.0f / SafeZoom);
	const FVector2D ViewMin = ClampViewCenter(ViewCenterUV, SafeZoom) - ViewSpan * 0.5f;
	ViewUV = (GlobalUV - ViewMin) / ViewSpan;
	return CombatMinimapPrivate::Finite(ViewUV)
		&& ViewUV.X >= 0.0f && ViewUV.X <= 1.0f && ViewUV.Y >= 0.0f && ViewUV.Y <= 1.0f;
}

bool UCombatMinimapWidget::ViewToGlobalUV(const FVector2D& ViewUV, FVector2D& GlobalUV) const
{
	if (!CombatMinimapPrivate::Finite(ViewUV) || ViewUV.X < 0.0f || ViewUV.X > 1.0f
		|| ViewUV.Y < 0.0f || ViewUV.Y > 1.0f) return false;
	const float SafeZoom = FMath::Max(1.0f, FMath::IsFinite(ViewZoom) ? ViewZoom : 1.0f);
	const FVector2D ViewSpan(1.0f / SafeZoom, 1.0f / SafeZoom);
	const FVector2D ViewMin = ClampViewCenter(ViewCenterUV, SafeZoom) - ViewSpan * 0.5f;
	GlobalUV = ViewMin + ViewUV * ViewSpan;
	return CombatMinimapPrivate::Finite(GlobalUV);
}

void UCombatMinimapWidget::UpdateCameraFromDrag(const FVector2D ScreenPosition)
{
	if (!bPointerDown || ActiveMap == INDEX_NONE) return;
	const FVector2D PreviousScreen = PointerPreviousScreen;
	PointerPreviousScreen = ScreenPosition;
	const FGeometry& Geometry = GetCachedGeometry();
	const FVector2D PreviousLocal = Geometry.AbsoluteToLocal(PreviousScreen);
	const FVector2D CurrentLocal = Geometry.AbsoluteToLocal(ScreenPosition);
	const FVector2D LocalDelta = CurrentLocal - PreviousLocal;
	if (LocalDelta.IsNearlyZero()) return;
	const FVector2D LocalSize = Geometry.GetLocalSize();
	const float MapSize = FMath::Max(1.0f, FMath::Min(LocalSize.X, LocalSize.Y) - CombatMinimapPrivate::Border * 2.0f);
	const float SafeZoom = FMath::Max(1.0f, FMath::IsFinite(ViewZoom) ? ViewZoom : 1.0f);
	const FVector2D ViewSpan(1.0f / SafeZoom, 1.0f / SafeZoom);
	const FVector2D Range = Maps[ActiveMap].WorldMax - Maps[ActiveMap].WorldMin;
	// 地图北向朝上：屏幕向右对应世界 +Y，屏幕向下对应世界 -X。
	const FVector2D WorldDelta(
		-LocalDelta.Y / MapSize * Range.X * ViewSpan.Y,
		LocalDelta.X / MapSize * Range.Y * ViewSpan.X);
	if (ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwningPlayer()))
		PC->PanCameraFromMinimap(WorldDelta);
}

void UCombatMinimapWidget::ResetPointerState()
{
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().ReleaseAllPointerCapture();
	bPointerDown = false;
	bDraggingCamera = false;
	bPointerAlt = false;
	PointerDownScreen = FVector2D::ZeroVector;
	PointerPreviousScreen = FVector2D::ZeroVector;
	PointerButton = FKey();
}

bool UCombatMinimapWidget::ActivateMapPoint(const FVector2D UV, const FKey Button, const bool bAlt)
{
	ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwningPlayer());
	if (!PC || ActiveMap == INDEX_NONE) return false;
	FVector2D GlobalUV;
	if (!ViewToGlobalUV(UV, GlobalUV)) return false;
	FVector2D WorldXY;
	if (!UnprojectMapToWorld(GlobalUV, Maps[ActiveMap].WorldMin, Maps[ActiveMap].WorldMax, WorldXY)) return false;
	if (Button == EKeys::LeftMouseButton)
	{
		PC->CancelCombatTargeting();
		if (bAlt)
		{
			PingUV = GlobalUV;
			PingRemaining = 2.0f;
			return true;
		}
		return PC->RelocateCameraFromMinimap(WorldXY);
	}
	if (Button == EKeys::RightMouseButton)
	{
		if (PC->IsCombatTargetingActive()) { PC->CancelCombatTargeting(); return true; }
		FVector GroundPoint;
		return CombatMinimapPrivate::GroundPoint(GetWorld(), WorldXY, Maps[ActiveMap].GroundZ, GroundPoint)
			&& PC->SubmitMinimapMoveOrder(GroundPoint);
	}
	return false;
}

FVector2D UCombatMinimapWidget::ToLocal(const FVector2D& UV, const FVector2D& Origin, const FVector2D& Size)
{
	return Origin + UV * Size;
}

void UCombatMinimapWidget::DrawLines(FSlateWindowElementList& Elements, const int32 Layer,
	const FPaintGeometry& Geometry, const TArray<FVector2D>& Points, const FLinearColor& Color,
	const float Thickness, const bool bClosed)
{
	if (Points.Num() < 2) return;
	FSlateDrawElement::MakeLines(Elements, Layer, Geometry, Points, ESlateDrawEffect::None, Color, true, Thickness);
	if (bClosed)
	{
		TArray<FVector2D> Closed = Points;
		Closed.Add(Points[0]);
		FSlateDrawElement::MakeLines(Elements, Layer, Geometry, Closed, ESlateDrawEffect::None, Color, true, Thickness);
	}
}

bool UCombatMinimapWidget::ShouldRelocateOnPress(const FKey Button, const bool bAlt)
{
	return Button == EKeys::LeftMouseButton && !bAlt;
}

bool UCombatMinimapWidget::ClipCameraTrapezoidToMap(TArray<FVector2D>& Points, const FVector2D& Size)
{
	if (Points.Num() != 4 || !CombatMinimapPrivate::Finite(Size) || Size.X <= 1.0f || Size.Y <= 1.0f)
		return false;

	for (const FVector2D& Point : Points)
		if (!CombatMinimapPrivate::Finite(Point)) return false;

	// Sutherland–Hodgman 裁剪只删除越过地图矩形的线段，不改变剩余顶点的世界投影位置。
	TArray<FVector2D> Clipped = Points;
	for (int32 Edge = 0; Edge < 4 && Clipped.Num() > 0; ++Edge)
	{
		TArray<FVector2D> Output;
		const int32 Count = Clipped.Num();
		const bool bVertical = Edge < 2;
		const float Boundary = Edge == 0 ? 0.0f : Edge == 1 ? Size.X : Edge == 2 ? 0.0f : Size.Y;
		auto IsInside = [Edge, Boundary](const FVector2D& Point)
		{
			return Edge == 0 ? Point.X >= Boundary : Edge == 1 ? Point.X <= Boundary
				: Edge == 2 ? Point.Y >= Boundary : Point.Y <= Boundary;
		};
		auto Intersect = [bVertical, Boundary](const FVector2D& Start, const FVector2D& End)
		{
			const float Denominator = bVertical ? End.X - Start.X : End.Y - Start.Y;
			if (FMath::Abs(Denominator) <= KINDA_SMALL_NUMBER) return End;
			const float Numerator = Boundary - (bVertical ? Start.X : Start.Y);
			const float Alpha = FMath::Clamp(Numerator / Denominator, 0.0f, 1.0f);
			return FMath::Lerp(Start, End, Alpha);
		};
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2D& Start = Clipped[Index];
			const FVector2D& End = Clipped[(Index + 1) % Count];
			const bool bStartInside = IsInside(Start);
			const bool bEndInside = IsInside(End);
			if (bStartInside && bEndInside) Output.Add(End);
			else if (bStartInside && !bEndInside) Output.Add(Intersect(Start, End));
			else if (!bStartInside && bEndInside)
			{
				Output.Add(Intersect(Start, End));
				Output.Add(End);
			}
		}
		Clipped = MoveTemp(Output);
	}

	for (int32 Index = Clipped.Num() - 1; Index >= 0 && Clipped.Num() > 1; --Index)
	{
		const int32 Previous = (Index + Clipped.Num() - 1) % Clipped.Num();
		if (FVector2D::DistSquared(Clipped[Index], Clipped[Previous]) <= FMath::Square(0.01f))
			Clipped.RemoveAt(Index);
	}
	if (Clipped.Num() < 3) return false;

	double SignedArea = 0.0;
	for (int32 Index = 0; Index < Clipped.Num(); ++Index)
	{
		const FVector2D& Current = Clipped[Index];
		const FVector2D& Next = Clipped[(Index + 1) % Clipped.Num()];
		SignedArea += static_cast<double>(Current.X) * Next.Y - static_cast<double>(Next.X) * Current.Y;
	}
	if (!FMath::IsFinite(SignedArea) || FMath::Abs(SignedArea) <= 1.0) return false;
	Points = MoveTemp(Clipped);
	return true;
}

void UCombatMinimapWidget::PaintTerrain(const FGeometry& Geometry, FSlateWindowElementList& Elements, const int32 Layer) const
{
	const FCombatMinimapMap* Map = GetActiveMap();
	if (!Map || Map->TerrainFeatures.Num() == 0) return;
	const FVector2D Origin(CombatMinimapPrivate::Border);
	const FVector2D MapSize(Layout.Size - CombatMinimapPrivate::Border * 2.0f);
	if (MapSize.X <= 1.0f || MapSize.Y <= 1.0f) return;
	const float SafeZoom = FMath::Max(1.0f, FMath::IsFinite(ViewZoom) ? ViewZoom : 1.0f);
	const FVector2D ViewSpan(1.0f / SafeZoom, 1.0f / SafeZoom);
	const FVector2D ViewMin = ClampViewCenter(ViewCenterUV, SafeZoom) - ViewSpan * 0.5f;
	const FBox2D Clip(FVector2D::ZeroVector, MapSize);

	for (const FCombatMinimapTerrainFeature& Feature : Map->TerrainFeatures)
	{
		FVector2D GlobalMin, GlobalMax;
		if (!CombatMinimapPrivate::ProjectWorldBoundsToMap(Feature, *Map, GlobalMin, GlobalMax)) continue;
		const FVector2D A = (GlobalMin - ViewMin) / ViewSpan * MapSize;
		const FVector2D B = (GlobalMax - ViewMin) / ViewSpan * MapSize;
		const FBox2D FeatureBox(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)),
			FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)));
		const FVector2D VisibleMin(
			FMath::Clamp(FeatureBox.Min.X, Clip.Min.X, Clip.Max.X),
			FMath::Clamp(FeatureBox.Min.Y, Clip.Min.Y, Clip.Max.Y));
		const FVector2D VisibleMax(
			FMath::Clamp(FeatureBox.Max.X, Clip.Min.X, Clip.Max.X),
			FMath::Clamp(FeatureBox.Max.Y, Clip.Min.Y, Clip.Max.Y));
		if (VisibleMax.X - VisibleMin.X <= 0.5f || VisibleMax.Y - VisibleMin.Y <= 0.5f) continue;

		const FSlateColorBrush Fill(Feature.FillColor);
		FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(
			VisibleMax - VisibleMin, FSlateLayoutTransform(Origin + VisibleMin)), &Fill,
			ESlateDrawEffect::None, Feature.FillColor);
		DrawLines(Elements, Layer + 1, Geometry.ToPaintGeometry(),
			{ Origin + VisibleMin,
				Origin + FVector2D(VisibleMax.X, VisibleMin.Y),
				Origin + VisibleMax,
				Origin + FVector2D(VisibleMin.X, VisibleMax.Y) }, Feature.OutlineColor, 1.0f, true);
	}
}

void UCombatMinimapWidget::PaintCamera(const FGeometry& Geometry, FSlateWindowElementList& Elements, const int32 Layer) const
{
	const ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwningPlayer());
	const FCombatMinimapMap* Map = GetActiveMap();
	if (!PC || !Map) return;
	int32 Width = 0, Height = 0;
	PC->GetViewportSize(Width, Height);
	if (Width <= 0 || Height <= 0 || Layout.Size <= CombatMinimapPrivate::Border * 2.0f) return;
	const FVector2D MapOrigin(CombatMinimapPrivate::Border);
	const FVector2D MapSize(Layout.Size - CombatMinimapPrivate::Border * 2.0f);
	const float SafeZoom = FMath::Max(1.0f, FMath::IsFinite(ViewZoom) ? ViewZoom : 1.0f);
	const FVector2D ViewSpan(1.0f / SafeZoom, 1.0f / SafeZoom);
	const FVector2D ViewMin = ClampViewCenter(ViewCenterUV, SafeZoom) - ViewSpan * 0.5f;
	TArray<FVector2D> Points;
	for (const FVector2D& Screen : { FVector2D(0, 0), FVector2D(Width, 0), FVector2D(Width, Height), FVector2D(0, Height) })
	{
		FVector RayOrigin, Direction;
		if (!PC->DeprojectScreenPositionToWorld(Screen.X, Screen.Y, RayOrigin, Direction)
			|| FMath::Abs(Direction.Z) < KINDA_SMALL_NUMBER) return;
		const double Time = (Map->GroundZ - RayOrigin.Z) / Direction.Z;
		if (!FMath::IsFinite(Time) || Time < 0.0) return;
		const FVector Point = RayOrigin + Direction * Time;
		const FVector2D Range = Map->WorldMax - Map->WorldMin;
		const FVector2D GlobalUV((Point.Y - Map->WorldMin.Y) / Range.Y, 1.0f - (Point.X - Map->WorldMin.X) / Range.X);
		const FVector2D ViewUV = (GlobalUV - ViewMin) / ViewSpan;
		if (!CombatMinimapPrivate::Finite(ViewUV)) return;
		// 先保留越界坐标，最后按地图表面裁剪；不能提前 Clamp，否则轮廓会在边缘停止移动。
		Points.Add(ToLocal(ViewUV, FVector2D::ZeroVector, MapSize));
	}
	if (!ClipCameraTrapezoidToMap(Points, MapSize)) return;
	for (FVector2D& Point : Points) Point += MapOrigin;
	DrawLines(Elements, Layer, Geometry.ToPaintGeometry(), Points, FLinearColor(0.3f, 1.0f, 0.9f, 0.8f), 1.5f, true);
}

int32 UCombatMinimapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& Culling, FSlateWindowElementList& Elements, const int32 Layer,
	const FWidgetStyle& Style, const bool bEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, Geometry, Culling, Elements, Layer, Style, bEnabled);
	const FCombatMinimapMap* Map = GetActiveMap();
	if (!Map || !Layout.bVisible) return BaseLayer;
	const FVector2D Origin(CombatMinimapPrivate::Border);
	const FVector2D MapSize(Layout.Size - CombatMinimapPrivate::Border * 2.0f);
	const FPaintGeometry PaintGeometry = Geometry.ToPaintGeometry();
	const FPaintGeometry MapGeometry = Geometry.ToPaintGeometry(MapSize, FSlateLayoutTransform(Origin));
	const FSlateColorBrush Frame(FLinearColor(0.01f, 0.02f, 0.05f, 0.96f));
	const FSlateColorBrush Surface(FLinearColor(0.04f, 0.13f, 0.10f, 0.96f));
	FSlateDrawElement::MakeBox(Elements, BaseLayer, PaintGeometry, &Frame,
		ESlateDrawEffect::None, Frame.GetTint(Style));
	if (!Map->Texture)
	{
		FSlateDrawElement::MakeBox(Elements, BaseLayer + 1, MapGeometry, &Surface,
			ESlateDrawEffect::None, Surface.GetTint(Style));
	}
	else
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetResourceObject(Map->Texture);
		Brush.ImageSize = MapSize;
		const float SafeZoom = FMath::Max(1.0f, FMath::IsFinite(ViewZoom) ? ViewZoom : 1.0f);
		const FVector2D ViewSpan(1.0f / SafeZoom, 1.0f / SafeZoom);
		const FVector2D ViewMin = ClampViewCenter(ViewCenterUV, SafeZoom) - ViewSpan * 0.5f;
		Brush.SetUVRegion(FBox2D(ViewMin, ViewMin + ViewSpan));
		FSlateDrawElement::MakeBox(Elements, BaseLayer + 1, MapGeometry, &Brush);
	}
	for (int32 Index = 1; Index < 4; ++Index)
	{
		const float Fraction = Index / 4.0f;
		DrawLines(Elements, BaseLayer + 2, PaintGeometry,
			{ Origin + FVector2D(MapSize.X * Fraction, 0), Origin + FVector2D(MapSize.X * Fraction, MapSize.Y) },
			FLinearColor(0.3f, 0.6f, 0.5f, 0.18f), 1.0f);
		DrawLines(Elements, BaseLayer + 2, PaintGeometry,
			{ Origin + FVector2D(0, MapSize.Y * Fraction), Origin + FVector2D(MapSize.X, MapSize.Y * Fraction) },
			FLinearColor(0.3f, 0.6f, 0.5f, 0.18f), 1.0f);
	}
	DrawLines(Elements, BaseLayer + 3, PaintGeometry,
		{ Origin + FVector2D(MapSize.X - 14, 17), Origin + FVector2D(MapSize.X - 14, 5) },
		FLinearColor(0.7f, 1.0f, 0.9f, 0.9f), 1.5f);
	PaintTerrain(Geometry, Elements, BaseLayer + 3);
	PaintCamera(Geometry, Elements, BaseLayer + 4);
	for (const FCombatMinimapMarker& Marker : Markers)
	{
		const float Radius = Marker.bPrimary ? 6.5f : 5.0f;
		FLinearColor Color = NeutralColor;
		uint8 Shape = 3;
		if (Marker.bPrimary) { Color = PrimaryColor; Shape = 0; }
		else if (Marker.Relation == ECombatTeamRelation::Friendly) { Color = FriendlyColor; Shape = 1; }
		else if (Marker.Relation == ECombatTeamRelation::Hostile) { Color = HostileColor; Shape = 2; }
		FVector2D ViewUV;
		if (!GlobalToViewUV(Marker.UV, ViewUV)) continue;
		const FVector2D Center = ToLocal(ViewUV, Origin, MapSize);
		TArray<FVector2D> Points;
		if (Shape == 1)
		{
			for (int32 Index = 0; Index <= 12; ++Index)
			{
				const float Angle = 2.0f * PI * Index / 12.0f;
				Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
			}
		}
		else if (Shape == 2) Points = { Center + FVector2D(0, -Radius), Center + FVector2D(Radius, Radius), Center + FVector2D(-Radius, Radius) };
		else if (Shape == 3) Points = { Center + FVector2D(-Radius, -Radius), Center + FVector2D(Radius, -Radius), Center + FVector2D(Radius, Radius), Center + FVector2D(-Radius, Radius) };
		else Points = { Center + FVector2D(0, -Radius), Center + FVector2D(Radius, 0), Center + FVector2D(0, Radius), Center + FVector2D(-Radius, 0) };
		DrawLines(Elements, BaseLayer + 5, PaintGeometry, Points, Color, 1.8f, true);
	}
	if (PingRemaining > 0.0f)
	{
		const float Progress = 1.0f - PingRemaining / 2.0f;
		const float Radius = 8.0f + 12.0f * Progress;
		FVector2D ViewUV;
		if (!GlobalToViewUV(PingUV, ViewUV)) return BaseLayer + 7;
		const FVector2D Center = ToLocal(ViewUV, Origin, MapSize);
		TArray<FVector2D> Ring;
		for (int32 Index = 0; Index <= 16; ++Index)
		{
			const float Angle = 2.0f * PI * Index / 16.0f;
			Ring.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		DrawLines(Elements, BaseLayer + 6, PaintGeometry, Ring, NeutralColor.CopyWithNewOpacity(1.0f - Progress), 2.0f);
	}
	DrawLines(Elements, BaseLayer + 7, PaintGeometry,
		{ FVector2D(0.0f, 0.0f), FVector2D(Layout.Size, 0.0f),
			FVector2D(Layout.Size, Layout.Size), FVector2D(0.0f, Layout.Size) },
		FLinearColor(0.25f, 0.9f, 0.8f, 0.75f), 2.0f, true);
	return BaseLayer + 7;
}

FReply UCombatMinimapWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	FVector2D UV;
	if (!PointerToUV(Event.GetScreenSpacePosition(), UV)) return FReply::Handled();
	const FKey Button = Event.GetEffectingButton();
	if (Button == EKeys::RightMouseButton)
	{
		ActivateMapPoint(UV, Button, Event.IsAltDown());
		return FReply::Handled();
	}
	if (Button != EKeys::LeftMouseButton && Button != EKeys::MiddleMouseButton) return FReply::Handled();
	bPointerDown = true;
	bDraggingCamera = false;
	bPointerAlt = Event.IsAltDown();
	PointerButton = Button;
	PointerDownScreen = Event.GetScreenSpacePosition();
	PointerPreviousScreen = PointerDownScreen;
	// 普通左键按下立即把镜头定位到按下点；后续移动只在此基准上累加本地相机增量。
	// Alt+左键保留到释放时才生成 Ping，中键不触发定位。
	if (ShouldRelocateOnPress(Button, bPointerAlt))
		ActivateMapPoint(UV, Button, false);
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UCombatMinimapWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bPointerDown || Event.GetEffectingButton() != PointerButton)
		return IsScreenPositionOverUI(Event.GetScreenSpacePosition()) ? FReply::Handled() : FReply::Unhandled();
	const bool bWasDragging = bDraggingCamera;
	const bool bAlt = bPointerAlt;
	const FKey Button = PointerButton;
	ResetPointerState();
	if (!bWasDragging && Button == EKeys::LeftMouseButton && bAlt)
	{
		FVector2D UV;
		if (PointerToUV(Event.GetScreenSpacePosition(), UV)) ActivateMapPoint(UV, Button, bAlt);
	}
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UCombatMinimapWidget::NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	ResetPointerState();
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UCombatMinimapWidget::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (bPointerDown)
	{
		const FVector2D CurrentScreen = Event.GetScreenSpacePosition();
		const FVector2D ScreenDelta = CurrentScreen - PointerDownScreen;
		if (!bDraggingCamera && ScreenDelta.SizeSquared() >= FMath::Square(CombatMinimapPrivate::ViewDragThreshold))
		{
			bDraggingCamera = true;
			if (ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwningPlayer()))
				PC->CancelCombatTargeting();
		}
		if (bDraggingCamera)
		{
			UpdateCameraFromDrag(CurrentScreen);
			Invalidate(EInvalidateWidgetReason::Paint);
			return FReply::Handled();
		}
		return FReply::Handled();
	}
	SetToolTipText(FText::GetEmpty());
	return FReply::Unhandled();
}

void UCombatMinimapWidget::NativeOnMouseLeave(const FPointerEvent& Event)
{
	SetToolTipText(FText::GetEmpty());
	Super::NativeOnMouseLeave(Event);
}

bool UCombatMinimapWidget::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation)
{
	// A map is a command surface only; dropping an inventory item here must never become item-drop gameplay.
	return IsScreenPositionOverUI(Event.GetScreenSpacePosition());
}

void UCombatMinimapWidget::NativeTick(const FGeometry& Geometry, const float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	UpdateViewportLayout();
	if (ActiveMap == INDEX_NONE) return;
	DiscoveryElapsed += FMath::Max(0.0f, DeltaTime);
	PresentationElapsed += FMath::Max(0.0f, DeltaTime);
	if (DiscoveryElapsed >= 0.5f) { DiscoveryElapsed = 0.0f; DiscoverUnits(); }
	if (PresentationElapsed >= 0.05f)
	{
		const float PresentationDelta = PresentationElapsed;
		PresentationElapsed = 0.0f;
		RefreshPresentation(PresentationDelta);
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}
