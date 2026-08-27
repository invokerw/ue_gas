#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatProjectilePresentationSubsystem.generated.h"

class ACombatProjectileActor;

/** 客户端弹体视觉协调统计。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatProjectilePresentationStats
{
	GENERATED_BODY()

	/** 尚未等到服务器身份的预测视觉数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile|Presentation", meta=(DisplayName="活动预测视觉数", ToolTip="尚未等到服务器弹体身份的客户端本地预测视觉数量。")) int32 ActivePredictedVisuals = 0;
	/** 当前已注册的服务器弹体视觉数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile|Presentation", meta=(DisplayName="活动服务器视觉数", ToolTip="当前已注册的唯一服务器弹体视觉数量。")) int32 ActiveServerVisuals = 0;
	/** 成功用服务器 Actor 替换预测视觉的累计次数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile|Presentation", meta=(DisplayName="视觉协调次数", ToolTip="成功用服务器 Actor 替换预测视觉的累计次数。")) int64 ReconcileCount = 0;
	/** 同一服务器 Handle 重复注册但未产生第二视觉的累计次数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile|Presentation", meta=(DisplayName="服务器身份去重次数", ToolTip="同一服务器 Handle 重复通知且未创建第二视觉的累计次数。")) int64 DuplicateServerIdentityCount = 0;
};

/** 只协调客户端预测视觉与服务器 Projectile Actor，不参与任何 gameplay 结算。 */
UCLASS()
class UE_GAS_API UCombatProjectilePresentationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 注册一个可随时丢弃的客户端本地预测视觉。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Projectile|Presentation", meta=(DisplayName="注册预测弹体视觉", ToolTip="按正数预测键注册本地视觉；服务器弹体到达后自动销毁该视觉。"))
	bool RegisterPredictedVisual(
		UPARAM(DisplayName="预测视觉键") int32 PredictionKey,
		UPARAM(DisplayName="视觉 Actor") AActor* VisualActor);
	/** 服务器 Actor 身份复制后销毁同键预测视觉并保持唯一服务器视觉。 */
	void ReconcileServerProjectile(ACombatProjectileActor* ServerActor);
	/** 服务器 Actor 在客户端结束时移除协调记录。 */
	void NotifyServerProjectileEnded(ACombatProjectileActor* ServerActor);
	/** 返回当前与累计协调统计。 */
	UFUNCTION(BlueprintPure, Category="Combat|Projectile|Presentation", meta=(DisplayName="获取弹体视觉协调统计", ToolTip="返回预测视觉、服务器视觉与去重计数。"))
	FCombatProjectilePresentationStats GetPresentationStats() const;
	/** World teardown 时销毁仍未协调的本地预测视觉并清空弱引用。 */
	virtual void Deinitialize() override;

private:
	/** 正数预测键到客户端本地视觉。 */
	TMap<int32, TWeakObjectPtr<AActor>> PredictedVisuals;
	/** 服务器稳定句柄到唯一复制 Actor。 */
	TMap<FCombatProjectileHandle, TWeakObjectPtr<ACombatProjectileActor>> ServerVisuals;
	/** 累计协调次数。 */
	uint64 ReconcileCount = 0;
	/** 累计重复服务器身份次数。 */
	uint64 DuplicateServerIdentityCount = 0;
};
