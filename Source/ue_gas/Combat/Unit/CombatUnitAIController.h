#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Navigation/CrowdFollowingComponent.h"

#include "CombatUnitAIController.generated.h"

class ACombatUnitCharacter;

/**
 * 战斗单位的专用服务器导航控制器。
 * 它只持有 PathFollowing/Crowd 执行职责，不表示玩家所有权，也不在客户端生成。
 */
UCLASS(Blueprintable, NotPlaceable)
class UE_GAS_API ACombatUnitAIController : public AAIController
{
	GENERATED_BODY()

public:
	/** 用 UCrowdFollowingComponent 替换默认 PathFollowing，并集中设置唯一一套避让参数。 */
	explicit ACombatUnitAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * 根据 Unit 的生命、控制状态、碰撞标签与 Motion 状态切换 Crowd participation。
	 * 仅 Authority 调用；Disabled agent 会被其他 Crowd agent 忽略。
	 */
	void RefreshCrowdParticipation();

	/** 返回本控制器持有的 CrowdFollowing；配置或类型异常时为空。 */
	UFUNCTION(BlueprintPure, Category="Combat|Movement", meta=(DisplayName="获取战斗 Crowd 跟随组件", ToolTip="返回服务器战斗 AIController 使用的 Detour Crowd 跟随组件。"))
	UCrowdFollowingComponent* GetCombatCrowdFollowing() const;

	/** 返回 Crowd 当前是否实际参与 steering，供诊断与自动化验证。 */
	UFUNCTION(BlueprintPure, Category="Combat|Movement", meta=(DisplayName="Crowd 是否活动", ToolTip="仅当单位状态允许且 Crowd simulation 未暂停时返回真。"))
	bool IsCrowdSteeringActive() const;
	/** 返回状态规则最近计算出的目标 Crowd 状态；无 NavData 的单元测试也可验证策略。 */
	ECrowdSimulationState GetDesiredCrowdSimulationState() const { return DesiredCrowdSimulationState; }

protected:
	/** Possess Unit 后注册 Crowd 状态并让 Order 重新绑定唯一 PathFollowing delegate。 */
	virtual void OnPossess(APawn* InPawn) override;
	/** UnPossess 前把 agent 移出 Crowd，避免 Controller teardown 留下活动 agent。 */
	virtual void OnUnPossess() override;

private:
	/** 生命周期规则计算出的目标状态；引擎 CrowdManager 不可用时仍保留诊断事实。 */
	ECrowdSimulationState DesiredCrowdSimulationState = ECrowdSimulationState::Disabled;
};
