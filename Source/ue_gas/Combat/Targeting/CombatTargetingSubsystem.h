#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Targeting/CombatTargetingTypes.h"

#include "CombatTargetingSubsystem.generated.h"

class ACombatUnitCharacter;

/** 为 UI、Order 和 Ability 提供同一套服务器目标与 AoE 查询规则。 */
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

	/** 直接校验一个单位目标，供 UI/Order 预览和服务器 cast point 复核。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Targeting", meta=(DisplayName="验证单位目标", ToolTip="校验一个单位目标，适用于 UI、命令预览与服务器执行点复核。"))
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

	/** 由服务器枚举半径内单位、复用状态/阵营规则、去重并稳定排序。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Targeting", meta=(DisplayName="查询半径内战斗单位", ToolTip="由服务器查询半径内符合阵营和状态规则的单位，并去重后稳定排序。"))
	TArray<ACombatUnitCharacter*> QueryUnitsInRadius(
		UPARAM(DisplayName="来源单位") ACombatUnitCharacter* Source,
		UPARAM(DisplayName="圆心") FVector Center,
		UPARAM(DisplayName="半径") float Radius,
		UPARAM(DisplayName="目标规则") const FCombatTargetingRules& Rules) const;

	/** 由服务器查询有限 XY 线段宽度内单位，去重后按沿线距离与 Actor identity 稳定排序。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Targeting", meta=(DisplayName="查询线段范围内战斗单位", ToolTip="由服务器查询有限 XY 线段宽度内的合法单位，并按沿线距离稳定排序。"))
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
