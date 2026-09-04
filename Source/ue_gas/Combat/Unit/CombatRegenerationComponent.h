#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatRegenerationComponent.generated.h"

/**
 * 服务器每 0.25 秒按当前每秒恢复属性恢复生命和法力。卡顿错过多次时合并结算，例如错过 3 次便按 0.75 秒计算。
 * 生命恢复走公共治疗入口，会受治疗增幅和效果回调影响；法力通过 GAS 直接增加。死亡时取消任务，复活后重新计时，不补算死亡期间的恢复。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatRegenerationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatRegenerationComponent();
	/** Authority BeginPlay 时建立恢复任务。 */
	virtual void BeginPlay() override;
	/** EndPlay 时取消恢复任务。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** 进入死亡清理时取消恢复任务；未结算的旧周期和死亡期间的时间都不在复活后补算。 */
	void HandleOwnerDeath();
	/** 新生命建立后确保恢复任务存在；新建任务的首次恢复在 0.25 秒后，已存在任务时不改变节拍。 */
	void HandleOwnerRespawn();

	/** 恢复检查间隔，单位为秒；每次请求量为当前每秒恢复属性乘此间隔，再乘合并的到期次数。 */
	static constexpr float RegenIntervalSeconds = 0.25f;

private:
	/** 仅在服务器且当前没有活动恢复任务时建立周期调度；回调另行检查单位是否存活。 */
	void StartSchedule();
	/** 取消当前恢复任务。 */
	void StopSchedule();
	/** 存活时读取当前恢复属性，按 TickCount 代表的到期次数合并一次生命/法力恢复；不逐次重放期间的历史属性。 */
	void HandleRegenTick(const FCombatScheduledTickContext& TickContext);

	/** 当前恢复 repeating 任务句柄。 */
	FCombatScheduleHandle RegenSchedule;
};
