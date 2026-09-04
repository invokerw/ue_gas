#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatThinker.generated.h"

/** 区域效果的位置与复制载体，自身不逐帧更新也不参与碰撞；作用时机和伤害由服务器区域子系统及调度器控制。 */
UCLASS(Blueprintable)
class UE_GAS_API ACombatThinker : public AActor
{
	GENERATED_BODY()

public:
	ACombatThinker();
	/** 创建后绑定区域记录的句柄，供 Actor 退出场景时通知子系统清理。 */
	void InitializeThinker(FCombatThinkerHandle InHandle) { ThinkerHandle = InHandle; }
	/** 子系统已完成区域结算时，销毁 Actor 前标记清理来源，避免 EndPlay 再次请求结束。 */
	void PrepareForSubsystemDestroy() { bSubsystemDestroying = true; }
	/** Actor 被外部销毁或世界关闭时通知区域子系统；子系统主动销毁产生的回调不重复处理。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 仅服务器本地调试使用的稳定句柄。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Thinker") FCombatThinkerHandle ThinkerHandle;

private:
	/** 表示本次 Actor 销毁由子系统的统一结束流程发起，无需再向子系统回报。 */
	bool bSubsystemDestroying = false;
};
