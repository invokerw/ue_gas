#pragma once
#include "CoreMinimal.h"
#include "CombatGameMode.h"
#include "DefenseGameMode.generated.h"
class ADefenseObjective;
class UDefenseDirectorComponent;
class UDefenseRunData;
/** 独立防守入口，复用服务器 Command Pawn 拓扑；只在本模式生成核心和启动导演。 */
UCLASS(Blueprintable, meta=(DisplayName="防守游戏模式", ToolTip="显式配置对局定义，生成核心并运行六波。"))
class COMBAT_API ADefenseGameMode : public ACombatGameMode
{
	GENERATED_BODY()
public:
	ADefenseGameMode();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& Id, FString& Error) override;
	virtual void Logout(AController* Exiting) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* Player, const FTransform& Transform) override;
	UDefenseDirectorComponent* GetDefenseDirector() const { return Director; }
	ADefenseObjective* GetDefenseObjective() const { return Objective; }
protected:
	UPROPERTY(EditDefaultsOnly, Category="Defense", meta=(DisplayName="防守对局定义", ToolTip="必填；缺失或校验失败不启动，不能用隐式默认配置替代损坏资产。"))
	TObjectPtr<UDefenseRunData> RunData;
private:
	/** 开局角色注册的统一入口，兼容 BeginPlay 与 PostLogin 顺序。 */
	void RegisterHero(AController* Player);
	UPROPERTY(VisibleAnywhere, Category="Defense", meta=(DisplayName="防守导演", ToolTip="仅服务器持有的对局编排组件。")) TObjectPtr<UDefenseDirectorComponent> Director;
	UPROPERTY(Transient) TObjectPtr<ADefenseObjective> Objective;
};
