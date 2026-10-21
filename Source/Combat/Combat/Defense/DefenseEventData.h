#pragma once
#include "CoreMinimal.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "DefenseEventData.generated.h"

/** 防守事件类型；安全选项立即结算，风险选项先进入可取消确认窗口。 */
UENUM(BlueprintType)
enum class EDefenseEventKind : uint8
{
	Vault UMETA(DisplayName="宝库探索"),
	Oath UMETA(DisplayName="神殿誓约")
};

/** 一个事件节点的稳定配置；运行时效果仍由服务器 Director 通过公共经济/治疗入口执行。 */
UCLASS(BlueprintType, meta=(DisplayName="防守事件定义", ToolTip="配置事件触发波次、安全收益和风险收益；客户端只读取事件说明，不能提交数值。"))
class COMBAT_API UDefenseEventData : public UCombatDefinitionData
{
	GENERATED_BODY()
public:
	/** 防守事件数据版本。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="事件数据版本", ToolTip="当前事件数据版本，必须为1。"))
	int32 DefenseSchemaVersion = 1;
	/** 事件类型，决定安全选项的公共结算方式。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="事件类型", ToolTip="宝库探索提供金币和经验；神殿誓约修复核心。"))
	EDefenseEventKind Kind = EDefenseEventKind::Vault;
	/** 事件在第几个普通波完成后出现，使用1-based波次编号。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="触发波次", ToolTip="事件在该普通波完成后出现；当前原型只允许第2或第4波。", ClampMin="1", ClampMax="5"))
	int32 TriggerAfterWave = 2;
	/** 事件标题。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="事件标题", ToolTip="显示在防守事件面板中的标题。"))
	FText Title;
	/** 安全选项说明。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="安全选项说明", ToolTip="不提高下一波压力，选择后立即结算。"))
	FText SafeDescription;
	/** 风险选项说明。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="风险选项说明", ToolTip="先进入可取消确认窗口；确认后提高下一波压力并增加Boss奖励。"))
	FText RiskDescription;
	/** 安全选项金币奖励。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="安全金币", ToolTip="安全选项立即给每名仍在本局玩家的金币。", ClampMin="0", ClampMax="100000"))
	int32 SafeGold = 0;
	/** 安全选项经验奖励。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="安全经验", ToolTip="安全选项立即给每名仍在本局英雄的经验；死亡英雄按现有待领取语义处理。", ClampMin="0", ClampMax="100000"))
	int32 SafeExperience = 0;
	/** 安全选项核心修复比例。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="安全核心修复比例", ToolTip="按核心当前最大生命计算，0表示不修复，范围0到1。", ClampMin="0", ClampMax="1"))
	float SafeCoreHealFraction = 0.0f;
	/** 风险选项下一波预算增量；原型按每12点增加一名基础敌人。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="风险预算增量", ToolTip="风险确认后仅作用于下一波；当前原型每12点增加一名基础敌人。", ClampMin="0", ClampMax="1000"))
	int32 RiskBudgetBonus = 0;
	/** 风险成功后每名玩家的延迟金币奖励。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="风险金币奖励", ToolTip="风险波次成功清场后一次性发放给每名仍在本局玩家。", ClampMin="0", ClampMax="100000"))
	int32 RiskGold = 0;
	/** 风险成功后每名英雄的延迟经验奖励。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="风险经验", ToolTip="风险波次成功清场后一次性发放给每名仍在本局英雄。", ClampMin="0", ClampMax="100000"))
	int32 RiskExperience = 0;
	/** 风险成功后Boss奖励倍率增量，例如0.1表示增加10%。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defense|Event", meta=(DisplayName="Boss奖励倍率增量", ToolTip="风险成功后只作用于后续Boss奖励，0.1表示增加10%。", ClampMin="0", ClampMax="1"))
	float BossRewardBonusFraction = 0.0f;

	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;
	/** 校验稳定身份、触发波次、文本和收益范围。 */
	bool ValidateRuntime(FString& OutDiagnostic) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
