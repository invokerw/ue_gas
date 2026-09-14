#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/Network/CombatNetworkTypes.h"
#include "CombatItemNetworkScenario.generated.h"
class ACombatPlayerController;
/** 显式物品联机 smoke 的客户端编排器；所有行为通过真实 owning Unit RPC，Tick 只推进测试步骤。 */
UCLASS(NotBlueprintable)
class COMBAT_API ACombatItemNetworkScenario : public AActor
{
	GENERATED_BODY()
public:
	ACombatItemNetworkScenario();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	/** 注入正常网络信封，保留当前生命与指挥版本。 */
	void Submit(ACombatPlayerController& Player, FCombatOrderRequest Request);
	/** 两客户端完成各自循环后投放竞争目标，再交换控制权并核对实例留在原单位。 */
	void TickServer();
	/** 记录独立最终结果，初始 Accepted 不计为成功。 */
	UFUNCTION() void ReceiveFinal(FCombatOrderResult Result);
	/** 一次性输出结果，超时也结束测试。 */
	void Finish(bool bSuccess, const TCHAR* Detail);
	FCombatItemHandle Item;
	FCombatItemHandle ContestedItem;
	TWeakObjectPtr<class ACombatUnitCharacter> BoundUnit;
	int32 Step = 0;
	int32 RequestId = 800001;
	int32 FinalCount = 0;
	int32 ContestedRevision = 0;
	int32 ContestedRequestId = 0;
	int32 InitialBindingGeneration = 0;
	int32 StaleRequestId = 0;
	float Elapsed = 0;
	float PhaseStarted = 0;
	bool bContestResolved = false;
	bool bContestWon = false;
	bool bFinished = false;
};
