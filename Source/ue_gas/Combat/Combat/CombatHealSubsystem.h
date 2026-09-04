#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Combat/CombatTransactionTypes.h"

#include "CombatHealSubsystem.generated.h"

/**
 * 服务器治疗结算入口。依次处理来源的治疗前回调、双方治疗增幅和目标的治疗前回调，再通过 GAS 的临时治疗属性恢复生命。
 * 结果区分实际恢复量与超出生命上限的部分，之后通知双方治疗后回调；吸血等后续治疗可通过父事件关联到原始伤害。
 */
UCLASS()
class UE_GAS_API UCombatHealSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 同步结算治疗；来源和目标必须有效且存活，目标必须由服务器控制，治疗量必须有限且非负。bSuccess 表示请求完成，满血时也可成功；实际恢复量读取 Event.AppliedAmount，超额部分读取 Event.OverhealAmount。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Heal", meta=(DisplayName="执行战斗治疗", ToolTip="同步结算治疗；来源和目标必须有效且存活，目标必须由服务器控制，治疗量必须有限且非负。bSuccess 表示请求完成，满血时也可成功；实际恢复量读取 Event.AppliedAmount，超额部分读取 Event.OverhealAmount。"))
	FCombatHealResult Heal(UPARAM(DisplayName="治疗请求") const FCombatHealRequest& Request);

private:
	/** 创建根事件或受深度上限约束的子事件。 */
	FCombatEventContext CreateEventContext(const FCombatEventContext& Parent) const;
	/** 写入本次成功结算的治疗结果日志；前置校验失败的返回路径不调用此函数。 */
	void EmitResultLog(const FCombatHealResult& Result) const;
};
