#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatUnitViewTypes.generated.h"

class UCombatUnitViewComponent;

/** 客户端 UI 可见的扁平 Modifier 投影，不包含 Runtime UObject 或秘密状态。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatModifierView : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** 服务器 Runtime 对应的稳定句柄，只用于 View 增量身份与诊断。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="Modifier 句柄", ToolTip="用于区分同一定义的多个可见 Modifier 实例。"))
	FCombatModifierHandle Handle;
	/** 客户端用于解析名称、图标和文本的稳定定义 ID。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="定义 ID", ToolTip="客户端通过 AssetManager 解析的稳定 Modifier 定义 ID。"))
	FPrimaryAssetId DefinitionId;
	/** 当前服务器权威叠加层数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="叠加层数", ToolTip="当前服务器权威的 Modifier 层数。"))
	int32 StackCount = 0;
	/** 服务器绝对 World Game Time；0 表示无限持续。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="服务器结束时间", ToolTip="服务器绝对结束时间；0 表示无限持续。", Units="s"))
	double ServerEndTime = 0.0;
	/** 是否属于 Debuff。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="是减益", ToolTip="该 Modifier 是否按减益规则处理。"))
	bool bIsDebuff = false;
	/** 是否至少能被 Strong Dispel 移除。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="可驱散", ToolTip="该 Modifier 是否允许被任一驱散强度移除。"))
	bool bDispellable = false;

	/** 比较客户端可见 payload，忽略 FastArray 内部复制字段。 */
	bool HasSamePayload(const FCombatModifierView& Other) const;
};

/** FastArray 增量复制容器。 */
USTRUCT()
struct UE_GAS_API FCombatModifierViewArray : public FFastArraySerializer
{
	GENERATED_BODY()

	/** 当前可见 Modifier 条目。 */
	UPROPERTY()
	TArray<FCombatModifierView> Items;

	/** 反向通知所属组件刷新蓝图/UI。 */
	UPROPERTY(NotReplicated)
	TObjectPtr<UCombatUnitViewComponent> Owner = nullptr;

	/** 使用 UE FastArray delta serializer。 */
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FCombatModifierView, FCombatModifierViewArray>(
			Items, DeltaParams, *this);
	}

	/** 客户端新增条目后统一通知一次。 */
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	/** 客户端修改条目后统一通知一次。 */
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	/** 客户端移除条目前统一通知一次。 */
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
};

template<>
struct TStructOpsTypeTraits<FCombatModifierViewArray> : TStructOpsTypeTraitsBase2<FCombatModifierViewArray>
{
	enum { WithNetDeltaSerializer = true };
};

/** Owner 与非 Owner UI 共用的单位只读投影。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatUnitView
{
	GENERATED_BODY()

	/** UnitData 的稳定身份；未初始化时无效。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="单位定义 ID", ToolTip="客户端通过 AssetManager 解析的稳定 Unit 定义 ID。")) FPrimaryAssetId UnitDefinitionId;
	/** 当前服务器权威队伍。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="队伍 ID", ToolTip="当前服务器权威战斗队伍。")) FCombatTeamId TeamId;
	/** 当前生命代次。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="生命代次", ToolTip="当前生命代次，用于淘汰复活前的旧表现。")) int64 LifeGeneration = 0;
	/** 当前完整生命状态。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="生命状态", ToolTip="当前服务器权威 Alive/Dying/Dead/Respawning 状态。")) ECombatLifeState LifeState = ECombatLifeState::Alive;
	/** 当前与最大生命。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="当前生命值", ToolTip="当前服务器权威生命值。")) float Health = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="最大生命值", ToolTip="当前服务器权威最大生命值。")) float MaxHealth = 0.0f;
	/** 当前与最大法力。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="当前法力值", ToolTip="当前服务器权威法力值。")) float Mana = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="最大法力值", ToolTip="当前服务器权威最大法力值。")) float MaxMana = 0.0f;
	/** 当前最近开始且仍活动的 Ability 定义。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="活动技能定义 ID", ToolTip="当前最近开始且尚未结束的 Ability 定义 ID。")) FPrimaryAssetId ActiveAbilityDefinitionId;
	/** 当前 Ability 根激活 ID，用于 exactly-once 清理 View。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="技能激活 ID", ToolTip="当前 Ability 的根激活事件 ID。")) FCombatEventId AbilityActivationId;
	/** 当前 Ability 的服务器起止时间。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="技能服务器开始时间", ToolTip="当前 Ability 的服务器绝对开始时间。", Units="s")) double AbilityServerStartTime = 0.0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="技能服务器结束时间", ToolTip="当前 Ability 的服务器绝对结束时间；0 表示尚未确定。", Units="s")) double AbilityServerEndTime = 0.0;
	/** 当前 Ability 是否处于 Channelled 模式。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|View", meta=(DisplayName="正在引导", ToolTip="当前活动 Ability 是否处于 Channelled 模式。")) bool bChanneling = false;
};

/** Unit View 或 Modifier FastArray 变化时通知蓝图 UI。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCombatUnitViewChangedDelegate);
