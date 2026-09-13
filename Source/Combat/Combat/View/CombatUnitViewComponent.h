#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Combat/View/CombatUnitViewTypes.h"
#include "Combat/View/CombatHUDViewTypes.h"

#include "CombatUnitViewComponent.generated.h"

class ACombatUnitCharacter;
struct FOnAttributeChangeData;

/**
 * 从服务器单位、技能和持续效果提取界面所需的只读快照，并通过属性与 FastArray 增量复制。
 * 接收该单位复制的拥有者和其他客户端得到相同的可见字段，不包含服务器效果实例；界面据此显示资源、施法进度和状态，不通过快照修改战斗结果。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class COMBAT_API UCombatUnitViewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** v5 在拥有者快照增加成长字段和技能可升级标志；原公共 View 和核心 Combat Event schema 保持兼容。 */
	static constexpr int32 PresentationSchemaVersion = 6;
	UCombatUnitViewComponent();

	/** 返回当前客户端或服务器的单位 View。 */
	UFUNCTION(BlueprintPure, Category="Combat|View", meta=(DisplayName="获取战斗单位 View", ToolTip="返回 UI 使用的服务器权威扁平单位投影。"))
	const FCombatUnitView& GetUnitView() const { return UnitView; }
	/** 返回当前可见 Modifier 快照。 */
	UFUNCTION(BlueprintPure, Category="Combat|View", meta=(DisplayName="获取可见 Modifier", ToolTip="返回 FastArray 当前可见条目的只读副本，不包含 Runtime UObject。"))
	TArray<FCombatModifierView> GetVisibleModifiers() const { return ModifierViews.Items; }
	/** 返回拥有者 HUD 快照；客户端按本地输入槽匹配技能，等待复制期间保留空槽。 */
	UFUNCTION(BlueprintPure, Category="Combat|View", meta=(DisplayName="获取拥有者 HUD 快照", ToolTip="只读英雄属性和技能；不查询或修改客户端战斗 Runtime。"))
	FCombatHUDOwnerView GetHUDOwnerView() const;
	/** 服务器采样当前主控单位的显示信息；无主控连接时清空，仅变化时发布。 */
	void RefreshHUDOwnerView();
	/** 使用服务器时间计算一个 View 的剩余持续时间；无限持续返回 -1。 */
	UFUNCTION(BlueprintPure, Category="Combat|View", meta=(DisplayName="计算 Modifier 剩余时间", ToolTip="按服务器结束时间计算剩余秒数；无限持续返回 -1。"))
	float GetModifierRemainingTime(UPARAM(DisplayName="Modifier View") const FCombatModifierView& View) const;
	/** 返回客户端校准后的服务器 World Time；无 GameState 时回退本地 World Time。 */
	UFUNCTION(BlueprintPure, Category="Combat|View", meta=(DisplayName="获取估算服务器时间", ToolTip="用于 UI 对齐技能和状态绝对时间窗。"))
	double GetEstimatedServerTimeSeconds() const;

	/** Unit View 字段变化时广播；订阅关系只在运行时存在，不会写入关卡或 Actor 资产。 */
	UPROPERTY(Transient, BlueprintAssignable, Category="Combat|View", meta=(DisplayName="单位 View 已变化", ToolTip="单位身份、生命、属性或技能投影变化时广播；运行时绑定不会保存到资产。")) FCombatUnitViewChangedDelegate OnUnitViewChanged;
	/** Modifier FastArray 增删改时广播；订阅关系只在运行时存在，不会写入关卡或 Actor 资产。 */
	UPROPERTY(Transient, BlueprintAssignable, Category="Combat|View", meta=(DisplayName="Modifier View 已变化", ToolTip="可见 Modifier FastArray 增删改时广播；运行时绑定不会保存到资产。")) FCombatUnitViewChangedDelegate OnModifierViewsChanged;
	UPROPERTY(Transient, BlueprintAssignable, Category="Combat|View", meta=(DisplayName="HUD 快照已变化", ToolTip="拥有者属性或技能快照变化后刷新本地 HUD。")) FCombatUnitViewChangedDelegate OnHUDOwnerViewChanged;

	/** 服务器从 Unit/Attribute 当前状态刷新基础投影。 */
	void RefreshUnitView();
	/** 服务器从 ModifierComponent 当前稳定快照增量刷新 FastArray。 */
	void RefreshModifierViews();
	/** 服务器记录最新开始技能的定义、激活 ID 和界面时间窗，会覆盖此前展示的技能；这里只保留一条施法记录。 */
	void NotifyAbilityStarted(
		const FPrimaryAssetId& DefinitionId,
		FCombatEventId ActivationId,
		double StartTime,
		double EndTime,
		bool bChanneling);
	/** 服务器在引导调度成功建立时切换当前阶段；只接受匹配的激活 ID。 */
	void NotifyAbilityChannelStarted(FCombatEventId ActivationId, double StartTime, double EndTime);
	/** 服务器只在激活 ID 与当前展示技能一致时清空施法信息，防止较早技能的结束通知抹掉新技能。 */
	void NotifyAbilityEnded(FCombatEventId ActivationId);
	/** FastArray 回调统一入口。 */
	void HandleModifierViewsReplicated();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** 绑定 Attribute 与 Modifier 变化源并建立初始 View。 */
	virtual void BeginPlay() override;
	/** 低频纯展示采样，不执行任何 gameplay 时序或结算。 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	/** 解绑全部原生委托。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 基础 View 复制后通知 UI。 */
	UFUNCTION() void OnRep_UnitView();
	/** 拥有者快照到达后通知；不以客户端时钟修改服务器技能。 */
	UFUNCTION() void OnRep_HUDOwnerView();
	/** 任一可见 Attribute 变化时刷新全部基础数值。 */
	void HandleAttributeChanged(const FOnAttributeChangeData& ChangeData);
	/** UI 白名单状态 Tag count 变化时刷新聚合状态投影。 */
	void HandleVisibleStatusTagChanged(const FGameplayTag Tag, int32 NewCount);
	/** 返回组件所属 Combat Unit。 */
	ACombatUnitCharacter* GetOwnerUnit() const;

	/** 接收该单位复制的客户端共用的基础界面快照。 */
	UPROPERTY(ReplicatedUsing=OnRep_UnitView)
	FCombatUnitView UnitView;
	/** 接收该单位复制的客户端共用的可见效果增量数组。 */
	UPROPERTY(Replicated)
	FCombatModifierViewArray ModifierViews;
	/** 只向当前 owning connection 复制，不暴露其他单位的技能库存。 */
	UPROPERTY(ReplicatedUsing=OnRep_HUDOwnerView)
	FCombatHUDOwnerView HUDOwnerView;
};
