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

/** 定义 Modifier 被驱散时需要的最低驱散强度。 */
UENUM(BlueprintType)
enum class ECombatModifierDispelRule : uint8
{
	/** Basic 或 Strong Dispel 都可以移除。 */
	Basic UMETA(DisplayName="基础驱散可移除"),
	/** 只有 Strong Dispel 可以移除。 */
	StrongOnly UMETA(DisplayName="仅强驱散可移除"),
	/** 任何 Dispel 都不能移除。 */
	NotDispellable UMETA(DisplayName="不可驱散")
};

/** 定义刷新 Modifier 后周期相位如何处理。 */
UENUM(BlueprintType)
enum class ECombatModifierRefreshPolicy : uint8
{
	/** 保留现有 Think 相位，只更新层数与 ExpireAt。 */
	PreservePhase UMETA(DisplayName="保留周期相位"),
	/** 从刷新时刻重新开始 Think 间隔。 */
	ResetInterval UMETA(DisplayName="重置周期间隔")
};

/** 描述 Modifier ActiveGE 对一个 Attribute 的聚合修改。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatModifierAttributeChange
{
	GENERATED_BODY()

	/** 待修改的 GAS Attribute。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="目标属性", ToolTip="选择该 Modifier 存活期间由 Active GameplayEffect 聚合修改的 GAS 属性。")) FGameplayAttribute Attribute;
	/** Additive、Multiplicitive 等 GAS 聚合操作。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="属性修改运算", ToolTip="选择幅值参与 GAS 属性聚合的运算方式，例如加法或乘法。")) TEnumAsByte<EGameplayModOp::Type> ModifierOp = EGameplayModOp::Additive;
	/** 写入动态 GameplayEffect 的静态 magnitude。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="静态幅值", ToolTip="未配置幅值参数键或本次施加没有对应覆盖值时，写入动态 GameplayEffect 的属性修改幅值。")) float Magnitude = 0.0f;
	/** 非 None 时优先从 Apply 请求的 RuntimeParameterOverrides 读取 magnitude。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="幅值参数键", ToolTip="非 None 时优先从本次施加请求的运行时参数覆盖中读取属性修改幅值。")) FName MagnitudeParameterKey;
};

/** 描述一个旧 DefinitionId 到新 DefinitionId 的显式版本迁移。 */
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

/** 提供 DefinitionId 集合校验、重定向解析与缺失占位文本。 */
struct UE_GAS_API FCombatDefinitionRegistry
{
	/** 当前 Combat 内容 schema 版本。 */
	static constexpr int32 CombatContentVersion = 1;

	/** 校验重定向的 ID、环路、重复源和已知目标，错误追加到 OutErrors。 */
	static bool ValidateRedirects(
		const TArray<FCombatDefinitionRedirect>& Redirects,
		const TSet<FPrimaryAssetId>& KnownIds,
		TArray<FString>& OutErrors);

	/** 沿显式重定向链解析 ID；解析到已知定义时返回 true。 */
	static bool ResolveDefinitionId(
		const FPrimaryAssetId& RequestedId,
		const TArray<FCombatDefinitionRedirect>& Redirects,
		const TSet<FPrimaryAssetId>& KnownIds,
		FPrimaryAssetId& OutResolvedId);

	/** 为无法解析的定义生成稳定且可诊断的占位文本。 */
	static FString MakeMissingPlaceholder(const FPrimaryAssetId& MissingId);
};

/** 保存按 1-based Ability Level 查询的一组可配置数值。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatSpecialValue
{
	GENERATED_BODY()

	/** 从 Level 1 开始排列的数值表。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="分级数值", ToolTip="从技能等级 1 开始依次填写；数量必须覆盖最大等级，查询越界时会稳定使用首值或末值。"))
	TArray<float> Values;

	/** 返回指定等级的数值；越界时使用首/末值进行稳定降级。 */
	float GetValueAtLevel(int32 Level) const;
	/** 检查数值数量是否覆盖 MaxLevel 且全部为有限值。 */
	bool IsValidForMaxLevel(int32 MaxLevel) const;
};

/**
 * 所有 Combat PrimaryDataAsset 的稳定身份与 schema 基类。
 * 派生定义通过唯一 lower_snake_case DefinitionName 生成 PrimaryAssetId，并在编辑器、运行时和迁移校验中共享同一内容版本边界。
 */
UCLASS(Abstract, BlueprintType)
class UE_GAS_API UCombatDefinitionData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** lower_snake_case 稳定定义名，与资产路径解耦。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category="Combat|Identity", meta=(DisplayName="稳定定义名", ToolTip="用于生成 PrimaryAssetId 的全局唯一 lower_snake_case 名称；重命名资产文件不会改变此身份。"))
	FName DefinitionName;

	/** 当前资产数据的 schema 版本。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category="Combat|Identity", meta=(DisplayName="数据结构版本", ToolTip="当前 Combat 定义的数据结构版本，由代码维护并用于内容兼容与迁移校验。"))
	int32 SchemaVersion = 1;

	/** 组合固定 PrimaryAssetType 与 DefinitionName 生成稳定 ID。 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	/** 返回派生定义类型对应的固定 Combat PrimaryAssetType。 */
	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const;

	/** 检查定义名是否满足 lower_snake_case 身份规则。 */
	static bool IsValidDefinitionName(FName Name);
	/** 检查定义集合是否存在空资产、无效 ID 或重复 ID。 */
	static bool ValidateDefinitionSet(const TArray<const UCombatDefinitionData*>& Definitions, TArray<FString>& OutErrors);

#if WITH_EDITOR
	/** 在 Editor 保存/验证时检查基础身份与 schema。 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** 定义战斗单位的初始队伍、碰撞覆盖与固有 AbilitySet。 */
UCLASS(BlueprintType)
class UE_GAS_API UCombatUnitData : public UCombatDefinitionData
{
	GENERATED_BODY()

public:
	/** 服务器初始化时写入 AttributeSet 的基础数值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit", meta=(DisplayName="基础战斗属性", ToolTip="服务器初始化单位时写入 Combat AttributeSet 的基础属性集合；展开后可配置生命、攻击、移速等数值。"))
	FCombatUnitBaseStats BaseStats;

	/** Unit 生成时使用的初始战斗队伍。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit", meta=(DisplayName="初始队伍", ToolTip="单位生成时使用的战斗队伍；0 表示中立，1 到 254 表示有效队伍，255 表示无效。"))
	FCombatTeamId InitialTeamId = FCombatTeamId(1);

	/** 大于 0 时覆盖 Character 默认胶囊半径，单位为厘米。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit", meta=(ClampMin="0", Units="cm", DisplayName="胶囊半径覆盖", ToolTip="大于 0 时覆盖 Character 默认碰撞胶囊半径，单位为厘米；0 表示保留 Character 配置。"))
	float CapsuleRadiusOverride = 0.0f;

	/** AttackSpeed=100 时从起手到 AttackLaunched 的基础前摇秒数。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(ClampMin="0", Units="s", DisplayName="基础攻击前摇", ToolTip="AttackSpeed 为 100 时，从攻击起手到生成 AttackLaunched 的基础时长，单位为秒。"))
	float BaseAttackPoint = 0.3f;

	/** 普攻起手前允许的最大 XY 朝向误差；Order 会先服务器转向再复核。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(ClampMin="0", ClampMax="180", Units="deg", DisplayName="攻击朝向容差", ToolTip="普通攻击起手前允许的最大水平朝向误差，范围为 0 到 180 度；超出时 Order 会先在服务器转向。"))
	float AttackFacingToleranceDegrees = 15.0f;

	/** true 时允许 CharacterMovement 仍在移动时直接创建 AttackRecord。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(DisplayName="允许移动中攻击", ToolTip="启用后，CharacterMovement 仍在移动时也可以创建普通攻击记录。"))
	bool bAllowAttackWhileMoving = false;

	/** 普攻起手与 impact 是否复用 CombatTargeting LOS。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(DisplayName="攻击需要视线", ToolTip="启用后，普通攻击起手和命中都通过 CombatTargeting 重新检查几何视线。"))
	bool bRequireAttackLineOfSight = false;

	/** 创建 AttackRecord 时快照的基础暴击概率。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(ClampMin="0", ClampMax="1", DisplayName="基础暴击概率", ToolTip="创建普通攻击记录时快照的暴击概率，取值 0 到 1；例如 0.25 表示 25%。"))
	float CriticalStrikeChance = 0.0f;

	/** Crit roll 成功后乘到主攻击伤害的基础倍率。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(ClampMin="1", DisplayName="基础暴击倍率", ToolTip="暴击成功后乘到普通攻击主伤害上的倍率，最小值为 1；例如 2 表示 200% 伤害。"))
	float CriticalStrikeMultiplier = 2.0f;

	/** 非空时普通攻击在 attack point 生成 Tracking Attack Projectile。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(DisplayName="普通攻击弹体定义", ToolTip="非空时，普通攻击在攻击前摇结束后生成追踪攻击弹体；为空时按近战命中结算。"))
	TObjectPtr<UCombatProjectileData> AttackProjectileData = nullptr;

	/** Unit 初始化时按顺序授予的 AbilitySet 软引用。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit", meta=(DisplayName="初始技能集合", ToolTip="单位初始化时按数组顺序加载并授予的 AbilitySet 软引用。"))
	TArray<TSoftObjectPtr<UCombatAbilitySet>> AbilitySets;

	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;

#if WITH_EDITOR
	/** 检查基础属性、攻击策略、初始队伍、胶囊和 AbilitySet 引用。 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** 定义等级、目标、生命周期提交策略与 DataDriven Actions。 */
UCLASS(BlueprintType)
class UE_GAS_API UCombatAbilityData : public UCombatDefinitionData
{
	GENERATED_BODY()

public:
	/** AbilitySpec 允许的最大权威等级。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(ClampMin="1", DisplayName="最大等级", ToolTip="该 AbilitySpec 允许的最大权威等级；SpecialValues 的每个数组都必须覆盖此等级。"))
	int32 MaxLevel = 1;

	/** 描述目标模式、被动、引导和 AutoCast 等行为。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(Categories="Ability.Behavior", DisplayName="行为标签", ToolTip="声明目标模式、被动、引导、自动施法、可被法术格挡等技能行为；组合必须通过 Ability 数据校验。"))
	FGameplayTagContainer BehaviorTags;

	/** Unit/Point 目标的阵营、状态、范围、LOS 与可见性规则。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="目标规则", ToolTip="服务器用于复核单位或点目标的阵营、状态、范围、视线和可见性规则。"))
	FCombatTargetingRules TargetingRules;

	/** 以稳定字段名索引的等级数值表。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="技能分级参数", ToolTip="以稳定参数名索引的分级数值表；Action 通过对应键按当前技能等级读取。"))
	TMap<FName, FCombatSpecialValue> SpecialValues;

	/** 前摇时间；gameplay 完成点只由 Combat Scheduler 驱动。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(ClampMin="0", Units="s", DisplayName="施法前摇", ToolTip="从激活到 SpellStarted 的服务器权威时长，单位为秒；由 Combat Scheduler 驱动。"))
	float CastPoint = 0.0f;

	/** 引导总时长；只有 Channelled Ability 可以大于 0。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(ClampMin="0", Units="s", DisplayName="引导总时长", ToolTip="引导技能从 SpellStarted 到 ChannelFinish 的总时长，单位为秒；只有带引导行为标签时可以大于 0。"))
	float ChannelDuration = 0.0f;

	/** 引导逻辑 tick 间隔。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(ClampMin="0", Units="s", DisplayName="引导触发间隔", ToolTip="引导期间两次 ChannelTick 之间的时长，单位为秒；必须与引导总时长和行为标签一致。"))
	float ChannelInterval = 0.0f;

	/** ManaCost 在哪个生命周期阶段提交。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="费用提交阶段", ToolTip="选择 ManaCost 在技能激活生命周期中的权威提交阶段。"))
	ECombatAbilityCommitStage CostCommitPoint = ECombatAbilityCommitStage::SpellStarted;

	/** Cooldown 在哪个生命周期阶段提交。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="冷却提交阶段", ToolTip="选择 Cooldown 在技能激活生命周期中的权威提交阶段。"))
	ECombatAbilityCommitStage CooldownCommitPoint = ECombatAbilityCommitStage::SpellStarted;

	/** UnitTarget 在 cast point 失效后的处理策略。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="目标丢失策略", ToolTip="单位目标在施法前摇结束时失效后的处理方式；仅点或范围行为可继续使用最后已知位置。"))
	ECombatTargetLostPolicy TargetLostPolicy = ECombatTargetLostPolicy::Fail;

	/** 引导中断后 OrderComponent 处理后续队列的策略。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="引导中断队列策略", ToolTip="引导被中断后，OrderComponent 是保留后续命令还是清空尚未执行的队列。"))
	ECombatChannelInterruptOrderPolicy ChannelInterruptOrderPolicy = ECombatChannelInterruptOrderPolicy::Continue;

	/** true 时 Ability End 取消本 Activation 显式绑定的 Projectile；默认 fire-and-forget。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="技能结束时取消弹体", ToolTip="启用后，Ability 结束时取消本次激活显式绑定的弹体；关闭时弹体发射后独立完成。"))
	bool bCancelProjectilesWithAbility = false;

	/** SpellStarted 时按顺序执行的服务器公共动作。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="公共动作序列", ToolTip="SpellStarted 时由服务器按数组顺序执行的 DataDriven Action。", TitleProperty="Type"))
	TArray<FCombatAbilityAction> Actions;

	/** AbilitySpec 存在期间幂等施加的固有 Modifier。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="固有 Modifier", ToolTip="该 AbilitySpec 存在期间幂等施加的被动或法球 Modifier；为空表示没有固有效果。"))
	TObjectPtr<UCombatModifierData> IntrinsicModifier = nullptr;

	/** Attack 法球 winner 可快照到 AttackRecord 的弹体覆盖。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Attack", meta=(DisplayName="法球弹体定义", ToolTip="该技能的法球胜出时冻结到攻击记录中的弹体覆盖；为空时使用单位默认弹体。"))
	TObjectPtr<UCombatProjectileData> AttackOrbProjectileData = nullptr;

	/** Attack 法球命中后施加的 Modifier，例如 Frost Arrows slow。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Attack", meta=(DisplayName="法球命中 Modifier", ToolTip="该技能的法球命中后施加的 Modifier，例如 Frost Arrows 的减速。"))
	TObjectPtr<UCombatModifierData> AttackOrbOnHitModifierData = nullptr;

	/** 读取指定等级的 special；缺失键返回 DefaultValue。 */
	float GetSpecialValue(FName Key, int32 Level, float DefaultValue = 0.0f) const;
	/** 在运行时和自动化中执行与 Editor validator 相同的 Ability schema 校验。 */
	bool ValidateRuntime(FString& OutDiagnostic) const;

	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;

#if WITH_EDITOR
	/** 检查目标模式、等级、时序、提交策略和全部 Action。 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** 定义 Modifier 的稳定优先级、周期与死亡清理策略。 */
UCLASS(BlueprintType)
class UE_GAS_API UCombatModifierData : public UCombatDefinitionData
{
	GENERATED_BODY()

public:
	/** 与 ActiveGE 一一对应的 C++/Blueprint Runtime 类型。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="运行时类", ToolTip="与 Active GameplayEffect 一一对应的 Modifier Runtime 类；用于承载有状态 Hook 和周期行为。"))
	TSubclassOf<UCombatModifierRuntime> RuntimeClass;

	/** Hook 排序的第一关键字，数值越大越先执行。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="Hook 优先级", ToolTip="多个 Modifier Hook 同时执行时的第一排序键；数值越大越先执行。"))
	int32 Priority = 0;

	/** 大于 0 时由 Combat Scheduler 驱动的周期秒数。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(ClampMin="0", Units="s", DisplayName="周期触发间隔", ToolTip="大于 0 时由 Combat Scheduler 周期调用 OnThink，单位为秒；0 表示不周期触发。"))
	float ThinkInterval = 0.0f;

	/** 0 表示无限持续；正数由 Combat Scheduler 管理绝对过期时间。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(ClampMin="0", Units="s", DisplayName="持续时间", ToolTip="Modifier 的基础持续时间，单位为秒；0 表示无限持续，正数由 Combat Scheduler 管理过期。"))
	float Duration = 0.0f;

	/** 同一来源和定义最多允许的 Runtime 层数。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(ClampMin="1", DisplayName="最大层数", ToolTip="同一来源和同一定义最多允许共存的 Runtime 层数，最小值为 1。"))
	int32 MaxStacks = 1;

	/** 刷新已存在 Runtime 时采用的周期相位策略。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="刷新周期策略", ToolTip="已有 Runtime 被刷新时，选择保留当前周期相位或从刷新时刻重置间隔。"))
	ECombatModifierRefreshPolicy RefreshPolicy = ECombatModifierRefreshPolicy::PreservePhase;

	/** 恰好位于 ExpireAt 的周期 tick 是否执行。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="过期时执行周期", ToolTip="启用后，恰好落在过期时刻的 OnThink 仍会执行一次；关闭时先过期。"))
	bool bTickOnExpire = false;

	/** 标记该 Modifier 是否属于 Debuff。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="属于负面效果", ToolTip="标记该 Modifier 是否为 Debuff；状态抗性相关配置只对 Debuff 生效。"))
	bool bIsDebuff = false;

	/** Debuff 持续时间是否按目标当前 StatusResistancePct 缩短。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(EditCondition="bIsDebuff", DisplayName="受状态抗性影响", ToolTip="启用后，Debuff 的实际持续时间按目标当前 StatusResistancePct 缩短。"))
	bool bDurationAffectedByStatusResistance = false;

	/** 该 Modifier 对 Basic/Strong Dispel 的响应规则。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="驱散规则", ToolTip="选择基础驱散、强驱散能否移除该 Modifier。"))
	ECombatModifierDispelRule DispelRule = ECombatModifierDispelRule::Basic;

	/** ActiveGE 在 Runtime 存活期间聚合的 Attribute 修改。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="属性修改", ToolTip="Active GameplayEffect 在该 Runtime 存活期间持续聚合的属性修改列表。", TitleProperty="Attribute"))
	TArray<FCombatModifierAttributeChange> AttributeChanges;

	/** ActiveGE 在 Runtime 存活期间贡献的可计数状态标签。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="授予状态标签", ToolTip="Active GameplayEffect 在 Runtime 存活期间向目标 ASC 贡献的可计数状态标签。"))
	FGameplayTagContainer GrantedTags;

	/** Runtime 使用的只读参数，例如 shield_amount、damage_per_tick。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="运行时参数", ToolTip="以稳定名称提供给 Modifier Runtime 的只读参数，例如 shield_amount 或 damage_per_tick。"))
	TMap<FName, float> RuntimeParameters;

	/** Unit 进入死亡清理时是否移除该 Modifier。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="死亡时移除", ToolTip="启用后，单位进入死亡清理时移除该 Modifier；关闭时允许它跨死亡阶段保留。"))
	bool bRemoveOnDeath = true;

	/** State.Broken 存在时是否暂停该 Runtime 的 Hook/法球行为。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="受 Break 禁用", ToolTip="启用后，目标具有 State.Broken 时暂停该 Runtime 的 Hook、周期和法球行为。"))
	bool bDisabledByBreak = false;

	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;

#if WITH_EDITOR
	/** 检查周期、持续、层数、属性修改和 Runtime 参数。 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** 定义弹体的基础运动、半径、最大距离与碰撞 Profile。 */
UCLASS(BlueprintType)
class UE_GAS_API UCombatProjectileData : public UCombatDefinitionData
{
	GENERATED_BODY()

public:
	/** 权威 Actor 类型；为空时使用 ACombatProjectileActor。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="弹体 Actor 类", ToolTip="服务器生成的权威弹体 Actor 类型；为空时使用 ACombatProjectileActor。"))
	TSubclassOf<ACombatProjectileActor> ProjectileActorClass;

	/** Linear 或 Tracking 连续运动。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="运动类型", ToolTip="选择沿初始方向直线运动，或每帧追踪仍合法的目标。"))
	ECombatProjectileMovementType MovementType = ECombatProjectileMovementType::Linear;

	/** Tracking 目标失效后 fizzle 或去最后已知点。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="追踪目标丢失策略", ToolTip="追踪目标失效后，选择立即结束或继续飞向最后一次合法位置。"))
	ECombatProjectileTargetLostPolicy TargetLostPolicy = ECombatProjectileTargetLostPolicy::Fizzle;

	/** 阵营、穿透、first-hit 与 world-stop 默认策略。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="默认命中策略", ToolTip="配置弹体能命中的阵营、自身命中、首个单位命中结束和世界阻挡行为。"))
	FCombatProjectileHitPolicy HitPolicy;

	/** 弹体线性速度，单位为厘米/秒。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(ClampMin="0", Units="cm/s", DisplayName="飞行速度", ToolTip="弹体的基础线性速度，单位为厘米/秒且必须大于 0；技能动作可以在生成快照时覆盖。"))
	float Speed = 0.0f;

	/** 碰撞 sweep 半径，单位为厘米。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(ClampMin="0", Units="cm", DisplayName="碰撞半径", ToolTip="弹体连续 sweep 使用的基础半径，单位为厘米；技能动作可以在生成快照时覆盖。"))
	float Radius = 0.0f;

	/** 超出后 fizzle 的最大飞行距离，单位为厘米。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(ClampMin="0", Units="cm", DisplayName="最大飞行距离", ToolTip="弹体从生成点累计飞行的最大距离，单位为厘米且必须大于 0；到达后以 MaxDistance 结束。"))
	float MaxDistance = 0.0f;

	/** 0 表示只按距离结束；正数到期时 timeout。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(ClampMin="0", Units="s", DisplayName="最大生存时间", ToolTip="弹体允许存活的最长时间，单位为秒；0 表示不按时间结束，只受距离和其他结束条件影响。"))
	float MaxLifetime = 10.0f;

	/** 单次 sweep 的最大路径长度，用于高速 substep。 */
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

/** AbilitySet 中一项待授予 Ability 的类、初始等级与 AutoCast 状态。 */
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

/** 保存可复用的一组 Ability 授予条目。 */
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
