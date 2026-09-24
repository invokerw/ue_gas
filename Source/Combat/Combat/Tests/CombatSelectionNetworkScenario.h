#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatSelectionNetworkScenario.generated.h"

class ACombatUnitCharacter;

/** 显式 -CombatSelectionSmoke 启用的独立网络夹具；合成选择输入，真实 Owner/RPC/导航/复制。 */
UCLASS(NotBlueprintable)
class ACombatSelectionNetworkScenario : public AActor
{
	GENERATED_BODY()
public:
	ACombatSelectionNetworkScenario();
	virtual void Tick(float DeltaSeconds) override;
private:
	/** 每进程只记录一次结果，客户端通过不能代替服务器拒绝证据。 */
	void Finish(bool bPassed, const FString& Detail);
	float Elapsed = 0;
	int32 Phase = 0;
	int32 LastRequest = 0;
	TWeakObjectPtr<ACombatUnitCharacter> First, Second, Foreign;
	FVector FirstStart = FVector::ZeroVector, SecondStart = FVector::ZeroVector;
	/** 同点移动必须等待到达并静置，不能用刚开始移动代替碰撞验收。 */
	FVector MoveGoal = FVector::ZeroVector;
	FVector CameraAnchor = FVector::ZeroVector;
	int32 MoveLeg = 0;
	float ArrivalStableSeconds = 0;
	double MaxArrivalRise = 0;
	double MaxAirborneUpSpeed = 0;
	double MaxServerStep = 0;
	TMap<TWeakObjectPtr<ACombatUnitCharacter>, FVector> ServerPrevious;
	TMap<TWeakObjectPtr<ACombatUnitCharacter>, FVector> ServerStarts;
};
