#pragma once

#include "CoreMinimal.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "CombatItemData.generated.h"

class UTexture2D;
class UStaticMesh;

/** 地面物品的拾取授权；绑定在第一次进入背包时确定，丢弃不清除绑定。 */
UENUM(BlueprintType)
enum class ECombatItemSharing : uint8
{
	Public UMETA(DisplayName="所有单位可拾取"),
	BoundUnit UMETA(DisplayName="绑定首次持有单位"),
	AlliedTeam UMETA(DisplayName="首次持有者的友军可拾取")
};

/** 一项常驻物品效果；空互斥组可叠加，同组只让最靠前的装备槽提供效果。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatItemPassive
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="效果定义", ToolTip="装备生效时施加的常驻 Modifier；应配置无限持续且不可驱散。")) TObjectPtr<UCombatModifierData> Modifier;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="被动互斥组", ToolTip="None 表示独立叠加；同名非空组只启用最靠前的可用装备槽，不影响物品其他被动。")) FName UniqueGroup;
};

/**
 * 物品的只读内容定义。实例数量、能量、位置和冷却由世界物品登记表管理。
 * 主动行为沿用技能类 CDO 的单向 AbilityData 配置；属性和 Hook 仍由 Modifier 的 GAS 效果结算。
 */
UCLASS(BlueprintType)
class COMBAT_API UCombatItemData : public UCombatDefinitionData
{
	GENERATED_BODY()
public:
	UCombatItemData();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Presentation", meta=(DisplayName="物品说明", ToolTip="背包与地面悬浮提示中展示的玩法说明，不参与结算。", MultiLine="true")) FText Description;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Presentation", meta=(DisplayName="图标", ToolTip="可选图标纹理；为空时显示简称与品质色。")) TSoftObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Presentation", meta=(DisplayName="图标简称", ToolTip="没有图标纹理时使用的一至两个字，例如甲、靴、药。")) FText Glyph;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Presentation", meta=(DisplayName="物品颜色", ToolTip="地面轮廓与 HUD 图标底色，仅用于表现。")) FLinearColor Tint = FLinearColor(0.3f, 0.65f, 0.8f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Presentation", meta=(DisplayName="地面模型", ToolTip="可选拾取物模型；为空时使用统一的物品晶体模型。")) TSoftObjectPtr<UStaticMesh> WorldMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="最大堆叠数量", ToolTip="同一实例允许的物品数量，范围 1 到 99；带独立能量的物品必须为 1。", ClampMin="1", ClampMax="99")) int32 MaxStack = 1;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="初始能量", ToolTip="实例生成时的使用次数；0 表示不使用能量，带能量时最大堆叠必须为 1。", ClampMin="0", ClampMax="9999")) int32 InitialCharges = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="每次消耗数量", ToolTip="在技能费用提交阶段扣除的物品数量；0 表示不消耗物品，最后一件消耗后等当前施法结束才撤销技能。", ClampMin="0", ClampMax="99")) int32 QuantityPerUse = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="每次消耗能量", ToolTip="有初始能量时在费用提交阶段扣除的能量；不自动充能，0 表示不消耗能量。", ClampMin="0", ClampMax="9999")) int32 ChargesPerUse = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="能量耗尽时销毁", ToolTip="启用后最后一次能量消耗完成施法后移除物品；否则保留空能量的物品及被动。")) bool bDestroyWhenChargesEmpty = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="主动技能类", ToolTip="可选的 Combat GameplayAbility 类；每个实例独立授予，不占 QWER 技能槽，也不能用英雄技能点升级。")) TSubclassOf<UCombatGameplayAbility> ActiveAbility;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="被动效果", ToolTip="装备生效时维护的常驻 Modifier；背包、地面和重新装备等待期间全部撤销。", TitleProperty="Modifier")) TArray<FCombatItemPassive> Passives;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="共享冷却组", ToolTip="None 表示实例独立冷却；同组携带物品在主动提交冷却时至少获得相同的剩余冷却，转移后不清零。")) FName SharedCooldownGroup;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="移动类主动", ToolTip="启用后缠绕也禁止此物品主动；位移结算仍必须使用 Combat Motion。")) bool bMovementActive = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="拾取共享规则", ToolTip="所有单位可拾取、绑定首次持有单位，或仅允许首次持有者的友军拾取；丢弃保持绑定。")) ECombatItemSharing Sharing = ECombatItemSharing::Public;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="死亡掉落", ToolTip="启用后单位死亡时此物品落到脚下；默认随单位保留并继续计算冷却。")) bool bDropOnDeath = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="允许主动丢弃", ToolTip="关闭后禁止拖到地面或通过菜单丢弃；死亡掉落仍由死亡掉落选项独立控制。")) bool bCanDrop = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="允许进入背包", ToolTip="关闭后只能放入六个装备槽；装备槽全满时无法拾取，不能交换到三个背包槽。")) bool bCanEnterBackpack = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item|Aura", meta=(DisplayName="携带光环效果", ToolTip="可选的光环子 Modifier，装备生效时维护；背包、地面、死亡和重新装备等待期间停止光环并移除子效果。")) TObjectPtr<UCombatModifierData> AuraModifier;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item|Aura", meta=(DisplayName="光环半径", ToolTip="以持有单位为中心的光环作用半径，单位厘米。", Units="cm", ClampMin="0")) float AuraRadius = 600.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Item|Aura", meta=(DisplayName="光环目标规则", ToolTip="光环目标的队伍与状态筛选；默认作用于友军和自身。")) FCombatTargetingRules AuraTargeting;

	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;
	/** 服务器授予和 Editor 验证共用：拒绝无法一致消费、无限被动不合法以及缺少 AbilityData 的定义。 */
	bool ValidateRuntime(FString& OutError) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
