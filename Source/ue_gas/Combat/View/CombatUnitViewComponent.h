#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Combat/View/CombatUnitViewTypes.h"

#include "CombatUnitViewComponent.generated.h"

class ACombatUnitCharacter;
struct FOnAttributeChangeData;

/**
 * 将服务器 Unit、Ability 与 Modifier 状态投影为 UI 安全的扁平复制 View。
 * Owner 与非 Owner 接收相同的白名单字段和 FastArray 快照；组件只从权威 gameplay 状态刷新并通知表现层，UI 不能通过 View 反向修改属性或结算。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatUnitViewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatUnitViewComponent();

	/** 返回当前客户端或服务器的单位 View。 */
	UFUNCTION(BlueprintPure, Category="Combat|View", meta=(DisplayName="获取战斗单位 View", ToolTip="返回 UI 使用的服务器权威扁平单位投影。"))
	const FCombatUnitView& GetUnitView() const { return UnitView; }
	/** 返回当前可见 Modifier 快照。 */
	UFUNCTION(BlueprintPure, Category="Combat|View", meta=(DisplayName="获取可见 Modifier", ToolTip="返回 FastArray 当前可见条目的只读副本，不包含 Runtime UObject。"))
	TArray<FCombatModifierView> GetVisibleModifiers() const { return ModifierViews.Items; }
	/** 使用服务器时间计算一个 View 的剩余持续时间；无限持续返回 -1。 */
	UFUNCTION(BlueprintPure, Category="Combat|View", meta=(DisplayName="计算 Modifier 剩余时间", ToolTip="按服务器结束时间计算剩余秒数；无限持续返回 -1。"))
	float GetModifierRemainingTime(UPARAM(DisplayName="Modifier View") const FCombatModifierView& View) const;
	/** 返回客户端校准后的服务器 World Time；无 GameState 时回退本地 World Time。 */
	UFUNCTION(BlueprintPure, Category="Combat|View", meta=(DisplayName="获取估算服务器时间", ToolTip="用于 UI 对齐技能和状态绝对时间窗。"))
	double GetEstimatedServerTimeSeconds() const;

	/** Unit View 字段变化时广播。 */
	UPROPERTY(BlueprintAssignable, Category="Combat|View", meta=(DisplayName="单位 View 已变化", ToolTip="单位身份、生命、属性或技能投影变化时广播。")) FCombatUnitViewChangedDelegate OnUnitViewChanged;
	/** Modifier FastArray 增删改时广播。 */
	UPROPERTY(BlueprintAssignable, Category="Combat|View", meta=(DisplayName="Modifier View 已变化", ToolTip="可见 Modifier FastArray 增删改时广播。")) FCombatUnitViewChangedDelegate OnModifierViewsChanged;

	/** 服务器从 Unit/Attribute 当前状态刷新基础投影。 */
	void RefreshUnitView();
	/** 服务器从 ModifierComponent 当前稳定快照增量刷新 FastArray。 */
	void RefreshModifierViews();
	/** Ability CastStarted 时写入当前 UI 时间窗。 */
	void NotifyAbilityStarted(
		const FPrimaryAssetId& DefinitionId,
		FCombatEventId ActivationId,
		double StartTime,
		double EndTime,
		bool bChanneling);
	/** Ability End 时只清理匹配的激活 ID。 */
	void NotifyAbilityEnded(FCombatEventId ActivationId);
	/** FastArray 回调统一入口。 */
	void HandleModifierViewsReplicated();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** 绑定 Attribute 与 Modifier 变化源并建立初始 View。 */
	virtual void BeginPlay() override;
	/** 解绑全部原生委托。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 基础 View 复制后通知 UI。 */
	UFUNCTION() void OnRep_UnitView();
	/** 任一可见 Attribute 变化时刷新全部基础数值。 */
	void HandleAttributeChanged(const FOnAttributeChangeData& ChangeData);
	/** UI 白名单状态 Tag count 变化时刷新聚合状态投影。 */
	void HandleVisibleStatusTagChanged(const FGameplayTag Tag, int32 NewCount);
	/** 返回组件所属 Combat Unit。 */
	ACombatUnitCharacter* GetOwnerUnit() const;

	/** 所有客户端统一接收的单位基础投影。 */
	UPROPERTY(ReplicatedUsing=OnRep_UnitView)
	FCombatUnitView UnitView;
	/** 所有客户端统一接收的 Modifier FastArray。 */
	UPROPERTY(Replicated)
	FCombatModifierViewArray ModifierViews;
};
