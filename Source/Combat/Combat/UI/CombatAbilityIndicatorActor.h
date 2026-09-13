#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatAbilityIndicatorActor.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
struct FCombatAbilityAimPreview;

/** 本地 AimComponent 持有的纯视觉 Actor：复用三层贴花，不复制、不碰撞、不 Tick，不查询或命中单位。 */
UCLASS(NotBlueprintable, Transient)
class COMBAT_API ACombatAbilityIndicatorActor : public AActor
{
	GENERATED_BODY()
public:
	ACombatAbilityIndicatorActor();
	/** 用当前展示帧更新施法圈、作用形状和落点标记；缺少材质时保持隐藏。 */
	void ShowPreview(const FCombatAbilityAimPreview& Preview);
private:
	/** 配置单层世界坐标 SDF 贴花；位移只影响本视觉组件，绝不移动战斗单位。 */
	void ShowLayer(int32 Index, FVector Center, FVector Direction, float Radius, float Length, float Shape,
		FLinearColor Color, float Stroke, float Fill, bool bDashed);
	UPROPERTY(Transient) TArray<TObjectPtr<UDecalComponent>> IndicatorLayers;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
	/** CDO 持有可被 cook 收集的软引用；不能仅在 LoadObject 中保留路径字符串。 */
	UPROPERTY() TSoftObjectPtr<UMaterialInterface> IndicatorMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/Combat/Shared/Materials/M_CombatAbilityIndicator.M_CombatAbilityIndicator")));
};
