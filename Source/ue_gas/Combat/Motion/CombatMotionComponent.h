#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Combat/Motion/CombatMotionTypes.h"

#include "CombatMotionComponent.generated.h"

class ACombatUnitCharacter;

/**
 * 每个 Unit 唯一的服务器权威强制位移执行器。
 * 组件以稳定 Handle 管理水平、垂直独占通道和优先级抢占，只通过 CharacterMovement 推进连续位移；死亡、释放或 EndPlay 都会使旧请求失效并 exactly-once 广播结果。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatMotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatMotionComponent();

	/** 校验请求并获取通道；严格高优先级会中断旧 owner。 */
	FCombatMotionResult TryAcquireMotion(const FCombatMotionRequest& Request);
	/** 显式释放活动 Handle；旧/重复 Handle 安全返回 false。 */
	bool ReleaseMotion(FCombatMotionHandle Handle, ECombatMotionFinishReason Reason = ECombatMotionFinishReason::Cancelled);
	/** 返回完整 Handle 是否仍活动。 */
	bool IsMotionActive(FCombatMotionHandle Handle) const;
	/** 返回任意通道是否被占用。 */
	bool HasActiveMotion() const { return !ActiveMotions.IsEmpty(); }
	/** 返回活动请求数量，Both 只计一条。 */
	int32 GetActiveMotionCount() const { return ActiveMotions.Num(); }
	/** 返回最近 exactly-once 结束结果。 */
	const FCombatMotionResult& GetLastMotionResult() const { return LastMotionResult; }
	/** 返回完成观察委托。 */
	FOnCombatMotionFinished& OnMotionFinished() { return MotionFinishedDelegate; }

	/** Unit Dying 时释放全部通道并淘汰 generation。 */
	void HandleOwnerDeath();
	/** Respawn 后建立空的新 generation。 */
	void HandleOwnerRespawn();
	/** 每帧通过 CharacterMovement SafeMove 推进，不结算周期 gameplay。 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	/** EndPlay exactly-once 释放通道和 delegate。 */
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
	/** 完成/中断单条记录，释放通道后再广播和恢复 Order。 */
	bool FinishMotion(FCombatMotionHandle Handle, ECombatMotionFinishReason Reason, FGameplayTag FailureTag);
	/** 判断请求是否占用水平通道。 */
	static bool UsesHorizontal(ECombatMotionChannel Channel);
	/** 判断请求是否占用垂直通道。 */
	static bool UsesVertical(ECombatMotionChannel Channel);
	/** 全部 Motion 结束后投影 NavMesh 并只 Pump 当前 Order。 */
	void RestoreNavigationAndOrder(bool bProjectToNavigation);
	/** 输出 MotionStarted/MotionFinished 结构化记录。 */
	void EmitMotionLog(
		const FActiveMotion& Motion,
		FGameplayTag EventType,
		ECombatMotionFinishReason FinishReason,
		FGameplayTag FailureTag) const;

	/** 以 Handle Id 索引的活动请求。 */
	TMap<uint64, FActiveMotion> ActiveMotions;
	/** 两个通道当前 owner。 */
	FCombatMotionHandle HorizontalOwner;
	FCombatMotionHandle VerticalOwner;
	/** Handle 分配与生命周期 generation。 */
	uint64 NextMotionId = 1;
	uint32 MotionGeneration = 1;
	/** 高优先级请求正在替换旧请求时，抑制中途恢复导航与 Order。 */
	bool bAcquiringReplacement = false;
	/** EndPlay 期间不恢复导航或 Order。 */
	bool bEnding = false;
	/** 最近结果与观察者。 */
	FCombatMotionResult LastMotionResult;
	FOnCombatMotionFinished MotionFinishedDelegate;
};
