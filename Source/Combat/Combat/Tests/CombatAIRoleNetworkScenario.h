#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatAIRoleNetworkScenario.generated.h"

/** 独立进程角色 smoke 的观察器；仅显式开关生成，不向角色提交测试命令。 */
UCLASS(NotBlueprintable)
class ACombatAIRoleNetworkScenario : public AActor
{
	GENERATED_BODY()
public:
	ACombatAIRoleNetworkScenario();
	virtual void Tick(float DeltaSeconds) override;
private:
	/** 输出本进程唯一结果；服务器与两个客户端分别报告事实。 */
	void Finish(bool bPassed, const FString& Detail);
	float Elapsed = 0;
	bool bGuardMoved = false;
	bool bLaneMoved = false;
};
