#pragma once
#include "CoreMinimal.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Defense/DefenseTypes.h"
#include "Combat/Defense/DefenseEventData.h"
#include "DefenseData.generated.h"

class UDefenseAugmentPoolData;

/** 单波定义，只保存计划与奖励；运行状态由 Director 保存。 */
UCLASS(BlueprintType, meta=(DisplayName="防守波次定义", ToolTip="固定出生组、时限与共享奖励。"))
class COMBAT_API UDefenseWaveData : public UCombatDefinitionData
{
	GENERATED_BODY()
public:
	/** 独立防守数据版本，当前仅支持 1。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="防守数据版本", ToolTip="独立防守数据版本，当前仅支持 1。"))
	int32 DefenseSchemaVersion = 1;
	/** 非空，按顺序生成；总数最多 128。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="出生组", ToolTip="非空，按顺序生成；总数最多 128。"))
	TArray<FDefenseSpawnGroup> SpawnGroups;
	/** 游戏秒数，1 到 900；到期失败，清理不计击杀。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="最长波次时间", ToolTip="游戏秒数，1 到 900；到期失败，清理不计击杀。", ClampMin="1", ClampMax="900", Units="s"))
	float MaxDuration = 90.0f;
	/** 仅最后一波为真，必须包含一名 Boss。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="最终波", ToolTip="仅最后一波为真，必须包含一名 Boss。"))
	bool bBossWave = false;
	/** 每名在局玩家获得相同金额，0 到 100000。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="共享金币", ToolTip="每名在局玩家获得相同金额，0 到 100000。", ClampMin="0", ClampMax="100000"))
	int32 GoldReward = 100;
	/** 每名在局英雄获得相同经验，死亡时暂存至复活。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="共享经验", ToolTip="每名在局英雄获得相同经验，死亡时暂存至复活。", ClampMin="0", ClampMax="100000"))
	int32 ExperienceReward = 200;
	/** 可选永久友方 Modifier；死亡保留，Phase 2 可替换为候选选择结果。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="固定强化", ToolTip="可选永久友方 Modifier；死亡保留，Phase 2 可替换为候选选择结果。"))
	TObjectPtr<UCombatModifierData> FixedAugment;
	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;
	/** 拒绝空波、非法身份、超量计划和 Boss 配置错误。 */
	bool ValidateRuntime(FString& OutDiagnostic) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
/** 对局入口定义；仅新模式加载，开始时复制计划，禁止中途修改源资产改变当前局。 */
UCLASS(BlueprintType, meta=(DisplayName="防守对局定义", ToolTip="配置六波、核心、路线和安全复活点。"))
class COMBAT_API UDefenseRunData : public UCombatDefinitionData
{
	GENERATED_BODY()
public:
	/** 独立防守数据版本，当前仅支持 1。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="防守数据版本", ToolTip="独立防守数据版本，当前仅支持 1。"))
	int32 DefenseSchemaVersion = 1;
	/** 五个普通波加一个 Boss 波，各自定义身份唯一。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="波次", ToolTip="五个普通波加一个 Boss 波，各自定义身份唯一。"))
	TArray<TObjectPtr<UDefenseWaveData>> Waves;
	/** Phase 1 恰好两条不同命名的路线。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="路线", ToolTip="Phase 1 恰好两条不同命名的路线。"))
	TArray<FDefenseRoute> Routes;
	/** 友方核心 UnitData；移速与攻击为零、无击杀奖励。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="核心定义", ToolTip="友方核心 UnitData；移速与攻击为零、无击杀奖励。"))
	TObjectPtr<UCombatUnitData> ObjectiveData;
	/** 世界坐标厘米，根胶囊中心。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="核心位置", ToolTip="世界坐标厘米，根胶囊中心。"))
	FVector ObjectiveLocation = FVector(0,0,100);
	/** 世界坐标厘米，复活前进行导航和胶囊空位检查。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="安全复活点", ToolTip="世界坐标厘米，复活前进行导航和胶囊空位检查。"))
	FVector RespawnLocation = FVector(500,0,100);
	/** 游戏秒数 0 到 120；该阶段允许加入，之后拒绝新连接。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="准备时间", ToolTip="游戏秒数 0 到 120；该阶段允许加入，之后拒绝新连接。", ClampMin="0", ClampMax="120", Units="s"))
	float PreparationDuration = 20.0f;
	/** 游戏秒数 0 到 120；Boss 前共用此窗口。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="整备时间", ToolTip="游戏秒数 0 到 120；Boss 前共用此窗口。", ClampMin="0", ClampMax="120", Units="s"))
	float IntermissionDuration = 10.0f;
	/** 英雄死亡后的游戏秒数，1 到 60；落点阻塞会有界重试。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="复活等待", ToolTip="英雄死亡后的游戏秒数，1 到 60；落点阻塞会有界重试。", ClampMin="1", ClampMax="60", Units="s"))
	float RespawnDelay = 8.0f;
	/** 失败后最多重试 0 到 10 次，每次间隔 0.5 秒；用尽后对局失败。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense", meta=(DisplayName="出生重试", ToolTip="失败后最多重试 0 到 10 次，每次间隔 0.5 秒；用尽后对局失败。", ClampMin="0", ClampMax="10"))
	int32 SpawnRetryLimit = 3;
	/** Phase 2A 候选池；为空保持 DEF-002 固定强化兼容。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="强化池", ToolTip="Phase 2A 服务器候选池；为空时不生成随机强化候选。"))
	TObjectPtr<UDefenseAugmentPoolData> AugmentPool;
	/** 开发/测试用固定种子；0 表示服务器生成。完整种子不复制给客户端。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|RNG", meta=(DisplayName="开发固定种子", ToolTip="仅开发或自动化使用；非零时服务器使用该 RunSeed，生产默认 0 由服务器生成。", ClampMin="0"))
	int64 DevelopmentSeedOverride = 0;
	/** 事件目录；为空时按开关加载两个固定原型事件。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="事件目录", ToolTip="第2/4波后的事件定义；为空且启用原型事件时使用代码内置的两个原型事件。"))
	TArray<TObjectPtr<UDefenseEventData>> Events;
	/** 是否启用第2/4波固定事件；生产防守资产保持开启，纯波次测试可关闭。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="启用原型事件", ToolTip="开启后在没有事件资产时使用宝库探索和神殿誓约；关闭仅用于不需要事件的旧测试夹具。"))
	bool bEnablePrototypeEvents = true;
	/** 团队商店目录；为空保持 DEF-004 无团队购买。 */
	/** 选择不暂停对局，整备时间必须容纳整个窗口。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="选择时长", ToolTip="初始与普通波后选择的游戏秒数，1 到 60；不能大于准备或整备时长。", ClampMin="1", ClampMax="60", Units="s"))
	float ChoiceDuration = 15.0f;
	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;
	/** 校验全部定义、路线坐标、有限数、队伍、时限与六波结构。 */
	bool ValidateRuntime(FString& OutDiagnostic) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
#if WITH_DEV_AUTOMATION_TESTS
	/** 仅自动化使用的独立瞬态 fixture，生产 GameMode 必须显式配置资产。 */
	static UDefenseRunData* CreatePhase1Default(UObject* Outer);
#endif
};
