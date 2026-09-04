#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Combat/Motion/CombatMotionTypes.h"

#include "CombatMotionComponent.generated.h"

class ACombatUnitCharacter;

/**
 * 单位上的服务器强制位移组件，用于击退、拉拽等不由普通寻路产生的移动。
 * 水平和垂直通道分别独占，新请求只有优先级严格更高才能中断冲突请求；同一请求可同时占用两个通道。
 * 接受请求后暂停普通寻路，每帧通过 CharacterMovement 推进；最后一条请求结束后可投影到导航网格并重新评估原命令。
 * 到达、受阻、抢占、取消、死亡及退出场景都汇入统一结束流程，旧生命或旧代次句柄不能控制当前单位。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatMotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatMotionComponent();

	/**
	 * 在服务器校验存活状态、目标和速度；所需通道空闲时接受，或在优先级严格高于全部冲突请求时先中断旧请求再接受。
	 * 返回成功表示已经登记并暂停普通移动，尚未发生位移；失败不会改变现有通道。
	 */
	FCombatMotionResult TryAcquireMotion(const FCombatMotionRequest& Request);
	/** 按指定原因结束活动位移并释放通道；最后一条结束时恢复普通移动和命令。旧、重复或身份不匹配的句柄返回 false。 */
	bool ReleaseMotion(FCombatMotionHandle Handle, ECombatMotionFinishReason Reason = ECombatMotionFinishReason::Cancelled);
	/** 返回完整 Handle 是否仍活动。 */
	bool IsMotionActive(FCombatMotionHandle Handle) const;
	/** 检查是否至少有一条活动强制位移；用于暂停普通指令，一个同时占两通道的请求仍只算一条。 */
	bool HasActiveMotion() const { return !ActiveMotions.IsEmpty(); }
	/** 返回活动请求数量，Both 只计一条。 */
	int32 GetActiveMotionCount() const { return ActiveMotions.Num(); }
	/** 取得最近一次强制位移的最终结果；首次结束前为默认值，后续结束会覆盖。 */
	const FCombatMotionResult& GetLastMotionResult() const { return LastMotionResult; }
	/** 订阅强制位移最终结束的服务器本地通知；订阅者退出时应解绑。 */
	FOnCombatMotionFinished& OnMotionFinished() { return MotionFinishedDelegate; }

	/** 单位开始死亡时以 Death 原因结束全部请求并提升组件代次，防止旧回调作用于下一次生命。 */
	void HandleOwnerDeath();
	/** 复活时清空通道、提升组件代次并停止 Tick；不会恢复上一条生命的强制位移。 */
	void HandleOwnerRespawn();
	/** 仅在服务器有活动请求时逐帧推进各请求；按通道计算位移，达到终点或扫掠受阻时结束。此 Tick 不造成周期伤害。 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	/** 单位退出场景时结束全部强制位移并提升代次；仍广播各记录的最终结束通知，但不再恢复寻路和命令。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 单条活动请求。 */
	struct FActiveMotion
	{
		FCombatMotionHandle Handle;
		FCombatMotionRequest Request;
		FVector StartLocation = FVector::ZeroVector;
	};

	/** 返回所属 Combat Unit。 */
	ACombatUnitCharacter* GetOwnerUnit() const;
	/** 统一结束入口：先移除记录和通道占用，刷新移动状态并广播结果；只在全部请求结束且当前没有抢占替换时恢复命令。 */
	bool FinishMotion(FCombatMotionHandle Handle, ECombatMotionFinishReason Reason, FGameplayTag FailureTag);
	/** 判断请求是否占用水平通道。 */
	static bool UsesHorizontal(ECombatMotionChannel Channel);
	/** 判断请求是否占用垂直通道。 */
	static bool UsesVertical(ECombatMotionChannel Channel);
	/** 停止残余速度；按请求选项尝试把单位校正到附近导航点，然后通知指令组件重新处理当前有效命令，不恢复旧路径请求。 */
	void RestoreNavigationAndOrder(bool bProjectToNavigation);
	/** 输出 MotionStarted/MotionFinished 结构化记录。 */
	void EmitMotionLog(
		const FActiveMotion& Motion,
		FGameplayTag EventType,
		ECombatMotionFinishReason FinishReason,
		FGameplayTag FailureTag) const;

	/** 以 Handle Id 索引的活动请求。 */
	TMap<uint64, FActiveMotion> ActiveMotions;
	/** 水平与垂直通道当前占用者的句柄；同时占用的请求会出现在两个字段中，但活动表只有一条记录。 */
	FCombatMotionHandle HorizontalOwner;
	FCombatMotionHandle VerticalOwner;
	/** Handle 分配与生命周期 generation。 */
	uint64 NextMotionId = 1;
	uint32 MotionGeneration = 1;
	/** 高优先级请求正在替换旧请求时，抑制中途恢复导航与 Order。 */
	bool bAcquiringReplacement = false;
	/** EndPlay 期间不恢复导航或 Order。 */
	bool bEnding = false;
	/** 最近最终结果和结束通知；结果只保留一份，不形成历史记录。 */
	FCombatMotionResult LastMotionResult;
	FOnCombatMotionFinished MotionFinishedDelegate;
};
