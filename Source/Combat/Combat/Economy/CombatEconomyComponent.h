#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Economy/CombatEconomyTypes.h"
#include "CombatEconomyComponent.generated.h"

class ACombatPlayerController;
class UCombatEconomyData;
class UCombatItemData;
class UCombatItemInstance;
class UCombatItemSubsystem;
class UCombatInventoryComponent;
class UCombatShopData;
class UCombatModifierComponent;

/**
 * PlayerController 持有的服务器权威玩家资源账本。金币和后续战略资源跟随玩家连接；
 * 购买和出售仍直接作用于当前主控英雄的九格库存，经济组件只保存该库存的拥有者投影。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent, DisplayName="战斗经济组件", ToolTip="管理玩家级战略资源、商店事务和当前主控英雄物品栏投影。"))
class COMBAT_API UCombatEconomyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatEconomyComponent();

	/** 使用 GameMode 已校验的关卡规则初始化一次；运行中只保留冻结数值快照。 */
	bool InitializeForMatch(UCombatEconomyData* EconomyData, UCombatShopData* ShopData,
		bool bEnableDebugCommands, FString& OutError);
	/** 服务器增加非负金币并按关卡上限限制；零值是无状态变化的成功操作。 */
	bool AddGold(int64 Amount, FName Reason);
	/** 服务器消费玩家金币；非负金额、当前资源修订和足额余额均满足才提交。拒绝重入；不操作英雄库存。 */
	bool TrySpendGold(int64 Amount, int32 ExpectedRevision, FName Reason);
	/** 仅调试命令使用；越界值拒绝且不静默截断。 */
	bool SetGoldForDebug(int64 Amount, FString& OutError);

	/** 在当前主控单位库存域中购买目标、消费未锁定组件并原子生成最终实例。 */
	bool PurchaseItem(const FPrimaryAssetId& ItemDefinitionId, int32 ExpectedEconomyRevision,
		int32 ExpectedInventoryRevision, FCombatItemHandle& OutResultHandle, FGameplayTag& OutFailure,
		FGuid PurchaseContextId=FGuid(), int32 PurchaseContextRevision=0);
	/** 出售当前主控单位库存中的精确实例；沿用配置的全额退款窗口与折扣值。 */
	bool SellInventoryItem(FCombatItemHandle ItemHandle, int32 ExpectedItemRevision,
		int32 ExpectedEconomyRevision, int32 ExpectedInventoryRevision, FGameplayTag& OutFailure);
	/** 切换当前主控单位库存实例的锁定状态；解锁后立即检测并稳定当前库存可用的合成。 */
	bool ToggleInventoryItemLock(FCombatItemHandle ItemHandle, int32 ExpectedItemRevision,
		int32 ExpectedEconomyRevision, int32 ExpectedInventoryRevision, bool& OutLocked, FGameplayTag& OutFailure);
	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="获取金币"))
	int64 GetGold() const { return ReplicatedView.Gold; }
	/** 返回玩家级战略资源快照；资源归 PlayerController，不随英雄切换或英雄销毁转移。 */
	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="获取玩家资源"))
	FCombatPlayerResourceView GetPlayerResourceView() const;
	/** 返回当前主控英雄库存快照；物品实例仍由该英雄的库存组件持有。 */
	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="获取英雄库存"))
	FCombatHeroInventoryView GetHeroInventoryView() const;
	/** 返回当前有效商店折扣；服务器读取活动 Modifier，客户端读取 owner-only 快照。 */
	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="获取商店折扣", ToolTip="服务器读取当前有效效果，客户端只读拥有者价格快照；总折扣最多20%。"))
	float GetShopDiscountFraction() const;
	/** 对一个已验证的正整数基础费用执行一次封顶与向下取整。 */
	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="计算折后费用", ToolTip="对整笔非负费用一次向下取整，折扣最多20%；负数输入返回0。"))
	int64 ApplyShopDiscount(UPARAM(DisplayName="基础费用") int64 BasePrice) const;
	/** 使用当前拥有者投影构造商品报价，供商店界面显示；服务器购买仍会重新构造权威计划。 */
	bool CalculateCurrentItemPrice(const FPrimaryAssetId& ItemDefinitionId, int64& OutPrice, FString& OutError) const;
	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="获取经济修订"))
	int32 GetEconomyRevision() const { return ReplicatedView.EconomyRevision; }
	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="获取玩家资源修订"))
	int32 GetPlayerResourceRevision() const { return ReplicatedView.EconomyRevision; }
	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="获取物品栏修订", ToolTip="返回当前主控单位九槽物品栏的服务器投影修订。"))
	int32 GetInventoryRevision() const { return ReplicatedView.InventoryRevision; }
	/** 返回当前主控单位库存实例的当前修订；不存在或不属于该单位时返回 0。 */
	int32 FindInventoryItemRevision(FCombatItemHandle Handle) const;
	/** 返回独立 schema1 拥有者价格投影；不是客户端付款输入。 */
	const FCombatShopPriceView& GetShopPriceView() const { return ShopPriceView; }
	/** 返回拥有者只读快照。 */
	const FCombatEconomyView& GetEconomyView() const { return ReplicatedView; }
	/** 主控单位绑定或库存变化后刷新 owner-only 经济/库存投影；不改变任何权威状态。 */
	void RefreshInventoryProjection();
	bool IsInitialized() const { return bInitialized || (ReplicatedView.GoldCap > 0 && ReplicatedView.ShopDefinitionId.IsValid()); }
	bool AreDebugCommandsEnabled() const { return bDebugCommandsEnabled; }
	UCombatShopData* GetShopData() const { return ShopData; }
	/** 只重建商品条件，不改变金币/库存修订。领域阶段变化时调用。 */
	void RefreshShopOffers();
	/** 当前拥有者投影中的指定商品条件；无购买策略的普通商品返回空。 */
	const FCombatShopOfferView* FindShopOffer(FPrimaryAssetId Id) const;
	/** 是否为当前目录的获得即用商品，客户端不能自行指定。 */
	bool IsAcquirePurchase(const FCombatEconomyRequest& Request) const;
    /** 是否使用领域条件与完整回执；包括即用商品和可选普通商品购买策略。 */
    bool IsContextPurchase(const FCombatEconomyRequest& Request) const;
	/** 检查领域商品完整回执：0 未缓存、1 同载荷、-1 换载荷。 */
	int32 InspectAcquireReplay(const FCombatEconomyRequest& Request) const;
	/** 读取缓存回执，调用方先完成当前连接/绑定/上下文验证。 */
	FCombatEconomyResult GetAcquireReplay(int32 RequestId) const;
	/** 记住终态回执，最多 32 条；高水位淘汰旧号。 */
	void RememberAcquireResult(const FCombatEconomyRequest& Request,const FCombatEconomyResult& Result);
	int32 GetAcquireReceiptCount() const { return AcquireReceipts.Num(); }
	int32 GetAcquireRequestHighWater() const { return AcquireRequestHighWater; }


	UPROPERTY(BlueprintAssignable, Category="Combat|Economy", meta=(DisplayName="经济界面变化"))
	FCombatEconomyViewChangedDelegate OnEconomyViewChanged;
	UPROPERTY(BlueprintAssignable, Category="Combat|Economy", meta=(DisplayName="玩家资源变化"))
	FCombatPlayerResourceViewChangedDelegate OnPlayerResourceViewChanged;
	UPROPERTY(BlueprintAssignable, Category="Combat|Economy", meta=(DisplayName="英雄库存变化"))
	FCombatHeroInventoryViewChangedDelegate OnHeroInventoryViewChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class UCombatInventoryComponent;

	struct FConsumedEntry
	{
		UCombatItemInstance* Item = nullptr;
		int32 Quantity = 0;
	};

	/** 在原经济互斥范围内购买并同步消费即用物品；失败恢复金币和修订。 */
	bool PurchaseAcquireItem(UCombatItemData& Item,int32 EconomyRevision,int32 InventoryRevision,FGuid ContextId,int32 ContextRevision,FGameplayTag& Failure);
	ACombatPlayerController* GetCombatPlayer() const;
	UCombatItemSubsystem* GetItems() const;
	UCombatInventoryComponent* GetCommandedInventory() const;
	/** 从当前主控英雄的有效效果读取折扣，不缓存 gameplay 真值。 */
	float ResolveAuthoritativeShopDiscountFraction() const;
	/** 切换观察源时先解绑旧英雄；只订阅当前主控的效果集合。 */
	void BindShopPriceSource();
	/** 效果变化后重建独立价格投影；事务内推迟到最终 RefreshView。 */
	void RefreshShopPriceProjection();
	/** 服务器使用本局冻结目录；客户端按服务器目录身份加载只读资产。 */
	const UCombatShopData* ResolvePriceCatalog() const;
	/** 按拥有者库存数量展开未锁定定义，供只读配方补购报价使用。 */
	void BuildProjectedOwnedDefinitions(TArray<FPrimaryAssetId>& OutDefinitions) const;
	bool ValidateInventoryTransactionRevisions(int32 ExpectedEconomyRevision, int32 ExpectedInventoryRevision,
		FGameplayTag& OutFailure, UCombatInventoryComponent*& OutInventory) const;
	bool ResolveInventoryConsumedEntries(UCombatInventoryComponent& Inventory,
		const TArray<FPrimaryAssetId>& Definitions, TArray<FConsumedEntry>& OutEntries,
		FGameplayTag& OutFailure) const;
	bool CommitInventoryResult(UCombatInventoryComponent& Inventory, UCombatItemData& TargetDefinition,
		const TArray<FConsumedEntry>& Consumed, int64 AdditionalPaidGold,
		FCombatItemHandle& OutHandle, FGameplayTag& OutFailure, const TCHAR* Action);
	/** 在一个英雄的九格库存域内按稳定优先级自动合成；购买、接管或解锁的顶层修改结束后调用。 */
	void StabilizeInventoryCrafting(UCombatInventoryComponent& Inventory, FCombatItemHandle& InOutTrackedHandle);
	bool TryCraftInventoryRecipe(UCombatInventoryComponent& Inventory, UCombatItemData& TargetDefinition,
		FCombatItemHandle& OutHandle);
	void StartPassiveIncome();
	/** 按调度器提供的 World Game Time 补齐绝对累计收入；无上下文时回退到当前 World 时间。 */
	void ApplyPassiveIncome(double CurrentGameTime = -1.0);
	void RefreshView(bool bResourceChanged = true, bool bInventoryChanged = true);
	void EmitEconomyEvent(FGameplayTag EventType, const FCombatSourceContext& Source,
		FName Action, int64 PreviousGold) const;
	void AdvanceEconomyRevision();
	static void AdvanceRevision(int32& Revision);

	UFUNCTION()
	void OnRep_EconomyView();
	UFUNCTION()
	void OnRep_ShopPriceView();

	/** 仅向 PlayerController 的 owning connection 复制的完整经济投影。 */
	UPROPERTY(ReplicatedUsing=OnRep_EconomyView, Transient)
	FCombatEconomyView ReplicatedView;
	/** 独立的拥有者价格字段，保持冻结经济聚合结构不变。 */
	UPROPERTY(ReplicatedUsing=OnRep_ShopPriceView, Transient)
	FCombatShopPriceView ShopPriceView;
	/** 观察者不持有英雄生命；换主控和 EndPlay 显式解绑。 */
	TWeakObjectPtr<UCombatModifierComponent> ShopPriceSource;
	FDelegateHandle ShopPriceChangedHandle;
	UPROPERTY(Transient) TObjectPtr<UCombatShopData> ShopData;
	UPROPERTY(Transient) TArray<TObjectPtr<UCombatItemData>> FrozenOfferItems;
	TArray<FCombatAcquirePurchaseReceipt> AcquireReceipts;
	int32 AcquireRequestHighWater=0;
	int64 FrozenGoldCap = 0;
	int64 FrozenPassiveGoldPerMinute = 0;
	float FrozenFullRefundSeconds = 0.0f;
	int32 FrozenSellValueBasisPoints = 0;
	double PassiveOriginTime = 0.0;
	int64 CreditedPassiveGold = 0;
	FCombatScheduleHandle PassiveIncomeSchedule;
	bool bInitialized = false;
	bool bDebugCommandsEnabled = false;
	bool bMutating = false;
	bool bEnding = false;
};
