#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"

#include "CombatFissureBlocker.generated.h"

class UBoxComponent;

/** 沟壑的服务器物理阻挡物：创建后固定在原处，不逐帧更新，由调度器到期销毁；创建和移除都通知指令组件按阻挡范围判断是否需要重新寻路。 */
UCLASS()
class UE_GAS_API ACombatFissureBlocker : public AActor
{
	GENERATED_BODY()

public:
	ACombatFissureBlocker();
	/** 仅服务器首次初始化：按 Start/End 的 XY 长度设置碰撞尺寸并安排限时销毁，宽度、高度和时长必须为有限正数。位置与朝向须由 SpawnActor 预先设置，此函数不移动 Actor；失败后由调用方销毁它。 */
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
	/** 向当前 World 所有战斗单位发送阻挡包围盒；是否与路径相交、是否重试由各单位的指令组件判断。 */
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
