#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Targeting/CombatTargetingTypes.h"

#include "CombatTargetingSubsystem.generated.h"

class ACombatUnitCharacter;

/**
 * 指令、技能和界面预览共用的目标规则。校验单位/点的阵营、生命、状态、距离和视线，并查询范围内满足条件的单位。
 * 接口按所在 World 的当前状态计算，本身不拒绝客户端调用；客户端结果只能用于预览，实际施法与命中必须由服务器重新调用确认。
 */
UCLASS()
class UE_GAS_API UCombatTargetingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 根据 Ability 三种目标模式校验 TargetData，且拒绝客户端命中列表。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Targeting", meta=(DisplayName="验证技能目标", ToolTip="根据技能行为标签与目标规则校验目标数据；命中列表始终由服务器生成。"))
	FCombatTargetValidationResult ValidateAbilityTarget(
		UPARAM(DisplayName="来源单位") ACombatUnitCharacter* Source,
		UPARAM(DisplayName="技能行为标签") const FGameplayTagContainer& BehaviorTags,
		UPARAM(DisplayName="目标规则") const FCombatTargetingRules& Rules,
		UPARAM(DisplayName="目标数据") const FCombatAbilityTargetData& TargetData) const;

	/** 检查单位目标的状态、阵营、双方胶囊边缘距离与可选视线条件；供本地预览、指令预检和服务器施法生效前复核共用。成功位置取目标当前位置。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Targeting", meta=(DisplayName="验证单位目标", ToolTip="检查单位目标的状态、阵营、双方胶囊边缘距离与可选视线条件；供本地预览、指令预检和服务器施法生效前复核共用。成功位置取目标当前位置。"))
	FCombatTargetValidationResult ValidateUnitTarget(
		UPARAM(DisplayName="来源单位") ACombatUnitCharacter* Source,
		UPARAM(DisplayName="目标单位") ACombatUnitCharacter* Target,
		UPARAM(DisplayName="目标规则") const FCombatTargetingRules& Rules) const;

	/** 直接校验一个点目标；结果包含服务器接受的位置。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Targeting", meta=(DisplayName="验证点目标", ToolTip="校验一个世界位置目标，并返回服务器实际接受的位置。"))
	FCombatTargetValidationResult ValidatePointTarget(
		UPARAM(DisplayName="来源单位") ACombatUnitCharacter* Source,
		UPARAM(DisplayName="目标位置") FVector TargetLocation,
		UPARAM(DisplayName="目标规则") const FCombatTargetingRules& Rules) const;

	/** 枚举当前 World 中与 XY 圆形范围相交的单位，计入目标胶囊半径，复用状态/阵营/视线规则并按 Actor ID 排序。跳过来源施法距离限制；权威命中须在服务器调用。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Targeting", meta=(DisplayName="查询半径内战斗单位", ToolTip="枚举当前 World 中与 XY 圆形范围相交的单位，计入目标胶囊半径，复用状态/阵营/视线规则并按 Actor ID 排序。跳过来源施法距离限制；权威命中须在服务器调用。"))
	TArray<ACombatUnitCharacter*> QueryUnitsInRadius(
		UPARAM(DisplayName="来源单位") ACombatUnitCharacter* Source,
		UPARAM(DisplayName="圆心") FVector Center,
		UPARAM(DisplayName="半径") float Radius,
		UPARAM(DisplayName="目标规则") const FCombatTargetingRules& Rules) const;

	/** 查询 XY 线段两侧 HalfWidth 厘米内的单位，计入目标胶囊半径及端点圆形区域，按沿线投影位置再按 Actor ID 排序。零长度退化为圆形查询；跳过来源施法距离限制，权威命中须在服务器调用。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Targeting", meta=(DisplayName="查询线段范围内战斗单位", ToolTip="查询 XY 线段两侧 HalfWidth 厘米内的单位，计入目标胶囊半径及端点圆形区域，按沿线投影位置再按 Actor ID 排序。零长度退化为圆形查询；跳过来源施法距离限制，权威命中须在服务器调用。"))
	TArray<ACombatUnitCharacter*> QueryUnitsAlongSegment(
		UPARAM(DisplayName="来源单位") ACombatUnitCharacter* Source,
		UPARAM(DisplayName="线段起点") FVector Start,
		UPARAM(DisplayName="线段终点") FVector End,
		UPARAM(DisplayName="半宽") float HalfWidth,
		UPARAM(DisplayName="目标规则") const FCombatTargetingRules& Rules) const;

private:
	/** 执行单位校验，并允许 AoE 查询跳过来源施法距离检查。 */
	FCombatTargetValidationResult ValidateUnitTargetInternal(
		ACombatUnitCharacter* Source,
		ACombatUnitCharacter* Target,
		const FCombatTargetingRules& Rules,
		bool bCheckCastRange) const;
	/** 使用 CombatTargeting trace channel 检查来源到单位或点的阻挡。 */
	bool HasLineOfSight(ACombatUnitCharacter& Source, const FVector& AimPoint, const AActor* TargetToIgnore) const;
};
