#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Ability/CombatAbilityTypes.h"
#include "Combat/Core/CombatTypes.h"
#include "Combat/Projectile/CombatProjectileTypes.h"
#include "Combat/Targeting/CombatTargetingTypes.h"

#include "CombatDefinitionData.generated.h"

class UCombatAbilitySet;
class UCombatGameplayAbility;
class UCombatModifierRuntime;
class UCombatProjectileData;
class ACombatProjectileActor;

/**
 * 配置 Modifier 对驱散操作的抵抗等级。
 * UCombatModifierComponent::Dispel 会把本次驱散的强度与该规则比较，强度足够时才移除 Modifier。
 * 本规则只限制 Dispel：不会阻止持续时间到期、死亡清理、按句柄明确移除或 World teardown。
 */
UENUM(BlueprintType)
enum class ECombatModifierDispelRule : uint8
{
	/** 普通驱散和强驱散都能移除。 */
	Basic UMETA(DisplayName="基础驱散可移除"),
	/** 普通驱散会跳过，只有强驱散能移除。 */
	StrongOnly UMETA(DisplayName="仅强驱散可移除"),
	/** 所有驱散都会跳过；Modifier 仍可通过其他正常生命周期结束。 */
	NotDispellable UMETA(DisplayName="不可驱散")
};

/**
 * 控制重复施加同一 Modifier、系统刷新已有 Runtime 时，下一次周期 OnThink 何时触发。
 * 刷新总会重新计算层数、运行时参数和过期时间；本策略只决定周期计时是否从刷新时刻重新开始。
 * 例如在 0s 施加、每 1s 触发并于 1.5s 刷新：PreservePhase 下一次仍在 2s，ResetInterval 下一次改为 2.5s。
 */
UENUM(BlueprintType)
enum class ECombatModifierRefreshPolicy : uint8
{
	/** 不重置周期计时，刷新前排定的下一次 OnThink 仍按原时刻触发。 */
	PreservePhase UMETA(DisplayName="保留周期相位"),
	/** 取消原周期计时，从刷新时刻经过一个 ThinkInterval 后再触发 OnThink。 */
	ResetInterval UMETA(DisplayName="重置周期间隔")
};

/**
 * 配置 Modifier 存活期间持续生效的一项 GAS 属性修改。
 * Attribute 选择属性，ModifierOp 选择计算方式，Magnitude 是默认幅值；若本次 Apply 请求带有
 * MagnitudeParameterKey 对应的运行时覆盖值，则该实例改用覆盖值。例如 MoveSpeed 使用
 * Multiplicitive 和 0.8 表示保留 80% 移速，覆盖值为 0.6 时本次施加改为保留 60%。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatModifierAttributeChange
{
	GENERATED_BODY()

	/** 选择要修改的 GAS 属性，例如 MoveSpeed 或 Armor。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="目标属性", ToolTip="选择该 Modifier 存活期间持续修改的 GAS 属性，例如 MoveSpeed 或 Armor。")) FGameplayAttribute Attribute;
	/** 选择幅值如何参与属性计算，例如 Additive 相加、Multiplicitive 相乘或 Override 覆盖。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="属性修改运算", ToolTip="选择幅值如何参与属性计算，例如 Additive 相加、Multiplicitive 相乘或 Override 覆盖。")) TEnumAsByte<EGameplayModOp::Type> ModifierOp = EGameplayModOp::Additive;
	/** 没有同名运行时覆盖值时使用的默认幅值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="默认幅值", ToolTip="没有配置幅值参数键，或本次施加请求没有同名覆盖值时使用的默认幅值。")) float Magnitude = 0.0f;
	/** 可选覆盖键；本次 Apply 请求含有同名 RuntimeParameterOverrides 时，其值优先于 Magnitude。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="幅值参数键", ToolTip="可选；本次施加请求含有同名运行时参数覆盖时使用覆盖值，否则使用默认幅值。None 表示始终使用默认幅值。")) FName MagnitudeParameterKey;
};

/**
 * 把已经发布过的旧 DefinitionId 显式迁移到当前 DefinitionId。
 * 例如 CombatModifier:frost_slow 改名为 CombatModifier:frost_arrow_slow 后保留一条重定向，使旧存档和日志仍能解析。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatDefinitionRedirect
{
	GENERATED_BODY()

	/** 已废弃但仍可能出现在存档或网络记录中的旧 ID。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Identity", meta=(DisplayName="旧定义 ID", ToolTip="已经废弃但仍可能出现在存档、日志或网络记录中的 PrimaryAssetId。"))
	FPrimaryAssetId OldId;

	/** 当前可解析的目标 ID。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Identity", meta=(DisplayName="新定义 ID", ToolTip="旧定义 ID 应迁移到的当前有效 PrimaryAssetId。"))
	FPrimaryAssetId NewId;

	/** 首次引入该重定向的 Combat 内容版本。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Identity", meta=(ClampMin="1", DisplayName="引入版本", ToolTip="首次加入该重定向的 Combat 内容版本，最小值为 1。"))
	int32 IntroducedInVersion = 1;
};

/**
 * 集中处理 Combat 定义 ID 的兼容性：发布前检查重定向，加载旧 ID 时通过一条映射找到当前资产。
 * v1 要求 OldId 直接指向 KnownIds 中的当前 ID，禁止多跳链和环路，避免不同消费者得到不同解析结果。
 * 若旧 ID 最终仍无法解析，则生成包含原 ID 的占位文本，便于日志和 UI 诊断缺失内容。
 */
struct UE_GAS_API FCombatDefinitionRegistry
{
	/** 当前 Combat 内容 schema 版本。 */
	static constexpr int32 CombatContentVersion = 1;

	/** 检查源和目标是否合法、旧源是否重复，并拒绝目标仍是另一条旧源的链式映射；目标必须直接存在于 KnownIds。 */
	static bool ValidateRedirects(
		const TArray<FCombatDefinitionRedirect>& Redirects,
		const TSet<FPrimaryAssetId>& KnownIds,
		TArray<FString>& OutErrors);

	/** RequestedId 已知时原样返回，否则查找一条直接指向 KnownIds 的重定向；没有直接映射时返回 false。 */
	static bool ResolveDefinitionId(
		const FPrimaryAssetId& RequestedId,
		const TArray<FCombatDefinitionRedirect>& Redirects,
		const TSet<FPrimaryAssetId>& KnownIds,
		FPrimaryAssetId& OutResolvedId);

	/** 为无法解析的 ID 生成包含资产类型和名称的缺失提示，供 UI 与日志显示。 */
	static FString MakeMissingPlaceholder(const FPrimaryAssetId& MissingId);
};

/**
 * 保存从技能等级 1 开始的一组分级数值，供 Ability Action 按当前等级读取。
 * 例如 Values=[100, 140, 180] 分别对应 1、2、3 级；等级小于 1 返回 0，高于数组长度取末值。
 * 只填写一个值表示所有合法等级共用同一数值，否则数组数量必须等于 AbilityData.MaxLevel。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatSpecialValue
{
	GENERATED_BODY()

	/** 从 Level 1 开始排列的数值表。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="分级数值", ToolTip="从技能等级 1 开始依次填写；可只填一个值供所有等级共用，否则数量必须等于最大等级。等级小于 1 返回 0，高于数组长度取末值。"))
	TArray<float> Values;

	/** 按 1-based Level 读取；Level 小于 1 或数组为空时返回 0，高于数组长度时返回末值。 */
	float GetValueAtLevel(int32 Level) const;
	/** 检查数组是“一个全等级共用值”或“恰好 MaxLevel 个分级值”，并要求每个值都是有限数。 */
	bool IsValidForMaxLevel(int32 MaxLevel) const;
};

/**
 * Unit、Ability、Modifier、Projectile 和 AbilitySet 等 Combat DataAsset 的公共基类。
 * 每个资产用“固定类型 + 同类型内唯一的 DefinitionName”生成 PrimaryAssetId；网络、日志和存档引用这个 ID，因此移动或重命名资产文件不会改变内容身份。
 * SchemaVersion 由代码维护，用于判断旧内容是否需要迁移。
 */
UCLASS(Abstract, BlueprintType)
class UE_GAS_API UCombatDefinitionData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 生成 PrimaryAssetId 的 lower_snake_case 名称，例如 frost_arrow_slow；在同一 PrimaryAssetType 内必须唯一。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category="Combat|Identity", meta=(DisplayName="稳定定义名", ToolTip="用于生成 PrimaryAssetId 的 lower_snake_case 名称，在同一定义类型内必须唯一；重命名资产文件不会改变此身份。"))
	FName DefinitionName;

	/** 当前 C++ 数据结构版本，由代码维护，内容迁移与兼容检查使用。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category="Combat|Identity", meta=(DisplayName="数据结构版本", ToolTip="当前 Combat 定义的数据结构版本，由代码维护并用于内容兼容与迁移校验。"))
	int32 SchemaVersion = 1;

	/** 返回“派生类型固定 PrimaryAssetType + DefinitionName”组成的 ID，供网络、日志、存档和资产查找使用。 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	/** 返回派生定义类型对应的固定 Combat PrimaryAssetType。 */
	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const;

	/** 检查名称是否非空且只使用 lower_snake_case 允许的字符。 */
	static bool IsValidDefinitionName(FName Name);
	/** 检查集合中的空引用、非法 DefinitionName 和重复 PrimaryAssetId，并把所有错误追加到 OutErrors。 */
	static bool ValidateDefinitionSet(const TArray<const UCombatDefinitionData*>& Definitions, TArray<FString>& OutErrors);

#if WITH_EDITOR
	/** 在 Editor 保存/验证时检查基础身份与 schema。 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/**
 * 战斗单位的服务器初始化模板，集中配置出生时的属性、队伍、普攻规则和固有 AbilitySet。
 * 这里只保存初始值和定义引用；生成后的属性变化由 GAS 管理，运行中的技能与 Modifier 状态不回写资产。
 */
UCLASS(BlueprintType)
class UE_GAS_API UCombatUnitData : public UCombatDefinitionData
{
	GENERATED_BODY()

public:
	/** 单位生成时写入 AttributeSet；Health/Mana 同时初始化为各自最大值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit", meta=(DisplayName="基础战斗属性", ToolTip="服务器初始化单位时写入 Combat AttributeSet 的基础属性集合；展开后可配置生命、攻击、移速等数值。"))
	FCombatUnitBaseStats BaseStats;

	/** 单位生成时使用的队伍；0 为中立，1..254 为有效队伍，255 为无效保留值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit", meta=(DisplayName="初始队伍", ToolTip="单位生成时使用的战斗队伍；0 表示中立，1 到 254 表示有效队伍，255 表示无效。"))
	FCombatTeamId InitialTeamId = FCombatTeamId(1);

	/** 大于 0 时覆盖 Character 默认胶囊半径，单位为厘米。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit", meta=(ClampMin="0", Units="cm", DisplayName="胶囊半径覆盖", ToolTip="大于 0 时覆盖 Character 默认碰撞胶囊半径，单位为厘米；0 表示保留 Character 配置。"))
	float CapsuleRadiusOverride = 0.0f;

	/** AttackSpeed=100 时，从发出攻击到生成 AttackLaunched 事件的基础前摇秒数。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(ClampMin="0", Units="s", DisplayName="基础攻击前摇", ToolTip="AttackSpeed 为 100 时，从攻击起手到生成 AttackLaunched 的基础时长，单位为秒。"))
	float BaseAttackPoint = 0.3f;

	/** 普攻起手允许的最大水平朝向误差；超过该角度时服务器先转向，满足后才开始前摇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(ClampMin="0", ClampMax="180", Units="deg", DisplayName="攻击朝向容差", ToolTip="普通攻击起手前允许的最大水平朝向误差，范围为 0 到 180 度；超出时 Order 会先在服务器转向。"))
	float AttackFacingToleranceDegrees = 15.0f;

	/** 开启后单位移动中也能进入攻击前摇；关闭后必须先停止移动。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(DisplayName="允许移动中攻击", ToolTip="启用后，CharacterMovement 仍在移动时也可以创建普通攻击记录。"))
	bool bAllowAttackWhileMoving = false;

	/** 开启后在普通攻击开始和真正命中时都重新检查攻击者与目标之间是否有几何遮挡。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(DisplayName="攻击需要视线", ToolTip="启用后，普通攻击起手和命中都通过 CombatTargeting 重新检查几何视线。"))
	bool bRequireAttackLineOfSight = false;

	/** 攻击开始时记录的暴击概率，范围 0..1；0.25 表示 25%。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(ClampMin="0", ClampMax="1", DisplayName="基础暴击概率", ToolTip="创建普通攻击记录时快照的暴击概率，取值 0 到 1；例如 0.25 表示 25%。"))
	float CriticalStrikeChance = 0.0f;

	/** 暴击成功时乘到普通攻击主伤害上的倍率；2 表示造成 200% 伤害。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(ClampMin="1", DisplayName="基础暴击倍率", ToolTip="暴击成功后乘到普通攻击主伤害上的倍率，最小值为 1；例如 2 表示 200% 伤害。"))
	float CriticalStrikeMultiplier = 2.0f;

	/** 非空时在攻击前摇结束后发射追踪弹体，弹体命中才结算攻击；为空时立即按近战命中结算。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(DisplayName="普通攻击弹体定义", ToolTip="非空时，普通攻击在攻击前摇结束后生成追踪攻击弹体；为空时按近战命中结算。"))
	TObjectPtr<UCombatProjectileData> AttackProjectileData = nullptr;

	/** 单位初始化时按数组顺序加载的技能集合；每个集合再授予其中配置的技能。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit", meta=(DisplayName="初始技能集合", ToolTip="单位初始化时按数组顺序加载并授予的 AbilitySet 软引用。"))
	TArray<TSoftObjectPtr<UCombatAbilitySet>> AbilitySets;

	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;

#if WITH_EDITOR
	/** 检查基础属性、攻击策略、初始队伍、胶囊和 AbilitySet 引用。 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/**
 * 技能的只读设计数据，供服务器校验目标、安排施法阶段并按顺序执行 DataDriven Action。
 * 每次施法的目标、等级和生命代次会另存为激活快照；此资产不保存单次施法的可变状态。
 */
UCLASS(BlueprintType)
class UE_GAS_API UCombatAbilityData : public UCombatDefinitionData
{
	GENERATED_BODY()

public:
	/** 服务器允许写入 AbilitySpec 的最高等级；每组 SpecialValues 必须提供一个共用值或恰好这么多级。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(ClampMin="1", DisplayName="最大等级", ToolTip="该 AbilitySpec 允许的最高服务器等级；每组 SpecialValues 必须提供一个全等级共用值，或恰好提供这么多级。"))
	int32 MaxLevel = 1;

	/** 用 GameplayTag 组合声明目标模式以及被动、引导、自动施法、可被法术格挡等能力。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(Categories="Ability.Behavior", DisplayName="行为标签", ToolTip="声明目标模式、被动、引导、自动施法、可被法术格挡等技能行为；组合必须通过 Ability 数据校验。"))
	FGameplayTagContainer BehaviorTags;

	/** 服务器在激活和施法前摇结束时用来复核目标阵营、状态、距离、遮挡和可见性的规则。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="目标规则", ToolTip="服务器用于复核单位或点目标的阵营、状态、范围、视线和可见性规则。"))
	FCombatTargetingRules TargetingRules;

	/** 技能伤害、治疗、半径等按名称保存的分级数值；Action 用键名按当前技能等级读取。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="技能分级参数", ToolTip="以稳定参数名索引的分级数值表；Action 通过对应键按当前技能等级读取。"))
	TMap<FName, FCombatSpecialValue> SpecialValues;

	/** 从技能激活到 SpellStarted 的前摇秒数；到点后服务器会重新校验目标再执行动作。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(ClampMin="0", Units="s", DisplayName="施法前摇", ToolTip="从激活到 SpellStarted 的服务器权威时长，单位为秒；由 Combat Scheduler 驱动。"))
	float CastPoint = 0.0f;

	/** 从 SpellStarted 到 ChannelFinish 的引导总秒数；只对带 Channelled 行为标签的技能有效。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(ClampMin="0", Units="s", DisplayName="引导总时长", ToolTip="引导技能从 SpellStarted 到 ChannelFinish 的总时长，单位为秒；只有带引导行为标签时可以大于 0。"))
	float ChannelDuration = 0.0f;

	/** 引导期间相邻两次 ChannelTick 的秒数；0 表示不安排周期 ChannelTick。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(ClampMin="0", Units="s", DisplayName="引导触发间隔", ToolTip="引导期间两次 ChannelTick 之间的时长，单位为秒；必须与引导总时长和行为标签一致。"))
	float ChannelInterval = 0.0f;

	/** 选择在哪个施法阶段正式扣除 ManaCost；在该阶段前失败或中断不会扣除。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="费用提交阶段", ToolTip="选择 ManaCost 在技能激活生命周期中的权威提交阶段。"))
	ECombatAbilityCommitStage CostCommitPoint = ECombatAbilityCommitStage::SpellStarted;

	/** 选择在哪个施法阶段正式开始 Cooldown；在该阶段前失败或中断不会进入冷却。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="冷却提交阶段", ToolTip="选择 Cooldown 在技能激活生命周期中的权威提交阶段。"))
	ECombatAbilityCommitStage CooldownCommitPoint = ECombatAbilityCommitStage::SpellStarted;

	/** 单位目标在施法前摇结束时失效后，是中断技能还是改用激活时记录的位置继续点/范围动作。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="目标丢失策略", ToolTip="单位目标在施法前摇结束时失效后的处理方式；仅点或范围行为可继续使用最后已知位置。"))
	ECombatTargetLostPolicy TargetLostPolicy = ECombatTargetLostPolicy::Fail;

	/** 引导被中断并释放当前施法命令后，决定保留还是清空玩家已经排队的后续命令。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="引导中断队列策略", ToolTip="引导被中断后，OrderComponent 是保留后续命令还是清空尚未执行的队列。"))
	ECombatChannelInterruptOrderPolicy ChannelInterruptOrderPolicy = ECombatChannelInterruptOrderPolicy::Continue;

	/** 开启后技能结束会取消本次施法发射且显式绑定的弹体；关闭后弹体独立飞行到命中或超时。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="技能结束时取消弹体", ToolTip="启用后，Ability 结束时取消本次激活显式绑定的弹体；关闭时弹体发射后独立完成。"))
	bool bCancelProjectilesWithAbility = false;

	/** 目标复核通过并进入 SpellStarted 后，服务器按数组顺序执行的伤害、治疗、Modifier、事件、弹体或区域动作。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="公共动作序列", ToolTip="SpellStarted 时由服务器按数组顺序执行的 DataDriven Action。", TitleProperty="Type"))
	TArray<FCombatAbilityAction> Actions;

	/** 技能被授予期间保持生效的被动或法球 Modifier；重复同步只刷新同一实例，技能移除时一并清理。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="固有 Modifier", ToolTip="该 AbilitySpec 存在期间幂等施加的被动或法球 Modifier；为空表示没有固有效果。"))
	TObjectPtr<UCombatModifierData> IntrinsicModifier = nullptr;

	/** 本法球赢得同组仲裁后，当前普通攻击改用的弹体；为空时继续使用单位默认攻击弹体。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Attack", meta=(DisplayName="法球弹体定义", ToolTip="该技能的法球胜出时冻结到攻击记录中的弹体覆盖；为空时使用单位默认弹体。"))
	TObjectPtr<UCombatProjectileData> AttackOrbProjectileData = nullptr;

	/** 本法球赢得仲裁且普通攻击命中后施加的效果，例如 Frost Arrows 的减速。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Attack", meta=(DisplayName="法球命中 Modifier", ToolTip="该技能的法球命中后施加的 Modifier，例如 Frost Arrows 的减速。"))
	TObjectPtr<UCombatModifierData> AttackOrbOnHitModifierData = nullptr;

	/** 按键和 1-based 等级读取 SpecialValues；键不存在时返回 DefaultValue，等级越界时取首值或末值。 */
	float GetSpecialValue(FName Key, int32 Level, float DefaultValue = 0.0f) const;
	/** 在运行时和自动化中执行与 Editor validator 相同的 Ability schema 校验。 */
	bool ValidateRuntime(FString& OutDiagnostic) const;

	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;

#if WITH_EDITOR
	/** 检查目标模式、等级、时序、提交策略和全部 Action。 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/**
 * Modifier 的设计模板，描述持续效果如何叠层、过期、驱散、周期触发并修改 GAS 属性或标签。
 * ModifierComponent 施加它时创建一对 Active GameplayEffect 与 Runtime：前者持续聚合属性和标签，后者承载 Hook 与 OnThink。
 * 同一来源、同一定义和同一 Ability owner 再次施加时刷新现有实例；实例可因到期、驱散、死亡清理、明确移除或 World teardown 结束。
 */
UCLASS(BlueprintType)
class UE_GAS_API UCombatModifierData : public UCombatDefinitionData
{
	GENERATED_BODY()

public:
	/** 每个活动实例使用的 C++/Blueprint Runtime 类型，用于保存状态并接收创建、刷新、周期和销毁 Hook。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="运行时类", ToolTip="每个活动 Modifier 实例使用的 Runtime 类，用于保存状态并接收创建、刷新、周期和销毁 Hook。"))
	TSubclassOf<UCombatModifierRuntime> RuntimeClass;

	/** 多个活动 Modifier 处理同一 Hook 时的第一排序键；数值越大越先执行，相同值按施加顺序执行。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="Hook 优先级", ToolTip="多个 Modifier 同时处理同一战斗事件时，数值越大越先执行；相同值按施加顺序执行。"))
	int32 Priority = 0;

	/** 相邻两次 Runtime::OnThink 的秒数；大于 0 时由 Combat Scheduler 驱动，0 表示没有周期回调。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(ClampMin="0", Units="s", DisplayName="周期触发间隔", ToolTip="大于 0 时由 Combat Scheduler 周期调用 OnThink，单位为秒；0 表示不周期触发。"))
	float ThinkInterval = 0.0f;

	/** 基础持续秒数；0 表示不会自然过期，正数到期后组件移除 Runtime 及其 Active GameplayEffect。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(ClampMin="0", Units="s", DisplayName="持续时间", ToolTip="Modifier 的基础持续时间，单位为秒；0 表示无限持续，正数由 Combat Scheduler 管理过期。"))
	float Duration = 0.0f;

	/** 同一来源、同一定义和同一 Ability owner 的最大层数；达到上限后再次施加仍刷新持续时间但不再加层。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(ClampMin="1", DisplayName="最大层数", ToolTip="同一来源、同一定义和同一所属技能的最大层数；达到上限后再次施加仍刷新持续时间但不再加层。"))
	int32 MaxStacks = 1;

	/** 重复施加并刷新已有 Runtime 时，决定下一次 OnThink 保持原计划还是从刷新时刻重新计时。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="刷新周期策略", ToolTip="重复施加并刷新已有 Modifier 时，选择下一次周期回调保持原计划，或从刷新时刻重新等待一个完整间隔。"))
	ECombatModifierRefreshPolicy RefreshPolicy = ECombatModifierRefreshPolicy::PreservePhase;

	/** 当一次 OnThink 与自然过期排在同一时刻时，是否先执行这次 OnThink；关闭时该边界周期不会生效。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="过期时执行周期", ToolTip="启用后，恰好落在过期时刻的 OnThink 仍会执行一次；关闭时先过期。"))
	bool bTickOnExpire = false;

	/** 标记为负面效果；会影响 Debuff 免疫、仅驱散 Debuff 的筛选以及状态抗性是否可缩短持续时间。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="属于负面效果", ToolTip="标记为 Debuff 后会参与负面效果免疫、仅驱散负面效果的筛选，并可选择受状态抗性缩短。"))
	bool bIsDebuff = false;

	/** 是否按目标当前 StatusResistancePct 缩短本次 Debuff；例如 10 秒效果遇到 25% 状态抗性后持续 7.5 秒。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(EditCondition="bIsDebuff", DisplayName="受状态抗性影响", ToolTip="启用后按目标当前状态抗性缩短本次 Debuff；例如 25% 状态抗性会把 10 秒缩短到 7.5 秒。"))
	bool bDurationAffectedByStatusResistance = false;

	/** 决定普通驱散、强驱散能否移除该 Modifier；不会阻止到期、死亡清理或明确移除。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="驱散规则", ToolTip="选择普通驱散、强驱散能否移除该 Modifier；不可驱散仍可自然过期、死亡清理或由移除接口结束。"))
	ECombatModifierDispelRule DispelRule = ECombatModifierDispelRule::Basic;

	/** Runtime 存活期间持续生效的 GAS 属性修改；移除 Modifier 时这些修改一并撤销。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="属性修改", ToolTip="Modifier 存活期间持续生效的 GAS 属性修改列表；Modifier 移除时这些修改一并撤销。", TitleProperty="Attribute"))
	TArray<FCombatModifierAttributeChange> AttributeChanges;

	/** Modifier 存活期间授予目标 ASC 的可计数标签；最后一个同类来源移除后标签才消失。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="授予状态标签", ToolTip="Modifier 存活期间授予目标 ASC 的可计数状态标签；最后一个同类来源移除后标签才消失。"))
	FGameplayTagContainer GrantedTags;

	/** Runtime 按键读取的默认参数；本次 Apply 请求携带同名 RuntimeParameterOverrides 时优先使用覆盖值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="运行时参数", ToolTip="Modifier Runtime 按稳定名称读取的默认参数，例如 shield_amount 或 damage_per_tick；本次施加请求的同名覆盖值优先。"))
	TMap<FName, float> RuntimeParameters;

	/** 开启后单位进入 Dying 时立即移除；关闭后暂停其 Hook 和周期，并在复活时恢复尚未过期的实例。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="死亡时移除", ToolTip="启用后单位进入 Dying 时立即移除；关闭后暂停 Hook 和周期，复活时恢复尚未过期的实例。"))
	bool bRemoveOnDeath = true;

	/** 开启后目标带 State.Broken 时跳过战斗事件 Hook、法球和 SpellBlock；不会暂停 OnThink，也不会撤销属性或标签。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="受 Break 禁用", ToolTip="启用后，State.Broken 会跳过该 Modifier 的战斗事件 Hook、法球和 SpellBlock；周期 OnThink、属性与标签仍保持生效。"))
	bool bDisabledByBreak = false;

	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;

#if WITH_EDITOR
	/** 检查周期、持续、层数、属性修改和 Runtime 参数。 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/**
 * 服务器权威弹体的默认模板，配置 Actor 类型、运动方式、碰撞宽度、飞行上限和命中策略。
 * ProjectileSubsystem 在生成时把这些值复制到不可变 Spec；之后修改资产或结束来源 Ability 都不会自动改变已发射弹体。
 */
UCLASS(BlueprintType)
class UE_GAS_API UCombatProjectileData : public UCombatDefinitionData
{
	GENERATED_BODY()

public:
	/** 服务器实际生成的弹体 Actor 类；为空时使用默认 ACombatProjectileActor。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="弹体 Actor 类", ToolTip="服务器生成的权威弹体 Actor 类型；为空时使用 ACombatProjectileActor。"))
	TSubclassOf<ACombatProjectileActor> ProjectileActorClass;

	/** 选择沿发射方向直线飞行，或每帧朝有效目标位置追踪。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="运动类型", ToolTip="选择沿初始方向直线运动，或每帧追踪仍合法的目标。"))
	ECombatProjectileMovementType MovementType = ECombatProjectileMovementType::Linear;

	/** 追踪目标死亡或失效后，是立即无命中结束，还是飞到最后有效位置后再结束。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="追踪目标丢失策略", ToolTip="追踪目标失效后，选择立即结束或继续飞向最后一次合法位置。"))
	ECombatProjectileTargetLostPolicy TargetLostPolicy = ECombatProjectileTargetLostPolicy::Fizzle;

	/** 配置可命中的阵营、能否命中来源自身、命中首个单位后是否穿透，以及世界碰撞是否停止弹体。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="默认命中策略", ToolTip="配置弹体能命中的阵营、自身命中、首个单位命中结束和世界阻挡行为。"))
	FCombatProjectileHitPolicy HitPolicy;

	/** 弹体线性速度，单位为厘米/秒。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(ClampMin="0", Units="cm/s", DisplayName="飞行速度", ToolTip="弹体的基础线性速度，单位为厘米/秒且必须大于 0；技能动作可以在生成快照时覆盖。"))
	float Speed = 0.0f;

	/** 弹体每帧沿移动路径执行球形碰撞检测的半径，单位为厘米。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(ClampMin="0", Units="cm", DisplayName="碰撞半径", ToolTip="弹体连续 sweep 使用的基础半径，单位为厘米；技能动作可以在生成快照时覆盖。"))
	float Radius = 0.0f;

	/** 从生成点累计飞行的距离上限，单位为厘米；达到后无命中结束。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(ClampMin="0", Units="cm", DisplayName="最大飞行距离", ToolTip="弹体从生成点累计飞行的最大距离，单位为厘米且必须大于 0；到达后以 MaxDistance 结束。"))
	float MaxDistance = 0.0f;

	/** 弹体最多存活的秒数；0 表示不按时间结束，正数到期后以 Timeout 结束。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(ClampMin="0", Units="s", DisplayName="最大生存时间", ToolTip="弹体允许存活的最长时间，单位为秒；0 表示不按时间结束，只受距离和其他结束条件影响。"))
	float MaxLifetime = 10.0f;

	/** 每次碰撞检测允许覆盖的最大路径长度；高速弹体会拆成多个子步，越小越精确但开销越高。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(ClampMin="1", Units="cm", DisplayName="最大模拟步长", ToolTip="单次碰撞 sweep 允许的最大路径长度，单位为厘米；高速弹体会据此拆分 substep。"))
	float MaxSimulationStep = 100.0f;

	/** 弹体碰撞组件使用的固定 Profile 名。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="碰撞配置名称", ToolTip="弹体碰撞组件使用的 Collision Profile 名称；项目默认值为 CombatProjectile。"))
	FName CollisionProfileName = TEXT("CombatProjectile");

	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;

#if WITH_EDITOR
	/** 检查速度、半径、距离、寿命、substep 和碰撞 Profile。 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** 描述单位初始化时授予的一项技能，包括 Ability 类、初始等级和默认 AutoCast 状态。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAbilitySetEntry
{
	GENERATED_BODY()

	/** 待授予的 GameplayAbility 类。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|AbilitySet", meta=(DisplayName="技能类", ToolTip="单位初始化时授予的 Combat GameplayAbility 类；不能为空且同一集合中不能重复。"))
	TSubclassOf<UCombatGameplayAbility> AbilityClass;

	/** 写入 AbilitySpec.Level 的初始权威等级。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|AbilitySet", meta=(ClampMin="1", DisplayName="初始等级", ToolTip="授予后写入 AbilitySpec.Level 的权威等级，最小值为 1。"))
	int32 InitialLevel = 1;

	/** 授予后是否默认启用 AutoCast。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|AbilitySet", meta=(DisplayName="默认启用自动施法", ToolTip="启用后，技能授予完成时默认打开 AutoCast；技能仍需声明并满足自动施法规则。"))
	bool bAutoCastEnabled = false;
};

/** 可复用的技能装配表；Unit 初始化时按数组顺序授予各 AbilitySpec，并应用初始等级与 AutoCast 状态。 */
UCLASS(BlueprintType)
class UE_GAS_API UCombatAbilitySet : public UCombatDefinitionData
{
	GENERATED_BODY()

public:
	/** 按配置顺序授予的 Ability 条目。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|AbilitySet", meta=(DisplayName="技能授予条目", ToolTip="单位初始化时按数组顺序授予的技能类、等级和默认 AutoCast 状态。", TitleProperty="AbilityClass"))
	TArray<FCombatAbilitySetEntry> Abilities;

	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;

#if WITH_EDITOR
	/** 检查 Ability 类非空、等级合法且没有重复类。 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
