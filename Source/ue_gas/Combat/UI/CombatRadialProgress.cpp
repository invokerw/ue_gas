#include "Combat/UI/CombatRadialProgress.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"

/** UMG 进度环的轻量 Slate 图元；绘制参数由 Designer 配置，无 Tick 和外部订阅。 */
class SCombatRadialProgress : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCombatRadialProgress) {} SLATE_END_ARGS()
	void Construct(const FArguments&) {}
	float Percent = 0.0f;
	float Thickness = 3.0f;
	FLinearColor Fill, Track;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(36.0f); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override
	{
		const FVector2D Center = Geometry.GetLocalSize() * 0.5f;
		const float Radius = FMath::Max(0.0f, FMath::Min(Center.X, Center.Y) - Thickness * 0.5f);
		auto DrawArc = [&](float Fraction, const FLinearColor& Color, int32 DrawLayer)
		{
			if (Fraction <= 0.0f) return;
			TArray<FVector2D> Points;
			const int32 Segments = FMath::Max(1, FMath::CeilToInt(96 * Fraction));
			for (int32 Index = 0; Index <= Segments; ++Index)
			{
				const float Angle = -HALF_PI + 2.0f * PI * Fraction * Index / Segments;
				Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
			}
			FSlateDrawElement::MakeLines(Elements, DrawLayer, Geometry.ToPaintGeometry(), Points,
				bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
				Color * Style.GetColorAndOpacityTint(), true, Thickness);
		};
		DrawArc(1.0f, Track, Layer);
		DrawArc(Percent, Fill, Layer + 1);
		return Layer + 1;
	}
};

void UCombatRadialProgress::SetProgress(float InPercent)
{
	Percent = FMath::IsFinite(InPercent) ? FMath::Clamp(InPercent, 0.0f, 1.0f) : 0.0f;
	if (Ring) { Ring->Percent = Percent; Ring->Invalidate(EInvalidateWidgetReason::Paint); }
}

TSharedRef<SWidget> UCombatRadialProgress::RebuildWidget()
{
	Ring = SNew(SCombatRadialProgress);
	return Ring.ToSharedRef();
}

void UCombatRadialProgress::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (Ring)
	{
		Ring->Fill = FillColor;
		Ring->Track = TrackColor;
		Ring->Thickness = FMath::Max(1.0f, Thickness);
		SetProgress(Percent);
	}
}

void UCombatRadialProgress::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Ring.Reset();
}
