#pragma once

#include "CoreMinimal.h"

#include "Combat/Debug/CombatDebugSubsystem.h"

#include "CombatPerformanceBudget.generated.h"

/** M7 冻结的单场景容量与 Dedicated 帧时/带宽预算。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatPerformanceBudget
{
	GENERATED_BODY()

	/** 同时存在的战斗单位上限。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Performance", meta=(DisplayName="单位上限", ToolTip="M7 容量场景允许的最大单位数量。")) int32 MaxUnits = 64;
	/** 同时存在的 Modifier 上限。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Performance", meta=(DisplayName="Modifier 上限", ToolTip="全部单位活动 Modifier 的总预算。")) int32 MaxModifiers = 256;
	/** 同时存在的弹体上限。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Performance", meta=(DisplayName="弹体上限", ToolTip="服务器权威 Projectile registry 的总预算。")) int32 MaxProjectiles = 128;
	/** 同时存在的 Thinker 上限。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Performance", meta=(DisplayName="Thinker 上限", ToolTip="服务器权威 Thinker registry 的总预算。")) int32 MaxThinkers = 32;
	/** 同时存在的 Aura 上限。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Performance", meta=(DisplayName="Aura 上限", ToolTip="服务器权威 Aura registry 的总预算。")) int32 MaxAuras = 16;
	/** Aura child Modifier 总上限。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Performance", meta=(DisplayName="Aura 子项上限", ToolTip="全部 Aura 当前 child Modifier 的总预算。")) int32 MaxAuraChildren = 256;
	/** Scheduler 活动任务槽上限。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Performance", meta=(DisplayName="调度槽上限", ToolTip="Combat Scheduler 同时活动的任务槽预算。")) int32 MaxSchedulerSlots = 256;
	/** Scheduler 单帧执行回调上限。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Performance", meta=(DisplayName="单帧调度回调上限", ToolTip="超过该值表示预算配置或统计失效。")) int32 MaxSchedulerCallbacksPerFrame = 256;
	/** Dedicated Server 帧时 P95 预算，毫秒。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Performance", meta=(DisplayName="服务器 P95 帧时预算", ToolTip="容量 soak 中 Dedicated Server 帧时的 P95 上限；默认值为 30 Hz 的精确帧长。", Units="ms")) float MaxServerFrameP95Ms = 1000.0f / 30.0f;
	/** Dedicated Server 帧时 P99 预算，毫秒。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Performance", meta=(DisplayName="服务器 P99 帧时预算", ToolTip="容量 soak 中 Dedicated Server 帧时的 P99 上限。", Units="ms")) float MaxServerFrameP99Ms = 50.0f;
	/** 单连接平均网络出站带宽预算，KiB/s。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Performance", meta=(DisplayName="单连接带宽预算（KiB/s）", ToolTip="容量 soak 中每个客户端的平均复制出站带宽上限，单位为 KiB/s。")) float MaxPerConnectionBandwidthKiBps = 256.0f;
};

/** 容量和外部采样结果相对冻结预算的判定。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatPerformanceBudgetResult
{
	GENERATED_BODY()

	/** 所有容量、调度、帧时与带宽样本是否均在预算内。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Performance", meta=(DisplayName="预算已通过", ToolTip="所有已提供的容量、调度、帧时和带宽样本是否均在预算内。")) bool bPassed = false;
	/** 超预算字段的稳定中文诊断。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Performance", meta=(DisplayName="超预算项", ToolTip="每个超出冻结预算的字段及其实际值。")) TArray<FString> Violations;
};

/** 对运行时指标与可选外部 Dedicated 样本执行统一预算判定。 */
struct UE_GAS_API FCombatPerformanceBudgetEvaluator
{
	/** 负样本表示未提供外部帧时或带宽，仅判定可在 World 内采集的容量字段。 */
	static FCombatPerformanceBudgetResult Evaluate(
		const FCombatRuntimeMetrics& Metrics,
		const FCombatPerformanceBudget& Budget,
		float ServerFrameP95Ms = -1.0f,
		float ServerFrameP99Ms = -1.0f,
		float PerConnectionBandwidthKiBps = -1.0f);
};
