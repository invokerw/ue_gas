#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "Combat/Ability/CombatAbilityIndicatorGeometry.h"
#include "Combat/Network/CombatNetworkTypes.h"
#include "CombatAbilityAimComponent.generated.h"

class ACombatPlayerController;
class ACombatAbilityIndicatorActor;
class ACombatUnitCharacter;
class UCombatAbilityData;

/** 仅改变本地确认时机，不改变服务器施法、追击或提交时序。 */
UENUM(BlueprintType)
enum class ECombatAbilityCastMode : uint8
{
	Standard UMETA(DisplayName="标准施法", ToolTip="按下进入瞄准，左键确认，右键或 Escape 取消。"),
	QuickPress UMETA(DisplayName="按下快施", ToolTip="按下时读取当前光标并确认；无效目标不提交。"),
	QuickRelease UMETA(DisplayName="松开快施", ToolTip="按住预览，松开同一技能键时确认；取消或 UI 上松开不提交。")
};

/** 本地预检状态；超距仍可提交给服务器追击，所有结果都不是命中承诺。 */
enum class ECombatAbilityAimStatus : uint8 { Ready, OutOfRange, InvalidTarget, Blocked, Unavailable };

/** 渲染与 HUD 的纯展示输入；没有 Unit/ASC 指针，也没有权威命中列表。 */
struct COMBAT_API FCombatAbilityAimPreview
{
	bool bVisible = false;
	bool bAiming = false;
	bool bHasTarget = false;
	ECombatAbilityAimStatus Status = ECombatAbilityAimStatus::Unavailable;
	FCombatAbilityIndicatorGeometry Geometry;
	FVector CasterLocation = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	float CastRadius = 0.0f;
	float TargetRadius = 0.0f;
	/** 当前瞄准方向的三维弹道投影长度，不改变定义中的最大飞行距离。 */
	float PlanarLineLength = 0.0f;
	FText Message;
};

/**
 * Controller 拥有的本地瞄准会话和只读适配；不复制、不激活 Ability、不发送 RPC。
 * 身份由 Spec、绑定代次与生命代次共同约束；Tick 只更新光标展示。Controller 唯一提交 Order。
 * EndPlay 释放反馈委托与复用的纯视觉 Actor，旧输入必须携带当前 SessionSerial 才能确认。
 */
UCLASS(meta=(DisplayName="技能瞄准组件", ToolTip="本地技能预览与会话生命周期，不执行战斗结算。"))
class COMBAT_API UCombatAbilityAimComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCombatAbilityAimComponent();
	/** 与 QWER 授予顺序相同；空槽和纯被动返回空，不自动选取其他技能。 */
	static const FGameplayAbilitySpec* ResolveSlot(ACombatUnitCharacter* Unit, int32 Slot);
	/** 开始新会话并废弃旧会话；单位未就绪或槽位无效时失败。 */
	bool BeginAim(int32 Slot);
	/** 幂等清理本地会话；不停止服务器命令。 */
	void CancelAim();
	/** 清除瞄准、悬停与旧回执，供控制转移、失焦和 UI Escape 使用。 */
	void ResetLocalState();
	/** 只记录 UI 当前悬停槽位；离开传 INDEX_NONE，瞄准期间以瞄准技能优先。 */
	void SetHoveredSlot(int32 Slot) { HoveredSlot = Slot; }
	/** 按当前技能查询最新光标：点目标只查询已标记地面，单位目标保持 Visibility 精确命中。失败清空输出。 */
	bool TraceAimHit(FHitResult& OutHit) const;
	/** 按本次射线和 UI 命中更新预览；不保留上次有效落点。 */
	void UpdatePreview(const FHitResult& Hit, bool bWorldInputAllowed);
	/** 确认必须使用最新命中与会话号；无效/过期返回 false，超距保留原始目标。 */
	bool BuildConfirmedOrder(uint64 Serial, const FHitResult& Hit, bool bWorldInputAllowed, FCombatOrderRequest& OutOrder);
	/** 在唯一请求入口发送前关联回执并结束瞄准，避免同步回执和重复输入重入。 */
	void MarkSubmitted(int32 RequestId);
	/** 快施失败后结束会话并短暂保留当前原因，不重用目标或重发请求。 */
	void FinishQuickCastAttempt();
	bool IsAiming() const { return ActiveSlot != INDEX_NONE; }
	int32 GetActiveSlot() const { return ActiveSlot; }
	uint64 GetSessionSerial() const { return SessionSerial; }
	const FCombatAbilityAimPreview& GetPreview() const { return Preview; }
	/** 返回瞄准原因或短暂的请求接收状态；不将 Accepted 显示成施法成功。 */
	FText GetStatusText() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function) override;

private:
	/** 复核会话仍属于当前 Owner、生命和授予的技能。 */
	bool IsSessionCurrent() const;
	/** 隔离旧回执并显式解绑旧单位，允许多次调用。 */
	void ClearReceipt();
	/** 只接受匹配请求、控制绑定和生命代次的首个批次回执。 */
	UFUNCTION() void HandleOrderResult(FCombatOrderBatchResult Result);
	/** 本地原生输入适配入口；所有渲染器只消费 Preview。 */
	ACombatPlayerController* GetCombatController() const;
	/** 将已整理的帧交给单个复用 Actor；Dedicated 不创建视觉对象。 */
	void RenderPreview();

	int32 ActiveSlot = INDEX_NONE;
	int32 HoveredSlot = INDEX_NONE;
	uint64 SessionSerial = 0;
	FGameplayAbilitySpecHandle ActiveHandle;
	TWeakObjectPtr<ACombatUnitCharacter> SessionUnit;
	int32 BindingGeneration = 0;
	uint32 LifeGeneration = 0;
	/** 物品瞄准额外绑定实例和修订，换槽或消耗后旧确认立即失效。 */
	FCombatItemHandle ActiveItemHandle;
	int32 ActiveItemRevision = 0;
	FCombatAbilityAimPreview Preview;
	FDelegateHandle DeactivateHandle;
	TWeakObjectPtr<ACombatUnitCharacter> ReceiptUnit;
	int32 PendingRequestId = 0;
	int32 ReceiptBinding = 0;
	uint32 ReceiptLife = 0;
	FText ReceiptText;
	/** 仅本地视觉提示的秒数，不驱动玩法。 */
	float ReceiptRemaining = 0.0f;
	UPROPERTY(Transient) TObjectPtr<ACombatAbilityIndicatorActor> IndicatorActor;
};
