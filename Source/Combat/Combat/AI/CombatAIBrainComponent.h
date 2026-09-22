#pragma once

#include "Components/StateTreeComponent.h"
#include "Combat/AI/CombatAITypes.h"
#include "Combat/AI/CombatAITacticalTypes.h"
#include "CombatAIBrainComponent.generated.h"

class UCombatAIProfileData;
class UEnvQuery;
struct FEnvQueryResult;
struct FCombatLogRecord;

/** 一次战术 EQS 的完整本地身份；QueryId 为空时表示正在等待 World 启动配额。 */
struct FCombatAITacticalQueryWork
{
	FCombatAIDecisionScope Scope;
	uint64 Token = 0;
	uint64 Generation = 0;
	uint64 Activation = 0;
	uint64 AssignmentRevision = 0;
	uint64 SnapshotRevision = 0;
	FName ConsumerSlot;
	uint32 SelfLife = 0;
	TWeakObjectPtr<ACombatUnitCharacter> Target;
	uint32 TargetLife = 0;
	TWeakObjectPtr<UEnvQuery> QueryTemplate;
	double StartedAt = 0.0;
	int32 QueryId = INDEX_NONE;
	uint32 BudgetDeferrals = 0;

	bool IsValid() const { return Generation != 0 && Activation != 0 && Scope.IsValid(); }
	bool HasEngineQuery() const { return QueryId != INDEX_NONE; }
};

/** EQS 回调只写入此邮箱；StateTree 的下一安全回合才准备公共 Move Order。 */
struct FCombatAITacticalQueryInbox
{
	FCombatAITacticalQueryWork Identity;
	FVector Location = FVector::ZeroVector;
	FString Diagnostic;
	bool bReady = false;
	bool bSuccess = false;
};

/**
 * 单位服务器上的唯一 StateTree 宿主和 Order Bridge。观察、准备和回执各自有明确所有者；不保存第二套战斗属性。
 * StateTree 决定动作顺序，组件只实现身份、原子交接和清理。手动接管/死亡先撤销 epoch，再精确取消旧动作。
 * 导航仍由原 AIController 执行；客户端不启动逻辑，事件只唤醒原生树，所有玩法期限通过 Combat Scheduler。
 */
UCLASS(ClassGroup=(Combat), meta=(DisplayName="战斗 AI 决策宿主", ToolTip="服务器 StateTree 与 Order 的适配组件；客户端只读取战斗复制结果。"))
class COMBAT_API UCombatAIBrainComponent : public UStateTreeComponent
{
	GENERATED_BODY()
public:
	UCombatAIBrainComponent();
	/** 服务器更换 Profile 并清理旧运行；默认无玩家指挥且未主动进入 Manual 时尝试启动。空值禁用。 */
	UFUNCTION(BlueprintCallable, Category="Combat|AI", meta=(DisplayName="设置 AI 配置", ToolTip="服务器更换配置并清理旧运行；空值禁用。手动接管之后仍需显式恢复自主行为。"))
	void ConfigureProfile(UPARAM(DisplayName="配置") UCombatAIProfileData* NewProfile);
	/** 显式恢复自主行为，停止已有手动队列后启动新运行；缺少初始化/配置/导航控制器时返回 false。 */
	UFUNCTION(BlueprintCallable, Category="Combat|AI", meta=(DisplayName="恢复自主决策", ToolTip="仅服务器可用；停止当前手动队列后启动配置的 StateTree。条件不满足返回失败。"))
	bool ResumeAutonomous();
	/** 只接受服务器输入；保存弱目标与版本并唤醒树，实际动作仍由公共 Order 预检/执行。 */
	UFUNCTION(BlueprintCallable, Category="Combat|AI", meta=(DisplayName="设置 AI 目标命令", ToolTip="提供 Move、Attack 或 Cast 意图，不直接执行；目标和技能仍由服务器公共入口复核。"))
	void SetObjective(UPARAM(DisplayName="目标命令") const FCombatOrderRequest& Request);
	/** 通过业务预检的玩家/脚本请求调用；Manual 状态保持到显式 Resume，不因网络 Owner 消失自动恢复。 */
	void SuspendForManualCommand();
	/** Unit 初始化/复活/控制器改变后调用；控制器丢失时同步停止，恢复时只重启 Autonomous。 */
	void RefreshReadiness();
	/** 发出合并唤醒；接收者检查工作区事实，不依赖事件必达或顺序。 */
	void Wake();
	/** 判断某次 AI 提交是否仍拥有本单位控制权；Order 的专用入口必须复核。 */
	bool CanSubmit(uint64 ExpectedEpoch) const;
	bool HasPendingObjective() const { return bHasObjective && Context.ObjectiveRevision != ResolvedRevision; }
	bool IsAutonomous() const { return bAutonomous; }
	uint64 GetRunSerial() const { return RunSerial; }
	uint64 GetControlEpoch() const { return ControlEpoch; }
	FCombatAIContext& GetContextData() { return Context; }
	const FCombatAIDecisionScope& GetDecisionScope() const { return Scope; }
	const FCombatAICompletionReceipt& GetLastReceipt() const { return LastReceipt; }
	uint64 GetSubmittedCount() const { return SubmittedCount; }
	uint64 GetResolvedCount() const { return ResolvedCount; }
	int32 GetActiveWaitCount() const { return Waits.Num(); }
	/** 每次真实 Task Enter 分配新激活身份；重选保留时不重新申请。 */
	uint64 AllocateActivation() { return ++NextActivation; }
	/** 父级服务独占 Scope；重复或并行 Scope 被拒绝，链接执行资产继承现有 Scope。 */
	FCombatAIDecisionScope BeginDecisionScope();
	/** 离开父状态时废弃准备/凭证并清理精确旧动作；旧 Scope 无副作用。 */
	void EndDecisionScope(FCombatAIDecisionScope Expected);
	/** 冻结当前目标，失败同样记录 Receipt；只允许无活动动作和未确认凭证时准备。 */
	bool Prepare(FCombatAIDecisionScope Expected, FName ConsumerSlot, uint64 Producer);
	/** 条件中断准备任务时清理自己尚未被消费的数据；普通成功 Exit 不调用。 */
	void DiscardPrepared(uint64 Producer);
	/** 消费一次准备结果并提交；同一激活重复 Enter 不重复下单，其他写入者被拒绝。 */
	bool BeginAction(FCombatAIDecisionScope Expected, FName ConsumerSlot, uint64 Activation);
	/** 仅由树安全回合调用；先处理终态，再处理 Objective 变化与普通攻击边界。 */
	EStateTreeRunStatus PollAction(uint64 Activation);
	/** 执行 Task Exit 只清理自己的桥接记录，不清除已形成的 Receipt。 */
	void EndAction(uint64 Activation);
	/** Resolve 独占消费 Receipt 并推进已处理修订；失败不回滚公共战斗结算。 */
	bool ResolveReceipt(FCombatAIDecisionScope Expected);
	/** 注册单次等待，0 秒也通过 Scheduler 后续回合唤醒；不使用 StateTree Delay 或 Actor Timer。 */
	bool BeginWait(uint64 Activation, float Seconds);
	bool IsWaitComplete(uint64 Activation) const { return CompletedWaits.Contains(Activation); }
	/** 等待任务退出时取消对应 Scheduler 并清除完成标记。 */
	void EndWait(uint64 Activation);

	/** 服务器替换空间职责；非法坐标/超长路线无副作用，成功增加修订并解除同职责故障等待。 */
	UFUNCTION(BlueprintCallable, Category="Combat|AI", meta=(DisplayName="设置 AI 空间职责", ToolTip="仅服务器接受锚点、最多 64 航点和循环标志；成功后从首点开始。有效重发可显式重试，不自动解除手动接管。"))
	bool SetAssignment(UPARAM(DisplayName="空间职责") const FCombatAIAssignment& NewAssignment);
	const FCombatAIKnowledgeSnapshot& GetKnowledge() const { return Knowledge; }
	int32 GetRouteCursor() const { return RouteCursor; }
	bool HasAssignment() const { return bHasAssignment; }
	bool IsReturnRequested() const { return bReturnRequested; }
	uint64 GetHomeCommitCount() const { return HomeCommitCount; }
	uint64 GetRouteCommitCount() const { return RouteCommitCount; }
	bool HasPerceptionSchedule() const { return PerceptionSchedule.IsValid(); }
	bool HasPerceptionSubscription() const { return PerceptionBinding.IsValid(); }
	const UCombatAIProfileData* GetProfile() const { return Profile; }
	/** 重建当前合法技能候选快照；只调用公共预检，不激活技能、不扣费、不提交 Order。 */
	bool EvaluateTacticalCandidates();
	const FCombatAITacticalSnapshot& GetTacticalSnapshot() const { return TacticalSnapshot; }
	/** 供 StateTree Condition/Consideration 读取本轮已发布事实，不触发重新评估。 */
	bool IsTacticalActionAvailable(ECombatAITacticalAction Action) const;
	float GetTacticalActionScore(ECombatAITacticalAction Action) const;
	/** 将已发布候选或已知目标冻结到现有 PreparedIntent 协议。 */
	bool PrepareTacticalOrder(ECombatAITacticalAction Action, FName Slot, uint64 Producer);
	/** 消费战术准备并记录活动行为起点，保持单一 Order 写入者。 */
	bool BeginTacticalAction(FCombatAIDecisionScope Expected, FName ConsumerSlot, uint64 Activation,
		ECombatAITacticalAction Action);
	/** 战术执行安全回合；Cast 等待公共终态，Attack 额外处理普通边界切换。 */
	EStateTreeRunStatus PollTacticalAction(uint64 Activation);
	/** 按准备操作选择通用或角色回执记账。 */
	bool ResolveTacticalReceipt();
	/** 启动或等待一次预算化战术站位查询；同一单位不允许第二个在途查询。 */
	bool BeginTacticalLocationQuery(FCombatAIDecisionScope Expected, FName ConsumerSlot, uint64 Activation);
	/** 在 StateTree 安全回合消费查询邮箱，并把成功点冻结成精确 MoveToPoint 意图。 */
	EStateTreeRunStatus PollTacticalLocationQuery(uint64 Activation);
	/** 查询任务异常退出时只取消匹配激活，不影响后继查询。 */
	void CancelTacticalLocationQuery(uint64 Activation);
	/** EQS Context 只读取当前查询冻结且生命仍匹配的目标。 */
	ACombatUnitCharacter* GetTacticalQueryTarget() const;
	int32 GetActiveTacticalQueryCount() const { return TacticalQuery.HasEngineQuery() ? 1 : 0; }
	/** 下列查询只提供事实，StateTree 的进入条件负责选择行为分支。 */
	bool NeedsReturn() const;
	bool CanEngageKnownTarget() const;
	bool HasRoutePoint() const;
	bool IsRetryPending() const { return bRetryPending; }
	bool IsRetryBlocked() const;
	int32 GetFailureCount(ECombatAIRoleOperation Operation) const;
	/** 原子角色准备：冻结对应锚点/候选/航点，仍进入同一 Scope/有效期/消费槽协议。 */
	bool PrepareRoleOrder(ECombatAIRoleOperation Operation, FName Slot, uint64 Producer);
	/** 角色 Execute 的安全回合：先终态，随后检查职责/感知资格，取消只影响自己的完整 Order。 */
	EStateTreeRunStatus PollRoleAction(uint64 Activation);
	/** 结果节点单次确认回执；只有到达证据匹配才推进路线或清归位请求，故障按职责/操作累计。 */
	bool ResolveRoleReceipt();
	/** 退避计时完成后清除匹配职责的待退避标志；新职责不能被旧等待修改。 */
	void FinishRoleRetry(uint64 Revision);

	virtual TSubclassOf<UStateTreeSchema> GetSchema() const override;
	virtual void StartLogic() override;
	virtual void RestartLogic() override;
	virtual void StopLogic(const FString& Reason) override;
	virtual void Cleanup() override;
	virtual void PauseLogic(const FString& Reason) override;
	virtual EAILogicResuming::Type ResumeLogic(const FString& Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
protected:
	/** Profile 在单位初始化结束才注入，跳过引擎对构造阶段空引用的警告。 */
	virtual void ValidateStateTreeReference() override {}
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FCombatAITacticalEQSStaleCallbackIsolationTest;
#endif
	/** 在启动和提交时复核服务器、Alive、已初始化 ASC 与导航控制器。 */
	bool IsReady() const;
	bool IsScopeCurrent(FCombatAIDecisionScope Expected) const;
	/** Order 委托只能记录终态并唤醒，提交期间暂存以涵盖同步完成。 */
	void OnOrderFinished(const FCombatOrderResult& Result);
	void RecordReceipt(const FCombatOrderResult& Result, uint64 Activation, uint64 Revision, uint64 Preparation);
	/** 停止后的兜底清理；先移除桥接身份，再精确取消，防止取消回调写回新运行。 */
	void ClearRuntime();
	/** 为显式输入与角色节点共用准备协议，不改变外部职责修订。 */
	bool PrepareRequest(FCombatAIDecisionScope Expected, FName ConsumerSlot, uint64 Producer, const FCombatOrderRequest& Request,
		ECombatAIRoleOperation Operation, int32 RouteIndex = INDEX_NONE);
	/** 启停 Brain 唯一感知服务；仅发布事实，不下命令。 */
	void StartPerception();
	void ClearPerception();
	/** 安排下一次感知；非负覆盖值用于首次错峰或预算延期。 */
	void SchedulePerception(float DelayOverride = -1.0f);
	void SampleKnowledge();
	/** 只接受已知且当时仍可观察的来源；不把诊断事件当作全知目标提供者。 */
	void ObserveThreat(const FCombatLogRecord& Record);
	/** 路线职责采用最近已确认航点，守点职责采用 Home；不是当前追击点。 */
	FVector GetDutyAnchor() const;
	/** 从排序后的合法快照选择，返回值只在下一次采样/清理前有效。 */
	const FCombatAIKnownTarget* SelectKnownTarget() const;
	bool IsTacticalQueryCurrent(const FCombatAITacticalQueryWork& Identity) const;
	bool TryStartTacticalLocationQuery(uint64 ExpectedGeneration);
	void ScheduleTacticalLocationQueryRetry(uint64 ExpectedGeneration);
	void OnTacticalLocationQueryFinished(TSharedPtr<FEnvQueryResult> Result, uint64 ExpectedGeneration);
	void ClearTacticalLocationQuery();
	void PublishTacticalLocationQueryFailure(const FCombatAITacticalQueryWork& Identity, const FString& Diagnostic);

	UPROPERTY(Transient) TObjectPtr<UCombatAIProfileData> Profile;
	UPROPERTY(Transient) FCombatAIContext Context;
	/** Objective 仅保存意图，Actor 引用移到弱指针，避免延长目标生命周期。 */
	FCombatOrderRequest Objective;
	TWeakObjectPtr<ACombatUnitCharacter> ObjectiveTarget;
	bool bHasObjective = false;
	bool bObjectiveHadTarget = false;
	bool bAutonomous = false;
	bool bManualOverride = false;
	bool bEnding = false;
	bool bStopping = false;
	bool bWakeQueued = false;
	uint64 RunSerial = 0, ControlEpoch = 1, NextActivation = 0, NextScope = 0, NextPreparation = 0;
	uint64 ResolvedRevision = 0, SubmittedCount = 0, ResolvedCount = 0;
	FCombatAIDecisionScope Scope;
	FCombatAIPreparedIntent Prepared;
	FCombatAICompletionReceipt Receipt, LastReceipt;
	/** 唯一活动 Bridge；保存冻结请求及完整 Order 身份，不能由回调直接切状态。 */
	FCombatAIPreparedIntent ActionIntent;
	uint64 ActionActivation = 0;
	FCombatOrderHandle ActionOrder;
	FCombatExecutionBoundaryTicket Boundary;
	bool bSubmitting = false;
	TArray<FCombatOrderResult> SynchronousResults;
	FDelegateHandle FinishedBinding, BoundaryBinding;
	TMap<uint64, FCombatScheduleHandle> Waits;
	TSet<uint64> CompletedWaits;
	FCombatAIAssignment Assignment;
	FCombatAIKnowledgeSnapshot Knowledge;
	FCombatAITacticalSnapshot TacticalSnapshot;
	uint64 NextTacticalRevision = 0;
	uint64 NextTacticalQueryGeneration = 0;
	FCombatAITacticalQueryWork TacticalQuery;
	FCombatAITacticalQueryInbox TacticalQueryInbox;
	FCombatScheduleHandle TacticalQueryRetrySchedule;
	/** 同一职责/知识快照的查询失败只尝试一次，等待下一次真实观察后再进入站位分支。 */
	uint64 RepositionFailureAssignmentRevision = 0;
	uint64 RepositionFailureSnapshotRevision = 0;
	ECombatAITacticalAction ActiveTacticalAction = ECombatAITacticalAction::Guard;
	double TacticalActionStartedAt = 0.0;
	bool bHasAssignment = false;
	bool bReturnRequested = false;
	bool bRetryPending = false;
	int32 RouteCursor = 0;
	/** 与下一航点游标分开，巡逻回绕后仍保留最近实际到达的职责锚点。 */
	int32 LastReachedRouteIndex = INDEX_NONE;
	TMap<ECombatAIRoleOperation, int32> Failures;
	ECombatAIRoleOperation RetryOperation = ECombatAIRoleOperation::Explicit;
	ECombatAIRoleStopReason RoleStopReason = ECombatAIRoleStopReason::None;
	TWeakObjectPtr<ACombatUnitCharacter> SelectedTarget;
	uint32 SelectedTargetLife = 0;
	double SelectedAt = 0, EngagementStartedAt = 0;
	uint64 HomeCommitCount = 0, RouteCommitCount = 0;
	FCombatScheduleHandle PerceptionSchedule;
	FDelegateHandle PerceptionBinding;
	uint32 PerceptionBudgetDeferrals = 0;
};
