#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatRegenerationComponent.generated.h"

/** 以固定 Scheduler Coalesce 节拍结算 Unit 的 Health/Mana 每秒恢复。 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatRegenerationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 默认关闭 Tick，并使用 ADR-024 的 0.25 秒固定节拍。 */
	UCombatRegenerationComponent();
	/** Authority BeginPlay 时建立恢复任务。 */
	virtual void BeginPlay() override;
	/** EndPlay 时取消恢复任务。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** Dying 时取消当前相位且不补结死亡期间 tick。 */
	void HandleOwnerDeath();
	/** Alive 新生命建立后从新相位开始恢复。 */
	void HandleOwnerRespawn();

	/** ADR-024 冻结的恢复逻辑间隔。 */
	static constexpr float RegenIntervalSeconds = 0.25f;

private:
	/** 建立幂等 repeating Scheduler 任务。 */
	void StartSchedule();
	/** 取消当前恢复任务。 */
	void StopSchedule();
	/** 按 TickCount 补偿本次 Health/Mana 恢复量。 */
	void HandleRegenTick(const FCombatScheduledTickContext& TickContext);

	/** 当前恢复 repeating 任务句柄。 */
	FCombatScheduleHandle RegenSchedule;
};
