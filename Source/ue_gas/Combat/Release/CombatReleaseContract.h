#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "CombatReleaseContract.generated.h"

/**
 * 冻结 Combat v1 候选发布的版本号、权威边界与延期能力。
 *
 * 该结构可供蓝图、自动化和外部发布工具读取，避免代码、资产与文档分别维护互相漂移的版本声明。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatReleaseContract
{
	GENERATED_BODY()

	/** 发布契约结构自身的版本。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Release", meta=(DisplayName="发布契约版本", ToolTip="发布契约结构自身的版本；修改字段语义时必须递增。"))
	int32 ContractVersion = 1;

	/** 当前候选发布的稳定标识。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Release", meta=(DisplayName="候选发布标识", ToolTip="用于日志、验收报告与问题追踪的稳定候选发布标识。"))
	FName ReleaseId = TEXT("combat_v1_rc1");

	/** Combat PrimaryDataAsset 内容 schema 版本。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Release", meta=(DisplayName="内容版本", ToolTip="Combat PrimaryDataAsset 与 DefinitionId 迁移所使用的内容 schema 版本。"))
	int32 ContentVersion = 1;

	/** Native GameplayTag 命名与语义 schema 版本。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Release", meta=(DisplayName="标签结构版本", ToolTip="Native GameplayTag 的命名和语义版本；不兼容改名或语义变化时必须递增。"))
	int32 GameplayTagSchemaVersion = 1;

	/** 伤害、治疗与数值限制规则的公式版本。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Release", meta=(DisplayName="公式版本", ToolTip="战斗数值公式与取整、限制策略的版本。"))
	int32 FormulaVersion = 1;

	/** keyed RNG 哈希算法版本。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Release", meta=(DisplayName="随机算法版本", ToolTip="确定性随机输入到随机位的算法版本。"))
	int32 RngAlgorithmVersion = 1;

	/** Combat 日志事件 schema 版本。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Release", meta=(DisplayName="事件结构版本", ToolTip="CombatLogRecord 事件结构与消费者兼容性版本。"))
	int32 EventSchemaVersion = 1;

	/** v1 gameplay 结算是否只允许服务器权威入口。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Release", meta=(DisplayName="服务器权威结算", ToolTip="为 true 时客户端请求不能直接写入战斗结果，所有 gameplay 结算由服务器完成。"))
	bool bServerAuthoritativeGameplay = true;

	/** v1 是否允许客户端创建可被服务器弹体身份协调的纯视觉预测。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Release", meta=(DisplayName="弹体视觉预测", ToolTip="只预测视觉表现；预测对象不得造成伤害、施加 Modifier 或生成权威事件。"))
	bool bProjectileVisualPrediction = true;

	/** v1 是否承诺 Ability/伤害的客户端预测回滚。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Release", meta=(DisplayName="Gameplay 回滚", ToolTip="v1 候选发布不提供 Ability、伤害或 Modifier 的客户端预测回滚。"))
	bool bGameplayRollback = false;

	/** v1 是否承诺跨进程确定性重放。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Release", meta=(DisplayName="确定性重放", ToolTip="v1 只冻结事件与 RNG 输入，不承诺跨版本或跨进程的完整确定性重放。"))
	bool bDeterministicReplay = false;

	/** v1 是否包含召唤物与幻象语义。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Release", meta=(DisplayName="召唤与幻象", ToolTip="召唤物、幻象的所有权、继承和清理语义延期到 v1 之后。"))
	bool bSummonsAndIllusions = false;

	/** v1 是否包含物品与经济系统。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Release", meta=(DisplayName="物品与经济", ToolTip="物品、背包、商店与经济系统延期到 v1 之后。"))
	bool bItemsAndEconomy = false;

	/** 校验本契约是否仍与代码中的冻结版本常量和发布边界一致。 */
	bool IsSelfConsistent(FString& OutError) const;
};

/** 为蓝图、自动化与发布脚本提供唯一的 Combat v1 发布契约入口。 */
UCLASS()
class UE_GAS_API UCombatReleaseContractLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 返回当前代码冻结的 Combat v1 候选发布契约。 */
	UFUNCTION(BlueprintPure, Category="Combat|Release", meta=(DisplayName="获取战斗发布契约", ToolTip="返回当前代码冻结的版本号、服务器权威边界和显式延期能力。"))
	static FCombatReleaseContract GetCombatReleaseContract();

	/** 检查传入契约是否与当前代码版本和 v1 功能边界完全一致。 */
	UFUNCTION(BlueprintPure, Category="Combat|Release", meta=(DisplayName="校验战斗发布契约", ToolTip="用于启动检查、自动化或蓝图工具，诊断内容、公式、随机与事件版本漂移。"))
	static bool ValidateCombatReleaseContract(
		UPARAM(DisplayName="发布契约") const FCombatReleaseContract& Contract,
		UPARAM(DisplayName="错误信息") FString& OutError);
};
