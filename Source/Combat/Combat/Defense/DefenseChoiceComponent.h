#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Defense/DefenseAugmentData.h"
#include "DefenseChoiceComponent.generated.h"
class ACombatPlayerController;
class UDefenseDirectorComponent;
/** Controller 拥有的私有选择信箱；服务器导演保存真值，组件只验证信封和投影给 owning client。 */
UCLASS(meta=(DisplayName="个人强化选择", ToolTip="防守模式拥有者选择请求与私有只读投影。"))
class COMBAT_API UDefenseChoiceComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UDefenseChoiceComponent();
	const FDefenseChoiceView& GetView() const { return View; }
	/** 客户端提交当前视图中的卡片或重抽意图；服务器重新验证全部身份和价格。 */
	void RequestChoice(EDefenseChoiceAction Action,FPrimaryAssetId Id=FPrimaryAssetId());
	/** RPC 和自动化共用的服务器信封入口；Connection 必须就是本组件 owner。 */
	FDefenseChoiceResult ProcessForConnection(ACombatPlayerController* Connection,const FDefenseChoiceRequest& Request);
	/** 导演发布只读快照；无变化不提升复制修订。 */
	void PublishAuthority(const FDefenseChoiceView& Snapshot);
	/** 断线/终局清理私有候选，不向别的连接复制残留。 */
	void ClearAuthority();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	friend class ACombatDefenseNetworkScenario;
	/** 从同 World 的 Defense GameMode 找服务器导演；客户端返回空。 */
	UDefenseDirectorComponent* ResolveDirector() const;
	UFUNCTION(Server,Reliable) void ServerSubmitChoice(FDefenseChoiceRequest Request);
	UPROPERTY(Replicated) FDefenseChoiceView View;
	bool bEnding=false;
};
