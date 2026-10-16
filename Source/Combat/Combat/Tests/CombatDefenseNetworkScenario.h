#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/Defense/DefenseAugmentData.h"
#include "CombatDefenseNetworkScenario.generated.h"
class ACombatUnitCharacter;
/** 网络验证末态：由服务器采样属性，客户端比对复制的实际 AttributeSet；只用于测试探针。 */
USTRUCT()
struct FDefenseChoiceSmokeSample
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<ACombatUnitCharacter> Hero;
	UPROPERTY() TArray<float> Attributes;
};
/** 只在显式 DefenseSmoke 开关下运行的地图测试探针；生产地图无开关时不 Tick、不改战斗。 */
UCLASS()
class COMBAT_API ACombatDefenseNetworkScenario : public AActor
{
	GENERATED_BODY()
public:
	ACombatDefenseNetworkScenario();
	virtual void BeginPlay() override;
	virtual void Tick(float Delta) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	/** 汇总真实服务器或客户端观察，输出唯一结果。 */
	void Finish(bool bPass,const FString& Detail);
	/** 测试编排使用公共 Damage 形成合法死亡，不直接写生命或改导演计数。 */
	void Kill(ACombatUnitCharacter* Unit);
	/** 在真实连接上提交选择/重抽及相同载荷重放，只在显式测试开关下执行。 */
	void ExerciseChoices();
	/** 双客户端争购、同载荷重放、战斗期拒绝及共享效果到期验证。 */
	void ExerciseTeamShop();
	int32 ShopAttemptRevision=0;
	int32 ShopClosedRequestId=0;
	bool bShopActiveSeen=false,bShopSupplySeen=false,bShopExpiredSeen=false,bShopClosedSent=false,bShopClosedRejected=false;
	bool bShopServerActive=false;
	bool bCoreShieldFullSeen=false,bCoreShieldPartialSeen=false,bCoreShieldDepletedSeen=false;
	int32 SentChoiceRevision = 0;
	int32 RerollStep = 0;
	bool bChoiceEvidence = false;
	UPROPERTY(Replicated) TArray<FDefenseChoiceSmokeSample> FinalAttributes;
	float Elapsed=0;
	int32 LastWave=INDEX_NONE, ClearedWaves=0, ExpectedPlayers=1;
	double WaveStarted=0;
	float CoreAtWaveStart=0;
	bool bSentOrder=false,bRPC=false,bSawMovement=false,bSawDamage=false,bRespawn=false,bDone=false;
	TWeakObjectPtr<ACombatUnitCharacter> RespawnHero;
	uint32 PreviousLife=0;
	TMap<TWeakObjectPtr<ACombatUnitCharacter>,FVector> InitialPositions;
};
