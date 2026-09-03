// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ue_gasCharacter.generated.h"

class ACombatUnitCharacter;
class UCameraComponent;
class USceneComponent;
class USpringArmComponent;

/**
 * 顶视角 Command Pawn，只承载连接、输入焦点与相机。
 * 它不含 Combat 组件、不参与 gameplay collision，也不会回写被指挥 Unit 的 Transform。
 */
UCLASS(Blueprintable)
class Aue_gasCharacter : public APawn
{
	GENERATED_BODY()

private:
	/** 无碰撞的相机根节点。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true", DisplayName="命令 Pawn 根节点", ToolTip="Command Pawn 的无碰撞场景根节点。"))
	TObjectPtr<USceneComponent> CommandRoot;

	/** 顶视角本地表现相机。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true", DisplayName="顶视角相机", ToolTip="只观察 CommandedUnit，不参与战斗单位移动。"))
	TObjectPtr<UCameraComponent> TopDownCameraComponent;

	/** 把相机放置在战斗区域上方的弹簧臂。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true", DisplayName="相机弹簧臂", ToolTip="提供固定俯视角，不执行碰撞回缩。"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** 仅本地相机读取的跟随目标；不复制且不拥有 gameplay 对象。 */
	TWeakObjectPtr<ACombatUnitCharacter> FollowTarget;

	/** 本地相机根节点追随 Unit 的插值速度；0 表示立即对齐。 */
	UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0.0", DisplayName="相机跟随速度", ToolTip="Command Pawn 在本地追随 CommandedUnit 的插值速度。"))
	float CameraFollowSpeed = 12.0f;

public:
	Aue_gasCharacter();

	/** 初始化时保持碰撞关闭；实际跟随目标由 PlayerController 绑定。 */
	virtual void BeginPlay() override;

	/** 仅 owning client 平滑相机位置；绝不写入 Combat Unit Transform。 */
	virtual void Tick(float DeltaSeconds) override;

	/** 幂等设置本地相机跟随的 CommandedUnit；空值会停止跟随。 */
	void SetFollowTarget(ACombatUnitCharacter* NewTarget);

	/** 返回顶视角相机组件。 */
	UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent.Get(); }

	/** 返回相机弹簧臂组件。 */
	USpringArmComponent* GetCameraBoom() const { return CameraBoom.Get(); }
};
