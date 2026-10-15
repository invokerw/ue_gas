#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Combat/Defense/DefenseTypes.h"
#include "DefenseGameState.generated.h"
class UDefenseDirectorComponent;
/** 防守 GameState 只有只读快照；服务器 Director 是唯一写入者。 */
UCLASS()
class COMBAT_API ADefenseGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	/** 获取最新服务器快照；倒计时使用 Deadline 减服务器时间，不能驱动玩法。 */
	UFUNCTION(BlueprintPure, Category="Defense", meta=(DisplayName="防守对局快照", ToolTip="返回服务器已确认的阶段、核心生命和波次信息。"))
	const FDefenseRunView& GetRunView() const { return View; }
	EDefensePhase GetDefensePhase() const { return View.Phase; }
	int32 GetWaveIndex() const { return View.WaveIndex; }
	int64 GetPhaseGeneration() const { return View.PhaseGeneration; }
	ADefenseObjective* GetObjective() const { return View.Objective; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	friend class UDefenseDirectorComponent;
	/** 仅供同 World 的服务器 Director 发布；整个结构一次复制，避免跨阶段字段拼接。 */
	void Publish(const FDefenseRunView& Snapshot);
	UPROPERTY(Replicated) FDefenseRunView View;
};
