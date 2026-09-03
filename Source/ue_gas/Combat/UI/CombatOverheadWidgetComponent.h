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

	/** 由服务器把目标实际承受的伤害显示给当前相关客户端。 */
	UFUNCTION(BlueprintCallable, Category="Combat|UI", meta=(DisplayName="显示伤害跳字", ToolTip="仅服务器有效；按实际伤害值向相关客户端发送头顶跳字。"))
	void ShowDamageNumber(
		UPARAM(DisplayName="实际伤害") float AppliedAmount,
		UPARAM(DisplayName="伤害类型") ECombatDamageType DamageType);

	/** 由服务器把目标实际获得的治疗显示给当前相关客户端。 */
	UFUNCTION(BlueprintCallable, Category="Combat|UI", meta=(DisplayName="显示治疗跳字", ToolTip="仅服务器有效；按实际治疗值向相关客户端发送头顶跳字。"))
	void ShowHealingNumber(UPARAM(DisplayName="实际治疗") float AppliedAmount);

	/** 创建 Widget 后绑定所属 Unit 的复制 View。 */
	virtual void InitWidget() override;

protected:
	/** Dedicated Server 不创建或显示 Slate 表现。 */
	virtual void BeginPlay() override;

private:
	/** 跳字是可丢弃的瞬时表现，不参与任何 gameplay 判定。 */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShowFloatingText(float Amount, ECombatFloatingTextType Type);
};
