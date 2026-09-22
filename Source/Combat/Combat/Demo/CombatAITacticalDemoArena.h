#pragma once

#include "CoreMinimal.h"
#include "Combat/Core/CombatTypes.h"
#include "GameFramework/Actor.h"
#include "CombatAITacticalDemoArena.generated.h"

class ACombatUnitCharacter;
class UCombatAIProfileData;
struct FCombatOrderResult;
struct FCombatScheduledTickContext;

/** 阶段 C 可玩入口：Hero 在真实攻击边界切换主动技能，远程守卫以 EQS 选点后走公共移动命令。 */
UCLASS(meta=(DisplayName="AI 战术与容量演示场", ToolTip="生成 Hero 与远程守卫两组战术示例；所有决策只在服务器 StateTree 中执行。"))
class COMBAT_API ACombatAITacticalDemoArena : public AActor
{
	GENERATED_BODY()

public:
	ACombatAITacticalDemoArena();

	UPROPERTY(EditAnywhere, Category="AI Demo", meta=(DisplayName="Hero 战术配置", ToolTip="显式 v2 Utility Profile；需包含一个已授予的主动治疗规则。"))
	TObjectPtr<UCombatAIProfileData> HeroProfile;

	UPROPERTY(EditAnywhere, Category="AI Demo", meta=(DisplayName="远程守卫配置", ToolTip="显式 v2 Utility Profile；需配置战术站位 EQS 和触发距离。"))
	TObjectPtr<UCombatAIProfileData> RangedProfile;

	UPROPERTY(EditAnywhere, Category="AI Demo", meta=(DisplayName="Hero 单位类", ToolTip="包含主动技能 AbilitySet 的阶段 C Hero 单位蓝图。"))
	TSubclassOf<ACombatUnitCharacter> HeroClass;

	UPROPERTY(EditAnywhere, Category="AI Demo", meta=(DisplayName="远程守卫单位类", ToolTip="攻击射程覆盖 EQS 站位距离的阶段 C 远程单位蓝图。"))
	TSubclassOf<ACombatUnitCharacter> RangedClass;

	UPROPERTY(EditAnywhere, Category="AI Demo", meta=(DisplayName="训练目标类", ToolTip="两组演示各生成一个敌方训练目标，客户端通过既有单位复制观察结果。"))
	TSubclassOf<ACombatUnitCharacter> TargetClass;

	UPROPERTY(VisibleInstanceOnly, Category="AI Demo", meta=(DisplayName="Hero Bot", ToolTip="服务器生成的战术 Hero；客户端不依赖此引用做决策。"))
	TObjectPtr<ACombatUnitCharacter> HeroAgent;

	UPROPERTY(VisibleInstanceOnly, Category="AI Demo", meta=(DisplayName="远程守卫", ToolTip="服务器生成的 EQS 站位单位；客户端不依赖此引用做决策。"))
	TObjectPtr<ACombatUnitCharacter> RangedAgent;

	UPROPERTY(VisibleInstanceOnly, Category="AI Demo", meta=(DisplayName="Hero 目标", ToolTip="Hero 普攻与技能切换轨迹的敌方训练目标。"))
	TObjectPtr<ACombatUnitCharacter> HeroTarget;

	UPROPERTY(VisibleInstanceOnly, Category="AI Demo", meta=(DisplayName="远程守卫目标", ToolTip="触发远程守卫战术站位的敌方训练目标。"))
	TObjectPtr<ACombatUnitCharacter> RangedTarget;

	/** 返回碰撞调整后的 Hero 出生位置，供 PIE 与联机观察。 */
	FVector GetHeroStart() const { return HeroStart; }
	/** 返回碰撞调整后的远程守卫出生位置，供 PIE 与联机观察。 */
	FVector GetRangedStart() const { return RangedStart; }
	/** Hero 首次真实普攻发射次数；演示伤害只在首次发射后注入。 */
	uint64 GetHeroAttackLaunchCount() const { return HeroAttackLaunchCount; }
	/** Hero 已完成的成功主动施法次数。 */
	uint64 GetHeroCastCount() const { return HeroCastCount; }
	/** 伤害事务成功后完成的主动治疗次数；PIE/Dedicated 只以此证明边界切换。 */
	uint64 GetHeroCastAfterInjuryCount() const { return HeroCastAfterInjuryCount; }
	/** 是否已通过公共伤害事务制造治疗需求。 */
	bool WasHeroInjuryApplied() const { return bHeroInjuryApplied; }
	/** 公共伤害事务完成后的生命值，用于证明后续主动治疗产生了恢复。 */
	float GetHeroHealthAfterInjury() const { return HeroHealthAfterInjury; }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** 等待两组起点、目标和 EQS 预期落点均可导航后，再发布职责并启动 Profile。 */
	void TryStart(const FCombatScheduledTickContext& Context);
	/** 首次真实发射后经公共伤害入口降低 Hero 生命，制造可解释的治疗效用。 */
	void OnHeroAttackLaunched(FCombatAttackHandle AttackHandle, FCombatOrderHandle OrderHandle);
	/** 只观察公共 Order 终态，记录成功 Cast；不改变 StateTree 推进。 */
	void OnHeroOrderFinished(const FCombatOrderResult& Result);
	/** 幂等取消启动等待和委托，旧回调不能跨 World 写入。 */
	void ClearBindings();

	FVector HeroStart = FVector::ZeroVector;
	FVector RangedStart = FVector::ZeroVector;
	FCombatScheduleHandle StartupSchedule;
	FDelegateHandle HeroAttackBinding;
	FDelegateHandle HeroOrderBinding;
	double StartupDeadline = 0.0;
	int32 StartupAttempts = 0;
	uint64 HeroAttackLaunchCount = 0;
	uint64 HeroCastCount = 0;
	uint64 HeroCastAfterInjuryCount = 0;
	float HeroHealthAfterInjury = 0.0f;
	bool bHeroInjuryApplied = false;
};
