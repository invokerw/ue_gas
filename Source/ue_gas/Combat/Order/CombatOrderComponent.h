#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Navigation/PathFollowingComponent.h"

#include "Combat/Ability/CombatAbilityTypes.h"
#include "Combat/Order/CombatOrderTypes.h"

#include "CombatOrderComponent.generated.h"

class AAIController;
class AController;
class ACombatUnitCharacter;
class UEnvQuery;
class UEnvQueryInstanceBlueprintWrapper;
class UPathFollowingComponent;

/**
 * 单位上的服务器指令状态机，依次执行移动、施法和持续攻击；停止命令负责取消当前行为和清空队列。
 * IssueOrder 先验证载荷。替换模式取消当前命令和排队项，追加模式把新命令放到队尾；只有当前命令产生最终通知，未开始就被清空的排队项不逐条通知。
 * 移动由服务器 AIController 和 PathFollowing 执行，施法等待技能释放命令，攻击在每轮间隔后继续；强制位移和部分控制状态会暂停并保留当前项。
 * 每次替换、停止或死亡提升命令代次，并结合导航请求、技能、攻击及生命编号拒绝旧异步回调。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatOrderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatOrderComponent();

	/**
	 * 在服务器验证并接收一条命令。bQueue=false 先取消当前项和全部排队项，再执行新命令；true 追加到队尾且受容量限制。
	 * 返回成功只表示请求被接受，执行可能同步完成，也可能随后通过完成委托报告成功、失败或取消；Stop 总是走清空流程。
	 */
	UFUNCTION(BlueprintCallable, Category="Combat|Order", meta=(DisplayName="下达战斗命令", ToolTip="在服务器验证并接收一条命令。bQueue=false 先取消当前项和全部排队项，再执行新命令；true 追加到队尾且受容量限制。 返回成功只表示请求被接受，执行可能同步完成，也可能随后通过完成委托报告成功、失败或取消；Stop 总是走清空流程。"))
	FCombatOrderResult IssueOrder(
		UPARAM(DisplayName="命令请求") const FCombatOrderRequest& Request,
		UPARAM(DisplayName="加入队列") bool bQueue = false);
	/** 停止当前异步移动、技能或攻击前摇，提升命令代次并清空队列；仅当前正在执行的命令收到取消通知，空 Reason 使用默认取消标签。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Order", meta=(DisplayName="停止全部战斗命令", ToolTip="停止当前异步移动、技能或攻击前摇，提升命令代次并清空队列；仅当前正在执行的命令收到取消通知，空 Reason 使用默认取消标签。"))
	void StopAllOrders(UPARAM(DisplayName="停止原因") FGameplayTag Reason);
	/** 处理当前队首直到需要等待异步结果或队列为空；重入调用直接返回，后续由外层循环或回调继续。 */
	void PumpCurrentOrder();
	/** Controller 变化后幂等解绑旧 PathFollowing 并绑定新实例。 */
	void RefreshControllerBinding();
	/** 状态 Tag count 变化后暂停、取消前摇或恢复当前有效队首。 */
	void HandleOwnerStatusChanged();
	/** 生命周期 Dying 时取消异步行为、清空队列并淘汰旧 generation。 */
	void HandleOwnerDeath();
	/** Respawn 后保持空队列，允许新生命接受 Order。 */
	void HandleOwnerRespawn();
	/** 任意强制位移开始后取消当前寻路和追击检查，保留当前命令并进入暂停；已发射弹体等独立行为不撤销。 */
	void HandleOwnerMotionStarted();
	/** 最后一条强制位移结束后重新验证当前命令，从当前位置按需寻路；不会恢复位移前的旧 PathFollowing 请求。 */
	void HandleOwnerMotionFinished();
	/** 临时阻挡物变化时检查当前导航路径边界；相交则取消本次导航尝试并从当前命令重新寻路，命令句柄保持不变。 */
	void HandleGameplayBlockerChanged(const FBox& BlockerBounds);

	/** 返回当前状态。 */
	UFUNCTION(BlueprintPure, Category="Combat|Order", meta=(DisplayName="获取当前命令状态", ToolTip="返回战斗命令状态机的当前状态。"))
	ECombatOrderState GetCurrentState() const { return CurrentState; }
	/** 返回当前 OrderHandle；空闲时无效。 */
	UFUNCTION(BlueprintPure, Category="Combat|Order", meta=(DisplayName="获取当前命令句柄", ToolTip="返回当前命令的稳定句柄；空闲时句柄无效。"))
	FCombatOrderHandle GetCurrentOrderHandle() const;
	/** 返回尚未开始执行的排队命令数；当前正在执行或暂停的命令不计入。 */
	UFUNCTION(BlueprintPure, Category="Combat|Order", meta=(DisplayName="获取待处理命令数量", ToolTip="返回尚未开始执行的排队命令数；当前正在执行或暂停的命令不计入。"))
	int32 GetPendingOrderCount() const { return PendingOrders.Num(); }
	/** 返回可选 EQS Context 使用的当前权威移动目的点。 */
	FVector GetCurrentMoveGoal() const { return CurrentMoveGoal; }
	/** 订阅当前命令最终完成、失败或取消通知；初始接收结果不通过此委托发送，排队项被整体清空时也不逐条发送。 */
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
	/** 返回当前是否已绑定服务器 Combat AIController 的 PathFollowing，供控制器适配回归测试使用。 */
	bool HasPathFollowingBindingForTesting() const { return BoundPathFollowing.IsValid(); }

	/** 普通点移动可先运行的环境查询，用于选择可导航目的地；仅追踪目标时不使用，为空则直接向原目标点发起服务器寻路。 */
	UPROPERTY(EditAnywhere, Category="Combat|Order|Movement") TObjectPtr<UEnvQuery> MoveDestinationQuery;
	/** 纯移动命令允许距目标边缘的额外距离，单位为厘米；施法和攻击改用自身行为距离，所有距离判断另加统一容差。 */
	UPROPERTY(EditAnywhere, Category="Combat|Order|Movement", meta=(ClampMin="0", Units="cm")) float MovementAcceptanceRadius = 25.0f;
	/** 追击目标相对上次目的地移动达到此距离时取消旧导航并重新寻路，单位为厘米。 */
	UPROPERTY(EditAnywhere, Category="Combat|Order|Chase", meta=(ClampMin="1", Units="cm")) float ChaseWakeDistance = 50.0f;
	/** 服务器检查追击目标、范围和位置变化的间隔，单位为秒；卡顿积压时合并成一次当前状态检查。 */
	UPROPERTY(EditAnywhere, Category="Combat|Order|Chase", meta=(ClampMin="0.02", Units="s")) float ChaseCheckInterval = 0.10f;
	/** 追随单位、追击施法或攻击命令在追击检查中允许的最长世界游戏秒数；从命令成为当前项时开始计算，不因重新寻路而重置，超过后失败。 */
	UPROPERTY(EditAnywhere, Category="Combat|Order|Chase", meta=(ClampMin="0.1", Units="s")) float MaxChaseDuration = 10.0f;
	/** 移动受阻、请求无效、路径只到部分目的地或角色尚未停稳时允许的重试次数；每次等待 0.20 秒，超过上限后命令失败。 */
	UPROPERTY(EditAnywhere, Category="Combat|Order|Movement", meta=(ClampMin="0")) int32 MaxMoveRetries = 3;
	/** 等待执行的命令容量，不含当前项；达到上限时新的追加命令被拒绝，替换命令不受旧队列容量影响。 */
	UPROPERTY(EditAnywhere, Category="Combat|Order", meta=(ClampMin="1")) int32 MaxQueuedOrders = 16;

	/** 结束 Actor 前解绑所有 delegate 并使旧回调无效。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 绑定 ASC/Attack/Controller，并开始处理预置队列。 */
	virtual void BeginPlay() override;

private:
	/** 返回组件所属 Combat Unit。 */
	ACombatUnitCharacter* GetOwnerUnit() const;
	/** 只解析服务器 ACombatUnitAIController 上的 PathFollowing；其他 Controller 一律拒绝。 */
	UPathFollowingComponent* ResolvePathFollowingComponent() const;
	/** 幂等绑定 ASC/Attack 委托，避免动态 Spawn 时受组件 BeginPlay 顺序影响。 */
	void EnsureRuntimeBindings();
	/** 校验 payload 字段组合、有限位置、AbilitySpec 与目标 Actor。 */
	FCombatOperationResult ValidateOrderRequest(const FCombatOrderRequest& Request) const;
	/** 分配当前 generation/life generation 下的新句柄。 */
	FCombatOrderHandle AllocateOrderHandle() const;
	/** 从 FIFO 取出下一项并进入 Validating。 */
	bool BeginNextOrder();
	/** 停止当前异步行为、形成最终结果并清空当前项，广播后继续队列；委托回调可以提交新命令，重入由 Pump 保护。 */
	void CompleteCurrentOrder(bool bSuccess, FGameplayTag FailureTag, const FString& Diagnostic);
	/** 先提升命令代次使导航、技能和攻击旧回调失效，再取消当前异步行为并清空队列；可选只为当前项广播取消。 */
	void AdvanceGenerationAndCancel(FGameplayTag Reason, bool bBroadcastCurrent);
	/** 取消 EQS、Move、追击、Ability 和 attack windup。 */
	void CancelCurrentAsync(FGameplayTag Reason);
	/** 仅取消 EQS/Move/追击，不改变当前队列项。 */
	void CancelMovementAsync();
	/** 以水平胶囊边缘距离判断当前位置是否达到当前命令要求；纯移动使用完成容差，施法和攻击使用各自范围。 */
	bool IsCurrentDestinationReached() const;
	/** 返回当前行为需要的边缘范围。 */
	float GetCurrentDesiredRange() const;
	/** 朝当前 Actor/Point 目标设置服务器 XY 朝向。 */
	bool FaceCurrentTarget();
	/** 更新目标位置并递增导航尝试代次；普通点移动可先跑 EQS，动态目标直接发起服务器 AI 移动并安排追击检查。 */
	bool BeginMovement(bool bChasing);
	/** 要求单位由专用服务器 AIController 控制，向 PathFollowing 提交目标位置和接受半径；保存请求编号、命令句柄及本次导航代次用于回调验证。 */
	bool StartNavigationMove(bool bChasing);
	/** 为单位目标移动、施法追击或攻击追击安排定期检查；已有有效任务时不重复创建。 */
	void EnsureChaseSchedule();
	/** 定期检查同一命令和目标是否仍有效；已进入行为范围则停止移动并继续命令，目标明显位移则重新寻路，超过追击期限则失败。 */
	void HandleChaseCheck(FCombatOrderHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** 取消当前移动并增加重试计数，0.20 秒后重新处理同一命令；超过配置次数或无法调度时结束命令。 */
	void ScheduleMoveRetry(FGameplayTag FailureTag, const FString& Diagnostic);
	/** 重试 Schedule 到期后重新验证原 OrderHandle。 */
	void HandleMoveRetry(FCombatOrderHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** 向能力系统提交当前施法请求；若技能同步释放则当前命令可能当场完成，否则进入等待，后续不等待冷却或已发射弹体。 */
	void DispatchCurrentAbility();
	/** 尝试开始下一轮普通攻击；未就绪则等待攻击间隔，超出范围则追击，临时控制状态暂停，永久失败结束命令。 */
	void StartCurrentAttack();
	/** 技能允许释放命令时核对当前施法和技能句柄；按结果完成命令，或在中断策略要求时清空后续队列。 */
	void HandleAbilityOrderReleased(
		FGameplayAbilitySpecHandle Handle,
		bool bSuccess,
		FGameplayTag FailureTag,
		ECombatChannelInterruptOrderPolicy InterruptPolicy);
	/** 本轮普攻进入发射阶段后，让持续攻击命令等待再次就绪；远程攻击无需等待弹体命中即可开始下一轮。 */
	void HandleAttackLaunched(FCombatAttackHandle AttackHandle, FCombatOrderHandle OrderHandle);
	/** 攻击间隔结束后核对仍是同一持续攻击命令，再重新验证目标、范围与状态并尝试下一轮。 */
	void HandleAttackReady(FCombatOrderHandle OrderHandle);
	/** EQS 只接受当前 wrapper、OrderHandle 和成功状态。 */
	UFUNCTION() void HandleEqsFinished(UEnvQueryInstanceBlueprintWrapper* QueryInstance, EEnvQueryStatus::Type QueryStatus);
	/** 服务器导航完成时只处理匹配当前请求编号、命令、导航尝试代次和单位生命的回调；旧回调无副作用返回。 */
	void HandleMoveFinished(FAIRequestID RequestId, const FPathFollowingResult& Result);
	/** 统一处理真实或注入的移动结果；成功 partial path 只有实际达到行为范围时才可继续。 */
	void ResolveMoveCompletion(bool bSuccess, bool bPartial, FGameplayTag FailureTag, const FString& Diagnostic);
	/** 状态转换与结束输出统一结构化日志。 */
	void TransitionTo(ECombatOrderState NewState, FGameplayTag FailureTag = FGameplayTag(), const FString& Diagnostic = FString());

	/** 尚未开始的命令队列；当前项另存，完成入口负责切换，避免异步委托直接操作队首。 */
	TArray<FCombatQueuedOrder> PendingOrders;
	/** 唯一当前项。 */
	TOptional<FCombatQueuedOrder> CurrentOrder;
	/** 当前权威状态。 */
	ECombatOrderState CurrentState = ECombatOrderState::Idle;
	/** 替换、停止或死亡时递增；已有命令句柄仍可用于历史结果匹配，但不能再推进当前状态机。 */
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
	/** 同一命令每次重新寻路时递增，配合请求编号拒绝上一次路径、EQS 或阻挡变化前的旧回调。 */
	uint32 NavigationAttemptGeneration = 1;
	/** 已绑定 OnRequestFinished 的组件；Controller 变化时先解绑。 */
	TWeakObjectPtr<UPathFollowingComponent> BoundPathFollowing;
	/** 当前 EQS/Move 使用的目标位置和最近一次追击目标位置。 */
	FVector CurrentMoveGoal = FVector::ZeroVector;
	FVector LastChaseTargetLocation = FVector::ZeroVector;
	/** 当前命令成为队首时记录的世界游戏秒数；追击期限包含开始追击前的初始校验时间。 */
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
