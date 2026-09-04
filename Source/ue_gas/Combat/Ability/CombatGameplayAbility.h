#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"

#include "Combat/Ability/CombatAbilityTypes.h"
#include "Combat/Core/CombatTypes.h"

#include "CombatGameplayAbility.generated.h"

class ACombatUnitCharacter;
class UAbilityTask_WaitCombatInterval;
class UCombatAbilityData;

/**
 * 服务器上执行技能的基类，负责初始检查、施法前摇、执行点复核、引导以及结束清理。
 * 每次激活保存等级、目标及生命编号和事件身份；设计数据通过只读资产引用访问，不复制整份资产。
 * 按配置阶段提交法力和冷却，先调用技能扩展事件再执行数据动作；生成的弹体可独立继续飞行。
 * 技能结束时清理前摇、引导任务和施法显示，仅按显式选项取消绑定弹体；每次激活只释放一次施法命令。
 */
UCLASS(Blueprintable)
class UE_GAS_API UCombatGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UCombatGameplayAbility();

	/** 返回 Class CDO 单向引用的稳定 AbilityData。 */
	const UCombatAbilityData* GetAbilityData() const { return AbilityData; }
	/** 按本次技能使用的等级读取定义中的具名数值，例如伤害或半径；不存在的键使用默认值。 */
	UFUNCTION(BlueprintPure, Category="Combat|Ability", meta=(DisplayName="获取技能特殊值", ToolTip="按本次技能使用的等级读取定义中的具名数值，例如伤害或半径；不存在的键使用默认值。"))
	float GetSpecialValue(UPARAM(DisplayName="特殊值键") FName Key) const;
	/** 返回当前激活的只读服务器快照。 */
	UFUNCTION(BlueprintPure, Category="Combat|Ability", meta=(DisplayName="获取战斗技能上下文", ToolTip="返回当前激活冻结的服务器权威上下文。"))
	const FCombatAbilityActivationContext& GetCombatContext() const { return CombatContext; }
	/** 读取本次法术执行点的数据动作结果；异步动作成功表示弹体或区域已创建，不表示之后一定命中。 */
	UFUNCTION(BlueprintPure, Category="Combat|Ability", meta=(DisplayName="获取最近技能动作结果", ToolTip="读取本次法术执行点的数据动作结果；异步动作成功表示弹体或区域已创建，不表示之后一定命中。"))
	const FCombatAbilityActionResult& GetLastActionResult() const { return LastActionResult; }
	/** 返回 Cast/Channel 是否仍持有活动 Scheduler 任务，供生命周期测试。 */
	bool HasActiveCombatSchedule() const;
	/** 技能授予单位后，从技能类默认对象取得同一份设计资产，确保单位各自的技能实例使用正确配置。 */
	virtual void OnGiveAbility(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec) override;

	/** 检查单位状态、服务器暂存目标、法力及冷却，提供拒绝原因；此阶段不扣资源、不发射弹体。 */
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	/** 取出本次目标请求，记录等级、生命编号与根事件，提交施法开始阶段费用，并安排前摇；零前摇可同步进入执行点。 */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	/** 统一结束清理入口，取消前摇和引导计时、解绑任务、清理施法显示；中断时通知引导结束并释放命令，显式绑定的弹体按配置取消。 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/**
	 * 前摇结束、目标及阶段提交通过且未被法术格挡后调用，供蓝图或派生类实现技能行为。
	 * 调用发生在配置的公共数据动作之前，扩展实现应使用公共战斗入口。
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Ability", meta=(DisplayName="技能正式开始", ToolTip="前摇结束、目标及阶段提交通过且未被法术格挡后调用，供蓝图或派生类实现技能行为。 调用发生在配置的公共数据动作之前，扩展实现应使用公共战斗入口。"))
	void ReceiveSpellStart(UPARAM(DisplayName="技能上下文") const FCombatAbilityActivationContext& Context);
	/** 每次引导周期到点且目标复核通过后调用；计划时刻达到引导终点的周期不执行。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Ability", meta=(DisplayName="技能引导周期", ToolTip="每次引导周期到点且目标复核通过后调用；计划时刻达到引导终点的周期不执行。"))
	void ReceiveChannelTick(
		UPARAM(DisplayName="技能上下文") const FCombatAbilityActivationContext& Context,
		UPARAM(DisplayName="周期上下文") const FCombatScheduledTickContext& TickContext);
	/** 引导已经开始后，正常到期或被中断时调用一次；bInterrupted 区分两种情况，前摇期间取消不会产生此通知。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Ability", meta=(DisplayName="技能引导结束", ToolTip="引导已经开始后，正常到期或被中断时调用一次；bInterrupted 区分两种情况，前摇期间取消不会产生此通知。"))
	void ReceiveChannelFinish(
		UPARAM(DisplayName="技能上下文") const FCombatAbilityActivationContext& Context,
		UPARAM(DisplayName="是否中断") bool bInterrupted);

	/** Ability Class CDO 单向绑定的配置资产；DataAsset 不反向引用 Class。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability", meta=(DisplayName="技能数据", ToolTip="Ability Class CDO 单向绑定的配置资产；DataAsset 不反向引用 Ability Class。"))
	TObjectPtr<UCombatAbilityData> AbilityData;

protected:
	/** 按配置顺序执行伤害、治疗、效果、事件或创建弹体/区域；首个失败结束执行并返回原因，之前已经产生的效果不回滚。 */
	FCombatAbilityActionResult ExecuteDataDrivenActions();

	/** C++ 默认蓝图事件为空。 */
	virtual void ReceiveSpellStart_Implementation(const FCombatAbilityActivationContext& Context);
	virtual void ReceiveChannelTick_Implementation(const FCombatAbilityActivationContext& Context, const FCombatScheduledTickContext& TickContext);
	virtual void ReceiveChannelFinish_Implementation(const FCombatAbilityActivationContext& Context, bool bInterrupted);

private:
	/** 前摇到点后复核目标，提交生效阶段费用并检查法术格挡，再调用技能事件和配置动作；非引导技能随后释放命令并结束。 */
	void HandleCastPoint(const FCombatScheduledTickContext& TickContext);
	/** 收到引导周期后重新检查施法者和目标，合法才调用周期扩展事件，失败则中断技能。 */
	UFUNCTION() void HandleChannelTick(FCombatScheduledTickContext TickContext);
	/** 引导正常到期时通知引导结束并释放命令，再尝试提交正常结束阶段费用，进入统一技能清理。 */
	UFUNCTION() void HandleChannelFinished();
	/** 复核施法者仍存活于本次生命及目标是否合法；成功时更新目标位置，允许失去目标时保留最近一次有效位置供点/范围动作使用。 */
	bool RevalidateAtExecutionPoint(FGameplayTag& OutFailureTag);
	/** 把当前阶段交给能力系统组件提交费用和冷却，使用本次激活的已提交标志避免重复扣费或重开冷却。 */
	bool CommitStage(ECombatAbilityCommitStage Stage, FGameplayTag& OutFailureTag);
	/** 尝试提交正常结束阶段的费用和冷却，成功后正常结束，失败则中断；施法命令由前面的调用路径释放，此函数不再次释放。 */
	void FinishSuccessfully();
	/** 保存失败原因并记录中断事件，直接以取消标记调用统一 EndAbility 清理入口。 */
	void InterruptAbility(const FGameplayTag& FailureTag, const FString& Diagnostic);
	/** 本次激活首次释放命令时记录日志并同步通知能力系统组件；后续调用直接返回，避免推进两次命令队列。 */
	void ReleaseCombatOrder(bool bSuccess, const FGameplayTag& FailureTag);
	/** 记录本次激活的阶段事件；同一事件标签仅发送一次，日志关联本次根事件、技能定义和施法者身份。 */
	void EmitLifecycleEvent(const FGameplayTag& EventType, const FGameplayTag& FailureTag, const FString& Diagnostic);
	/** 返回当前激活的 Combat ASC。 */
	class UCombatAbilitySystemComponent* GetCombatAsc() const;

	/** 本次激活的服务器权威上下文。 */
	UPROPERTY(Transient) FCombatAbilityActivationContext CombatContext;
	/** 激活时的原始目标请求，执行点和引导周期用它重新校验；不接受客户端提供的命中集合。 */
	UPROPERTY(Transient) FCombatAbilityTargetData ActivationTargetData;
	/** DataDriven Actions 的最近聚合结果。 */
	UPROPERTY(Transient) FCombatAbilityActionResult LastActionResult;
	/** 等待施法前摇结束的单次任务；技能提前结束时取消。 */
	FCombatScheduleHandle CastPointSchedule;
	/** 负责引导周期与到期的技能任务；结束时解绑通知并销毁。 */
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
	/** 保存本次中断原因，供 EndAbility 释放 Order 时返回稳定 FailureTag。 */
	FGameplayTag LastInterruptFailureTag;
};
