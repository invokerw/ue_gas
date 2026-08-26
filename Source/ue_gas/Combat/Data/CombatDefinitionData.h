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
	Basic,
	/** 只有 Strong Dispel 可以移除。 */
	StrongOnly,
	/** 任何 Dispel 都不能移除。 */
	NotDispellable
};

/** 定义刷新 Modifier 后周期相位如何处理。 */
UENUM(BlueprintType)
enum class ECombatModifierRefreshPolicy : uint8
{
	/** 保留现有 Think 相位，只更新层数与 ExpireAt。 */
	PreservePhase,
	/** 从刷新时刻重新开始 Think 间隔。 */
	ResetInterval
};

/** 描述 Modifier ActiveGE 对一个 Attribute 的聚合修改。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatModifierAttributeChange
{
	GENERATED_BODY()

	/** 待修改的 GAS Attribute。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier") FGameplayAttribute Attribute;
	/** Additive、Multiplicitive 等 GAS 聚合操作。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier") TEnumAsByte<EGameplayModOp::Type> ModifierOp = EGameplayModOp::Additive;
	/** 写入动态 GameplayEffect 的静态 magnitude。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier") float Magnitude = 0.0f;
};

/** 描述一个旧 DefinitionId 到新 DefinitionId 的显式版本迁移。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatDefinitionRedirect
{
	GENERATED_BODY()

	/** 已废弃但仍可能出现在存档或网络记录中的旧 ID。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Identity")
	FPrimaryAssetId OldId;

	/** 当前可解析的目标 ID。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Identity")
	FPrimaryAssetId NewId;

	/** 首次引入该重定向的 Combat 内容版本。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Identity", meta=(ClampMin="1"))
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability")
	TArray<float> Values;

	/** 返回指定等级的数值；越界时使用首/末值进行稳定降级。 */
	float GetValueAtLevel(int32 Level) const;
	/** 检查数值数量是否覆盖 MaxLevel 且全部为有限值。 */
	bool IsValidForMaxLevel(int32 MaxLevel) const;
};

/** 所有 Combat PrimaryDataAsset 的身份和 schema 基类。 */
UCLASS(Abstract, BlueprintType)
class UE_GAS_API UCombatDefinitionData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** lower_snake_case 稳定定义名，与资产路径解耦。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category="Combat|Identity")
	FName DefinitionName;

	/** 当前资产数据的 schema 版本。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category="Combat|Identity")
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit")
	FCombatUnitBaseStats BaseStats;

	/** Unit 生成时使用的初始战斗队伍。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit")
	FCombatTeamId InitialTeamId = FCombatTeamId(1);

	/** 大于 0 时覆盖 Character 默认胶囊半径，单位为厘米。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit", meta=(ClampMin="0", Units="cm"))
	float CapsuleRadiusOverride = 0.0f;

	/** AttackSpeed=100 时从起手到 AttackLaunched 的基础前摇秒数。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(ClampMin="0", Units="s"))
	float BaseAttackPoint = 0.3f;

	/** 普攻起手前允许的最大 XY 朝向误差；Order 会先服务器转向再复核。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(ClampMin="0", ClampMax="180", Units="deg"))
	float AttackFacingToleranceDegrees = 15.0f;

	/** true 时允许 CharacterMovement 仍在移动时直接创建 AttackRecord。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack")
	bool bAllowAttackWhileMoving = false;

	/** 普攻起手与 impact 是否复用 CombatTargeting LOS。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack")
	bool bRequireAttackLineOfSight = false;

	/** 创建 AttackRecord 时快照的基础暴击概率。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(ClampMin="0", ClampMax="1"))
	float CriticalStrikeChance = 0.0f;

	/** Crit roll 成功后乘到主攻击伤害的基础倍率。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack", meta=(ClampMin="1"))
	float CriticalStrikeMultiplier = 2.0f;

	/** 非空时普通攻击在 attack point 生成 Tracking Attack Projectile。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit|Attack")
	TObjectPtr<UCombatProjectileData> AttackProjectileData = nullptr;

	/** Unit 初始化时按顺序授予的 AbilitySet 软引用。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Unit")
	TArray<TSoftObjectPtr<UCombatAbilitySet>> AbilitySets;

	/** 返回 CombatUnit PrimaryAssetType。 */
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(ClampMin="1"))
	int32 MaxLevel = 1;

	/** 描述目标模式、被动、引导和 AutoCast 等行为。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability")
	FGameplayTagContainer BehaviorTags;

	/** Unit/Point 目标的阵营、状态、范围、LOS 与可见性规则。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability")
	FCombatTargetingRules TargetingRules;

	/** 以稳定字段名索引的等级数值表。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability")
	TMap<FName, FCombatSpecialValue> SpecialValues;

	/** 前摇时间；gameplay 完成点只由 Combat Scheduler 驱动。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(ClampMin="0", Units="s"))
	float CastPoint = 0.0f;

	/** 引导总时长；只有 Channelled Ability 可以大于 0。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(ClampMin="0", Units="s"))
	float ChannelDuration = 0.0f;

	/** 引导逻辑 tick 间隔。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(ClampMin="0", Units="s"))
	float ChannelInterval = 0.0f;

	/** ManaCost 在哪个生命周期阶段提交。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability")
	ECombatAbilityCommitStage CostCommitPoint = ECombatAbilityCommitStage::SpellStarted;

	/** Cooldown 在哪个生命周期阶段提交。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability")
	ECombatAbilityCommitStage CooldownCommitPoint = ECombatAbilityCommitStage::SpellStarted;

	/** UnitTarget 在 cast point 失效后的处理策略。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability")
	ECombatTargetLostPolicy TargetLostPolicy = ECombatTargetLostPolicy::Fail;

	/** 引导中断后未来 OrderComponent 的队列策略。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability")
	ECombatChannelInterruptOrderPolicy ChannelInterruptOrderPolicy = ECombatChannelInterruptOrderPolicy::Continue;

	/** true 时 Ability End 取消本 Activation 显式绑定的 Projectile；默认 fire-and-forget。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability")
	bool bCancelProjectilesWithAbility = false;

	/** SpellStarted 时按顺序执行的服务器公共动作。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability")
	TArray<FCombatAbilityAction> Actions;

	/** AbilitySpec 存在期间幂等施加的固有 Modifier。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability")
	TObjectPtr<UCombatModifierData> IntrinsicModifier = nullptr;

	/** 读取指定等级的 special；缺失键返回 DefaultValue。 */
	float GetSpecialValue(FName Key, int32 Level, float DefaultValue = 0.0f) const;
	/** 在运行时和自动化中执行与 Editor validator 相同的 M3 schema 校验。 */
	bool ValidateRuntime(FString& OutDiagnostic) const;

	/** 返回 CombatAbility PrimaryAssetType。 */
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier")
	TSubclassOf<UCombatModifierRuntime> RuntimeClass;

	/** Hook 排序的第一关键字，数值越大越先执行。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier")
	int32 Priority = 0;

	/** 大于 0 时由 Combat Scheduler 驱动的周期秒数。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(ClampMin="0", Units="s"))
	float ThinkInterval = 0.0f;

	/** 0 表示无限持续；正数由 Combat Scheduler 管理绝对过期时间。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(ClampMin="0", Units="s"))
	float Duration = 0.0f;

	/** 同一来源和定义最多允许的 Runtime 层数。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(ClampMin="1"))
	int32 MaxStacks = 1;

	/** 刷新已存在 Runtime 时采用的周期相位策略。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier")
	ECombatModifierRefreshPolicy RefreshPolicy = ECombatModifierRefreshPolicy::PreservePhase;

	/** 恰好位于 ExpireAt 的周期 tick 是否执行。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier")
	bool bTickOnExpire = false;

	/** 标记该 Modifier 是否属于 Debuff。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier")
	bool bIsDebuff = false;

	/** Debuff 持续时间是否按目标当前 StatusResistancePct 缩短。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier", meta=(EditCondition="bIsDebuff"))
	bool bDurationAffectedByStatusResistance = false;

	/** 该 Modifier 对 Basic/Strong Dispel 的响应规则。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier")
	ECombatModifierDispelRule DispelRule = ECombatModifierDispelRule::Basic;

	/** ActiveGE 在 Runtime 存活期间聚合的 Attribute 修改。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier")
	TArray<FCombatModifierAttributeChange> AttributeChanges;

	/** ActiveGE 在 Runtime 存活期间贡献的可计数状态标签。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier")
	FGameplayTagContainer GrantedTags;

	/** Runtime 使用的只读参数，例如 shield_amount、damage_per_tick。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier")
	TMap<FName, float> RuntimeParameters;

	/** Unit 进入死亡清理时是否移除该 Modifier。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Modifier")
	bool bRemoveOnDeath = true;

	/** 返回 CombatModifier PrimaryAssetType。 */
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile")
	TSubclassOf<ACombatProjectileActor> ProjectileActorClass;

	/** Linear 或 Tracking 连续运动。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile")
	ECombatProjectileMovementType MovementType = ECombatProjectileMovementType::Linear;

	/** Tracking 目标失效后 fizzle 或去最后已知点。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile")
	ECombatProjectileTargetLostPolicy TargetLostPolicy = ECombatProjectileTargetLostPolicy::Fizzle;

	/** 阵营、穿透、first-hit 与 world-stop 默认策略。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile")
	FCombatProjectileHitPolicy HitPolicy;

	/** 弹体线性速度，单位为厘米/秒。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(ClampMin="0", Units="cm/s"))
	float Speed = 0.0f;

	/** 碰撞 sweep 半径，单位为厘米。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(ClampMin="0", Units="cm"))
	float Radius = 0.0f;

	/** 超出后 fizzle 的最大飞行距离，单位为厘米。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(ClampMin="0", Units="cm"))
	float MaxDistance = 0.0f;

	/** 0 表示只按距离结束；正数到期时 timeout。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(ClampMin="0", Units="s"))
	float MaxLifetime = 10.0f;

	/** 单次 sweep 的最大路径长度，用于高速 substep。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile", meta=(ClampMin="1", Units="cm"))
	float MaxSimulationStep = 100.0f;

	/** 弹体碰撞组件使用的固定 Profile 名。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Projectile")
	FName CollisionProfileName = TEXT("CombatProjectile");

	/** 返回 CombatProjectile PrimaryAssetType。 */
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|AbilitySet")
	TSubclassOf<UCombatGameplayAbility> AbilityClass;

	/** 写入 AbilitySpec.Level 的初始权威等级。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|AbilitySet", meta=(ClampMin="1"))
	int32 InitialLevel = 1;

	/** 授予后是否默认启用 AutoCast。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|AbilitySet")
	bool bAutoCastEnabled = false;
};

/** 保存可复用的一组 Ability 授予条目。 */
UCLASS(BlueprintType)
class UE_GAS_API UCombatAbilitySet : public UCombatDefinitionData
{
	GENERATED_BODY()

public:
	/** 按配置顺序授予的 Ability 条目。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|AbilitySet")
	TArray<FCombatAbilitySetEntry> Abilities;

	/** 返回 CombatAbilitySet PrimaryAssetType。 */
	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;

#if WITH_EDITOR
	/** 检查 Ability 类非空、等级合法且没有重复类。 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
