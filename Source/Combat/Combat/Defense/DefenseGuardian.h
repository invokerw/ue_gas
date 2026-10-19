#pragma once
#include "CoreMinimal.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "DefenseGuardian.generated.h"
class UStaticMeshComponent;

/** 神殿守卫纯表现组件；战斗数值与三主动一被动均来自独立Unit/AbilitySet资产。 */
UCLASS(Blueprintable, meta=(DisplayName="神殿守卫角色", ToolTip="近战守卫的盾牌与武器外形；不增加碰撞、导航障碍或游戏计时。"))
class COMBAT_API ADefenseGuardianHero : public ACombatUnitCharacter
{
	GENERATED_BODY()
public:
	ADefenseGuardianHero(const FObjectInitializer& Init=FObjectInitializer::Get());
protected:
	UPROPERTY(VisibleAnywhere, Category="Defense", meta=(DisplayName="盾牌外形", ToolTip="仅表现，附着角色根部并随朝向旋转；无碰撞或导航影响。")) TObjectPtr<UStaticMeshComponent> ShieldMesh;
	UPROPERTY(VisibleAnywhere, Category="Defense", meta=(DisplayName="武器外形", ToolTip="仅表现的近战武器；伤害仍由公共Attack结算。")) TObjectPtr<UStaticMeshComponent> WeaponMesh;
};

/** 盾击/震地共用公开施法扩展；只在SpellStarted按权威位置查询，角度过滤不绕过Targeting许可。 */
UCLASS(Blueprintable, meta=(DisplayName="守卫范围技能", ToolTip="公共范围查询后按分级半角筛选，再提交伤害与控制；180度半角表示整圆。"))
class COMBAT_API UDefenseGuardianAreaAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category="Defense", meta=(DisplayName="普通控制效果", ToolTip="可选；命中存活普通敌人后施加的控制定义。空值表示只有伤害。")) TObjectPtr<UCombatModifierData> ControlModifier;
	UPROPERTY(EditDefaultsOnly, Category="Defense", meta=(DisplayName="Boss失衡效果", ToolTip="Boss不施加普通硬控，改为本失衡定义；为空时Boss只有伤害。")) TObjectPtr<UCombatModifierData> BossStaggerModifier;
	UPROPERTY(EditDefaultsOnly, Category="Defense", meta=(DisplayName="Boss单位定义", ToolTip="精确匹配需要累计失衡的CombatUnit身份；只读稳定ID，不根据类名或血量猜测。")) FPrimaryAssetId BossDefinition;
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo,const FGameplayAbilitySpec& Spec) override;
protected:
	virtual void ReceiveSpellStart_Implementation(const FCombatAbilityActivationContext& Context) override;
};

/** 固定窗口失衡状态由Boss自持，允许不同守卫合计层数；触发后先锁定再排队控制和免疫。 */
UCLASS(Blueprintable, meta=(DisplayName="Boss失衡累计", ToolTip="达到配置层数后施加眩晕与保护并移除自身；窗口和保护均由公共Modifier过期调度管理。"))
class COMBAT_API UDefenseBossStaggerRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category="Defense", meta=(DisplayName="失衡眩晕", ToolTip="达到阈值施加的Stunned定义；时长由该资产配置。")) TObjectPtr<UCombatModifierData> StunData;
	UPROPERTY(EditDefaultsOnly, Category="Defense", meta=(DisplayName="失衡保护", ToolTip="存在此定义时忽略新层数；持续时间包含眩晕和随后免疫窗口。")) TObjectPtr<UCombatModifierData> RecoveryData;
protected:
	virtual void OnCreated_Implementation() override;
	virtual void OnRefreshed_Implementation() override;
private:
	/** 创建与刷新共用的触发入口；公共Apply在Hook内可能只排队，不依赖同步句柄。 */
	void ResolveStagger();
	bool bTriggered=false;
};

/** 受击蓄盾属于Intrinsic生命周期；只观察有效直接普攻，冷却用世界游戏时钟，无自建Timer。 */
UCLASS(Blueprintable, meta=(DisplayName="守卫坚忍蓄盾", ToolTip="累计直接普攻受击后，下一次有效普攻产生生命比例护盾；死亡清空，Break期间不响应Hook。"))
class COMBAT_API UDefenseGuardianPassiveRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category="Defense", meta=(DisplayName="蓄盾触发效果", ToolTip="自身普攻兑现的护盾定义；比例、持续时间和上限在该Modifier数据配置。")) TObjectPtr<UCombatModifierData> ShieldData;
	/** 只读调试计数；客户端没有权威Runtime，界面仍使用Modifier View。 */
	int32 GetReceivedHits() const { return ReceivedHits; }
	bool IsArmed() const { return bArmed; }
protected:
	virtual void OnPostTakeDamage_Implementation(const FCombatDamageEvent& Event) override;
	virtual void OnPostDealDamage_Implementation(const FCombatDamageEvent& Event) override;
private:
	/** 有效直接普攻且未计入本Runtime的根事件；有界缓存拒绝同一次攻击的派生重入。 */
	bool AcceptAttack(const FCombatDamageEvent& Event,TArray<FCombatEventId>& Recent);
	int32 ReceivedHits=0;
	bool bArmed=false;
	double ReadyAt=0;
	TArray<FCombatEventId> IncomingRoots,OutgoingRoots;
};
