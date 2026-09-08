#include "Combat/UI/CombatFloatingTextWidget.h"

void UCombatFloatingTextWidget::NativeConstruct()
{
	Super::NativeConstruct();
	OnFloatingTextInitialized(FloatingData);
}
