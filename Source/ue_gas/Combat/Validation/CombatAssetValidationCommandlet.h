#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "Combat/Data/CombatDefinitionData.h"

#include "CombatAssetValidationCommandlet.generated.h"

/** 单条 Combat 数据资产校验问题。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAssetValidationIssue
{
	GENERATED_BODY()

	/** 产生问题的资产路径；集合级问题为空。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Validation", meta=(DisplayName="资产路径", ToolTip="产生问题的资产路径；集合级问题为空。")) FString AssetPath;
	/** 稳定问题分类，例如 Identity、Schema、Redirect 或 DataValidation。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Validation", meta=(DisplayName="问题代码", ToolTip="稳定的问题分类，例如 Identity、Schema、Redirect 或 DataValidation。")) FString Code;
	/** 面向内容作者的诊断说明。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Validation", meta=(DisplayName="问题说明", ToolTip="面向内容作者的校验诊断说明。")) FString Message;
	/** true 表示必须阻止 cook；false 表示允许继续但需要关注。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Validation", meta=(DisplayName="是错误", ToolTip="启用表示必须阻止 cook；关闭表示仅为警告。")) bool bError = true;
};

/** 一次项目级 Combat 定义扫描的确定性报告。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAssetValidationReport
{
	GENERATED_BODY()

	/** 报告对应的 Combat 内容版本。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Validation", meta=(DisplayName="内容版本", ToolTip="本报告使用的 Combat 内容 schema 版本。")) int32 ContentVersion = 0;
	/** 被扫描的 Combat 定义资产数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Validation", meta=(DisplayName="扫描资产数", ToolTip="本报告扫描到的 Combat 定义资产总数。")) int32 ScannedAssets = 0;
	/** 阻止 cook 的错误数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Validation", meta=(DisplayName="错误数", ToolTip="会阻止 cook 的校验问题数量。")) int32 ErrorCount = 0;
	/** 不阻止 cook 的警告数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Validation", meta=(DisplayName="警告数", ToolTip="不会阻止 cook、但需要内容作者关注的问题数量。")) int32 WarningCount = 0;
	/** 按扫描顺序记录的全部问题。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Validation", meta=(DisplayName="问题列表", ToolTip="按稳定扫描顺序记录的全部错误与警告。")) TArray<FCombatAssetValidationIssue> Issues;

	/** 返回报告是否允许后续 cook。 */
	bool IsValid() const { return ErrorCount == 0; }
};

/** 保存项目级内容版本与显式 DefinitionId 迁移表。 */
UCLASS(Config=Game, DefaultConfig)
class UE_GAS_API UCombatAssetValidationSettings : public UObject
{
	GENERATED_BODY()

public:
	/** 预期内容版本；必须与代码冻结版本一致。 */
	UPROPERTY(Config, EditAnywhere, Category="Combat|Validation", meta=(ClampMin="1", DisplayName="Combat 内容版本", ToolTip="必须与 FCombatDefinitionRegistry 冻结的内容版本一致。"))
	int32 ContentVersion = FCombatDefinitionRegistry::CombatContentVersion;
	/** 旧 DefinitionId 到现行 DefinitionId 的显式一次迁移。 */
	UPROPERTY(Config, EditAnywhere, Category="Combat|Validation", meta=(DisplayName="定义 ID 重定向", ToolTip="记录已发布旧 ID 到当前 ID 的一次迁移；禁止重定向链、环和缺失目标。"))
	TArray<FCombatDefinitionRedirect> Redirects;
};

/** 扫描 Combat PrimaryDataAsset 并复用 Editor DataValidation 与迁移规则。 */
struct UE_GAS_API FCombatAssetValidator
{
	/** 检查已加载集合的内容版本、定义身份、迁移映射和结构版本；Editor 构建还调用各资产的 IsDataValid。报告保留输入扫描顺序，命令行和自动化共用此入口。 */
	static FCombatAssetValidationReport ValidateDefinitions(
		const TArray<const UCombatDefinitionData*>& Definitions,
		const UCombatAssetValidationSettings& Settings);
	/** 扫描 /Game 下全部 Combat 定义并生成报告。 */
	static FCombatAssetValidationReport ValidateProjectAssets();
	/** 将报告以稳定 JSON 写入指定路径。 */
	static bool WriteJsonReport(const FCombatAssetValidationReport& Report, const FString& OutputPath);
};

/** 供打包前流水线显式调用的数据资产校验命令；发现错误或无法保存报告时返回非零退出码。流水线需检查此退出码，本类不会自动挂接所有 cook 命令。 */
UCLASS()
class UE_GAS_API UCombatAssetValidationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCombatAssetValidationCommandlet();
	/** 执行项目扫描，并把 JSON 报告写到 -Report 或 Saved/CombatValidation。 */
	virtual int32 Main(const FString& Params) override;
};
