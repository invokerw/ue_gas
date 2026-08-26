// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Combat/Core/CombatTypes.h"

#include "TwinStickAoEAttack.generated.h"

class UStaticMeshComponent;
class USphereComponent;

/** TwinStick 模板的纯表现 AoE；Combat 周期结算统一改由 ThinkerSubsystem 执行。 */
UCLASS(abstract)
class ATwinStickAoEAttack : public AActor
{
	GENERATED_BODY()
	
	/** 提供 AoE 外观。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* SphereVisual;

	/** 仅供表现或蓝图读取的范围球，不再直接结算伤害。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USphereComponent* CollisionSphere;

protected:

	/** 由 Combat Scheduler 管理的表现开始任务。 */
	FCombatScheduleHandle StartAoESchedule;

	/** 由 Combat Scheduler 管理的表现结束任务。 */
	FCombatScheduleHandle StopAoESchedule;

	/** AoE 表现激活前的延迟。 */
	UPROPERTY(EditAnywhere, Category="AoE Attack", meta=(ClampMin = 0, ClampMax = 5, Units = "s"))
	float StartAoETime = 0.033f;

	/** AoE 表现结束前的延迟。 */
	UPROPERTY(EditAnywhere, Category="AoE Attack", meta=(ClampMin = 0, ClampMax = 5, Units = "s"))
	float StopAoETime = 0.5f;

	/** 表现是否已激活；不代表 gameplay 伤害窗口。 */
	bool bIsAoEActive = false;

public:	
	
	/** 创建表现组件并关闭无用 Actor Tick。 */
	ATwinStickAoEAttack();

protected:

	/** 使用 Combat Scheduler 安排表现开始与结束。 */
	virtual void BeginPlay() override;

	/** 取消残留 Scheduler 句柄。 */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

protected:

	/** Scheduler 到期后激活表现，不遍历或伤害重叠对象。 */
	void StartAoE(const FCombatScheduledTickContext& TickContext);

	/** Scheduler 到期后关闭表现并交给蓝图收尾。 */
	void StopAoE(const FCombatScheduledTickContext& TickContext);

	/** 允许蓝图播放淡出；蓝图应在表现结束时销毁 Actor。 */
	UFUNCTION(BlueprintImplementableEvent, Category="AoE Attack")
	void BP_AoEFinished();
};
