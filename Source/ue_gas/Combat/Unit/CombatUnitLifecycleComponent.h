#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Combat/Log/CombatEventSubsystem.h"

#include "CombatUnitLifecycleComponent.generated.h"

class ACombatUnitCharacter;

/** Unit 完成一次死亡清理后广播的原生事件。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCombatUnitDied, ACombatUnitCharacter*, const FCombatEventContext&);
/** Unit 建立新生命代次后广播的原生事件。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCombatUnitRespawned, ACombatUnitCharacter*, const FCombatEventContext&);

/**
 * 执行 Unit 的服务器权威 Alive/Dying/Dead/Respawning 状态机。
 * 死亡路径按固定顺序隔离 Order、Attack、Motion、Modifier 和碰撞状态；复活建立新的 LifeGeneration，使上一生命创建的异步回调无法作用于新生命。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatUnitLifecycleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatUnitLifecycleComponent();

	/** 仅服务器接受存活单位的死亡请求，同步取消当前行为、处理死亡效果和碰撞，再进入 Dead 并广播一次。此入口不要求生命已经为 0，也不主动扣血；通常由致死伤害调用，也可显式触发死亡。 */
	bool RequestDeath(const FCombatEventContext& CauseEvent, ACombatUnitCharacter* Killer);
	/** 仅服务器接受已死亡单位的复活请求；检查坐标数值后直接传送，不做导航投射或落点避障。成功时递增生命代次、恢复满生命/法力、重建效果和恢复任务，最后进入 Alive 并广播；传送失败则退回 Dead。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Life", meta=(DisplayName="在指定位置复活", ToolTip="仅服务器接受已死亡单位的复活请求；检查坐标数值后直接传送，不做导航投射或落点避障。成功时递增生命代次、恢复满生命/法力、重建效果和恢复任务，最后进入 Alive 并广播；传送失败则退回 Dead。"))
	bool RespawnAtLocation(UPARAM(DisplayName="复活位置") FVector NewLocation);

	/** 返回 exactly-once 死亡广播。 */
	FOnCombatUnitDied& OnDied() { return DiedDelegate; }
	/** 返回 exactly-once 复活广播。 */
	FOnCombatUnitRespawned& OnRespawned() { return RespawnedDelegate; }

private:
	/** 返回组件所属 Combat Unit。 */
	ACombatUnitCharacter* GetOwnerUnit() const;
	/** 为显式复活或无 CauseEvent 死亡创建根事件。 */
	FCombatEventContext CreateRootEvent() const;
	/** 为成功完成的死亡或复活转换写入一次结构化日志；重复请求由状态检查提前拒绝。 */
	void EmitLifecycleLog(const FCombatEventContext& Context, bool bRespawn, ACombatUnitCharacter* OtherUnit) const;

	/** 当前 Unit 的死亡观察者。 */
	FOnCombatUnitDied DiedDelegate;
	/** 当前 Unit 的复活观察者。 */
	FOnCombatUnitRespawned RespawnedDelegate;
};
