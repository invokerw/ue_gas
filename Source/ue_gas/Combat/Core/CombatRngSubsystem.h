#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatRngSubsystem.generated.h"

/** 冻结 Combat RNG v1 的算法版本，供记录、发布契约和迁移工具共用。 */
struct UE_GAS_API FCombatRngPolicyV1
{
	/** keyed RNG 输入编码与哈希算法版本。 */
	static constexpr uint16 AlgorithmVersion = 1;
};

/** 由两个 64 位字段组成、与 UObject 地址无关的 RNG 主体身份。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatRngSubjectId
{
	GENERATED_BODY()

	/** 主体身份高 64 位。 */
	uint64 High = 0;
	/** 主体身份低 64 位。 */
	uint64 Low = 0;

	/** 比较主体身份的完整 128 位值。 */
	bool operator==(const FCombatRngSubjectId& Other) const { return High == Other.High && Low == Other.Low; }
};

/** 保存一次可复现随机判定的输入与结果；结合比赛种子可核对该次判定。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatRngRollRecord
{
	GENERATED_BODY()

	/** 生成概率结果所使用的数值公式版本。 */
	uint16 FormulaVersion = 1;
	/** keyed RNG 哈希算法版本。 */
	uint16 RngAlgorithmVersion = FCombatRngPolicyV1::AlgorithmVersion;
	/** 本次随机判定所属的根战斗事件。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|RNG") FCombatEventId RootEventId;
	/** 区分闪避、暴击、效果概率触发等用途，避免不同用途复用同一个随机取样。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|RNG") FGameplayTag DomainTag;
	/** 随机判定主体的稳定身份。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|RNG") FCombatRngSubjectId SubjectId;
	/** 同一事件、域和主体内的显式判定序号。 */
	uint32 Ordinal = 0;
	/** 哈希算法输出的原始 64 位值。 */
	uint64 RawBits = 0;
	/** 调用方提供的原始概率。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|RNG") float ChanceRaw = 0.0f;
	/** 按 Numeric Policy 限制后的概率。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|RNG") float ChanceClamped = 0.0f;
	/** 由 RawBits 映射得到的 [0, 1) 浮点值。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|RNG") float Roll = 0.0f;
	/** Roll 是否小于限制后的概率。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|RNG") bool bSuccess = false;
};

/**
 * 服务器上的可复现概率判定服务，用于闪避、暴击或效果触发。
 * 比赛种子、根事件、用途标签、主体身份及判定序号 Ordinal 共同决定结果；同一组输入总是得到同一随机值，不受其他调用顺序影响。
 * 它支持复核单次随机结果，不代表整个战斗世界可跨进程确定性重放；客户端不能调用 Roll 作权威判定。
 */
UCLASS()
class UE_GAS_API UCombatRngSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 初始化比赛种子；测试可随后注入固定值。 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * 按事件、用途、主体和序号生成概率判定，并写入 OutRecord；概率按统一规则限制到 [0,1]。
	 * 返回 true 表示生成了记录，是否触发应读 OutRecord.bSuccess；无效事件、用途标签或非服务器世界返回 false，保持输出参数原值。
	 */
	bool Roll(
		const FCombatEventId& RootEventId,
		const FGameplayTag& DomainTag,
		const FCombatRngSubjectId& SubjectId,
		uint32 Ordinal,
		float Chance,
		FCombatRngRollRecord& OutRecord) const;

	/** 返回当前 World 使用的比赛种子。 */
	uint64 GetMatchSeed() const { return MatchSeed; }
	/** 仅供自动化注入固定比赛种子，以验证重放结果。 */
	void SetMatchSeedForTesting(uint64 InMatchSeed) { MatchSeed = InMatchSeed; }

	/** 按 Combat RNG v1 规则生成稳定的 64 位原始随机值。 */
	static uint64 CombatHash64V1(
		uint64 MatchSeed,
		const FCombatEventId& RootEventId,
		const FGameplayTag& DomainTag,
		const FCombatRngSubjectId& SubjectId,
		uint32 Ordinal);

private:
	/** 对字节序列执行 FNV-1a 64 位哈希。 */
	static uint64 Fnv1a64(const ANSICHAR* Bytes, int32 Length);
	/** 使用 SplitMix64 扩散组合后的输入位。 */
	static uint64 SplitMix64(uint64 Value);

	/** 当前比赛的随机种子；所有 roll 都显式派生自该值。 */
	uint64 MatchSeed = 0;
};
