#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatCameraNetworkScenario.generated.h"

/** 显式 -CombatCameraSmoke 的 PIE/独立联机检查；注入边缘样本，不冒充物理鼠标验收。 */
UCLASS(NotBlueprintable)
class ACombatCameraNetworkScenario : public AActor
{
	GENERATED_BODY()
public:
	ACombatCameraNetworkScenario();
	virtual void Tick(float DeltaSeconds) override;
private:
	/** 写出一次结果并停止测试更新，不修改单位、网络命令或全局输入设置。 */
	void Finish(bool bPassed, const FString& Detail);
	float Elapsed = 0;
};
