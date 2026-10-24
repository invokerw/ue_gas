#include "Combat/Defense/DefenseRecall.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Motion/CombatMotionComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "CombatPlayerController.h"
#include "Engine/World.h"

bool UDefenseDirectorComponent::CanRecall(const ACombatUnitCharacter* Hero) const
{
    const auto* PC = IsValid(Hero) ? Cast<ACombatPlayerController>(Hero->GetCommandingPlayerController()) : nullptr;
    const auto* Record = PC ? Players.Find(PC) : nullptr;
    return GetOwner() && GetOwner()->HasAuthority() && GetNetMode()!=NM_Client && !bEnding && !Terminal.bTerminal
        && RunData && IsCoreAlive() && Record && Record->Hero.Get()==Hero && PC->GetCommandedUnit()==Hero
        && Hero->GetResourceOwnerPlayerController()==PC && !Hero->IsActorBeingDestroyed()
        && Hero->GetLifeState()==ECombatLifeState::Alive && !Hero->GetCombatMotionComponent()->HasActiveMotion();
}
bool UDefenseDirectorComponent::ResolveRecallDestination(const ACombatUnitCharacter* Hero, FVector& OutDestination) const
{
    return CanRecall(Hero) && FindSafeLocation(*Hero, RunData->RespawnLocation, OutDestination);
}
UDefenseDirectorComponent* UDefenseRecallAbility::ResolveDirector() const
{
    const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ADefenseGameMode>() : nullptr;
    return Mode ? Mode->GetDefenseDirector() : nullptr;
}
bool UDefenseRecallAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    const auto* Hero = ActorInfo ? Cast<ACombatUnitCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    const auto* Director = Hero && Hero->GetWorld()->GetAuthGameMode<ADefenseGameMode>()
        ? Hero->GetWorld()->GetAuthGameMode<ADefenseGameMode>()->GetDefenseDirector() : nullptr;
    if (!Director || !Director->CanRecall(Hero))
    { if (OptionalRelevantTags) OptionalRelevantTags->AddTag(CombatTags::Failure_ActionUnsupported); return false; }
    return Super::CanActivateAbility(Handle,ActorInfo,SourceTags,TargetTags,OptionalRelevantTags);
}
float UDefenseRecallAbility::ResolveChannelDurationForActivation() const
{
    const auto* Hero = GetCombatContext().Caster.Get();
    const float Reduction = Hero ? Hero->GetCombatModifierComponent()->GetActiveRuntimeParameterSum(TEXT("defense_recall_reduction_seconds")) : 0;
    return FMath::Max(GetSpecialValue(TEXT("minimum_channel")), Super::ResolveChannelDurationForActivation()-FMath::Max(0.f,Reduction));
}
void UDefenseRecallAbility::ReceiveSpellStart_Implementation(const FCombatAbilityActivationContext& Context)
{
    UnbindObservers();
    auto* Hero = Context.Caster.Get(); auto* D = ResolveDirector();
    if (!Hero || !D || !D->CanRecall(Hero)) { CancelRecall(); return; }
    RecallPlayer = Cast<ACombatPlayerController>(Hero->GetCommandingPlayerController());
    BindingGeneration = RecallPlayer->GetCommandBindingGeneration(); RunId = D->GetRunId();
    StartLocation = Hero->GetActorLocation(); bWatching = true;
    ObservedEvents = GetWorld()->GetSubsystem<UCombatEventSubsystem>(); ObservedRoot = Hero->GetRootComponent();
    DamageHandle = ObservedEvents->OnRecord().AddUObject(this,&ThisClass::OnCombatRecord);
    TransformHandle = ObservedRoot->TransformUpdated.AddUObject(this,&ThisClass::OnTransformUpdated);
}
bool UDefenseRecallAbility::ValidateChannelCompletion(FGameplayTag& OutFailure) const
{
    OutFailure = CombatTags::Failure_ActionUnsupported;
    const auto* Hero = GetCombatContext().Caster.Get(); const auto* D = ResolveDirector();
    const bool Valid = bWatching && IsValid(Hero) && RecallPlayer.IsValid() && D && D->CanRecall(Hero)
        && D->GetRunId()==RunId && Hero->GetLifeGeneration()==GetCombatContext().CasterLifeGeneration
        && Hero->GetCommandingPlayerController()==RecallPlayer.Get() && RecallPlayer->GetCommandBindingGeneration()==BindingGeneration
        && !Hero->GetCombatAbilitySystemComponent()->IsCombatAbilityStateBlocked(GetCurrentAbilitySpecHandle())
        && Hero->GetActorLocation().Equals(StartLocation, GetSpecialValue(TEXT("movement_tolerance")));
    if (Valid) OutFailure = {};
    return Valid;
}
void UDefenseRecallAbility::ReceiveChannelTick_Implementation(const FCombatAbilityActivationContext& Context, const FCombatScheduledTickContext& Tick)
{
    FGameplayTag Failure; if (!ValidateChannelCompletion(Failure)) CancelRecall();
}
void UDefenseRecallAbility::ReceiveChannelFinish_Implementation(const FCombatAbilityActivationContext& Context, const bool bInterrupted)
{
    if (bInterrupted) { UnbindObservers(); return; }
    FVector Destination; auto* D = ResolveDirector(); auto* Hero = Context.Caster.Get();
    const bool Safe = D && D->ResolveRecallDestination(Hero,Destination);
    if (!Safe) { UnbindObservers(); CancelRecall(); return; }
    // 提交前保留扣血观察，先解除位置观察避免自身传送中断；所有开始日志/观察者返回后，由Motion再次调用完整业务复核。
    if (ObservedRoot.IsValid()) { ObservedRoot->TransformUpdated.Remove(TransformHandle); TransformHandle.Reset(); }
    const bool Teleported = Hero && Hero->GetCombatMotionComponent()->TryTeleport(Destination,Context.EventContext,Context.SourceContext,RecallPlayer.Get(),
        [this]() { FGameplayTag Failure; return IsActive() && ValidateChannelCompletion(Failure); }).bSuccess;
    UnbindObservers();
    if (!Teleported) { CancelRecall(); return; }
    UE_LOG(LogCombat,Display,TEXT("DefenseRecallCompleted Hero=%s Life=%u Destination=%s"),*Hero->GetName(),Hero->GetLifeGeneration(),*Destination.ToCompactString());
}
void UDefenseRecallAbility::OnCombatRecord(const FCombatLogRecord& Record)
{
    const auto* Hero = GetCombatContext().Caster.Get();
    if (!bWatching || !Hero) return;
    if (Record.EventType==CombatTags::Event_Combat_DamageApplied
        && Record.TargetActorId==Hero->GetUniqueID() && Record.AppliedAmount>0) { CancelRecall(); return; }

}
void UDefenseRecallAbility::OnTransformUpdated(USceneComponent* Component, EUpdateTransformFlags Flags, ETeleportType Teleport)
{
    if (bWatching && Component && !Component->GetComponentLocation().Equals(StartLocation,GetSpecialValue(TEXT("movement_tolerance")))) CancelRecall();
}
void UDefenseRecallAbility::CancelRecall()
{
    if (IsActive()) EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
}
void UDefenseRecallAbility::UnbindObservers()
{
    bWatching = false;
    if (ObservedEvents.IsValid()) ObservedEvents->OnRecord().Remove(DamageHandle);
    if (ObservedRoot.IsValid()) ObservedRoot->TransformUpdated.Remove(TransformHandle);
    ObservedEvents.Reset(); ObservedRoot.Reset(); DamageHandle.Reset(); TransformHandle.Reset();
}
void UDefenseRecallAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
    UnbindObservers();
    Super::EndAbility(Handle,ActorInfo,ActivationInfo,bReplicateEndAbility,bWasCancelled);
}
