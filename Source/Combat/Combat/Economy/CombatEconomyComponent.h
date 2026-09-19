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

/**
 * PlayerController 持有的服务器权威经济状态。金币跟随连接，购买和出售都直接作用于
 * 当前主控单位九格库存；经济组件不再维护第二个交易容器。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent, DisplayName="战斗经济组件", ToolTip="管理本局单一金币、商店事务和当前主控单位物品栏交付。"))
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

	/** 在当前主控单位库存域中购买目标、消费未锁定组件并原子生成最终实例。 */
	bool PurchaseItem(const FPrimaryAssetId& ItemDefinitionId, int32 ExpectedEconomyRevision,
		int32 ExpectedInventoryRevision, FCombatItemHandle& OutResultHandle, FGameplayTag& OutFailure);
	/** 出售当前主控单位库存中的精确实例；沿用配置的全额退款窗口与折扣值。 */
	bool SellInventoryItem(FCombatItemHandle ItemHandle, int32 ExpectedItemRevision,
		int32 ExpectedEconomyRevision, int32 ExpectedInventoryRevision, FGameplayTag& OutFailure);
	/** 切换当前主控单位库存实例的锁定状态；解锁后立即检测并稳定当前库存可用的合成。 */
	bool ToggleInventoryItemLock(FCombatItemHandle ItemHandle, int32 ExpectedItemRevision,
		int32 ExpectedEconomyRevision, int32 ExpectedInventoryRevision, bool& OutLocked, FGameplayTag& OutFailure);
	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="获取金币"))
	int64 GetGold() const { return ReplicatedView.Gold; }
	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="获取经济修订"))
	int32 GetEconomyRevision() const { return ReplicatedView.EconomyRevision; }
	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="获取物品栏修订", ToolTip="返回当前主控单位九槽物品栏的服务器投影修订。"))
	int32 GetInventoryRevision() const { return ReplicatedView.InventoryRevision; }
	/** 返回当前主控单位库存实例的当前修订；不存在或不属于该单位时返回 0。 */
	int32 FindInventoryItemRevision(FCombatItemHandle Handle) const;
	/** 返回拥有者只读快照。 */
	const FCombatEconomyView& GetEconomyView() const { return ReplicatedView; }
	/** 主控单位绑定或库存变化后刷新 owner-only 经济/库存投影；不改变任何权威状态。 */
	void RefreshInventoryProjection();
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
	UCombatInventoryComponent* GetCommandedInventory() const;
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
	void RefreshView();
	void EmitEconomyEvent(FGameplayTag EventType, const FCombatSourceContext& Source,
		FName Action, int64 PreviousGold) const;
	void AdvanceEconomyRevision();
	static void AdvanceRevision(int32& Revision);

	UFUNCTION()
	void OnRep_EconomyView();

	/** 仅向 PlayerController 的 owning connection 复制的完整经济投影。 */
	UPROPERTY(ReplicatedUsing=OnRep_EconomyView, Transient)
	FCombatEconomyView ReplicatedView;
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
