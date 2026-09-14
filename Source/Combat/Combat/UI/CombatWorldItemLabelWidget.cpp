#include "Combat/UI/CombatWorldItemLabelWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UCombatWorldItemLabelWidget::RebuildWidget()
{
	return SNew(SBorder).Visibility(EVisibility::HitTestInvisible).Padding(FMargin(5, 2))
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.015f, 0.022f, 0.03f, 0.84f))
		[SAssignNew(Label, STextBlock).Text(DisplayText).ColorAndOpacity(Tint).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
		 .Justification(ETextJustify::Center)];
}
void UCombatWorldItemLabelWidget::ShowItem(const FText& Name, int32 Quantity, FLinearColor Color)
{
	DisplayText = FText::Format(NSLOCTEXT("CombatItems", "GroundTitle", "{0} ×{1}\n右键拾取"), Name, FText::AsNumber(Quantity));
	Tint = Color;
	if (Label) { Label->SetText(DisplayText); Label->SetColorAndOpacity(Tint); }
}
void UCombatWorldItemLabelWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Label.Reset();
}
