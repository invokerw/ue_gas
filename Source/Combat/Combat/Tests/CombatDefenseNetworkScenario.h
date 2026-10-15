#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatDefenseNetworkScenario.generated.h"
class ACombatUnitCharacter;
/** 只在显式 DefenseSmoke 开关下运行的地图测试探针；生产地图无开关时不 Tick、不改战斗。 */
UCLASS()
class COMBAT_API ACombatDefenseNetworkScenario : public AActor
{
	GENERATED_BODY()
public:
	ACombatDefenseNetworkScenario();
	virtual void BeginPlay() override;
	virtual void Tick(float Delta) override;
private:
	/** 汇总真实服务器或客户端观察，输出唯一结果。 */
	void Finish(bool bPass,const FString& Detail);
	/** 测试编排使用公共 Damage 形成合法死亡，不直接写生命或改导演计数。 */
	void Kill(ACombatUnitCharacter* Unit);
	float Elapsed=0;
	int32 LastWave=INDEX_NONE, ClearedWaves=0, ExpectedPlayers=1;
	double WaveStarted=0;
	float CoreAtWaveStart=0;
	bool bSentOrder=false,bRPC=false,bSawMovement=false,bSawDamage=false,bRespawn=false,bDone=false;
	TWeakObjectPtr<ACombatUnitCharacter> RespawnHero;
	uint32 PreviousLife=0;
	TMap<TWeakObjectPtr<ACombatUnitCharacter>,FVector> InitialPositions;
};
