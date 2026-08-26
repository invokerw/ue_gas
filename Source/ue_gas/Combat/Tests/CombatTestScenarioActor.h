#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatTestScenarioActor.generated.h"

class ACombatUnitCharacter;
class UCombatProjectileData;

/** 为 L_CombatTest 提供可重复生成和清理的双队战斗场景入口。 */
UCLASS(Blueprintable)
class UE_GAS_API ACombatTestScenarioActor : public AActor
{
	GENERATED_BODY()

public:
	/** 配置测试 Actor 的默认 UnitClass，并关闭运行时 Tick。 */
	ACombatTestScenarioActor();

	/** 在 Authority 上生成 Team 1/2 各一名 Unit，并输出 M4/M5 场景日志。 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Combat|Test")
	void SpawnScenario();

	/** 销毁当前场景 Actor 生成的全部 Unit，并清空跟踪数组。 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Combat|Test")
	void DestroyScenario();

	/** 先清理旧 Unit，再按当前配置重新生成双队场景。 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Combat|Test")
	void RespawnScenario();

	/** 返回仍然有效的已生成 Unit 数量。 */
	UFUNCTION(BlueprintPure, Category="Combat|Test")
	int32 GetSpawnedUnitCount() const;

	/** BeginPlay 时是否自动创建双队场景。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Test")
	bool bAutoSpawnOnBeginPlay = true;

	/** 场景生成使用的 Combat Unit 类。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Test")
	TSubclassOf<ACombatUnitCharacter> UnitClass;

	/** Team 1 相对测试 Actor 的生成偏移。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Test")
	FVector TeamOneOffset = FVector(50.0, 0.0, 288.0);

	/** Team 2 相对测试 Actor 的生成偏移。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Test")
	FVector TeamTwoOffset = FVector(300.0, 0.0, 288.0);

protected:
	/** 根据 bAutoSpawnOnBeginPlay 在游戏 World 中建立测试场景。 */
	virtual void BeginPlay() override;
	/** Actor 结束前销毁其生成的全部 Unit。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 在 Authority 上按相对偏移生成单个 Unit 并设置 TeamId。 */
	ACombatUnitCharacter* SpawnUnit(const FVector& RelativeOffset, uint8 TeamValue);
	/** 等待 Character 落地后发出 M4 AttackTarget Order，并输出场景验收日志。 */
	void StartM4AttackScenario();
	/** 生成一枚追踪测试弹体，并检查 Projectile、Thinker 与 Motion 运行时就绪。 */
	void StartM5ProjectileScenario();

	/** 当前由本 Actor 生成并负责销毁的 Unit。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ACombatUnitCharacter>> SpawnedUnits;
	/** 延迟到单位落地后再发攻击 Order，避免使用生成帧中的空中导航位置。 */
	FTimerHandle M4AttackScenarioTimer;
	/** M5 场景使用的瞬态 Projectile 定义，保证弹体飞行期间不会被 GC。 */
	UPROPERTY(Transient)
	TObjectPtr<UCombatProjectileData> ScenarioProjectileData;
	/** 当前 M5 场景弹体，重建场景时显式取消。 */
	FCombatProjectileHandle ScenarioProjectileHandle;
};
