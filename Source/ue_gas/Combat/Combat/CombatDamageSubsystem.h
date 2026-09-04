#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Combat/CombatTransactionTypes.h"

#include "CombatDamageSubsystem.generated.h"

/**
 * 服务器伤害结算入口。依次处理效果的伤害前回调、增幅、抗性和护盾，再通过 GAS 的临时伤害属性扣除生命。
 * 结果使用实际扣除量；随后通知伤害后回调、结算吸血并处理死亡。生命移除标志 HPLoss 会跳过免疫、防御、效果回调和吸血，但仍经过生命扣除与死亡流程。
 */
UCLASS()
class UE_GAS_API UCombatDamageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 同步结算伤害；来源和目标必须有效且存活，目标必须由服务器控制，伤害量必须有限且非负。bSuccess 表示请求完成，免疫阻挡或实际扣血为 0 也可成功；实际扣血读取 Event.AppliedAmount。有效 ParentEvent 会关联为同一事件链中的子事件。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Damage", meta=(DisplayName="造成战斗伤害", ToolTip="同步结算伤害；来源和目标必须有效且存活，目标必须由服务器控制，伤害量必须有限且非负。bSuccess 表示请求完成，免疫阻挡或实际扣血为 0 也可成功；实际扣血读取 Event.AppliedAmount。有效 ParentEvent 会关联为同一事件链中的子事件。"))
	FCombatDamageResult DealDamage(UPARAM(DisplayName="伤害请求") const FCombatDamageRequest& Request);

private:
	/** 创建根事件或受深度上限约束的子事件。 */
	FCombatEventContext CreateEventContext(const FCombatEventContext& Parent) const;
	/** 写入本次成功结算的伤害结果日志，包括免疫阻挡；前置校验失败的返回路径不调用此函数。 */
	void EmitResultLog(const FCombatDamageResult& Result) const;
};
