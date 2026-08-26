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
	UFUNCTION(BlueprintCallable, Category="Combat|Targeting")
	FCombatTargetValidationResult ValidateAbilityTarget(
		ACombatUnitCharacter* Source,
		const FGameplayTagContainer& BehaviorTags,
		const FCombatTargetingRules& Rules,
		const FCombatAbilityTargetData& TargetData) const;

	/** 直接校验一个单位目标，供 UI/Order 预览和服务器 cast point 复核。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Targeting")
	FCombatTargetValidationResult ValidateUnitTarget(
		ACombatUnitCharacter* Source,
		ACombatUnitCharacter* Target,
		const FCombatTargetingRules& Rules) const;

	/** 直接校验一个点目标；结果包含服务器接受的位置。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Targeting")
	FCombatTargetValidationResult ValidatePointTarget(
		ACombatUnitCharacter* Source,
		FVector TargetLocation,
		const FCombatTargetingRules& Rules) const;

	/** 由服务器枚举半径内单位、复用状态/阵营规则、去重并稳定排序。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Targeting")
	TArray<ACombatUnitCharacter*> QueryUnitsInRadius(
		ACombatUnitCharacter* Source,
		FVector Center,
		float Radius,
		const FCombatTargetingRules& Rules) const;

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
