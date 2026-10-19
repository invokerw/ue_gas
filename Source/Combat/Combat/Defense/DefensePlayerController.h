#pragma once
#include "CoreMinimal.h"
#include "CombatPlayerController.h"
#include "DefensePlayerController.generated.h"
class UDefenseChoiceComponent;
class UDefenseHeroSelectionComponent;
/** 仅防守地图使用的玩家连接；继承全部 Combat 输入，追加私有选择和面板鼠标隔离。 */
UCLASS(Blueprintable, meta=(DisplayName="防守玩家控制器", ToolTip="保持 Combat 输入与控制拓扑，持有个人强化选择组件。"))
class COMBAT_API ADefensePlayerController : public ACombatPlayerController
{
	GENERATED_BODY()
public:
	ADefensePlayerController();
	UDefenseChoiceComponent* GetChoiceComponent() const { return Choice; }
	virtual bool IsPointerOverCombatUI() const override;
	UDefenseHeroSelectionComponent* GetHeroSelection() const { return HeroSelection; }
private:
	UPROPERTY(VisibleAnywhere, Category="Defense", meta=(DisplayName="开局英雄选择", ToolTip="仅拥有者可读的开局守卫选择与安全RPC组件。")) TObjectPtr<UDefenseHeroSelectionComponent> HeroSelection;
	UPROPERTY(VisibleAnywhere, Category="Defense", meta=(DisplayName="个人强化", ToolTip="仅拥有者可读的选择视图及 RPC 入口。")) TObjectPtr<UDefenseChoiceComponent> Choice;
};
