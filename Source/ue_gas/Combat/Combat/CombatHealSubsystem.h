#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Combat/CombatTransactionTypes.h"

#include "CombatHealSubsystem.generated.h"

/**
 * 服务器 Heal 的唯一业务入口。
 * 子系统按固定阶段执行来源/目标 Hook、治疗增幅和 GAS 元属性落账，以 AttributeSet 回报的真实 Health delta 区分有效与过量治疗，并保持 follow-up 的事件因果链。
 */
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
