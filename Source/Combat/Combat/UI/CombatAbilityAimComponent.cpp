#include "Combat/UI/CombatAbilityAimComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Misc/CoreDelegates.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/UI/CombatAbilityIndicatorActor.h"
#include "Combat/UI/CombatAbilityIndicatorGround.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "CombatPlayerController.h"

namespace CombatAbilityAim
{
	/** 目标失败使用稳定规则标签映射玩家文字，不展示内部枚举。 */
	FText TargetFailure(const FGameplayTag& Tag)
	{
		if (Tag == CombatTags::Failure_Target_OutOfRange) return FText::FromString(TEXT("超出施法距离 · 确认后尝试追近"));
		if (Tag == CombatTags::Failure_Target_LineOfSightBlocked) return FText::FromString(TEXT("目标被遮挡"));
		if (Tag == CombatTags::Failure_Target_FriendlyNotAllowed) return FText::FromString(TEXT("需要敌方目标"));
		if (Tag == CombatTags::Failure_Target_HostileNotAllowed) return FText::FromString(TEXT("需要友方目标"));
		if (Tag == CombatTags::Failure_Target_SelfNotAllowed) return FText::FromString(TEXT("不能对自己施放"));
		if (Tag == CombatTags::Failure_Target_MagicImmune) return FText::FromString(TEXT("目标魔法免疫"));
		if (Tag == CombatTags::Failure_Target_Invulnerable) return FText::FromString(TEXT("目标无敌"));
		if (Tag == CombatTags::Failure_Target_Dead || Tag == CombatTags::Failure_Target_Dying
			|| Tag == CombatTags::Failure_Target_Respawning) return FText::FromString(TEXT("目标已阵亡"));
		return FText::FromString(TEXT("目标不可选中"));
	}
	/** 地面锚点只影响贴花投影；目标请求仍使用原始世界坐标。 */
	FVector GroundAnchor(const ACombatUnitCharacter& Unit)
	{
		return Unit.GetActorLocation() - FVector(0, 0, Unit.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}
}

UCombatAbilityAimComponent::UCombatAbilityAimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(false);
}

ACombatPlayerController* UCombatAbilityAimComponent::GetCombatController() const
{
	return Cast<ACombatPlayerController>(GetOwner());
}

const FGameplayAbilitySpec* UCombatAbilityAimComponent::ResolveSlot(ACombatUnitCharacter* Unit, const int32 Slot)
{
	const UCombatAbilitySystemComponent* Asc = Unit ? Unit->GetCombatAbilitySystemComponent() : nullptr;
	if (!Asc || Slot < 0 || Slot >= 4) return nullptr;
	int32 Index = 0;
	for (const FGameplayAbilitySpec& Spec : Asc->GetActivatableAbilities())
	{
		const UCombatGameplayAbility* Ability = Cast<UCombatGameplayAbility>(Spec.Ability);
		const UCombatAbilityData* Data = Ability ? Ability->GetAbilityData() : nullptr;
		if (!Data || !Data->ShouldOccupyPlayerAbilitySlot()) continue;
		if (Index++ == Slot) return &Spec;
	}
	return nullptr;
}

void UCombatAbilityAimComponent::BeginPlay()
{
	Super::BeginPlay();
	// BeginPlay 可能早于 SetPlayer；不能因当时尚无 LocalPlayer 永久关闭后来拥有者的 Tick。
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}
	DeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &UCombatAbilityAimComponent::ResetLocalState);
}

void UCombatAbilityAimComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(DeactivateHandle);
	DeactivateHandle.Reset();
	ResetLocalState();
	if (IsValid(IndicatorActor)) IndicatorActor->Destroy();
	IndicatorActor = nullptr;
	Super::EndPlay(Reason);
}

bool UCombatAbilityAimComponent::BeginAim(const int32 Slot)
{
	CancelAim();
	ClearReceipt();
	ACombatPlayerController* PC = GetCombatController();
	ACombatUnitCharacter* Unit = PC ? PC->GetCommandedUnit() : nullptr;
	const FGameplayAbilitySpec* Spec = ResolveSlot(Unit, Slot);
	if (!PC || !PC->IsLocalController() || GetNetMode() == NM_DedicatedServer || !IsValid(Unit)
		|| Unit->GetCommandingPlayerController() != PC || Unit->GetLifeState() != ECombatLifeState::Alive || !Spec) return false;
	ActiveSlot = Slot;
	ActiveHandle = Spec->Handle;
	SessionUnit = Unit;
	BindingGeneration = PC->GetCommandBindingGeneration();
	LifeGeneration = Unit->GetLifeGeneration();
	PC->CurrentMouseCursor = EMouseCursor::Crosshairs;
	return true;
}

void UCombatAbilityAimComponent::CancelAim()
{
	++SessionSerial;
	if (SessionSerial == 0) ++SessionSerial;
	ActiveSlot = INDEX_NONE;
	ActiveHandle = {};
	SessionUnit.Reset();
	Preview = {};
	if (ACombatPlayerController* PC = GetCombatController()) PC->CurrentMouseCursor = PC->DefaultMouseCursor;
	if (IsValid(IndicatorActor)) IndicatorActor->SetActorHiddenInGame(true);
}

void UCombatAbilityAimComponent::ResetLocalState()
{
	CancelAim();
	HoveredSlot = INDEX_NONE;
	ClearReceipt();
}

bool UCombatAbilityAimComponent::IsSessionCurrent() const
{
	const ACombatPlayerController* PC = GetCombatController();
	const ACombatUnitCharacter* Unit = SessionUnit.Get();
	if (!IsAiming() || !PC || !IsValid(Unit) || PC->GetCommandedUnit() != Unit
		|| Unit->GetCommandingPlayerController() != PC || PC->GetCommandBindingGeneration() != BindingGeneration
		|| Unit->GetLifeGeneration() != LifeGeneration || Unit->GetLifeState() != ECombatLifeState::Alive) return false;
	const FGameplayAbilitySpec* Spec = ResolveSlot(SessionUnit.Get(), ActiveSlot);
	return Spec && Spec->Handle == ActiveHandle;
}

bool UCombatAbilityAimComponent::TraceAimHit(FHitResult& OutHit) const
{
	OutHit = {};
	if (!IsSessionCurrent()) return false;
	const UCombatAbilityData* Data = SessionUnit->GetCombatAbilitySystemComponent()->GetCombatAbilityData(ActiveHandle);
	ACombatPlayerController* PC = GetCombatController();
	if (!Data || !PC || !PC->IsLocalController()) return false;
	const bool bPointTarget = Data->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_PointTarget);
	const ECollisionChannel Channel = bPointTarget ? CombatAbilityIndicatorGround::TraceChannel : ECC_Visibility;
	if (!PC->GetHitResultUnderCursor(Channel, true, OutHit)
		|| (bPointTarget && !CombatAbilityIndicatorGround::IsGroundHit(OutHit)))
	{
		OutHit = {};
		return false;
	}
	return true;
}

void UCombatAbilityAimComponent::UpdatePreview(const FHitResult& Hit, const bool bWorldInputAllowed)
{
	if (IsAiming() && !IsSessionCurrent()) ResetLocalState();
	Preview = {};
	ACombatPlayerController* PC = GetCombatController();
	ACombatUnitCharacter* Unit = PC ? PC->GetCommandedUnit() : nullptr;
	if (!PC || !IsValid(Unit) || Unit->GetCommandingPlayerController() != PC || Unit->GetLifeState() != ECombatLifeState::Alive) return;
	const FGameplayAbilitySpec* Spec = ResolveSlot(Unit, IsAiming() ? ActiveSlot : HoveredSlot);
	if (!Spec) return;
	const UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	const UCombatAbilityData* Data = Asc->GetCombatAbilityData(Spec->Handle);
	const UCombatUnitViewComponent* View = Unit->GetCombatUnitViewComponent();
	if (!Data || !View) return;
	Preview.bAiming = IsAiming();
	Preview.Message = FText::FromString(TEXT("技能数据同步中"));
	const FCombatHUDOwnerView Owner = View->GetHUDOwnerView();
	const FCombatUnitView& Public = View->GetUnitView();
	const FCombatHUDAbilityView* Ability = Owner.Abilities.FindByPredicate(
		[Spec](const FCombatHUDAbilityView& Item) { return Item.SpecHandle == Spec->Handle; });
	// Unit 的初始化缓存只存在于服务器；客户端身份须核对同代的两份复制快照。
	if (!Ability || Owner.LifeGeneration != Unit->GetLifeGeneration() || Public.LifeGeneration != Owner.LifeGeneration
		|| !Owner.UnitDefinitionId.IsValid() || Owner.UnitDefinitionId != Public.UnitDefinitionId
		|| Ability->DefinitionId != Data->GetPrimaryAssetId()
		|| Ability->Level != Spec->Level) return;
	// ASC 与 owner View 分别复制；不同步时暂缓显示，保证共享 Targeting 与可见距离来自同一份数值。
	if (!FMath::IsNearlyEqual(Owner.CastRangeBonus, Asc->GetNumericAttribute(UCombatAttributeSet::GetCastRangeBonusAttribute()), 0.01f)
		|| !FMath::IsFinite(Owner.AttackRange) || !FMath::IsFinite(Owner.CastRangeBonus)) return;
	if (!Data->ResolveIndicatorGeometry(Spec->Level, Preview.Geometry)) return;
	const bool bAutoCast = Data->UsesAutoCastToggleInput();
	const bool bUnitTarget = Data->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_UnitTarget);
	const bool bPointTarget = Data->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_PointTarget);
	Preview.bVisible = true;
	Preview.Status = ECombatAbilityAimStatus::Ready;
	Preview.CasterLocation = CombatAbilityAim::GroundAnchor(*Unit);
	Preview.CastRadius = (bAutoCast ? Owner.AttackRange : (bUnitTarget || bPointTarget ? Data->TargetingRules.CastRange + Owner.CastRangeBonus : 0.0f));
	if (bAutoCast || bUnitTarget || bPointTarget) Preview.CastRadius = FMath::Max(0.0f, Preview.CastRadius + Unit->GetCapsuleComponent()->GetScaledCapsuleRadius());
	Preview.TargetLocation = Preview.CasterLocation;
	Preview.Direction = Unit->GetActorForwardVector().GetSafeNormal2D();
	Preview.PlanarLineLength = Preview.Geometry.Length;
	Preview.Message = FText::FromString(bAutoCast ? TEXT("自动施法 · 普通攻击距离") : TEXT("选择目标 · 左键确认 / 右键取消"));
	if (!bAutoCast && PC->GetAbilityCastMode() == ECombatAbilityCastMode::QuickRelease)
		Preview.Message = FText::FromString(TEXT("松开技能键确认 · 右键取消"));
	if (bAutoCast) return;
	FString Blocked;
	if (Ability->Level < 1) Blocked = TEXT("技能尚未学习");
	else if (Public.VisibleStatusTags.HasTagExact(CombatTags::State_Stunned)
		|| Public.VisibleStatusTags.HasTagExact(CombatTags::State_Hexed)
		|| Public.VisibleStatusTags.HasTagExact(CombatTags::State_Frozen)
		|| Public.VisibleStatusTags.HasTagExact(CombatTags::State_OutOfGame)) Blocked = TEXT("当前状态无法施法");
	else if (Public.VisibleStatusTags.HasTagExact(CombatTags::State_Silenced) && !Ability->bIgnoreSilence) Blocked = TEXT("已被沉默");
	else if (Ability->CooldownEndTime > View->GetEstimatedServerTimeSeconds()) Blocked = TEXT("技能冷却中");
	else if (Public.Mana < Ability->ManaCost) Blocked = TEXT("法力不足");
	if (!Blocked.IsEmpty())
	{
		Preview.Status = ECombatAbilityAimStatus::Blocked;
		Preview.Message = FText::FromString(Blocked);
	}
	if (!IsAiming())
	{
		// 悬停只看范围；指向型 AoE 不在 UI 下臆造世界落点，自身形状可直接显示。
		Preview.bHasTarget = Preview.Geometry.bCenterOnCaster && Preview.Geometry.Shape == ECombatIndicatorShape::Circle;
		return;
	}
	FCombatAbilityTargetData Target;
	const bool bFiniteHit = Hit.bBlockingHit && !Hit.Location.ContainsNaN();
	const bool bGroundHit = bPointTarget && CombatAbilityIndicatorGround::IsGroundHit(Hit);
	ACombatUnitCharacter* TargetUnit = bFiniteHit ? Cast<ACombatUnitCharacter>(Hit.GetActor()) : nullptr;
	if (bUnitTarget) Target.TargetActor = TargetUnit;
	else if (bGroundHit) { Target.TargetLocation = Hit.Location; Target.bHasTargetLocation = true; }
	Preview.bHasTarget = bWorldInputAllowed && (bUnitTarget ? IsValid(TargetUnit) : bPointTarget ? bGroundHit : true);
	if (Preview.bHasTarget)
	{
		Preview.TargetLocation = bUnitTarget ? CombatAbilityAim::GroundAnchor(*TargetUnit) : bPointTarget ? FVector(Hit.Location) : Preview.CasterLocation;
		Preview.TargetRadius = bUnitTarget ? TargetUnit->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0f;
		const FVector Direction = (Preview.TargetLocation - Preview.CasterLocation).GetSafeNormal2D();
		if (!Direction.IsNearlyZero()) Preview.Direction = Direction;
		if (Preview.Geometry.Shape == ECombatIndicatorShape::Line)
		{
			const FVector AimLocation = bUnitTarget ? TargetUnit->GetActorLocation() : bPointTarget ? FVector(Hit.Location) : Unit->GetActorLocation();
			Preview.PlanarLineLength = Preview.Geometry.GetPlanarLineLength(Unit->GetActorLocation(), AimLocation, Unit->GetActorForwardVector());
		}
	}
	if (!Blocked.IsEmpty()) return;
	if (!bWorldInputAllowed || !Preview.bHasTarget)
	{
		Preview.Status = ECombatAbilityAimStatus::InvalidTarget;
		Preview.Message = FText::FromString(!bWorldInputAllowed ? TEXT("移出界面后选择目标") : bUnitTarget ? TEXT("请选择一个单位") : TEXT("请选择地面位置"));
		return;
	}
	UCombatTargetingSubsystem* Targeting = GetWorld()->GetSubsystem<UCombatTargetingSubsystem>();
	if (!Targeting) { Preview.Status = ECombatAbilityAimStatus::Unavailable; Preview.bVisible = false; return; }
	const FCombatTargetValidationResult Result = Targeting->ValidateAbilityTarget(Unit, Data->BehaviorTags, Data->TargetingRules, Target);
	if (!Result.bValid)
	{
		Preview.Status = Result.FailureTag == CombatTags::Failure_Target_OutOfRange ? ECombatAbilityAimStatus::OutOfRange : ECombatAbilityAimStatus::InvalidTarget;
		Preview.Message = CombatAbilityAim::TargetFailure(Result.FailureTag);
	}
}

bool UCombatAbilityAimComponent::BuildConfirmedOrder(const uint64 Serial, const FHitResult& Hit, const bool bWorldInputAllowed, FCombatOrderRequest& OutOrder)
{
	OutOrder = {};
	if (Serial != SessionSerial || !IsSessionCurrent()) return false;
	UpdatePreview(Hit, bWorldInputAllowed);
	if (!IsAiming() || !Preview.bVisible || !Preview.bHasTarget
		|| (Preview.Status != ECombatAbilityAimStatus::Ready && Preview.Status != ECombatAbilityAimStatus::OutOfRange)) return false;
	const UCombatAbilityData* Data = SessionUnit->GetCombatAbilitySystemComponent()->GetCombatAbilityData(ActiveHandle);
	if (!Data) return false;
	OutOrder.AbilitySpecHandle = ActiveHandle;
	if (Data->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_UnitTarget))
	{
		OutOrder.Type = ECombatOrderType::CastTarget;
		OutOrder.TargetUnit = Cast<ACombatUnitCharacter>(Hit.GetActor());
	}
	else if (Data->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_PointTarget))
	{
		OutOrder.Type = ECombatOrderType::CastPoint;
		OutOrder.TargetLocation = Hit.Location;
		OutOrder.bHasTargetLocation = true;
	}
	else OutOrder.Type = ECombatOrderType::CastNoTarget;
	return true;
}

void UCombatAbilityAimComponent::ClearReceipt()
{
	if (ReceiptUnit.IsValid()) ReceiptUnit->OnOrderBatchResult.RemoveDynamic(this, &UCombatAbilityAimComponent::HandleOrderResult);
	ReceiptUnit.Reset();
	PendingRequestId = 0;
	ReceiptRemaining = 0.0f;
	ReceiptText = FText::GetEmpty();
}

void UCombatAbilityAimComponent::MarkSubmitted(const int32 RequestId)
{
	ClearReceipt();
	ReceiptUnit = SessionUnit;
	ReceiptBinding = BindingGeneration;
	ReceiptLife = LifeGeneration;
	PendingRequestId = RequestId;
	ReceiptText = FText::FromString(TEXT("指令已发送"));
	ReceiptRemaining = 1.5f;
	if (ReceiptUnit.IsValid()) ReceiptUnit->OnOrderBatchResult.AddUniqueDynamic(this, &UCombatAbilityAimComponent::HandleOrderResult);
	CancelAim();
}

void UCombatAbilityAimComponent::FinishQuickCastAttempt()
{
	if (!IsSessionCurrent()) { ResetLocalState(); return; }
	ClearReceipt();
	ReceiptUnit = SessionUnit;
	ReceiptBinding = BindingGeneration;
	ReceiptLife = LifeGeneration;
	ReceiptText = Preview.Message;
	ReceiptRemaining = 1.5f;
	CancelAim();
}

void UCombatAbilityAimComponent::HandleOrderResult(FCombatOrderBatchResult Result)
{
	const ACombatPlayerController* PC = GetCombatController();
	if (!PendingRequestId || Result.RequestId != PendingRequestId) return;
	if (!PC || !ReceiptUnit.IsValid() || PC->GetCommandedUnit() != ReceiptUnit.Get()
		|| ReceiptUnit->GetCommandingPlayerController() != PC || PC->GetCommandBindingGeneration() != ReceiptBinding
		|| ReceiptUnit->GetLifeGeneration() != ReceiptLife) { ClearReceipt(); return; }
	ReceiptText = FText::FromString(Result.bAccepted && Result.AcceptedOrderCount > 0 ? TEXT("指令已接收") : TEXT("施法指令被拒绝"));
	ReceiptRemaining = 1.5f;
	PendingRequestId = 0;
	ReceiptUnit->OnOrderBatchResult.RemoveDynamic(this, &UCombatAbilityAimComponent::HandleOrderResult);
}

FText UCombatAbilityAimComponent::GetStatusText() const
{
	return IsAiming() ? Preview.Message : ReceiptRemaining > 0.0f ? ReceiptText : FText::GetEmpty();
}

void UCombatAbilityAimComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* Function)
{
	Super::TickComponent(DeltaTime, TickType, Function);
	ACombatPlayerController* PC = GetCombatController();
	if (!PC || !PC->IsLocalController() || GetNetMode() == NM_DedicatedServer) return;
	if (ReceiptRemaining > 0.0f)
	{
		ReceiptRemaining -= DeltaTime;
		if (ReceiptRemaining <= 0.0f || !ReceiptUnit.IsValid() || PC->GetCommandedUnit() != ReceiptUnit.Get()
			|| PC->GetCommandBindingGeneration() != ReceiptBinding || ReceiptUnit->GetLifeGeneration() != ReceiptLife
			|| ReceiptUnit->GetLifeState() != ECombatLifeState::Alive) ClearReceipt();
	}
	FHitResult Hit;
	if (IsAiming()) TraceAimHit(Hit);
	UpdatePreview(Hit, !PC->IsPointerOverCombatUI());
	RenderPreview();
}

void UCombatAbilityAimComponent::RenderPreview()
{
	ACombatPlayerController* PC = GetCombatController();
	if (!PC || !PC->GetLocalPlayer() || GetNetMode() == NM_DedicatedServer) return;
	if (Preview.bVisible && !IsValid(IndicatorActor))
	{
		FActorSpawnParameters Params;
		Params.Owner = PC;
		Params.ObjectFlags |= RF_Transient;
		IndicatorActor = GetWorld()->SpawnActor<ACombatAbilityIndicatorActor>(Params);
	}
	if (IsValid(IndicatorActor)) IndicatorActor->ShowPreview(Preview);
}
