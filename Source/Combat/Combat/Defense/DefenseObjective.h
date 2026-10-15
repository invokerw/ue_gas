#pragma once
#include "CoreMinimal.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "DefenseObjective.generated.h"
class UStaticMeshComponent;
/** 灰盒敌人的可见模型；碰撞仍只有 Combat 胶囊，未配置定义时不能出生。 */
UCLASS(Blueprintable, meta=(DisplayName="防守敌人", ToolTip="带可见灰盒模型的 Combat 敌人。"))
class COMBAT_API ADefenseEnemy : public ACombatUnitCharacter
{
	GENERATED_BODY()
public:
	ADefenseEnemy(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
protected:
	UPROPERTY(VisibleAnywhere, Category="Defense", meta=(DisplayName="灰盒模型", ToolTip="纯表现静态网格，禁用碰撞和导航影响。")) TObjectPtr<UStaticMeshComponent> Body;
};
/** 核心复用 Combat ASC/Lifecycle，零移速不自主下令，导演监听死亡以失败。 */
UCLASS(Blueprintable, meta=(DisplayName="防守核心", ToolTip="生命由 Combat ASC 管理的静止核心。"))
class COMBAT_API ADefenseObjective : public ADefenseEnemy
{
	GENERATED_BODY()
public:
	ADefenseObjective(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
