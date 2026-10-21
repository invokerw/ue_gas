#pragma once
#include "CoreMinimal.h"
#include "Combat/Core/CombatTypes.h"
#include "Combat/Defense/DefenseEventData.h"
#include "DefenseTypes.generated.h"
class ACombatUnitCharacter;
class UCombatUnitData;
class UCombatAIProfileData;
class ADefenseObjective;

/** 防守阶段只由服务器 Director 改变；胜败是不可逆终态。 */
UENUM(BlueprintType)
enum class EDefensePhase : uint8
{
	/** 等待首波。 */ Preparation UMETA(DisplayName="准备"),
	/** 出生计划及清场。 */ WaveCombat UMETA(DisplayName="波次战斗"),
	/** 普通波之间整备。 */ Intermission UMETA(DisplayName="波间整备"),
	/** 最终波之前整备。 */ BossPreparation UMETA(DisplayName="Boss 整备"),
	/** 波次完成后处理一次性事件。 */ EventChoice UMETA(DisplayName="事件"),
	/** Boss 合法清场且核心存活。 */ Victory UMETA(DisplayName="胜利"),
	/** 核心死亡或不可恢复的对局故障。 */ Defeat UMETA(DisplayName="失败")
};
/** 两条预配置路线；航点由公共 Move Order 逐点经过，末点后攻击核心。 */
USTRUCT(BlueprintType)
struct COMBAT_API FDefenseRoute
{
	GENERATED_BODY()
	/** 非空且唯一的路线标识。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="路线名称", ToolTip="非空且唯一的路线标识。"))
	FName RouteName;
	/** 世界坐标厘米，出生前投射到导航并检查胶囊碰撞。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="出生位置", ToolTip="世界坐标厘米，出生前投射到导航并检查胶囊碰撞。"))
	FVector SpawnLocation = FVector::ZeroVector;
	/** 世界坐标厘米，最多 8 点；空数组直接向核心行进。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="路线航点", ToolTip="世界坐标厘米，最多 8 点；空数组直接向核心行进。"))
	TArray<FVector> Waypoints;
};
/** 一组固定敌人；定义身份固定，运行时 Actor 身份与定义分离。 */
USTRUCT(BlueprintType)
struct COMBAT_API FDefenseSpawnGroup
{
	GENERATED_BODY()
	/** 必须是有效非抽象 Combat Unit 类；蓝图原定义须与单位定义一致。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="单位类型", ToolTip="必须是有效非抽象 Combat Unit 类；蓝图原定义须与单位定义一致。"))
	TSubclassOf<ACombatUnitCharacter> UnitClass;
	/** 稳定 UnitData，敌队且无最后一击金币经验；奖励由整局导演发放。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="单位定义", ToolTip="稳定 UnitData，敌队且无最后一击金币经验；奖励由整局导演发放。"))
	TObjectPtr<UCombatUnitData> UnitData;
	/** 为空沿用导演队列；非空由角色树独占下令，在登记出生和设置路线职责后启动。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="路线角色AI", ToolTip="可选的已编译且开启感知的角色配置。空值由导演沿路线攻击核心；非空由角色树按职责选敌，单位定义的AI配置必须为空，Boss组不支持。本局开始时冻结参数。"))
	TObjectPtr<UCombatAIProfileData> RoleProfile;
	/** 1 到 64 个，单波总数最多 128。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="数量", ToolTip="1 到 64 个，单波总数最多 128。", ClampMin="1", ClampMax="64"))
	int32 Count = 1;
	/** 相邻出生的游戏秒数，范围 0 到 30；零也在下一调度回合生成。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="出生间隔", ToolTip="相邻出生的游戏秒数，范围 0 到 30；零也在下一调度回合生成。", ClampMin="0", ClampMax="30", Units="s"))
	float SpawnInterval = 1.0f;
	/** 从 0 开始，必须存在对应路线。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="路线索引", ToolTip="从 0 开始，必须存在对应路线。", ClampMin="0"))
	int32 RouteIndex = 0;
	/** 仅最终波允许，必须恰好一名 Boss。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="Boss 组", ToolTip="仅最终波允许，必须恰好一名 Boss。"))
	bool bBoss = false;
};

/** 事件在服务器上的终态；风险选项先经过可取消确认。 */
UENUM(BlueprintType)
enum class EDefenseEventState : uint8
{
	Inactive UMETA(DisplayName="未开启"),
	Offered UMETA(DisplayName="待选择"),
	PendingRisk UMETA(DisplayName="风险确认"),
	ResolvedSafe UMETA(DisplayName="安全已结算"),
	ResolvedRisk UMETA(DisplayName="风险已锁定"),
	Cancelled UMETA(DisplayName="已取消"),
	Expired UMETA(DisplayName="已超时")
};

/** 事件请求的选项；风险确认期间 Cancel 会回退到安全选项。 */
UENUM(BlueprintType)
enum class EDefenseEventOption : uint8
{
	Safe UMETA(DisplayName="安全"),
	Risk UMETA(DisplayName="风险"),
	Cancel UMETA(DisplayName="取消")
};

/** 风险事件的一次性账本；只由服务器 Director 写入。 */
USTRUCT(BlueprintType)
struct COMBAT_API FDefenseRiskLedger
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="事件编号", ToolTip="本次事件的稳定运行时身份。")) FGuid EventId;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="事件修订", ToolTip="每次风险确认或终态变化递增，旧请求不能再次结算。")) int32 Revision=0;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="事件定义", ToolTip="客户端显示用的稳定事件定义身份。")) FPrimaryAssetId DefinitionId;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="触发波次", ToolTip="该事件对应的1-based普通波次。")) int32 TriggerAfterWave=0;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="选项", ToolTip="服务器最终锁定的安全、风险或取消选项。")) EDefenseEventOption Option=EDefenseEventOption::Safe;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="状态", ToolTip="服务器事件终态。")) EDefenseEventState State=EDefenseEventState::Inactive;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="目标波次", ToolTip="风险压力和延迟奖励作用的0-based波次。")) int32 TargetWave=INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="预算增量", ToolTip="风险成功后下一波增加的预算点数。")) int32 RiskBudgetBonus=0;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="Boss奖励倍率增量", ToolTip="风险成功后后续Boss奖励增加的倍率。")) float BossRewardBonusFraction=0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="奖励凭证", ToolTip="风险延迟奖励使用的幂等凭证。")) FGuid RewardBatchId;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="奖励已发放", ToolTip="服务器防止重复发放的标记。")) bool bRewardGranted=false;
};

/** 复制给所有客户端的事件只读快照；不包含可伪造的奖励数值入口。 */
USTRUCT(BlueprintType)
struct COMBAT_API FDefenseEventView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="事件编号", ToolTip="当前事件运行时身份。")) FGuid EventId;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="事件修订", ToolTip="用于拒绝旧客户端请求。")) int32 Revision=0;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="事件定义", ToolTip="客户端解析标题和说明的稳定身份。")) FPrimaryAssetId DefinitionId;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="事件标题", ToolTip="当前事件标题。")) FText Title;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="安全说明", ToolTip="安全选项的结果说明。")) FText SafeDescription;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="风险说明", ToolTip="风险选项进入确认窗口后的结果说明。")) FText RiskDescription;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="状态", ToolTip="当前事件状态。")) EDefenseEventState State=EDefenseEventState::Inactive;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="发起者", ToolTip="最近提交风险确认的玩家名称。")) FString ProposerName;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="阶段截止", ToolTip="服务器世界时间；UI只用于倒计时显示。")) double Deadline=0.0;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="视图修订", ToolTip="事件视图变化时递增。")) int32 ViewRevision=0;
};

/** 事件请求信封；客户端只提交意图和当前身份。 */
USTRUCT()
struct COMBAT_API FDefenseEventRequest
{
	GENERATED_BODY()
	UPROPERTY() int32 RequestId=0;
	UPROPERTY() FGuid RunId;
	UPROPERTY() int64 PhaseGeneration=0;
	UPROPERTY() FGuid EventId;
	UPROPERTY() int32 EventRevision=0;
	UPROPERTY() int32 BindingGeneration=0;
	UPROPERTY() uint32 LifeGeneration=0;
	UPROPERTY() EDefenseEventOption Option=EDefenseEventOption::Safe;
};

/** 事件请求回执；重复请求返回同一结果，不重复结算。 */
USTRUCT(BlueprintType)
struct COMBAT_API FDefenseEventResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="请求编号", ToolTip="对应客户端提交的请求。")) int32 RequestId=0;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="是否成功", ToolTip="服务器是否接受并结算了本次意图。")) bool bSuccess=false;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="失败原因", ToolTip="失败时的稳定诊断原因。")) FName FailureReason;
};

/** 无第二套生命权威：客户端只读服务器从 ASC 投影的生命、事件和 Modifier 护盾快照，schema 4。 */
USTRUCT(BlueprintType)
struct COMBAT_API FDefenseRunView
{
	GENERATED_BODY()
	/** 防守独立投影版本。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="视图版本", ToolTip="防守独立投影版本。"))
	int32 SchemaVersion = 4;
	/** 本局唯一身份。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="对局编号", ToolTip="本局唯一身份。"))
	FGuid RunId;
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="等待开始", ToolTip="开发服务器还未锁定种子并开始准备。")) bool bWaitingForStart = false;
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="种子摘要", ToolTip="仅用于对应服务器日志，不包含完整种子。")) FString SeedSummary;
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="候选随机计数", ToolTip="本局候选已抽取槽数，只用于诊断。")) int64 OfferDrawCount = 0;
	/** 服务器确认的阶段。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="阶段", ToolTip="服务器确认的阶段。"))
	EDefensePhase Phase = EDefensePhase::Preparation;
	/** 从 0 开始，开局准备为 -1。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="波次索引", ToolTip="从 0 开始，开局准备为 -1。"))
	int32 WaveIndex = INDEX_NONE;
	/** 本局波次总数。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="波次数", ToolTip="本局波次总数。"))
	int32 TotalWaves = 0;
	/** 随阶段递增，拒绝旧回调。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="阶段代次", ToolTip="随阶段递增，拒绝旧回调。"))
	int64 PhaseGeneration = 0;
	/** 服务器世界游戏秒；终局为零。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="阶段截止", ToolTip="服务器世界游戏秒；终局为零。"))
	double Deadline = 0.0;
	/** 已出生且尚未确认死亡的本波敌人。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="存活敌人", ToolTip="已出生且尚未确认死亡的本波敌人。"))
	int32 RemainingEnemies = 0;
	/** 计划中尚未成功生成的敌人。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="待出生", ToolTip="计划中尚未成功生成的敌人。"))
	int32 PendingSpawns = 0;
	/** 当前局核心 Actor。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="核心", ToolTip="当前局核心 Actor。"))
	TObjectPtr<ADefenseObjective> Objective;
	/** 来自核心 ASC 的只读当前生命。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="核心生命", ToolTip="来自核心 ASC 的只读当前生命。"))
	float ObjectiveHealth = 0.0f;
	/** 来自核心 ASC 的只读最大生命。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="核心生命上限", ToolTip="来自核心 ASC 的只读最大生命。"))
	float ObjectiveMaxHealth = 0.0f;
	/** 核心屏障 Runtime 余量的只读投影，不参与结算。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="核心护盾", ToolTip="服务器当前神殿屏障剩余吸收量；耗尽、到期或终局为0。"))
	float ObjectiveShield = 0.0f;
	/** 当前事件只读投影；非事件阶段为 Inactive。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="事件", ToolTip="服务器发布的事件标题、选项状态和截止时间。"))
	FDefenseEventView Event;
	/** 稳定诊断原因；非终局为 None。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="终局原因", ToolTip="稳定诊断原因；非终局为 None。"))
	FName TerminalReason;
};
/** 本地终局凭证；一个 Run 最多写入一次。 */
struct COMBAT_API FDefenseTerminalResult
{
	bool bTerminal = false;
	bool bVictory = false;
	FName Reason;
};
