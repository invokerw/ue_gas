#include "Combat/Defense/DefenseOfferSampler.h"
#include "Combat/Core/CombatRngSubsystem.h"

bool FDefenseOfferSampler::IsEligible(const UDefenseAugmentData& Card, const UDefenseAugmentPoolData& Pool,
	const FPrimaryAssetId Hero, const int32 Wave, const TSet<FPrimaryAssetId>& Owned)
{
	if (Owned.Contains(Card.GetPrimaryAssetId()) || Card.EarliestWave > Wave+1
		|| (!Card.CompatibleHeroes.IsEmpty() && !Card.CompatibleHeroes.Contains(Hero))
		|| (Wave<0 && Card.Rarity!=EDefenseAugmentRarity::Basic)) return false;
	for (const auto& Id : Card.Prerequisites) if (!Owned.Contains(Id)) return false;
	for (const auto& Id : Card.MutuallyExclusive) if (Owned.Contains(Id)) return false;
	int32 FamilyCount=0;
	for (const UDefenseAugmentData* A : Pool.Augments)
	{
		if (!Owned.Contains(A->GetPrimaryAssetId())) continue;
		if (A->MutuallyExclusive.Contains(Card.GetPrimaryAssetId())) return false;
		if ((A->CompatibleHeroes.IsEmpty() || A->CompatibleHeroes.Contains(Hero)) && A->Family==Card.Family) ++FamilyCount;
	}
	return Card.Rarity!=EDefenseAugmentRarity::Final || (Wave>=2 && FamilyCount>=2);
}
TArray<FPrimaryAssetId> FDefenseOfferSampler::Draw(const UDefenseAugmentPoolData& Pool, const uint64 Seed,
	const int32 Seat, const int32 Wave, const int32 Revision, const FPrimaryAssetId Hero,
	const TSet<FPrimaryAssetId>& Owned, const TArray<FPrimaryAssetId>& Excluded, const bool bStrictExclude)
{
	TArray<const UDefenseAugmentData*> Legal,Preferred;
	TSet<EDefenseAugmentFamily> OwnedFamilies;
	for (const UDefenseAugmentData* A : Pool.Augments)
	{
		if (Owned.Contains(A->GetPrimaryAssetId()) && (A->CompatibleHeroes.IsEmpty() || A->CompatibleHeroes.Contains(Hero))) OwnedFamilies.Add(A->Family);
		if (!IsEligible(*A,Pool,Hero,Wave,Owned)) continue;
		Legal.Add(A); if (!Excluded.Contains(A->GetPrimaryAssetId())) Preferred.Add(A);
	}
	if (bStrictExclude || Preferred.Num()>=3) Legal=MoveTemp(Preferred);
	Legal.Sort([](const UDefenseAugmentData& A,const UDefenseAugmentData& B){return A.GetPrimaryAssetId().ToString()<B.GetPrimaryAssetId().ToString();});
	TArray<FPrimaryAssetId> Result;
	FCombatRngSubjectId Subject;
	Subject.High=static_cast<uint64>(Seat+1); Subject.Low=(uint64(uint32(Wave+1))<<32)|uint32(Revision);
	EDefenseAugmentFamily FirstFamily=EDefenseAugmentFamily::Frost;
	for (int32 Slot=0; Slot<3 && !Legal.IsEmpty(); ++Slot)
	{
		TArray<const UDefenseAugmentData*> SlotPool;
		for (const auto* A : Legal)
		{
			const bool bPreferred = Slot==0 ? ((Wave<=1) ? A->Rarity==EDefenseAugmentRarity::Basic : OwnedFamilies.Contains(A->Family))
				: Slot==1 ? A->Family!=FirstFamily : (A->Family==EDefenseAugmentFamily::Guardian || A->Family==EDefenseAugmentFamily::Economy);
			if (bPreferred) SlotPool.Add(A);
		}
		if (SlotPool.IsEmpty()) SlotPool=Legal;
		uint64 Total=0; for (const auto* A : SlotPool) Total+=A->Weight;
		uint64 Roll=UCombatRngSubsystem::HashSeededTextV1(Seed,TEXT("Run.AugmentOffer"),Subject,uint32(Slot))%Total;
		const UDefenseAugmentData* Selected=SlotPool.Last();
		for (const auto* A : SlotPool) { if (Roll<uint64(A->Weight)) { Selected=A; break; } Roll-=A->Weight; }
		Result.Add(Selected->GetPrimaryAssetId()); if(Slot==0) FirstFamily=Selected->Family;
		Legal.Remove(Selected);
	}
	return Result;
}
