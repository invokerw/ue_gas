// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CombatCharacter.generated.h"

class ACombatUnitCharacter;
class UCameraComponent;
class USceneComponent;
class USpringArmComponent;

/** 本地镜头更新模式；单位移动与命令执行不受此状态控制。 */
UENUM(BlueprintType)
enum class ECombatCameraMode : uint8
{
	Free UMETA(DisplayName="自由观察"),
	EdgePan UMETA(DisplayName="边缘滚屏"),
	FollowHeld UMETA(DisplayName="按住跟随")
};

/**
 * 顶视角 Command Pawn，只承载连接、输入焦点与相机。
 * 它不含 Combat 组件、不参与 gameplay collision，也不会回写被指挥 Unit 的 Transform。
 */
UCLASS(Blueprintable)
class ACombatCharacter : public APawn
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
	UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0.0", DisplayName="相机跟随速度", ToolTip="按住跟随期间的每秒插值速率；0 立即对齐。只跟随 XY，保持绑定时的高度。"))
	float CameraFollowSpeed = 12.0f;

	UPROPERTY(EditAnywhere, Category="Camera", meta=(DisplayName="启用边缘滚屏", ToolTip="鼠标贴近游戏视口边缘时自动平移，无需任何抓取键。"))
	bool bEnableEdgePan = true;
	UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0.001", ClampMax="0.25", DisplayName="边缘触发比例", ToolTip="触发带宽度占视口短边的比例；0.025 表示 2.5%，越靠边速度越快。"))
	float EdgePanScreenThreshold = 0.025f;
	UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0.0", Units="cm/s", DisplayName="边缘平移速度", ToolTip="贴到边界时的最大世界平移速度；0 禁用平移，角落不会额外加速。"))
	float EdgePanSpeed = 1800.0f;
	UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0.0", DisplayName="边缘停止减速", ToolTip="离开边缘后的每秒速度衰减率；0 立即停止，正数越大停得越快。UI 阻挡或失焦始终立即停止。"))
	float EdgePanDeceleration = 0.0f;
	UPROPERTY(EditAnywhere, Category="Camera", meta=(DisplayName="限制相机边界", ToolTip="启用后把镜头锚点限制在下方世界 XY 矩形内，不限制单位。"))
	bool bClampCameraBounds = false;
	UPROPERTY(EditAnywhere, Category="Camera", meta=(EditCondition="bClampCameraBounds", DisplayName="相机边界最小值", ToolTip="世界 XY 最小坐标，单位厘米；应分别小于最大值。非法矩形忽略。"))
	FVector2D CameraBoundsMin = FVector2D(-5000, -5000);
	UPROPERTY(EditAnywhere, Category="Camera", meta=(EditCondition="bClampCameraBounds", DisplayName="相机边界最大值", ToolTip="世界 XY 最大坐标，单位厘米；应分别大于最小值。非法矩形忽略。"))
	FVector2D CameraBoundsMax = FVector2D(5000, 5000);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(AllowPrivateAccess="true", DisplayName="相机模式", ToolTip="本地自由观察、边缘滚屏或按住跟随；不复制。"))
	ECombatCameraMode CameraMode = ECombatCameraMode::Free;

	/** 当前按住手势与绑定的身份；边缘接管后保留按住号，直到对应释放才允许再按。 */
	uint64 FollowPressSerial = 0;
	uint64 NextFollowSerial = 0;
	int32 CameraBindingGeneration = INDEX_NONE;
	/** 每个本地 Command Pawn 仅定位一次；选择确认的空窗不能重新触发初始居中。 */
	bool bCameraAnchorInitialized = false;
	FVector PanVelocity = FVector::ZeroVector;
	/** 在已有滚屏中按下 Space 时先跟随；鼠标离开再进入边缘，才作为新的手动接管。 */
	bool bWasAtEdge = false;
	bool bIgnoreEdgeUntilExit = false;
	/** 只允许当前占有者在本机驱动相机；独立服务器和远端连接均拒绝。 */
	bool CanUpdateLocalCamera() const;
	/** 对有效配置执行平面边界限制；不改变锚点高度。 */
	FVector ClampCameraLocation(FVector Location) const;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FCombatCameraMovementTest;
#endif

public:
	ACombatCharacter();

	/** 初始化时保持碰撞关闭；实际跟随目标由 PlayerController 绑定。 */
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** 绑定变化时取消旧会话；仅首次有效目标初始化镜头，后续选择或重新就绪保持锚点。只供本地 Controller 调用。 */
	void SetFollowTarget(ACombatUnitCharacter* NewTarget, int32 BindingGeneration = 0);
	/** 开始一次按住跟随；无就绪目标/重复按住返回 0。返回号必须交给匹配的释放调用。 */
	uint64 BeginCameraFollow();
	/** 只结束匹配的物理按住号，旧释放无效；不影响已经接管的边缘平移。 */
	void EndCameraFollow(uint64 PressSerial);
	/** 失焦/teardown 使全部旧会话失效并立即清除惯性；保留锚点与绑定。 */
	void ResetCameraInput();
	/** 每帧由 Controller 输入处理后调用一次；EdgeInput 为屏幕右/上方向，UI 门控为 false 时立即清除平移。 */
	void UpdateCamera(float DeltaSeconds, FVector2D EdgeInput, bool bAllowEdgePan = true);
	/** 将游戏视口中的像素位置换算为边缘方向与强度；无效/出界坐标返回零，无需鼠标位移事件。 */
	FVector2D GetEdgePanInput(FVector2D Cursor, FVector2D ViewportSize) const;
	ECombatCameraMode GetCameraMode() const { return CameraMode; }

	/** 返回顶视角相机组件。 */
	UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent.Get(); }

	/** 返回相机弹簧臂组件。 */
	USpringArmComponent* GetCameraBoom() const { return CameraBoom.Get(); }
};
