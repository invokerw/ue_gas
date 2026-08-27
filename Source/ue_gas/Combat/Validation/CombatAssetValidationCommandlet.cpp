#include "Combat/Validation/CombatAssetValidationCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/DataValidation.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Combat/Log/CombatEventSubsystem.h"

namespace CombatAssetValidation
{
	/** 向报告追加一条问题并更新聚合计数。 */
	static void AddIssue(FCombatAssetValidationReport& Report, const FString& AssetPath, const FString& Code, const FString& Message, const bool bError)
	{
		FCombatAssetValidationIssue& Issue = Report.Issues.AddDefaulted_GetRef();
		Issue.AssetPath = AssetPath;
		Issue.Code = Code;
		Issue.Message = Message;
		Issue.bError = bError;
		bError ? ++Report.ErrorCount : ++Report.WarningCount;
	}
}

FCombatAssetValidationReport FCombatAssetValidator::ValidateDefinitions(
	const TArray<const UCombatDefinitionData*>& Definitions,
	const UCombatAssetValidationSettings& Settings)
{
	FCombatAssetValidationReport Report;
	Report.ContentVersion = Settings.ContentVersion;
	Report.ScannedAssets = Definitions.Num();

	if (Settings.ContentVersion != FCombatDefinitionRegistry::CombatContentVersion)
	{
		CombatAssetValidation::AddIssue(Report, FString(), TEXT("ContentVersion"),
			FString::Printf(TEXT("配置内容版本 %d 与代码版本 %d 不一致"), Settings.ContentVersion, FCombatDefinitionRegistry::CombatContentVersion), true);
	}

	TArray<FString> SetErrors;
	UCombatDefinitionData::ValidateDefinitionSet(Definitions, SetErrors);
	for (const FString& Error : SetErrors)
	{
		CombatAssetValidation::AddIssue(Report, FString(), TEXT("Identity"), Error, true);
	}

	TSet<FPrimaryAssetId> KnownIds;
	for (const UCombatDefinitionData* Definition : Definitions)
	{
		if (IsValid(Definition) && Definition->GetPrimaryAssetId().IsValid())
		{
			KnownIds.Add(Definition->GetPrimaryAssetId());
		}
	}
	TArray<FString> RedirectErrors;
	FCombatDefinitionRegistry::ValidateRedirects(Settings.Redirects, KnownIds, RedirectErrors);
	for (const FString& Error : RedirectErrors)
	{
		CombatAssetValidation::AddIssue(Report, FString(), TEXT("Redirect"), Error, true);
	}

	for (const UCombatDefinitionData* Definition : Definitions)
	{
		if (!IsValid(Definition))
		{
			continue;
		}
		const FString AssetPath = Definition->GetPathName();
		if (Definition->SchemaVersion != 1)
		{
			CombatAssetValidation::AddIssue(Report, AssetPath, TEXT("Schema"),
				FString::Printf(TEXT("不支持的定义 schema 版本：%d"), Definition->SchemaVersion), true);
		}
#if WITH_EDITOR
		FDataValidationContext Context(true, EDataValidationUsecase::Commandlet, {});
		const EDataValidationResult Result = Definition->IsDataValid(Context);
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			const bool bError = Issue.Severity == EMessageSeverity::Error;
			CombatAssetValidation::AddIssue(Report, AssetPath, TEXT("DataValidation"), Issue.Message.ToString(), bError);
		}
		if (Result == EDataValidationResult::Invalid && Context.GetNumErrors() == 0)
		{
			CombatAssetValidation::AddIssue(Report, AssetPath, TEXT("DataValidation"), TEXT("资产返回 Invalid 但没有提供错误文本"), true);
		}
#endif
	}
	return Report;
}

FCombatAssetValidationReport FCombatAssetValidator::ValidateProjectAssets()
{
	FAssetRegistryModule& Module = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& Registry = Module.Get();
	Registry.SearchAllAssets(true);

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(TEXT("/Game")));
	Filter.ClassPaths.Add(UCombatDefinitionData::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;
	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);
	Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });

	TArray<const UCombatDefinitionData*> Definitions;
	Definitions.Reserve(Assets.Num());
	for (const FAssetData& Asset : Assets)
	{
		Definitions.Add(Cast<UCombatDefinitionData>(Asset.GetAsset()));
	}
	return ValidateDefinitions(Definitions, *GetDefault<UCombatAssetValidationSettings>());
}

bool FCombatAssetValidator::WriteJsonReport(const FCombatAssetValidationReport& Report, const FString& OutputPath)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("contentVersion"), Report.ContentVersion);
	Root->SetNumberField(TEXT("scannedAssets"), Report.ScannedAssets);
	Root->SetNumberField(TEXT("errorCount"), Report.ErrorCount);
	Root->SetNumberField(TEXT("warningCount"), Report.WarningCount);
	TArray<TSharedPtr<FJsonValue>> Issues;
	for (const FCombatAssetValidationIssue& Issue : Report.Issues)
	{
		TSharedRef<FJsonObject> JsonIssue = MakeShared<FJsonObject>();
		JsonIssue->SetStringField(TEXT("assetPath"), Issue.AssetPath);
		JsonIssue->SetStringField(TEXT("code"), Issue.Code);
		JsonIssue->SetStringField(TEXT("message"), Issue.Message);
		JsonIssue->SetBoolField(TEXT("error"), Issue.bError);
		Issues.Add(MakeShared<FJsonValueObject>(JsonIssue));
	}
	Root->SetArrayField(TEXT("issues"), Issues);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true);
	return FFileHelper::SaveStringToFile(Json, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

UCombatAssetValidationCommandlet::UCombatAssetValidationCommandlet()
{
	IsClient = false;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UCombatAssetValidationCommandlet::Main(const FString& Params)
{
	FString ReportPath;
	if (!FParse::Value(*Params, TEXT("Report="), ReportPath))
	{
		ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CombatValidation"), TEXT("CombatAssetReport.json"));
	}
	ReportPath = FPaths::ConvertRelativePathToFull(ReportPath);
	const FCombatAssetValidationReport Report = FCombatAssetValidator::ValidateProjectAssets();
	const bool bSaved = FCombatAssetValidator::WriteJsonReport(Report, ReportPath);
	UE_LOG(LogCombat, Display, TEXT("CombatAssetValidation Version=%d Assets=%d Errors=%d Warnings=%d Report=%s Saved=%s"),
		Report.ContentVersion, Report.ScannedAssets, Report.ErrorCount, Report.WarningCount, *ReportPath, bSaved ? TEXT("true") : TEXT("false"));
	for (const FCombatAssetValidationIssue& Issue : Report.Issues)
	{
		UE_LOG(LogCombat, Warning, TEXT("CombatAssetValidation Code=%s Asset=%s Error=%s Message=%s"),
			*Issue.Code, *Issue.AssetPath, Issue.bError ? TEXT("true") : TEXT("false"), *Issue.Message);
	}
	return Report.IsValid() && bSaved ? 0 : 1;
}
