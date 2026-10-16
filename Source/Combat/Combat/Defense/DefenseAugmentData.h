#pragma once
#include "CoreMinimal.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Defense/DefenseTypes.h"
#include "DefenseAugmentData.generated.h"

class UCombatModifierData;
class UDefenseAugmentData;

/** 强化家族只影响候选排序/协同显示，效果仍由公共 Modifier/Ability 入口实现。 */
UENUM(BlueprintType)
enum class EDefenseAugmentFamily : uint8
{
	Frost UMETA(DisplayName="霜冻弹幕"),
	Skill UMETA(DisplayName="技能爆发"),
	Guardian UMETA(DisplayName="守护反击"),
	Economy UMETA(DisplayName="风险富集")
};
/** 候选稀有度仅作为权重/显示输入，不改变服务器合法性。 */
UENUM(BlueprintType)
enum class EDefenseAugmentRarity : uint8
{
	Basic UMETA(DisplayName="基础"),
	Advanced UMETA(DisplayName="进阶"),
	Final UMETA(DisplayName="终局")
};

/** Phase 2A 强化定义；每张卡只描述数据与合法门槛，不自行结算。 */
UCLASS(BlueprintType, meta=(DisplayName="防守强化定义", ToolTip="定义一张稳定身份的局内强化卡；实际属性和状态由引用的公共 Modifier 处理。"))
class COMBAT_API UDefenseAugmentData : public UCombatDefinitionData
{
	GENERATED_BODY()
public:
	/** 候选家族。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="强化家族", ToolTip="用于候选排序、构筑协同和 HUD 显示；不会绕过 Modifier 合法性。"))
	EDefenseAugmentFamily Family = EDefenseAugmentFamily::Frost;
	/** 候选稀有度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="稀有度", ToolTip="用于显示和权重调优；服务器仍按完整门槛校验。"))
	EDefenseAugmentRarity Rarity = EDefenseAugmentRarity::Basic;
	/** 最早出现的波次，1-based。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Augment", meta=(ClampMin="0", ClampMax="6", DisplayName="最早波次", ToolTip="该卡允许出现的最早波次，1-based；0 表示准备阶段即可出现。"))
	int32 EarliestWave = 0;
	/** 权重必须为正；候选抽取使用稳定哈希加权抽取。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Augment", meta=(ClampMin="1", ClampMax="100000", DisplayName="候选权重", ToolTip="同一合法池内的相对抽取权重；不代表最终出现概率。"))
	int32 Weight = 100;
	/** 可持有叠层上限；当前原型每张卡最多持有一次。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Augment", meta=(ClampMin="1", ClampMax="1", DisplayName="持有上限", ToolTip="同一玩家最多持有该卡的叠层数；Phase 2A 选择同一 DefinitionId 时拒绝重复。"))
	int32 MaxStacks = 1;
	/** 通用 Modifier；服务器选择后通过公共入口应用，死亡暂存至复活。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="公共 Modifier", ToolTip="选择后由服务器通过 Combat Modifier Component 应用；不可为空；永久友方属性效果，死亡保留。"))
	TObjectPtr<UCombatModifierData> ModifierData;
	/** 只读显示名。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Presentation", meta=(DisplayName="卡片名称", ToolTip="显示在强化候选和已持有列表中的本地化名称。"))
	FText CardName;
	/** 只读短描述。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Presentation", meta=(DisplayName="短描述", ToolTip="用一句话说明玩家选择后会获得什么。"))
	FText ShortDescription;
	/** 互斥卡 DefinitionId；任一已持有时该卡不进入候选。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="互斥强化", ToolTip="已持有其中任意 DefinitionId 时，本卡不进入候选；服务器按稳定 ID 比较。"))
	TArray<FPrimaryAssetId> MutuallyExclusive;
	/** 前置卡 DefinitionId；全部持有后本卡才合法。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="前置强化", ToolTip="玩家必须已经持有的 DefinitionId；为空表示无前置。"))
	TArray<FPrimaryAssetId> Prerequisites;
	/** 适用英雄 DefinitionId；为空表示通用。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="适用英雄", ToolTip="为空表示所有英雄适用；非空时当前英雄 DefinitionId 必须命中其中一个。"))
	TArray<FPrimaryAssetId> CompatibleHeroes;

	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;
	/** 校验身份、范围、引用和互斥/前置 DefinitionId。 */
	bool ValidateRuntime(FString& OutDiagnostic) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** 候选池定义；目录顺序不参与随机结果，生成前按 DefinitionId 排序。 */
UCLASS(BlueprintType, meta=(DisplayName="防守强化池", ToolTip="配置服务器可抽取的强化定义集合和 Phase 2A 规则版本。"))
class COMBAT_API UDefenseAugmentPoolData : public UCombatDefinitionData
{
	GENERATED_BODY()
public:
	/** 候选池独立 schema 版本。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="强化池版本", ToolTip="当前强化池规则版本；Phase 2A 为 1。"))
	int32 PoolSchemaVersion = 1;
	/** 可抽取定义；重复 DefinitionId 会被拒绝。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="强化定义", ToolTip="服务器候选池；生成前按稳定 DefinitionId 排序，数组顺序不会影响固定种子结果。"))
	TArray<TObjectPtr<UDefenseAugmentData>> Augments;

	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;
	/** 校验池身份、版本、重复定义和每张卡的运行时数据。 */
	bool ValidateRuntime(FString& OutDiagnostic) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** 一次玩家强化候选的只读凭证；候选 DefinitionId 可复制，效果对象不复制。 */
USTRUCT(BlueprintType)
struct COMBAT_API FDefenseAugmentOffer
{
	GENERATED_BODY()
	/** Offer schema 版本。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="候选版本", ToolTip="候选视图 schema 版本，DEF-004 为 2。"))
	int32 SchemaVersion = 2;
	/** 服务器选择代次；重复请求必须带同一值。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="选择修订", ToolTip="当前玩家候选修订；旧修订选择会安全失败。"))
	int32 ChoiceRevision = 0;
	/** 产生候选时的 0-based 波次。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="波次", ToolTip="候选生成所属波次。"))
	int32 WaveIndex = INDEX_NONE;
	/** 最多三张候选，合法不足时可以少于三张。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="候选定义", ToolTip="稳定 PrimaryAssetId 列表；客户端按本地资产解析显示。"))
	TArray<FPrimaryAssetId> Candidates;
	/** 选择是否已经提交。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="已选择", ToolTip="该候选是否已经完成一次合法选择。"))
	bool bResolved = false;
	/** 属于本局、本阶段的窗口；截止采用服务器游戏秒。 */
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="对局编号", ToolTip="请求必须匹配当前局。")) FGuid RunId;
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="阶段代次", ToolTip="旧阶段窗口不能提交。")) int64 PhaseGeneration = 0;
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="截止时刻", ToolTip="服务器世界游戏秒；超时默认首张。")) double Deadline = 0;
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="补给候选", ToolTip="没有合法卡时一次领取 80 金币。")) bool bSupply = false;
	UPROPERTY(BlueprintReadOnly, Category="Defense", meta=(DisplayName="已取消", ToolTip="终局取消，不授予默认奖励。")) bool bCancelled = false;

};


/** 选择请求的服务器结果；同一 RequestId 重发返回同一结果。 */
USTRUCT(BlueprintType)
struct COMBAT_API FDefenseChoiceResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="成功", ToolTip="服务器是否接受本次候选选择。")) bool bAccepted = false;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="请求 ID", ToolTip="客户端提交的幂等请求 ID。")) int32 RequestId = 0;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="选择修订", ToolTip="请求对应的候选修订。")) int32 ChoiceRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="选择定义", ToolTip="成功时被选择的稳定 DefinitionId。")) FPrimaryAssetId ChosenDefinition;
	UPROPERTY(BlueprintReadOnly, Category="Defense|Augment", meta=(DisplayName="失败原因", ToolTip="拒绝时的稳定诊断原因。")) FName FailureReason;
};

/** 服务器拥有的可复现种子与随机域计数；不把完整种子复制给客户端。 */
USTRUCT()
struct COMBAT_API FDefenseSeedState
{
	GENERATED_BODY()
	uint64 RunSeed = 0;
	uint16 AlgorithmVersion = 1;
	TMap<FName, uint32> DomainOrdinals;
};

/** 客户端只提交意图，不带卡片数值或价格。 */
UENUM()
enum class EDefenseChoiceAction : uint8 { Select, Reroll };
/** 有界选择信封；组件所有权由连接验证，局/窗口/英雄身份由导演复核。 */
USTRUCT()
struct COMBAT_API FDefenseChoiceRequest
{
	GENERATED_BODY()
	UPROPERTY() int32 RequestId = 0;
	UPROPERTY() FGuid RunId;
	UPROPERTY() int32 ChoiceRevision = 0;
	UPROPERTY() EDefenseChoiceAction Action = EDefenseChoiceAction::Select;
	UPROPERTY() FPrimaryAssetId DefinitionId;
	UPROPERTY() int64 LifeGeneration = 0;
	UPROPERTY() int32 BindingGeneration = 0;
	UPROPERTY() int32 EconomyRevision = 0;
	/** 完整载荷比较用于缓存重放，不能仅比较 RequestId。 */
	bool operator==(const FDefenseChoiceRequest& R) const;
};
/** 一张候选/已持有卡的白名单投影，定义 UObject 不复制。 */
USTRUCT()
struct COMBAT_API FDefenseCardView
{
	GENERATED_BODY()
	UPROPERTY() FPrimaryAssetId DefinitionId;
	UPROPERTY() FText Name;
	UPROPERTY() FText Description;
	UPROPERTY() EDefenseAugmentFamily Family = EDefenseAugmentFamily::Frost;
	UPROPERTY() bool bPending = false;
};
/** 仅所属 Controller 拥有者可见；Revision 用于 UI 合并刷新，非权威输入。 */
USTRUCT()
struct COMBAT_API FDefenseChoiceView
{
	GENERATED_BODY()
	UPROPERTY() int32 Revision = 0;
	UPROPERTY() FDefenseAugmentOffer Offer;
	UPROPERTY() TArray<FDefenseCardView> Cards;
	UPROPERTY() TArray<FDefenseCardView> Owned;
	UPROPERTY() int32 RerollsUsed = 0;
	UPROPERTY() int32 RerollCost = 60;
	UPROPERTY() bool bCanReroll = false;
	UPROPERTY() bool bCanChoose = false;
	UPROPERTY() FDefenseChoiceResult LastResult;
};
/** 有界完整请求及结果记录，只在服务器保存。 */
struct FDefenseChoiceReceipt
{
	FDefenseChoiceRequest Request;
	FDefenseChoiceResult Result;
};
