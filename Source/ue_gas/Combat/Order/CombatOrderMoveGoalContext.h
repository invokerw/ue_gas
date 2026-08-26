#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"

#include "CombatOrderMoveGoalContext.generated.h"

/** 让可选 EQS 从 Combat OrderComponent 读取当前 generation 对应的移动目的点。 */
UCLASS()
class UE_GAS_API UCombatOrderMoveGoalContext : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	/** 向 EQS 写入当前 Combat Unit 的权威 Order move goal。 */
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
