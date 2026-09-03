#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Combat/CombatTransactionTypes.h"

#include "CombatDamageSubsystem.generated.h"

/**
 * 服务器 Damage 的唯一业务入口。
 * 子系统按固定阶段执行来源/目标 Hook、抗性、Shield 和 GAS 元属性落账，以 AttributeSet 回报的真实 Health delta 生成结果；follow-up 继承 RootEventId，并受递归深度与标志约束。
 */
UCLASS()
class UE_GAS_API UCombatDamageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 同步执行完整 Damage 流水线并返回 AttributeSet 的真实 Health delta。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Damage", meta=(DisplayName="造成战斗伤害", ToolTip="在服务器同步执行完整伤害流水线，并返回目标生命值的真实变化。"))
	FCombatDamageResult DealDamage(UPARAM(DisplayName="伤害请求") const FCombatDamageRequest& Request);

private:
	/** 创建根事件或受深度上限约束的子事件。 */
	FCombatEventContext CreateEventContext(const FCombatEventContext& Parent) const;
	/** 写入一条 exactly-once Damage Result 结构化日志。 */
	void EmitResultLog(const FCombatDamageResult& Result) const;
};
