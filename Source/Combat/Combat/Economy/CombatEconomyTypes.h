#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Combat/Items/CombatItemTypes.h"
#include "Combat/Items/CombatItemAcquireEffect.h"
#include "CombatEconomyTypes.generated.h"

/** 客户端可以向服务器提交的经济意图；所有价格和结果均由服务器重新计算。 */
UENUM(BlueprintType)
enum class ECombatEconomyAction : uint8
{
	Purchase UMETA(DisplayName="购买"),
	SellInventoryItem UMETA(DisplayName="出售物品栏物品"),
	ToggleInventoryItemLock UMETA(DisplayName="切换物品锁定")
};

	/** 一次经济 RPC 的有界意图，不携带客户端价格或余额。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatEconomyRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="请求 ID", ToolTip="PlayerController 连接内单调递增的正整数，与 Order 共用重放窗口。"))
	int32 RequestId = 0;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="控制绑定代次", ToolTip="拒绝控制权切换前生成的旧库存请求。"))
	int32 CommandBindingGeneration = 0;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="期望经济修订", ToolTip="提交时观察到的金币修订；不匹配时拒绝陈旧请求。"))
	int32 ExpectedEconomyRevision = 0;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="期望物品栏修订", ToolTip="提交时观察到的当前主控单位物品栏修订；购买、出售和锁定不匹配时拒绝陈旧请求。"))
	int32 ExpectedInventoryRevision = 0;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="动作", ToolTip="购买、库存出售或锁定切换的服务器意图。"))
	ECombatEconomyAction Action = ECombatEconomyAction::Purchase;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="物品定义", ToolTip="购买目标的稳定物品 ID；其他动作可为空。"))
	FPrimaryAssetId ItemDefinitionId;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="物品实例", ToolTip="出售或锁定时的精确物品栏实例。"))
	FCombatItemHandle ItemHandle;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="物品修订", ToolTip="出售或锁定时观察到的物品栏实例修订。"))
	int32 ItemRevision = 0;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="商品上下文", ToolTip="即用或带购买策略商品的领域上下文ID，取自服务器商店投影；无策略普通商品为空。")) FGuid PurchaseContextId;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Economy", meta=(DisplayName="商品条件修订", ToolTip="提交时看到的领域条件修订；旧阶段或限购状态会拒绝。")) int32 PurchaseContextRevision=0;
	/** 完整载荷比较，不允许同号换商品、动作或任何修订。 */
	bool operator==(const FCombatEconomyRequest& Other) const;
};

/** 一次服务器经济事务的最终结果。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatEconomyResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="获得即用已消耗", ToolTip="true 表示同步效果成功且物品已消耗，结果句柄为空、库存无新增。")) bool bConsumedOnAcquire=false;

	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="请求 ID", ToolTip="对应客户端请求 ID。")) int32 RequestId = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="控制绑定代次", ToolTip="请求提交时的控制绑定代次；客户端用它淘汰旧回执。")) int32 CommandBindingGeneration = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="成功", ToolTip="服务器是否完整提交了该事务。")) bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="失败标签", ToolTip="失败时的稳定原因；成功时为空。")) FGameplayTag FailureTag;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="物品定义", ToolTip="购买目标或结果的稳定定义 ID。")) FPrimaryAssetId ItemDefinitionId;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="结果实例", ToolTip="购买成功后的稳定物品句柄；出售或锁定不适用时为空。")) FCombatItemHandle ItemHandle;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="金币变化", ToolTip="本次事务造成的金币差值；购买为负、出售为正。")) int64 GoldDelta = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="玩家资源修订", ToolTip="事务结束后的服务器玩家资源修订。")) int32 EconomyRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="物品栏修订", ToolTip="事务结束后的当前主控单位物品栏修订。")) int32 InventoryRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="锁定状态", ToolTip="锁定切换成功后的服务器权威状态；其他事务不适用时为 false。")) bool bLocked = false;
};

/** 仅拥有者复制的玩家级战略资源快照。库存不属于该快照。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatPlayerResourceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="金币", ToolTip="服务器权威、绑定到玩家连接而不是某个英雄的金币余额。")) int64 Gold = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="金币上限", ToolTip="当前关卡在对局开始时冻结的玩家资源上限。")) int64 GoldCap = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="每分钟被动金币", ToolTip="当前关卡在对局开始时冻结的玩家级被动收入。")) int64 PassiveGoldPerMinute = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="资源修订", ToolTip="玩家资源变化时单调递增并跳过 0。")) int32 ResourceRevision = 0;
};

/** 仅拥有者复制的当前英雄库存投影；物品实例和 Holder 仍由英雄库存组件持有。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatHeroInventoryView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="物品栏修订", ToolTip="当前英雄库存变化时单调递增并跳过 0。")) int32 InventoryRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="物品栏物品", ToolTip="当前主控英雄九槽拥有者快照；物品实例仍属于英雄。")) TArray<FCombatItemView> Items;
};

/** 独立的拥有者价格投影（schema 1）；只影响本地报价，不能作为服务器付款输入。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatShopPriceView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="报价版本", ToolTip="价格投影独立版本；当前为1，不改变兼容经济聚合快照。")) int32 SchemaVersion = 1;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(ClampMin="0", ClampMax="2000", DisplayName="折扣万分比", ToolTip="服务器当前有效折扣，500表示5%，范围0至2000；只供拥有者显示，不接受客户端修改。")) int32 DiscountBasisPoints = 0;
};

/** 仅拥有者复制的兼容聚合快照；新的代码应按玩家资源和英雄库存分别读取。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatEconomyView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="金币", ToolTip="服务器权威、绑定到玩家而不是英雄的单一金币余额。")) int64 Gold = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="金币上限", ToolTip="当前关卡在对局开始时冻结的金币上限。")) int64 GoldCap = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="每分钟被动金币", ToolTip="当前关卡在对局开始时冻结的每分钟被动收入。")) int64 PassiveGoldPerMinute = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="经济修订", ToolTip="金币变化时单调递增并跳过 0。")) int32 EconomyRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="物品栏修订", ToolTip="当前主控单位物品栏变化时单调递增并跳过 0。")) int32 InventoryRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="商店目录", ToolTip="当前关卡唯一商店的稳定定义 ID，客户端据此加载只读目录。")) FPrimaryAssetId ShopDefinitionId;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="物品栏物品", ToolTip="当前主控英雄九槽拥有者快照；空槽为无效物品，锁定状态随实例复制，物品仍属于英雄。")) TArray<FCombatItemView> InventoryItems;

	UPROPERTY(BlueprintReadOnly, Category="Combat|Shop", meta=(DisplayName="商品购买状态", ToolTip="服务器提供的当前商品可购、限购、领取者与效果到期状态。")) TArray<FCombatShopOfferView> ShopOffers;
	bool operator==(const FCombatEconomyView& Other) const;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCombatEconomyViewChangedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCombatPlayerResourceViewChangedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCombatHeroInventoryViewChangedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatEconomyResultDelegate, FCombatEconomyResult, Result);

/** 仅服务器持有的有界即用购买回执。 */
struct FCombatAcquirePurchaseReceipt
{
	FCombatEconomyRequest Request;
	FCombatEconomyResult Result;
};
