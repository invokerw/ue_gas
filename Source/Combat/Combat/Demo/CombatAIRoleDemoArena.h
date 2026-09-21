#pragma once

#include "CoreMinimal.h"
#include "CombatCharacter.h"
#include "Combat/Core/CombatTypes.h"
#include "GameFramework/Actor.h"
#include "CombatAIRoleDemoArena.generated.h"

class UCombatAIProfileData;
class ACombatUnitCharacter;

/** 角色演示专用观察 Pawn，扩大初始视野；只改变本地相机，不参与战斗或寻路。 */
UCLASS(meta=(DisplayName="AI 角色演示观察相机", ToolTip="角色演示场的宽视野相机，沿用原滚屏和 Space 跟随操作。"))
class COMBAT_API ACombatAIRoleObserverPawn : public ACombatCharacter
{
	GENERATED_BODY()
public:
	ACombatAIRoleObserverPawn();
};

/** 阶段 B 可玩入口：仅生成与提供空间职责，自动索敌/追击/归位/路线推进来自保存的 StateTree。 */
UCLASS(meta=(DisplayName="AI 通用角色演示场", ToolTip="在导航地面放置野怪和小兵两组演示；初始视野中有独立角色与靶子标识。"))
class COMBAT_API ACombatAIRoleDemoArena : public AActor
{
	GENERATED_BODY()
public:
	ACombatAIRoleDemoArena();
	UPROPERTY(EditAnywhere, Category="AI Demo", meta=(DisplayName="野怪配置", ToolTip="启用感知的守点/归位 StateTree Profile。")) TObjectPtr<UCombatAIProfileData> GuardProfile;
	UPROPERTY(EditAnywhere, Category="AI Demo", meta=(DisplayName="小兵配置", ToolTip="启用感知的巡线 StateTree Profile。")) TObjectPtr<UCombatAIProfileData> LaneProfile;
	UPROPERTY(EditAnywhere, Category="AI Demo", meta=(DisplayName="野怪单位类", ToolTip="含独立名称和数值 DataAsset 的 Combat Unit 蓝图。")) TSubclassOf<ACombatUnitCharacter> GuardClass;
	UPROPERTY(EditAnywhere, Category="AI Demo", meta=(DisplayName="小兵单位类", ToolTip="含独立名称和数值 DataAsset 的 Combat Unit 蓝图。")) TSubclassOf<ACombatUnitCharacter> LaneClass;
	UPROPERTY(EditAnywhere, Category="AI Demo", meta=(DisplayName="训练目标类", ToolTip="已配置属性与血条的木桩蓝图；两只目标均设为敌对队伍 2。")) TSubclassOf<ACombatUnitCharacter> TargetClass;
	UPROPERTY(Replicated, VisibleInstanceOnly, Category="AI Demo", meta=(DisplayName="野怪", ToolTip="服务器生成的守点 AI，客户端仅观察复制。")) TObjectPtr<ACombatUnitCharacter> GuardAgent;
	UPROPERTY(Replicated, VisibleInstanceOnly, Category="AI Demo", meta=(DisplayName="小兵", ToolTip="服务器生成的巡线 AI，客户端不运行决策。")) TObjectPtr<ACombatUnitCharacter> LaneAgent;
	UPROPERTY(Replicated, VisibleInstanceOnly, Category="AI Demo", meta=(DisplayName="野怪训练目标", ToolTip="GUARD TARGET 标识附近的独立木桩。")) TObjectPtr<ACombatUnitCharacter> GuardTarget;
	UPROPERTY(Replicated, VisibleInstanceOnly, Category="AI Demo", meta=(DisplayName="小兵训练目标", ToolTip="LANE TARGET 标识附近的独立木桩。")) TObjectPtr<ACombatUnitCharacter> LaneTarget;
	/** 只读返回实际出生点的导航投影，供演示验收复核归位。 */
	FVector GetGuardHome() const { return GuardHome; }
	/** 只读返回实际小兵出生点；客户端用 Actor 初始位置观察位移。 */
	FVector GetLaneStart() const { return GetActorLocation() + FVector(0, 600, 0); }
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	/** 等待两组局部导航可达后一次发布职责，超时有诊断；不编排攻击或模拟结果。 */
	void TryStart(const FCombatScheduledTickContext&);
	FVector GuardHome = FVector::ZeroVector;
	FCombatScheduleHandle StartupSchedule;
	double StartupDeadline = 0;
	int32 StartupAttempts = 0;
};
