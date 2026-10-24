#pragma once
#include "CoreMinimal.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Components/SceneComponent.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "DefenseRecall.generated.h"
class UDefenseDirectorComponent;
class ACombatPlayerController;

/** 防守回城：沿原GAS任务/冷却/Order执行，观察真实扣血和位置变化，完成时只请求Motion安全传送。
 * 每次激活绑定精确Run/英雄生命/控制代次；所有结束入口先解绑两个观察者，没有自己的计时器或复制字段。
 */
UCLASS(Blueprintable, meta=(DisplayName="防守基础回城", ToolTip="零蓝耗，按当前装备冻结引导时间；真实扣血或移动中断，冷却仍由原ASC持有。"))
class COMBAT_API UDefenseRecallAbility : public UCombatGameplayAbility
{
    GENERATED_BODY()
public:
    virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
        FGameplayTagContainer* OptionalRelevantTags=nullptr) const override;
    virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
protected:
    virtual float ResolveChannelDurationForActivation() const override;
    virtual bool ValidateChannelCompletion(FGameplayTag& OutFailure) const override;
    virtual void ReceiveSpellStart_Implementation(const FCombatAbilityActivationContext& Context) override;
    virtual void ReceiveChannelTick_Implementation(const FCombatAbilityActivationContext& Context, const FCombatScheduledTickContext& Tick) override;
    virtual void ReceiveChannelFinish_Implementation(const FCombatAbilityActivationContext& Context, bool bInterrupted) override;
private:
    /** 查本World权威模式；非防守客户端没有服务器导演。 */
    UDefenseDirectorComponent* ResolveDirector() const;
    /** 当前观察者只取消自己的激活，通过原EndAbility清理任务。 */
    void CancelRecall();
    /** 同一真实伤害事件只据目标和AppliedAmount判断，不把请求值或护盾吸收当受伤。 */
    void OnCombatRecord(const FCombatLogRecord& Record);
    /** 位移发生后立即校验原点容差；自身完成传送之前会解绑此观察者。 */
    void OnTransformUpdated(USceneComponent* Component, EUpdateTransformFlags Flags, ETeleportType Teleport);
    /** 正常/取消/死亡/EndPlay共用的幂等解绑，不依赖弱引用自然失效。 */
    void UnbindObservers();
    TWeakObjectPtr<UCombatEventSubsystem> ObservedEvents;
    TWeakObjectPtr<USceneComponent> ObservedRoot;
    TWeakObjectPtr<ACombatPlayerController> RecallPlayer;
    FDelegateHandle DamageHandle, TransformHandle;
    FGuid RunId;
    int32 BindingGeneration = 0;
    FVector StartLocation = FVector::ZeroVector;
    bool bWatching = false;
};
