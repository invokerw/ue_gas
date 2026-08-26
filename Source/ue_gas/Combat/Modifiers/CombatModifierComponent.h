#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"

#include "Combat/Combat/CombatTransactionTypes.h"
#include "Combat/Core/CombatDeferredOperationQueue.h"
#include "Combat/Data/CombatDefinitionData.h"

#include "CombatModifierComponent.generated.h"

class ACombatUnitCharacter;
class UCombatModifierRuntime;
class UGameplayEffect;

/** 表示本次 Dispel 请求允许移除的最高 Modifier 规则。 */
UENUM(BlueprintType)
enum class ECombatDispelStrength : uint8
{
	/** 只移除 DispelRule=Basic。 */
	Basic,
	/** 移除 Basic 与 StrongOnly。 */
	Strong
};

/** 服务器施加 Modifier 时使用的统一请求。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatModifierApplyRequest
{
	GENERATED_BODY()

	/** 提供 Runtime、ActiveGE 和周期规则的定义。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier") TObjectPtr<UCombatModifierData> ModifierData = nullptr;
	/** 施加 Modifier 的来源单位。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier") TObjectPtr<ACombatUnitCharacter> Source = nullptr;
	/** 小于 0 使用定义 Duration；0 表示本次无限；正数覆盖定义持续时间。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier") float DurationOverride = -1.0f;
	/** Intrinsic Modifier 使用的 AbilitySpec owner key；普通 Modifier 保持无效。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier") FGameplayAbilitySpecHandle AbilityOwnerHandle;
};

/** ApplyModifier 返回的成功状态、句柄与刷新信息。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatModifierApplyResult
{
	GENERATED_BODY()

	/** ActiveGE 与 Runtime 是否成功创建或刷新。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Modifier") bool bSuccess = false;
	/** true 表示复用了现有的一一映射。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Modifier") bool bRefreshed = false;
	/** 新建或刷新 Runtime 的稳定句柄。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Modifier") FCombatModifierHandle Handle;
	/** 失败时的机器可判定原因。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Modifier") FGameplayTag FailureTag;
};

/** 管理 Unit 的 ActiveGE/Runtime 一一映射、稳定 Hook、周期、刷新和驱散。 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatModifierComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 默认关闭 Tick；所有周期由 Combat Scheduler 驱动。 */
	UCombatModifierComponent();

	/** 在 Authority 上施加或刷新 Modifier。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Modifier")
	FCombatModifierApplyResult ApplyModifier(const FCombatModifierApplyRequest& Request);
	/** 按句柄移除一对 ActiveGE/Runtime；Hook 内调用会延迟到阶段结束。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Modifier")
	bool RemoveModifier(FCombatModifierHandle Handle);
	/** 对稳定快照执行 Basic 或 Strong Dispel，并返回移除数量。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Modifier")
	int32 Dispel(ECombatDispelStrength Strength, bool bDebuffsOnly = true);
	/** 返回当前 ActiveGE/Runtime 一一映射数量。 */
	UFUNCTION(BlueprintPure, Category="Combat|Modifier")
	int32 GetActiveModifierCount() const { return ActiveModifiers.Num(); }
	/** 按句柄查找只读 Runtime。 */
	UCombatModifierRuntime* FindRuntime(FCombatModifierHandle Handle) const;

	/** 依稳定快照执行来源伤害前置 Hook。 */
	void ExecutePreDealDamage(FCombatDamageEvent& Event);
	/** 依稳定快照执行目标伤害前置 Hook。 */
	void ExecutePreTakeDamage(FCombatDamageEvent& Event);
	/** 依稳定快照执行目标 Shield/Block Hook。 */
	void ExecuteDamageBlock(FCombatDamageEvent& Event);
	/** 依稳定快照执行来源伤害后置 Hook。 */
	void ExecutePostDealDamage(const FCombatDamageEvent& Event);
	/** 依稳定快照执行目标伤害后置 Hook。 */
	void ExecutePostTakeDamage(const FCombatDamageEvent& Event);
	/** 依稳定快照执行来源治疗前置 Hook。 */
	void ExecutePreDealHeal(FCombatHealEvent& Event);
	/** 依稳定快照执行目标治疗前置 Hook。 */
	void ExecutePreTakeHeal(FCombatHealEvent& Event);
	/** 依稳定快照执行来源治疗后置 Hook。 */
	void ExecutePostDealHeal(const FCombatHealEvent& Event);
	/** 依稳定快照执行目标治疗后置 Hook。 */
	void ExecutePostTakeHeal(const FCombatHealEvent& Event);
	/** 技能进入 SpellStarted 后向来源 Unit 的 Modifier 稳定派发一次。 */
	void ExecuteAbilityExecuted(const FPrimaryAssetId& AbilityDefinitionId, const FCombatEventContext& Context);

	/** 生命周期进入 Dying 时移除死亡清理 Modifier，并暂停保留 Runtime 的 Hook/Think。 */
	void HandleOwnerDeath();
	/** 生命周期重新 Alive 时恢复未过期保留 Runtime 的调度。 */
	void HandleOwnerRespawn();
	/** Actor EndPlay 时无 Hook 地清理全部 ActiveGE 与调度。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 创建动态无限 ActiveGE 并建立 Runtime 一一映射。 */
	FCombatModifierApplyResult ApplyNewModifier(const FCombatModifierApplyRequest& Request, float EffectiveDuration);
	/** 刷新现有 Runtime 的层数、过期时间和周期相位。 */
	FCombatModifierApplyResult RefreshModifier(UCombatModifierRuntime& Runtime, float EffectiveDuration);
	/** 立即移除一一映射；调用者负责确保不在 Hook 遍历中。 */
	bool RemoveModifierImmediate(FCombatModifierHandle Handle, bool bCallDestroyed = true);
	/** 返回同来源、同定义且仍活动的 Runtime。 */
	UCombatModifierRuntime* FindRefreshCandidate(const FCombatModifierApplyRequest& Request) const;
	/** 按状态抗性计算本次持续时间快照。 */
	float CalculateEffectiveDuration(const FCombatModifierApplyRequest& Request) const;
	/** 建立或重排 Runtime 的 Think 与 Expire Scheduler 任务。 */
	void ScheduleRuntime(UCombatModifierRuntime& Runtime, bool bResetThinkPhase);
	/** 取消 Runtime 持有的全部 Scheduler 句柄。 */
	void CancelRuntimeSchedules(UCombatModifierRuntime& Runtime);
	/** Scheduler 到期时派发 Runtime Think，并执行边界规则。 */
	void HandleRuntimeThink(FCombatModifierHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** Scheduler 到达 ExpireAt 时自然移除 Runtime。 */
	void HandleRuntimeExpired(FCombatModifierHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** 返回 Priority desc、ApplySequence asc 的活动快照。 */
	TArray<UCombatModifierRuntime*> MakeSortedSnapshot() const;
	/** 返回组件所属 Combat Unit。 */
	ACombatUnitCharacter* GetOwnerUnit() const;
	/** 为 Apply/Refresh/Remove 写入包含 DefinitionId、层数与 Handle 的结构化日志。 */
	void EmitModifierLog(const UCombatModifierRuntime& Runtime, bool bRemoved, bool bRefreshed = false) const;

	/** 强引用全部活动 Runtime，直到延迟阶段完整提交。 */
	UPROPERTY(Transient) TArray<TObjectPtr<UCombatModifierRuntime>> ActiveModifiers;
	/** 保存动态 GE 定义，确保 ActiveGameplayEffectSpec 生命周期内不会被 GC。 */
	UPROPERTY(Transient) TArray<TObjectPtr<UGameplayEffect>> RuntimeEffectDefinitions;
	/** Hook 中的 Apply/Remove/Refresh/Purge FIFO 延迟队列。 */
	FCombatDeferredOperationQueue DeferredOperations;
	/** 下一个 Modifier 句柄 ID。 */
	uint64 NextHandleId = 1;
	/** 下一个 Hook 稳定施加序号。 */
	uint64 NextApplySequence = 1;
	/** 非 Alive 阶段禁止 Hook 与 Think。 */
	bool bHooksPaused = false;
};
