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

/** 执行服务器权威 Alive/Dying/Dead/Respawning 同步状态机。 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatUnitLifecycleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 默认关闭 Tick；状态转换只由 Damage 或显式 Respawn 事件驱动。 */
	UCombatUnitLifecycleComponent();

	/** 仅 Authority 接受 Alive 单位的死亡请求，并同步完成 Dying 清理和 Dead。 */
	bool RequestDeath(const FCombatEventContext& CauseEvent, ACombatUnitCharacter* Killer);
	/** 仅 Authority 接受 Dead 单位的复活请求，验证位置后建立新生命代次。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Life")
	bool RespawnAtLocation(FVector NewLocation);

	/** 返回 exactly-once 死亡广播。 */
	FOnCombatUnitDied& OnDied() { return DiedDelegate; }
	/** 返回 exactly-once 复活广播。 */
	FOnCombatUnitRespawned& OnRespawned() { return RespawnedDelegate; }

private:
	/** 返回组件所属 Combat Unit。 */
	ACombatUnitCharacter* GetOwnerUnit() const;
	/** 为显式复活或无 CauseEvent 死亡创建根事件。 */
	FCombatEventContext CreateRootEvent() const;
	/** 输出 UnitDeath/UnitRespawned 的 exactly-once 结构化日志。 */
	void EmitLifecycleLog(const FCombatEventContext& Context, bool bRespawn, ACombatUnitCharacter* OtherUnit) const;

	/** 当前 Unit 的死亡观察者。 */
	FOnCombatUnitDied DiedDelegate;
	/** 当前 Unit 的复活观察者。 */
	FOnCombatUnitRespawned RespawnedDelegate;
};
