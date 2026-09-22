#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "CombatAITacticalTargetContext.generated.h"

/** 只向当前战术查询暴露启动时冻结的目标身份，不读取黑板或全局候选。 */
UCLASS()
class COMBAT_API UCombatAITacticalTargetContext : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

/**
 * 生成目标外侧的单个确定点。EQS 只负责选点，不投影导航、不执行移动；
 * 最终点仍须经过 Combat Move Order 的公共预检和导航入口。
 */
UCLASS(meta=(DisplayName="战术目标外侧单点"))
class COMBAT_API UCombatAITacticalLocationGenerator : public UEnvQueryGenerator
{
	GENERATED_BODY()

public:
	UCombatAITacticalLocationGenerator(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	UPROPERTY(EditDefaultsOnly, Category="Generator", meta=(DisplayName="目标距离", ToolTip="从冻结目标朝远离查询者方向生成单点的 XY 距离，单位厘米，必须为有限正数。", Units="cm", ClampMin="1"))
	float Distance = 600.0f;
};
