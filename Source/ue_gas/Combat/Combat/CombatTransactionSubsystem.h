#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Combat/CombatTransactionTypes.h"

#include "CombatTransactionSubsystem.generated.h"

class ACombatUnitCharacter;

/** 保存 ApplyGameplayEffect 与 AttributeSet 回调之间的同步结果槽。 */
UCLASS()
class UE_GAS_API UCombatTransactionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 为 Damage 或 Heal 创建唯一 EventId 槽；重复或无效输入返回 false。 */
	bool BeginSlot(const FCombatEventContext& Context, ECombatTransactionKind Kind, ACombatUnitCharacter* Target);
	/** 由 AttributeSet 回报一次真实 delta；未知、重复或类型错误的槽安全失败。 */
	bool ReportDelta(FCombatEventId EventId, ECombatTransactionKind Kind, const FCombatTransactionDelta& Delta);
	/** 在 Apply GE 返回后读取并关闭槽；尚未回报的槽返回 false。 */
	bool ConsumeSlot(FCombatEventId EventId, ECombatTransactionKind Kind, FCombatTransactionDelta& OutDelta);
	/** GE 应用失败时显式关闭槽，避免后续 EventId 误命中。 */
	bool CancelSlot(FCombatEventId EventId);
	/** 返回当前尚未关闭的同步槽数量，供自动化和诊断使用。 */
	int32 GetOpenSlotCount() const { return Slots.Num(); }

private:
	/** 单个 EventId 在同步 Apply 期间的等待状态。 */
	struct FSlot
	{
		/** 槽位期望的元属性类型。 */
		ECombatTransactionKind Kind = ECombatTransactionKind::Damage;
		/** 防止错误 AttributeSet 或目标回报到同一 EventId。 */
		TWeakObjectPtr<ACombatUnitCharacter> Target;
		/** AttributeSet 写入的真实结果。 */
		FCombatTransactionDelta Delta;
		/** 每个槽只允许回报一次。 */
		bool bReported = false;
	};

	/** 以单调 EventId 序号索引的短生命周期同步槽。 */
	TMap<uint64, FSlot> Slots;
};
