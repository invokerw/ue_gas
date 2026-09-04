#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Network/CombatNetworkTypes.h"

#include "CombatNetworkSecuritySubsystem.generated.h"

class ACombatUnitCharacter;
class APlayerController;

/**
 * 客户端指令 RPC 的服务器安全检查。以指挥玩家的 PlayerController 为键保存请求额度和最近请求 ID，检查单位所有权、正请求 ID、批次数量、估算载荷、重复请求与发送频率。
 * 通过后才允许逐项提交业务指令；这不代表每条指令都合法或执行成功。
 */
UCLASS()
class UE_GAS_API UCombatNetworkSecuritySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 全部安全检查通过后消耗该玩家 1 个批次额度并记住请求 ID；返回 true 才能继续处理业务指令。检查失败时返回原因标签与诊断，不执行任何指令。 */
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
	/** 每个玩家每秒补充的请求额度，1 个额度允许提交 1 个批次，与批次中的指令条数无关；运行时至少按 1 计算。 */
	UPROPERTY(EditAnywhere, Category="Combat|Network|Limits", meta=(ClampMin="1", DisplayName="每秒请求额度", ToolTip="每个玩家每秒补充的请求额度，1 个额度允许提交 1 个批次，与批次中的指令条数无关；运行时至少按 1 计算。"))
	float RequestsPerSecond = 20.0f;
	/** 每个玩家最多积攒的请求额度，也作为首次请求的初始额度；运行时至少按 1 计算。 */
	UPROPERTY(EditAnywhere, Category="Combat|Network|Limits", meta=(ClampMin="1", DisplayName="突发请求容量", ToolTip="每个玩家最多积攒的请求额度，也作为首次请求的初始额度；运行时至少按 1 计算。"))
	float BurstCapacity = 32.0f;
	/** 每个玩家保留的最近已接受请求 ID 数，运行时至少保留 8 个；被淘汰的旧 ID 不再由此窗口识别为重复。 */
	UPROPERTY(EditAnywhere, Category="Combat|Network|Limits", meta=(ClampMin="8", ClampMax="4096", DisplayName="请求 ID 重放窗口", ToolTip="每个玩家保留的最近已接受请求 ID 数，运行时至少保留 8 个；被淘汰的旧 ID 不再由此窗口识别为重复。"))
	int32 ReplayWindowSize = 128;

private:
	/** 一个指挥玩家的剩余批次额度、上次补充时间及最近已接受请求 ID。 */
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
