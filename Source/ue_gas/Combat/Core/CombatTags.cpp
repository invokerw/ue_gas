#include "Combat/Core/CombatTags.h"

namespace CombatTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Alive, "State.Alive", "单位处于存活状态")
	UE_DEFINE_GAMEPLAY_TAG(State_Dying, "State.Dying")
	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead")
	UE_DEFINE_GAMEPLAY_TAG(State_Respawning, "State.Respawning")
	UE_DEFINE_GAMEPLAY_TAG(State_Stunned, "State.Stunned")
	UE_DEFINE_GAMEPLAY_TAG(State_Silenced, "State.Silenced")
	UE_DEFINE_GAMEPLAY_TAG(State_Rooted, "State.Rooted")
	UE_DEFINE_GAMEPLAY_TAG(State_Disarmed, "State.Disarmed")
	UE_DEFINE_GAMEPLAY_TAG(State_Hexed, "State.Hexed")
	UE_DEFINE_GAMEPLAY_TAG(State_Invisible, "State.Invisible")
	UE_DEFINE_GAMEPLAY_TAG(State_Invulnerable, "State.Invulnerable")
	UE_DEFINE_GAMEPLAY_TAG(State_OutOfGame, "State.OutOfGame")
	UE_DEFINE_GAMEPLAY_TAG(State_MagicImmune, "State.MagicImmune")
	UE_DEFINE_GAMEPLAY_TAG(State_Untargetable, "State.Untargetable")
	UE_DEFINE_GAMEPLAY_TAG(State_NoUnitCollision, "State.NoUnitCollision")
	UE_DEFINE_GAMEPLAY_TAG(State_NoHealthBar, "State.NoHealthBar")
	UE_DEFINE_GAMEPLAY_TAG(State_Frozen, "State.Frozen")

	UE_DEFINE_GAMEPLAY_TAG(Ability_Behavior_NoTarget, "Ability.Behavior.NoTarget")
	UE_DEFINE_GAMEPLAY_TAG(Ability_Behavior_UnitTarget, "Ability.Behavior.UnitTarget")
	UE_DEFINE_GAMEPLAY_TAG(Ability_Behavior_PointTarget, "Ability.Behavior.PointTarget")
	UE_DEFINE_GAMEPLAY_TAG(Ability_Behavior_Passive, "Ability.Behavior.Passive")
	UE_DEFINE_GAMEPLAY_TAG(Ability_Behavior_Channelled, "Ability.Behavior.Channelled")
	UE_DEFINE_GAMEPLAY_TAG(Ability_Behavior_AoE, "Ability.Behavior.AoE")
	UE_DEFINE_GAMEPLAY_TAG(Ability_Behavior_Attack, "Ability.Behavior.Attack")
	UE_DEFINE_GAMEPLAY_TAG(Ability_Behavior_AutoCast, "Ability.Behavior.AutoCast")
	UE_DEFINE_GAMEPLAY_TAG(Ability_Behavior_IgnoreSilence, "Ability.Behavior.IgnoreSilence")
	UE_DEFINE_GAMEPLAY_TAG(Ability_Behavior_IgnoreMagicImmune, "Ability.Behavior.IgnoreMagicImmune")
	UE_DEFINE_GAMEPLAY_TAG(Ability_Behavior_IgnoreUntargetable, "Ability.Behavior.IgnoreUntargetable")

	UE_DEFINE_GAMEPLAY_TAG(TargetTeam_None, "TargetTeam.None")
	UE_DEFINE_GAMEPLAY_TAG(TargetTeam_Enemy, "TargetTeam.Enemy")
	UE_DEFINE_GAMEPLAY_TAG(TargetTeam_Friendly, "TargetTeam.Friendly")
	UE_DEFINE_GAMEPLAY_TAG(TargetTeam_Both, "TargetTeam.Both")

	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_Physical, "Damage.Type.Physical")
	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_Magical, "Damage.Type.Magical")
	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_Pure, "Damage.Type.Pure")
	UE_DEFINE_GAMEPLAY_TAG(Damage_Flag_BypassMagicImmune, "Damage.Flag.BypassMagicImmune")
	UE_DEFINE_GAMEPLAY_TAG(Damage_Flag_HPLoss, "Damage.Flag.HPLoss")
	UE_DEFINE_GAMEPLAY_TAG(Damage_Flag_NoSpellAmplification, "Damage.Flag.NoSpellAmplification")
	UE_DEFINE_GAMEPLAY_TAG(Damage_Flag_Reflection, "Damage.Flag.Reflection")
	UE_DEFINE_GAMEPLAY_TAG(Damage_Flag_NoLifesteal, "Damage.Flag.NoLifesteal")
	UE_DEFINE_GAMEPLAY_TAG(Damage_Flag_NoCrit, "Damage.Flag.NoCrit")
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage_Final, "Data.Damage.Final")
	UE_DEFINE_GAMEPLAY_TAG(Data_Heal_Final, "Data.Heal.Final")
	UE_DEFINE_GAMEPLAY_TAG(Cue_Combat, "Cue.Combat")

	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_DamageApplied, "Event.Combat.DamageApplied")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_HealApplied, "Event.Combat.HealApplied")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_ModifierApplied, "Event.Combat.ModifierApplied")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_ModifierRemoved, "Event.Combat.ModifierRemoved")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_UnitDeath, "Event.Combat.UnitDeath")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_UnitRespawned, "Event.Combat.UnitRespawned")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_TeamChanged, "Event.Combat.TeamChanged")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_AbilityGranted, "Event.Combat.AbilityGranted")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_AbilityRemoved, "Event.Combat.AbilityRemoved")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_AbilityLevelChanged, "Event.Combat.AbilityLevelChanged")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_AutoCastChanged, "Event.Combat.AutoCastChanged")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_AbilityCastStarted, "Event.Combat.AbilityCastStarted")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_AbilitySpellStarted, "Event.Combat.AbilitySpellStarted")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_AbilityChannelEnded, "Event.Combat.AbilityChannelEnded")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_AbilityOrderReleased, "Event.Combat.AbilityOrderReleased")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_AbilityInterrupted, "Event.Combat.AbilityInterrupted")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_AbilityEnded, "Event.Combat.AbilityEnded")
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_AbilityActionFailed, "Event.Combat.AbilityActionFailed")

	UE_DEFINE_GAMEPLAY_TAG(Failure_Authority, "Combat.Failure.Authority")
	UE_DEFINE_GAMEPLAY_TAG(Failure_InvalidNumber, "Combat.Failure.InvalidNumber")
	UE_DEFINE_GAMEPLAY_TAG(Failure_ActionUnsupported, "Combat.Failure.ActionUnsupported")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_Invalid, "Combat.Failure.Target.Invalid")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_TeamInvalid, "Combat.Failure.Target.TeamInvalid")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_SelfNotAllowed, "Combat.Failure.Target.SelfNotAllowed")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_FriendlyNotAllowed, "Combat.Failure.Target.FriendlyNotAllowed")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_HostileNotAllowed, "Combat.Failure.Target.HostileNotAllowed")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_NeutralNotAllowed, "Combat.Failure.Target.NeutralNotAllowed")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_Dying, "Combat.Failure.Target.Dying")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_Dead, "Combat.Failure.Target.Dead")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_Respawning, "Combat.Failure.Target.Respawning")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_Untargetable, "Combat.Failure.Target.Untargetable")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_OutOfGame, "Combat.Failure.Target.OutOfGame")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_Invulnerable, "Combat.Failure.Target.Invulnerable")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_MagicImmune, "Combat.Failure.Target.MagicImmune")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_OutOfRange, "Combat.Failure.Target.OutOfRange")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_LocationInvalid, "Combat.Failure.Target.LocationInvalid")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Target_LineOfSightBlocked, "Combat.Failure.Target.LineOfSightBlocked")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Life_NotAlive, "Combat.Failure.Life.NotAlive")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Life_InvalidTransition, "Combat.Failure.Life.InvalidTransition")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Ability_DuplicateDefinition, "Combat.Failure.Ability.DuplicateDefinition")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Ability_InvalidLevel, "Combat.Failure.Ability.InvalidLevel")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Ability_NotGranted, "Combat.Failure.Ability.NotGranted")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Ability_InvalidTargetData, "Combat.Failure.Ability.InvalidTargetData")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Ability_Cost, "Combat.Failure.Ability.Cost")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Ability_Cooldown, "Combat.Failure.Ability.Cooldown")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Ability_CommitFailed, "Combat.Failure.Ability.CommitFailed")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Ability_AlreadyActive, "Combat.Failure.Ability.AlreadyActive")
	UE_DEFINE_GAMEPLAY_TAG(Failure_Ability_UnitStateBlocked, "Combat.Failure.Ability.UnitStateBlocked")

	UE_DEFINE_GAMEPLAY_TAG(Order_Failure_Cancelled, "Order.Failure.Cancelled")
	UE_DEFINE_GAMEPLAY_TAG(Order_Failure_QueueFull, "Order.Failure.QueueFull")
	UE_DEFINE_GAMEPLAY_TAG(Order_Failure_PathFailed, "Order.Failure.PathFailed")
	UE_DEFINE_GAMEPLAY_TAG(Order_Failure_Blocked, "Order.Failure.Blocked")
	UE_DEFINE_GAMEPLAY_TAG(Order_Failure_AbilityRejected, "Order.Failure.AbilityRejected")
	UE_DEFINE_GAMEPLAY_TAG(Order_Failure_UnitStateBlocked, "Order.Failure.UnitStateBlocked")

	UE_DEFINE_GAMEPLAY_TAG(RNG_Attack_Evasion, "Combat.RNG.Attack.Evasion")
	UE_DEFINE_GAMEPLAY_TAG(RNG_Attack_Crit, "Combat.RNG.Attack.Crit")
	UE_DEFINE_GAMEPLAY_TAG(RNG_Modifier_Proc, "Combat.RNG.Modifier.Proc")

	bool TryGetSingleDamageType(const FGameplayTagContainer& Tags, FGameplayTag& OutDamageType)
	{
		OutDamageType = FGameplayTag();
		const FGameplayTag DamageTypes[] = { Damage_Type_Physical, Damage_Type_Magical, Damage_Type_Pure };
		for (const FGameplayTag& DamageType : DamageTypes)
		{
			if (!Tags.HasTagExact(DamageType))
			{
				continue;
			}
			if (OutDamageType.IsValid())
			{
				OutDamageType = FGameplayTag();
				return false;
			}
			OutDamageType = DamageType;
		}
		return OutDamageType.IsValid();
	}
}
