#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/Economy/CombatEconomyTypes.h"
#include "Combat/Items/CombatItemTypes.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "CombatEconomyNetworkScenario.generated.h"

class ACombatPlayerController;
struct FCombatItemView;

/**
 * Demo PIE/Dedicated 经济验证编排器。
 * 只通过 owning PlayerController 的公开购买、锁定、解锁和出售入口发请求，
 * 用真实复制快照确认库存直达、锁定过滤、解锁合成和退款闭环；默认只跑一轮，
 * `-CombatEconomySoak` 会每 30 秒重复闭环、持续 300 秒，不改变生产经济语义。
 */
UCLASS(NotBlueprintable)
class COMBAT_API ACombatEconomyNetworkScenario : public AActor
{
	GENERATED_BODY()

public:
	ACombatEconomyNetworkScenario();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 接收当前验证动作的权威回执；旧修订无副作用时等待投影后有限重试。 */
	UFUNCTION()
	void ReceiveEconomyResult(FCombatEconomyResult Result);

	/** 从本帧拥有者快照查找稳定实例，返回值不得跨帧持有。 */
	const FCombatItemView* FindItem(const FCombatItemHandle& Handle) const;
	/** 按定义查找物品，合成检查时排除场景启动前已有的结果实例。 */
	const FCombatItemView* FindDefinition(FName DefinitionName, bool bExcludeInitialBlades = false) const;
	/** 等待主控单位、经济规则和九槽投影完成接线。 */
	bool IsReady() const;
	/** 以失败结束本端场景并保存可由外部脚本读取的报告。 */
	void Fail(const TCHAR* Detail);
	/** 成功完成闭环或 soak 后保存本端报告。 */
	void Finish();
	/** 报告写入 Saved，命令行报告名用于区分两个客户端。 */
	void SaveReport() const;
	/** 通过 owning Controller 发送购买意图，不直接写库存或金币。 */
	void SendPurchase(const FPrimaryAssetId& DefinitionId);
	/** 使用本帧实例修订提交锁定切换。 */
	void SendToggle(const FCombatItemHandle& Handle);
	/** 通过公开出售入口提交新合成结果。 */
	void SendSell(const FCombatItemView& Item);

	/** 本帧拥有者值快照；查找返回的指针仅在下一帧刷新前有效。 */
	FCombatHUDOwnerView OwnerSnapshot;
	TWeakObjectPtr<ACombatPlayerController> Controller;
	FCombatItemHandle LockedComponent;
	FCombatItemHandle PurchasedComponent;
	FCombatItemHandle SecondaryPurchasedComponent;
	FCombatItemHandle CraftedBlade;
	FPrimaryAssetId PurchaseDefinitionId;
	TSet<FCombatItemHandle> InitialBlades;
	FCombatEconomyResult LastResult;
	float Elapsed = 0.0f;
	float CycleStarted = 0.0f;
	float NextCycleAt = 0.0f;
	int32 Step = 0;
	int32 AwaitingAction = 0;
	int32 CompletedCycles = 0;
	int32 ResultCount = 0;
	int32 FailureCount = 0;
	int32 StaleRetryCount = 0;
	int32 TotalStaleRetries = 0;
	int64 CycleGoldDelta = 0;
	bool bHasResult = false;
	bool bFinished = false;
	bool bSoak = false;
	bool bBoundResultDelegate = false;
	bool bCapturedInitialBlades = false;
	bool bSawPurchase = false;
	bool bSawSale = false;
};
