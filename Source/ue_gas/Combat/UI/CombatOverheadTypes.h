#pragma once

#include "CoreMinimal.h"
#include "Combat/Core/CombatTypes.h"
#include "CombatOverheadTypes.generated.h"

/** 头顶展示契约独立版本，不改变核心战斗发布契约。 */
namespace CombatOverheadPresentation
{
	inline constexpr int32 SchemaVersion = 2;
}

/** 头顶战斗跳字的表现分类；数值始终来自服务器实际落账结果。 */
UENUM(BlueprintType)
enum class ECombatFloatingTextType : uint8
{
	PhysicalDamage UMETA(DisplayName="物理伤害"),
	MagicalDamage UMETA(DisplayName="魔法伤害"),
	PureDamage UMETA(DisplayName="纯粹伤害"),
	Healing UMETA(DisplayName="治疗")
};

/** 蓝图配置的控制状态展示规则；只决定显示顺序、名称和颜色，不改变控制效果。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatControlPresentationRule
{
	GENERATED_BODY()

	/** 对应 View 白名单内的控制标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="状态标签", ToolTip="对应 View 白名单内的控制标签。"))
	FGameplayTag Tag;

	/** 该控制状态的本地化短名称。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="显示名称", ToolTip="该控制状态的本地化短名称。"))
	FText Label;

	/** 数值越大越优先；相同优先级按标签名稳定排序。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="显示优先级", ToolTip="数值越大越优先；相同优先级按标签名稳定排序。"))
	int32 Priority = 0;

	/** 主要控制状态的进度条颜色。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="显示颜色", ToolTip="主要控制状态的进度条颜色。"))
	FLinearColor Color = FLinearColor::White;
};

/** 本地只读展示快照；比例和可见性由复制 View 派生，不形成第二套战斗属性。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatOverheadDisplayData
{
	GENERATED_BODY()

	/** 单位身份与生命代次已到达；未就绪时隐藏资源信息。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="数据已就绪", ToolTip="单位身份与生命代次已到达；未就绪时隐藏资源信息。"))
	bool bReady = false;

	/** 单位存活且没有隐藏血条状态时为真。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="显示单位信息", ToolTip="单位存活且没有隐藏血条状态时为真。"))
	bool bShowInfo = false;

	/** 最大法力大于零时为真。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="显示法力条", ToolTip="最大法力大于零时为真。"))
	bool bShowMana = false;

	/** 用于清理复活前的动画和拒绝旧生命跳字。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="生命代次", ToolTip="用于清理复活前的动画和拒绝旧生命跳字。"))
	int64 LifeGeneration = 0;

	/** 通过稳定定义 ID 解析的本地化名称。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="单位名称", ToolTip="通过稳定定义 ID 解析的本地化名称。"))
	FText UnitName;

	/** 相对本地指挥单位的外交关系；未就绪时为 Invalid。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="相对阵营关系", ToolTip="相对本地指挥单位的外交关系；未就绪时为 Invalid。"))
	ECombatTeamRelation Relation = ECombatTeamRelation::Invalid;

	/** 来自 View 的当前生命，仅供文本显示。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="当前生命", ToolTip="来自 View 的当前生命，仅供文本显示。"))
	float Health = 0.0f;

	/** 来自 View 的最大生命，仅供文本与视觉刻度使用。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="最大生命", ToolTip="来自 View 的最大生命，仅供文本与视觉刻度使用。"))
	float MaxHealth = 0.0f;

	/** 有限且限制在零到一的即时生命比例。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="生命比例", ToolTip="有限且限制在零到一的即时生命比例。"))
	float HealthPercent = 0.0f;

	/** 有限且限制在零到一的法力比例。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="法力比例", ToolTip="有限且限制在零到一的法力比例。"))
	float ManaPercent = 0.0f;

	/** 存在已配置的可见控制状态时为真。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="显示控制状态", ToolTip="存在已配置的可见控制状态时为真。"))
	bool bShowStatus = false;

	/** 按展示优先级稳定排列的当前控制状态名称。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="控制状态名称", ToolTip="按展示优先级稳定排列的当前控制状态名称。"))
	FText StatusLabel;

	/** 决定主状态进度的控制标签。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="主要控制标签", ToolTip="决定主状态进度的控制标签。"))
	FGameplayTag PrimaryStatusTag;

	/** 复制蓝图规则中的主要状态颜色。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="控制状态颜色", ToolTip="复制蓝图规则中的主要状态颜色。"))
	FLinearColor StatusColor = FLinearColor::White;

	/** 服务器绝对秒数；仅用于本地进度计算。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="控制开始时间", ToolTip="服务器绝对秒数；仅用于本地进度计算。"))
	double StatusStartTime = 0.0;

	/** 服务器绝对秒数；零表示无期限或没有可见的有限时间来源。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="控制结束时间", ToolTip="服务器绝对秒数；零表示无期限或没有可见的有限时间来源。"))
	double StatusEndTime = 0.0;

	/** 当前存在有效的前摇或引导时间窗。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="显示技能", ToolTip="当前存在有效的前摇或引导时间窗。"))
	bool bShowAbility = false;

	/** 通过定义 ID 解析的本地化技能名称。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="技能名称", ToolTip="通过定义 ID 解析的本地化技能名称。"))
	FText AbilityName;

	/** 仅服务器已经进入引导阶段时为真；前摇期间为假。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="当前处于引导", ToolTip="仅服务器已经进入引导阶段时为真；前摇期间为假。"))
	bool bIsChanneling = false;

	/** 当前前摇或引导阶段的服务器绝对开始秒数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="技能阶段开始时间", ToolTip="当前前摇或引导阶段的服务器绝对开始秒数。"))
	double AbilityStartTime = 0.0;

	/** 当前前摇或引导阶段的服务器绝对结束秒数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="技能阶段结束时间", ToolTip="当前前摇或引导阶段的服务器绝对结束秒数。"))
	double AbilityEndTime = 0.0;
};

/** 根据服务器时间计算的本地进度；到期只影响显示，不结束技能或状态。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatOverheadProgressData
{
	GENERATED_BODY()

	/** 零到一；无期限状态保持一。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="控制剩余比例", ToolTip="零到一；无期限状态保持一。"))
	float StatusPercent = 1.0f;

	/** 负一表示无期限，有限状态到期后为零。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="控制剩余秒数", ToolTip="负一表示无期限，有限状态到期后为零。"))
	float StatusRemaining = -1.0f;

	/** 当前阶段剩余时长除以阶段总时长。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="技能剩余比例", ToolTip="当前阶段剩余时长除以阶段总时长。"))
	float AbilityPercent = 0.0f;

	/** 当前阶段剩余秒数，最小为零。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="技能剩余秒数", ToolTip="当前阶段剩余秒数，最小为零。"))
	float AbilityRemaining = 0.0f;

	/** 有效阶段仍有剩余显示时间时为真。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="显示技能进度", ToolTip="有效阶段仍有剩余显示时间时为真。"))
	bool bShowAbility = false;
};

/** 服务器结果对应的本地跳字载荷；生命代次来自结果发生端，序号仅用于本地错位。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatFloatingTextPayload
{
	GENERATED_BODY()

	/** 服务器实际扣除或恢复的生命值。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="实际数值", ToolTip="服务器实际扣除或恢复的生命值。"))
	float Amount = 0.0f;

	/** 决定蓝图使用的颜色与数字前缀。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="跳字类型", ToolTip="决定蓝图使用的颜色与数字前缀。"))
	ECombatFloatingTextType Type = ECombatFloatingTextType::PhysicalDamage;

	/** 结果发生时目标的生命代次；不同于当前 View 时丢弃。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="结果生命代次", ToolTip="结果发生时目标的生命代次；不同于当前 View 时丢弃。"))
	int64 LifeGeneration = 0;

	/** 从零递增的本地显示序号，用于让连续跳字错位。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="本地序号", ToolTip="从零递增的本地显示序号，用于让连续跳字错位。"))
	int32 Sequence = 0;
};
