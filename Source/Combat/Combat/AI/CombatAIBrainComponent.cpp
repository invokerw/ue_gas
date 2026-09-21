#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/AI/CombatAIProfileData.h"
#include "Combat/AI/CombatAIStateTreeSchema.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Unit/CombatUnitAIController.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Engine/World.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"

UCombatAIBrainComponent::UCombatAIBrainComponent()
{
	bStartLogicAutomatically = false;
	SetIsReplicatedByDefault(false);
}

TSubclassOf<UStateTreeSchema> UCombatAIBrainComponent::GetSchema() const { return UCombatAIStateTreeSchema::StaticClass(); }

bool UCombatAIBrainComponent::IsReady() const
{
	const auto* Unit = Cast<ACombatUnitCharacter>(GetOwner());
	return !bEnding && GetNetMode() != NM_Client && Unit && Unit->HasAuthority() && Unit->GetLifeState() == ECombatLifeState::Alive
		&& Unit->GetUnitDefinitionId().IsValid() && Unit->GetCombatAbilitySystemComponent()->IsCombatActorInfoInitialized()
		&& Cast<ACombatUnitAIController>(Unit->GetController()) && Profile && Profile->RootTree;
}

bool UCombatAIBrainComponent::CanSubmit(const uint64 ExpectedEpoch) const
{
	return bAutonomous && !bStopping && ControlEpoch == ExpectedEpoch && RunSerial != 0 && IsReady();
}

void UCombatAIBrainComponent::ConfigureProfile(UCombatAIProfileData* NewProfile)
{
	if (GetNetMode() == NM_Client || !GetOwner() || !GetOwner()->HasAuthority() || bEnding) return;
	StopLogic(TEXT("Profile changed"));
	Profile = NewProfile;
	Context.Unit = Cast<ACombatUnitCharacter>(GetOwner());
	if (!Profile) bAutonomous = false;
	else if (!bManualOverride && Context.Unit && !Context.Unit->GetCommandingPlayerController()) bAutonomous = true;
	if (Profile) SetStateTree(Profile->RootTree);
	RefreshReadiness();
}

bool UCombatAIBrainComponent::ResumeAutonomous()
{
	if (!IsReady()) return false;
	FString Diagnostic;
	if (!Profile->ValidateRuntime(Diagnostic)) return false;
	StopLogic(TEXT("Explicit autonomous resume"));
	Context.Unit->GetCombatOrderComponent()->StopAllOrders(CombatTags::Order_Failure_Cancelled);
	bManualOverride = false;
	bAutonomous = true;
	// 显式恢复是新一次决策机会；同一目标也允许重新执行。
	ResolvedRevision = 0;
	StartLogic();
	return IsRunning();
}

void UCombatAIBrainComponent::SetObjective(const FCombatOrderRequest& Request)
{
	if (GetNetMode() == NM_Client || !GetOwner() || !GetOwner()->HasAuthority() || bEnding) return;
	// 自动角色由空间职责输入驱动，显式目标不能伪装成新职责来绕过故障等待。
	if (Profile && Profile->bEnablePerception) return;
	Objective = Request;
	ObjectiveTarget = Request.TargetUnit;
	bObjectiveHadTarget = Request.TargetUnit != nullptr;
	Objective.TargetUnit = nullptr;
	bHasObjective = true;
	++Context.ObjectiveRevision;
	Wake();
}

void UCombatAIBrainComponent::SuspendForManualCommand()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	bManualOverride = true;
	bAutonomous = false;
	StopLogic(TEXT("Manual command takeover"));
}

void UCombatAIBrainComponent::RefreshReadiness()
{
	if (!IsReady()) { if (IsRunning()) StopLogic(TEXT("Unit not ready")); return; }
	if (bAutonomous && !IsRunning() && !bStopping) StartLogic();
}

void UCombatAIBrainComponent::StartLogic()
{
	if (!bAutonomous || bStopping || IsRunning() || !IsReady()) return;
	FString Diagnostic;
	if (!Profile->ValidateRuntime(Diagnostic))
	{
		UE_LOG(LogTemp, Warning, TEXT("AIStartRejected Unit=%s Reason=%s"), *GetNameSafe(GetOwner()), *Diagnostic);
		return;
	}
	ClearRuntime();
	++RunSerial;
	++ControlEpoch;
	Context.Unit = CastChecked<ACombatUnitCharacter>(GetOwner());
	SetStateTree(Profile->RootTree);
	auto* Orders = Context.Unit->GetCombatOrderComponent();
	FinishedBinding = Orders->OnOrderFinished().AddUObject(this, &UCombatAIBrainComponent::OnOrderFinished);
	BoundaryBinding = Orders->OnExecutionBoundaryReady().AddWeakLambda(this, [this](FCombatExecutionBoundaryTicket) { Wake(); });
	StartPerception();
	Super::StartLogic();
	if (!IsRunning()) ClearRuntime();
}

void UCombatAIBrainComponent::RestartLogic() { StopLogic(TEXT("Restart")); StartLogic(); }

void UCombatAIBrainComponent::StopLogic(const FString& Reason)
{
	if (bStopping) return;
	TGuardValue<bool> Guard(bStopping, true);
	++ControlEpoch;
	Super::StopLogic(Reason);
	ClearRuntime();
}

void UCombatAIBrainComponent::Cleanup() { StopLogic(TEXT("Cleanup")); Super::Cleanup(); }
void UCombatAIBrainComponent::PauseLogic(const FString& Reason) { StopLogic(Reason); }
EAILogicResuming::Type UCombatAIBrainComponent::ResumeLogic(const FString& Reason) { StartLogic(); return EAILogicResuming::RestartedInstead; }

void UCombatAIBrainComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	bWakeQueued = false;
	if (!CanSubmit(ControlEpoch)) { StopLogic(TEXT("Authority or readiness lost")); return; }
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	// 树可能在同一次 Tick 内消费任务新发出的事件后休眠，不能把该事件仍当作“排队中”而吞掉下一次 Scheduler 唤醒。
	bWakeQueued = false;
	if (!IsRunning()) ClearRuntime();
}

void UCombatAIBrainComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	bEnding = true;
	StopLogic(TEXT("EndPlay"));
	Super::EndPlay(Reason);
}

void UCombatAIBrainComponent::Wake()
{
	if (bWakeQueued || bStopping || !IsRunning() || !IsReady()) return;
	bWakeQueued = true;
	FCombatAIWakeSignal Signal;
	Signal.Run = RunSerial;
	SendStateTreeEvent({}, FConstStructView::Make(Signal), TEXT("CombatAI"));
}

bool UCombatAIBrainComponent::IsScopeCurrent(const FCombatAIDecisionScope Expected) const
{
	return Expected.IsValid() && Expected == Scope && Expected.Run == RunSerial && Expected.ControlEpoch == ControlEpoch
		&& Context.Unit && Expected.Life == Context.Unit->GetLifeGeneration() && CanSubmit(Expected.ControlEpoch);
}

FCombatAIDecisionScope UCombatAIBrainComponent::BeginDecisionScope()
{
	if (!CanSubmit(ControlEpoch) || Scope.IsValid()) return {};
	Scope = { RunSerial, ControlEpoch, Context.Unit->GetLifeGeneration(), ++NextScope };
	return Scope;
}

void UCombatAIBrainComponent::EndDecisionScope(const FCombatAIDecisionScope Expected)
{
	if (!(Expected == Scope) || !Expected.IsValid()) return;
	EndAction(ActionActivation);
	Prepared = {};
	Receipt = {};
	Scope = {};
}

bool UCombatAIBrainComponent::Prepare(const FCombatAIDecisionScope Expected, const FName ConsumerSlot, const uint64 Producer)
{
	FCombatOrderRequest Request = Objective;
	Request.TargetUnit = ObjectiveTarget.Get();
	return PrepareRequest(Expected, ConsumerSlot, Producer, Request, ECombatAIRoleOperation::Explicit);
}

bool UCombatAIBrainComponent::PrepareRequest(const FCombatAIDecisionScope Expected, const FName ConsumerSlot, const uint64 Producer,
	const FCombatOrderRequest& Request, ECombatAIRoleOperation Operation, int32 RouteIndex)
{
	if (!IsScopeCurrent(Expected) || ActionActivation || Receipt.bValid) return false;
	Prepared = {};
	const uint64 Preparation = ++NextPreparation;
	auto Validation = Context.Unit->GetCombatOrderComponent()->PreflightAIOrder(Request);
	const bool bInputValid = Operation == ECombatAIRoleOperation::Explicit
		? bHasObjective && (!bObjectiveHadTarget || ObjectiveTarget.IsValid()) : bHasAssignment;
	if (!bInputValid || ConsumerSlot.IsNone() || !Validation.bSuccess)
	{
		FCombatOrderResult Result;
		Result.State = ECombatOrderState::Failed;
		Result.FailureTag = Validation.FailureTag.IsValid() ? Validation.FailureTag : CombatTags::Order_Failure_InvalidRequest.GetTag();
		Result.Diagnostic = TEXT("AI preparation rejected: ") + Validation.Diagnostic;
		RecordReceipt(Result, Producer, Context.ObjectiveRevision, Preparation);
		Receipt.Operation = Operation;
		Receipt.RouteIndex = RouteIndex;
		return false;
	}
	Prepared.Scope = Scope;
	Prepared.Preparation = Preparation;
	Prepared.Producer = Producer;
	Prepared.ConsumerSlot = ConsumerSlot;
	Prepared.ObjectiveRevision = Context.ObjectiveRevision;
	Prepared.Target = Request.TargetUnit.Get();
	Prepared.bHadTarget = Request.TargetUnit != nullptr;
	Prepared.TargetLife = Request.TargetUnit ? Request.TargetUnit->GetLifeGeneration() : 0;
	Prepared.Request = Request;
	Prepared.Request.TargetUnit = nullptr;
	Prepared.Operation = Operation;
	Prepared.RouteIndex = RouteIndex;
	Prepared.ExpiresAt = GetWorld()->GetTimeSeconds() + Profile->IntentLifetime;
	return true;
}

void UCombatAIBrainComponent::DiscardPrepared(const uint64 Producer)
{
	if (Prepared.Producer == Producer) Prepared = {};
}

bool UCombatAIBrainComponent::BeginAction(const FCombatAIDecisionScope Expected, const FName ConsumerSlot, const uint64 Activation)
{
	if (!IsScopeCurrent(Expected) || !Activation) return false;
	if (ActionActivation) return ActionActivation == Activation;
	if (Receipt.bValid) return false;
	const auto Intent = Prepared;
	Prepared = {};
	const bool bValid = Intent.Preparation && Intent.Scope == Expected && Intent.ConsumerSlot == ConsumerSlot
		&& Intent.ObjectiveRevision == Context.ObjectiveRevision && GetWorld()->GetTimeSeconds() <= Intent.ExpiresAt
		&& (!Intent.bHadTarget || (Intent.Target.IsValid() && Intent.Target->GetLifeGeneration() == Intent.TargetLife));
	if (!bValid)
	{
		FCombatOrderResult Result;
		Result.State = ECombatOrderState::Failed;
		Result.FailureTag = CombatTags::Order_Failure_InvalidRequest;
		Result.Diagnostic = TEXT("Prepared intent missing, stale, expired or wrong consumer slot");
		RecordReceipt(Result, Activation, Intent.Preparation ? Intent.ObjectiveRevision : Context.ObjectiveRevision, Intent.Preparation);
		Receipt.Operation = Intent.Operation;
		Receipt.RouteIndex = Intent.RouteIndex;
		return false;
	}
	ActionActivation = Activation;
	ActionIntent = Intent;
	RoleStopReason = ECombatAIRoleStopReason::None;
	if (Intent.Operation == ECombatAIRoleOperation::Attack)
	{
		EngagementStartedAt = GetWorld()->GetTimeSeconds();
		GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>()->Cancel(PerceptionSchedule);
		PerceptionSchedule = {};
		SchedulePerception();
	}
	FCombatOrderRequest Request = Intent.Request;
	Request.TargetUnit = Intent.Target.Get();
	SynchronousResults.Reset();
	bSubmitting = true;
	++SubmittedCount;
	const FCombatOrderResult Accepted = Context.Unit->GetCombatOrderComponent()->IssueAutonomousOrder(Request, this, ControlEpoch);
	bSubmitting = false;
	// 同步技能/委托可能在 IssueOrder 栈内造成死亡或手动接管；返回后不可重新安装旧动作。
	if (ActionActivation != Activation || !IsScopeCurrent(Intent.Scope))
	{
		Context.Unit->GetCombatOrderComponent()->CancelCurrentOrderIfMatches(Accepted.Handle, CombatTags::Order_Failure_Cancelled);
		SynchronousResults.Reset();
		return false;
	}
	ActionOrder = Accepted.Handle;
	if (!Accepted.bSuccess) RecordReceipt(Accepted, Activation, Intent.ObjectiveRevision, Intent.Preparation);
	else
	{
		for (const auto& Result : SynchronousResults)
			if (Result.Handle == ActionOrder) { RecordReceipt(Result, Activation, Intent.ObjectiveRevision, Intent.Preparation); break; }
	}
	SynchronousResults.Reset();
	return true;
}

void UCombatAIBrainComponent::OnOrderFinished(const FCombatOrderResult& Result)
{
	if (!ActionActivation || !IsScopeCurrent(ActionIntent.Scope)) return;
	if (bSubmitting) { if (SynchronousResults.Num() < 4) SynchronousResults.Add(Result); return; }
	if (ActionOrder.IsValid() && Result.Handle == ActionOrder)
		RecordReceipt(Result, ActionActivation, ActionIntent.ObjectiveRevision, ActionIntent.Preparation);
}

void UCombatAIBrainComponent::RecordReceipt(const FCombatOrderResult& Result, uint64 Activation, uint64 Revision, uint64 Preparation)
{
	if (Receipt.bValid || !IsScopeCurrent(Scope)) return;
	Receipt = { Scope, Preparation, Activation, Revision, Result, true };
	if (ActionActivation == Activation)
	{
		Receipt.Operation = ActionIntent.Operation;
		Receipt.RouteIndex = ActionIntent.RouteIndex;
		Receipt.StopReason = RoleStopReason;
	}
	Wake();
}

EStateTreeRunStatus UCombatAIBrainComponent::PollAction(const uint64 Activation)
{
	// 终态优先：同帧 ObjectiveChanged 不能跳过待消费的成功/失败凭证。
	if (Receipt.bValid && Receipt.Activation == Activation) return EStateTreeRunStatus::Succeeded;
	if (ActionActivation != Activation || !IsScopeCurrent(ActionIntent.Scope)) return EStateTreeRunStatus::Failed;
	if (Context.ObjectiveRevision != ActionIntent.ObjectiveRevision)
	{
		auto* Orders = Context.Unit->GetCombatOrderComponent();
		// 重复发布同一有效动作只刷新其观察版本，不取消重发。Actor 使用弱身份和生命代次单独比较。
		if (ObjectiveTarget == ActionIntent.Target && (!ActionIntent.bHadTarget || (ObjectiveTarget.IsValid()
			&& ObjectiveTarget->GetLifeGeneration() == ActionIntent.TargetLife))
			&& FCombatOrderRequest::StaticStruct()->CompareScriptStruct(&Objective, &ActionIntent.Request, 0))
		{
			ActionIntent.ObjectiveRevision = Context.ObjectiveRevision;
			Orders->ReleaseExecutionBoundary(Boundary);
			Boundary = {};
			return EStateTreeRunStatus::Running;
		}
		if (ActionIntent.Request.Type == ECombatOrderType::AttackTarget)
		{
			// 超时可能已释放旧票据；重新申请时会重新检查是否又进入了前摇。
			if (!Orders->IsExecutionBoundaryReady(Boundary)) Boundary = Orders->RequestExecutionBoundary(ActionOrder, Profile->BoundaryHoldSeconds);
			if (!Orders->IsExecutionBoundaryReady(Boundary)) return EStateTreeRunStatus::Running;
		}
		else if (ActionIntent.Request.Type != ECombatOrderType::MoveToPoint && ActionIntent.Request.Type != ECombatOrderType::MoveToUnit)
			return EStateTreeRunStatus::Running; // Cast 只等公共 OrderReleased，不取消已提交的施法。
		Orders->CancelCurrentOrderIfMatches(ActionOrder, CombatTags::Order_Failure_Cancelled);
		if (Receipt.bValid) return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}

void UCombatAIBrainComponent::EndAction(const uint64 Activation)
{
	if (!Activation || ActionActivation != Activation) return;
	const auto OldOrder = ActionOrder;
	const bool bWasTerminal = Receipt.bValid && Receipt.Activation == Activation;
	ActionActivation = 0;
	ActionOrder = {};
	ActionIntent = {};
	Boundary = {};
	if (!bWasTerminal && Context.Unit) Context.Unit->GetCombatOrderComponent()->CancelCurrentOrderIfMatches(OldOrder, CombatTags::Order_Failure_Cancelled);
}

bool UCombatAIBrainComponent::ResolveReceipt(const FCombatAIDecisionScope Expected)
{
	if (!IsScopeCurrent(Expected) || !Receipt.bValid || !(Receipt.Scope == Expected)) return false;
	LastReceipt = Receipt;
	ResolvedRevision = Receipt.ObjectiveRevision;
	++ResolvedCount;
	UE_LOG(LogTemp, Verbose, TEXT("AIReceipt Unit=%s Run=%llu Scope=%llu Preparation=%llu Result=%d Detail=%s"),
		*GetNameSafe(GetOwner()), RunSerial, Scope.Serial, Receipt.Preparation, Receipt.Result.bSuccess, *Receipt.Result.Diagnostic);
	Receipt = {};
	return true;
}

bool UCombatAIBrainComponent::BeginWait(const uint64 Activation, const float Seconds)
{
	if (!CanSubmit(ControlEpoch) || !Activation || Waits.Contains(Activation)) return false;
	auto* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>();
	const uint64 Epoch = ControlEpoch;
	const auto Handle = Scheduler->ScheduleOnce(this, Seconds, 0, FCombatScheduledDelegate::CreateWeakLambda(this,
		[this, Activation, Epoch](const FCombatScheduledTickContext&)
		{
			if (!CanSubmit(Epoch) || !Waits.Contains(Activation)) return;
			CompletedWaits.Add(Activation);
			Wake();
		}));
	if (!Handle.IsValid()) return false;
	Waits.Add(Activation, Handle);
	return true;
}

void UCombatAIBrainComponent::EndWait(const uint64 Activation)
{
	if (const auto* Handle = Waits.Find(Activation))
		if (auto* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr) Scheduler->Cancel(*Handle);
	Waits.Remove(Activation);
	CompletedWaits.Remove(Activation);
}

void UCombatAIBrainComponent::ClearRuntime()
{
	ClearPerception();
	EndAction(ActionActivation);
	if (Context.Unit)
	{
		auto* Orders = Context.Unit->GetCombatOrderComponent();
		Orders->OnOrderFinished().Remove(FinishedBinding);
		Orders->OnExecutionBoundaryReady().Remove(BoundaryBinding);
	}
	FinishedBinding.Reset(); BoundaryBinding.Reset();
	if (auto* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr) Scheduler->CancelAllForOwner(this);
	Waits.Reset(); CompletedWaits.Reset();
	Scope = {}; Prepared = {}; Receipt = {}; Boundary = {};
	SynchronousResults.Reset();
	bSubmitting = false;
	bWakeQueued = false;
}
