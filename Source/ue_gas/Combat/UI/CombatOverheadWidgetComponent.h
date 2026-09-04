#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"

#include "Combat/Combat/CombatTransactionTypes.h"
#include "Combat/UI/CombatOverheadTypes.h"

#include "CombatOverheadWidgetComponent.generated.h"

/**
 * 每个 Combat Unit 自带的屏幕空间头顶 UI。
 * 资源和状态读取复制 View；短生命周期伤害/治疗跳字通过不可靠多播发送给相关客户端。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatOverheadWidgetComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UCombatOverheadWidgetComponent();

	/** 服务器将传入的实际扣血量以不可靠多播发送为跳字；调用方负责传真实结算结果，组件只检查数值有限且大于容差，不重新结算伤害。 */
	UFUNCTION(BlueprintCallable, Category="Combat|UI", meta=(DisplayName="显示伤害跳字", ToolTip="服务器将传入的实际扣血量以不可靠多播发送为跳字；调用方负责传真实结算结果，组件只检查数值有限且大于容差，不重新结算伤害。"))
	void ShowDamageNumber(
		UPARAM(DisplayName="实际伤害") float AppliedAmount,
		UPARAM(DisplayName="伤害类型") ECombatDamageType DamageType);

	/** 服务器将传入的实际恢复量以不可靠多播发送为跳字；调用方负责传真实结算结果，组件只检查数值有限且大于容差，不重新结算治疗。 */
	UFUNCTION(BlueprintCallable, Category="Combat|UI", meta=(DisplayName="显示治疗跳字", ToolTip="服务器将传入的实际恢复量以不可靠多播发送为跳字；调用方负责传真实结算结果，组件只检查数值有限且大于容差，不重新结算治疗。"))
	void ShowHealingNumber(UPARAM(DisplayName="实际治疗") float AppliedAmount);

	/** 在游戏 World 创建 Widget 后绑定所属 Unit 的复制 View；编辑器预览不建立运行时委托。 */
	virtual void InitWidget() override;

protected:
	/** 专用服务器开始运行时隐藏头顶组件；跳字多播实现也会在专用服务器端直接返回。 */
	virtual void BeginPlay() override;

private:
	/** 跳字是可丢弃的瞬时表现，不参与任何 gameplay 判定。 */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShowFloatingText(float Amount, ECombatFloatingTextType Type);
};
