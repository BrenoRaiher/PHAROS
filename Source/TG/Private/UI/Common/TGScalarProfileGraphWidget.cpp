// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Common/TGScalarProfileGraphWidget.h"

#include "Rendering/DrawElementTypes.h"

namespace
{
    FVector2f MakeGraphPoint(
        float X,
        float Y)
    {
        return FVector2f(X, Y);
    }
}

void
UTGScalarProfileGraphWidget::SetProfileSamples(
    const TArray<FTGScalarCurveSample>& InSamples)
{
    ProfileSamples.Reset(InSamples.Num());

    /*
     * Invalid values are not drawn even if an unvalidated caller invokes
     * this function directly.
     */
    for (const FTGScalarCurveSample& Sample : InSamples)
    {
        if (FMath::IsFinite(Sample.TimeSeconds)
            && FMath::IsFinite(Sample.Value))
        {
            ProfileSamples.Add(Sample);
        }
    }

    InvalidateLayoutAndVolatility();
}

void
UTGScalarProfileGraphWidget::ClearProfileSamples()
{
    ProfileSamples.Reset();
    InvalidateLayoutAndVolatility();
}

int32
UTGScalarProfileGraphWidget::GetProfileSampleCount() const
{
    return ProfileSamples.Num();
}

int32
UTGScalarProfileGraphWidget::NativePaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled) const
{
    const int32 BaseLayer =
        Super::NativePaint(
            Args,
            AllottedGeometry,
            MyCullingRect,
            OutDrawElements,
            LayerId,
            InWidgetStyle,
            bParentEnabled);

    const FVector2D LocalSize =
        AllottedGeometry.GetLocalSize();

    const float Left = PlotPadding;
    const float Right =
        static_cast<float>(LocalSize.X) - PlotPadding;

    const float Top = PlotPadding;
    const float Bottom =
        static_cast<float>(LocalSize.Y) - PlotPadding;

    if (Right <= Left || Bottom <= Top)
    {
        return BaseLayer;
    }

    const ESlateDrawEffect DrawEffects =
        bParentEnabled
            ? ESlateDrawEffect::None
            : ESlateDrawEffect::DisabledEffect;

    /*
     * Draw the horizontal and vertical axes.
     */
    TArray<FVector2f> HorizontalAxis;
    HorizontalAxis.Add(
        MakeGraphPoint(Left, Bottom));
    HorizontalAxis.Add(
        MakeGraphPoint(Right, Bottom));

    TArray<FVector2f> VerticalAxis;
    VerticalAxis.Add(
        MakeGraphPoint(Left, Bottom));
    VerticalAxis.Add(
        MakeGraphPoint(Left, Top));

    const int32 AxisLayer = BaseLayer + 1;

    FSlateDrawElement::MakeLines(
        OutDrawElements,
        AxisLayer,
        AllottedGeometry.ToPaintGeometry(),
        HorizontalAxis,
        DrawEffects,
        AxisColor,
        true,
        AxisThickness);

    FSlateDrawElement::MakeLines(
        OutDrawElements,
        AxisLayer,
        AllottedGeometry.ToPaintGeometry(),
        VerticalAxis,
        DrawEffects,
        AxisColor,
        true,
        AxisThickness);

    if (ProfileSamples.IsEmpty())
    {
        return AxisLayer;
    }

    double MinimumTime =
        ProfileSamples[0].TimeSeconds;

    double MaximumTime =
        ProfileSamples[0].TimeSeconds;

    double MinimumValue =
        ProfileSamples[0].Value;

    double MaximumValue =
        ProfileSamples[0].Value;

    for (const FTGScalarCurveSample& Sample : ProfileSamples)
    {
        MinimumTime =
            FMath::Min(
                MinimumTime,
                Sample.TimeSeconds);

        MaximumTime =
            FMath::Max(
                MaximumTime,
                Sample.TimeSeconds);

        MinimumValue =
            FMath::Min(
                MinimumValue,
                Sample.Value);

        MaximumValue =
            FMath::Max(
                MaximumValue,
                Sample.Value);
    }

    /*
     * A one-sample Isp profile is a constant profile under endpoint
     * clamping, so give it a visible horizontal time range.
     */
    if (FMath::IsNearlyEqual(
            MinimumTime,
            MaximumTime))
    {
        MinimumTime -= 0.5;
        MaximumTime += 0.5;
    }

    /*
     * Give constant-valued curves some vertical space instead of placing
     * the line directly on one plot boundary.
     */
    if (FMath::IsNearlyEqual(
            MinimumValue,
            MaximumValue))
    {
        const double ValuePadding =
            FMath::Max(
                FMath::Abs(MinimumValue) * 0.05,
                1.0);

        MinimumValue -= ValuePadding;
        MaximumValue += ValuePadding;
    }

    const double TimeRange =
        MaximumTime - MinimumTime;

    const double ValueRange =
        MaximumValue - MinimumValue;

    const auto MapSampleToGraph =
        [Left,
         Right,
         Top,
         Bottom,
         MinimumTime,
         MinimumValue,
         TimeRange,
         ValueRange](
            const FTGScalarCurveSample& Sample)
        {
            const double NormalizedTime =
                (Sample.TimeSeconds - MinimumTime)
                / TimeRange;

            const double NormalizedValue =
                (Sample.Value - MinimumValue)
                / ValueRange;

            const float X =
                FMath::Lerp(
                    Left,
                    Right,
                    static_cast<float>(NormalizedTime));

            /*
             * Slate coordinates increase downward, so larger profile
             * values must map toward Top.
             */
            const float Y =
                FMath::Lerp(
                    Bottom,
                    Top,
                    static_cast<float>(NormalizedValue));

            return MakeGraphPoint(X, Y);
        };

    TArray<FVector2f> CurvePoints;

    if (ProfileSamples.Num() == 1)
    {
        const FVector2f CenterPoint =
            MapSampleToGraph(ProfileSamples[0]);

        CurvePoints.Add(
            MakeGraphPoint(
                Left,
                CenterPoint.Y));

        CurvePoints.Add(
            MakeGraphPoint(
                Right,
                CenterPoint.Y));
    }
    else
    {
        CurvePoints.Reserve(ProfileSamples.Num());

        for (const FTGScalarCurveSample& Sample
             : ProfileSamples)
        {
            CurvePoints.Add(
                MapSampleToGraph(Sample));
        }
    }

    const int32 CurveLayer = AxisLayer + 1;

    FSlateDrawElement::MakeLines(
        OutDrawElements,
        CurveLayer,
        AllottedGeometry.ToPaintGeometry(),
        CurvePoints,
        DrawEffects,
        CurveColor,
        true,
        CurveThickness);

    /*
     * Show representative sample markers. Very large files retain the
     * full line, but markers are thinned to avoid excessive draw calls.
     */
    constexpr int32 MaximumDisplayedMarkers = 128;

    const int32 MarkerStride =
        FMath::Max(
            1,
            FMath::CeilToInt(
                static_cast<double>(
                    ProfileSamples.Num())
                / MaximumDisplayedMarkers));

    const int32 MarkerLayer = CurveLayer + 1;

    for (int32 SampleIndex = 0;
         SampleIndex < ProfileSamples.Num();
         SampleIndex += MarkerStride)
    {
        const FVector2f Point =
            MapSampleToGraph(
                ProfileSamples[SampleIndex]);

        TArray<FVector2f> HorizontalMarker;
        HorizontalMarker.Add(
            MakeGraphPoint(
                Point.X - MarkerHalfSize,
                Point.Y));
        HorizontalMarker.Add(
            MakeGraphPoint(
                Point.X + MarkerHalfSize,
                Point.Y));

        TArray<FVector2f> VerticalMarker;
        VerticalMarker.Add(
            MakeGraphPoint(
                Point.X,
                Point.Y - MarkerHalfSize));
        VerticalMarker.Add(
            MakeGraphPoint(
                Point.X,
                Point.Y + MarkerHalfSize));

        FSlateDrawElement::MakeLines(
            OutDrawElements,
            MarkerLayer,
            AllottedGeometry.ToPaintGeometry(),
            HorizontalMarker,
            DrawEffects,
            MarkerColor,
            true,
            1.0f);

        FSlateDrawElement::MakeLines(
            OutDrawElements,
            MarkerLayer,
            AllottedGeometry.ToPaintGeometry(),
            VerticalMarker,
            DrawEffects,
            MarkerColor,
            true,
            1.0f);
    }

    /*
     * Ensure the final sample always receives a marker when marker
     * thinning did not land on it.
     */
    if (ProfileSamples.Num() > 1
        && (ProfileSamples.Num() - 1)
            % MarkerStride != 0)
    {
        const FVector2f FinalPoint =
            MapSampleToGraph(
                ProfileSamples.Last());

        TArray<FVector2f> HorizontalMarker;
        HorizontalMarker.Add(
            MakeGraphPoint(
                FinalPoint.X - MarkerHalfSize,
                FinalPoint.Y));
        HorizontalMarker.Add(
            MakeGraphPoint(
                FinalPoint.X + MarkerHalfSize,
                FinalPoint.Y));

        TArray<FVector2f> VerticalMarker;
        VerticalMarker.Add(
            MakeGraphPoint(
                FinalPoint.X,
                FinalPoint.Y - MarkerHalfSize));
        VerticalMarker.Add(
            MakeGraphPoint(
                FinalPoint.X,
                FinalPoint.Y + MarkerHalfSize));

        FSlateDrawElement::MakeLines(
            OutDrawElements,
            MarkerLayer,
            AllottedGeometry.ToPaintGeometry(),
            HorizontalMarker,
            DrawEffects,
            MarkerColor,
            true,
            1.0f);

        FSlateDrawElement::MakeLines(
            OutDrawElements,
            MarkerLayer,
            AllottedGeometry.ToPaintGeometry(),
            VerticalMarker,
            DrawEffects,
            MarkerColor,
            true,
            1.0f);
    }

    return MarkerLayer;
}