#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "CombatCharacterMovementComponent.generated.h"

/**
 * 复用 CharacterMovement 的移动、复制和转速配置，补充服务器 Order 的原地朝向准备。
 * Order 持有目标与生命周期；本组件只按 RotationRate.Yaw 推进连续旋转，不激活技能或维护第二套计时。
 * 定身使移动模式变为 None 时仍可为合法施法转身；客户端只消费服务器移动复制。
 */
UCLASS(meta=(DisplayName="战斗角色移动组件", ToolTip="普通移动和技能、普攻准备共用水平转身速率；朝向只在服务器推进。"))
class COMBAT_API UCombatCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UCombatCharacterMovementComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysicsRotation(float DeltaTime) override;
	/** 单位是阻挡体而非可站立地面，避免接触胶囊顶部后触发自动跳离；地形和平台沿用 UE 判定。 */
	virtual bool IsWalkable(const FHitResult& Hit) const override;

private:
	/** 只在当前移动 Tick 内抑制普通移动朝向，避免同一帧对 Order 转身重复叠加旋转。 */
	bool bUpdatingOrderFacing = false;
};
