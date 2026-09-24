#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Combat/Core/CombatTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "CombatLogTypes.generated.h"

class UCombatLogComponent;

/** 玩家记录的显示分类；只控制筛选，不改变事件或结算。 */
UENUM(BlueprintType)
enum class ECombatLogCategory : uint8
{
	Damage UMETA(DisplayName="伤害信息"),
	Healing UMETA(DisplayName="治疗信息"),
	Ability UMETA(DisplayName="技能"),
	Status UMETA(DisplayName="状态信息"),
	Item UMETA(DisplayName="物品与经济")
};

/** 独立于核心事件 schema 的玩家历史快照；定义在本地解析，实例 ID 仅作不透明筛选键。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatLogEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="记录序号", ToolTip="服务器提交顺序；用于排序和去重，不是客户端可提交的请求。"))
	int64 Sequence = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="事件时间", ToolTip="事件发生时的服务器游戏时间，单位秒。", Units="s"))
	double ServerTime = 0.0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="事件类型", ToolTip="服务器已完成的战斗事件类型，仅用于显示。"))
	FGameplayTag EventType;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="显示分类", ToolTip="该条记录对应的伤害、治疗、技能或状态筛选项。"))
	ECombatLogCategory Category = ECombatLogCategory::Damage;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="来源实例", ToolTip="服务器进程中的不透明单位标识；不可与客户端 UObject ID 比较。"))
	int32 SourceActorId = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="目标实例", ToolTip="用于区分同名目标的服务器标识；0 表示没有单位目标。"))
	int32 TargetActorId = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="来源定义", ToolTip="事件发生时来源的稳定定义，用于本地显示名称。"))
	FPrimaryAssetId SourceDefinitionId;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="目标定义", ToolTip="事件发生时目标的稳定定义；目标销毁后仍可显示。"))
	FPrimaryAssetId TargetDefinitionId;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="效果定义", ToolTip="伤害与技能事件优先关联技能，状态事件优先持续效果；缺失时回退持续效果或弹体定义。"))
	FPrimaryAssetId EffectDefinitionId;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="物品来源", ToolTip="事件发生时冻结的物品定义，实例销毁后仍保留。")) FPrimaryAssetId ItemDefinitionId;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="物品实例", ToolTip="服务器物品实例身份，仅用于归因。")) FCombatItemHandle ItemHandle;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="物品操作", ToolTip="物品变化的稳定动作名称。")) FName ItemAction;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="物品数量", ToolTip="此次物品操作完成后的堆叠数量，充能单独记录。")) int32 ItemQuantity = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="物品充能", ToolTip="此次物品操作完成后的可用充能次数。")) int32 ItemCharges = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="金币变化", ToolTip="购买为负、收入和出售为正；非经济事件为 0。")) int64 GoldDelta = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="金币余额", ToolTip="经济事务完成后的服务器权威单一金币余额。")) int64 GoldBalance = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="来源属于玩家", ToolTip="事件发生时来源是玩家主控英雄或玩家私有资源端点，用于非英雄筛选；不提供玩法分类。"))
	bool bSourceHero = false;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="目标是主控英雄", ToolTip="事件发生时目标有玩家指挥，历史不会因控制权变化而改写。"))
	bool bTargetHero = false;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="实际数值", ToolTip="伤害/治疗为实际生命变化；状态施加为当前层数。"))
	float Amount = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="有资源前后值", ToolTip="仅当服务器提供真实事务快照时显示生命前后值，禁止从当前血量反算。"))
	bool bHasHealthChange = false;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="变化前生命", ToolTip="该次事务落账前的生命值；不会随之后伤害、治疗或复活更新。"))
	float PreviousHealth = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="变化后生命", ToolTip="该次事务落账后的生命值；不是读取日志时的当前生命。"))
	float NewHealth = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="自动施法已开启", ToolTip="仅用于自动施法切换事件，冻结服务器当时的开关状态。"))
	bool bAutoCastEnabled = false;
};

/** 每个玩家连接的有界历史；服务器追加，拥有者客户端只接收完整批次通知。 */
USTRUCT()
struct COMBAT_API FCombatLogArray : public FFastArraySerializer
{
	GENERATED_BODY()
	UPROPERTY() TArray<FCombatLogEntry> Items;
	UPROPERTY(NotReplicated) TObjectPtr<UCombatLogComponent> Owner = nullptr;

	/** 追加递增序号，重复/旧记录返回 false；超过上限时淘汰最旧条目。 */
	bool Append(const FCombatLogEntry& Entry);
	/** 复用 UE 增量序列化，只复制有变动的历史条目。 */
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FCombatLogEntry, FCombatLogArray>(Items, DeltaParams, *this);
	}
	/** 本批增删全部完成后再刷新 UI，避免显示已经淘汰的旧条目。 */
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);
};

template<>
struct TStructOpsTypeTraits<FCombatLogArray> : TStructOpsTypeTraitsBase2<FCombatLogArray>
{
	enum { WithNetDeltaSerializer = true };
};

/** 本地筛选条件；0 的来源/目标表示全部，0 秒表示全部保留历史。 */
struct COMBAT_API FCombatLogFilter
{
	int32 SourceActorId = 0;
	int32 TargetActorId = 0;
	double TimeWindowSeconds = 30.0;
	bool bDamage = true;
	bool bHealing = true;
	bool bAbility = true;
	bool bStatus = true;
	bool bItem = true;
	bool bIncludeNonHeroes = true;

	/** 所有条件按 AND 组合；时间采用服务器时钟，拒绝非有限值。 */
	bool Matches(const FCombatLogEntry& Entry, double ServerTime) const;
};

/** 战斗记录展示规则；不读取或修改 gameplay 对象。 */
namespace CombatLogPresentation
{
	inline constexpr int32 SchemaVersion = 3;
	inline constexpr int32 MaxEntries = 512;
	/** 将玩家关心的事件归类；内部诊断和重复生命周期阶段返回 false。 */
	COMBAT_API bool Classify(FGameplayTag EventType, ECombatLogCategory& OutCategory);
	/** 将秒数显示为分钟、秒和毫秒；异常时间安全回退为零。 */
	COMBAT_API FString FormatTimestamp(double ServerTime);
	/** 生成不会与“全部”或其他单位标签冲突的选项，实例 ID 始终由独立映射保存。 */
	COMBAT_API FString BuildUnitOptionLabel(const FString& Name, int32 ActorId, bool bDuplicateName,
		const TMap<FString, int32>& ExistingOptions);
	/** 生成带样式标签的中文文字，所有外部显示名称先转义。 */
	COMBAT_API FString BuildRichText(const FCombatLogEntry& Entry, const FString& SourceName,
		const FString& TargetName, const FString& EffectName);
}
