#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatAIWorldSubsystem.generated.h"

/** 战术 EQS 的本地终态分类；只用于服务器诊断，不进入网络或战斗事件 schema。 */
enum class ECombatAIEQSResult : uint8
{
	Succeeded,
	Failed,
	Cancelled,
	Stale
};

/** World 级 AI 查询统计快照；累计计数只增不减，ActiveEQS 表示当前在途数量。 */
struct COMBAT_API FCombatAIWorldBudgetSnapshot
{
	uint64 PerceptionRequests = 0;
	uint64 PerceptionGranted = 0;
	uint64 PerceptionDeferred = 0;
	uint64 EQSRequests = 0;
	uint64 EQSGranted = 0;
	uint64 EQSDeferred = 0;
	uint64 EQSSucceeded = 0;
	uint64 EQSFailed = 0;
	uint64 EQSCancelled = 0;
	uint64 EQSStale = 0;
	int32 ActiveEQS = 0;
	int32 PeakActiveEQS = 0;
	float EQSP95Milliseconds = 0.0f;
	float EQSP99Milliseconds = 0.0f;
};

/**
 * 每个服务器 World 的轻量 AI 查询闸门。调用者在执行全量感知或启动 EQS 前申请额度；
 * 清理、回执、边界处理和权限检查不经过这里，避免容量压力阻塞生命周期终态。
 */
UCLASS()
class COMBAT_API UCombatAIWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 当前 World 时间片申请一次感知全量扫描；超额只记录延期，不执行扫描。 */
	bool TryAcquirePerception();
	/** 分配本 World 内不与当前在途查询冲突的稳定非零 token。 */
	uint64 AllocateEQSQueryToken();
	/** 当前 World 时间片申请并登记一个全局唯一查询 token；同 token 不可重复在途。 */
	bool TryStartEQS(uint64 QueryToken);
	/** 精确结束仍在途的 token 并记录终态和毫秒耗时；重复/旧完成返回 false。 */
	bool FinishEQS(uint64 QueryToken, ECombatAIEQSResult Result, double ElapsedMilliseconds);
	/** 返回累计计数及当前延迟样本的 p95/p99。 */
	FCombatAIWorldBudgetSnapshot GetSnapshot() const;
	/** 按稳定单位身份把首次采样分布到一个间隔内；非法间隔返回 0。 */
	static float ComputeStableInitialDelay(uint32 StableUnitId, float IntervalSeconds);

#if WITH_DEV_AUTOMATION_TESTS
	/** 自动化覆盖每时间片上限；负数按 0 处理。 */
	void SetFrameLimitsForTesting(int32 PerceptionLimit, int32 EQSStartLimit);
#endif

	virtual void Deinitialize() override;

private:
	void RefreshBudgetSlice();
	static float Percentile(const TArray<float>& SortedSamples, float Quantile);

	int32 PerceptionPerSlice = 16;
	int32 EQSStartsPerSlice = 4;
	double BudgetSliceTime = -DBL_MAX;
	int32 PerceptionUsed = 0;
	int32 EQSStartsUsed = 0;
	FCombatAIWorldBudgetSnapshot Counters;
	uint64 NextQueryToken = 0;
	TSet<uint64> ActiveQueries;
	/** 有界诊断窗口，避免长局按查询数无限增长。 */
	TArray<float> EQSDurationsMilliseconds;
};
