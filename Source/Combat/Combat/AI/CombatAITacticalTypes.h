#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "Combat/Order/CombatOrderTypes.h"
#include "CombatAITacticalTypes.generated.h"

class ACombatUnitCharacter;

/** StateTree Utility 分支类别；硬性归位与控制权不属于该枚举。 */
UENUM(BlueprintType)
enum class ECombatAITacticalAction : uint8
{
	Guard UMETA(DisplayName="观察等待"),
	Attack UMETA(DisplayName="普通攻击"),
	Cast UMETA(DisplayName="主动施法"),
	Reposition UMETA(DisplayName="战术站位")
};

/** 技能在 AI 决策中的用途标签；只影响偏好，不改变 Ability 的真实效果。 */
UENUM(BlueprintType)
enum class ECombatAIAbilityIntentRole : uint8
{
	SingleTargetDamage UMETA(DisplayName="单体输出"),
	AreaDamage UMETA(DisplayName="范围输出"),
	Control UMETA(DisplayName="控制"),
	Heal UMETA(DisplayName="治疗"),
	Defense UMETA(DisplayName="防御"),
	Mobility UMETA(DisplayName="位移")
};

/** AI 为公共施法命令提供哪类可观察目标；技能定义仍决定真实目标模式。 */
UENUM(BlueprintType)
enum class ECombatAIAbilityTargetPolicy : uint8
{
	Self UMETA(DisplayName="自身"),
	CurrentEnemy UMETA(DisplayName="当前敌人"),
	CurrentEnemyLocation UMETA(DisplayName="当前敌人位置"),
	/** 保留既有序列化数值；阶段 C 的 EQS 只服务 Reposition，配置校验明确拒绝此项。 */
	TacticalLocation UMETA(Hidden)
};

/** 普通战术候选是否值得等待当前攻击的合法边界；不会赋予强制打断权限。 */
UENUM(BlueprintType)
enum class ECombatAIInterruptPreference : uint8
{
	Never UMETA(DisplayName="不切换当前动作"),
	AttackBoundary UMETA(DisplayName="攻击边界切换")
};

/**
 * Profile 对一个已授予技能的用途偏好；DefinitionId 只解析当前 ASC 的 Spec。
 * 范围、费用、冷却和目标合法性继续由 AbilityData、ASC 与 Order 公共预检决定。
 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatAIAbilityUsageRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Tactics", meta=(DisplayName="技能定义", ToolTip="必须是唯一的 CombatAbility PrimaryAssetId；运行时只解析本单位当前已授予的技能。"))
	FPrimaryAssetId AbilityDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Tactics", meta=(DisplayName="技能用途", ToolTip="用于选择评分启发式，不改变技能效果、伤害或控制规则。"))
	ECombatAIAbilityIntentRole IntentRole = ECombatAIAbilityIntentRole::SingleTargetDamage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Tactics", meta=(DisplayName="目标策略", ToolTip="指定 AI 从自身、当前可观察敌人或其位置构造施法意图；必须与技能真实目标模式匹配。阶段 C 的 EQS 仅用于战术站位，不作为技能目标。"))
	ECombatAIAbilityTargetPolicy TargetPolicy = ECombatAIAbilityTargetPolicy::CurrentEnemy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Tactics", meta=(DisplayName="基础效用", ToolTip="合法候选的基础分数，范围 0 到 1；最终仍由公共预检和 StateTree 条件决定能否进入。", ClampMin="0", ClampMax="1"))
	float BaseUtility = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Tactics", meta=(DisplayName="最低有效目标数", ToolTip="可观察目标少于此数时不生成候选；范围 1 到 64，不代表最终命中数量。", ClampMin="1", ClampMax="64"))
	int32 MinimumEffectiveTargets = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Tactics", meta=(DisplayName="施法后法力保留比例", ToolTip="偏好层要求施法后至少保留的最大法力比例，范围 0 到 1；不会改变 ASC 的真实费用。", ClampMin="0", ClampMax="1"))
	float ManaReserveRatio = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Tactics", meta=(DisplayName="普通切换偏好", ToolTip="选择攻击边界时只申请已有 Order 交接；选择不切换时不会为该候选打断当前动作。"))
	ECombatAIInterruptPreference InterruptPreference = ECombatAIInterruptPreference::AttackBoundary;

	/** 校验配置身份和有界数值；不解析技能资产或已授予 Spec。 */
	bool Validate(FString* OutDiagnostic = nullptr) const;
};

/** 一次无副作用评估得到的最佳施法候选；目标使用弱身份并保存生命代次。 */
struct COMBAT_API FCombatAIAbilityCandidate
{
	FPrimaryAssetId AbilityDefinitionId;
	FGameplayAbilitySpecHandle SpecHandle;
	int32 RuleIndex = INDEX_NONE;
	ECombatOrderType OrderType = ECombatOrderType::CastNoTarget;
	ECombatAIAbilityIntentRole IntentRole = ECombatAIAbilityIntentRole::SingleTargetDamage;
	ECombatAIInterruptPreference InterruptPreference = ECombatAIInterruptPreference::Never;
	TWeakObjectPtr<ACombatUnitCharacter> Target;
	uint32 TargetLife = 0;
	FVector TargetLocation = FVector::ZeroVector;
	bool bHasTargetLocation = false;
	bool bValid = false;
	float BaseUtility = 0.0f;
	float NeedUtility = 0.0f;
	float FinalUtility = 0.0f;

	/** 重建供公共 Order 预检/提交的短生命周期请求；调用方仍须复核目标生命。 */
	FCombatOrderRequest MakeOrderRequest() const;
};

/** Brain 发布的本轮战术只读快照；StateTree 节点只消费，不在评分时写入。 */
struct COMBAT_API FCombatAITacticalSnapshot
{
	uint64 Revision = 0;
	double EvaluatedAt = 0.0;
	FCombatAIAbilityCandidate BestAbility;
	float GuardUtility = 0.0f;
	float AttackUtility = 0.0f;
	float RepositionUtility = 0.0f;

	/** 返回已由纯函数约束到 [0,1] 的分支分数。 */
	float GetScore(ECombatAITacticalAction Action) const;
};

/**
 * StateTree 战术效用的纯数学入口；不读取 World，也不决定候选是否合法。
 * 调用方必须先通过 Condition 或公共预检过滤候选，再用这里的结果比较合法分支。
 */
struct COMBAT_API FCombatAIUtilityScoring
{
	/** 将有限分数约束到 [0,1]；NaN 和无穷统一退化为 0，避免污染 StateTree 选择。 */
	static float ClampScore(float Score);

	/**
	 * 当前动作达到最短保持时间且挑战者严格超过切换分差时返回 true。
	 * 非有限的时间或门槛拒绝切换；负保持时间和负分差按 0 处理。
	 */
	static bool ShouldSwitch(float CurrentScore, float ChallengerScore, double HeldSeconds,
		float MinHoldSeconds, float SwitchMargin);
};
