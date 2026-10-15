#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Defense/DefenseTypes.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "DefenseDirectorComponent.generated.h"
class ACombatPlayerController;
class ADefenseGameState;
class ADefenseObjective;
class UDefenseRunData;
class UCombatUnitData;
class UCombatModifierData;

/** 仅服务器持有的一名敌人身份；死亡按 Actor、生命代次和阶段代次匹配。 */
struct FDefenseEnemyRecord
{
	TWeakObjectPtr<ACombatUnitCharacter> Unit;
	uint32 Life = 0;
	uint32 Phase = 0;
	bool bDead = false;
};
/** 玩家局内记录归 Controller，英雄死亡不丢失强化及待领经验；断线移除。 */
struct FDefensePlayerRecord
{
	TWeakObjectPtr<ACombatUnitCharacter> Hero;
	TSet<int32> RewardedWaves;
	TArray<FCombatModifierHandle> AugmentHandles;
	TSet<FPrimaryAssetId> OwnedAugments;
	TArray<TWeakObjectPtr<UCombatModifierData>> PendingAugments;
	int32 PendingExperience = 0;
	uint32 DeadLife = 0;
	double RespawnAt = 0;
	int32 RespawnRetries = 0;
};
/**
 * 服务器拥有整局状态和固定出生计划，GameState 只投影。
 * 离散工作全部使用 Combat Scheduler；阶段代次和生命身份淘汰旧回调。
 * 统一终结取消调度/解绑委托/销毁自建敌人，清场不在死亡委托栈内判胜。
 */
UCLASS(ClassGroup=(Combat), meta=(DisplayName="防守导演", ToolTip="服务器固定波次、终局、共享奖励与安全复活管理。"))
class COMBAT_API UDefenseDirectorComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UDefenseDirectorComponent();
	/** 开局只允许一次；复制有效计划，拒绝客户端、跨 World 和无效核心。 */
	bool InitializeForRun(UDefenseRunData* Data, ADefenseObjective* Core, ADefenseGameState* State);
	/** 登记开局玩家；仅准备阶段接受，不改变英雄控制权。 */
	bool RegisterPlayer(ACombatPlayerController* Player, ACombatUnitCharacter* Hero);
	/** 断线撤销本局强化和待领奖励，不接管别人的英雄。 */
	void RemovePlayer(ACombatPlayerController* Player);
	/** 幂等清理所有己方资源；GameMode 和 Component EndPlay 共用。 */
	void Shutdown();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	EDefensePhase GetPhase() const { return View.Phase; }
	int32 GetCurrentWaveIndex() const { return View.WaveIndex; }
	uint32 GetPhaseGeneration() const { return static_cast<uint32>(View.PhaseGeneration); }
	const FDefenseTerminalResult& GetTerminalResult() const { return Terminal; }
	const UDefenseRunData* GetFrozenRunData() const { return RunData; }
	int32 GetActiveEnemyCount() const { return View.RemainingEnemies; }
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FDefenseTestAccess;
#endif
	/** 阶段转换取消旧阶段工作，RunId 不变而 generation 递增。 */
	void EnterPhase(EDefensePhase Phase, int32 Wave, float Duration);
	/** 安排一次阶段任务；回调前复核 RunId 和 generation。 */
	void SchedulePhase(float Delay);
	/** Scheduler 统一推进准备、出生与延后清场。 */
	void Advance(const FCombatScheduledTickContext& Tick, FGuid ExpectedRun, uint32 Generation);
	/** 有界出生重试；耗尽失败而不是算作击杀。 */
	void SpawnNext();
	/** Deferred Spawn 后经公共单位初始化，沿路线提交公共 Move/Attack。 */
	bool SpawnEnemy(const FDefenseSpawnGroup& Group);
	/** 登记成功生成的敌人并绑定生命代次；不计未知单位。 */
	void RegisterEnemy(ACombatUnitCharacter* Enemy);
	/** 低频检查核心、敌人旧生命、异常 EndPlay、超时和玩家复活。 */
	void Monitor(const FCombatScheduledTickContext& Tick);
	/** 合法清场仅排队；下一 Scheduler 回合才奖励及判断胜利。 */
	void QueueWaveCompletion();
	/** 核心死亡优先，旧核心和旧生命不能终结新局。 */
	void OnCoreDied(ACombatUnitCharacter* Unit, const FCombatEventContext& Cause);
	/** 死亡只记账一次，延后回合清理尸体/判胜，避免伤害管线重入。 */
	void OnEnemyDied(ACombatUnitCharacter* Unit, const FCombatEventContext& Cause);
	/** 唯一终结入口，先封闭状态再清资源，不因重入产生第二次结果。 */
	void Finish(bool bVictory, FName Reason);
	/** 解绑并清理全部本波生成单位及其专用 AIController。 */
	void ClearEnemies();
	/** 先写奖励凭证再调用现有金币/经验/Modifier API；死亡玩家延后领取经验和强化。 */
	bool RewardWave();
	/** 尝试安全复活并兑现待领状态；终局不再复活或补奖。 */
	void ServicePlayers(double Now);
	/** 从现有 ASC 投影生命，客户端不接收定义 UObject 或权威写入口。 */
	void Publish();
	/** 核心必须仍是开局的有效生命。 */
	bool IsCoreAlive() const;
	/** 复核同 World 服务器、活跃局与阶段身份。 */
	bool IsCurrent(FGuid ExpectedRun, uint32 Generation) const;
	/** 导航投射和候选胶囊碰撞检查；失败时不生成/传送到任意位置。 */
	bool FindSafeLocation(const ACombatUnitCharacter& Unit, FVector Desired, FVector& Out) const;

	/** 本局深拷贝计划，只在服务器持有；不复制到客户端。 */
	UPROPERTY(Transient) TObjectPtr<UDefenseRunData> RunData;
	/** 世界持有核心，导演只保留弱引用和开局生命代次。 */
	TWeakObjectPtr<ADefenseObjective> Objective;
	TWeakObjectPtr<ADefenseGameState> GameState;
	FDefenseRunView View;
	FDefenseTerminalResult Terminal;
	uint32 CoreLife = 0;
	bool bInitialized = false;
	bool bEnding = false;
	bool bCompletionPending = false;
	int32 GroupIndex = 0, SpawnIndex = 0, SpawnFailures = 0;
	TArray<FDefenseEnemyRecord> Enemies;
	TMap<TWeakObjectPtr<ACombatPlayerController>, FDefensePlayerRecord> Players;
	/** 阶段任务被每次转换取消；监视任务跨阶段存活并在终局统一取消。 */
	FCombatScheduleHandle PhaseSchedule, MonitorSchedule;
};
