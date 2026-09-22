#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatAITacticalNetworkScenario.generated.h"

/** 阶段 C 独立进程 smoke 观察器；服务器检查战术决策，客户端只检查既有单位/属性复制。 */
UCLASS(NotBlueprintable)
class ACombatAITacticalNetworkScenario : public AActor
{
	GENERATED_BODY()

public:
	ACombatAITacticalNetworkScenario();
	virtual void Tick(float DeltaSeconds) override;

private:
	/** 每个进程只输出一个可机读终态。 */
	void Finish(bool bPassed, const FString& Detail);
	float Elapsed = 0.0f;
};
