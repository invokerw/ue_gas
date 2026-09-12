#include "Combat/Core/CombatRngSubsystem.h"

#include "Combat/Core/CombatNumericPolicy.h"
#include "Engine/World.h"

void UCombatRngSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const uint64 WorldHash = GetTypeHash(GetWorld());
	MatchSeed = SplitMix64(FPlatformTime::Cycles64() ^ WorldHash);
	if (MatchSeed == 0)
	{
		MatchSeed = 1;
	}
}

bool UCombatRngSubsystem::Roll(
	const FCombatEventId& RootEventId,
	const FGameplayTag& DomainTag,
	const FCombatRngSubjectId& SubjectId,
	const uint32 Ordinal,
	const float Chance,
	FCombatRngRollRecord& OutRecord) const
{
	if (!RootEventId.IsValid() || !DomainTag.IsValid() || !GetWorld() || GetWorld()->GetNetMode() == NM_Client)
	{
		return false;
	}

	OutRecord = FCombatRngRollRecord();
	OutRecord.RootEventId = RootEventId;
	OutRecord.DomainTag = DomainTag;
	OutRecord.SubjectId = SubjectId;
	OutRecord.Ordinal = Ordinal;
	OutRecord.ChanceRaw = Chance;
	OutRecord.ChanceClamped = FCombatNumericPolicyV1::ClampChance(Chance);
	OutRecord.RawBits = CombatHash64V1(MatchSeed, RootEventId, DomainTag, SubjectId, Ordinal);
	// 使用高 24 位映射到 [0,1)，与 float 有效精度匹配并保证跨平台重放。
	const uint32 Top24Bits = static_cast<uint32>(OutRecord.RawBits >> 40);
	OutRecord.Roll = static_cast<float>(Top24Bits) / 16777216.0f;
	OutRecord.bSuccess = OutRecord.Roll < OutRecord.ChanceClamped;
	return true;
}

uint64 UCombatRngSubsystem::CombatHash64V1(
	const uint64 InMatchSeed,
	const FCombatEventId& RootEventId,
	const FGameplayTag& DomainTag,
	const FCombatRngSubjectId& SubjectId,
	const uint32 Ordinal)
{
	// Tag 以 UTF-8 稳定文本参与哈希，避免依赖进程内 GameplayTag 索引。
	const FString DomainString = DomainTag.ToString();
	const FTCHARToUTF8 DomainUtf8(*DomainString);
	const uint64 DomainHash = Fnv1a64(DomainUtf8.Get(), DomainUtf8.Length());

	uint64 State = 0xC0B47A11D06A5EEDull;
	State = SplitMix64(State ^ InMatchSeed);
	State = SplitMix64(State ^ RootEventId.Sequence);
	State = SplitMix64(State ^ DomainHash);
	State = SplitMix64(State ^ SubjectId.High);
	State = SplitMix64(State ^ SubjectId.Low);
	State = SplitMix64(State ^ static_cast<uint64>(Ordinal));
	return State;
}

uint64 UCombatRngSubsystem::Fnv1a64(const ANSICHAR* Bytes, const int32 Length)
{
	uint64 Hash = 0xcbf29ce484222325ull;
	for (int32 Index = 0; Index < Length; ++Index)
	{
		Hash ^= static_cast<uint8>(Bytes[Index]);
		Hash *= 0x100000001b3ull;
	}
	return Hash;
}

uint64 UCombatRngSubsystem::SplitMix64(const uint64 Value)
{
	uint64 Z = Value + 0x9e3779b97f4a7c15ull;
	Z = (Z ^ (Z >> 30)) * 0xbf58476d1ce4e5b9ull;
	Z = (Z ^ (Z >> 27)) * 0x94d049bb133111ebull;
	return Z ^ (Z >> 31);
}
