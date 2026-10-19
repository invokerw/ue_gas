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

/** 攻城兵纯表现外形；生命/移速/攻击来自UnitData，路线和目标由原Director/Order驱动。 */
UCLASS(Blueprintable, meta=(DisplayName="防守攻城冲槌", ToolTip="慢速攻城单位的车体与冲槌外形；不新增碰撞、AI、Timer或伤害入口。"))
class COMBAT_API ADefenseSiegeEnemy : public ADefenseEnemy
{
	GENERATED_BODY()
public:
	ADefenseSiegeEnemy(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
private:
	UPROPERTY(VisibleAnywhere, Category="Defense", meta=(DisplayName="攻城冲槌", ToolTip="仅用于识别攻城兵的前向冲槌；禁用碰撞和导航影响。")) TObjectPtr<UStaticMeshComponent> Ram;
};

/** 刺客仅提供瘦高灰盒外形；选敌、追击和伤害由出生组角色配置与公共Combat系统驱动。 */
UCLASS(Blueprintable, meta=(DisplayName="防守游猎刺客", ToolTip="紫色尖锥刺客外形；无新增碰撞、Tick或战斗结算。"))
class COMBAT_API ADefenseAssassinEnemy : public ADefenseEnemy
{
	GENERATED_BODY()
public:
	ADefenseAssassinEnemy(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
