#pragma once

#include "CoreMinimal.h"
#include "Combat/Targeting/CombatTargetingTypes.h"
#include "CombatAIRoleTypes.generated.h"

/** 准备与回执的操作类别，用于到达记账和重试隔离；行为选择仍在 StateTree 资产。 */
UENUM()
enum class ECombatAIRoleOperation : uint8
{
	Explicit UMETA(DisplayName="显式命令"),
	Attack UMETA(DisplayName="已知目标攻击"),
	Home UMETA(DisplayName="职责归位"),
	Route UMETA(DisplayName="路线航点")
};

/** 角色任务精确取消自己的 Order 时保留的本地原因，不新增 Combat 网络事件。 */
enum class ECombatAIRoleStopReason : uint8 { None, AssignmentChanged, LostPerception, LeashExceeded, EngageOpportunity };

/** 服务器提供的空间职责；空路线表示守点，循环只控制成功到点后的游标。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatAIAssignment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(DisplayName="职责锚点", ToolTip="服务器世界坐标，单位厘米；归位使用 XY 到达复核，必须位于可达导航区域。", Units="cm")) FVector Home = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(DisplayName="路线航点", ToolTip="最多 64 个有限世界坐标；空数组表示守点。仅成功到点才推进游标。")) TArray<FVector> Route;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(DisplayName="循环路线", ToolTip="开启后从最后航点回到首点；关闭则到达终点后等待新职责或敌人。")) bool bLoopRoute = false;
	/** 只校验数据域，不隐式修改坐标或把不可达锚点瞬移到地图中。 */
	bool IsValid() const;
};

/** 稳定定义的候选优先级；未列出的定义为 0，数值越大越优先，再按威胁与距离排序。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatAITargetPriority
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(DisplayName="单位定义", ToolTip="精确 CombatUnit PrimaryAssetId；不可为空或重复，不持有实例对象。")) FPrimaryAssetId Definition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(DisplayName="优先级", ToolTip="范围 -100 到 100；未知定义为 0，较高优先级先于距离和威胁。", ClampMin="-100", ClampMax="100")) int32 Priority = 0;
};

/** 范围/LOS 感知配置；不提供战争迷雾，开启完整可见性要求会被校验拒绝。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatAIPerceptionConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(DisplayName="警戒半径", ToolTip="XY 查询半径，单位厘米，范围 1 到 10000；公共 Targeting 同时计入目标胶囊半径。", Units="cm", ClampMin="1", ClampMax="10000")) float Radius = 800;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(DisplayName="需要几何视线", ToolTip="启用时由 CombatTargeting 通道检查遮挡；不会读取失去视线目标的新位置。")) bool bRequireLineOfSight = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(DisplayName="权威可见性", ToolTip="当前仅支持不检查战争迷雾；要求权威可见会拒绝启动，不降级为全知。")) ECombatVisibilityPolicy VisibilityPolicy = ECombatVisibilityPolicy::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(DisplayName="交战采样间隔", ToolTip="有活动攻击时的游戏秒数，范围 0.05 到 5；失感知/越界最多延迟一次采样加调度延期。", Units="s", ClampMin="0.05", ClampMax="5")) float ActiveInterval = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(DisplayName="空闲采样间隔", ToolTip="其他情况下的游戏秒数，范围 0.05 到 5；时间缩放和暂停遵守 Combat Scheduler。", Units="s", ClampMin="0.05", ClampMax="5")) float IdleInterval = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(DisplayName="记忆时长", ToolTip="失去感知后保留最后观测的游戏秒数，范围 0 到 60；0 表示下一次采样立即移除失感知记忆。", Units="s", ClampMin="0", ClampMax="60")) float MemorySeconds = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(DisplayName="候选上限", ToolTip="完整合法候选排序后保留的数量，范围 1 到 64；不是按 World 枚举先后截断。", ClampMin="1", ClampMax="64")) int32 MaxCandidates = 16;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(DisplayName="记忆上限", ToolTip="总记忆条数，范围 1 到 128 且不得小于候选上限；优先保留当前候选，再保留最近观察。", ClampMin="1", ClampMax="128")) int32 MaxMemories = 32;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(DisplayName="记录可见来源威胁", ToolTip="只为当时仍通过范围/LOS 公共校验的已知攻击者累计实际受伤量；隐藏或未知攻击者不暴露身份和位置。")) bool bObserveDamageThreat = true;
};

/** 不持有敌人强引用的历史观察；只有当前感知许可时才更新位置、定义和生命代次。 */
struct COMBAT_API FCombatAIKnownTarget
{
	TWeakObjectPtr<ACombatUnitCharacter> Unit;
	uint32 Life = 0;
	uint32 StableId = 0;
	FPrimaryAssetId Definition;
	FVector LastSeenPosition = FVector::ZeroVector;
	double LastSeenAt = 0;
	float Threat = 0;
	float Score = 0;
	int32 Priority = 0;
	bool bVisible = false;
};

/** 本轮已完成的只读观察，普通更新不改变职责版本，也不直接选择行为状态。 */
struct COMBAT_API FCombatAIKnowledgeSnapshot
{
	uint64 Revision = 0;
	double ObservedAt = 0;
	TArray<FCombatAIKnownTarget> Candidates;
	TArray<FCombatAIKnownTarget> Memories;
};

/** 纯候选排序/保持算法，不访问 World、不读敌人状态、不提交 Order。 */
struct COMBAT_API FCombatAITargetSelection
{
	/** 优先级降序、分数降序、Actor 身份升序；输入必须已通过公共合法性过滤。 */
	static void Sort(TArray<FCombatAIKnownTarget>& Candidates);
	/** 当前身份仍存在时执行最短保持/分差；失效或新生命不受保持门槛阻挡。 */
	static int32 Select(const TArray<FCombatAIKnownTarget>& Candidates, TWeakObjectPtr<ACombatUnitCharacter> Current,
		uint32 Life, double HeldSeconds, float MinHold, float SwitchMargin);
};
