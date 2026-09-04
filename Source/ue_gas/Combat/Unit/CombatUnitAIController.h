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

	/** 仅服务器更新单位是否参与人群避让：正常存活单位主动绕行；定身、眩晕或强制位移期间只作为障碍供其他单位避让；死亡或无单位碰撞时完全退出，其他单位也不再避让它。 */
	void RefreshCrowdParticipation();

	/** 返回本控制器持有的 CrowdFollowing；配置或类型异常时为空。 */
	UFUNCTION(BlueprintPure, Category="Combat|Movement", meta=(DisplayName="获取战斗 Crowd 跟随组件", ToolTip="返回服务器战斗 AIController 使用的 Detour Crowd 跟随组件。"))
	UCrowdFollowingComponent* GetCombatCrowdFollowing() const;

	/** 返回人群系统是否正在为本单位计算导航移动方向，供诊断与自动化验证。 */
	UFUNCTION(BlueprintPure, Category="Combat|Movement", meta=(DisplayName="Crowd 是否活动", ToolTip="返回人群系统是否正在为本单位计算导航移动方向，供诊断与自动化验证。"))
	bool IsCrowdSteeringActive() const;
	/** 返回状态规则最近计算出的目标 Crowd 状态；无 NavData 的单元测试也可验证策略。 */
	ECrowdSimulationState GetDesiredCrowdSimulationState() const { return DesiredCrowdSimulationState; }

protected:
	/** 控制单位后更新人群避让状态，并让指令组件重新绑定本控制器的路径完成通知。 */
	virtual void OnPossess(APawn* InPawn) override;
	/** 解除控制前停止人群导航并移除避让参与状态，避免控制器销毁后仍保留活动单位记录。 */
	virtual void OnUnPossess() override;

private:
	/** 生命周期规则计算出的目标状态；引擎 CrowdManager 不可用时仍保留诊断事实。 */
	ECrowdSimulationState DesiredCrowdSimulationState = ECrowdSimulationState::Disabled;
};
