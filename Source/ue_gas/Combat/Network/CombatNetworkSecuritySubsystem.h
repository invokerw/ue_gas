#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Network/CombatNetworkTypes.h"

#include "CombatNetworkSecuritySubsystem.generated.h"

class ACombatUnitCharacter;
class APlayerController;

/**
 * Order RPC 的服务器连接级安全边界。
 * 子系统按 owning connection 保存有界限频和 RequestId 重放窗口，先验证控制权与批次载荷，再允许业务 Order 进入 Unit；状态只用于安全判定和诊断，不参与 gameplay 结算。
 */
UCLASS()
class UE_GAS_API UCombatNetworkSecuritySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 校验并原子消费一个请求额度；返回 true 后调用者才允许执行 Order。 */
	bool ValidateAndConsumeOrderRequest(
		APlayerController* RequestingController,
		ACombatUnitCharacter* Unit,
		const FCombatOrderBatchRequest& Request,
		FGameplayTag& OutFailureTag,
		FString& OutDiagnostic);

	/** 返回当前 World 的累计安全统计。 */
	UFUNCTION(BlueprintPure, Category="Combat|Network", meta=(DisplayName="获取 RPC 安全统计", ToolTip="返回当前 World 累计的 Order RPC 接受与拒绝计数。"))
	FCombatRpcSecurityStats GetSecurityStats() const { return Stats; }

	/** 清理所有 connection 状态与统计，主要用于 World teardown 和自动化。 */
	virtual void Deinitialize() override;

	/** 每个请求最多携带的 Order 数量。 */
	UPROPERTY(EditAnywhere, Category="Combat|Network|Limits", meta=(ClampMin="1", ClampMax="64", DisplayName="每请求最大命令数", ToolTip="一个客户端批次允许携带的最大 Order 数量。"))
	int32 MaxOrdersPerRequest = 8;
	/** 请求估算序列化大小上限。 */
	UPROPERTY(EditAnywhere, Category="Combat|Network|Limits", meta=(ClampMin="128", Units="B", DisplayName="最大估算载荷", ToolTip="反序列化后按固定 schema 估算的单请求字节上限。"))
	int32 MaxEstimatedPayloadBytes = 4096;
	/** 每连接每秒持续补充的 token 数量。 */
	UPROPERTY(EditAnywhere, Category="Combat|Network|Limits", meta=(ClampMin="1", DisplayName="每秒请求额度", ToolTip="每个连接 token bucket 每秒持续补充的 token 数。"))
	float RequestsPerSecond = 20.0f;
	/** 每连接 token bucket 的最大突发容量。 */
	UPROPERTY(EditAnywhere, Category="Combat|Network|Limits", meta=(ClampMin="1", DisplayName="突发请求容量", ToolTip="每个连接 token bucket 允许积累的最大 token 数。"))
	float BurstCapacity = 32.0f;
	/** 每连接保存的最近请求 ID 数量。 */
	UPROPERTY(EditAnywhere, Category="Combat|Network|Limits", meta=(ClampMin="8", ClampMax="4096", DisplayName="请求 ID 重放窗口", ToolTip="每个连接保存并拒绝重复使用的最近请求 ID 数量。"))
	int32 ReplayWindowSize = 128;

private:
	/** 单个 owning connection 的 token bucket 与重放窗口。 */
	struct FConnectionGuardState
	{
		double Tokens = 0.0;
		double LastRefillTime = 0.0;
		TSet<int32> RecentRequestIds;
		TArray<int32> RequestIdOrder;
		bool bInitialized = false;
	};

	/** 估算固定 schema 的网络载荷大小，用于在反序列化后执行第二道包上限。 */
	static int32 EstimatePayloadBytes(const FCombatOrderBatchRequest& Request);
	/** 将请求 ID 加入有界窗口并淘汰最旧记录。 */
	void RememberRequestId(FConnectionGuardState& State, int32 RequestId);
	/** 更新拒绝分类并写入结构化 Combat Event。 */
	void RecordRejection(
		APlayerController* RequestingController,
		ACombatUnitCharacter* Unit,
		const FCombatOrderBatchRequest& Request,
		FGameplayTag FailureTag,
		const FString& Diagnostic);
	/** 移除已经销毁的连接键。 */
	void PruneInvalidConnections();

	/** 每个 PlayerController 对应唯一安全状态。 */
	TMap<TWeakObjectPtr<APlayerController>, FConnectionGuardState> ConnectionStates;
	/** World 生命周期内累计的安全计数。 */
	FCombatRpcSecurityStats Stats;
};
