#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatTestScenarioActor.generated.h"

class ACombatUnitCharacter;
class UCombatModifierData;
class UCombatProjectileData;

/** 为 L_CombatTest 提供可重复生成和清理的双队战斗场景入口。 */
UCLASS(Blueprintable)
class UE_GAS_API ACombatTestScenarioActor : public AActor
{
	GENERATED_BODY()

public:
	/** 配置测试 Actor 的默认 UnitClass，并关闭运行时 Tick。 */
	ACombatTestScenarioActor();

	/** 在 Authority 上生成 Team 1/2 各一名 Unit，并输出 M4-M8 场景日志。 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Combat|Test", meta=(DisplayName="生成战斗测试场景", ToolTip="在 Authority 上生成 Team 1/2 测试单位并启动 M4-M8 自动场景链。"))
	void SpawnScenario();

	/** 销毁当前场景 Actor 生成的全部 Unit，并清空跟踪数组。 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Combat|Test", meta=(DisplayName="销毁战斗测试场景", ToolTip="销毁本场景 Actor 创建的全部战斗单位并清空跟踪状态。"))
	void DestroyScenario();

	/** 先清理旧 Unit，再按当前配置重新生成双队场景。 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Combat|Test", meta=(DisplayName="重建战斗测试场景", ToolTip="先清理旧场景，再按当前配置重新生成双队测试单位。"))
	void RespawnScenario();

	/** 返回仍然有效的已生成 Unit 数量。 */
	UFUNCTION(BlueprintPure, Category="Combat|Test", meta=(DisplayName="获取测试单位数", ToolTip="返回当前仍有效且由本场景 Actor 管理的战斗单位数量。"))
	int32 GetSpawnedUnitCount() const;

	/** BeginPlay 时是否自动创建双队场景。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Test", meta=(DisplayName="开始游戏时自动生成", ToolTip="启用后 Authority BeginPlay 会自动创建测试单位并启动场景链。"))
	bool bAutoSpawnOnBeginPlay = true;

	/** 场景生成使用的 Combat Unit 类。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Test", meta=(DisplayName="测试单位类", ToolTip="测试场景生成的 Combat Unit 类。"))
	TSubclassOf<ACombatUnitCharacter> UnitClass;

	/** Team 1 相对测试 Actor 的生成偏移。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Test", meta=(DisplayName="队伍一生成偏移", ToolTip="Team 1 单位相对测试场景 Actor 的生成位置。"))
	FVector TeamOneOffset = FVector(50.0, 0.0, 288.0);

	/** Team 2 相对测试 Actor 的生成偏移。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Test", meta=(DisplayName="队伍二生成偏移", ToolTip="Team 2 单位相对测试场景 Actor 的生成位置。"))
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
	/** 启动一条真实 Aura，并检查 M6 示例与扩展运行时就绪。 */
	void StartM6ContentScenario();
	/** 等待玩家连接后建立 Mixed/Minimal 矩阵，并输出 M7 网络、View、诊断与容量快照。 */
	void StartM7NetworkScenario();
	/** 带 -CombatM7ClientSmoke 的客户端向自己拥有的单位提交一个安全 Order 批次。 */
	void StartM7ClientRpcSmoke();
	/** 带 -CombatSAMMovementSmoke 时在服务器让 A 撞向静止 B，并记录 B 的权威净位移。 */
	void StartSamMovementConsistencyScenario();
	/** SAM 对撞稳定窗口结束后输出服务器 A/B 位置与静止 B 净位移。 */
	void FinishSamMovementConsistencyScenario();
	/** owning client 输出 Command Pawn、SimulatedProxy 角色和当前可见 Unit 位置。 */
	void LogSamClientPositions();
	/** 每 30 秒输出 Dedicated 帧时、带宽、容量和统一预算结果。 */
	void LogM7PerformanceSnapshot();
	/** 输出 M8 冻结版本、服务器权威边界和显式延期能力。 */
	void LogM8ReleaseContract();
	/** 带 -CombatM7CapacitySmoke 时把场景扩展到 64 Unit / 256 Modifier 冻结边界。 */
	bool ExpandM7CapacityScenario();

	/** 当前由本 Actor 生成并负责销毁的 Unit。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ACombatUnitCharacter>> SpawnedUnits;
	/** 延迟到单位落地后再发攻击 Order，避免使用生成帧中的空中导航位置。 */
	FTimerHandle M4AttackScenarioTimer;
	/** 等待 Dedicated 玩家完成登录后再冻结 owning connection 复制矩阵。 */
	FTimerHandle M7NetworkScenarioTimer;
	/** 客户端等待 Owner 与 Unit View 完成初始复制后再发送 smoke RPC。 */
	FTimerHandle M7ClientRpcTimer;
	/** Dedicated soak 周期性能快照计时器。 */
	FTimerHandle M7PerformanceTimer;
	/** SAM 服务器对撞稳定窗口计时器。 */
	FTimerHandle SamMovementTimer;
	/** SAM 客户端等待复制收敛后的位置快照计时器。 */
	FTimerHandle SamClientPositionTimer;
	/** SAM 对撞开始时静止 B 的服务器权威位置。 */
	FVector SamStationaryUnitStart = FVector::ZeroVector;
	/** SAM 对撞开始时移动 A 的服务器权威位置，用于防止“只验证静止目标”的假阳性。 */
	FVector SamMovingUnitStart = FVector::ZeroVector;
	/** M5 场景使用的瞬态 Projectile 定义，保证弹体飞行期间不会被 GC。 */
	UPROPERTY(Transient)
	TObjectPtr<UCombatProjectileData> ScenarioProjectileData;
	/** 当前 M5 场景弹体，重建场景时显式取消。 */
	FCombatProjectileHandle ScenarioProjectileHandle;
	/** M6 Aura 使用的瞬态 child Modifier 定义，保证 Aura 活动期间不会被 GC。 */
	UPROPERTY(Transient)
	TObjectPtr<UCombatModifierData> ScenarioAuraChildData;
	/** 当前 M6 场景 Aura，重建场景时显式取消。 */
	FCombatAuraHandle ScenarioAuraHandle;
	/** 容量 soak 动态 Modifier 定义的强引用，保证 60 秒运行期间不会被 GC。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCombatModifierData>> ScenarioCapacityModifierData;
};
