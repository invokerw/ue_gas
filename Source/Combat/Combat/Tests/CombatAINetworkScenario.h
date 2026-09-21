#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatAINetworkScenario.generated.h"

class ACombatAIDemoArena;

/** 仅 -CombatAISmoke 显式开启的真实联机检查；测试编排可以逐帧观察，AI 玩法仍全部来自树资产。 */
UCLASS(NotBlueprintable)
class ACombatAINetworkScenario : public AActor
{
	GENERATED_BODY()
public:
	ACombatAINetworkScenario();
	virtual void Tick(float DeltaSeconds) override;
private:
	/** 输出唯一联机结果并停止观察；不把服务器的通过代替客户端证据。 */
	void Finish(bool bPassed, const FString& Detail);
	TWeakObjectPtr<ACombatAIDemoArena> Arena;
	float Elapsed = 0;
	bool bReturnRequested = false;
	bool bObservedServerOrder = false;
	uint64 SubmittedBeforeReturn = 0;
};
