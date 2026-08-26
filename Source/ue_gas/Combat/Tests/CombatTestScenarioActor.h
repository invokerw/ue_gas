#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CombatTestScenarioActor.generated.h"

class ACombatUnitCharacter;

/** 为 L_CombatTest 提供可重复生成和清理的双队战斗场景入口。 */
UCLASS(Blueprintable)
class UE_GAS_API ACombatTestScenarioActor : public AActor
{
	GENERATED_BODY()

public:
	/** 配置测试 Actor 的默认 UnitClass，并关闭运行时 Tick。 */
	ACombatTestScenarioActor();

	/** 在 Authority 上生成 Team 1/2 各一名 Unit，并输出 M3 场景状态日志。 */
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
	FVector TeamOneOffset = FVector(-300.0, 0.0, 100.0);

	/** Team 2 相对测试 Actor 的生成偏移。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Test")
	FVector TeamTwoOffset = FVector(300.0, 0.0, 100.0);

protected:
	/** 根据 bAutoSpawnOnBeginPlay 在游戏 World 中建立测试场景。 */
	virtual void BeginPlay() override;
	/** Actor 结束前销毁其生成的全部 Unit。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 在 Authority 上按相对偏移生成单个 Unit 并设置 TeamId。 */
	ACombatUnitCharacter* SpawnUnit(const FVector& RelativeOffset, uint8 TeamValue);

	/** 当前由本 Actor 生成并负责销毁的 Unit。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ACombatUnitCharacter>> SpawnedUnits;
};
