#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"

#include "Combat/Ability/CombatAbilityTypes.h"
#include "Combat/Targeting/CombatTargetingTypes.h"

#include "CombatAbilitySystemComponent.generated.h"

/** 技能允许当前施法命令结束时的服务器同步通知；表示可以处理后续命令，不表示冷却或已发射弹体也结束。 */
DECLARE_MULTICAST_DELEGATE_FourParams(
	FOnCombatAbilityOrderReleased,
	FGameplayAbilitySpecHandle,
	bool,
	FGameplayTag,
	ECombatChannelInterruptOrderPolicy);

/**
 * 单位持有的 GAS 扩展组件，管理已授予技能的记录（AbilitySpec）、等级、自动施法开关、法力费用与冷却。
 * 服务器暂存一次激活所需的目标请求，供技能校验并消费；客户端提交的目标始终重新验证。
 * 它还维护 GAS 的所有者/化身绑定（ActorInfo），以及技能授予期间应常驻的固有效果（Intrinsic Modifier），并转发施法命令释放通知。
 * 技能移除时清理对应效果和冷却；单位退出场景时由生命周期入口清除绑定。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UCombatAbilitySystemComponent();

	/** 设置 GAS 的所有者与实际施法 Actor；同一对对象不重复初始化，但仍检查并补齐技能应有的固有效果，支持复活后恢复。 */
	void InitializeCombatActorInfo(AActor* InOwnerActor, AActor* InAvatarActor);
	/** 清空 ActorInfo，供失去控制权或 EndPlay 时停止后续 GAS 操作。 */
	void ClearCombatActorInfo();
	/** 返回 ActorInfo 是否同时具有有效 Owner 与 Avatar。 */
	bool IsCombatActorInfoInitialized() const;
	/** UnitData 初次授予 Ability 后记录服务器权威 AutoCast 初始状态。 */
	bool SetInitialAutoCastState(FGameplayAbilitySpecHandle Handle, bool bEnabled);
	/** 返回指定 AbilitySpec 当前记录的 AutoCast 状态。 */
	bool IsAutoCastEnabled(FGameplayAbilitySpecHandle Handle) const;

	/** 服务器读取技能类默认对象绑定的定义，按定义 ID 授予唯一技能记录；校验等级、重复定义及固有效果，失败写出原因。 */
	bool GrantCombatAbility(
		TSubclassOf<class UCombatGameplayAbility> AbilityClass,
		int32 InitialLevel,
		bool bInitialAutoCast,
		FGameplayAbilitySpecHandle& OutHandle,
		FGameplayTag& OutFailureTag);
	/** 服务器修改已授予技能的等级并同步固有效果；越界或未授予时失败，不改变等级。 */
	bool SetCombatAbilityLevel(FGameplayAbilitySpecHandle Handle, int32 NewLevel, FGameplayTag& OutFailureTag);
	/** 移除已授予技能：先取消本次施法，再移除该技能的固有效果、自动施法和冷却记录；不会撤销已独立生效的普通效果。 */
	bool RemoveCombatAbility(FGameplayAbilitySpecHandle Handle, FGameplayTag& OutFailureTag);
	/** 服务器校验存活状态与技能的自动施法行为标签后，修改该技能记录的开关；开启开关本身不会立即执行一次技能。 */
	bool SetAutoCastEnabled(FGameplayAbilitySpecHandle Handle, bool bEnabled, FGameplayTag& OutFailureTag);
	/** 返回指定 DefinitionId 已授予的唯一 Spec；未找到返回空。 */
	FGameplayAbilitySpec* FindCombatAbilitySpecByDefinitionId(const FPrimaryAssetId& DefinitionId);
	/** 取得已授予技能的类默认对象所绑定的只读定义；句柄或技能类型不匹配时返回空。 */
	const class UCombatAbilityData* GetCombatAbilityData(FGameplayAbilitySpecHandle Handle) const;

	/** 服务器校验技能、状态与原始目标请求，暂存目标后尝试激活 GAS 技能；返回初始激活是否接受，后续前摇或引导仍可能失败。 */
	bool TryActivateCombatAbility(
		FGameplayAbilitySpecHandle Handle,
		const FCombatAbilityTargetData& TargetData,
		FGameplayTag& OutFailureTag);
	/** 客户端请求激活本单位已授予的技能，只提交技能句柄和原始目标；服务器重新验证目标、状态、法力与冷却。 */
	UFUNCTION(Server, Reliable)
	void ServerTryActivateCombatAbility(FGameplayAbilitySpecHandle Handle, FCombatAbilityTargetData TargetData);
	/** 客户端请求切换本单位技能的自动施法开关；RPC 先验证网络所有权，服务器再验证技能和单位状态。 */
	UFUNCTION(Server, Reliable)
	void ServerSetAutoCastEnabled(FGameplayAbilitySpecHandle Handle, bool bEnabled);

	/** 读取当前待激活技能的目标请求供条件检查；保留该请求，找不到时返回 false。 */
	bool PeekPendingTargetData(FGameplayAbilitySpecHandle Handle, FCombatAbilityTargetData& OutTargetData) const;
	/** 技能开始激活时取出并删除暂存目标请求；同一份请求只能成功消费一次，后续读取返回 false。 */
	bool ConsumePendingTargetData(FGameplayAbilitySpecHandle Handle, FCombatAbilityTargetData& OutTargetData);
	/** 预检当前法力与该技能的冷却是否满足激活要求，不扣法力也不开始冷却；失败返回原因标签。 */
	bool PreflightCombatAbility(
		FGameplayAbilitySpecHandle Handle,
		const UCombatAbilityData& AbilityData,
		int32 AbilityLevel,
		FGameplayTag& OutFailureTag) const;
	/**
	 * 进入指定技能阶段时，提交配置在该阶段且尚未提交的法力费用和冷却；先整体预检，同阶段失败时避免只提交一半。
	 * 由调用方保存两个已提交标志，确保本次激活各执行一次；已在更早阶段提交的费用和冷却不因后续失败自动撤销。
	 */
	bool CommitCombatAbilityStage(
		FGameplayAbilitySpecHandle Handle,
		const UCombatAbilityData& AbilityData,
		int32 AbilityLevel,
		ECombatAbilityCommitStage Stage,
		bool& bCostCommitted,
		bool& bCooldownCommitted,
		FGameplayTag& OutFailureTag);
	/** 返回冻结 cooldown 剩余秒数；到期或未开始为 0。 */
	float GetCombatAbilityCooldownRemaining(FGameplayAbilitySpecHandle Handle) const;
	/** 检查每个已授予技能应有的固有效果；已有有效实例保持原状，缺失时在单位存活后补建，不重复叠层或续期。 */
	void ReconcileIntrinsicModifiers();
	/** 状态 Tag 新增时取消被该状态阻断的活动 Ability，并保留 IgnoreSilence 例外。 */
	void CancelCombatAbilitiesBlockedByStatus(FGameplayTag StatusTag);
	/** 转发技能释放施法命令的结果和队列处理策略；每次激活只通知一次的保护由技能调用方负责。 */
	void NotifyCombatAbilityOrderReleased(
		FGameplayAbilitySpecHandle Handle,
		bool bSuccess,
		FGameplayTag FailureTag,
		ECombatChannelInterruptOrderPolicy InterruptPolicy);
	/** 订阅技能释放施法命令的本地同步通知，供指令组件继续或清空后续队列。 */
	FOnCombatAbilityOrderReleased& OnCombatAbilityOrderReleased() { return AbilityOrderReleasedDelegate; }

protected:
	/** ASC Tag count 变化的统一入口，驱动状态响应并在新增阻断状态时取消 Ability。 */
	virtual void OnTagUpdated(const FGameplayTag& Tag, bool bTagExists) override;

private:
	/** 返回 GAS 当前化身对应的 Combat Unit；仅做类型转换，不在此函数中检查服务器权限。 */
	class ACombatUnitCharacter* GetCombatAvatar() const;
	/** 为授予、等级、移除和 AutoCast 输出统一结构化日志。 */
	void EmitAbilitySpecLog(FGameplayAbilitySpecHandle Handle, const FGameplayTag& EventType, const FString& Diagnostic) const;
	/** 确保指定已授予技能具有其固有效果；有效实例保持不变，无定义时无需创建，单位未存活时暂缓补建。 */
	bool ReconcileIntrinsicModifier(FGameplayAbilitySpecHandle Handle);
	/** 按提交时计算出的秒数记录冷却终点，并建立持续 GameplayEffect；时长为 0 表示立即到期，非法时长或应用失败返回 false。 */
	bool ApplyCombatCooldown(FGameplayAbilitySpecHandle Handle, float Duration);

	/** 按已授予技能句柄保存的服务器自动施法开关；这是技能行为状态，不表示技能正在施放。 */
	TMap<FGameplayAbilitySpecHandle, bool> AutoCastStates;
	/** 暂存激活入口与技能实例之间的一次性目标请求，技能激活消费后删除，不能用作长期目标状态。 */
	TMap<FGameplayAbilitySpecHandle, FCombatAbilityTargetData> PendingTargetData;
	/** 各技能已提交冷却的世界游戏时间终点，单位为秒；冷却缩减在提交时计算，之后属性变化不重算此终点。 */
	TMap<FGameplayAbilitySpecHandle, double> CooldownEndTimes;
	/** Duration Cooldown GE 句柄，死亡时保留，移除 Spec 时清理。 */
	TMap<FGameplayAbilitySpecHandle, FActiveGameplayEffectHandle> CooldownEffectHandles;
	/** 每个已授予技能所维护的固有效果句柄，用技能句柄区分归属，避免不同技能的效果互相合并。 */
	TMap<FGameplayAbilitySpecHandle, FCombatModifierHandle> IntrinsicModifierHandles;
	/** 每个 Spec 只强引用最近一次动态 Duration GE 定义，避免历史 cooldown 定义累积。 */
	UPROPERTY(Transient) TMap<FGameplayAbilitySpecHandle, TObjectPtr<class UGameplayEffect>> CooldownEffectDefinitions;
	/** 当前 ASC 上全部 Combat Ability 共用的 Order 释放观察者。 */
	FOnCombatAbilityOrderReleased AbilityOrderReleasedDelegate;
};
