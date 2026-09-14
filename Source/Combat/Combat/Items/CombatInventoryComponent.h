#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Items/CombatItemTypes.h"
#include "CombatInventoryComponent.generated.h"

class UCombatItemData;
class UCombatItemInstance;
class UCombatItemSubsystem;
class ACombatUnitCharacter;

/**
 * 单位的服务器背包。六个装备槽、三个背包槽只保存世界登记表句柄；UI 只读取 UnitView。
 * 同步事务拒绝重入，离开装备时撤销精确被动与光环；主动授予直到离开持有者或最后一次施法结束才清理。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent, DisplayName="战斗物品背包", ToolTip="服务器权威装备与背包；客户端通过指令提交操作。"))
class COMBAT_API UCombatInventoryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCombatInventoryComponent();
	/** 服务器剧情、出生或奖励入口。优先完整合并，再选装备空槽，最后背包；失败不留实例。 */
	bool GiveItem(UCombatItemData* Definition, int32 Quantity, FCombatItemHandle& OutHandle, FGameplayTag& Failure);
	/** 到达后再次校验实例、绑定、距离、视线及容量，完成地面到持有者的原子交接。 */
	bool TryPickup(FCombatItemHandle Handle, int32 ExpectedRevision, FGameplayTag& Failure);
	/** 校验预定落点后才移除原物品；失败仍完整保留装备与主动授予。 */
	bool TryDrop(FCombatItemHandle Handle, int32 ExpectedRevision, const FVector& Location, FGameplayTag& Failure);
	/** 即时交换，不替换当前移动；库存修订和两槽原句柄必须都匹配。 */
	bool TrySwap(int32 From, int32 To, int32 ExpectedInventoryRevision, FCombatItemHandle ExpectedFrom, FCombatItemHandle ExpectedTo, FGameplayTag& Failure);
	/** 不写状态的激活检查；bCheckCost=false 用于同一次施法已经消费末件后的后续阶段。 */
	bool ValidateActive(FCombatItemHandle Handle, bool bCheckCost, FGameplayTag& Failure) const;
	/** 费用阶段预检成功后只提交物品消耗；清理延迟到技能退出调用栈。 */
	void CommitConsumption(FCombatItemHandle Handle);
	/** 保存冻结冷却，并更新同持有者的共享组；速率改变不重置余额。 */
	void CommitCooldown(FCombatItemHandle Handle, float Duration);
	/** 同一提交阶段先发布数量与冷却，再应用法力 GE；回调中死亡或销毁也只能观察到完整提交。 */
	bool CommitActiveStage(FCombatItemHandle Handle, float ManaCost, float Cooldown, bool bConsume, bool bStartCooldown,
		bool& bCostCommitted, bool& bCooldownCommitted, FGameplayTag& Failure);
	/** 主动技能结束后调度末件清理，避免 ClearAbility 重入 GAS 当前栈。 */
	void NotifyAbilityEnded(FCombatItemHandle Handle);
	/** 根据装备位置、等待时间、死亡状态协调精确 Modifier 与 Aura。 */
	void ReconcileEffects();
	/** 死亡取消光环与常驻被动，可按定义掉落；通常保留持有实例和冷却。 */
	void HandleOwnerDeath();
	/** World/Actor teardown 清空全部授予与登记记录，重复调用安全。 */
	void ClearInventory();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	/** 生成九槽拥有者投影，空槽仍保留索引；不复制 DataAsset 指针。 */
	void BuildViews(TArray<FCombatItemView>& Out) const;
	FCombatItemHandle GetItemAt(int32 Slot) const { return Slots.IsValidIndex(Slot) ? Slots[Slot] : FCombatItemHandle(); }
	int32 GetRevision() const { return InventoryRevision; }
	int32 GetItemCount() const;

private:
	/** 新实例或地面实例的共同接管入口，调用前持有事务锁。 */
	bool AcceptItem(UCombatItemInstance& Item, FCombatItemHandle& OutHandle, FGameplayTag& Failure);
	/** 将背包冷却结算到当前时间，并在换位后按新位置设置速率和启用时间。 */
	void ChangeSlot(UCombatItemInstance& Item, int32 Slot);
	/** 撤销这件物品的所有常驻效果和光环，不清除已经发出的独立效果。 */
	void RemoveEffects(UCombatItemInstance& Item);
	/** 释放该实例的槽位、技能和登记记录。必须在当前技能调用栈之外使用。 */
	void RemoveItem(UCombatItemInstance& Item);
	/** 记录结构变化并立即刷新拥有者快照。 */
	void NotifyChanged(UCombatItemInstance* Item, const TCHAR* Action, int32 PreviousQuantity = INDEX_NONE);
	/** 重新装备的最近启用时刻只有一个组件级调度任务，回调检查生命代次。 */
	void ScheduleReconcile();
	/** 比较可完整合并的定义、绑定、能量、等待和冷却状态。 */
	bool CanMerge(const UCombatItemInstance& Into, const UCombatItemInstance& From) const;
	bool IsCasting(const UCombatItemInstance& Item) const;
	ACombatUnitCharacter* GetUnit() const;
	UCombatItemSubsystem* GetItems() const;
	UPROPERTY(Transient) TArray<FCombatItemHandle> Slots;
	int32 InventoryRevision = 1;
	bool bMutating = false;
	bool bEnding = false;
	FCombatScheduleHandle ReconcileSchedule;
};
