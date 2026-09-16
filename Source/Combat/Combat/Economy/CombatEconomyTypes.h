#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Combat/Items/CombatItemTypes.h"
#include "CombatEconomyTypes.generated.h"

namespace CombatEconomy
{
	inline constexpr int32 StashSlots = 6;
}

/** 客户端可以向服务器提交的经济意图；所有价格和结果均由服务器重新计算。 */
UENUM(BlueprintType)
enum class ECombatEconomyAction : uint8
{
	Purchase UMETA(DisplayName="购买"),
	SellStashItem UMETA(DisplayName="出售储藏处物品"),
	TransferStashItem UMETA(DisplayName="取出储藏处物品"),
	TakeAllStashItems UMETA(DisplayName="全部取出")
};

/** 一次经济 RPC 的有界意图，不携带客户端价格或余额。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatEconomyRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="请求 ID", ToolTip="PlayerController 连接内单调递增的正整数，与 Order 共用重放窗口。"))
	int32 RequestId = 0;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="控制绑定代次", ToolTip="拒绝控制权切换前生成的旧储藏处转移请求。"))
	int32 CommandBindingGeneration = 0;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="期望经济修订", ToolTip="提交时观察到的金币修订；不匹配时拒绝陈旧请求。"))
	int32 ExpectedEconomyRevision = 0;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="期望储藏修订", ToolTip="提交时观察到的储藏处修订；不匹配时拒绝陈旧请求。"))
	int32 ExpectedStashRevision = 0;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="动作", ToolTip="购买、出售、取出单件或全部取出的服务器意图。"))
	ECombatEconomyAction Action = ECombatEconomyAction::Purchase;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="物品定义", ToolTip="购买目标的稳定物品 ID；其他动作可为空。"))
	FPrimaryAssetId ItemDefinitionId;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="物品实例", ToolTip="出售或取出时的精确储藏处实例。"))
	FCombatItemHandle ItemHandle;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="物品修订", ToolTip="出售或取出时观察到的实例修订。"))
	int32 ItemRevision = 0;
};

/** 一次服务器经济事务的最终结果。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatEconomyResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="请求 ID", ToolTip="对应客户端请求 ID。")) int32 RequestId = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="控制绑定代次", ToolTip="请求提交时的控制绑定代次；客户端用它淘汰旧回执。")) int32 CommandBindingGeneration = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="成功", ToolTip="服务器是否完整提交了该事务。")) bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="失败标签", ToolTip="失败时的稳定原因；成功时为空。")) FGameplayTag FailureTag;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="物品定义", ToolTip="购买目标或结果的稳定定义 ID。")) FPrimaryAssetId ItemDefinitionId;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="结果实例", ToolTip="购买或转移成功后的稳定物品句柄；不适用时为空。")) FCombatItemHandle ItemHandle;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="金币变化", ToolTip="本次事务造成的金币差值；购买为负、出售为正。")) int64 GoldDelta = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="移动数量", ToolTip="全部取出时成功移动的实例数，其他操作通常为 0 或 1。")) int32 MovedItemCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="经济修订", ToolTip="事务结束后的服务器经济修订。")) int32 EconomyRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="储藏修订", ToolTip="事务结束后的服务器储藏处修订。")) int32 StashRevision = 0;
};

/** 仅拥有者复制的金币、关卡规则和六格储藏处快照。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatEconomyView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="金币", ToolTip="服务器权威的单一金币余额。")) int64 Gold = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="金币上限", ToolTip="当前关卡在对局开始时冻结的金币上限。")) int64 GoldCap = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="每分钟被动金币", ToolTip="当前关卡在对局开始时冻结的每分钟被动收入。")) int64 PassiveGoldPerMinute = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="经济修订", ToolTip="金币变化时单调递增并跳过 0。")) int32 EconomyRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="储藏修订", ToolTip="储藏内容变化时单调递增并跳过 0。")) int32 StashRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="商店目录", ToolTip="当前关卡唯一商店的稳定定义 ID，客户端据此加载只读目录。")) FPrimaryAssetId ShopDefinitionId;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="储藏处物品", ToolTip="固定六槽拥有者快照；空槽为无效物品。")) TArray<FCombatItemView> StashItems;

	bool operator==(const FCombatEconomyView& Other) const;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCombatEconomyViewChangedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatEconomyResultDelegate, FCombatEconomyResult, Result);
