#include "Combat/Defense/DefenseGuardian.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

ADefenseGuardianHero::ADefenseGuardianHero(const FObjectInitializer& Init):Super(Init)
{
	ShieldMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GuardianShield")); ShieldMesh->SetupAttachment(GetRootComponent());
	WeaponMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GuardianWeapon")); WeaponMesh->SetupAttachment(GetRootComponent());
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	for(auto* Part:{ShieldMesh.Get(),WeaponMesh.Get()}) { Part->SetStaticMesh(Cube.Object); Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); Part->SetCanEverAffectNavigation(false); }
	ShieldMesh->SetRelativeLocation(FVector(25,-52,0)); ShieldMesh->SetRelativeScale3D(FVector(.16,.7,1.0));
	WeaponMesh->SetRelativeLocation(FVector(25,52,5)); WeaponMesh->SetRelativeScale3D(FVector(.1,.1,1.2));
}
void UDefenseGuardianAreaAbility::OnGiveAbility(const FGameplayAbilityActorInfo* Info,const FGameplayAbilitySpec& Spec)
{
	if(const auto* CDO=GetClass()->GetDefaultObject<UDefenseGuardianAreaAbility>();CDO&&CDO!=this)
	{ ControlModifier=CDO->ControlModifier; BossStaggerModifier=CDO->BossStaggerModifier; BossDefinition=CDO->BossDefinition; }
	Super::OnGiveAbility(Info,Spec);
}
void UDefenseGuardianAreaAbility::ReceiveSpellStart_Implementation(const FCombatAbilityActivationContext& C)
{
	auto* Hero=C.Caster.Get(); const auto* Data=GetAbilityData();
	if(!IsValid(Hero)||!Hero->HasAuthority()||Hero->GetNetMode()==NM_Client||!Data||Hero->GetLifeGeneration()!=C.CasterLifeGeneration) return;
	const float Radius=GetSpecialValue(TEXT("radius")),Damage=GetSpecialValue(TEXT("damage")),HalfAngle=GetSpecialValue(TEXT("half_angle"));
	if(!FMath::IsFinite(Radius)||Radius<=0||!FMath::IsFinite(Damage)||Damage<0||!FMath::IsFinite(HalfAngle)||HalfAngle<=0||HalfAngle>180) return;
	const FVector Origin=Hero->GetActorLocation(); FVector Direction=(C.TargetLocation-Origin).GetSafeNormal2D();
	if(Direction.IsNearlyZero()) Direction=Hero->GetActorForwardVector().GetSafeNormal2D();
	auto Rules=Data->TargetingRules; Rules.TargetTeamTag=CombatTags::TargetTeam_Enemy; Rules.bAllowSelf=false;
	const auto Targets=Hero->GetWorld()->GetSubsystem<UCombatTargetingSubsystem>()->QueryUnitsInRadius(Hero,Origin,Radius,Rules);
	for(auto* Target:Targets)
	{
		if(!IsValid(Hero)||Hero->GetLifeState()!=ECombatLifeState::Alive||Hero->GetLifeGeneration()!=C.CasterLifeGeneration) break;
		if(!IsValid(Target)) continue;
		const FVector ToTarget=(Target->GetActorLocation()-Origin).GetSafeNormal2D();
		if(HalfAngle<180&&!ToTarget.IsNearlyZero()&&FVector::DotProduct(Direction,ToTarget)<FMath::Cos(FMath::DegreesToRadians(HalfAngle))) continue;
		const uint32 Life=Target->GetLifeGeneration();
		FCombatDamageRequest Hit; Hit.Source=Hero; Hit.Target=Target; Hit.Amount=Damage; Hit.DamageType=ECombatDamageType::Physical;
		Hit.ParentEvent=C.EventContext; Hit.SourceContext=C.SourceContext;
		Hero->GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Hit);
		if(!IsValid(Hero)||Hero->GetLifeState()!=ECombatLifeState::Alive||Hero->GetLifeGeneration()!=C.CasterLifeGeneration) break;
		if(!IsValid(Target)||Target->GetLifeState()!=ECombatLifeState::Alive||Target->GetLifeGeneration()!=Life) continue;
		const bool Boss=Target->GetUnitDefinitionId()==BossDefinition;
		auto* Effect=Boss?BossStaggerModifier.Get():ControlModifier.Get(); if(!Effect) continue;
		FCombatModifierApplyRequest R; R.ModifierData=Effect; R.Source=Boss?Target:Hero; R.SourceContext=C.SourceContext;
		if(Boss)
		{
			// 以Boss为来源合并全队层数，首次层数的截止不被后续震地延长。
			TArray<FCombatModifierView> Views; Target->GetCombatModifierComponent()->BuildModifierViews(Views);
			for(const auto& V:Views) if(V.DefinitionId==Effect->GetPrimaryAssetId())
				if(const auto* Runtime=Target->GetCombatModifierComponent()->FindRuntime(V.Handle))
				{
					const double Remaining=Runtime->GetExpireAt()-Hero->GetWorld()->GetTimeSeconds();
					if(Remaining>0) R.DurationOverride=Remaining; else Target->GetCombatModifierComponent()->RemoveModifier(V.Handle);
				}
		}
		Target->GetCombatModifierComponent()->ApplyModifier(R);
	}
}
void UDefenseBossStaggerRuntime::OnCreated_Implementation() { ResolveStagger(); }
void UDefenseBossStaggerRuntime::OnRefreshed_Implementation() { ResolveStagger(); }
void UDefenseBossStaggerRuntime::ResolveStagger()
{
	auto* Unit=GetTargetUnit(); if(!IsValid(Unit)||!Unit->HasAuthority()||bTriggered||!StunData||!RecoveryData) return;
	TArray<FCombatModifierView> Views; auto* Modifiers=Unit->GetCombatModifierComponent(); Modifiers->BuildModifierViews(Views);
	if(Views.ContainsByPredicate([&](const auto& V){return V.DefinitionId==RecoveryData->GetPrimaryAssetId();})) { RequestRemoveSelf(); return; }
	const int32 Threshold=FMath::Clamp(FMath::RoundToInt(GetRuntimeParameter(TEXT("stagger_threshold"),4)),1,32);
	if(GetStackCount()<Threshold) return;
	bTriggered=true;
	for(auto* Effect:{RecoveryData.Get(),StunData.Get()})
	{ FCombatModifierApplyRequest R; R.Source=Unit; R.ModifierData=Effect; R.SourceContext=GetSourceContext(); Modifiers->ApplyModifier(R); }
	RequestRemoveSelf();
}
bool UDefenseGuardianPassiveRuntime::AcceptAttack(const FCombatDamageEvent& E,TArray<FCombatEventId>& Recent)
{
	if(E.SourceContext.DirectSourceType!=ECombatDirectSourceType::Attack||E.Flags.HasTagExact(CombatTags::Damage_Flag_Reflection)
		||E.AppliedAmount+E.AbsorbedAmount<=0||!E.Context.RootEventId.IsValid()||Recent.Contains(E.Context.RootEventId)) return false;
	Recent.Add(E.Context.RootEventId); if(Recent.Num()>32) Recent.RemoveAt(0); return true;
}
void UDefenseGuardianPassiveRuntime::OnPostTakeDamage_Implementation(const FCombatDamageEvent& E)
{
	auto* Unit=GetTargetUnit(); if(!IsValid(Unit)||Unit->GetLifeState()!=ECombatLifeState::Alive||E.Source==Unit||bArmed||Unit->GetWorld()->GetTimeSeconds()<ReadyAt||!AcceptAttack(E,IncomingRoots)) return;
	const int32 Threshold=FMath::Clamp(FMath::RoundToInt(GetRuntimeParameter(TEXT("hits_required"),3)),1,32);
	if(++ReceivedHits>=Threshold) { ReceivedHits=0; bArmed=true; }
}
void UDefenseGuardianPassiveRuntime::OnPostDealDamage_Implementation(const FCombatDamageEvent& E)
{
	auto* Unit=GetTargetUnit(); if(!IsValid(Unit)||Unit->GetLifeState()!=ECombatLifeState::Alive||!ShieldData||!bArmed||E.Target==Unit
		||Unit->GetWorld()->GetTimeSeconds()<ReadyAt||!AcceptAttack(E,OutgoingRoots)) return;
	bArmed=false; ReceivedHits=0; ReadyAt=Unit->GetWorld()->GetTimeSeconds()+FMath::Max(0.f,GetRuntimeParameter(TEXT("internal_cooldown")));
	FCombatModifierApplyRequest R; R.ModifierData=ShieldData; R.Source=Unit; R.SourceContext=GetSourceContext();
	Unit->GetCombatModifierComponent()->ApplyModifier(R);
}
