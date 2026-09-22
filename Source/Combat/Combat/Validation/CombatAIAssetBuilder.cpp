#include "Combat/Validation/CombatAIAssetBuilder.h"

#if WITH_EDITOR
#include "Combat/AI/CombatAIProfileData.h"
#include "Combat/AI/CombatAIStateTreeSchema.h"
#include "Combat/AI/CombatAIStateTreeTasks.h"
#include "Combat/AI/CombatAIRoleTasks.h"
#include "Combat/AI/CombatAITacticalTasks.h"
#include "Combat/AI/CombatAITacticalTargetContext.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace CombatAIAssetBuilder
{
	/** 递归检查活动父链，链接资产继承父 Scope；当前路径集合用于拒绝引用环。 */
	bool ValidateState(const UStateTreeState& State, int32 Scopes, int32 Phases, TSet<const UStateTree*>& Stack, FString& Diagnostic)
	{
		if (!State.bEnabled) return true;
		if (State.Type == EStateTreeStateType::Linked || State.Type == EStateTreeStateType::Subtree)
		{ Diagnostic = TEXT("Stage A reusable actions use Linked Asset; local subtree routing is not enabled"); return false; }
		for (const auto& Task : State.Tasks)
		{
			const auto* Type = Task.Node.GetScriptStruct();
			if (!Type || !Type->IsChildOf(FCombatAITaskBase::StaticStruct())) { Diagnostic = TEXT("Only Combat AI tasks are allowed"); return false; }
			if (!Task.Node.Get<FCombatAITaskBase>().bTaskEnabled)
			{ Diagnostic = TEXT("Stage A protocol tasks must remain enabled; disable the complete branch instead"); return false; }
			if (Type->IsChildOf(FCombatAIPrepareOrderTask::StaticStruct()) || Type->IsChildOf(FCombatAIExecuteOrderTask::StaticStruct()))
			{
				const auto* Parameters = Task.Instance.GetPtr<FCombatAITaskInstanceData>();
				if (!Parameters || Parameters->ConsumerSlot.IsNone()) { Diagnostic = TEXT("AI command tasks require a non-empty consumer slot"); return false; }
			}
			if (const auto* Parameters = Task.Instance.GetPtr<FCombatAIRoleTaskInstanceData>())
			{
				if (Type->IsChildOf(FCombatAIPrepareRoleTask::StaticStruct()) && (Parameters->Operation < ECombatAIRoleOperation::Attack || Parameters->Operation > ECombatAIRoleOperation::Route))
				{ Diagnostic = TEXT("Role preparation requires Attack, Home or Route operation"); return false; }
				if (Type->IsChildOf(FCombatAIRoleWaitTask::StaticStruct()) && Parameters->Wait > ECombatAIRoleWait::RoutePause)
				{ Diagnostic = TEXT("Role wait has an unsupported wake condition"); return false; }
			}
			Scopes += Type->IsChildOf(FCombatAIDecisionScopeTask::StaticStruct()) ? 1 : 0;
			Phases += Type->IsChildOf(FCombatAIPrepareOrderTask::StaticStruct()) || Type->IsChildOf(FCombatAIExecuteOrderTask::StaticStruct())
				|| Type->IsChildOf(FCombatAIResolveReceiptTask::StaticStruct()) ? 1 : 0;
		}
		if (Scopes > 1 || Phases > 1 || (Phases && Scopes != 1))
		{
			Diagnostic = TEXT("Active path requires one Scope and at most one Prepare/Execute/Resolve phase: ") + State.Name.ToString();
			return false;
		}
		for (const auto& Transition : State.Transitions)
			if (Transition.bDelayTransition || EnumHasAnyFlags(Transition.Trigger, EStateTreeTransitionTrigger::OnTick | EStateTreeTransitionTrigger::OnEvent))
			{
				Diagnostic = TEXT("Stage A uses Scheduler waits and completion transitions, not delayed or polling transitions"); return false;
			}
		if (State.LinkedAsset)
		{
			const auto* Linked = State.LinkedAsset.Get();
			const auto* Data = Cast<UStateTreeEditorData>(Linked->EditorData);
			if (Stack.Contains(Linked) || Stack.Num() >= 16 || !Data || !Data->Schema || !Data->Schema->IsA<UCombatAIStateTreeSchema>() || !Data->Evaluators.IsEmpty() || !Data->GlobalTasks.IsEmpty())
			{ Diagnostic = TEXT("Linked AI asset has invalid schema, cycle, depth or global tasks"); return false; }
			Stack.Add(Linked);
			for (const auto& Child : Data->SubTrees) if (Child && !ValidateState(*Child, Scopes, Phases, Stack, Diagnostic)) return false;
			Stack.Remove(Linked);
		}
		for (const auto& Child : State.Children) if (Child && !ValidateState(*Child, Scopes, Phases, Stack, Diagnostic)) return false;
		return true;
	}
	/** 建立一致 Schema 的编辑器数据；编译器负责生成可 cook 的紧凑运行数据。 */
	UStateTreeEditorData* Initialize(UStateTree& Tree)
	{
		auto* Data = NewObject<UStateTreeEditorData>(&Tree);
		Data->Schema = NewObject<UCombatAIStateTreeSchema>(Data);
		Tree.EditorData = Data;
		return Data;
	}
	/** 编译失败输出原生诊断并返回空值，禁止保存不可运行树。 */
	UStateTree* Compile(UStateTree* Tree)
	{
		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		if (!Compiler.Compile(*Tree)) { Log.DumpToLog(LogTemp); return nullptr; }
		return Tree;
	}
	/** 保存新资产并注册 AssetRegistry；保存失败作为命令行失败处理。 */
	bool Save(UObject* Asset)
	{
		if (!Asset) return false;
		Asset->SetFlags(RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Asset);
		Asset->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const FString Filename = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		return UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, Args);
	}
	/** 只保存已经注册的资产；用于一次性补齐早期阶段 C 生成物，不重复发送 AssetCreated。 */
	bool SaveExisting(UObject* Asset)
	{
		if (!Asset) return false;
		Asset->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		return UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, Args);
	}
	/** 填充阶段 C 根树；硬职责选择包住最后的 Utility 分支，所有结果重新回到硬选择点。 */
	UStateTree* PopulateTacticalRootTree(UStateTree& Tree)
	{
		auto& Root = Initialize(Tree)->AddRootState();
		Root.AddTask<FCombatAIDecisionScopeTask>();
		auto& Initial = Root.AddChildState(TEXT("等待合法职责"));
		Initial.AddTask<FCombatAIRoleWaitTask>().GetInstanceData().Wait = ECombatAIRoleWait::Assignment;
		auto& HardSelect = Root.AddChildState(TEXT("硬优先级选择"));

		auto& Blocked = HardSelect.AddChildState(TEXT("故障超限等待新职责"));
		Blocked.AddEnterCondition<FCombatAIRoleCondition>().GetInstanceData().Fact = ECombatAIRoleFact::RetryBlocked;
		Blocked.AddTask<FCombatAIRoleWaitTask>().GetInstanceData().Wait = ECombatAIRoleWait::NewAssignment;
		auto& Retry = HardSelect.AddChildState(TEXT("有界故障退避"));
		Retry.AddEnterCondition<FCombatAIRoleCondition>().GetInstanceData().Fact = ECombatAIRoleFact::RetryPending;
		Retry.AddTask<FCombatAIRoleWaitTask>().GetInstanceData().Wait = ECombatAIRoleWait::Retry;

		auto& Return = HardSelect.AddChildState(TEXT("优先归位"));
		Return.AddEnterCondition<FCombatAIRoleCondition>().GetInstanceData().Fact = ECombatAIRoleFact::NeedReturn;
		auto& ReturnPrepare = Return.AddChildState(TEXT("准备归位意图"));
		auto& ReturnPrepareData = ReturnPrepare.AddTask<FCombatAIPrepareRoleTask>().GetInstanceData();
		ReturnPrepareData.Operation = ECombatAIRoleOperation::Home;
		ReturnPrepareData.ConsumerSlot = TEXT("TacticalHome");
		auto& ReturnExecute = Return.AddChildState(TEXT("执行归位命令"));
		ReturnExecute.AddTask<FCombatAIExecuteRoleTask>().GetInstanceData().ConsumerSlot = TEXT("TacticalHome");
		auto& ReturnResolve = Return.AddChildState(TEXT("确认归位结果"));
		ReturnResolve.AddTask<FCombatAIResolveRoleTask>();
		ReturnPrepare.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded,
			EStateTreeTransitionType::GotoState, &ReturnExecute);
		ReturnPrepare.AddTransition(EStateTreeTransitionTrigger::OnStateFailed,
			EStateTreeTransitionType::GotoState, &ReturnResolve);
		ReturnExecute.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted,
			EStateTreeTransitionType::GotoState, &ReturnResolve);
		ReturnResolve.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted,
			EStateTreeTransitionType::GotoState, &HardSelect);

		auto& Tactical = HardSelect.AddChildState(TEXT("战术效用选择"));
		auto& Evaluate = Tactical.AddChildState(TEXT("发布战术候选"));
		Evaluate.AddTask<FCombatAIEvaluateTacticsTask>();
		auto& Select = Tactical.AddChildState(TEXT("最高效用战术选择"));
		Select.SelectionBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenWithHighestUtility;
		auto& Execute = Tactical.AddChildState(TEXT("执行战术命令"));
		auto& ExecuteData = Execute.AddTask<FCombatAIExecuteTacticalTask>().GetInstanceData();
		ExecuteData.ConsumerSlot = TEXT("TacticalAction");
		auto& Resolve = Tactical.AddChildState(TEXT("确认战术结果"));
		Resolve.AddTask<FCombatAIResolveTacticalTask>();

		const auto AddAction = [&](const TCHAR* NameValue, const ECombatAITacticalAction Action)
		{
			auto& Branch = Select.AddChildState(NameValue);
			Branch.AddEnterCondition<FCombatAITacticalCondition>().GetInstanceData().Action = Action;
			Branch.AddConsideration<FCombatAITacticalConsideration>().GetInstanceData().Action = Action;
			auto& Prepare = Branch.AddChildState(TEXT("准备战术意图"));
			auto& PrepareData = Action == ECombatAITacticalAction::Reposition
				? Prepare.AddTask<FCombatAIQueryTacticalLocationTask>().GetInstanceData()
				: Prepare.AddTask<FCombatAIPrepareTacticalTask>().GetInstanceData();
			PrepareData.Action = Action;
			PrepareData.ConsumerSlot = TEXT("TacticalAction");
			Prepare.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded,
				EStateTreeTransitionType::GotoState, &Execute);
			Prepare.AddTransition(EStateTreeTransitionTrigger::OnStateFailed,
				EStateTreeTransitionType::GotoState, &Resolve);
		};
		AddAction(TEXT("主动施法"), ECombatAITacticalAction::Cast);
		AddAction(TEXT("战术站位"), ECombatAITacticalAction::Reposition);
		AddAction(TEXT("普通攻击"), ECombatAITacticalAction::Attack);
		auto& Guard = Select.AddChildState(TEXT("观察等待"));
		Guard.AddEnterCondition<FCombatAITacticalCondition>().GetInstanceData().Action = ECombatAITacticalAction::Guard;
		Guard.AddConsideration<FCombatAITacticalConsideration>().GetInstanceData().Action = ECombatAITacticalAction::Guard;
		Guard.AddTask<FCombatAIRoleWaitTask>().GetInstanceData().Wait = ECombatAIRoleWait::Decision;

		Initial.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded,
			EStateTreeTransitionType::GotoState, &HardSelect);
		Blocked.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded,
			EStateTreeTransitionType::GotoState, &HardSelect);
		Retry.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded,
			EStateTreeTransitionType::GotoState, &HardSelect);
		Evaluate.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted,
			EStateTreeTransitionType::GotoState, &Select);
		Execute.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted,
			EStateTreeTransitionType::GotoState, &Resolve);
		Resolve.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted,
			EStateTreeTransitionType::GotoState, &HardSelect);
		Guard.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted,
			EStateTreeTransitionType::GotoState, &HardSelect);
		return Compile(&Tree);
	}
}

UStateTree* FCombatAIAssetBuilder::BuildActionTree(UObject* Outer, FName Name)
{
	auto* Tree = NewObject<UStateTree>(Outer, Name);
	auto& Root = CombatAIAssetBuilder::Initialize(*Tree)->AddRootState();
	Root.AddTask<FCombatAIExecuteOrderTask>();
	Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);
	return CombatAIAssetBuilder::Compile(Tree);
}

UStateTree* FCombatAIAssetBuilder::BuildRootTree(UObject* Outer, UStateTree* ActionTree, FName Name)
{
	if (!ActionTree) return nullptr;
	auto* Tree = NewObject<UStateTree>(Outer, Name);
	auto& Root = CombatAIAssetBuilder::Initialize(*Tree)->AddRootState();
	auto& Idle = Root.AddChildState(TEXT("等待显式目标"));
	Idle.AddTask<FCombatAIWaitObjectiveTask>();
	auto& Decision = Root.AddChildState(TEXT("决策作用域"));
	Decision.AddTask<FCombatAIDecisionScopeTask>();
	auto& Prepare = Decision.AddChildState(TEXT("准备命令"));
	Prepare.AddTask<FCombatAIPrepareOrderTask>();
	auto& Execute = Decision.AddChildState(TEXT("链接执行"), EStateTreeStateType::LinkedAsset);
	Execute.SetLinkedStateAsset(ActionTree);
	auto& Resolve = Decision.AddChildState(TEXT("确认完成凭证"));
	Resolve.AddTask<FCombatAIResolveReceiptTask>();
	auto& Wait = Root.AddChildState(TEXT("有界决策间隔"));
	Wait.AddTask<FCombatAIWaitTask>();
	Idle.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Decision);
	Prepare.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Execute);
	Prepare.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Resolve);
	Execute.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Resolve);
	Resolve.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Wait);
	Wait.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Idle);
	return CombatAIAssetBuilder::Compile(Tree);
}

UStateTree* FCombatAIAssetBuilder::BuildRoleActionTree(UObject* Outer, ECombatAIRoleOperation Operation, FName Slot, FName Name)
{
	auto* Tree = NewObject<UStateTree>(Outer, Name);
	auto& Root = CombatAIAssetBuilder::Initialize(*Tree)->AddRootState();
	auto& Prepare = Root.AddChildState(TEXT("准备职责意图"));
	auto& PrepareData = Prepare.AddTask<FCombatAIPrepareRoleTask>().GetInstanceData();
	PrepareData.Operation = Operation;
	PrepareData.ConsumerSlot = Slot;
	auto& Execute = Root.AddChildState(TEXT("执行公共命令"));
	Execute.AddTask<FCombatAIExecuteRoleTask>().GetInstanceData().ConsumerSlot = Slot;
	auto& Resolve = Root.AddChildState(TEXT("确认职责完成凭证"));
	Resolve.AddTask<FCombatAIResolveRoleTask>();
	Prepare.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Execute);
	Prepare.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Resolve);
	Execute.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Resolve);
	Resolve.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::Failed);
	if (Operation == ECombatAIRoleOperation::Route)
	{
		auto& Pause = Root.AddChildState(TEXT("到点或中断后短暂停留"));
		Pause.AddTask<FCombatAIRoleWaitTask>().GetInstanceData().Wait = ECombatAIRoleWait::RoutePause;
		Resolve.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Pause);
		Pause.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);
	}
	else Resolve.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::Succeeded);
	return CombatAIAssetBuilder::Compile(Tree);
}

UStateTree* FCombatAIAssetBuilder::BuildGuardTree(UObject* Outer, FName Name)
{
	auto* Tree = NewObject<UStateTree>(Outer, Name);
	auto& Root = CombatAIAssetBuilder::Initialize(*Tree)->AddRootState();
	Root.AddTask<FCombatAIRoleWaitTask>().GetInstanceData().Wait = ECombatAIRoleWait::Decision;
	Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);
	return CombatAIAssetBuilder::Compile(Tree);
}

UStateTree* FCombatAIAssetBuilder::BuildRoleRootTree(UObject* Outer, UStateTree* Home, UStateTree* Engage, UStateTree* Duty, UStateTree* Guard, FName Name)
{
	if (!Home || !Engage || !Guard) return nullptr;
	auto* Tree = NewObject<UStateTree>(Outer, Name);
	auto& Root = CombatAIAssetBuilder::Initialize(*Tree)->AddRootState();
	Root.AddTask<FCombatAIDecisionScopeTask>();
	auto& Initial = Root.AddChildState(TEXT("等待合法职责"));
	Initial.AddTask<FCombatAIRoleWaitTask>().GetInstanceData().Wait = ECombatAIRoleWait::Assignment;
	auto& Select = Root.AddChildState(TEXT("有序职责选择"));
	auto& Blocked = Select.AddChildState(TEXT("故障超限等待新职责"));
	Blocked.AddEnterCondition<FCombatAIRoleCondition>().GetInstanceData().Fact = ECombatAIRoleFact::RetryBlocked;
	Blocked.AddTask<FCombatAIRoleWaitTask>().GetInstanceData().Wait = ECombatAIRoleWait::NewAssignment;
	auto& Retry = Select.AddChildState(TEXT("有界故障退避"));
	Retry.AddEnterCondition<FCombatAIRoleCondition>().GetInstanceData().Fact = ECombatAIRoleFact::RetryPending;
	Retry.AddTask<FCombatAIRoleWaitTask>().GetInstanceData().Wait = ECombatAIRoleWait::Retry;
	// 分支优先级只由资产顺序定义，服务不计算或保存当前 Guard/Combat/Return 行为枚举。
	const auto AddBranch = [&](FName BranchName, UStateTree* Asset, ECombatAIRoleFact Fact) -> UStateTreeState&
	{
		auto& Branch = Select.AddChildState(BranchName, EStateTreeStateType::LinkedAsset);
		Branch.SetLinkedStateAsset(Asset);
		Branch.AddEnterCondition<FCombatAIRoleCondition>().GetInstanceData().Fact = Fact;
		Branch.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Select);
		return Branch;
	};
	AddBranch(TEXT("优先归位"), Home, ECombatAIRoleFact::NeedReturn);
	AddBranch(TEXT("已知目标交战"), Engage, ECombatAIRoleFact::CanEngage);
	if (Duty) AddBranch(TEXT("路线职责"), Duty, ECombatAIRoleFact::HasRoute);
	auto& Idle = Select.AddChildState(TEXT("守点观察"), EStateTreeStateType::LinkedAsset);
	Idle.SetLinkedStateAsset(Guard);
	Idle.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Select);
	Blocked.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Select);
	Retry.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Select);
	Initial.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Select);
	auto& Fallback = Root.AddChildState(TEXT("异常完成等待新职责"));
	Fallback.AddTask<FCombatAIRoleWaitTask>().GetInstanceData().Wait = ECombatAIRoleWait::NewAssignment;
	Fallback.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Select);
	Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Fallback);
	return CombatAIAssetBuilder::Compile(Tree);
}

UStateTree* FCombatAIAssetBuilder::BuildTacticalRootTree(UObject* Outer, FName Name)
{
	auto* Tree = NewObject<UStateTree>(Outer, Name);
	return CombatAIAssetBuilder::PopulateTacticalRootTree(*Tree);
}

bool FCombatAIAssetBuilder::RebuildTacticalRootTree(UStateTree* Tree)
{
	return Tree && CombatAIAssetBuilder::PopulateTacticalRootTree(*Tree) != nullptr;
}

UEnvQuery* FCombatAIAssetBuilder::BuildTacticalLocationQuery(UObject* Outer, const float TargetDistance, FName Name)
{
	if (!Outer || !FMath::IsFinite(TargetDistance) || TargetDistance <= 0.0f) return nullptr;
	auto* Query = NewObject<UEnvQuery>(Outer, Name);
	Query->bStripFromClientBuilds = true;
	auto* Option = NewObject<UEnvQueryOption>(Query);
	auto* Generator = NewObject<UCombatAITacticalLocationGenerator>(Option);
	Generator->Distance = TargetDistance;
	Option->Generator = Generator;
	Query->GetOptionsMutable().Add(Option);
	return Query;
}

bool FCombatAIAssetBuilder::ValidateRootTree(const UStateTree* Tree, FString& Diagnostic)
{
	const auto* Data = Tree ? Cast<UStateTreeEditorData>(Tree->EditorData) : nullptr;
	if (!Data || !Data->Schema || !Data->Schema->IsA<UCombatAIStateTreeSchema>() || !Data->GlobalTasks.IsEmpty() || !Data->Evaluators.IsEmpty())
	{ Diagnostic = TEXT("AI root requires Combat schema and no global writers/evaluators in stage A"); return false; }
	TSet<const UStateTree*> Stack;
	Stack.Add(Tree);
	for (const auto& Root : Data->SubTrees) if (Root && !CombatAIAssetBuilder::ValidateState(*Root, 0, 0, Stack, Diagnostic)) return false;
	return true;
}
#endif

UCombatAIAssetsCommandlet::UCombatAIAssetsCommandlet() { IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true; }

int32 UCombatAIAssetsCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	if (FParse::Param(*Params, TEXT("Tactics")))
	{
		const FString Folder = TEXT("/Game/Combat/Demo/AI/Tactics/");
		const FString TreePackage = Folder + TEXT("ST_AI_Tactical");
		auto* Tree = LoadObject<UStateTree>(nullptr, *(TreePackage + TEXT(".ST_AI_Tactical")));
		if (!Tree)
		{
			Tree = FCombatAIAssetBuilder::BuildTacticalRootTree(CreatePackage(*TreePackage), TEXT("ST_AI_Tactical"));
			if (!CombatAIAssetBuilder::Save(Tree)) return 1;
		}
		else if (!FCombatAIAssetBuilder::RebuildTacticalRootTree(Tree) || !CombatAIAssetBuilder::SaveExisting(Tree))
		{
			return 1;
		}
		const FString QueryPackage = Folder + TEXT("EQS_AI_TargetOutside");
		auto* Query = LoadObject<UEnvQuery>(nullptr, *(QueryPackage + TEXT(".EQS_AI_TargetOutside")));
		if (!Query)
		{
			Query = FCombatAIAssetBuilder::BuildTacticalLocationQuery(
				CreatePackage(*QueryPackage), 600.0f, TEXT("EQS_AI_TargetOutside"));
			if (!CombatAIAssetBuilder::Save(Query)) return 1;
		}

		const auto Profile = [&](const TCHAR* AssetName, const TCHAR* Identity, const bool bPositioning)
		{
			const FString Package = Folder + AssetName;
			auto* Result = LoadObject<UCombatAIProfileData>(nullptr, *(Package + TEXT(".") + AssetName));
			if (!Result)
			{
				Result = NewObject<UCombatAIProfileData>(CreatePackage(*Package), FName(AssetName));
				Result->DefinitionName = FName(Identity);
				Result->RootTree = Tree;
				Result->AIProfileVersion = 2;
				Result->bEnableTactics = true;
				Result->bEnablePerception = true;
				Result->Perception.Radius = 900.0f;
				Result->Perception.ActiveInterval = 0.2f;
				Result->Perception.IdleInterval = 0.8f;
				Result->bReturnAfterCombat = false;
				if (bPositioning)
				{
					Result->TacticalLocationQuery = Query;
					Result->RepositionTriggerDistance = 350.0f;
					Result->RepositionUtility = 0.8f;
				}
				if (!CombatAIAssetBuilder::Save(Result)) return static_cast<UCombatAIProfileData*>(nullptr);
			}
			return Result;
		};
		auto* Hero = Profile(TEXT("DA_AI_HeroTactics"), TEXT("ai_hero_tactics"), false);
		auto* Ranged = Profile(TEXT("DA_AI_RangedGuard"), TEXT("ai_ranged_guard"), true);
		if (!Hero || !Ranged) return 1;
		// 首批命令行资产在 Hero AbilitySet 完成前生成；只为空规则补一次，保留后续作者调整。
		if (Hero->AbilityUsageRules.IsEmpty())
		{
			FCombatAIAbilityUsageRule Rule;
			Rule.AbilityDefinitionId = FPrimaryAssetId(TEXT("CombatAbility"), TEXT("indicator_heal"));
			Rule.IntentRole = ECombatAIAbilityIntentRole::Heal;
			Rule.TargetPolicy = ECombatAIAbilityTargetPolicy::Self;
			Rule.BaseUtility = 0.15f;
			Rule.InterruptPreference = ECombatAIInterruptPreference::AttackBoundary;
			Hero->AbilityUsageRules.Add(Rule);
			if (!CombatAIAssetBuilder::SaveExisting(Hero)) return 1;
		}
		FString HeroDiagnostic, RangedDiagnostic;
		const bool bValid = Hero->ValidateRuntime(HeroDiagnostic) && Ranged->ValidateRuntime(RangedDiagnostic);
		UE_LOG(LogTemp, Display, TEXT("AITacticsAssetsReadback Result=%s Hero=%s Ranged=%s Tree=%s Query=%s HeroDetail=%s RangedDetail=%s"),
			bValid ? TEXT("Pass") : TEXT("Fail"), *Hero->GetPathName(), *Ranged->GetPathName(), *Tree->GetPathName(),
			*Query->GetPathName(), *HeroDiagnostic, *RangedDiagnostic);
		return bValid ? 0 : 1;
	}
	if (FParse::Param(*Params, TEXT("Roles")))
	{
		const FString Roles = TEXT("/Game/Combat/Demo/AI/Roles/");
		const auto Tree = [&](const TCHAR* Name, auto Build) -> UStateTree*
		{
			const FString Package = Roles + Name;
			if (auto* Existing = LoadObject<UStateTree>(nullptr, *(Package + TEXT(".") + Name))) return Existing;
			auto* Result = Build(CreatePackage(*Package), FName(Name));
			return CombatAIAssetBuilder::Save(Result) ? Result : nullptr;
		};
		auto* Home = Tree(TEXT("ST_AI_ReturnHome"), [](UObject* Outer, FName Name) { return FCombatAIAssetBuilder::BuildRoleActionTree(Outer, ECombatAIRoleOperation::Home, TEXT("HomeMove"), Name); });
		auto* Engage = Tree(TEXT("ST_AI_Engage"), [](UObject* Outer, FName Name) { return FCombatAIAssetBuilder::BuildRoleActionTree(Outer, ECombatAIRoleOperation::Attack, TEXT("BasicAttack"), Name); });
		auto* Lane = Tree(TEXT("ST_AI_LaneAdvance"), [](UObject* Outer, FName Name) { return FCombatAIAssetBuilder::BuildRoleActionTree(Outer, ECombatAIRoleOperation::Route, TEXT("RouteMove"), Name); });
		auto* Guard = Tree(TEXT("ST_AI_Guard"), [](UObject* Outer, FName Name) { return FCombatAIAssetBuilder::BuildGuardTree(Outer, Name); });
		auto* Patrol = Tree(TEXT("ST_AI_Patrol"), [Lane](UObject* Outer, FName Name)
		{
			auto* Result = NewObject<UStateTree>(Outer, Name);
			auto& Root = CombatAIAssetBuilder::Initialize(*Result)->AddRootState();
			auto& Reuse = Root.AddChildState(TEXT("共享航点推进"), EStateTreeStateType::LinkedAsset);
			Reuse.SetLinkedStateAsset(Lane);
			Reuse.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);
			return CombatAIAssetBuilder::Compile(Result);
		});
		if (!Home || !Engage || !Lane || !Guard || !Patrol) return 1;
		const TCHAR* ProfileNames[] = { TEXT("DA_AI_NeutralCamp"), TEXT("DA_AI_Lane"), TEXT("DA_AI_Patrol") };
		const TCHAR* TreeNames[] = { TEXT("ST_AI_NeutralCamp"), TEXT("ST_AI_Lane"), TEXT("ST_AI_PatrolRoot") };
		const TCHAR* Ids[] = { TEXT("ai_neutral_camp"), TEXT("ai_lane"), TEXT("ai_patrol") };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			auto* Root = Tree(TreeNames[Index], [&](UObject* Outer, FName Name) { return FCombatAIAssetBuilder::BuildRoleRootTree(Outer, Home, Engage, Index == 0 ? nullptr : Index == 1 ? Lane : Patrol, Guard, Name); });
			if (!Root) return 1;
			const FString Package = Roles + ProfileNames[Index];
			auto* Profile = LoadObject<UCombatAIProfileData>(nullptr, *(Package + TEXT(".") + ProfileNames[Index]));
			if (!Profile)
			{
				Profile = NewObject<UCombatAIProfileData>(CreatePackage(*Package), FName(ProfileNames[Index]));
				Profile->DefinitionName = FName(Ids[Index]);
				Profile->RootTree = Root;
				Profile->bEnablePerception = true;
				Profile->bReturnAfterCombat = Index == 0;
				if (Index > 0) Profile->LeashDistance = 3000;
				if (!CombatAIAssetBuilder::Save(Profile)) return 1;
			}
			FString Diagnostic;
			if (!Profile->ValidateRuntime(Diagnostic)) { UE_LOG(LogTemp, Error, TEXT("AIRoleAssetsInvalid %s"), *Diagnostic); return 1; }
			UE_LOG(LogTemp, Display, TEXT("AIRoleAssetsReadback Profile=%s Tree=%s Result=Pass"), *Profile->GetPrimaryAssetId().ToString(), *Root->GetPathName());
		}
		return 0;
	}
	const FString Folder = TEXT("/Game/Combat/Demo/AI/");
	const FString ActionPath = Folder + TEXT("ST_CombatAI_Action");
	const FString RootPath = Folder + TEXT("ST_CombatAI_Root");
	const FString ProfilePath = Folder + TEXT("DA_CombatAI_Basic");
	// 首次生成后通过编辑器继续编排；命令行重复运行只校验，避免覆盖作者后续修改。
	if (auto* Existing = LoadObject<UCombatAIProfileData>(nullptr, *(ProfilePath + TEXT(".DA_CombatAI_Basic"))))
	{
		FString Diagnostic;
		const bool bValid = Existing->ValidateRuntime(Diagnostic);
		UE_LOG(LogTemp, Display, TEXT("AIAssetsReadback Result=%s Profile=%s Detail=%s"), bValid ? TEXT("Pass") : TEXT("Fail"), *Existing->GetPathName(), *Diagnostic);
		return bValid ? 0 : 1;
	}
	auto* Action = FCombatAIAssetBuilder::BuildActionTree(CreatePackage(*ActionPath), TEXT("ST_CombatAI_Action"));
	if (!CombatAIAssetBuilder::Save(Action)) return 1;
	auto* Root = FCombatAIAssetBuilder::BuildRootTree(CreatePackage(*RootPath), Action, TEXT("ST_CombatAI_Root"));
	if (!CombatAIAssetBuilder::Save(Root)) return 1;
	auto* Profile = NewObject<UCombatAIProfileData>(CreatePackage(*ProfilePath), TEXT("DA_CombatAI_Basic"));
	Profile->DefinitionName = TEXT("ai_basic");
	Profile->RootTree = Root;
	if (!CombatAIAssetBuilder::Save(Profile)) return 1;
	FString Diagnostic;
	const bool bValid = Profile->ValidateRuntime(Diagnostic);
	UE_LOG(LogTemp, Display, TEXT("AIAssetsSaved Result=%s Profile=%s"), bValid ? TEXT("Pass") : TEXT("Fail"), *Profile->GetPrimaryAssetId().ToString());
	return bValid ? 0 : 1;
#else
	return 1;
#endif
}
