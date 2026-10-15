#pragma once
#include "CoreMinimal.h"
#include "Combat/Core/CombatTypes.h"
#include "DefenseTypes.generated.h"
class ACombatUnitCharacter;
class UCombatUnitData;
class ADefenseObjective;

/** 防守阶段只由服务器 Director 改变；胜败是不可逆终态。 */
UENUM(BlueprintType)
enum class EDefensePhase : uint8
{
	/** 等待首波。 */ Preparation UMETA(DisplayName="准备"),
	/** 出生计划及清场。 */ WaveCombat UMETA(DisplayName="波次战斗"),
	/** 普通波之间整备。 */ Intermission UMETA(DisplayName="波间整备"),
	/** 最终波之前整备。 */ BossPreparation UMETA(DisplayName="Boss 整备"),
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
/** 无第二套生命权威：客户端只读服务器从 ASC 投影的完整快照，schema 1。 */
USTRUCT(BlueprintType)
struct COMBAT_API FDefenseRunView
{
	GENERATED_BODY()
	/** 防守独立投影版本。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="视图版本", ToolTip="防守独立投影版本。"))
	int32 SchemaVersion = 1;
	/** 本局唯一身份。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="对局编号", ToolTip="本局唯一身份。"))
	FGuid RunId;
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
