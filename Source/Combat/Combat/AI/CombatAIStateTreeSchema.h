#pragma once

#include "Components/StateTreeComponentSchema.h"
#include "CombatAIStateTreeSchema.generated.h"

/** Combat Unit、Brain 和只读 Context 的专用 Schema；只允许遵守 Order/Scheduler 协议的原生任务。 */
UCLASS(meta=(DisplayName="Combat AI 决策树", ToolTip="单位服务器决策 Schema；任务通过 Order 执行，等待通过 Combat Scheduler。"))
class COMBAT_API UCombatAIStateTreeSchema : public UStateTreeComponentSchema
{
	GENERATED_BODY()
public:
	UCombatAIStateTreeSchema();
protected:
	virtual bool IsStructAllowed(const UScriptStruct* Struct) const override;
	virtual bool IsClassAllowed(const UClass* Class) const override { return false; }
	virtual bool IsExternalItemAllowed(const UStruct& Struct) const override;
	virtual void SetContextData(FContextDataSetter& Setter, bool bLogErrors) const override;
};
