#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Defense/DefenseTypes.h"
#include "DefenseEventComponent.generated.h"
class ACombatPlayerController;
class UDefenseDirectorComponent;

/** 防守事件拥有者请求组件；服务器 Director 保存事件真值，组件只复制回执。 */
UCLASS(meta=(DisplayName="防守事件请求", ToolTip="显示事件选项并向服务器提交安全、风险或取消意图。"))
class COMBAT_API UDefenseEventComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UDefenseEventComponent();
	const FDefenseEventResult& GetLastResult() const { return LastResult; }
	/** 本地按钮只提交当前事件快照中的意图。 */
	void RequestOption(EDefenseEventOption Option);
	/** RPC与自动化共用的服务器请求入口。 */
	FDefenseEventResult ProcessForConnection(ACombatPlayerController* Connection, const FDefenseEventRequest& Request);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	/** 显式网络探针重放真实RPC；生产无DefenseEventSmoke开关时不运行。 */
	friend class ACombatDefenseNetworkScenario;
	UFUNCTION(Server,Reliable) void ServerSubmitEvent(FDefenseEventRequest Request);
	UDefenseDirectorComponent* ResolveDirector() const;
	UPROPERTY(Replicated) FDefenseEventResult LastResult;
	bool bEnding=false;
};
