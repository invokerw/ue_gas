#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Combat/CombatTransactionTypes.h"

#include "CombatHealSubsystem.generated.h"

/** 服务器 Heal 唯一入口，负责 Hook、增幅、真实落账与过量治疗。 */
UCLASS()
class UE_GAS_API UCombatHealSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 同步执行完整 Heal 流水线并返回 AttributeSet 的真实 Health delta。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Heal", meta=(DisplayName="执行战斗治疗", ToolTip="在服务器同步执行完整治疗流水线，并返回目标生命值的真实变化。"))
	FCombatHealResult Heal(UPARAM(DisplayName="治疗请求") const FCombatHealRequest& Request);

private:
	/** 创建根事件或受深度上限约束的子事件。 */
	FCombatEventContext CreateEventContext(const FCombatEventContext& Parent) const;
	/** 写入一条 exactly-once Heal Result 结构化日志。 */
	void EmitResultLog(const FCombatHealResult& Result) const;
};
