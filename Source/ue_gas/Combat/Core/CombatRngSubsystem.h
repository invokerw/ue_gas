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

/** 保存一次 keyed roll 的完整输入与结果，供日志和重放校验。 */
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
	/** 区分闪避、暴击、Modifier proc 等随机域。 */
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
 * 当前 World 的确定性 keyed RNG 服务。
 * 每次取样由 MatchSeed、域、稳定对象身份和 RollIndex 独立决定，不依赖其他系统的调用顺序；服务器结果可通过同一键重放和诊断，客户端不得用它替代权威判定。
 */
UCLASS()
class UE_GAS_API UCombatRngSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 初始化比赛种子；测试可随后注入固定值。 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * 使用显式事件、随机域、主体和序号执行一次确定性概率判定。
	 * @return 输入有效并成功生成记录时返回 true；无效事件或标签返回 false。
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
