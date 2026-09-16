#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/Items/CombatItemTypes.h"
#include "CombatItemSubsystem.generated.h"

class UCombatItemData;
class ACombatUnitCharacter;
class ACombatWorldItem;
class UCombatInventoryComponent;
class UCombatEconomyComponent;
class ACombatPlayerController;

/** 仅服务器登记表持有的物品实例；没有可编辑属性，也不复制 UObject 指针。 */
UCLASS()
class COMBAT_API UCombatItemInstance : public UObject
{
	GENERATED_BODY()
public:
	FCombatItemHandle GetHandle() const { return Handle; }
	const UCombatItemData* GetDefinition() const { return Definition; }
	int32 GetRevision() const { return Revision; }
	int32 GetQuantity() const { return Quantity; }
	int32 GetCharges() const { return Charges; }
	int32 GetSlot() const { return Slot; }
	ACombatUnitCharacter* GetHolder() const { return Holder.Get(); }
	ACombatWorldItem* GetWorldActor() const { return WorldActor.Get(); }
	ACombatPlayerController* GetStashOwner() const { return StashOwner.Get(); }
	int32 GetStashSlot() const { return StashSlot; }
	FGameplayAbilitySpecHandle GetAbilityHandle() const { return AbilityHandle; }
	/** 读取按位置速率推进的冷却余额，暂停世界时不会消耗。 */
	float GetCooldownRemaining(double Now) const;
	float GetCooldownDuration() const { return CooldownDuration; }
	float GetCooldownRate() const { return CooldownRate; }
	/** 生成不可变来源快照，消耗末件后仍可用于后续弹体与效果。 */
	FCombatSourceContext MakeSource() const;

private:
	friend class UCombatItemSubsystem;
	friend class UCombatInventoryComponent;
	friend class UCombatEconomyComponent;
	UPROPERTY(Transient) TObjectPtr<UCombatItemData> Definition;
	FCombatItemHandle Handle;
	TWeakObjectPtr<ACombatUnitCharacter> Holder;
	/** 储藏处与英雄/地面互斥的玩家级所有者；仅经济组件修改。 */
	TWeakObjectPtr<ACombatPlayerController> StashOwner;
	TWeakObjectPtr<ACombatWorldItem> WorldActor;
	TWeakObjectPtr<ACombatUnitCharacter> BoundUnit;
	/** 绑定不会因原单位 EndPlay 导致弱引用失效而被清除。 */
	bool bBoundUnitAssigned = false;
	FCombatTeamId BoundTeam;
	int32 Revision = 1;
	int32 Quantity = 1;
	int32 Charges = 0;
	int32 Slot = INDEX_NONE;
	int32 StashSlot = INDEX_NONE;
	FGameplayAbilitySpecHandle AbilityHandle;
	/** 各被动以定义列表下标持有精确句柄；临时禁用时清空。 */
	TMap<int32, FCombatModifierHandle> PassiveHandles;
	FCombatAuraHandle AuraHandle;
	double CooldownCheckpoint = 0.0;
	float CooldownRemaining = 0.0f;
	float CooldownDuration = 0.0f;
	float CooldownRate = 1.0f;
	/** 背包曾经抑制此物品；通过地面中转也必须补足重新装备等待。 */
	bool bNeedsReequipDelay = false;
	double EnabledAt = 0.0;
	/** 当前未使用购买链实际支付的金币；免费来源或使用后为 0/不可退款。 */
	int64 PurchasePaidGold = 0;
	double PurchaseWorldTime = 0.0;
	bool bRefundEligible = false;
};

/**
 * 一个游戏世界内的物品唯一登记表。持有单位组件只管理槽位，地面 Actor 只投影这里的实例。
 * 创建、销毁、数量与位置变更均只在服务器发生，世界结束后清空全部记录并使旧句柄失效。
 */
UCLASS()
class COMBAT_API UCombatItemSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	/** 按完整句柄查询本世界实例；销毁、合并或旧代次返回空。 */
	const UCombatItemInstance* FindItem(FCombatItemHandle Handle) const;
	/** 服务器向场景投放新的定义实例；场景预放 Actor 通过相同登记入口初始化。 */
	FCombatItemHandle SpawnItem(UCombatItemData* Definition, int32 Quantity, const FVector& Location);
	/** 预放 Actor 的 BeginPlay 入口；运行时生成的投影已带句柄，不重复登记。 */
	bool RegisterPlacedItem(ACombatWorldItem& Actor, UCombatItemData* Definition, int32 Quantity);
	/** 地面投影意外退出时释放仍由该 Actor 持有的实例；拾取销毁投影不删除已转移记录。 */
	void NotifyWorldActorEndPlay(ACombatWorldItem& Actor);
	int32 GetInstanceCount() const { return Instances.Num(); }
	bool IsShuttingDown() const { return bShuttingDown; }

private:
	friend class UCombatInventoryComponent;
	friend class UCombatEconomyComponent;
	/** 创建尚未分配位置的实例；只允许同一同步事务立即接管或销毁。 */
	UCombatItemInstance* CreateItem(UCombatItemData* Definition, int32 Quantity);
	/** 生成地面投影后再修改持有关系，失败时原持有者保持完整。 */
	ACombatWorldItem* CreateWorldActor(UCombatItemInstance& Item, const FVector& Location);
	/** 从唯一登记表解除实例；调用方先清理授予和地面投影。 */
	void DestroyItem(FCombatItemHandle Handle);
	UCombatItemInstance* FindMutable(FCombatItemHandle Handle) const;
	UPROPERTY(Transient) TMap<uint64, TObjectPtr<UCombatItemInstance>> Instances;
	uint64 NextId = 1;
	uint32 Generation = 0;
	bool bShuttingDown = false;
};
