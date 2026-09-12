#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"

#include "CombatProgressionComponent.generated.h"

class ACombatUnitCharacter;

/**
 * 管理战斗单位的服务器权威经验、英雄等级和未使用技能点。
 * 经验使用 Dota 风格累计曲线：从 1 级到下一等级的增量为 200、300、400……；
 * 升级只增加技能点，具体技能等级仍由 Combat Ability System Component 修改。
 * 客户端只能读取复制快照或发送技能加点请求，不能直接写入成长状态。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent, DisplayName="战斗成长组件", ToolTip="服务器权威管理等级、经验和技能点；客户端只能读取快照或请求技能加点。"))
class COMBAT_API UCombatProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Dota 风格的默认等级上限。 */
	static constexpr int32 DefaultMaxLevel = 30;

	UCombatProgressionComponent();

	/** 返回当前英雄等级。 */
	UFUNCTION(BlueprintPure, Category="Combat|Progression", meta=(DisplayName="获取英雄等级", ToolTip="返回服务器权威英雄等级。"))
	int32 GetLevel() const { return Level; }
	/** 返回累计经验；等级内经验可用 GetExperienceIntoLevel 查询。 */
	UFUNCTION(BlueprintPure, Category="Combat|Progression", meta=(DisplayName="获取累计经验", ToolTip="返回从 1 级起累计的服务器权威经验值。"))
	int64 GetExperience() const { return Experience; }
	/** 返回尚未分配的技能点。 */
	UFUNCTION(BlueprintPure, Category="Combat|Progression", meta=(DisplayName="获取未使用技能点", ToolTip="返回可用于提升技能等级的服务器权威技能点。"))
	int32 GetUnspentAbilityPoints() const { return UnspentAbilityPoints; }
	/** 返回等级上限。 */
	UFUNCTION(BlueprintPure, Category="Combat|Progression", meta=(DisplayName="获取等级上限", ToolTip="返回本单位的等级上限，默认 30。"))
	int32 GetMaxLevel() const { return MaxLevel; }
	/** 返回达到指定等级所需的累计经验；传入等级会限制在 1 到等级上限。 */
	UFUNCTION(BlueprintPure, Category="Combat|Progression", meta=(DisplayName="获取等级累计经验", ToolTip="返回达到指定等级所需的累计经验；1 级为 0，超出上限时按等级上限计算。"))
	int64 GetExperienceForLevel(int32 TargetLevel) const;
	/** 返回当前等级已经积累的经验。 */
	UFUNCTION(BlueprintPure, Category="Combat|Progression", meta=(DisplayName="获取当前等级经验", ToolTip="返回当前等级起点之后的经验，不包含之前等级的累计经验。"))
	int64 GetExperienceIntoLevel() const;
	/** 返回升到下一级还需要的累计经验；满级返回 0。 */
	UFUNCTION(BlueprintPure, Category="Combat|Progression", meta=(DisplayName="获取升级所需经验", ToolTip="返回升到下一级还需要的经验；达到等级上限时返回 0。"))
	int64 GetExperienceToNextLevel() const;
	/** 返回当前等级经验环的 0 到 1 进度；满级返回 1。 */
	UFUNCTION(BlueprintPure, Category="Combat|Progression", meta=(DisplayName="获取等级经验进度", ToolTip="返回当前等级到下一级的经验比例，范围 0 到 1；满级固定为 1。"))
	float GetExperienceProgress() const;

	/** 等级或经验变化时通知本地 UI；客户端只消费，不通过委托修改成长状态。 */
	DECLARE_MULTICAST_DELEGATE(FOnProgressionChanged);
	FOnProgressionChanged& OnProgressionChanged() { return ProgressionChangedDelegate; }

	/**
	 * 服务器用单位定义初始化等级和等级内经验。该入口每个单位只能成功一次；
	 * 初始等级不会赠送技能点，后续真实升级才会产生可分配点数。
	 */
	bool InitializeProgression(int32 InitialLevel, int64 InitialExperienceIntoLevel);
	/** 服务器增加经验，达到新等级时按跨过的等级数增加技能点。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Progression", meta=(DisplayName="增加战斗经验", ToolTip="仅服务器可调用；增加有限非负经验，跨级时自动增加同等数量技能点。"))
	bool AddExperience(int32 Amount);
	/** 服务器消耗一个技能点并将指定技能提升一级。技能等级不能超过英雄等级或技能自身上限。 */
	bool UpgradeAbility(FGameplayAbilitySpecHandle AbilityHandle, FGameplayTag& OutFailureTag);
	/** 本地或服务器请求技能加点；客户端通过可靠 RPC 发送，服务器重新检查所有条件。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Progression", meta=(DisplayName="请求技能升级", ToolTip="请求将指定技能提升一级；服务器检查拥有权、技能点、英雄等级和技能上限。"))
	bool RequestAbilityUpgrade(FGameplayAbilitySpecHandle AbilityHandle);

	/** 组件结束时解除委托，复制属性只由服务器写入。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** 从 UnitData 初始化后允许服务器调用；未配置 UnitData 时保留默认 1 级。 */
	virtual void BeginPlay() override;

private:
	/** 客户端复制成长快照后通知 HUD。 */
	UFUNCTION() void OnRep_Progression();
	/** owning client 的技能升级请求，服务器忽略客户端等级和技能点猜测。 */
	UFUNCTION(Server, Reliable) void ServerUpgradeAbility(FGameplayAbilitySpecHandle AbilityHandle);
	/** 返回所属战斗单位。 */
	ACombatUnitCharacter* GetOwnerUnit() const;
	/** 为经验和升级事件写入服务器诊断记录。 */
	void EmitProgressionEvent(const FGameplayTag& EventType, float RequestedAmount, float AppliedAmount, const FString& Diagnostic) const;

	/** 当前服务器权威等级。 */
	UPROPERTY(ReplicatedUsing=OnRep_Progression, BlueprintReadOnly, Category="Combat|Progression", meta=(AllowPrivateAccess="true", DisplayName="英雄等级", ToolTip="服务器权威英雄等级；达到上限后不再增加。", ClampMin="1"))
	int32 Level = 1;
	/** 从 1 级起累计的服务器权威经验。 */
	UPROPERTY(ReplicatedUsing=OnRep_Progression, BlueprintReadOnly, Category="Combat|Progression", meta=(AllowPrivateAccess="true", DisplayName="累计经验", ToolTip="服务器权威累计经验；经验环显示当前等级区间内的比例。", ClampMin="0"))
	int64 Experience = 0;
	/** 还未用于技能升级的技能点。 */
	UPROPERTY(ReplicatedUsing=OnRep_Progression, BlueprintReadOnly, Category="Combat|Progression", meta=(AllowPrivateAccess="true", DisplayName="未使用技能点", ToolTip="每次英雄升级增加一个技能点；加点成功后减少一个。", ClampMin="0"))
	int32 UnspentAbilityPoints = 0;
	/** 可按单位蓝图覆盖的等级上限，默认使用 Dota 风格 30 级。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Progression", meta=(AllowPrivateAccess="true", DisplayName="等级上限", ToolTip="单位可达到的最高英雄等级；等级上限必须至少为 1。", ClampMin="1", ClampMax="100"))
	int32 MaxLevel = DefaultMaxLevel;
	/** 防止 BeginPlay 或重复初始化再次重置服务器进度。 */
	bool bInitialized = false;
	/** 等级、经验或技能点变化时的本地通知。 */
	FOnProgressionChanged ProgressionChangedDelegate;
};
