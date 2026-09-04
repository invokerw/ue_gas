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
	/** 服务器弹体找到并消费同键预测记录的累计次数；即使记录中的临时 Actor 已失效也计数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile|Presentation", meta=(DisplayName="视觉协调次数", ToolTip="服务器弹体找到并消费同键预测记录的累计次数；即使记录中的临时 Actor 已失效也计数。")) int64 ReconcileCount = 0;
	/** 同一服务器句柄已有有效登记时，再次登记被忽略的累计次数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile|Presentation", meta=(DisplayName="服务器身份去重次数", ToolTip="同一服务器句柄已有有效登记时，再次登记被忽略的累计次数。")) int64 DuplicateServerIdentityCount = 0;
};

/**
 * 管理客户端提前播放的弹体视觉与之后复制到达的服务器弹体。用预测键找到同一次发射的临时视觉，将其销毁并登记服务器 Actor。
 * 这里只管理视觉对象身份，不执行路径检测、命中、伤害或攻击结算；调用者应只注册可丢弃的纯视觉 Actor。
 */
UCLASS()
class UE_GAS_API UCombatProjectilePresentationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 登记已创建的本地纯视觉 Actor；预测键必须为正且 Actor 有效、同世界。相同键已有另一视觉时会销毁旧视觉，再记录新对象。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Projectile|Presentation", meta=(DisplayName="注册预测弹体视觉", ToolTip="登记已创建的本地纯视觉 Actor；预测键必须为正且 Actor 有效、同世界。相同键已有另一视觉时会销毁旧视觉，再记录新对象。"))
	bool RegisterPredictedVisual(
		UPARAM(DisplayName="预测视觉键") int32 PredictionKey,
		UPARAM(DisplayName="视觉 Actor") AActor* VisualActor);
	/** 登记复制到达的服务器弹体，并移除同预测键的本地视觉；同一服务器句柄已有有效记录时忽略重复注册，不销毁重复传入的服务器 Actor。 */
	void ReconcileServerProjectile(ACombatProjectileActor* ServerActor);
	/** 服务器 Actor 在客户端结束时移除协调记录。 */
	void NotifyServerProjectileEnded(ACombatProjectileActor* ServerActor);
	/** 返回当前与累计协调统计。 */
	UFUNCTION(BlueprintPure, Category="Combat|Projectile|Presentation", meta=(DisplayName="获取弹体视觉协调统计", ToolTip="返回预测视觉、服务器视觉与去重计数。"))
	FCombatProjectilePresentationStats GetPresentationStats() const;
	/** 世界关闭时销毁尚未被服务器弹体替换的本地临时视觉，并清空本地与服务器弹体的弱引用记录。 */
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
