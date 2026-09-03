#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatDebugSubsystem.generated.h"

class ACombatUnitCharacter;

/** 汇总一个 World 当前战斗负载与累计安全、事件计数。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatRuntimeMetrics
{
	GENERATED_BODY()

	/** 当前战斗单位数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="战斗单位数", ToolTip="当前 World 中的战斗单位总数。")) int32 Units = 0;
	/** 全部单位当前 Modifier 数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="Modifier 数", ToolTip="全部单位当前活动 Modifier Runtime 总数。")) int32 Modifiers = 0;
	/** 全部单位当前 AttackRecord 数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="攻击记录数", ToolTip="全部单位当前活动 AttackRecord 总数。")) int32 Attacks = 0;
	/** 全部单位当前强制位移通道数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="强制位移数", ToolTip="全部单位当前活动 Motion 通道总数。")) int32 Motions = 0;
	/** 全部单位待处理 Order 数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="待处理命令数", ToolTip="全部单位当前排队等待的 Order 总数。")) int32 PendingOrders = 0;
	/** 当前权威弹体数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="权威弹体数", ToolTip="服务器 Projectile registry 当前活动弹体数。")) int32 Projectiles = 0;
	/** 当前 Thinker 数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="Thinker 数", ToolTip="服务器 Thinker registry 当前活动对象数。")) int32 Thinkers = 0;
	/** 当前 Aura 数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="Aura 数", ToolTip="服务器 Aura registry 当前活动 Aura 数。")) int32 Auras = 0;
	/** 当前 Aura child Modifier 总数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="Aura 子项数", ToolTip="全部 Aura 当前维护的 child Modifier 总数。")) int32 AuraChildren = 0;
	/** 当前 Scheduler 活动槽位。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="调度槽数", ToolTip="Combat Scheduler 当前活动任务槽数量。")) int32 SchedulerSlots = 0;
	/** 最近一帧 Scheduler 实际回调数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="本帧调度回调数", ToolTip="Combat Scheduler 最近一帧实际执行的回调数量。")) int32 SchedulerCallbacks = 0;
	/** 最近一帧 Scheduler 预算延期数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="调度延期数", ToolTip="Combat Scheduler 最近一帧因预算限制而延期的回调数量。")) int32 SchedulerDeferrals = 0;
	/** World 生命周期内累计 Combat Event 数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="累计战斗事件数", ToolTip="当前 World 生命周期内累计发出的 Combat Event 数量。")) int64 EmittedEvents = 0;
	/** World 生命周期内累计被安全层拒绝的 Order RPC 数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="累计拒绝命令请求数", ToolTip="当前 World 生命周期内被网络安全层拒绝的 Order 批次数量。")) int64 RejectedOrderRequests = 0;
	/** Dedicated/Listen Server 滚动窗口内有效帧时样本数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="服务器帧时样本数", ToolTip="跳过启动预热后保留在滚动窗口内的服务器帧时样本数。")) int32 ServerFrameSamples = 0;
	/** Dedicated/Listen Server 滚动窗口帧时 P95，毫秒。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="服务器 P95 帧时", ToolTip="当前滚动窗口服务器帧时的第 95 百分位。", Units="ms")) float ServerFrameP95Ms = 0.0f;
	/** Dedicated/Listen Server 滚动窗口帧时 P99，毫秒。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="服务器 P99 帧时", ToolTip="当前滚动窗口服务器帧时的第 99 百分位。", Units="ms")) float ServerFrameP99Ms = 0.0f;
	/** 当前服务器网络连接数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="网络连接数", ToolTip="当前 NetDriver 持有的远端客户端连接数量。")) int32 NetworkConnections = 0;
	/** 采样期间观察到的单连接最大总出站带宽，KiB/s。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Debug", meta=(DisplayName="单连接最大出站带宽（KiB/s）", ToolTip="NetConnection 统计周期内观察到的单连接最大总出站带宽；该值比仅战斗流量更保守。")) float MaxConnectionOutKiBps = 0.0f;

	/** 生成适合服务器日志与自动化断言的单行快照。 */
	FString ToString() const;
};

/**
 * 汇总 Unit、RootEvent、运行时 registry 和容量指标的只读调试入口。
 * 子系统只消费公开快照生成日志或世界绘制，不持有战斗对象、不修正状态，也不参与任何服务器结算路径。
 */
UCLASS()
class UE_GAS_API UCombatDebugSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 按单位当前权威组件状态生成单行转储。 */
	UFUNCTION(BlueprintPure, Category="Combat|Debug", meta=(DisplayName="转储战斗单位", ToolTip="返回单位身份、生命、属性、命令、Modifier、攻击与位移的诊断文本。"))
	FString DumpUnit(UPARAM(DisplayName="战斗单位") const ACombatUnitCharacter* Unit) const;
	/** 展开当前环形窗口内同一 RootEvent 的完整因果链。 */
	FString DumpRootEvent(FCombatEventId RootEventId) const;
	/** 捕获当前 World 战斗容量、调度与 RPC 安全指标。 */
	UFUNCTION(BlueprintPure, Category="Combat|Debug", meta=(DisplayName="捕获战斗运行指标", ToolTip="返回当前 World 的单位、Modifier、弹体、Thinker、Aura、调度和安全计数。"))
	FCombatRuntimeMetrics CaptureMetrics() const;
	/** 启用或关闭单位命令、位移和弹体的持久调试绘制。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Debug", meta=(DisplayName="设置战斗调试绘制", ToolTip="启用后每帧绘制单位状态、命令目标、运动方向与权威弹体。"))
	void SetDebugDrawEnabled(UPARAM(DisplayName="启用") bool bEnabled) { bDebugDrawEnabled = bEnabled; }
	/** 返回当前是否启用战斗调试绘制。 */
	UFUNCTION(BlueprintPure, Category="Combat|Debug", meta=(DisplayName="战斗调试绘制已启用", ToolTip="返回 World 级战斗调试绘制开关。"))
	bool IsDebugDrawEnabled() const { return bDebugDrawEnabled; }

	/** 在启用时绘制当前服务器或客户端可见的只读状态。 */
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	/** 仅在游戏、PIE 与 GamePreview World 创建调试子系统。 */
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	/** 跳过服务器启动预热后，记录滚动帧时和每连接出站带宽。 */
	void RecordPerformanceSample(float DeltaTime);

	/** World 级调试绘制开关，也可由 combat.Debug.Draw 控制台变量启用。 */
	bool bDebugDrawEnabled = false;
	/** 服务器帧时滚动样本；固定容量覆盖旧样本，避免长时 soak 自身增长。 */
	TArray<float> ServerFrameSamplesMs;
	/** 滚动样本写指针。 */
	int32 NextServerFrameSampleIndex = 0;
	/** 首帧真实时间，用于排除地图和 NetDriver 启动阶段。 */
	double PerformanceSamplingStartRealTime = -1.0;
	/** 当前 World 采样期间观察到的最大单连接总出站带宽。 */
	float MaxObservedConnectionOutKiBps = 0.0f;
};
