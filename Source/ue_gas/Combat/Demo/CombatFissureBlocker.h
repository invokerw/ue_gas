#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"

#include "CombatFissureBlocker.generated.h"

class UBoxComponent;

/** Fissure 第一版物理 blocker：无 Tick、Scheduler 生命周期，并主动通知相交 Order repath。 */
UCLASS()
class UE_GAS_API ACombatFissureBlocker : public AActor
{
	GENERATED_BODY()

public:
	/** 创建复制的 CombatBlocker Box，默认关闭 Tick。 */
	ACombatFissureBlocker();
	/** 在 Authority 上冻结线段几何与持续时间，并通知相关移动路径。 */
	bool InitializeBlocker(
		FVector Start,
		FVector End,
		float HalfWidth,
		float Height,
		float Duration,
		const FCombatEventContext& ParentEvent,
		const FCombatSourceContext& SourceContext);
	/** 返回当前世界空间阻挡包围盒。 */
	FBox GetBlockerBounds() const;
	/** 返回生命周期 Schedule，供自动化确认未使用 Timer。 */
	FCombatScheduleHandle GetLifetimeSchedule() const { return LifetimeSchedule; }

	/** EndPlay 时取消调度并通知 blocker 移除后的 repath。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Scheduler 到期后销毁 Actor；真实清理由 EndPlay 统一执行。 */
	void HandleLifetimeExpired(const FCombatScheduledTickContext& TickContext);
	/** 只通知路径线段与当前 bounds 相交的 Combat Unit。 */
	void NotifyAffectedOrders() const;
	/** 输出创建/移除同一 Event 链的结构化记录。 */
	void EmitBlockerLog(bool bCreated) const;

	/** 使用固定 CombatBlocker Profile 的根碰撞体。 */
	UPROPERTY(VisibleAnywhere, Category="Combat|Fissure") TObjectPtr<UBoxComponent> BlockerCollision;
	/** 生命周期唯一 Scheduler 句柄。 */
	FCombatScheduleHandle LifetimeSchedule;
	/** 继承 Fissure Ability 的事件树和来源身份。 */
	FCombatEventContext BlockerEvent;
	FCombatSourceContext BlockerSource;
	/** 避免初始化失败或重复 EndPlay 通知。 */
	bool bInitialized = false;
};
