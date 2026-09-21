#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/Core/CombatTypes.h"
#include "CombatAIDemoArena.generated.h"

class ACombatUnitCharacter;
class UCombatAIProfileData;

/** 阶段 A 可玩入口：生成指定单位并提供一次明确攻击目标；后续执行全部由资产 StateTree 和公共 Order 完成。 */
UCLASS(meta=(DisplayName="StateTree AI 演示场", ToolTip="放在有导航的关卡，配置单位与靶子类型及 AI Profile；服务器生成并启动示例。"))
class COMBAT_API ACombatAIDemoArena : public AActor
{
	GENERATED_BODY()
public:
	ACombatAIDemoArena();
	UPROPERTY(EditAnywhere, Category="AI Demo", meta=(DisplayName="AI 配置", ToolTip="必须选择已编译的 Combat AI Profile；空值不生成演示单位。")) TObjectPtr<UCombatAIProfileData> Profile;
	UPROPERTY(EditAnywhere, Category="AI Demo", meta=(DisplayName="AI 单位类型", ToolTip="选择已配置 UnitData 的 Combat Unit 蓝图；例如卓尔游侠。")) TSubclassOf<ACombatUnitCharacter> AgentClass;
	UPROPERTY(EditAnywhere, Category="AI Demo", meta=(DisplayName="靶子类型", ToolTip="选择已配置 UnitData 的 Combat Unit 蓝图；例如木桩。演示把靶子设置为队伍 2。")) TSubclassOf<ACombatUnitCharacter> TargetClass;
	UPROPERTY(EditAnywhere, Category="AI Demo", meta=(DisplayName="靶子偏移", ToolTip="相对演示场原点的初始世界轴偏移，单位厘米；应落在可达导航地面。", Units="cm")) FVector TargetOffset = FVector(900, 0, 0);
	UPROPERTY(Replicated, VisibleInstanceOnly, Category="AI Demo", meta=(DisplayName="自主单位", ToolTip="服务器生成的自主单位，客户端只读取复制状态。")) TObjectPtr<ACombatUnitCharacter> Agent;
	UPROPERTY(Replicated, VisibleInstanceOnly, Category="AI Demo", meta=(DisplayName="当前靶子", ToolTip="服务器生成的明确目标；本阶段不会自动感知和搜索其他目标。")) TObjectPtr<ACombatUnitCharacter> Target;
	/** 返回生成后位置的导航投影；若在普攻前摇中，树会等待攻击边界再切换。 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="AI Demo", meta=(DisplayName="AI 返回起点", ToolTip="PIE 服务器上返回生成位置的导航投影；前摇结束后切换为 Move，完成后保持等待。"))
	void RequestReturn();
	/** 显式恢复自主权，并提供已有靶子；不自动选择其他单位。 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="AI Demo", meta=(DisplayName="AI 攻击靶子", ToolTip="PIE 服务器上恢复自主行为并攻击配置的靶子，沿用原 StateTree 和 Order。"))
	void RequestAttack();
	/** 返回生成后位置的导航投影；避开 Spawn 碰撞调整与关卡原点不可达造成的演示歧义。 */
	FVector GetReturnLocation() const { return ReturnLocation; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	/** 等待导航生成后提供演示的初始目标；30 秒仍不可用则结束等待并记录原因。 */
	void TryStartAfterNavigation(const FCombatScheduledTickContext& Context);
	/** 显式演示输入和 EndPlay 撤销启动等待，避免旧回调覆盖新目标。 */
	void CancelStartupWait();
	FCombatScheduleHandle StartupSchedule;
	/** 使用 World Game Time，暂停时不消耗等待期限。 */
	double StartupDeadline = 0;
	FVector ReturnLocation = FVector::ZeroVector;
};
