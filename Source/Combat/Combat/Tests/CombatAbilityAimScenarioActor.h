#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineBaseTypes.h"
#include "InputCoreTypes.h"
#include "CombatAbilityAimScenarioActor.generated.h"

class ACombatPlayerController;
class ACombatUnitCharacter;
class ACombatAbilityIndicatorActor;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/** 仅由显式 -CombatAimPIESmoke 编排创建的实机验证 Actor；注入原生输入，生产地图不放置此对象。 */
UCLASS(NotBlueprintable)
class COMBAT_API ACombatAbilityAimScenarioActor : public AActor
{
	GENERATED_BODY()
public:
	ACombatAbilityAimScenarioActor();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Test", meta=(DisplayName="验证已结束", ToolTip="一次性 PIE 输入场景是否已经结束。")) bool bFinished = false;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Test", meta=(DisplayName="验证通过", ToolTip="所有实际输入、请求计数和形状检查是否通过。")) bool bPassed = true;
private:
	/** 向真实 PlayerInput 注入模拟按键，仍经过 Enhanced Input/IMC 与生产 Controller。 */
	void Key(FKey Input, EInputEvent Event);
	/** 把有限世界位置投影到玩家视口，再通过鼠标射线读取；不注入假命中。 */
	void PointAt(FVector Location);
	/** 记录具体断言结果；失败仍继续检查后续独立路径。 */
	void Check(bool bCondition, const TCHAR* Description);
	/** 保存含 HUD 的真实游戏视口截图，供人工检查贴花。 */
	void Screenshot(const TCHAR* Name);
	/** 创建临时受控画面；冻结本次采样的英雄姿态，保留真实网格与指示器材质。 */
	void PrepareGroundVisual(ACombatUnitCharacter* Unit);
	/** 读取实际渲染像素并保存原始 PNG；仅供显式 PIE 测试。 */
	bool ReadGroundFrame(const TCHAR* Name, TArray<FColor>& OutPixels);
	/** 比较同一静止场景开关指示器的像素，分别检查地面变色与遮挡物不变色。 */
	void CheckGroundPixels(const TArray<FColor>& Pixels, bool bLine);
	/** 删除临时画面对象并恢复英雄动画，重复清理安全。 */
	void CleanupGroundVisual();
	TWeakObjectPtr<ACombatPlayerController> Player;
	FVector NearPoint = FVector::ZeroVector;
	float Elapsed = 0.0f;
	int32 Step = 0;
	int32 BaselineRequest = 0;
	int32 ExpectedRequest = 0;
	FVector CaptureOrigin = FVector::ZeroVector;
	FVector HeroSample = FVector::ZeroVector;
	bool bSavedPauseAnims = false;
	TWeakObjectPtr<ACombatUnitCharacter> CapturedUnit;
	TArray<TWeakObjectPtr<AActor>> VisualFixtures;
	TArray<FColor> GroundBaseline;
	UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> GroundCapture;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> GroundTarget;
	UPROPERTY(Transient) TObjectPtr<ACombatAbilityIndicatorActor> GroundIndicator;
};
