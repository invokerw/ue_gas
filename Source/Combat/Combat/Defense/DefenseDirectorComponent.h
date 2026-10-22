#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Defense/DefenseTypes.h"
#include "Combat/Defense/DefenseAugmentData.h"
#include "Combat/Defense/DefenseEventData.h"
#include "Combat/Items/CombatItemAcquireEffect.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "DefenseDirectorComponent.generated.h"
class ACombatPlayerController;
class ADefenseGameState;
class ADefenseObjective;
class UCombatShopData;
class UCombatItemData;
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
/** 精确绑定到一个英雄或核心的团队效果句柄；不同组件的同编号句柄不能混用。 */
struct FDefenseTeamAppliedModifier
{
	TWeakObjectPtr<ACombatUnitCharacter> Unit;
	FCombatModifierHandle Handle;
};
/** 本局共享购买记录；终局保留已购状态，效果句柄单独撤销。 */
struct FDefenseTeamPurchaseRecord
{
	bool bPurchased=false;
	bool bEffectActive=false;
	/** 1-based 最后覆盖波，清场后到期。 */
	int32 ExpireWave=0;
	/** 尚未分配给新死亡的次数；已分配后断线或终局不退款。 */
	int32 RemainingReinforcements=0;
	/** 购买时已经Dead/Dying的生命不追溯；只保留本局弱引用。 */
	TMap<TWeakObjectPtr<ACombatUnitCharacter>,uint32> ExcludedDeaths;
	FString BuyerName;
	TArray<FDefenseTeamAppliedModifier> AppliedModifiers;
	FCombatSourceContext Source;
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
	/** 救援凭证绑定某次死亡，只允许紧接着的新生命领取一次。 */
	FPrimaryAssetId PendingReinforcement;
	uint32 ReinforcementDeathLife=0;
	/** 按服务器加入顺序分配，不依赖 UObject 地址。 */
	int32 PlayerOrdinal = INDEX_NONE;
	int32 LastOfferWave = INDEX_NONE;
	FDefenseAugmentOffer CurrentOffer;
	/** 最多 32 个完整载荷回执；HighRequestId 防止缓存淘汰后再次执行旧号。 */
	TArray<FDefenseChoiceReceipt> ChoiceResults;
	int32 HighRequestId = 0;
	int32 RerollsUsed = 0;
    /** 最近成功购买装备的整备阶段代次；死亡、出售和控制切换不重置本窗口次数。 */
    int32 EquipmentPurchasePhase = 0;
    int32 EquipmentPurchases = 0;
	/** 初始空构筑选择只成功一次，不作为中途英雄迁移入口。 */
	bool bStartingHeroSelected=false;
	TArray<FPrimaryAssetId> PreviousUnchosen;
	TArray<FPrimaryAssetId> ChosenCards;
	TSet<FPrimaryAssetId> PendingCards;
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
	bool InitializeForRun(UDefenseRunData* Data, ADefenseObjective* Core, ADefenseGameState* State, bool bWaitForStart = false);
	/** 登记开局玩家；仅准备阶段接受，不改变英雄控制权。 */
	bool RegisterPlayer(ACombatPlayerController* Player, ACombatUnitCharacter* Hero);
	/** 服务器替换当前英雄；保留本局选择/回执/经济/团队账本，通用卡迁移，不兼容卡休眠。 */
	bool ReplacePlayerHero(ACombatPlayerController* Player,ACombatUnitCharacter* NewHero);
	/** 初始准备、空构筑/库存/经验且无团队交易时可选择一次；不读取客户端判断。 */
	bool CanSelectStartingHero(ACombatPlayerController* Player) const;
	/** 安全出生并初始化白名单英雄，绑定失败保留旧Unit；仅成功后改绑生命周期并清理旧角色。 */
	bool SelectStartingHero(ACombatPlayerController* Player,TSubclassOf<ACombatUnitCharacter> HeroClass,FName& Failure);
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
	/** 返回服务器当前 RunSeed；仅开发诊断和自动化读取，客户端不通过此接口获得完整种子。 */
	uint64 GetRunSeedForTesting() const { return SeedState.RunSeed; }
	/** 仅开发本地服务器开局前可锁定输入种子；空白使用随机。客户端没有该 RPC。 */
	bool StartWithSeed(const FString& Input, FString& Error);
	/** 严格解析十进制/0x 正整数；溢出、符号、零和非法字符拒绝。 */
	static bool ParseSeed(const FString& Input, uint64& Out);
	/** 只有对应组件/服务器可信调用者可送请求，业务身份在此再次校验。 */
	FDefenseChoiceResult ProcessChoice(ACombatPlayerController* Player, const FDefenseChoiceRequest& Request);
	/** 返回玩家自己的白名单快照；不包含完整种子或效果对象。 */
	FDefenseChoiceView BuildChoiceView(ACombatPlayerController* Player) const;
	/** 完整缓存命中状态：0 未命中，1 同载荷，-1 复用 ID 换载荷。 */
	int32 InspectChoiceReplay(ACombatPlayerController* Player, const FDefenseChoiceRequest& Request) const;
	/** 为玩家返回当前修订候选；没有候选时按当前波次生成一次。 */
	bool GetOrCreateAugmentOffer(ACombatPlayerController* Player, FDefenseAugmentOffer& OutOffer);
	/** 提交一张当前候选；同一 RequestId 重发返回原结果。 */
	FDefenseChoiceResult ChooseAugment(ACombatPlayerController* Player, int32 RequestId, int32 ChoiceRevision, const FPrimaryAssetId& DefinitionId);
	/** 从唯一 CombatShopData 收集团队获得效果；只初始化一次，价格不在导演复制保存。 */
	bool InitializeTeamItems(UCombatShopData* Shop);
	/** 为原商店生成单项只读领取状态。 */
	FCombatShopOfferView BuildTeamItemOffer(ACombatPlayerController* Player,FPrimaryAssetId ItemId) const;
	/** 公开领取入口的领域预检，不扣款、不修改物品实例。 */
	bool CanReceiveTeamItem(const FCombatItemAcquireContext& Context,FGameplayTag& Failure) const;
	/** 在 ItemSubsystem 的领取锁内提交团队效果和限购账本。 */
	bool ReceiveTeamItem(const FCombatItemAcquireContext& Context,FGameplayTag& Failure);
    /** 普通装备只读准入：当前登记英雄、整备阶段、同款持有限额及每窗口两次配额。 */
    FCombatShopOfferView BuildEquipmentOffer(ACombatPlayerController* Player, const UCombatItemData* Item) const;
    /** 经济互斥内使用预检时冻结的局/阶段记录成功购买，不调用外部API或广播。 */
    void RecordEquipmentPurchase(ACombatPlayerController* Player, const FCombatShopOfferView& AcceptedOffer);
	/** 事件组件提交当前事件意图；服务器复核阶段、修订、玩家身份和重复请求。 */
	FDefenseEventResult ProcessDefenseEvent(ACombatPlayerController* Player, const FDefenseEventRequest& Request);
	/** 构造所有客户端共享的事件只读快照。 */
	FDefenseEventView BuildEventView() const;
	/** 检查事件请求是否已缓存；0未命中，1同载荷，-1复用编号换载荷。 */
	int32 InspectEventReplay(ACombatPlayerController* Player, const FDefenseEventRequest& Request) const;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FDefenseTestAccess;
	friend struct FDefenseEventPIEAccess;
	friend struct FDefenseChoiceTestAccess;
	friend struct FDefenseTeamShopTestAccess;
	friend struct FDefenseEquipmentTestAccess;
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
	/** 死亡事件先锁生命代次和等待截止，再消耗至多一次团队援军；不在死亡栈内复活。 */
	void OnHeroDied(ACombatUnitCharacter* Hero,const FCombatEventContext& Cause);
	/** 复活后领取与上一死亡绑定的护盾；旧生命、终局或已消费凭证不重复执行。 */
	void OnHeroRespawned(ACombatUnitCharacter* Hero,const FCombatEventContext& Cause);
	/** 唯一援军待领入口；Hook内保留待领由Monitor重试，失败且当前生命仍合法时返回false。 */
	bool ApplyPendingReinforcement(ACombatUnitCharacter* Hero);
	/** 统计指定团队道具尚未兑现的复活凭证，只供状态投影。 */
	int32 CountPendingReinforcements(FPrimaryAssetId ItemId) const;
	/** 断线撤销该英雄的短盾，不退次数、不影响其他英雄。 */
	void RemoveHeroReinforcement(ACombatUnitCharacter* Hero);
	/** 登记解除、终局与Shutdown共用的幂等委托解绑。 */
	void UnbindHeroLifecycle(ACombatUnitCharacter* Hero);
	/** 生成固定种子下的三张合法候选。 */
	bool GenerateAugmentOffer(FDefensePlayerRecord& Record, int32 WaveIndex);
	/** 创建并发布一个事件节点；没有资产配置时使用两个固定原型事件。 */
	void OpenEventAfterWave(int32 CompletedWaveIndex);
	/** 事件安全/风险/取消/超时的唯一结算入口。 */
	bool ResolveEvent(EDefenseEventOption Option, EDefenseEventState State, ACombatPlayerController* Proposer);
	/** 事件结束后恢复整备流程；旧事件句柄在此之前全部失效。 */
	void FinishEventAndResume();
	/** 为当前事件创建默认原型数据。 */
	void BuildDefaultEvents();
	/** 结算风险事件成功波次的延迟奖励。 */
	bool RewardRiskEvent(int32 CompletedWaveIndex);
	/** 风险预算换算为下一波的额外基础敌人数量。 */
	int32 ConsumeRiskBonusForWave(int32 WaveIndex);
	/** 发布所有客户端可读的事件状态。 */
	void PublishEvent();
	/** 返回当前玩家是否持有某个 DefinitionId。 */
	static bool HasOwnedAugment(const FDefensePlayerRecord& Record, const FPrimaryAssetId& DefinitionId);
	/** 只在整备阶段开窗，生成成功后更新所有者投影。 */
	void OpenChoiceWindows();
	/** 处理超时默认选择，不受英雄死亡阻止；可在下一波开始前强制收口。 */
	void ExpireChoices(double Now, bool bForce = false);
	/** 唯一选择落账，先锁结果再生效；死亡时只保存待生效定义。 */
	bool CommitChoice(ACombatPlayerController* Player, FDefensePlayerRecord& Record, FPrimaryAssetId DefinitionId);
	/** 将待生效卡应用为持久 Modifier，成功保存精确句柄。 */
	bool ApplyPendingCards(FDefensePlayerRecord& Record);
	/** 查找本局冻结的合法卡。 */
	UDefenseAugmentData* FindCard(FPrimaryAssetId Id) const;
	/** 开窗、提交、生命变化时通知只读复制组件。 */
	void PublishChoices();
	/** 发布共同账本和逐项阶段门，保留连接私有失败提示。 */
	void PublishTeamShop();
	/** 只读当前账本中有效核心屏障余量；无活跃句柄时为0。 */
	float ReadObjectiveShield() const;
	/** 监控时清除耗尽屏障；清场阶段内按 1-based 最后覆盖波撤销；不在战斗期间提前过期。 */
	void ExpireTeamShop();
	/** 撤销精确句柄；终局保留限购记录，Shutdown 才丢弃整局账本。 */
	void ClearTeamShopEffects(bool bClearLedger=false);
	/** 死亡时购买或准备期加入的英雄补领当前有效效果，已有句柄不重复应用。 */
	bool ApplyTeamShopEffects(ACombatUnitCharacter* Hero);
	/** 只允许本局三个整备阶段交易。 */
	bool IsTeamShopOpen() const;
	/** 当前阶段是否允许个人强化窗口。 */
	bool IsChoicePhase() const;
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
	FDefenseSeedState SeedState;
	UPROPERTY(Transient) TMap<FPrimaryAssetId,TObjectPtr<UCombatItemData>> FrozenTeamItems;
	TMap<FPrimaryAssetId,FDefenseTeamPurchaseRecord> TeamPurchaseLedger;
	int32 TeamShopRevision=1;
	UPROPERTY(Transient) TArray<TObjectPtr<UDefenseEventData>> FrozenEvents;
	FDefenseRiskLedger RiskLedger;
	FDefenseEventView EventView;
	struct FDefenseEventReceipt { TWeakObjectPtr<ACombatPlayerController> Player; FDefenseEventRequest Request; FDefenseEventResult Result; };
	TArray<FDefenseEventReceipt> EventReceipts;
	/** 请求编号只在同一连接内单调，玩家之间不能共享高水位或回执。 */
	TMap<TWeakObjectPtr<ACombatPlayerController>,int32> HighEventRequestIds;
	TWeakObjectPtr<ACombatPlayerController> EventProposer;
	/** 已成功清场的事件累计倍率；新事件不能覆盖先前已赚取的Boss奖励。 */
	float EarnedBossRewardBonus=0;
	bool bEventMutating=false;
	int64 LastShopPhase=0;
	bool bTeamItemsInitialized=false;
	int32 NextPlayerOrdinal = 0;
	uint32 CoreLife = 0;
	bool bInitialized = false;
	bool bEnding = false;
	bool bCompletionPending = false;
	bool bChoiceMutating = false;
	bool bStartingHeroMutating=false;
	bool bTeamShopMutating = false;
	int32 GroupIndex = 0, SpawnIndex = 0, SpawnFailures = 0;
	int32 RiskExtraSpawnsRemaining = 0;
	TArray<FDefenseEnemyRecord> Enemies;
	TMap<TWeakObjectPtr<ACombatPlayerController>, FDefensePlayerRecord> Players;
	/** 阶段任务被每次转换取消；监视任务跨阶段存活并在终局统一取消。 */
	FCombatScheduleHandle PhaseSchedule, MonitorSchedule;
};
