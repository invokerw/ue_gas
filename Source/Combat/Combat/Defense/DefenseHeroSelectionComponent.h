#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DefenseHeroSelectionComponent.generated.h"
class ACombatPlayerController;

/** 开局固定白名单选择意图；不接受类、位置、数值或其它玩家身份。 */
USTRUCT()
struct FDefenseHeroChoiceRequest
{
	GENERATED_BODY()
	UPROPERTY() int32 RequestId=0;
	UPROPERTY() FGuid RunId;
	UPROPERTY() int32 BindingGeneration=0;
	UPROPERTY() uint32 LifeGeneration=0;
	bool operator==(const FDefenseHeroChoiceRequest& B) const { return RequestId==B.RequestId&&RunId==B.RunId&&BindingGeneration==B.BindingGeneration&&LifeGeneration==B.LifeGeneration; }
};
/** 仅拥有者读取的开局选择结果schema1，不持有英雄或定义对象。 */
USTRUCT()
struct FDefenseHeroChoiceView
{
	GENERATED_BODY()
	UPROPERTY() int32 SchemaVersion=1;
	UPROPERTY() int32 Revision=0;
	UPROPERTY() bool bCanChoose=false;
	UPROPERTY() bool bGuardianSelected=false;
	UPROPERTY() int32 LastRequestId=0;
	UPROPERTY() bool bLastSuccess=false;
	UPROPERTY() FName FailureReason;
};
/** 每连接一个有界信箱；身份/阶段由服务器再次检查，复用公共限频和重放保护，无Tick。 */
UCLASS(meta=(DisplayName="开局英雄选择", ToolTip="开局空构筑期间可改用配置的神殿守卫一次；只复制拥有者可读结果。"))
class COMBAT_API UDefenseHeroSelectionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UDefenseHeroSelectionComponent();
	const FDefenseHeroChoiceView& GetView() const { return View; }
	/** 本地按钮只组装当前身份并发送固定选择意图，不生成单位。 */
	void RequestGuardian();
	/** RPC与自动化共用信封入口；调用连接必须为本组件Owner。 */
	FDefenseHeroChoiceView ProcessForConnection(ACombatPlayerController* Player,const FDefenseHeroChoiceRequest& Request);
	/** Director的现有监控节奏投影资格，不新增权威计时器。 */
	void PublishEligibility(bool bCanChoose,bool bSelected);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	friend class ACombatDefenseNetworkScenario;
	UFUNCTION(Server,Reliable) void ServerChooseGuardian(FDefenseHeroChoiceRequest Request);
	/** 只发布实际变化，保留回执与资格两个独立信息。 */
	void PublishResult(const FDefenseHeroChoiceView& Result);
	struct FReceipt { FDefenseHeroChoiceRequest Request; FDefenseHeroChoiceView Result; };
	TArray<FReceipt> Receipts;
	int32 HighRequestId=0;
	bool bEnding=false,bMutating=false;
	UPROPERTY(Replicated) FDefenseHeroChoiceView View;
};
