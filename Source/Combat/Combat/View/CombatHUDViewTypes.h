#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "Combat/Core/CombatTypes.h"
#include "Combat/Items/CombatItemTypes.h"
#include "CombatHUDViewTypes.generated.h"

/** 一个已授予技能的拥有者展示数据；时间窗来自已提交的服务器冷却，不参与技能判定。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatHUDAbilityView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="技能句柄", ToolTip="用于匹配拥有者复制的技能记录；不是新的施法入口。"))
	FGameplayAbilitySpecHandle SpecHandle;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="技能定义", ToolTip="本地解析名称与图标的稳定定义 ID。"))
	FPrimaryAssetId DefinitionId;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="技能等级", ToolTip="服务器 AbilitySpec 的当前等级。"))
	int32 Level = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="最大等级", ToolTip="定义允许的最高技能等级，用于显示等级刻度。"))
	int32 MaxLevel = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="法力消耗", ToolTip="当前等级的配置费用；实际提交仍由服务器重新校验。"))
	float ManaCost = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="冷却结束时间", ToolTip="已提交冷却的服务器绝对游戏时间；0 表示没有活动冷却。", Units="s"))
	double CooldownEndTime = 0.0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="已提交冷却时长", ToolTip="开始冷却时冻结的时长；后续冷却缩减变化不会重算。", Units="s"))
	float CooldownDuration = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="忽略沉默", ToolTip="该技能的行为配置允许忽略沉默；仅用于阻断状态展示。"))
	bool bIgnoreSilence = false;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="技能槽切换自动施法", ToolTip="为 true 时，该槽快捷键只请求服务器切换 AutoCast，不创建施法命令。"))
	bool bUsesAutoCastToggleInput = false;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="自动施法已开启", ToolTip="服务器权威 AutoCast 状态；只用于拥有者显示，攻击时仍由服务器重新检查。"))
	bool bAutoCastEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="可以升级", ToolTip="当前拥有者是否有技能点且英雄等级允许提升该技能；点击仍需服务器重新校验。"))
	bool bCanUpgrade = false;

	/** 比较完整显示内容，避免未变化快照触发复制。 */
	bool operator==(const FCombatHUDAbilityView& Other) const;
};

/** 仅复制给单位拥有者的 HUD 补充快照；生命/法力及 Buff 继续使用现有公共 View。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatHUDOwnerView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="单位定义", ToolTip="与公共 View 匹配，防止显示另一单位的技能和属性。"))
	FPrimaryAssetId UnitDefinitionId;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="生命代次", ToolTip="与公共 View 同代次时才显示该快照；0 表示尚未就绪。"))
	int64 LifeGeneration = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="力量", ToolTip="服务器 ASC 当前聚合力量三围。"))
	float Strength = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="敏捷", ToolTip="服务器 ASC 当前聚合敏捷三围。"))
	float Agility = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="智力", ToolTip="服务器 ASC 当前聚合智力三围。"))
	float Intelligence = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="主属性", ToolTip="服务器 UnitData 选择的主属性；每点主属性额外增加攻击力。"))
	ECombatPrimaryAttribute PrimaryAttribute = ECombatPrimaryAttribute::Strength;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="当前生命值", ToolTip="服务器 ASC 当前聚合生命值；公共 Unit View 也提供该字段。"))
	float Health = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="最大生命值", ToolTip="服务器 ASC 当前聚合最大生命值；公共 Unit View 也提供该字段。"))
	float MaxHealth = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="当前法力值", ToolTip="服务器 ASC 当前聚合法力值；公共 Unit View 也提供该字段。"))
	float Mana = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="最大法力值", ToolTip="服务器 ASC 当前聚合最大法力值；公共 Unit View 也提供该字段。"))
	float MaxMana = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="攻击力", ToolTip="服务器 ASC 当前聚合后的普通攻击基础伤害。"))
	float AttackDamage = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="护甲", ToolTip="服务器 ASC 当前聚合护甲，允许负值。"))
	float Armor = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="魔法抗性", ToolTip="服务器 ASC 当前抗性比例；0.25 显示为 25%。"))
	float MagicResist = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="移动速度", ToolTip="服务器 ASC 当前聚合地面移速，单位厘米每秒。", Units="cm/s"))
	float MoveSpeed = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="生命恢复速率", ToolTip="服务器 ASC 每秒生命恢复属性；死亡时界面显示暂停。"))
	float HealthRegen = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="法力恢复速率", ToolTip="服务器 ASC 每秒法力恢复属性；死亡时界面显示暂停。"))
	float ManaRegen = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="施法距离加成", ToolTip="服务器 ASC 当前施法边缘距离加成；允许负值，只用于本地范围预览。", Units="cm"))
	float CastRangeBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="攻击距离", ToolTip="服务器 ASC 当前普通攻击边缘距离；用于 AutoCast 悬停预览，实际攻击仍由服务器裁决。", Units="cm"))
	float AttackRange = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="闪避概率", ToolTip="服务器 ASC 当前普攻闪避概率，0.25 表示 25%。"))
	float Evasion = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="攻击速度", ToolTip="服务器 ASC 当前攻击速度属性。"))
	float AttackSpeed = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="基础攻击间隔", ToolTip="服务器 ASC 当前基础攻击间隔，单位为秒。", Units="s"))
	float BaseAttackTime = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="吸血比例", ToolTip="服务器 ASC 当前吸血比例；0.2 表示 20%。"))
	float LifestealPct = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="技能伤害增幅", ToolTip="服务器 ASC 当前非物理伤害增幅；0.25 表示 25%。"))
	float SpellAmplifyPct = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="冷却缩减比例", ToolTip="服务器 ASC 当前技能冷却缩减比例；0.25 表示 25%。"))
	float CooldownReductionPct = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="状态抗性比例", ToolTip="服务器 ASC 当前状态抗性比例；0.25 表示 25%。"))
	float StatusResistancePct = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="治疗来源增幅", ToolTip="服务器 ASC 当前施加治疗增幅；0.25 表示 25%。"))
	float HealAmplifyPct = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="受到治疗增幅", ToolTip="服务器 ASC 当前受到治疗增幅；0.25 表示 25%。"))
	float HealReceivedPct = 0.0f;
	/** 服务器权威成长快照；经验和技能点只向拥有者复制。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="英雄等级", ToolTip="当前主控单位的服务器权威英雄等级。"))
	int32 Level = 1;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="累计经验", ToolTip="从 1 级起累计的服务器权威经验。"))
	int64 Experience = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="当前等级经验", ToolTip="当前等级区间内已经积累的经验。"))
	int64 ExperienceIntoLevel = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="升级所需经验", ToolTip="升到下一级还需要的经验；满级为 0。"))
	int64 ExperienceToNextLevel = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="经验进度", ToolTip="当前等级经验环的 0 到 1 比例；满级为 1。"))
	float ExperienceProgress = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="未使用技能点", ToolTip="可用于提升技能等级的服务器权威技能点。"))
	int32 UnspentAbilityPoints = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="技能槽", ToolTip="最多四个可直接输入的技能；包含主动技能及可切换 AutoCast 的被动技能，客户端按自身输入所用的 AbilitySpec 顺序匹配。"))
	TArray<FCombatHUDAbilityView> Abilities;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="物品槽", ToolTip="固定九槽的拥有者物品快照；前六格装备，后三格背包。")) TArray<FCombatItemView> Items;
	UPROPERTY(BlueprintReadOnly, Category="Combat|HUD", meta=(DisplayName="背包修订", ToolTip="换位请求携带此值，拒绝操作已变化的旧背包快照。")) int32 InventoryRevision = 0;

	/** 比较完整快照，不把本地倒计时写入复制数据。 */
	bool operator==(const FCombatHUDOwnerView& Other) const;
};
