#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"

#include "Combat/Ability/CombatAbilityTypes.h"
#include "Combat/Targeting/CombatTargetingTypes.h"

#include "CombatAbilitySystemComponent.generated.h"

/** Ability 释放其 Order 占用时向 OrderComponent 广播的同步结果。 */
DECLARE_MULTICAST_DELEGATE_FourParams(
	FOnCombatAbilityOrderReleased,
	FGameplayAbilitySpecHandle,
	bool,
	FGameplayTag,
	ECombatChannelInterruptOrderPolicy);

/**
 * Unit 自持的 GAS 扩展组件，负责 Combat AbilitySpec 的授予、等级、自动施法、消耗和冷却状态。
 * 它在服务器保存激活所需的短生命周期 TargetData，并统一维护 ActorInfo、Intrinsic Modifier 与 Order 释放契约；客户端 RPC 只表达请求，最终激活条件仍由服务器复核。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UCombatAbilitySystemComponent();

	/** 使用明确的 Owner/Avatar 初始化 ActorInfo，并避免重复初始化同一对 Actor。 */
	void InitializeCombatActorInfo(AActor* InOwnerActor, AActor* InAvatarActor);
	/** 清空 ActorInfo，供失去控制权或 EndPlay 时停止后续 GAS 操作。 */
	void ClearCombatActorInfo();
	/** 返回 ActorInfo 是否同时具有有效 Owner 与 Avatar。 */
	bool IsCombatActorInfoInitialized() const;
	/** UnitData 初次授予 Ability 后记录服务器权威 AutoCast 初始状态。 */
	bool SetInitialAutoCastState(FGameplayAbilitySpecHandle Handle, bool bEnabled);
	/** 返回指定 AbilitySpec 当前记录的 AutoCast 状态。 */
	bool IsAutoCastEnabled(FGameplayAbilitySpecHandle Handle) const;

	/** 服务器按 Class CDO 的 AbilityData 身份授予一个唯一 Combat AbilitySpec。 */
	bool GrantCombatAbility(
		TSubclassOf<class UCombatGameplayAbility> AbilityClass,
		int32 InitialLevel,
		bool bInitialAutoCast,
		FGameplayAbilitySpecHandle& OutHandle,
		FGameplayTag& OutFailureTag);
	/** 服务器原子修改已授予 Spec.Level；越界或未授予时不改动。 */
	bool SetCombatAbilityLevel(FGameplayAbilitySpecHandle Handle, int32 NewLevel, FGameplayTag& OutFailureTag);
	/** 取消活动实例、清理 Intrinsic/AutoCast/Cooldown 后移除 Spec。 */
	bool RemoveCombatAbility(FGameplayAbilitySpecHandle Handle, FGameplayTag& OutFailureTag);
	/** 服务器验证行为标签和生命状态后切换 per-Spec AutoCast。 */
	bool SetAutoCastEnabled(FGameplayAbilitySpecHandle Handle, bool bEnabled, FGameplayTag& OutFailureTag);
	/** 返回指定 DefinitionId 已授予的唯一 Spec；未找到返回空。 */
	FGameplayAbilitySpec* FindCombatAbilitySpecByDefinitionId(const FPrimaryAssetId& DefinitionId);
	/** 返回 Spec Class CDO 单向引用的 AbilityData。 */
	const class UCombatAbilityData* GetCombatAbilityData(FGameplayAbilitySpecHandle Handle) const;

	/** 服务器入口：缓存最小 TargetData 后调用 GAS TryActivateAbility。 */
	bool TryActivateCombatAbility(
		FGameplayAbilitySpecHandle Handle,
		const FCombatAbilityTargetData& TargetData,
		FGameplayTag& OutFailureTag);
	/** Client/Order 只能请求本 ASC 的 Spec 与原始 TargetData，服务器完整复核。 */
	UFUNCTION(Server, Reliable)
	void ServerTryActivateCombatAbility(FGameplayAbilitySpecHandle Handle, FCombatAbilityTargetData TargetData);
	/** Client/Order 请求切换 AutoCast；组件 ownership 由 UE RPC 层先验证。 */
	UFUNCTION(Server, Reliable)
	void ServerSetAutoCastEnabled(FGameplayAbilitySpecHandle Handle, bool bEnabled);

	/** Ability CanActivate 读取但不消费当前服务器待激活 TargetData。 */
	bool PeekPendingTargetData(FGameplayAbilitySpecHandle Handle, FCombatAbilityTargetData& OutTargetData) const;
	/** Ability Activate 时 exactly-once 消费待激活 TargetData。 */
	bool ConsumePendingTargetData(FGameplayAbilitySpecHandle Handle, FCombatAbilityTargetData& OutTargetData);
	/** 检查当前 Mana 与该 Spec 的冻结 cooldown 是否允许激活。 */
	bool PreflightCombatAbility(
		FGameplayAbilitySpecHandle Handle,
		const UCombatAbilityData& AbilityData,
		int32 AbilityLevel,
		FGameplayTag& OutFailureTag) const;
	/** 按 Stage 对尚未提交的 Cost/Cooldown 整体预检并最多各应用一次。 */
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
	/** 幂等恢复全部已授予 Ability 的 Intrinsic Modifier。 */
	void ReconcileIntrinsicModifiers();
	/** 状态 Tag 新增时取消被该状态阻断的活动 Ability，并保留 IgnoreSilence 例外。 */
	void CancelCombatAbilitiesBlockedByStatus(FGameplayTag StatusTag);
	/** Ability 正常释放或中断时 exactly-once 通知当前 OrderComponent。 */
	void NotifyCombatAbilityOrderReleased(
		FGameplayAbilitySpecHandle Handle,
		bool bSuccess,
		FGameplayTag FailureTag,
		ECombatChannelInterruptOrderPolicy InterruptPolicy);
	/** 返回 Ability OrderReleased 同步委托。 */
	FOnCombatAbilityOrderReleased& OnCombatAbilityOrderReleased() { return AbilityOrderReleasedDelegate; }

protected:
	/** ASC Tag count 变化的统一入口，驱动状态响应并在新增阻断状态时取消 Ability。 */
	virtual void OnTagUpdated(const FGameplayTag& Tag, bool bTagExists) override;

private:
	/** 返回 Owner/Avatar 对应的 Authority Combat Unit。 */
	class ACombatUnitCharacter* GetCombatAvatar() const;
	/** 为授予、等级、移除和 AutoCast 输出统一结构化日志。 */
	void EmitAbilitySpecLog(FGameplayAbilitySpecHandle Handle, const FGameplayTag& EventType, const FString& Diagnostic) const;
	/** 对单个 Spec 幂等建立或恢复 Intrinsic Modifier。 */
	bool ReconcileIntrinsicModifier(FGameplayAbilitySpecHandle Handle);
	/** 以 Duration GE 启动提交点快照后的 cooldown。 */
	bool ApplyCombatCooldown(FGameplayAbilitySpecHandle Handle, float Duration);

	/** per-Spec 服务器权威 AutoCast 状态；客户端通过安全 View 读取投影结果。 */
	TMap<FGameplayAbilitySpecHandle, bool> AutoCastStates;
	/** TryActivate 与 Ability 实例之间的同步、一次性 TargetData 槽。 */
	TMap<FGameplayAbilitySpecHandle, FCombatAbilityTargetData> PendingTargetData;
	/** 已开始 cooldown 的绝对结束时间。 */
	TMap<FGameplayAbilitySpecHandle, double> CooldownEndTimes;
	/** Duration Cooldown GE 句柄，死亡时保留，移除 Spec 时清理。 */
	TMap<FGameplayAbilitySpecHandle, FActiveGameplayEffectHandle> CooldownEffectHandles;
	/** Intrinsic Modifier 以 AbilitySpecHandle 为 owner key 的活动句柄。 */
	TMap<FGameplayAbilitySpecHandle, FCombatModifierHandle> IntrinsicModifierHandles;
	/** 每个 Spec 只强引用最近一次动态 Duration GE 定义，避免历史 cooldown 定义累积。 */
	UPROPERTY(Transient) TMap<FGameplayAbilitySpecHandle, TObjectPtr<class UGameplayEffect>> CooldownEffectDefinitions;
	/** 当前 ASC 上全部 Combat Ability 共用的 Order 释放观察者。 */
	FOnCombatAbilityOrderReleased AbilityOrderReleasedDelegate;
};
