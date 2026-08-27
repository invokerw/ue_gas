#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Combat/Order/CombatOrderTypes.h"

#include "CombatNetworkTypes.generated.h"

/** 客户端向一个已拥有 Unit 提交的有界 Order 批次。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatOrderBatchRequest
{
	GENERATED_BODY()

	/** 每个连接重放窗口内必须唯一的非零请求 ID。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Network", meta=(DisplayName="请求 ID", ToolTip="同一连接最近窗口内必须唯一且非零；重复 ID 不会再次执行命令。"))
	int32 RequestId = 0;

	/** true 时首个命令追加到当前队列；false 时首个命令替换旧行为。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Network", meta=(DisplayName="追加到现有队列", ToolTip="启用后首个命令也加入现有队列；关闭时首个命令替换当前行为。"))
	bool bAppendToExistingQueue = false;

	/** 按数组顺序提交给同一个 Unit 的命令，数量受服务器单包上限约束。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Network", meta=(DisplayName="命令列表", ToolTip="同一战斗单位的一组命令；服务器会逐项重新校验。"))
	TArray<FCombatOrderRequest> Orders;
};

/** Order RPC 的服务器接收结果与逐项命令结果。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatOrderBatchResult
{
	GENERATED_BODY()

	/** 对应原请求 ID。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Network", meta=(DisplayName="请求 ID", ToolTip="对应客户端提交的请求 ID。"))
	int32 RequestId = 0;

	/** 安全层是否接受该批次；逐项 Order 仍可能被业务规则拒绝。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Network", meta=(DisplayName="批次已接受", ToolTip="表示请求通过所有权、载荷、重放和限频校验。"))
	bool bAccepted = false;

	/** 实际由 OrderComponent 接受的命令数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Network", meta=(DisplayName="接受命令数", ToolTip="逐项业务校验后成功进入 OrderComponent 的数量。"))
	int32 AcceptedOrderCount = 0;

	/** 安全层拒绝时的稳定原因。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Network", meta=(DisplayName="失败标签", ToolTip="请求被安全层拒绝时的稳定 Native GameplayTag。"))
	FGameplayTag FailureTag;

	/** 每个命令的业务结果；安全层拒绝时为空。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Network", meta=(DisplayName="命令结果", ToolTip="与输入命令顺序一致的服务器业务结果。"))
	TArray<FCombatOrderResult> OrderResults;
};

/** World 内累计的 Order RPC 安全统计。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatRpcSecurityStats
{
	GENERATED_BODY()

	/** 已通过安全层并进入业务处理的批次数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Network", meta=(DisplayName="已接受请求数", ToolTip="通过全部网络安全校验并进入业务处理的批次数。")) int64 AcceptedRequests = 0;
	/** 所有安全拒绝请求总数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Network", meta=(DisplayName="已拒绝请求数", ToolTip="网络安全层拒绝的请求总数。")) int64 RejectedRequests = 0;
	/** 所有权拒绝次数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Network", meta=(DisplayName="所有权拒绝数", ToolTip="请求连接不拥有目标单位而被拒绝的次数。")) int64 OwnershipRejects = 0;
	/** 限频拒绝次数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Network", meta=(DisplayName="限频拒绝数", ToolTip="请求超过连接 token bucket 配额而被拒绝的次数。")) int64 RateLimitRejects = 0;
	/** 单包数量或载荷拒绝次数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Network", meta=(DisplayName="载荷拒绝数", ToolTip="命令数量或估算载荷超过单包上限而被拒绝的次数。")) int64 PayloadRejects = 0;
	/** 重复请求 ID 拒绝次数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Network", meta=(DisplayName="重放拒绝数", ToolTip="请求 ID 重复而被拒绝的次数。")) int64 ReplayRejects = 0;
};

/** owning client 收到 Order 批次结果时广播。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatOrderBatchResultDelegate, FCombatOrderBatchResult, Result);
