#pragma once
#include "Combat/Defense/DefenseAugmentData.h"
/** 无状态候选算法，输出只有稳定 ID；不改变 RNG 全局状态、金币或属性。 */
struct COMBAT_API FDefenseOfferSampler
{
	/** 相同种子/席位/波次/修订固定复现；重抽时 Excluded 严格排除，否则不足三张才放宽。 */
	static TArray<FPrimaryAssetId> Draw(const UDefenseAugmentPoolData& Pool, uint64 Seed, int32 Seat,
		int32 Wave, int32 Revision, FPrimaryAssetId Hero, const TSet<FPrimaryAssetId>& Owned,
		const TArray<FPrimaryAssetId>& Excluded, bool bStrictExclude);
	/** 校验当前英雄、波次、基础/终局、已持有、前置和双向互斥；不产生副作用。 */
	static bool IsEligible(const UDefenseAugmentData& Card, const UDefenseAugmentPoolData& Pool,
		FPrimaryAssetId Hero, int32 Wave, const TSet<FPrimaryAssetId>& Owned);
};
