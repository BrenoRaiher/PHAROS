// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Visualization/TGSolarSystemOverviewWidget.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElementTypes.h"
#include "UI/Theme/TGUiTheme.h"
#include "Visualization/TGSimulationPlaybackActor.h"

namespace TGSolarSystemOverviewPrivate
{
    FVector RotateForView(
        const FVector& Value,
        const FQuat& ViewRotation)
    {
        return ViewRotation.RotateVector(Value);
    }

    FVector2f MapPoint(
        const FVector& Position,
        const FVector& Center,
        const FQuat& ViewRotation,
        const FVector2D& LocalSize,
        const double Scale,
        const FVector2D& PanPixels)
    {
        const FVector Projected = RotateForView(
            Position - Center,
            ViewRotation);
        return FVector2f(
            static_cast<float>(
                LocalSize.X * 0.5 + PanPixels.X + Projected.X * Scale),
            static_cast<float>(
                LocalSize.Y * 0.5 + PanPixels.Y - Projected.Y * Scale));
    }

    void DrawCircleStroke(
        FSlateWindowElementList& Elements,
        const int32 Layer,
        const FGeometry& Geometry,
        const FVector2f& Center,
        const float Radius,
        const FLinearColor& Color,
        const ESlateDrawEffect Effects,
        const float Thickness,
        const int32 SegmentCount = 32)
    {
        if (Radius <= 0.0f || Thickness <= 0.0f)
        {
            return;
        }

        TArray<FVector2f> Points;
        // Extend one segment beyond both ends of the loop. Slate strokes are
        // open polylines, so the overlap prevents their end caps from forming
        // a visible radial seam when many circles are layered into a sphere.
        Points.Reserve(SegmentCount + 3);
        for (int32 Index = -1; Index <= SegmentCount + 1; ++Index)
        {
            const double Angle =
                (2.0 * UE_DOUBLE_PI) *
                static_cast<double>(Index) /
                static_cast<double>(SegmentCount);
            Points.Add(Center + FVector2f(
                static_cast<float>(FMath::Cos(Angle) * Radius),
                static_cast<float>(FMath::Sin(Angle) * Radius)));
        }
        FSlateDrawElement::MakeLines(
            Elements,
            Layer,
            Geometry.ToPaintGeometry(),
            Points,
            Effects,
            Color,
            true,
            Thickness);
    }

    void DrawMarker(
        FSlateWindowElementList& Elements,
        const int32 Layer,
        const FGeometry& Geometry,
        const FVector2f& Center,
        const float Radius,
        const FLinearColor& Color,
        const ESlateDrawEffect Effects)
    {
        DrawCircleStroke(
            Elements,
            Layer,
            Geometry,
            Center,
            Radius,
            Color,
            Effects,
            FMath::Max(1.0f, Radius * 0.45f),
            20);
    }

    int32 DrawShadedSphere(
        FSlateWindowElementList& Elements,
        const int32 FirstLayer,
        const FGeometry& Geometry,
        const FVector2f& Center,
        const float Radius,
        const FLinearColor& BodyColor,
        const ESlateDrawEffect Effects)
    {
        if (Radius <= 0.5f)
        {
            return FirstLayer;
        }

        FLinearColor RimColor = BodyColor * 0.24f;
        RimColor.A = 1.0f;
        constexpr int32 ShadeLayers = 24;
        FLinearColor Highlight = BodyColor * 1.35f;
        Highlight.A = 1.0f;
        const float RadialStep = Radius / static_cast<float>(ShadeLayers);

        // Overlapping circular strokes form a reliable solid disk in Slate.
        // Shifting the brighter inner rings toward the upper left gives the
        // miniature body a simple directional-lighted sphere appearance.
        for (int32 Index = 0; Index < ShadeLayers; ++Index)
        {
            const float Fraction = static_cast<float>(Index) /
                static_cast<float>(ShadeLayers - 1);
            const float LayerRadius = FMath::Max(
                0.5f,
                Radius - (static_cast<float>(Index) + 0.5f) * RadialStep);
            const FVector2f LayerCenter = Center - FVector2f(
                Radius * 0.08f * Fraction,
                Radius * 0.08f * Fraction);
            DrawCircleStroke(
                Elements,
                FirstLayer + Index,
                Geometry,
                LayerCenter,
                LayerRadius,
                FMath::Lerp(RimColor, Highlight, Fraction),
                Effects,
                FMath::Max(1.5f, RadialStep * 2.6f),
                40);
        }

        FLinearColor OutlineColor = BodyColor * 0.75f;
        OutlineColor.A = 1.0f;
        DrawCircleStroke(
            Elements,
            FirstLayer + ShadeLayers,
            Geometry,
            Center,
            Radius,
            OutlineColor,
            Effects,
            1.5f,
            40);
        return FirstLayer + ShadeLayers;
    }
}

void UTGSolarSystemOverviewWidget::InitializeForPlayback(
    ATGSimulationPlaybackActor* InPlaybackActor)
{
    if (IsValid(PlaybackActor))
    {
        PlaybackActor->OnPlaybackTimeChanged.RemoveDynamic(
            this,
            &UTGSolarSystemOverviewWidget::HandlePlaybackTimeChanged);
    }
    PlaybackActor = InPlaybackActor;
    if (IsValid(PlaybackActor))
    {
        PlaybackActor->OnPlaybackTimeChanged.AddUniqueDynamic(
            this,
            &UTGSolarSystemOverviewWidget::HandlePlaybackTimeChanged);
    }
    SetIsFocusable(true);
    SetVisibility(ESlateVisibility::Visible);
    SetClipping(EWidgetClipping::ClipToBounds);
    InvalidateLayoutAndVolatility();
}

void UTGSolarSystemOverviewWidget::SetBodyCenteredView(const bool bEnabled)
{
    if (bBodyCenteredView == bEnabled)
    {
        return;
    }
    bBodyCenteredView = bEnabled;
    ViewRotation = FQuat(FRotator(55.0f, -25.0f, 0.0f));
    ViewZoom = 1.0;
    ViewPanPixels = FVector2D::ZeroVector;
    InvalidateLayoutAndVolatility();
}

void UTGSolarSystemOverviewWidget::NativeDestruct()
{
    if (IsValid(PlaybackActor))
    {
        PlaybackActor->SetVisualizationUiCapturesCameraInput(false);
        PlaybackActor->OnPlaybackTimeChanged.RemoveDynamic(
            this,
            &UTGSolarSystemOverviewWidget::HandlePlaybackTimeChanged);
    }
    Super::NativeDestruct();
}

void UTGSolarSystemOverviewWidget::HandlePlaybackTimeChanged(
    double ElapsedSimulationSeconds,
    double EphemerisTimeTdbSeconds,
    double NormalizedTime)
{
    // Cached histories stay fixed; only the current-position markers repaint.
    InvalidateLayoutAndVolatility();
}

void UTGSolarSystemOverviewWidget::FocusSpacecraftPath()
{
    if (!IsValid(PlaybackActor))
    {
        return;
    }
    const TArray<FTGSolarSystemOverviewPath>& Paths =
        PlaybackActor->GetSolarSystemOverviewPaths();
    const FTGSolarSystemOverviewPath* SpacecraftPath = Paths.FindByPredicate(
        [](const FTGSolarSystemOverviewPath& Path)
        {
            return Path.bSpacecraft;
        });
    if (
        SpacecraftPath == nullptr ||
        SpacecraftPath->PositionsIcrfMeters.IsEmpty())
    {
        return;
    }

    FVector GlobalMinimum(
        TNumericLimits<double>::Max(),
        TNumericLimits<double>::Max(),
        TNumericLimits<double>::Max());
    FVector GlobalMaximum(
        TNumericLimits<double>::Lowest(),
        TNumericLimits<double>::Lowest(),
        TNumericLimits<double>::Lowest());
    for (const FTGSolarSystemOverviewPath& Path : Paths)
    {
        for (const FVector& Position : Path.PositionsIcrfMeters)
        {
            GlobalMinimum = GlobalMinimum.ComponentMin(Position);
            GlobalMaximum = GlobalMaximum.ComponentMax(Position);
        }
    }
    const FVector GlobalCenter = (GlobalMinimum + GlobalMaximum) * 0.5;

    FVector2D GlobalProjectedMinimum(
        TNumericLimits<double>::Max(),
        TNumericLimits<double>::Max());
    FVector2D GlobalProjectedMaximum(
        TNumericLimits<double>::Lowest(),
        TNumericLimits<double>::Lowest());
    FVector2D TargetMinimum = GlobalProjectedMinimum;
    FVector2D TargetMaximum = GlobalProjectedMaximum;
    for (const FTGSolarSystemOverviewPath& Path : Paths)
    {
        for (const FVector& Position : Path.PositionsIcrfMeters)
        {
            const FVector Projected =
                TGSolarSystemOverviewPrivate::RotateForView(
                    Position - GlobalCenter,
                    ViewRotation);
            GlobalProjectedMinimum.X = FMath::Min(
                GlobalProjectedMinimum.X,
                Projected.X);
            GlobalProjectedMinimum.Y = FMath::Min(
                GlobalProjectedMinimum.Y,
                Projected.Y);
            GlobalProjectedMaximum.X = FMath::Max(
                GlobalProjectedMaximum.X,
                Projected.X);
            GlobalProjectedMaximum.Y = FMath::Max(
                GlobalProjectedMaximum.Y,
                Projected.Y);
            if (Path.bSpacecraft)
            {
                TargetMinimum.X = FMath::Min(TargetMinimum.X, Projected.X);
                TargetMinimum.Y = FMath::Min(TargetMinimum.Y, Projected.Y);
                TargetMaximum.X = FMath::Max(TargetMaximum.X, Projected.X);
                TargetMaximum.Y = FMath::Max(TargetMaximum.Y, Projected.Y);
            }
        }
    }

    const FVector2D LocalSize = GetCachedGeometry().GetLocalSize();
    const double GlobalScale = FMath::Min(
        static_cast<double>(LocalSize.X) * 0.88 /
            FMath::Max(
                GlobalProjectedMaximum.X - GlobalProjectedMinimum.X,
                1.0),
        static_cast<double>(LocalSize.Y) * 0.88 /
            FMath::Max(
                GlobalProjectedMaximum.Y - GlobalProjectedMinimum.Y,
                1.0));
    const double TargetScale = FMath::Min(
        static_cast<double>(LocalSize.X) * 0.72 /
            FMath::Max(TargetMaximum.X - TargetMinimum.X, 1.0),
        static_cast<double>(LocalSize.Y) * 0.72 /
            FMath::Max(TargetMaximum.Y - TargetMinimum.Y, 1.0));
    ViewZoom = FMath::Clamp(
        TargetScale / FMath::Max(GlobalScale, UE_DOUBLE_SMALL_NUMBER),
        0.05,
        1.0e9);
    const FVector2D TargetCenter = (TargetMinimum + TargetMaximum) * 0.5;
    ViewPanPixels = FVector2D(
        -TargetCenter.X * GlobalScale * ViewZoom,
        TargetCenter.Y * GlobalScale * ViewZoom);
    InvalidateLayoutAndVolatility();
}

void UTGSolarSystemOverviewWidget::ResetSolarSystemView()
{
    ViewZoom = 1.0;
    ViewPanPixels = FVector2D::ZeroVector;
    InvalidateLayoutAndVolatility();
}

int32 UTGSolarSystemOverviewWidget::NativePaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    const int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    const bool bParentEnabled) const
{
    const int32 BaseLayer = Super::NativePaint(
        Args,
        AllottedGeometry,
        MyCullingRect,
        OutDrawElements,
        LayerId,
        InWidgetStyle,
        bParentEnabled);

    if (!IsValid(PlaybackActor))
    {
        return BaseLayer;
    }

    if (bBodyCenteredView)
    {
        return PaintClosestBodyView(
            AllottedGeometry,
            OutDrawElements,
            BaseLayer,
            bParentEnabled);
    }

    const TArray<FTGSolarSystemOverviewPath>& Paths =
        PlaybackActor->GetSolarSystemOverviewPaths();
    if (Paths.IsEmpty())
    {
        return BaseLayer;
    }

    FVector Minimum(
        TNumericLimits<double>::Max(),
        TNumericLimits<double>::Max(),
        TNumericLimits<double>::Max());
    FVector Maximum(
        TNumericLimits<double>::Lowest(),
        TNumericLimits<double>::Lowest(),
        TNumericLimits<double>::Lowest());
    bool bHasPoint = false;
    for (const FTGSolarSystemOverviewPath& Path : Paths)
    {
        for (const FVector& Position : Path.PositionsIcrfMeters)
        {
            Minimum.X = FMath::Min(Minimum.X, Position.X);
            Minimum.Y = FMath::Min(Minimum.Y, Position.Y);
            Minimum.Z = FMath::Min(Minimum.Z, Position.Z);
            Maximum.X = FMath::Max(Maximum.X, Position.X);
            Maximum.Y = FMath::Max(Maximum.Y, Position.Y);
            Maximum.Z = FMath::Max(Maximum.Z, Position.Z);
            bHasPoint = true;
        }
    }
    if (!bHasPoint)
    {
        return BaseLayer;
    }

    const FVector Center = (Minimum + Maximum) * 0.5;
    FVector2D ProjectedMinimum(
        TNumericLimits<double>::Max(),
        TNumericLimits<double>::Max());
    FVector2D ProjectedMaximum(
        TNumericLimits<double>::Lowest(),
        TNumericLimits<double>::Lowest());
    for (const FTGSolarSystemOverviewPath& Path : Paths)
    {
        for (const FVector& Position : Path.PositionsIcrfMeters)
        {
            const FVector Projected =
                TGSolarSystemOverviewPrivate::RotateForView(
                    Position - Center,
                    ViewRotation);
            ProjectedMinimum.X = FMath::Min(ProjectedMinimum.X, Projected.X);
            ProjectedMinimum.Y = FMath::Min(ProjectedMinimum.Y, Projected.Y);
            ProjectedMaximum.X = FMath::Max(ProjectedMaximum.X, Projected.X);
            ProjectedMaximum.Y = FMath::Max(ProjectedMaximum.Y, Projected.Y);
        }
    }

    const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
    const double RangeX = FMath::Max(
        ProjectedMaximum.X - ProjectedMinimum.X,
        1.0);
    const double RangeY = FMath::Max(
        ProjectedMaximum.Y - ProjectedMinimum.Y,
        1.0);
    const double Scale = FMath::Min(
        static_cast<double>(LocalSize.X) * 0.88 / RangeX,
        static_cast<double>(LocalSize.Y) * 0.88 / RangeY) *
        ViewZoom;
    const ESlateDrawEffect DrawEffects = bParentEnabled
        ? ESlateDrawEffect::None
        : ESlateDrawEffect::DisabledEffect;

    int32 DrawLayer = BaseLayer + 1;
    for (const FTGSolarSystemOverviewPath& Path : Paths)
    {
        if (Path.PositionsIcrfMeters.Num() < 2)
        {
            continue;
        }
        TArray<FVector2f> PathPoints;
        PathPoints.Reserve(Path.PositionsIcrfMeters.Num());
        for (const FVector& Position : Path.PositionsIcrfMeters)
        {
            PathPoints.Add(TGSolarSystemOverviewPrivate::MapPoint(
                Position,
                Center,
                ViewRotation,
                LocalSize,
                Scale,
                ViewPanPixels));
        }
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            DrawLayer,
            AllottedGeometry.ToPaintGeometry(),
            PathPoints,
            DrawEffects,
            Path.Color.CopyWithNewOpacity(Path.bSpacecraft ? 0.96f : 0.68f),
            true,
            Path.bSpacecraft ? 2.0f : 1.35f);
    }

    ++DrawLayer;
    const FSlateFontInfo LabelFont =
        TGUiTheme::GetSlateFont(ETGUiTextStyle::Caption);
    for (const FTGSolarSystemOverviewPath& Path : Paths)
    {
        FVector CurrentPosition;
        if (!PlaybackActor->EvaluateOverviewPathAtCurrentTime(
                Path,
                CurrentPosition))
        {
            continue;
        }
        const FVector2f MarkerPosition =
            TGSolarSystemOverviewPrivate::MapPoint(
                CurrentPosition,
                Center,
                ViewRotation,
                LocalSize,
                Scale,
                ViewPanPixels);
        if (
            MarkerPosition.X < -24.0f ||
            MarkerPosition.Y < -24.0f ||
            MarkerPosition.X > LocalSize.X + 24.0f ||
            MarkerPosition.Y > LocalSize.Y + 24.0f)
        {
            continue;
        }
        const float MarkerRadius = Path.bSpacecraft
            ? 5.0f
            : (Path.Key == TEXT("sun") ? 8.0f : 4.0f);
        TGSolarSystemOverviewPrivate::DrawMarker(
            OutDrawElements,
            DrawLayer,
            AllottedGeometry,
            MarkerPosition,
            MarkerRadius,
            Path.Color,
            DrawEffects);

        const FString Label = Path.DisplayName.IsEmpty()
            ? Path.Key
            : Path.DisplayName;
        FSlateDrawElement::MakeText(
            OutDrawElements,
            DrawLayer + 1,
            AllottedGeometry.ToOffsetPaintGeometry(FVector2D(
                MarkerPosition.X + MarkerRadius + 5.0f,
                MarkerPosition.Y - 7.0f)),
            Label,
            LabelFont,
            DrawEffects,
            TGUiTheme::GetPalette().TextPrimary);
    }

    return DrawLayer + 1;
}

int32 UTGSolarSystemOverviewWidget::PaintClosestBodyView(
    const FGeometry& AllottedGeometry,
    FSlateWindowElementList& OutDrawElements,
    const int32 BaseLayer,
    const bool bParentEnabled) const
{
    FTGClosestBodyOverviewData Data;
    if (!PlaybackActor->BuildClosestBodyOverviewData(Data))
    {
        return BaseLayer;
    }

    const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
    if (LocalSize.X <= 1.0 || LocalSize.Y <= 1.0)
    {
        return BaseLayer;
    }

    TArray<FVector> RotatedPath;
    RotatedPath.Reserve(Data.SpacecraftPathRelativeMeters.Num());
    double MaximumAbsoluteX = Data.BodyReferenceRadiusMeters;
    double MaximumAbsoluteY = Data.BodyReferenceRadiusMeters;
    for (const FVector& RelativePosition : Data.SpacecraftPathRelativeMeters)
    {
        const FVector Rotated =
            TGSolarSystemOverviewPrivate::RotateForView(
                RelativePosition,
                ViewRotation);
        RotatedPath.Add(Rotated);
        MaximumAbsoluteX = FMath::Max(MaximumAbsoluteX, FMath::Abs(Rotated.X));
        MaximumAbsoluteY = FMath::Max(MaximumAbsoluteY, FMath::Abs(Rotated.Y));
    }

    const FVector RotatedCurrent =
        TGSolarSystemOverviewPrivate::RotateForView(
            Data.CurrentSpacecraftPositionRelativeMeters,
            ViewRotation);
    MaximumAbsoluteX = FMath::Max(MaximumAbsoluteX, FMath::Abs(RotatedCurrent.X));
    MaximumAbsoluteY = FMath::Max(MaximumAbsoluteY, FMath::Abs(RotatedCurrent.Y));

    const double Scale = FMath::Min(
        static_cast<double>(LocalSize.X) * 0.42 /
            FMath::Max(MaximumAbsoluteX, 1.0),
        static_cast<double>(LocalSize.Y) * 0.42 /
            FMath::Max(MaximumAbsoluteY, 1.0)) *
        ViewZoom;
    const FVector2f ViewCenter(
        static_cast<float>(LocalSize.X * 0.5 + ViewPanPixels.X),
        static_cast<float>(LocalSize.Y * 0.5 + ViewPanPixels.Y));
    const ESlateDrawEffect DrawEffects = bParentEnabled
        ? ESlateDrawEffect::None
        : ESlateDrawEffect::DisabledEffect;

    TArray<FVector2f> ScreenPath;
    ScreenPath.Reserve(RotatedPath.Num());
    for (const FVector& Rotated : RotatedPath)
    {
        ScreenPath.Add(ViewCenter + FVector2f(
            static_cast<float>(Rotated.X * Scale),
            static_cast<float>(-Rotated.Y * Scale)));
    }

    const FLinearColor PathColor(0.84f, 0.90f, 0.95f, 0.96f);
    int32 DrawLayer = BaseLayer + 1;
    if (ScreenPath.Num() >= 2)
    {
        // Draw the complete orbit first. The sphere then masks its far-side
        // projection, and the front-side runs are restored afterward.
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            DrawLayer,
            AllottedGeometry.ToPaintGeometry(),
            ScreenPath,
            DrawEffects,
            PathColor,
            true,
            2.0f);
    }

    const float BodyRadiusPixels = static_cast<float>(
        Data.BodyReferenceRadiusMeters * Scale);
    DrawLayer = TGSolarSystemOverviewPrivate::DrawShadedSphere(
        OutDrawElements,
        DrawLayer + 1,
        AllottedGeometry,
        ViewCenter,
        BodyRadiusPixels,
        Data.BodyColor,
        DrawEffects);

    TArray<FVector2f> FrontRun;
    auto FlushFrontRun = [&]()
    {
        if (FrontRun.Num() >= 2)
        {
            FSlateDrawElement::MakeLines(
                OutDrawElements,
                DrawLayer + 1,
                AllottedGeometry.ToPaintGeometry(),
                FrontRun,
                DrawEffects,
                PathColor,
                true,
                2.0f);
        }
        FrontRun.Reset();
    };
    for (int32 PointIndex = 1; PointIndex < RotatedPath.Num(); ++PointIndex)
    {
        const bool bFrontSegment =
            (RotatedPath[PointIndex - 1].Z + RotatedPath[PointIndex].Z) >= 0.0;
        if (bFrontSegment)
        {
            if (FrontRun.IsEmpty())
            {
                FrontRun.Add(ScreenPath[PointIndex - 1]);
            }
            FrontRun.Add(ScreenPath[PointIndex]);
        }
        else
        {
            FlushFrontRun();
        }
    }
    FlushFrontRun();
    ++DrawLayer;

    const FVector2f CurrentMarker = ViewCenter + FVector2f(
        static_cast<float>(RotatedCurrent.X * Scale),
        static_cast<float>(-RotatedCurrent.Y * Scale));
    TGSolarSystemOverviewPrivate::DrawMarker(
        OutDrawElements,
        DrawLayer + 1,
        AllottedGeometry,
        CurrentMarker,
        5.0f,
        FLinearColor(0.15f, 0.95f, 0.82f),
        DrawEffects);

    const FSlateFontInfo LabelFont =
        TGUiTheme::GetSlateFont(ETGUiTextStyle::Caption);
    FSlateDrawElement::MakeText(
        OutDrawElements,
        DrawLayer + 2,
        AllottedGeometry.ToOffsetPaintGeometry(FVector2D(10.0f, 8.0f)),
        FString::Printf(
            TEXT("Closest body: %s"),
            *Data.BodyDisplayName),
        LabelFont,
        DrawEffects,
        TGUiTheme::GetPalette().TextPrimary);
    FSlateDrawElement::MakeText(
        OutDrawElements,
        DrawLayer + 2,
        AllottedGeometry.ToOffsetPaintGeometry(FVector2D(
            CurrentMarker.X + 9.0f,
            CurrentMarker.Y - 8.0f)),
        TEXT("Spacecraft"),
        LabelFont,
        DrawEffects,
        TGUiTheme::GetPalette().TextPrimary);

    return DrawLayer + 2;
}

FReply UTGSolarSystemOverviewWidget::NativeOnMouseButtonDown(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    const FKey Button = InMouseEvent.GetEffectingButton();
    if (
        Button != EKeys::LeftMouseButton &&
        Button != EKeys::MiddleMouseButton &&
        Button != EKeys::RightMouseButton)
    {
        return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    }
    bRotating = Button == EKeys::LeftMouseButton;
    bPanning = !bRotating;
    const TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
    return CachedWidget.IsValid()
        ? FReply::Handled().CaptureMouse(CachedWidget.ToSharedRef())
        : FReply::Handled();
}

FReply UTGSolarSystemOverviewWidget::NativeOnMouseButtonUp(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    const FKey Button = InMouseEvent.GetEffectingButton();
    if (
        (Button == EKeys::LeftMouseButton && !bRotating) ||
        ((Button == EKeys::MiddleMouseButton ||
          Button == EKeys::RightMouseButton) && !bPanning))
    {
        return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
    }
    bRotating = false;
    bPanning = false;
    if (
        IsValid(PlaybackActor) &&
        !InGeometry.IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
    {
        PlaybackActor->SetVisualizationUiCapturesCameraInput(false);
    }
    return FReply::Handled().ReleaseMouseCapture();
}

FReply UTGSolarSystemOverviewWidget::NativeOnMouseMove(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    if (!bRotating && !bPanning)
    {
        return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
    }
    const FVector2D Delta = InMouseEvent.GetCursorDelta();
    if (bRotating)
    {
        constexpr double RadiansPerPixel =
            0.45 * UE_DOUBLE_PI / 180.0;
        // Pre-multiplication applies these axes in graph-camera coordinates:
        // horizontal dragging orbits around screen-up, while vertical
        // dragging orbits around screen-right. A quaternion keeps both axes
        // active through every orientation without a pole or Euler lock.
        const FQuat HorizontalOrbit(
            FVector::YAxisVector,
            -Delta.X * RadiansPerPixel);
        const FQuat VerticalOrbit(
            FVector::XAxisVector,
            -Delta.Y * RadiansPerPixel);
        ViewRotation =
            (VerticalOrbit * HorizontalOrbit * ViewRotation).GetNormalized();
    }
    else
    {
        ViewPanPixels += Delta;
    }
    InvalidateLayoutAndVolatility();
    return FReply::Handled();
}

void UTGSolarSystemOverviewWidget::NativeOnMouseEnter(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
    if (IsValid(PlaybackActor))
    {
        PlaybackActor->SetVisualizationUiCapturesCameraInput(true);
    }
}

void UTGSolarSystemOverviewWidget::NativeOnMouseLeave(
    const FPointerEvent& InMouseEvent)
{
    if (IsValid(PlaybackActor) && !bRotating && !bPanning)
    {
        PlaybackActor->SetVisualizationUiCapturesCameraInput(false);
    }
    Super::NativeOnMouseLeave(InMouseEvent);
}

FReply UTGSolarSystemOverviewWidget::NativeOnMouseWheel(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    const double PreviousZoom = ViewZoom;
    ViewZoom = FMath::Clamp(
        PreviousZoom * FMath::Pow(1.35, InMouseEvent.GetWheelDelta()),
        0.05,
        1.0e9);

    // Keep the ICRF point under the cursor stationary while zooming. This is
    // what makes inner-system and spacecraft-scale inspection practical even
    // while the fitted view still preserves real distance proportions.
    const FVector2D CursorLocal = InGeometry.AbsoluteToLocal(
        InMouseEvent.GetScreenSpacePosition());
    const FVector2D FromViewportCenter =
        CursorLocal - InGeometry.GetLocalSize() * 0.5;
    const double ZoomRatio = ViewZoom / PreviousZoom;
    ViewPanPixels =
        FromViewportCenter -
        (FromViewportCenter - ViewPanPixels) * ZoomRatio;
    InvalidateLayoutAndVolatility();
    return FReply::Handled();
}
