#include "Combat/Log/CombatLogTypes.h"
#include "Combat/Log/CombatLogComponent.h"
#include "Combat/Core/CombatTags.h"

bool FCombatLogArray::Append(const FCombatLogEntry& Entry)
{
	if (Entry.Sequence <= 0 || (!Items.IsEmpty() && Entry.Sequence <= Items.Last().Sequence)) return false;
	if (Items.Num() >= CombatLogPresentation::MaxEntries)
	{
		Items.RemoveAt(0, Items.Num() - CombatLogPresentation::MaxEntries + 1, EAllowShrinking::No);
		MarkArrayDirty();
	}
	FCombatLogEntry& Added = Items.Add_GetRef(Entry);
	// 不继承另一个容器的复制身份；每个玩家拥有独立的 FastArray。
	Added.ReplicationID = INDEX_NONE;
	Added.ReplicationKey = INDEX_NONE;
	MarkItemDirty(Added);
	return true;
}

void FCombatLogArray::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (Owner) Owner->NotifyHistoryChanged();
}

bool FCombatLogFilter::Matches(const FCombatLogEntry& Entry, const double ServerTime) const
{
	if (!FMath::IsFinite(ServerTime) || !FMath::IsFinite(Entry.ServerTime)
		|| !FMath::IsFinite(Entry.Amount) || !FMath::IsFinite(TimeWindowSeconds)) return false;
	if (SourceActorId != 0 && SourceActorId != Entry.SourceActorId) return false;
	if (TargetActorId != 0 && TargetActorId != Entry.TargetActorId) return false;
	if (!bIncludeNonHeroes && !Entry.bSourceHero && !Entry.bTargetHero) return false;
	if (TimeWindowSeconds > 0.0 && Entry.ServerTime < ServerTime - TimeWindowSeconds) return false;
	switch (Entry.Category)
	{
	case ECombatLogCategory::Damage: return bDamage;
	case ECombatLogCategory::Healing: return bHealing;
	case ECombatLogCategory::Ability: return bAbility;
	case ECombatLogCategory::Status: return bStatus;
	default: return false;
	}
}

bool CombatLogPresentation::Classify(const FGameplayTag EventType, ECombatLogCategory& OutCategory)
{
	if (EventType == CombatTags::Event_Combat_DamageApplied) OutCategory = ECombatLogCategory::Damage;
	else if (EventType == CombatTags::Event_Combat_HealApplied) OutCategory = ECombatLogCategory::Healing;
	else if (EventType == CombatTags::Event_Combat_AbilitySpellStarted
		|| EventType == CombatTags::Event_Combat_AbilityInterrupted
		|| EventType == CombatTags::Event_Combat_AutoCastChanged
		|| EventType == CombatTags::Event_Combat_AbilitySpellBlocked) OutCategory = ECombatLogCategory::Ability;
	else if (EventType == CombatTags::Event_Combat_ModifierApplied
		|| EventType == CombatTags::Event_Combat_ModifierRemoved
		|| EventType == CombatTags::Event_Combat_UnitDeath
		|| EventType == CombatTags::Event_Combat_UnitRespawned) OutCategory = ECombatLogCategory::Status;
	else return false;
	return true;
}

FString CombatLogPresentation::FormatTimestamp(const double ServerTime)
{
	const double SafeTime = FMath::IsFinite(ServerTime) ? FMath::Clamp(ServerTime, 0.0, 31536000.0) : 0.0;
	const int64 Milliseconds = FMath::RoundToInt64(SafeTime * 1000.0);
	return FString::Printf(TEXT("[%02lld:%02lld.%03lld]"), Milliseconds / 60000, Milliseconds / 1000 % 60, Milliseconds % 1000);
}

FString CombatLogPresentation::BuildUnitOptionLabel(const FString& Name, const int32 ActorId,
	const bool bDuplicateName, const TMap<FString, int32>& ExistingOptions)
{
	const FString InstanceSuffix = FString::Printf(TEXT(" · #%d"), ActorId);
	FString Label = bDuplicateName ? Name + InstanceSuffix : Name;
	// “全部”与作者填写的带后缀名称同样占用标签，不能覆盖它们对应的实例映射。
	while (ExistingOptions.Contains(Label)) Label += InstanceSuffix;
	return Label;
}

namespace CombatLogText
{
	/** 名称可能来自内容作者，转义防止把名字误解释为颜色标签。 */
	FString Styled(const TCHAR* Style, FString Value)
	{
		Value.ReplaceInline(TEXT("&"), TEXT("&amp;"));
		Value.ReplaceInline(TEXT("<"), TEXT("&lt;"));
		Value.ReplaceInline(TEXT(">"), TEXT("&gt;"));
		Value.ReplaceInline(TEXT("\""), TEXT("&quot;"));
		return FString::Printf(TEXT("<%s>%s</>"), Style, *Value);
	}
	/** 保留小数伤害，去掉没有信息的尾零。 */
	FString Number(const float Value)
	{
		return FString::SanitizeFloat(FMath::IsFinite(Value) ? Value : 0.0f, 0);
	}
}

FString CombatLogPresentation::BuildRichText(const FCombatLogEntry& Entry, const FString& SourceName,
	const FString& TargetName, const FString& EffectName)
{
	using namespace CombatLogText;
	const FString Source = Styled(TEXT("source"), SourceName.IsEmpty() ? TEXT("未知来源") : SourceName);
	const FString Target = Styled(TEXT("target"), TargetName.IsEmpty() ? TEXT("未知目标") : TargetName);
	const FString Effect = Styled(TEXT("effect"), EffectName);
	FString Body;
	if (Entry.Category == ECombatLogCategory::Damage || Entry.Category == ECombatLogCategory::Healing)
	{
		const bool bHeal = Entry.Category == ECombatLogCategory::Healing;
		Body = Source + (EffectName.IsEmpty() ? TEXT("") : TEXT(" 使用了 ") + Effect)
			+ TEXT("，对 ") + Target + (bHeal ? TEXT(" 恢复 ") : TEXT(" 造成 "))
			+ Styled(bHeal ? TEXT("health") : TEXT("amount"), Number(Entry.Amount)) + (bHeal ? TEXT(" 点生命") : TEXT(" 点伤害"));
		if (Entry.bHasHealthChange && FMath::IsFinite(Entry.PreviousHealth) && FMath::IsFinite(Entry.NewHealth))
			Body += TEXT(" ") + Styled(TEXT("health"), TEXT("(") + Number(Entry.PreviousHealth) + TEXT(" → ") + Number(Entry.NewHealth) + TEXT(")"));
	}
	else if (Entry.EventType == CombatTags::Event_Combat_ModifierApplied)
	{
		Body = Target + TEXT(" 获得了来自 ") + Source + TEXT(" 的 ") + Effect + Styled(TEXT("status"), TEXT(" 效果"));
		if (Entry.Amount > 1.0f) Body += Styled(TEXT("status"), FString::Printf(TEXT("（%d 层）"), FMath::RoundToInt(Entry.Amount)));
	}
	else if (Entry.EventType == CombatTags::Event_Combat_ModifierRemoved)
		Body = Target + TEXT(" 身上的 ") + Effect + Styled(TEXT("status"), TEXT(" 效果结束"));
	else if (Entry.EventType == CombatTags::Event_Combat_UnitDeath)
		Body = Source + TEXT(" 击杀了 ") + Target;
	else if (Entry.EventType == CombatTags::Event_Combat_UnitRespawned)
		Body = Target + Styled(TEXT("health"), TEXT(" 已复活"));
	else if (Entry.EventType == CombatTags::Event_Combat_AutoCastChanged)
		Body = Source + (Entry.bAutoCastEnabled ? TEXT(" 开启了 ") : TEXT(" 关闭了 ")) + Effect + TEXT(" 的自动施法");
	else if (Entry.EventType == CombatTags::Event_Combat_AbilityInterrupted)
		Body = Source + TEXT(" 的 ") + Effect + Styled(TEXT("status"), TEXT(" 被中断"));
	else if (Entry.EventType == CombatTags::Event_Combat_AbilitySpellBlocked)
		Body = Source + TEXT(" 的 ") + Effect + Styled(TEXT("status"), TEXT(" 被技能格挡"));
	else
		Body = Source + (Entry.TargetActorId ? TEXT(" 对 ") + Target : TEXT("")) + TEXT(" 施放了 ") + Effect;
	return Styled(TEXT("time"), FormatTimestamp(Entry.ServerTime)) + TEXT(" ") + Body + TEXT("。");
}
