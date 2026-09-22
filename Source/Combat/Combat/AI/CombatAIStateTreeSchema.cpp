#include "Combat/AI/CombatAIStateTreeSchema.h"
#include "Combat/AI/CombatAIStateTreeTasks.h"
#include "Combat/AI/CombatAITacticalTasks.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "StateTreeConditionBase.h"

UCombatAIStateTreeSchema::UCombatAIStateTreeSchema()
{
	ContextActorClass = ACombatUnitCharacter::StaticClass();
	GetContextActorDataDesc().Struct = ACombatUnitCharacter::StaticClass();
	ScheduledTickPolicy = EStateTreeComponentSchemaScheduledTickPolicy::Allowed;
	ContextDataDescs.Add({ TEXT("Brain"), UCombatAIBrainComponent::StaticClass(), FGuid(0x17295031, 0x49154820, 0xADF71C32, 0x03814FCB) });
	ContextDataDescs.Add({ TEXT("AIContext"), FCombatAIContext::StaticStruct(), FGuid(0xEE295CB1, 0x972548DD, 0xADD77F32, 0x97515542) });
}

bool UCombatAIStateTreeSchema::IsStructAllowed(const UScriptStruct* Struct) const
{
	return Struct && (Struct->IsChildOf(FCombatAITaskBase::StaticStruct())
		|| Struct->IsChildOf(FStateTreeConditionCommonBase::StaticStruct())
		|| Struct->IsChildOf(FCombatAITacticalConsideration::StaticStruct()));
}

bool UCombatAIStateTreeSchema::IsExternalItemAllowed(const UStruct& Struct) const
{
	return Struct.IsChildOf(UCombatAIBrainComponent::StaticClass()) || Struct.IsChildOf(ACombatUnitCharacter::StaticClass());
}

void UCombatAIStateTreeSchema::SetContextData(FContextDataSetter& Setter, bool bLogErrors) const
{
	Super::SetContextData(Setter, bLogErrors);
	const UBrainComponent* Component = Setter.GetComponent();
	if (auto* Brain = const_cast<UCombatAIBrainComponent*>(Cast<UCombatAIBrainComponent>(Component)))
	{
		Setter.SetContextDataByName(TEXT("Brain"), FStateTreeDataView(Brain));
		Setter.SetContextDataByName(TEXT("AIContext"), FStateTreeDataView(FStructView::Make(Brain->GetContextData())));
	}
}
