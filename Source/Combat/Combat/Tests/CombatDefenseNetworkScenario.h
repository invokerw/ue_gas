#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/Defense/DefenseAugmentData.h"
#include "CombatDefenseNetworkScenario.generated.h"
class ACombatUnitCharacter;
struct FCombatLogRecord;
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
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	/** 汇总真实服务器或客户端观察，输出唯一结果。 */
	void Finish(bool bPass,const FString& Detail);
	/** 测试编排使用公共 Damage 形成合法死亡，不直接写生命或改导演计数。 */
	void Kill(ACombatUnitCharacter* Unit);
	/** 在真实连接上提交选择/重抽及相同载荷重放，只在显式测试开关下执行。 */
	void ExerciseChoices();
	/** 显式攻城验证只观察生产出生/移动/伤害，不为攻城单位发出测试命令。 */
	void ObserveSiege();
	/** 原自然攻击日志用于确认每名攻城兵确实打到核心。 */
	void OnSiegeCombatRecord(const FCombatLogRecord& Record);
	/** 当前含攻城的波次必须等两名攻城真实击中核心才允许测试清场。 */
	bool IsSiegeWaveObserved() const;
	/** 显式刺客探针等待生产角色自然伤害英雄，客户端仅检查可见移动与无权威AI。 */
	void ObserveAssassins();
	/** 只记录普攻真实AppliedAmount，不使用测试清场伤害作为交战证据。 */
	void OnAssassinCombatRecord(const FCombatLogRecord& Record);
	/** 含刺客波两名均移动并击中英雄后才允许清场。 */
	bool IsAssassinWaveObserved() const;
	TMap<int32,TSet<int32>> AssassinByWave;
	TMap<int32,FVector> AssassinInitialPositions;
	TSet<int32> AssassinMoved,AssassinHeroHit;
	TMap<int32,TSet<int32>> SiegeByWave;
	TMap<int32,FVector> SiegeInitialPositions;
	TSet<int32> SiegeMoved,SiegeCoreHit;
	/** 显式守卫验证：客户端选角/施法，服务器训练靶只用于隔离技能证据。 */
	void ExerciseGuardian();
	/** 统计真实技能伤害与护盾吸收，不把fixture清场算入技能命中。 */
	void OnGuardianCombatRecord(const FCombatLogRecord& Record);
	UPROPERTY(Replicated) TObjectPtr<ACombatUnitCharacter> GuardianSmokeHero;
	UPROPERTY(Replicated) TObjectPtr<ACombatUnitCharacter> GuardianTrainingTarget;
	int32 GuardianStage=0,GuardianSkillMask=0;
	double GuardianNextAction=0;
	bool bGuardianRequestSent=false,bGuardianWSeen=false,bGuardianPassiveSeen=false,bGuardianStunSeen=false;
	bool bGuardianDamageSeen=false,bGuardianAbsorbed=false,bGuardianSecurity=false,bGuardianCombatReady=false,bGuardianPhaseRejected=false;
	/** 双客户端争购、同载荷重放、战斗期拒绝及共享效果到期验证。 */
	void ExerciseTeamShop();
	/** 显式援军测试：真实RPC争购，三次死亡5/5/8秒与两次4秒护盾复制。 */
	void ExerciseReinforcement();
	int32 AidDeaths=0,AidRevivals=0,AidExpiries=0,AidAttemptRevision=0;
	bool bAidTwo=false,bAidOne=false,bAidZero=false,bAidExpired=false;
	TSet<FString> AidClientShields;
	TWeakObjectPtr<ACombatUnitCharacter> AidHero,AidProtectedHero;
	uint32 AidDeathLife=0;
	double AidDeathAt=0,AidShieldExpires=0;
	FCombatModifierHandle AidShieldHandle;
	int32 ShopAttemptRevision=0;
	bool bShopAgileSent=false;
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
