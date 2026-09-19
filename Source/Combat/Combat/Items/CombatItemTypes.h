#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "Combat/Core/CombatTypes.h"
#include "CombatItemTypes.generated.h"

/** 物品统一位置约定；0..5 为装备，6..8 为背包，INDEX_NONE 为地面。 */
namespace CombatItems
{
	inline constexpr int32 EquippedSlots = 6;
	inline constexpr int32 TotalSlots = 9;
	inline constexpr float ReequipDelay = 6.0f;
	inline constexpr float InteractionRange = 150.0f;
	inline constexpr float BackpackCooldownRate = 0.5f;
	inline bool IsSlot(int32 Slot) { return Slot >= 0 && Slot < TotalSlots; }
	inline bool IsEquipped(int32 Slot) { return Slot >= 0 && Slot < EquippedSlots; }
}

/** 仅拥有者复制的物品值快照；客户端按服务器时间外推显示，不持有或更改权威实例。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatItemView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="实例", ToolTip="物品稳定句柄；空槽为无效值。")) FCombatItemHandle Handle;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="定义", ToolTip="本地资源解析使用的稳定物品 ID。")) FPrimaryAssetId DefinitionId;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="主动技能", ToolTip="该物品独立授予的 AbilitySpec；纯被动为空。")) FGameplayAbilitySpecHandle AbilityHandle;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="修订", ToolTip="请求提交时原样携带，服务器拒绝已变化的实例。")) int32 Revision = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="数量", ToolTip="实例中的实际物品数量。")) int32 Quantity = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="能量", ToolTip="剩余能量；不用能量的物品为 0。")) int32 Charges = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="已锁定", ToolTip="锁定物品不会被自动合成或商店购买计划消费；解锁仍需服务器确认。")) bool bLocked = false;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="冷却检查点", ToolTip="剩余冷却数值对应的服务器时间，单位秒。")) double CooldownCheckpoint = 0.0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="检查点剩余冷却", ToolTip="按正常速度计量的剩余秒数；显示时减去经过时间乘冷却速率。")) float CooldownRemaining = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="冷却总量", ToolTip="本次提交后冻结的正常速度冷却秒数。")) float CooldownDuration = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="冷却速率", ToolTip="装备为 1，背包与地面为 0.5。")) float CooldownRate = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="重新装备解禁时间", ToolTip="服务器世界时间；到达前主动与被动都被禁用。")) double EnabledAt = 0.0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="法力费用", ToolTip="当前主动技能的法力费用；无主动为 0。")) float ManaCost = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="失败原因", ToolTip="服务器最近投影的不可用原因；客户端不能据此跳过服务器复核。")) FGameplayTag FailureTag;
	bool operator==(const FCombatItemView& Other) const;
	/** 按同步服务器时间计算显示剩余秒数，不回写权威冷却。 */
	float GetRemaining(double ServerTime) const { return FMath::Max(0.0, CooldownRemaining - FMath::Max(0.0, ServerTime - CooldownCheckpoint) * CooldownRate); }
};
