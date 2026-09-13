#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Log/CombatLogTypes.h"
#include "CombatLogComponent.generated.h"

class ACombatUnitCharacter;
class UCombatEventSubsystem;
struct FCombatLogRecord;
struct FCombatLogResourceChange;

/** 当前连接的完整历史批次可读取时通知本地界面。 */
DECLARE_MULTICAST_DELEGATE(FOnCombatLogHistoryChanged);

/**
 * PlayerController 的只读战斗记录投影。服务器订阅已完成事件，按网络相关性筛选后只向拥有者复制有界历史。
 * 客户端不提供写入 RPC；组件销毁时解绑事件并清空弱单位缓存，UI 生命周期不影响记录。
 */
UCLASS(meta=(DisplayName="玩家战斗记录", ToolTip="服务器生成、仅向本玩家复制的战斗历史；不参与战斗结算。"))
class COMBAT_API UCombatLogComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCombatLogComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** 返回按提交顺序排列的本连接历史；最多 512 条，无 Actor 强引用。 */
	const TArray<FCombatLogEntry>& GetEntries() const { return OrderedEntries; }
	/** 用于本地 UMG 幂等订阅历史变化。 */
	FOnCombatLogHistoryChanged& OnHistoryChanged() { return HistoryChanged; }
	/** FastArray 或服务器追加完成后重建只读排序索引并通知观察者。 */
	void NotifyHistoryChanged();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** 只接受服务器已提交事件，过滤内部诊断并冻结显示所需身份与结果。 */
	void HandleRecord(const FCombatLogRecord& Record, const FCombatLogResourceChange& ResourceChange);
	/** 将服务器进程内 ID 解析为本 World 弱单位；仅用于生成投影，不发送对象指针。 */
	ACombatUnitCharacter* FindUnit(int32 ActorId);

	UPROPERTY(Replicated) FCombatLogArray History;
	/** 独立于 FastArray 内部索引的只读排序副本，避免客户端删除交换顺序影响 UI。 */
	UPROPERTY(Transient) TArray<FCombatLogEntry> OrderedEntries;
	TWeakObjectPtr<UCombatEventSubsystem> BoundEvents;
	TMap<int32, TWeakObjectPtr<ACombatUnitCharacter>> UnitCache;
	FOnCombatLogHistoryChanged HistoryChanged;
};
