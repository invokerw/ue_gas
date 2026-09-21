#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "CombatNavigationBuildCommandlet.generated.h"

/** 为指定编辑器地图显式构建并保存 NavMesh，避免新建 World Partition 地图在 Dedicated 中读到空导航。 */
UCLASS()
class COMBAT_API UCombatNavigationBuildCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCombatNavigationBuildCommandlet();
	/** 同步构建并保存；Map 为 /Game 长包名，Probe=X=0,Y=0,Z=0 默认为原点。载入/投影/保存失败均返回非零。 */
	virtual int32 Main(const FString& Params) override;
};
