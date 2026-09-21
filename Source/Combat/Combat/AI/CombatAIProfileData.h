#pragma once

#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/AI/CombatAIRoleTypes.h"
#include "CombatAIProfileData.generated.h"

class UStateTree;

/** 自主行为定义；StateTree 负责角色编排，可选感知仅发布知识。旧资产默认不启用感知。 */
UCLASS(BlueprintType, meta=(DisplayName="战斗 AI 配置", ToolTip="配置 StateTree、动作协议和可选范围/LOS 感知；不生成或授予技能。"))
class COMBAT_API UCombatAIProfileData : public UCombatDefinitionData
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(DisplayName="根决策树", ToolTip="必须是已编译的 Combat AI Schema StateTree；为空时无法启动。")) TObjectPtr<UStateTree> RootTree;
	UPROPERTY(VisibleAnywhere, Category="AI", meta=(DisplayName="AI 配置版本", ToolTip="本地 AI 配置格式版本，当前只支持 1；不替代 Combat 内容版本。")) int32 AIProfileVersion = 1;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(DisplayName="准备结果有效期", ToolTip="从准备成功起到消费的最长游戏秒数，必须大于 0；过期后形成失败凭证。", Units="s", ClampMin="0.01")) float IntentLifetime = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(DisplayName="攻击边界等待上限", ToolTip="从前摇结束且边界 Ready 起最多保持的游戏秒数，范围 0.01 到 5；超时继续原攻击。", Units="s", ClampMin="0.01", ClampMax="5")) float BoundaryHoldSeconds = 0.25f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Perception", meta=(DisplayName="启用自主感知", ToolTip="默认关闭，保留阶段 A 显式命令行为；角色树需开启并提供合法职责。")) bool bEnablePerception = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Perception", meta=(DisplayName="感知与记忆", ToolTip="仅启用自主感知时使用；公共 Targeting 的范围/LOS 模型，不是完整迷雾。", EditCondition="bEnablePerception")) FCombatAIPerceptionConfig Perception;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Targets", meta=(DisplayName="定义优先级", ToolTip="按单位定义配置候选优先级；未列出的定义为 0。不得重复，最多 64 条。", TitleProperty="Definition")) TArray<FCombatAITargetPriority> TargetPriorities;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Targets", meta=(DisplayName="距离权重", ToolTip="同优先级按 Threat×威胁权重减去距离厘米×本权重排序，必须有限非负。", ClampMin="0", ClampMax="100")) float DistanceWeight = 0.01f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Targets", meta=(DisplayName="威胁权重", ToolTip="实际可观察受伤量的权重，必须有限非负；单目标威胁累计封顶 10000。", ClampMin="0", ClampMax="100")) float ThreatWeight = 1;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Targets", meta=(DisplayName="目标最短保持", ToolTip="合法选择点保留当前候选的最少游戏秒数，范围 0 到 30；无效或新生命立即淘汰。不会在持续攻击中自动换敌。", Units="s", ClampMin="0", ClampMax="30")) float MinTargetHold = 1;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Targets", meta=(DisplayName="换目标分差", ToolTip="最短保持结束后，同优先级新候选需超过当前分数的差值；范围 0 到 10000。", ClampMin="0", ClampMax="10000")) float TargetSwitchMargin = 5;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Duty", meta=(DisplayName="最大职责偏离", ToolTip="单位距职责锚点或最近已确认路线航点的最大 XY 距离，范围 80 到 10000 厘米；超限允许打断攻击前摇并归位。", Units="cm", ClampMin="80", ClampMax="10000")) float LeashDistance = 1200;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Duty", meta=(DisplayName="单次交战时限", ToolTip="一次连续攻击最多持续的游戏秒数，范围 0.1 到 120；到期请求归位，不重发攻击来刷新期限。", Units="s", ClampMin="0.1", ClampMax="120")) float MaxEngagementSeconds = 20;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Duty", meta=(DisplayName="到达复核容差", ToolTip="Order 成功后复核 XY 距离，范围 1 到 500 厘米；必须兼容 Order 的移动接受半径。", Units="cm", ClampMin="1", ClampMax="500")) float ArrivalTolerance = 80;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Duty", meta=(DisplayName="交战后归位", ToolTip="野怪开启：目标结束或丢失后先回职责锚点。小兵关闭：继续原航点；越界仍必须归位。")) bool bReturnAfterCombat = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Duty", meta=(DisplayName="归位清理记忆", ToolTip="成功到家确认后清空旧候选和威胁；下一次 Scheduler 采样重新观察，不恢复旧目标。")) bool bClearMemoryOnReturn = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Recovery", meta=(DisplayName="同操作尝试上限", ToolTip="同职责修订同操作最多尝试次数，包含首次，范围 1 到 10；超限只等待新合法职责或显式恢复。", ClampMin="1", ClampMax="10")) int32 MaxAttempts = 3;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Recovery", meta=(DisplayName="失败退避秒数", ToolTip="失败后等待的游戏秒数，范围 0.05 到 10；普通感知或受击不能跳过。", Units="s", ClampMin="0.05", ClampMax="10")) float RetryDelay = 0.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Duty", meta=(DisplayName="到点停留秒数", ToolTip="路线到点后的游戏秒数，范围 0.05 到 10，防止重合航点形成同步忙循环；由 Scheduler 驱动。", Units="s", ClampMin="0.05", ClampMax="10")) float RoutePause = 0.2f;
	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;
	/** Runtime 与编辑器共用预检，拒绝错误 Schema、未编译树和非有限参数。 */
	bool ValidateRuntime(FString& Diagnostic) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
