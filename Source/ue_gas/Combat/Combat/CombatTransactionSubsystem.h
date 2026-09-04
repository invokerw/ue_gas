#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Combat/CombatTransactionTypes.h"

#include "CombatTransactionSubsystem.generated.h"

class ACombatUnitCharacter;

/**
 * 在伤害/治疗调用与 GAS 属性执行回调之间传递实际生命变化。
 * 调用方先按事件 ID 建立临时槽，AttributeSet 同步回报一次结果，调用方再取走并删除；应用失败时必须显式取消。回报本身不会删除槽。
 */
UCLASS()
class UE_GAS_API UCombatTransactionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 为 Damage 或 Heal 创建唯一 EventId 槽；重复或无效输入返回 false。 */
	bool BeginSlot(const FCombatEventContext& Context, ECombatTransactionKind Kind, ACombatUnitCharacter* Target);
	/** 由 AttributeSet 写入一次实际生命变化；槽不存在、类型不符、已回报或目标已销毁时返回 false。成功后保留结果，等待 ConsumeSlot 取走。 */
	bool ReportDelta(FCombatEventId EventId, ECombatTransactionKind Kind, const FCombatTransactionDelta& Delta);
	/** 在效果应用返回后取走结果并删除槽；不存在、类型不符或尚未回报时返回 false，保持槽和 OutDelta 不变。 */
	bool ConsumeSlot(FCombatEventId EventId, ECombatTransactionKind Kind, FCombatTransactionDelta& OutDelta);
	/** GE 应用失败时显式关闭槽，避免后续 EventId 误命中。 */
	bool CancelSlot(FCombatEventId EventId);
	/** 返回当前尚未关闭的同步槽数量，供自动化和诊断使用。 */
	int32 GetOpenSlotCount() const { return Slots.Num(); }

private:
	/** 单个事件在同步应用效果期间的等待状态与一次性结果。 */
	struct FSlot
	{
		/** 槽位期望的元属性类型。 */
		ECombatTransactionKind Kind = ECombatTransactionKind::Damage;
		/** 弱引用预期目标，回报时只检查它是否仍存在；回报接口不携带目标参数，事件 ID 必须由调用链正确传递。 */
		TWeakObjectPtr<ACombatUnitCharacter> Target;
		/** AttributeSet 写入的真实结果。 */
		FCombatTransactionDelta Delta;
		/** 每个槽只允许回报一次。 */
		bool bReported = false;
	};

	/** 以单调 EventId 序号索引的短生命周期同步槽。 */
	TMap<uint64, FSlot> Slots;
};
