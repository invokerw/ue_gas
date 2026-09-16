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
class UCombatShopData;

/**
 * PlayerController 持有的服务器权威经济状态。金币与六格储藏处不绑定当前英雄生命，
 * 客户端只接收拥有者快照并通过 PlayerController RPC 提交购买、出售和转移意图。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent, DisplayName="战斗经济组件", ToolTip="管理本局单一金币和六格玩家储藏处。"))
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
	/** 仅调试命令使用；越界值拒绝且不静默截断。 */
	bool SetGoldForDebug(int64 Amount, FString& OutError);

	/** 在储藏处域中购买目标、消费已有组件并原子生成最终实例。 */
	bool PurchaseItem(const FPrimaryAssetId& ItemDefinitionId, int32 ExpectedEconomyRevision,
		int32 ExpectedStashRevision, FCombatItemHandle& OutResultHandle, FGameplayTag& OutFailure);
	/** 出售一个精确储藏处实例；新鲜且完整的购买链按实付额退款。 */
	bool SellStashItem(FCombatItemHandle ItemHandle, int32 ExpectedItemRevision,
		int32 ExpectedEconomyRevision, int32 ExpectedStashRevision, FGameplayTag& OutFailure);
	/** 把一个精确储藏实例交给当前存活英雄的九格库存。 */
	bool TransferStashItem(FCombatItemHandle ItemHandle, int32 ExpectedItemRevision,
		int32 ExpectedEconomyRevision, int32 ExpectedStashRevision,
		FCombatItemHandle& OutInventoryHandle, FGameplayTag& OutFailure);
	/** 按储藏槽顺序尽可能多地转入当前存活英雄；至少移动一件才返回成功。 */
	bool TakeAllStashItems(int32 ExpectedEconomyRevision, int32 ExpectedStashRevision,
		int32& OutMovedItemCount, FGameplayTag& OutFailure);

	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="获取金币"))
	int64 GetGold() const { return ReplicatedView.Gold; }
	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="获取经济修订"))
	int32 GetEconomyRevision() const { return ReplicatedView.EconomyRevision; }
	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="获取储藏处修订"))
	int32 GetStashRevision() const { return ReplicatedView.StashRevision; }
	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="获取储藏处物品数量"))
	int32 GetStashItemCount() const;
	/** 返回精确储藏处实例的当前修订；不存在时返回 0。 */
	int32 FindStashItemRevision(FCombatItemHandle Handle) const;
	/** 返回拥有者只读快照。 */
	const FCombatEconomyView& GetEconomyView() const { return ReplicatedView; }
	bool IsInitialized() const { return bInitialized || (ReplicatedView.GoldCap > 0 && ReplicatedView.ShopDefinitionId.IsValid()); }
	bool AreDebugCommandsEnabled() const { return bDebugCommandsEnabled; }
	UCombatShopData* GetShopData() const { return ShopData; }

	UPROPERTY(BlueprintAssignable, Category="Combat|Economy", meta=(DisplayName="经济界面变化"))
	FCombatEconomyViewChangedDelegate OnEconomyViewChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class UCombatInventoryComponent;

	struct FConsumedEntry
	{
		UCombatItemInstance* Item = nullptr;
		int32 Quantity = 0;
	};

	ACombatPlayerController* GetCombatPlayer() const;
	UCombatItemSubsystem* GetItems() const;
	bool ValidateTransactionRevisions(int32 ExpectedEconomyRevision, int32 ExpectedStashRevision,
		FGameplayTag& OutFailure) const;
	bool ResolveConsumedEntries(const TArray<FPrimaryAssetId>& Definitions,
		TArray<FConsumedEntry>& OutEntries, FGameplayTag& OutFailure) const;
	bool TryCraftStashRecipe(UCombatItemData& TargetDefinition, FCombatItemHandle& OutHandle);
	void StabilizeStashCrafting(FCombatItemHandle& InOutLastHandle);
	/** 在一个英雄的九格库存域内按稳定优先级自动合成；仅由库存顶层事务结束后调用。 */
	void StabilizeInventoryCrafting(UCombatInventoryComponent& Inventory, FCombatItemHandle& InOutTrackedHandle);
	bool TryCraftInventoryRecipe(UCombatInventoryComponent& Inventory, UCombatItemData& TargetDefinition,
		FCombatItemHandle& OutHandle);
	void StartPassiveIncome();
	/** 按调度器提供的 World Game Time 补齐绝对累计收入；无上下文时回退到当前 World 时间。 */
	void ApplyPassiveIncome(double CurrentGameTime = -1.0);
	void ClearStash();
	void RefreshView();
	void EmitEconomyEvent(FGameplayTag EventType, const FCombatSourceContext& Source,
		FName Action, int64 PreviousGold) const;
	void AdvanceEconomyRevision();
	void AdvanceStashRevision();
	static void AdvanceRevision(int32& Revision);

	UFUNCTION()
	void OnRep_EconomyView();

	/** 仅向 PlayerController 的 owning connection 复制的完整经济投影。 */
	UPROPERTY(ReplicatedUsing=OnRep_EconomyView, Transient)
	FCombatEconomyView ReplicatedView;
	/** 服务器储藏槽；空槽保存无效 Handle。 */
	TArray<FCombatItemHandle> StashSlots;
	UPROPERTY(Transient) TObjectPtr<UCombatShopData> ShopData;
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
