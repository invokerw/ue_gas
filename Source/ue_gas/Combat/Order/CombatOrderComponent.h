#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Navigation/PathFollowingComponent.h"

#include "Combat/Ability/CombatAbilityTypes.h"
#include "Combat/Order/CombatOrderTypes.h"

#include "CombatOrderComponent.generated.h"

class AAIController;
class ACombatUnitCharacter;
class UEnvQuery;
class UEnvQueryInstanceBlueprintWrapper;
class UPathFollowingComponent;

/** 统一执行 Move、Cast、Attack 与 Stop 的服务器权威 FIFO 状态机。 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatOrderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 默认关闭 Tick；异步推进只来自受句柄保护的回调与 Scheduler。 */
	UCombatOrderComponent();

	/** 接受一个权威 Order；bQueue=false 会提升 generation 并替换全部旧行为。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Order", meta=(DisplayName="下达战斗命令", ToolTip="向服务器权威 FIFO 状态机提交命令；不入队时会替换全部旧行为。"))
	FCombatOrderResult IssueOrder(
		UPARAM(DisplayName="命令请求") const FCombatOrderRequest& Request,
		UPARAM(DisplayName="加入队列") bool bQueue = false);
	/** 提升 generation，取消全部当前行为并清空 pending FIFO。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Order", meta=(DisplayName="停止全部战斗命令", ToolTip="取消当前行为、清空待处理队列，并使旧异步回调失效。"))
	void StopAllOrders(UPARAM(DisplayName="停止原因") FGameplayTag Reason);
	/** 只允许当前状态机入口推动验证、移动、Cast 或 Attack。 */
	void PumpCurrentOrder();
	/** Controller 变化后幂等解绑旧 PathFollowing 并绑定新实例。 */
	void RefreshControllerBinding();
	/** 状态 Tag count 变化后暂停、取消前摇或恢复当前有效队首。 */
	void HandleOwnerStatusChanged();
	/** 生命周期 Dying 时取消异步行为、清空队列并淘汰旧 generation。 */
	void HandleOwnerDeath();
	/** Respawn 后保持空队列，允许新生命接受 Order。 */
	void HandleOwnerRespawn();
	/** 强制位移开始时取消旧 AI Move 并暂停当前有效队首。 */
	void HandleOwnerMotionStarted();
	/** 全部强制位移结束后只重新 Pump 当前 generation 的队首。 */
	void HandleOwnerMotionFinished();
	/** 临时 gameplay blocker 创建/销毁时，对相交移动路径主动取消旧请求并重寻路。 */
	void HandleGameplayBlockerChanged(const FBox& BlockerBounds);

	/** 返回当前状态。 */
	UFUNCTION(BlueprintPure, Category="Combat|Order", meta=(DisplayName="获取当前命令状态", ToolTip="返回战斗命令状态机的当前状态。"))
	ECombatOrderState GetCurrentState() const { return CurrentState; }
	/** 返回当前 OrderHandle；空闲时无效。 */
	UFUNCTION(BlueprintPure, Category="Combat|Order", meta=(DisplayName="获取当前命令句柄", ToolTip="返回当前命令的稳定句柄；空闲时句柄无效。"))
	FCombatOrderHandle GetCurrentOrderHandle() const;
	/** 返回 pending FIFO 数量，不包含当前项。 */
	UFUNCTION(BlueprintPure, Category="Combat|Order", meta=(DisplayName="获取待处理命令数量", ToolTip="返回待处理 FIFO 数量，不包含当前命令。"))
	int32 GetPendingOrderCount() const { return PendingOrders.Num(); }
	/** 返回可选 EQS Context 使用的当前权威移动目的点。 */
	FVector GetCurrentMoveGoal() const { return CurrentMoveGoal; }
	/** 返回当前 Order 完成委托。 */
	FOnCombatOrderFinished& OnOrderFinished() { return OrderFinishedDelegate; }

	/** 自动化可令导航保持 deferred，以稳定注入成功/失败和旧 Handle。 */
	void SetNavigationDeferredForTesting(bool bDeferred) { bNavigationDeferredForTesting = bDeferred; }
	/** 自动化模拟当前 Move 回调；Handle 不匹配时必须无副作用返回 false。 */
	bool CompleteMovementForTesting(FCombatOrderHandle Handle, bool bSuccess, bool bPartial = false);
	/** 自动化连同导航 attempt generation 注入回调，用于证明 repath 后旧尝试失效。 */
	bool CompleteMovementAttemptForTesting(
		FCombatOrderHandle Handle,
		uint32 AttemptGeneration,
		bool bSuccess,
		bool bPartial = false);
	/** 返回当前导航尝试代次，供 blocker/repath 自动化记录旧值。 */
	uint32 GetNavigationAttemptGenerationForTesting() const { return NavigationAttemptGeneration; }

	/** 可选的目的点 EQS；为空时直接使用 AI MoveTo。 */
	UPROPERTY(EditAnywhere, Category="Combat|Order|Movement") TObjectPtr<UEnvQuery> MoveDestinationQuery;
	/** 普通 MoveToPoint/MoveToUnit 的完成容差。 */
	UPROPERTY(EditAnywhere, Category="Combat|Order|Movement", meta=(ClampMin="0", Units="cm")) float MovementAcceptanceRadius = 25.0f;
	/** 动态目标位移超过该值时主动重发 Move。 */
	UPROPERTY(EditAnywhere, Category="Combat|Order|Chase", meta=(ClampMin="1", Units="cm")) float ChaseWakeDistance = 50.0f;
	/** 动态追击的 Coalesce 服务器复核间隔。 */
	UPROPERTY(EditAnywhere, Category="Combat|Order|Chase", meta=(ClampMin="0.02", Units="s")) float ChaseCheckInterval = 0.10f;
	/** 单个持续 Cast/Attack Order 的最长追击时间。 */
	UPROPERTY(EditAnywhere, Category="Combat|Order|Chase", meta=(ClampMin="0.1", Units="s")) float MaxChaseDuration = 10.0f;
	/** Blocked/Invalid/PartialPath 的有界重试次数。 */
	UPROPERTY(EditAnywhere, Category="Combat|Order|Movement", meta=(ClampMin="0")) int32 MaxMoveRetries = 3;
	/** pending FIFO 上限，不包含当前项。 */
	UPROPERTY(EditAnywhere, Category="Combat|Order", meta=(ClampMin="1")) int32 MaxQueuedOrders = 16;

	/** 结束 Actor 前解绑所有 delegate 并使旧回调无效。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 绑定 ASC/Attack/Controller，并开始处理预置队列。 */
	virtual void BeginPlay() override;

private:
	/** 返回组件所属 Combat Unit。 */
	ACombatUnitCharacter* GetOwnerUnit() const;
	/** 幂等绑定 ASC/Attack 委托，避免动态 Spawn 时受组件 BeginPlay 顺序影响。 */
	void EnsureRuntimeBindings();
	/** 校验 payload 字段组合、有限位置、AbilitySpec 与目标 Actor。 */
	FCombatOperationResult ValidateOrderRequest(const FCombatOrderRequest& Request) const;
	/** 分配当前 generation/life generation 下的新句柄。 */
	FCombatOrderHandle AllocateOrderHandle() const;
	/** 从 FIFO 取出下一项并进入 Validating。 */
	bool BeginNextOrder();
	/** 原子完成当前项，广播结果后再处理下一个 FIFO 项。 */
	void CompleteCurrentOrder(bool bSuccess, FGameplayTag FailureTag, const FString& Diagnostic);
	/** 提升 generation，先使回调失效，再取消当前异步行为。 */
	void AdvanceGenerationAndCancel(FGameplayTag Reason, bool bBroadcastCurrent);
	/** 取消 EQS、Move、追击、Ability 和 attack windup。 */
	void CancelCurrentAsync(FGameplayTag Reason);
	/** 仅取消 EQS/Move/追击，不改变当前队列项。 */
	void CancelMovementAsync();
	/** 检查当前位置是否已满足 Move/Cast/Attack 的边缘距离。 */
	bool IsCurrentDestinationReached() const;
	/** 返回当前行为需要的边缘范围。 */
	float GetCurrentDesiredRange() const;
	/** 朝当前 Actor/Point 目标设置服务器 XY 朝向。 */
	bool FaceCurrentTarget();
	/** 开始直接或 EQS 解析后的 AI Move。 */
	bool BeginMovement(bool bChasing);
	/** 使用给定位置或动态 Actor 创建受 RequestId 保护的 AI Move。 */
	bool StartAiMove(bool bChasing);
	/** 为动态目标建立唯一 Coalesce 追击检查。 */
	void EnsureChaseSchedule();
	/** 追击 Schedule 回调：范围复核、位移唤醒和 deadline。 */
	void HandleChaseCheck(FCombatOrderHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** 对 Blocked/Invalid/PartialPath 执行有界延迟重试。 */
	void ScheduleMoveRetry(FGameplayTag FailureTag, const FString& Diagnostic);
	/** 重试 Schedule 到期后重新验证原 OrderHandle。 */
	void HandleMoveRetry(FCombatOrderHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** 执行 CastNoTarget/CastPoint/CastTarget 并等待 ASC OrderReleased。 */
	void DispatchCurrentAbility();
	/** 执行或继续持续 AttackTarget 单轮。 */
	void StartCurrentAttack();
	/** ASC 的 OrderReleased 回调，必须匹配当前 Spec 与 Order。 */
	void HandleAbilityOrderReleased(
		FGameplayAbilitySpecHandle Handle,
		bool bSuccess,
		FGameplayTag FailureTag,
		ECombatChannelInterruptOrderPolicy InterruptPolicy);
	/** Attack point 到达后将持续 Order 切换到 WaitingAttackReady。 */
	void HandleAttackLaunched(FCombatAttackHandle AttackHandle, FCombatOrderHandle OrderHandle);
	/** BAT ready 后复核同一 OrderHandle 并开始下一轮。 */
	void HandleAttackReady(FCombatOrderHandle OrderHandle);
	/** EQS 只接受当前 wrapper、OrderHandle 和成功状态。 */
	UFUNCTION() void HandleEqsFinished(UEnvQueryInstanceBlueprintWrapper* QueryInstance, EEnvQueryStatus::Type QueryStatus);
	/** PathFollowing 只接受当前 RequestId、OrderHandle 和生命代次。 */
	void HandleMoveFinished(FAIRequestID RequestId, const FPathFollowingResult& Result);
	/** 统一处理真实或注入的移动结果；成功 partial path 只有实际达到行为范围时才可继续。 */
	void ResolveMoveCompletion(bool bSuccess, bool bPartial, FGameplayTag FailureTag, const FString& Diagnostic);
	/** 状态转换与结束输出统一结构化日志。 */
	void TransitionTo(ECombatOrderState NewState, FGameplayTag FailureTag = FGameplayTag(), const FString& Diagnostic = FString());

	/** pending FIFO；当前项单独保存，禁止 delegate 直接 Pop。 */
	TArray<FCombatQueuedOrder> PendingOrders;
	/** 唯一当前项。 */
	TOptional<FCombatQueuedOrder> CurrentOrder;
	/** 当前权威状态。 */
	ECombatOrderState CurrentState = ECombatOrderState::Idle;
	/** replace/Stop/Death 时递增，所有已分配 Handle 立即过期。 */
	uint32 OrderGeneration = 1;
	/** 下一个 OrderHandle 槽位 ID。 */
	mutable uint64 NextOrderId = 1;
	/** 防止同步 Complete/Callback 重入 Pump。 */
	bool bPumping = false;
	/** 自动化导航注入开关。 */
	bool bNavigationDeferredForTesting = false;
	/** 当前 EQS wrapper 与其绑定的 OrderHandle。 */
	UPROPERTY(Transient) TObjectPtr<UEnvQueryInstanceBlueprintWrapper> ActiveQuery;
	FCombatOrderHandle ActiveQueryOrderHandle;
	/** 当前 PathFollowing 请求及其 OrderHandle。 */
	FAIRequestID ActiveMoveRequestId = FAIRequestID::InvalidRequest;
	FCombatOrderHandle ActiveMoveOrderHandle;
	FNavPathSharedPtr ActiveMovePath;
	/** 每次 BeginMovement 递增，使同 OrderHandle 的旧 blocker 前回调也会失效。 */
	uint32 NavigationAttemptGeneration = 1;
	/** 已绑定 OnRequestFinished 的组件；Controller 变化时先解绑。 */
	TWeakObjectPtr<UPathFollowingComponent> BoundPathFollowing;
	/** 当前 EQS/Move 使用的目标位置和最近一次追击目标位置。 */
	FVector CurrentMoveGoal = FVector::ZeroVector;
	FVector LastChaseTargetLocation = FVector::ZeroVector;
	/** 当前追击开始的绝对 World Game Time。 */
	double ChaseStartedAt = 0.0;
	/** 当前移动失败重试次数。 */
	int32 MoveRetryCount = 0;
	/** 追击与重试 Scheduler 句柄。 */
	FCombatScheduleHandle ChaseSchedule;
	FCombatScheduleHandle RetrySchedule;
	/** 当前等待释放的 AbilitySpec。 */
	FGameplayAbilitySpecHandle ActiveAbilitySpecHandle;
	/** 每个结束结果的观察者。 */
	FOnCombatOrderFinished OrderFinishedDelegate;
};
