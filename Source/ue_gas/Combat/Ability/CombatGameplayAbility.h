#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"

#include "Combat/Ability/CombatAbilityTypes.h"
#include "Combat/Core/CombatTypes.h"

#include "CombatGameplayAbility.generated.h"

class ACombatUnitCharacter;
class UAbilityTask_WaitCombatInterval;
class UCombatAbilityData;

/** 服务器权威的 Combat Ability 基类，统一目标、提交、Scheduler 生命周期与清理。 */
UCLASS(Blueprintable)
class UE_GAS_API UCombatGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	/** 默认使用 InstancedPerActor、ServerOnly，实例保存本次激活快照。 */
	UCombatGameplayAbility();

	/** 返回 Class CDO 单向引用的稳定 AbilityData。 */
	const UCombatAbilityData* GetAbilityData() const { return AbilityData; }
	/** 按当前 AbilitySpec.Level 读取 DataAsset special。 */
	UFUNCTION(BlueprintPure, Category="Combat|Ability") float GetSpecialValue(FName Key) const;
	/** 返回当前激活的只读服务器快照。 */
	UFUNCTION(BlueprintPure, Category="Combat|Ability") const FCombatAbilityActivationContext& GetCombatContext() const { return CombatContext; }
	/** 返回本次 SpellStarted 的 DataDriven Action 聚合结果。 */
	UFUNCTION(BlueprintPure, Category="Combat|Ability") const FCombatAbilityActionResult& GetLastActionResult() const { return LastActionResult; }
	/** 返回 Cast/Channel 是否仍持有活动 Scheduler 任务，供生命周期测试。 */
	bool HasActiveCombatSchedule() const;
	/** Spec 授予时把 Class CDO 的唯一 AbilityData 显式同步到 InstancedPerActor 实例。 */
	virtual void OnGiveAbility(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec) override;

	/** 无副作用检查状态、TargetData、Mana 与 cooldown。 */
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	/** 消费服务器 TargetData，建立 ActivationId 并启动 cast point。 */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	/** 所有成功、中断、移除和死亡路径最终在这里取消 Task/Delegate/Schedule。 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/** SpellStarted 时供蓝图扩展复杂行为；公共动作已经由基类统一执行。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Ability")
	void ReceiveSpellStart(const FCombatAbilityActivationContext& Context);
	/** 每个确定性 Channel tick 调用。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Ability")
	void ReceiveChannelTick(const FCombatAbilityActivationContext& Context, const FCombatScheduledTickContext& TickContext);
	/** Channel 正常完成或中断时 exactly-once 调用。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Ability")
	void ReceiveChannelFinish(const FCombatAbilityActivationContext& Context, bool bInterrupted);

	/** Ability Class CDO 单向绑定的配置资产；DataAsset 不反向引用 Class。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability")
	TObjectPtr<UCombatAbilityData> AbilityData;

protected:
	/** 按服务器权威 Target 快照依次执行全部 M3 DataDriven Actions。 */
	FCombatAbilityActionResult ExecuteDataDrivenActions();

	/** C++ 默认蓝图事件为空。 */
	virtual void ReceiveSpellStart_Implementation(const FCombatAbilityActivationContext& Context);
	virtual void ReceiveChannelTick_Implementation(const FCombatAbilityActivationContext& Context, const FCombatScheduledTickContext& TickContext);
	virtual void ReceiveChannelFinish_Implementation(const FCombatAbilityActivationContext& Context, bool bInterrupted);

private:
	/** Scheduler 到达 cast point 后复核目标、提交 SpellStarted 并执行 Action。 */
	void HandleCastPoint(const FCombatScheduledTickContext& TickContext);
	/** AbilityTask 转发一个确定性 Channel tick。 */
	UFUNCTION() void HandleChannelTick(FCombatScheduledTickContext TickContext);
	/** AbilityTask duration 到期后完成 Channel 与 Ability。 */
	UFUNCTION() void HandleChannelFinished();
	/** 重新检查施法者生命代次和当前目标；可选保留最后位置。 */
	bool RevalidateAtExecutionPoint(FGameplayTag& OutFailureTag);
	/** 调用 ASC 分阶段提交并复用本实例幂等标记。 */
	bool CommitStage(ECombatAbilityCommitStage Stage, FGameplayTag& OutFailureTag);
	/** 发出 OrderReleased 并执行 AbilityEnded stage，随后正常结束。 */
	void FinishSuccessfully();
	/** 通过 GAS CancelAbility 进入统一 EndAbility 中断路径。 */
	void InterruptAbility(const FGameplayTag& FailureTag, const FString& Diagnostic);
	/** 写入一个同 ActivationId exactly-once 的生命周期结构化日志。 */
	void EmitLifecycleEvent(const FGameplayTag& EventType, const FGameplayTag& FailureTag, const FString& Diagnostic);
	/** 返回当前激活的 Combat ASC。 */
	class UCombatAbilitySystemComponent* GetCombatAsc() const;

	/** 本次激活的服务器权威上下文。 */
	UPROPERTY(Transient) FCombatAbilityActivationContext CombatContext;
	/** 激活入口提交且 cast point 继续复核的原始最小 TargetData。 */
	UPROPERTY(Transient) FCombatAbilityTargetData ActivationTargetData;
	/** DataDriven Actions 的最近聚合结果。 */
	UPROPERTY(Transient) FCombatAbilityActionResult LastActionResult;
	/** cast point 一次性 Scheduler 句柄。 */
	FCombatScheduleHandle CastPointSchedule;
	/** Channel interval/duration 的受管理 AbilityTask。 */
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitCombatInterval> ChannelTask;
	/** 本 Activation 已写入日志的生命周期标签集合。 */
	TSet<FGameplayTag> EmittedLifecycleEvents;
	/** Cost 每个 Activation 最多提交一次。 */
	bool bCostCommitted = false;
	/** Cooldown 每个 Activation 最多提交一次。 */
	bool bCooldownCommitted = false;
	/** SpellStarted 已发出，用于区分前摇与引导中断。 */
	bool bSpellStarted = false;
	/** Channel Task 已建立。 */
	bool bChannelStarted = false;
	/** ChannelEnded 已发出。 */
	bool bChannelEnded = false;
	/** OrderReleased 已发出。 */
	bool bOrderReleased = false;
	/** 防止 EndAbility 清理过程递归进入。 */
	bool bEnding = false;
};
