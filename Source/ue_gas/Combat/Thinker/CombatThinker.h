#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatThinker.generated.h"

/** 无 Tick、无碰撞的权威区域 Actor；玩法时序全部由 ThinkerSubsystem/Scheduler 持有。 */
UCLASS(Blueprintable)
class UE_GAS_API ACombatThinker : public AActor
{
	GENERATED_BODY()

public:
	/** 默认关闭 Tick、碰撞并启用基础 Actor 复制。 */
	ACombatThinker();
	/** Spawn 后写入稳定句柄。 */
	void InitializeThinker(FCombatThinkerHandle InHandle) { ThinkerHandle = InHandle; }
	/** Subsystem Destroy 前阻止 EndPlay 反向重复 Finish。 */
	void PrepareForSubsystemDestroy() { bSubsystemDestroying = true; }
	/** 外部 Destroy/World teardown 时回报 registry。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 仅服务器本地调试使用的稳定句柄。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Thinker") FCombatThinkerHandle ThinkerHandle;

private:
	/** true 表示当前 Destroy 已由 Finish 发起。 */
	bool bSubsystemDestroying = false;
};
