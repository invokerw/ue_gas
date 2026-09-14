#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/Items/CombatItemTypes.h"
#include "CombatWorldItem.generated.h"

class UCombatItemData;
class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;
struct FStreamableHandle;

/**
 * 可右键命中的地面物品投影，不是战斗单位，不阻挡导航、弹体或角色。
 * 预放物品由服务器 BeginPlay 创建实例；运行时丢弃沿用原实例，客户端只接收身份与展示数量。
 */
UCLASS(Blueprintable, meta=(DisplayName="战斗地面物品", ToolTip="放入关卡并配置物品定义即可产生可拾取物品。"))
class COMBAT_API ACombatWorldItem : public AActor
{
	GENERATED_BODY()
public:
	ACombatWorldItem();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="物品定义", ToolTip="关卡预放物品在服务器开始游戏时登记的物品定义；运行时丢弃使用已有实例。")) TObjectPtr<UCombatItemData> ItemDefinition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="初始数量", ToolTip="关卡预放实例中的物品数量；必须处于定义允许的堆叠范围。", ClampMin="1", ClampMax="99")) int32 InitialQuantity = 1;
	FCombatItemHandle GetItemHandle() const { return ItemHandle; }
	int32 GetItemRevision() const { return ItemRevision; }
	FPrimaryAssetId GetItemDefinitionId() const { return DefinitionId; }
	/** 仅登记表在实例交接提交后刷新世界投影。 */
	void InitializeProjection(FCombatItemHandle Handle, int32 Revision, UCombatItemData* Definition, int32 Quantity);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnConstruction(const FTransform& Transform) override;
private:
	/** 本地定义解析与模型展示；异步回调核对当前实例身份，退出时取消。 */
	UFUNCTION() void RefreshPresentation();
	UPROPERTY(VisibleAnywhere, Category="Combat|Item", meta=(DisplayName="拾取查询球", ToolTip="只响应鼠标可见性查询，不阻挡导航或战斗。")) TObjectPtr<USphereComponent> PickupShape;
	UPROPERTY(VisibleAnywhere, Category="Combat|Item", meta=(DisplayName="物品模型", ToolTip="地面物品的纯表现模型。")) TObjectPtr<UStaticMeshComponent> ItemMesh;
	UPROPERTY(VisibleAnywhere, Category="Combat|Item", meta=(DisplayName="物品名称", ToolTip="地面物品的显示名称和数量。")) TObjectPtr<UWidgetComponent> GroundLabel;
	UPROPERTY(ReplicatedUsing=RefreshPresentation) FCombatItemHandle ItemHandle;
	UPROPERTY(ReplicatedUsing=RefreshPresentation) int32 ItemRevision = 0;
	UPROPERTY(ReplicatedUsing=RefreshPresentation) FPrimaryAssetId DefinitionId;
	UPROPERTY(ReplicatedUsing=RefreshPresentation) int32 Quantity = 0;
	TSharedPtr<FStreamableHandle> DefinitionLoad;
	TSharedPtr<FStreamableHandle> MeshLoad;
	bool bEnding = false;
};
