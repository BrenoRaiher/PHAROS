// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Visualization/TGSimulationPlaybackActor.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/LineBatchComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DefaultPawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Simulation/TGCelestialCatalogLibrary.h"
#include "SpiceBridge.h"
#include "Styling/CoreStyle.h"
#include "UI/Theme/TGUiTheme.h"
#include "UI/Visualization/TGVisualizationHudWidget.h"
#include "Visualization/TGSpacecraftOrbitCameraActor.h"
#include "Widgets/SLeafWidget.h"

namespace TGSimulationPlaybackPresentationPrivate
{
    FString PrettifyKey(const FString& Key)
    {
        FString Result = Key.Replace(TEXT("_"), TEXT(" "));
        bool bCapitalize = true;
        for (int32 Index = 0; Index < Result.Len(); ++Index)
        {
            if (FChar::IsWhitespace(Result[Index]))
            {
                bCapitalize = true;
            }
            else if (bCapitalize)
            {
                Result[Index] = FChar::ToUpper(Result[Index]);
                bCapitalize = false;
            }
        }
        return Result;
    }

    FString ResolveCelestialDisplayName(const FString& Key)
    {
        for (const FTGCelestialCatalogEntry& Entry :
             UTGCelestialCatalogLibrary::GetCelestialCatalog())
        {
            if (Entry.SpiceTarget.Equals(Key, ESearchCase::IgnoreCase) ||
                Entry.CatalogKey.ToString().Equals(
                    Key,
                    ESearchCase::IgnoreCase))
            {
                return Entry.DisplayName.ToString();
            }
        }

        return PrettifyKey(Key);
    }

    FString VectorLine(
        const TCHAR* Label,
        const FVector& Value,
        const TCHAR* Unit)
    {
        return FString::Printf(
            TEXT("%s: [%.6g, %.6g, %.6g] %s"),
            Label,
            Value.X,
            Value.Y,
            Value.Z,
            Unit);
    }

    double ApparentDiameterPixels(
        const double RadiusMeters,
        const double DistanceMeters,
        const double ViewportHeightPixels,
        const double VerticalFovRadians,
        const double Magnification)
    {
        if (RadiusMeters <= 0.0 || DistanceMeters <= 0.0)
        {
            return DistanceMeters <= 0.0
                ? TNumericLimits<double>::Max()
                : 0.0;
        }
        const double AngularDiameter = 2.0 * FMath::Asin(FMath::Clamp(
            RadiusMeters * FMath::Max(0.0, Magnification) / DistanceMeters,
            0.0,
            1.0));
        return AngularDiameter * ViewportHeightPixels /
            FMath::Max(VerticalFovRadians, UE_DOUBLE_SMALL_NUMBER);
    }

    void AddPathSample(
        FTGSolarSystemOverviewPath& Path,
        const double EphemerisTime,
        const FVector& PositionIcrfMeters)
    {
        Path.EphemerisTimes.Add(EphemerisTime);
        Path.PositionsIcrfMeters.Add(PositionIcrfMeters);
    }

    FLinearColor ResolveArrowDisplayColor(
        const FLinearColor& SourceColor,
        const bool bArrowHead)
    {
        FLinearColor Hsv = SourceColor.GetClamped().LinearRGBToHSV();

        // Neutral vectors remain neutral. Colored vectors keep their semantic
        // hue, but use a controlled saturation and value so auto exposure does
        // not wash every unlit arrow toward the same pastel white.
        if (Hsv.G < 0.08f)
        {
            return bArrowHead
                ? FLinearColor(0.38f, 0.43f, 0.48f, 1.0f)
                : FLinearColor(0.16f, 0.19f, 0.22f, 1.0f);
        }

        Hsv.G = FMath::Clamp(FMath::Max(Hsv.G, 0.72f), 0.0f, 1.0f);
        Hsv.B = bArrowHead ? 0.36f : 0.18f;
        FLinearColor DisplayColor = Hsv.HSVToLinearRGB();
        DisplayColor.A = 1.0f;
        return DisplayColor;
    }

    /** Crisp body labels anchored above the projected world-space reticles. */
    class STGBodyReticleLabelOverlay final : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(STGBodyReticleLabelOverlay) {}
            SLATE_ARGUMENT(
                TWeakObjectPtr<ATGSimulationPlaybackActor>,
                PlaybackActor)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            PlaybackActor = InArgs._PlaybackActor;
            SetVisibility(EVisibility::HitTestInvisible);
        }

        virtual FVector2D ComputeDesiredSize(float) const override
        {
            return FVector2D::ZeroVector;
        }

        virtual bool ComputeVolatility() const override
        {
            return true;
        }

        virtual int32 OnPaint(
            const FPaintArgs& Args,
            const FGeometry& AllottedGeometry,
            const FSlateRect& MyCullingRect,
            FSlateWindowElementList& OutDrawElements,
            const int32 LayerId,
            const FWidgetStyle& InWidgetStyle,
            const bool bParentEnabled) const override
        {
            const ATGSimulationPlaybackActor* Actor = PlaybackActor.Get();
            const UWorld* World = Actor != nullptr ? Actor->GetWorld() : nullptr;
            const APlayerController* PlayerController = World != nullptr
                ? World->GetFirstPlayerController()
                : nullptr;
            const FVector2D ViewSize = AllottedGeometry.GetLocalSize();
            if (Actor == nullptr || PlayerController == nullptr ||
                ViewSize.X <= 1.0 || ViewSize.Y <= 1.0 ||
                !FSlateApplication::IsInitialized())
            {
                return LayerId;
            }

            const FSlateFontInfo LabelFont =
                TGUiTheme::GetSlateFont(ETGUiTextStyle::BodyStrong);
            const TSharedRef<FSlateFontMeasure> FontMeasure =
                FSlateApplication::Get()
                    .GetRenderer()
                    ->GetFontMeasureService();
            const ESlateDrawEffect DrawEffects = ShouldBeEnabled(
                bParentEnabled)
                ? ESlateDrawEffect::None
                : ESlateDrawEffect::DisabledEffect;
            const FLinearColor WidgetTint =
                InWidgetStyle.GetColorAndOpacityTint();
            const FVector ReticleOrigin = Actor->GetActorLocation();

            const auto ProjectWorldPoint = [PlayerController](
                    const FVector& WorldPoint,
                    FVector2D& OutScreenPosition)
            {
                return UWidgetLayoutLibrary::
                    ProjectWorldLocationToWidgetPosition(
                        PlayerController,
                        WorldPoint,
                        OutScreenPosition,
                        true);
            };

            int32 HighestLayer = LayerId;

            for (const FTGVisualizationArrowInfo& Info :
                 Actor->GetVisualizationArrows())
            {
                if (!Info.bVisible ||
                    Info.Quantity !=
                        ETGVisualizationArrowQuantity::BodyDirection ||
                    !Info.bBodyTargetAvailable)
                {
                    continue;
                }

                constexpr double MinimumAngularRadiusRadians =
                    1.0 * UE_DOUBLE_PI / 180.0;
                constexpr double ApparentRadiusMargin = 1.05;
                constexpr double MaximumAngularRadiusRadians =
                    80.0 * UE_DOUBLE_PI / 180.0;
                constexpr int32 ProjectionSampleCount = 96;

                const FVector Normal =
                    Info.CurrentBodyDirectionUnreal.GetSafeNormal();
                const double DistanceWorldUnits = FVector::Distance(
                    ReticleOrigin,
                    Info.CurrentBodyWorldLocation);
                if (Normal.IsNearlyZero() ||
                    DistanceWorldUnits <= UE_DOUBLE_SMALL_NUMBER)
                {
                    continue;
                }

                const double AngularRadius = FMath::Clamp(
                    FMath::Max(
                        MinimumAngularRadiusRadians,
                        Info.CurrentBodyAngularRadiusRadians *
                            ApparentRadiusMargin),
                    MinimumAngularRadiusRadians,
                    MaximumAngularRadiusRadians);
                const double RadiusWorldUnits =
                    DistanceWorldUnits * FMath::Tan(AngularRadius);

                FVector RingRight = FVector::CrossProduct(
                    FVector::UpVector,
                    Normal).GetSafeNormal();
                if (RingRight.IsNearlyZero())
                {
                    RingRight = FVector::CrossProduct(
                        FVector::ForwardVector,
                        Normal).GetSafeNormal();
                }
                const FVector RingUp = FVector::CrossProduct(
                    Normal,
                    RingRight).GetSafeNormal();
                if (RingRight.IsNearlyZero() || RingUp.IsNearlyZero())
                {
                    continue;
                }

                FVector2D ReticleCenter = FVector2D::ZeroVector;
                if (!ProjectWorldPoint(
                        Info.CurrentBodyWorldLocation,
                        ReticleCenter))
                {
                    continue;
                }

                FVector2D ReticleTop = FVector2D::ZeroVector;
                bool bFoundReticleTop = false;
                for (int32 SampleIndex = 0;
                     SampleIndex < ProjectionSampleCount;
                     ++SampleIndex)
                {
                    const double Angle = 2.0 * UE_DOUBLE_PI *
                        static_cast<double>(SampleIndex) /
                        static_cast<double>(ProjectionSampleCount);
                    const FVector RingPoint =
                        Info.CurrentBodyWorldLocation +
                        (RingRight * FMath::Cos(Angle) +
                            RingUp * FMath::Sin(Angle)) *
                            RadiusWorldUnits;
                    FVector2D ProjectedPoint;
                    if (ProjectWorldPoint(RingPoint, ProjectedPoint) &&
                        (!bFoundReticleTop ||
                            ProjectedPoint.Y < ReticleTop.Y))
                    {
                        ReticleTop = ProjectedPoint;
                        bFoundReticleTop = true;
                    }
                }

                if (!bFoundReticleTop ||
                    ReticleTop.X < -200.0 ||
                    ReticleTop.X > ViewSize.X + 200.0 ||
                    ReticleTop.Y < -100.0 ||
                    ReticleTop.Y > ViewSize.Y + 100.0)
                {
                    continue;
                }

                const FString& Label = Info.DisplayName;
                const FVector2D TextSize = FontMeasure->Measure(
                    Label,
                    LabelFont);
                const FVector2D Padding(5.0, 2.0);
                constexpr double LabelGapPixels = 8.0;
                const FVector2D TextPosition(
                    ReticleCenter.X - TextSize.X * 0.5,
                    ReticleTop.Y - LabelGapPixels - Padding.Y -
                        TextSize.Y);
                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    LayerId,
                    AllottedGeometry.ToPaintGeometry(
                        TextSize + Padding * 2.0,
                        FSlateLayoutTransform(TextPosition - Padding)),
                    FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
                    DrawEffects,
                    FLinearColor::Black *
                        WidgetTint);
                FSlateDrawElement::MakeText(
                    OutDrawElements,
                    LayerId + 1,
                    AllottedGeometry.ToOffsetPaintGeometry(TextPosition),
                    Label,
                    LabelFont,
                    DrawEffects,
                    Info.DisplayColor * WidgetTint);
                HighestLayer = FMath::Max(HighestLayer, LayerId + 1);
            }
            return HighestLayer;
        }

    private:
        TWeakObjectPtr<ATGSimulationPlaybackActor> PlaybackActor;
    };
}

TArray<FTGVisualizationArrowInfo>
ATGSimulationPlaybackActor::GetVisualizationArrows() const
{
    return VisualizationArrowInfos;
}

bool ATGSimulationPlaybackActor::SetVisualizationArrowVisible(
    const FName ArrowId,
    const bool bVisible)
{
    FTGVisualizationArrowInfo* Info = VisualizationArrowInfos.FindByPredicate(
        [ArrowId](const FTGVisualizationArrowInfo& Candidate)
        {
            return Candidate.ArrowId == ArrowId;
        });
    if (Info == nullptr)
    {
        return false;
    }

    Info->bVisible = bVisible;
    if (bResultLoaded)
    {
        FText IgnoredError;
        ApplyCurrentFrame(IgnoredError);
    }
    return true;
}

bool ATGSimulationPlaybackActor::IsVisualizationArrowVisible(
    const FName ArrowId) const
{
    const FTGVisualizationArrowInfo* Info =
        VisualizationArrowInfos.FindByPredicate(
            [ArrowId](const FTGVisualizationArrowInfo& Candidate)
            {
                return Candidate.ArrowId == ArrowId;
            });
    return Info != nullptr && Info->bVisible;
}

bool ATGSimulationPlaybackActor::GetVisualizationArrowInfo(
    const FName ArrowId,
    FTGVisualizationArrowInfo& OutInfo) const
{
    const FTGVisualizationArrowInfo* Info =
        VisualizationArrowInfos.FindByPredicate(
            [ArrowId](const FTGVisualizationArrowInfo& Candidate)
            {
                return Candidate.ArrowId == ArrowId;
            });
    if (Info == nullptr)
    {
        OutInfo = FTGVisualizationArrowInfo{};
        return false;
    }
    OutInfo = *Info;
    return true;
}

const TArray<FTGSolarSystemOverviewPath>&
ATGSimulationPlaybackActor::GetSolarSystemOverviewPaths() const
{
    return SolarSystemOverviewPaths;
}

bool ATGSimulationPlaybackActor::EvaluateOverviewPathAtCurrentTime(
    const FTGSolarSystemOverviewPath& Path,
    FVector& OutPositionIcrfMeters) const
{
    OutPositionIcrfMeters = FVector::ZeroVector;
    const int32 Count = FMath::Min(
        Path.EphemerisTimes.Num(),
        Path.PositionsIcrfMeters.Num());
    if (Count <= 0)
    {
        return false;
    }

    const double Time = GetCurrentEphemerisTimeTdbSeconds();
    if (
        Count >= 2 &&
        Time >= Path.EphemerisTimes[0] &&
        Time <= Path.EphemerisTimes[Count - 1])
    {
        int32 Low = 0;
        int32 High = Count - 1;
        while (Low + 1 < High)
        {
            const int32 Middle = Low + (High - Low) / 2;
            if (Path.EphemerisTimes[Middle] <= Time)
            {
                Low = Middle;
            }
            else
            {
                High = Middle;
            }
        }

        const double Span = Path.EphemerisTimes[High] -
            Path.EphemerisTimes[Low];
        const double Alpha = Span > 0.0
            ? (Time - Path.EphemerisTimes[Low]) / Span
            : 0.0;
        // Use the exact same segment that NativePaint draws. The marker is
        // therefore guaranteed to sit on its displayed orbit instead of
        // showing the small chord-versus-SPICE-curve discrepancy.
        OutPositionIcrfMeters = FMath::Lerp(
            Path.PositionsIcrfMeters[Low],
            Path.PositionsIcrfMeters[High],
            Alpha);
        return true;
    }

    // Outside the cached interval, retain an exact current SPICE marker for
    // unusually long missions instead of freezing it at a path endpoint.
    if (!Path.SpiceTargetName.IsEmpty())
    {
        FVector IgnoredVelocity;
        FString SpiceMessage;
        if (FSpiceBridge::GetBodyICRFStateSI(
                Path.SpiceTargetName,
                Time,
                OutPositionIcrfMeters,
                IgnoredVelocity,
                SpiceMessage))
        {
            return true;
        }
    }

    OutPositionIcrfMeters = Time <= Path.EphemerisTimes[0]
        ? Path.PositionsIcrfMeters[0]
        : Path.PositionsIcrfMeters[Count - 1];
    return true;
}

bool ATGSimulationPlaybackActor::BuildClosestBodyOverviewData(
    FTGClosestBodyOverviewData& OutData) const
{
    using namespace TGSimulationPlaybackPresentationPrivate;

    OutData = FTGClosestBodyOverviewData{};
    if (!bResultLoaded || NumericRows.IsEmpty())
    {
        return false;
    }

    const double CurrentTime = GetCurrentEphemerisTimeTdbSeconds();
    int32 LowerIndex = INDEX_NONE;
    int32 UpperIndex = INDEX_NONE;
    double Alpha = 0.0;
    if (!FindInterpolationRows(
            CurrentTime,
            LowerIndex,
            UpperIndex,
            Alpha))
    {
        return false;
    }

    FVector CurrentSpacecraftPosition;
    if (!EvaluateHermitePositionAtEphemerisTime(
            SpacecraftPosition,
            SpacecraftVelocity,
            CurrentTime,
            CurrentSpacecraftPosition))
    {
        return false;
    }

    const FCelestialTrack* ClosestBody = FindClosestPhysicalBodyTrack(
        CurrentSpacecraftPosition,
        LowerIndex,
        UpperIndex,
        Alpha);
    if (ClosestBody == nullptr)
    {
        return false;
    }

    FVector CurrentBodyPosition;
    if (!EvaluateHermitePositionAtEphemerisTime(
            ClosestBody->Position,
            ClosestBody->Velocity,
            CurrentTime,
            CurrentBodyPosition))
    {
        return false;
    }

    const double StartTime = GetStartEphemerisTimeTdbSeconds();
    const double EndTime = GetEndEphemerisTimeTdbSeconds();
    const double SelectedStartTime = FMath::Lerp(
        StartTime,
        EndTime,
        FMath::Clamp(LocalTrajectoryDisplayStartNormalized, 0.0, 1.0));
    const double SelectedEndTime = FMath::Lerp(
        StartTime,
        EndTime,
        FMath::Clamp(LocalTrajectoryDisplayEndNormalized, 0.0, 1.0));

    OutData.BodyKey = ClosestBody->Key;
    OutData.BodyDisplayName = ResolveCelestialDisplayName(ClosestBody->Key);
    OutData.BodyColor = ResolveBodyColor(ClosestBody->Key);
    OutData.BodyReferenceRadiusMeters = InterpolateColumn(
        ClosestBody->ReferenceRadius,
        LowerIndex,
        UpperIndex,
        Alpha,
        0.0);
    OutData.CurrentSpacecraftPositionRelativeMeters =
        CurrentSpacecraftPosition - CurrentBodyPosition;

    // A compact graph cannot resolve thousands of distinct screen samples.
    // Keep the same Hermite curve and selected time interval as the main path,
    // while capping only the HUD sampling density.
    constexpr int32 MaximumHudTrajectorySamples = 1024;
    if (SelectedEndTime > SelectedStartTime)
    {
        BuildSpacecraftTrajectorySamples(
            ClosestBody,
            SelectedStartTime,
            SelectedEndTime,
            MaximumHudTrajectorySamples,
            true,
            OutData.SpacecraftPathRelativeMeters);
    }

    // A collapsed display range hides only the trajectory. The body and the
    // current spacecraft marker remain a valid body-centered snapshot.
    return OutData.BodyReferenceRadiusMeters > 0.0;
}

void ATGSimulationPlaybackActor::ToggleVisualizationMenu()
{
    if (VisualizationHudWidget != nullptr)
    {
        VisualizationHudWidget->TogglePauseMenu();
    }
}

bool ATGSimulationPlaybackActor::IsVisualizationMenuOpen() const
{
    return VisualizationHudWidget != nullptr &&
        VisualizationHudWidget->IsPauseMenuOpen();
}

void ATGSimulationPlaybackActor::SetVisualizationUiCapturesCameraInput(
    const bool bCapturesInput)
{
    bVisualizationUiCapturesCameraInput = bCapturesInput;
}

bool ATGSimulationPlaybackActor::IsVisualizationUiCapturingCameraInput() const
{
    return bVisualizationUiCapturesCameraInput ||
        (VisualizationHudWidget != nullptr &&
         VisualizationHudWidget->IsPointerOverControls());
}

void ATGSimulationPlaybackActor::BuildOverviewPaths()
{
    using namespace TGSimulationPlaybackPresentationPrivate;
    SolarSystemOverviewPaths.Reset();
    if (NumericRows.IsEmpty() || TimeColumn == INDEX_NONE)
    {
        return;
    }

    constexpr int32 MaximumOverviewSamples = 512;
    const int32 Stride = FMath::Max(
        1,
        FMath::DivideAndRoundUp(
            NumericRows.Num(),
            MaximumOverviewSamples));

    auto AppendCsvPath = [this, Stride](
        FTGSolarSystemOverviewPath& Path,
        const FColumnVector& PositionColumns)
    {
        for (int32 Row = 0; Row < NumericRows.Num(); Row += Stride)
        {
            TGSimulationPlaybackPresentationPrivate::AddPathSample(
                Path,
                NumericRows[Row][TimeColumn],
                FVector(
                    NumericRows[Row][PositionColumns.X],
                    NumericRows[Row][PositionColumns.Y],
                    NumericRows[Row][PositionColumns.Z]));
        }
        const int32 LastRow = NumericRows.Num() - 1;
        if (
            Path.EphemerisTimes.IsEmpty() ||
            Path.EphemerisTimes.Last() != NumericRows[LastRow][TimeColumn])
        {
            TGSimulationPlaybackPresentationPrivate::AddPathSample(
                Path,
                NumericRows[LastRow][TimeColumn],
                FVector(
                    NumericRows[LastRow][PositionColumns.X],
                    NumericRows[LastRow][PositionColumns.Y],
                    NumericRows[LastRow][PositionColumns.Z]));
        }
    };

    FTGSolarSystemOverviewPath SpacecraftPath;
    SpacecraftPath.Key = TEXT("spacecraft");
    SpacecraftPath.DisplayName = TEXT("Spacecraft");
    SpacecraftPath.Color = FLinearColor(0.15f, 0.95f, 0.82f);
    SpacecraftPath.bSpacecraft = true;
    AppendCsvPath(SpacecraftPath, SpacecraftPosition);
    SolarSystemOverviewPaths.Add(MoveTemp(SpacecraftPath));

    struct FBodyOrbitSpec
    {
        const TCHAR* Name;
        double SiderealPeriodDays;
    };
    const FBodyOrbitSpec MajorBodies[] = {
        {TEXT("Mercury"), 87.9691},
        {TEXT("Venus"), 224.701},
        {TEXT("Earth"), 365.256},
        {TEXT("Moon"), 27.321661},
        {TEXT("Mars"), 686.980},
        {TEXT("Jupiter"), 4332.589},
        {TEXT("Saturn"), 10759.22},
        {TEXT("Uranus"), 30688.5},
        {TEXT("Neptune"), 60182.0},
        {TEXT("Pluto"), 90560.0}};

    TSet<FString> IncludedBodies;
    FString SpiceMessage;
    const bool bSpiceAvailable = FSpiceBridge::LoadKernels(SpiceMessage);
    if (!bSpiceAvailable)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Solar-system overview could not load SPICE: %s"),
            *SpiceMessage);
    }

    if (bSpiceAvailable)
    {
        // The Sun is a labeled current-position reference. Planet and Moon
        // histories below are sampled over one complete sidereal period so
        // even a short mission still presents recognizable orbital paths.
        FTGSolarSystemOverviewPath SunPath;
        SunPath.Key = TEXT("sun");
        SunPath.DisplayName = TEXT("Sun");
        SunPath.SpiceTargetName = TEXT("Sun");
        SunPath.Color = ResolveBodyColor(SunPath.Key);
        double IgnoredGm = 0.0;
        FSpiceBridge::GetBodyGravityMetadataSI(
            TEXT("Sun"),
            IgnoredGm,
            SunPath.ReferenceRadiusMeters,
            SpiceMessage);
        FVector SunPosition;
        FVector IgnoredVelocity;
        if (FSpiceBridge::GetBodyICRFStateSI(
                TEXT("Sun"),
                NumericRows[0][TimeColumn],
                SunPosition,
                IgnoredVelocity,
                SpiceMessage))
        {
            AddPathSample(
                SunPath,
                NumericRows[0][TimeColumn],
                SunPosition);
            SolarSystemOverviewPaths.Add(MoveTemp(SunPath));
            IncludedBodies.Add(TEXT("sun"));
        }

        constexpr int32 SamplesPerOrbit = 1024;
        constexpr double SecondsPerDay = 86400.0;
        const double StartEt = NumericRows[0][TimeColumn];
        for (const FBodyOrbitSpec& Body : MajorBodies)
        {
            const FString BodyName(Body.Name);
            FTGSolarSystemOverviewPath Path;
            Path.Key = NormalizeColumnKey(BodyName);
            Path.DisplayName = BodyName;
            Path.SpiceTargetName = BodyName;
            Path.Color = ResolveBodyColor(Path.Key);
            FSpiceBridge::GetBodyGravityMetadataSI(
                BodyName,
                IgnoredGm,
                Path.ReferenceRadiusMeters,
                SpiceMessage);

            const double PeriodSeconds =
                Body.SiderealPeriodDays * SecondsPerDay;
            for (int32 Sample = 0; Sample <= SamplesPerOrbit; ++Sample)
            {
                const double Et = StartEt +
                    PeriodSeconds * static_cast<double>(Sample) /
                    static_cast<double>(SamplesPerOrbit);
                FVector Position;
                FVector Velocity;
                if (FSpiceBridge::GetBodyICRFStateSI(
                        BodyName,
                        Et,
                        Position,
                        Velocity,
                        SpiceMessage))
                {
                    AddPathSample(Path, Et, Position);
                }
            }
            if (!Path.PositionsIcrfMeters.IsEmpty())
            {
                IncludedBodies.Add(Path.Key);
                SolarSystemOverviewPaths.Add(MoveTemp(Path));
            }
        }
    }

    // Preserve any additional physical body supplied by the backend CSV.
    // Known major bodies above use their full SPICE orbit instead.
    for (const FCelestialTrack& Track : CelestialTracks)
    {
        if (IncludedBodies.Contains(Track.Key))
        {
            continue;
        }
        const double Radius = InterpolateColumn(
            Track.ReferenceRadius,
            0,
            0,
            0.0,
            0.0);
        if (Radius <= 0.0)
        {
            continue;
        }

        FTGSolarSystemOverviewPath Path;
        Path.Key = Track.Key;
        Path.DisplayName = ResolveCelestialDisplayName(Track.Key);
        Path.Color = ResolveBodyColor(Track.Key);
        Path.ReferenceRadiusMeters = Radius;
        AppendCsvPath(Path, Track.Position);
        SolarSystemOverviewPaths.Add(MoveTemp(Path));
    }
}

void ATGSimulationPlaybackActor::BuildVisualizationArrows()
{
    using namespace TGSimulationPlaybackPresentationPrivate;
    ClearVisualizationArrows();

    auto AddArrow = [this](
        const FName ArrowId,
        const FString& DisplayName,
        const FLinearColor& Color,
        const ETGVisualizationArrowQuantity Quantity,
        const FString& MagnitudeUnit)
    {
        FTGVisualizationArrowInfo Info;
        Info.ArrowId = ArrowId;
        Info.DisplayName = DisplayName;
        Info.Color = Color;
        Info.DisplayColor = ResolveArrowDisplayColor(Color, true);
        Info.bVisible = false;
        Info.Quantity = Quantity;
        Info.MagnitudeUnit = MagnitudeUnit;
        VisualizationArrowInfos.Add(MoveTemp(Info));
    };

    TSet<FString> AddedBodyKeys;
    for (const FCelestialTrack& Track : CelestialTracks)
    {
        AddArrow(
            FName(*(TEXT("body_") + Track.Key)),
            ResolveCelestialDisplayName(Track.Key),
            ResolveBodyColor(Track.Key),
            ETGVisualizationArrowQuantity::BodyDirection,
            FString{});
        AddedBodyKeys.Add(Track.Key);
    }

    // A short backend CSV usually contains only nearby active gravity
    // contributors. The overview already has SPICE paths for the major Solar
    // System bodies, so expose those directions in the same vector picker too.
    for (const FTGSolarSystemOverviewPath& Path : SolarSystemOverviewPaths)
    {
        if (Path.bSpacecraft || AddedBodyKeys.Contains(Path.Key))
        {
            continue;
        }
        AddArrow(
            FName(*(TEXT("body_") + Path.Key)),
            Path.DisplayName,
            Path.Color,
            ETGVisualizationArrowQuantity::BodyDirection,
            FString{});
        AddedBodyKeys.Add(Path.Key);
    }

    // Direction vectors are a catalog feature, not a gravity or rendering
    // feature. List every physical SPICE body even when it is too distant to
    // receive a spawned visual actor or was omitted from force evaluation.
    FString SpiceMessage;
    for (const FTGCelestialCatalogEntry& Entry :
         UTGCelestialCatalogLibrary::GetCelestialCatalog())
    {
        if (Entry.SourceRole == ETGCelestialSourceRole::SystemBarycenter)
        {
            continue;
        }

        const FString BodyKey = NormalizeColumnKey(
            Entry.CatalogKey.ToString());
        const FString SpiceKey = NormalizeColumnKey(Entry.SpiceTarget);
        VectorBodySpiceTargets.Add(BodyKey, Entry.SpiceTarget);

        double IgnoredGravitationalParameter = 0.0;
        double ReferenceRadiusMeters = 0.0;
        FSpiceBridge::GetBodyGravityMetadataSI(
            Entry.SpiceTarget,
            IgnoredGravitationalParameter,
            ReferenceRadiusMeters,
            SpiceMessage);
        VectorBodyReferenceRadiiMeters.Add(
            BodyKey,
            FMath::Max(0.0, ReferenceRadiusMeters));

        if (AddedBodyKeys.Contains(BodyKey) ||
            AddedBodyKeys.Contains(SpiceKey))
        {
            continue;
        }

        AddArrow(
            FName(*(TEXT("body_") + BodyKey)),
            Entry.DisplayName.ToString(),
            ResolveBodyColor(BodyKey),
            ETGVisualizationArrowQuantity::BodyDirection,
            FString{});
        AddedBodyKeys.Add(BodyKey);
    }
    for (const FForceTrack& Track : ForceTracks)
    {
        const FString DisplayName = Track.Key == TEXT("srp")
            ? TEXT("Solar Radiation Pressure Force (SRP)")
            : PrettifyKey(Track.Key) + TEXT(" Force");
        AddArrow(
            FName(*(TEXT("force_") + Track.Key)),
            DisplayName,
            ResolveForceColor(Track.Key),
            ETGVisualizationArrowQuantity::Force,
            TEXT("N"));
    }
    for (const FPhysicalVectorTrack& Track : PhysicalVectorTracks)
    {
        AddArrow(
            Track.ArrowId,
            Track.DisplayName,
            ResolveVectorColor(Track.Quantity, Track.Key),
            Track.Quantity,
            Track.Unit);
    }
}

ATGSimulationPlaybackActor::FVisualizationArrowMesh*
ATGSimulationPlaybackActor::FindOrCreateVisualizationArrowMesh(
    const FName ArrowId,
    const FLinearColor& Color)
{
    if (PlaybackRoot == nullptr ||
        VectorArrowCylinderMesh == nullptr ||
        VectorArrowConeMesh == nullptr)
    {
        return nullptr;
    }

    if (FVisualizationArrowMesh* Existing =
            VisualizationArrowMeshes.Find(ArrowId))
    {
        if (Existing->Shaft.IsValid() && Existing->Head.IsValid())
        {
            return Existing;
        }
        if (Existing->Shaft.IsValid())
        {
            Existing->Shaft->DestroyComponent();
        }
        if (Existing->Head.IsValid())
        {
            Existing->Head->DestroyComponent();
        }
        VisualizationArrowMeshes.Remove(ArrowId);
    }

    const auto CreateMeshPart = [this, ArrowId](
        UStaticMesh* Mesh,
        const TCHAR* PartName) -> UStaticMeshComponent*
    {
        const FName ComponentName = MakeUniqueObjectName(
            this,
            UStaticMeshComponent::StaticClass(),
            FName(*FString::Printf(
                TEXT("VisualizationArrow_%s_%s"),
                *ArrowId.ToString(),
                PartName)));
        UStaticMeshComponent* Component =
            NewObject<UStaticMeshComponent>(this, ComponentName);
        if (Component == nullptr)
        {
            return nullptr;
        }

        AddInstanceComponent(Component);
        Component->SetupAttachment(PlaybackRoot);
        Component->SetStaticMesh(Mesh);
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
        Component->SetCastShadow(false);
        Component->SetReceivesDecals(false);
        Component->SetAffectDynamicIndirectLighting(false);
        Component->SetAffectDistanceFieldLighting(false);
        Component->SetEmissiveLightSource(false);
        Component->SetLightingChannels(false, false, false);
        Component->SetRenderCustomDepth(false);
        Component->SetDepthPriorityGroup(SDPG_Foreground);
        Component->SetHiddenInGame(true);
        Component->RegisterComponent();
        return Component;
    };

    FVisualizationArrowMesh NewArrow;
    NewArrow.Shaft = CreateMeshPart(
        VectorArrowCylinderMesh,
        TEXT("Shaft"));
    NewArrow.Head = CreateMeshPart(
        VectorArrowConeMesh,
        TEXT("Head"));
    if (!NewArrow.Shaft.IsValid() || !NewArrow.Head.IsValid())
    {
        if (NewArrow.Shaft.IsValid())
        {
            NewArrow.Shaft->DestroyComponent();
        }
        if (NewArrow.Head.IsValid())
        {
            NewArrow.Head->DestroyComponent();
        }
        return nullptr;
    }

    if (VectorArrowMaterial != nullptr)
    {
        UMaterialInstanceDynamic* ShaftMaterial =
            UMaterialInstanceDynamic::Create(
                VectorArrowMaterial,
                this);
        UMaterialInstanceDynamic* HeadMaterial =
            UMaterialInstanceDynamic::Create(
                VectorArrowMaterial,
                this);
        if (ShaftMaterial != nullptr && HeadMaterial != nullptr)
        {
            const FLinearColor ShaftColor =
                TGSimulationPlaybackPresentationPrivate::
                    ResolveArrowDisplayColor(Color, false);
            const FLinearColor HeadColor =
                TGSimulationPlaybackPresentationPrivate::
                    ResolveArrowDisplayColor(Color, true);
            ShaftMaterial->SetVectorParameterValue(TEXT("Color"), ShaftColor);
            ShaftMaterial->SetVectorParameterValue(
                TEXT("BaseColor"),
                ShaftColor);
            HeadMaterial->SetVectorParameterValue(TEXT("Color"), HeadColor);
            HeadMaterial->SetVectorParameterValue(
                TEXT("BaseColor"),
                HeadColor);
            NewArrow.Shaft->SetMaterial(0, ShaftMaterial);
            NewArrow.Head->SetMaterial(0, HeadMaterial);
        }
    }

    return &VisualizationArrowMeshes.Add(ArrowId, MoveTemp(NewArrow));
}

void ATGSimulationPlaybackActor::ClearVisualizationArrows()
{
    for (TPair<FName, FVisualizationArrowMesh>& Pair :
         VisualizationArrowMeshes)
    {
        if (Pair.Value.Shaft.IsValid())
        {
            Pair.Value.Shaft->DestroyComponent();
        }
        if (Pair.Value.Head.IsValid())
        {
            Pair.Value.Head->DestroyComponent();
        }
    }
    if (BodyReticleLineBatch != nullptr)
    {
        BodyReticleLineBatch->Flush();
    }
    VisualizationArrowMeshes.Reset();
    VisualizationArrowInfos.Reset();
    VectorBodySpiceTargets.Reset();
    VectorBodyReferenceRadiiMeters.Reset();
}

void ATGSimulationPlaybackActor::BuildPresentation()
{
    UWorld* World = GetWorld();
    APlayerController* PlayerController =
        World != nullptr ? World->GetFirstPlayerController() : nullptr;
    if (World == nullptr || PlayerController == nullptr)
    {
        return;
    }

    // The visualization camera does not use Unreal's automatically spawned
    // DefaultPawn. Its stock mesh is a visible 0.7 m sphere, which otherwise
    // appears to float beside the spacecraft while the controller's view is
    // redirected to OrbitCameraActor.
    APawn* PlayerPawn = PlayerController->GetPawn();
    if (PlayerPawn != nullptr && PlayerPawn->IsA<ADefaultPawn>())
    {
        SuppressedDefaultPawn = PlayerPawn;
        bSuppressedDefaultPawnWasHidden = PlayerPawn->IsHidden();
        bSuppressedDefaultPawnHadCollision =
            PlayerPawn->GetActorEnableCollision();
        PlayerPawn->SetActorHiddenInGame(true);
        PlayerPawn->SetActorEnableCollision(false);
    }

    if (bCreateSpacecraftOrbitCamera)
    {
        FActorSpawnParameters Parameters;
        Parameters.Owner = this;
        Parameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        OrbitCameraActor = World->SpawnActor<ATGSpacecraftOrbitCameraActor>(
            ATGSpacecraftOrbitCameraActor::StaticClass(),
            GetActorTransform(),
            Parameters);
        if (OrbitCameraActor != nullptr)
        {
            OrbitCameraActor->InitializeForPlayback(
                this,
                this,
                InitialCameraDistanceCentimeters);
            OrbitCameraActor->SetFieldOfViewDegrees(
                VisualizationCameraFovDegrees);
            OrbitCameraActor->SetOrbitSensitivityDegreesPerPixel(
                VisualizationOrbitSensitivityDegreesPerPixel);
        }
    }

    PlayerController->bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetLockMouseToViewportBehavior(
        EMouseLockMode::DoNotLock);
    PlayerController->SetInputMode(InputMode);

    if (UGameViewportClient* Viewport = World->GetGameViewport())
    {
        BodyReticleLabelOverlay =
            SNew(TGSimulationPlaybackPresentationPrivate::
                STGBodyReticleLabelOverlay)
            .PlaybackActor(TWeakObjectPtr<ATGSimulationPlaybackActor>(this));
        Viewport->AddViewportWidgetContent(
            BodyReticleLabelOverlay.ToSharedRef(),
            9);
    }

    if (bCreateNativeVisualizationHud)
    {
        UClass* HudClass = VisualizationHudClass.LoadSynchronous();
        if (HudClass == nullptr)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Visualization HUD not created: make "
                     "/Game/UI/Visualization/WBP_VisualizationHUD with "
                     "TGVisualizationHudWidget as its parent."));
        }
        else
        {
            VisualizationHudWidget = CreateWidget<UTGVisualizationHudWidget>(
                PlayerController,
                HudClass);
        }
        if (VisualizationHudWidget != nullptr)
        {
            VisualizationHudWidget->InitializeForPlayback(this);
            VisualizationHudWidget->AddToViewport(10);
        }
    }
}

void ATGSimulationPlaybackActor::DestroyPresentation()
{
    bVisualizationUiCapturesCameraInput = false;

    if (BodyReticleLabelOverlay.IsValid())
    {
        UWorld* World = GetWorld();
        UGameViewportClient* Viewport = World != nullptr
            ? World->GetGameViewport()
            : nullptr;
        if (Viewport != nullptr)
        {
            Viewport->RemoveViewportWidgetContent(
                BodyReticleLabelOverlay.ToSharedRef());
        }
        BodyReticleLabelOverlay.Reset();
    }

    if (IsValid(SuppressedDefaultPawn))
    {
        SuppressedDefaultPawn->SetActorHiddenInGame(
            bSuppressedDefaultPawnWasHidden);
        SuppressedDefaultPawn->SetActorEnableCollision(
            bSuppressedDefaultPawnHadCollision);
    }
    SuppressedDefaultPawn = nullptr;

    if (VisualizationHudWidget != nullptr)
    {
        VisualizationHudWidget->RemoveFromParent();
        VisualizationHudWidget = nullptr;
    }
    if (OrbitCameraActor != nullptr)
    {
        OrbitCameraActor->Destroy();
        OrbitCameraActor = nullptr;
    }
}

bool ATGSimulationPlaybackActor::ShouldEverSpawnCelestialTrack(
    const FCelestialTrack& Track,
    const FTGCelestialPlaybackBinding* Binding) const
{
    if (
        !Track.Position.IsComplete() ||
        Track.ReferenceRadius == INDEX_NONE ||
        NumericRows.IsEmpty())
    {
        return false;
    }

    const double ViewHeight = FMath::Max(
        1.0,
        CelestialVisibilityReferenceViewportHeightPixels);
    const double VerticalFov = FMath::DegreesToRadians(FMath::Clamp(
        CelestialVisibilityReferenceVerticalFovDegrees,
        1.0,
        170.0));
    for (const TArray<double>& Row : NumericRows)
    {
        const double Radius = Row[Track.ReferenceRadius];
        // Barycenters are force/ephemeris references rather than visible
        // bodies. Their zero radius must not pass a zero-pixel threshold and
        // reach the engine-sphere fallback.
        if (Radius <= 0.0)
        {
            continue;
        }
        const FVector SampleSpacecraftPosition(
            Row[SpacecraftPosition.X],
            Row[SpacecraftPosition.Y],
            Row[SpacecraftPosition.Z]);
        const FVector BodyPosition(
            Row[Track.Position.X],
            Row[Track.Position.Y],
            Row[Track.Position.Z]);
        const double Distance =
            (BodyPosition - SampleSpacecraftPosition).Length();
        if (
            Binding != nullptr &&
            Binding->MaximumVisibleDistanceMeters > 0.0 &&
            Distance > Binding->MaximumVisibleDistanceMeters)
        {
            continue;
        }
        if (TGSimulationPlaybackPresentationPrivate::ApparentDiameterPixels(
                Radius,
                Distance,
                ViewHeight,
                VerticalFov,
                CelestialBodyVisualMagnification) >=
            MinimumCelestialApparentDiameterPixels)
        {
            return true;
        }
    }
    return false;
}

bool ATGSimulationPlaybackActor::IsCelestialTrackVisibleAtFrame(
    const FCelestialTrack& Track,
    const FTGCelestialPlaybackBinding* Binding,
    const FVector& SpacecraftPositionIcrf,
    const int32 LowerIndex,
    const int32 UpperIndex,
    const double Alpha) const
{
    if (Track.ReferenceRadius == INDEX_NONE)
    {
        return false;
    }
    const FVector BodyPosition = InterpolateVector(
        Track.Position,
        LowerIndex,
        UpperIndex,
        Alpha);
    const double Distance = (BodyPosition - SpacecraftPositionIcrf).Length();
    if (
        Binding != nullptr &&
        Binding->MaximumVisibleDistanceMeters > 0.0 &&
        Distance > Binding->MaximumVisibleDistanceMeters)
    {
        return false;
    }

    double ViewHeight = FMath::Max(
        1.0,
        CelestialVisibilityReferenceViewportHeightPixels);
    double VerticalFov = FMath::DegreesToRadians(FMath::Clamp(
        CelestialVisibilityReferenceVerticalFovDegrees,
        1.0,
        170.0));
    if (APlayerController* PlayerController =
            GetWorld() != nullptr
                ? GetWorld()->GetFirstPlayerController()
                : nullptr)
    {
        int32 Width = 0;
        int32 Height = 0;
        PlayerController->GetViewportSize(Width, Height);
        if (Width > 0 && Height > 0)
        {
            ViewHeight = Height;
            if (PlayerController->PlayerCameraManager != nullptr)
            {
                const double HorizontalFov = FMath::DegreesToRadians(
                    PlayerController->PlayerCameraManager->GetFOVAngle());
                const double Aspect =
                    static_cast<double>(Width) / static_cast<double>(Height);
                VerticalFov = 2.0 * FMath::Atan(
                    FMath::Tan(HorizontalFov * 0.5) /
                    FMath::Max(Aspect, UE_DOUBLE_SMALL_NUMBER));
            }
        }
    }

    const double Radius = InterpolateColumn(
        Track.ReferenceRadius,
        LowerIndex,
        UpperIndex,
        Alpha,
        0.0);
    if (Radius <= 0.0)
    {
        return false;
    }
    return TGSimulationPlaybackPresentationPrivate::ApparentDiameterPixels(
        Radius,
        Distance,
        ViewHeight,
        VerticalFov,
        CelestialBodyVisualMagnification) >=
        MinimumCelestialApparentDiameterPixels;
}

AActor* ATGSimulationPlaybackActor::SpawnCelestialActor(
    const FCelestialTrack& Track,
    const FTGCelestialPlaybackBinding* Binding)
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return nullptr;
    }

    AActor* BodyActor = nullptr;
    if (Binding != nullptr && !Binding->ActorClass.IsNull())
    {
        if (UClass* ActorClass = Binding->ActorClass.LoadSynchronous())
        {
            BodyActor = World->SpawnActor<AActor>(
                ActorClass,
                GetActorTransform());
        }
    }

    if (
        BodyActor == nullptr &&
        bUseGenericCelestialFallback &&
        GenericCelestialSphereMesh != nullptr)
    {
        AStaticMeshActor* SphereActor = World->SpawnActor<AStaticMeshActor>(
            AStaticMeshActor::StaticClass(),
            GetActorTransform());
        if (SphereActor != nullptr)
        {
            UStaticMeshComponent* Mesh = SphereActor->GetStaticMeshComponent();
            Mesh->SetMobility(EComponentMobility::Movable);
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Mesh->SetStaticMesh(GenericCelestialSphereMesh);
            if (GenericCelestialMaterial != nullptr)
            {
                UMaterialInstanceDynamic* Material =
                    UMaterialInstanceDynamic::Create(
                        GenericCelestialMaterial,
                        SphereActor);
                if (Material != nullptr)
                {
                    const FLinearColor Color = ResolveBodyColor(Track.Key);
                    Material->SetVectorParameterValue(TEXT("Color"), Color);
                    Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
                    Mesh->SetMaterial(0, Material);
                }
            }
            BodyActor = SphereActor;
        }
    }

    if (BodyActor != nullptr)
    {
        TArray<UPrimitiveComponent*> Primitives;
        BodyActor->GetComponents<UPrimitiveComponent>(Primitives);
        for (UPrimitiveComponent* Primitive : Primitives)
        {
            if (IsValid(Primitive))
            {
                Primitive->SetCastShadow(false);
                // Channel 0 receives the physical Sun. Channel 2 is reserved
                // for the optional camera-facing body visibility fill.
                Primitive->SetLightingChannels(true, false, true);
            }
        }

    }
    return BodyActor;
}

const ATGSimulationPlaybackActor::FCelestialTrack*
ATGSimulationPlaybackActor::FindClosestPhysicalBodyTrack(
    const FVector& SpacecraftPositionIcrf,
    const int32 LowerIndex,
    const int32 UpperIndex,
    const double Alpha) const
{
    const FCelestialTrack* Closest = nullptr;
    double ClosestSurfaceDistance = TNumericLimits<double>::Max();
    for (const FCelestialTrack& Track : CelestialTracks)
    {
        const double Radius = InterpolateColumn(
            Track.ReferenceRadius,
            LowerIndex,
            UpperIndex,
            Alpha,
            0.0);
        if (Radius <= 0.0)
        {
            continue;
        }
        const double CenterDistance = (
            InterpolateVector(
                Track.Position,
                LowerIndex,
                UpperIndex,
                Alpha) - SpacecraftPositionIcrf).Length();
        const double SurfaceDistance = FMath::Max(
            0.0,
            CenterDistance - Radius);
        if (SurfaceDistance < ClosestSurfaceDistance)
        {
            ClosestSurfaceDistance = SurfaceDistance;
            Closest = &Track;
        }
    }
    return Closest;
}

void ATGSimulationPlaybackActor::UpdateVisualizationOverlays(
    const int32 LowerIndex,
    const int32 UpperIndex,
    const double Alpha,
    const FVector& SpacecraftPositionIcrf)
{
    // Preserve the initial 600 cm camera : 350 cm arrow relationship at every
    // zoom level. This keeps the vectors' apparent size stable in the view.
    constexpr double VectorLengthToCameraDistanceRatio = 7.0 / 12.0;
    const double StandardVectorLengthCentimeters =
        IsValid(OrbitCameraActor)
            ? FMath::Max(
                1.0,
                OrbitCameraActor->GetOrbitDistanceCentimeters() *
                    VectorLengthToCameraDistanceRatio)
            : FMath::Max(VectorArrowLengthCentimeters, 1.0);

    // The trajectory belongs to the world depth layer. Draw it first so a
    // tangential force such as aerodynamic drag cannot disappear beneath it.
    UpdateLocalTrajectory();

    // Hide the pooled arrow meshes first. Selected vectors are positioned and
    // revealed below; deselected or zero-length vectors remain hidden.
    for (TPair<FName, FVisualizationArrowMesh>& Pair :
         VisualizationArrowMeshes)
    {
        if (Pair.Value.Shaft.IsValid())
        {
            Pair.Value.Shaft->SetHiddenInGame(true);
        }
        if (Pair.Value.Head.IsValid())
        {
            Pair.Value.Head->SetHiddenInGame(true);
        }
    }
    if (BodyReticleLineBatch != nullptr)
    {
        BodyReticleLineBatch->Flush();
    }
    for (FTGVisualizationArrowInfo& Info : VisualizationArrowInfos)
    {
        if (Info.Quantity ==
            ETGVisualizationArrowQuantity::BodyDirection)
        {
            Info.bBodyTargetAvailable = false;
            Info.CurrentBodyDirectionUnreal = FVector::ZeroVector;
            Info.CurrentBodyWorldLocation = FVector::ZeroVector;
            Info.CurrentBodyAngularRadiusRadians = 0.0;
        }
    }

    auto UpdateArrow = [this](
        const FName ArrowId,
        const FVector& DirectionIcrf,
        const double DisplayLengthCentimeters,
        const double BodyRadiusMeters = 0.0,
        const double PhysicalMagnitude = 0.0,
        const double RelativeVisualAmplification = 1.0)
    {
        FTGVisualizationArrowInfo* Info =
            VisualizationArrowInfos.FindByPredicate(
                [ArrowId](const FTGVisualizationArrowInfo& Candidate)
                {
                    return Candidate.ArrowId == ArrowId;
                });
        if (Info != nullptr &&
            Info->Quantity !=
                ETGVisualizationArrowQuantity::BodyDirection)
        {
            Info->CurrentMagnitude = PhysicalMagnitude;
            Info->RelativeVisualAmplification = FMath::Max(
                1.0,
                RelativeVisualAmplification);
            Info->bVisuallyAmplified =
                Info->RelativeVisualAmplification > 1.01;
        }

        if (
            Info == nullptr ||
            !IsVisualizationArrowVisible(ArrowId) ||
            DisplayLengthCentimeters <= 1.0)
        {
            return;
        }

        // DirectionIcrf may be a physical force whose magnitude is far below
        // one SI unit. GetSafeNormal's tolerance applies to squared length,
        // so using a generic engine epsilon would wrongly erase valid uN/nN
        // vectors. Normalize explicitly after checking for an exact finite
        // zero; display length is handled independently below.
        const double DirectionMagnitude = DirectionIcrf.Length();
        if (
            !FMath::IsFinite(DirectionMagnitude) ||
            DirectionMagnitude <= 0.0)
        {
            return;
        }
        const FVector DisplayDirection =
            ConvertIcrfVectorToUnreal(DirectionIcrf) / DirectionMagnitude;
        const FVector Origin = PlaybackRoot != nullptr
            ? PlaybackRoot->GetComponentLocation()
            : GetActorLocation();
        if (Info->Quantity ==
            ETGVisualizationArrowQuantity::BodyDirection)
        {
            Info->CurrentBodyDirectionUnreal = DisplayDirection;
            Info->CurrentBodyWorldLocation = Origin + DisplayDirection *
                (DirectionMagnitude * WorldUnitsPerMeter);
            Info->CurrentBodyAngularRadiusRadians = BodyRadiusMeters > 0.0
                ? FMath::Asin(FMath::Clamp(
                    BodyRadiusMeters / DirectionMagnitude,
                    0.0,
                    1.0))
                : 0.0;
            Info->bBodyTargetAvailable = true;
        }

        const double Length = FMath::Max(DisplayLengthCentimeters, 1.0);
        // Every dimension uses the same length-dependent proportions so the
        // complete cylinder-and-cone mesh scales uniformly with camera zoom.
        const double HeadLength = Length * 0.16;
        const double ShaftLength = FMath::Max(1.0, Length - HeadLength);
        const double ShaftRadius = Length * 0.0075;
        const double HeadRadius = Length * 0.025;
        FVisualizationArrowMesh* ArrowMesh =
            FindOrCreateVisualizationArrowMesh(ArrowId, Info->Color);
        if (ArrowMesh == nullptr ||
            !ArrowMesh->Shaft.IsValid() ||
            !ArrowMesh->Head.IsValid())
        {
            return;
        }

        const FQuat ArrowOrientation = FQuat::FindBetweenNormals(
            FVector::UpVector,
            DisplayDirection);

        const FBoxSphereBounds CylinderBounds =
            VectorArrowCylinderMesh->GetBounds();
        const FVector CylinderSize = CylinderBounds.BoxExtent * 2.0;
        const FVector ShaftScale(
            (2.0 * ShaftRadius) /
                FMath::Max(CylinderSize.X, UE_DOUBLE_SMALL_NUMBER),
            (2.0 * ShaftRadius) /
                FMath::Max(CylinderSize.Y, UE_DOUBLE_SMALL_NUMBER),
            ShaftLength /
                FMath::Max(CylinderSize.Z, UE_DOUBLE_SMALL_NUMBER));
        const FVector ShaftCenter =
            Origin + DisplayDirection * (ShaftLength * 0.5);
        const FVector ShaftLocation = ShaftCenter -
            ArrowOrientation.RotateVector(
                CylinderBounds.Origin * ShaftScale);
        ArrowMesh->Shaft->SetWorldTransform(FTransform(
            ArrowOrientation,
            ShaftLocation,
            ShaftScale));

        const FBoxSphereBounds ConeBounds = VectorArrowConeMesh->GetBounds();
        const FVector ConeSize = ConeBounds.BoxExtent * 2.0;
        const FVector HeadScale(
            (2.0 * HeadRadius) /
                FMath::Max(ConeSize.X, UE_DOUBLE_SMALL_NUMBER),
            (2.0 * HeadRadius) /
                FMath::Max(ConeSize.Y, UE_DOUBLE_SMALL_NUMBER),
            HeadLength /
                FMath::Max(ConeSize.Z, UE_DOUBLE_SMALL_NUMBER));
        const FVector HeadCenter = Origin + DisplayDirection *
            (ShaftLength + HeadLength * 0.5);
        const FVector HeadLocation = HeadCenter -
            ArrowOrientation.RotateVector(ConeBounds.Origin * HeadScale);
        ArrowMesh->Head->SetWorldTransform(FTransform(
            ArrowOrientation,
            HeadLocation,
            HeadScale));

        ArrowMesh->Shaft->SetHiddenInGame(false);
        ArrowMesh->Head->SetHiddenInGame(false);
    };

    for (const FCelestialTrack& Track : CelestialTracks)
    {
        const FVector BodyPositionIcrf = InterpolateVector(
            Track.Position,
            LowerIndex,
            UpperIndex,
            Alpha);
        UpdateArrow(
            FName(*(TEXT("body_") + Track.Key)),
            BodyPositionIcrf - SpacecraftPositionIcrf,
            StandardVectorLengthCentimeters,
            InterpolateColumn(
                Track.ReferenceRadius,
                LowerIndex,
                UpperIndex,
                Alpha,
                0.0));
    }

    // The remaining body arrows come from the cached SPICE overview paths,
    // not from CSV columns. Evaluate only visible arrows to keep the normal
    // playback path free of unnecessary SPICE calls.
    TSet<FString> CsvBodyKeys;
    for (const FCelestialTrack& Track : CelestialTracks)
    {
        CsvBodyKeys.Add(Track.Key);
    }
    for (const FTGSolarSystemOverviewPath& Path : SolarSystemOverviewPaths)
    {
        if (Path.bSpacecraft || CsvBodyKeys.Contains(Path.Key))
        {
            continue;
        }
        const FName ArrowId(*(TEXT("body_") + Path.Key));
        if (!IsVisualizationArrowVisible(ArrowId))
        {
            UpdateArrow(
                ArrowId,
                FVector::ZeroVector,
                StandardVectorLengthCentimeters);
            continue;
        }
        FVector BodyPositionIcrf;
        UpdateArrow(
            ArrowId,
            EvaluateOverviewPathAtCurrentTime(Path, BodyPositionIcrf)
                ? BodyPositionIcrf - SpacecraftPositionIcrf
                : FVector::ZeroVector,
            StandardVectorLengthCentimeters,
            Path.ReferenceRadiusMeters);
    }

    // Catalog bodies without CSV or overview tracks are evaluated directly
    // from SPICE only while selected. Distance never removes their checkbox or
    // direction vector; failed kernel coverage merely makes that live target
    // unavailable for the affected epoch.
    TSet<FString> CachedBodyKeys = CsvBodyKeys;
    for (const FTGSolarSystemOverviewPath& Path : SolarSystemOverviewPaths)
    {
        if (!Path.bSpacecraft)
        {
            CachedBodyKeys.Add(Path.Key);
        }
    }
    for (const TPair<FString, FString>& Pair : VectorBodySpiceTargets)
    {
        if (CachedBodyKeys.Contains(Pair.Key))
        {
            continue;
        }

        const FName ArrowId(*(TEXT("body_") + Pair.Key));
        if (!IsVisualizationArrowVisible(ArrowId))
        {
            UpdateArrow(
                ArrowId,
                FVector::ZeroVector,
                StandardVectorLengthCentimeters);
            continue;
        }

        FVector BodyPositionIcrf;
        FVector IgnoredVelocityIcrf;
        FString SpiceMessage;
        const bool bHasState = FSpiceBridge::GetBodyICRFStateSI(
            Pair.Value,
            GetCurrentEphemerisTimeTdbSeconds(),
            BodyPositionIcrf,
            IgnoredVelocityIcrf,
            SpiceMessage);
        const double* ReferenceRadius =
            VectorBodyReferenceRadiiMeters.Find(Pair.Key);
        UpdateArrow(
            ArrowId,
            bHasState
                ? BodyPositionIcrf - SpacecraftPositionIcrf
                : FVector::ZeroVector,
            StandardVectorLengthCentimeters,
            ReferenceRadius != nullptr ? *ReferenceRadius : 0.0);
    }

    double MaximumForceMagnitude = 0.0;
    for (const FForceTrack& Track : ForceTracks)
    {
        MaximumForceMagnitude = FMath::Max(
            MaximumForceMagnitude,
            InterpolateVector(
                Track.ForceIcrf,
                LowerIndex,
                UpperIndex,
                Alpha).Length());
    }
    const double MaximumLength = FMath::Max(
        StandardVectorLengthCentimeters,
        1.0);
    const double MinimumFraction = FMath::Clamp(
        MinimumForceArrowLengthFraction,
        0.05,
        1.0);
    const double CompressionExponent = FMath::Clamp(
        ForceArrowCompressionExponent,
        0.05,
        1.0);
    for (const FForceTrack& Track : ForceTracks)
    {
        const FVector ForceIcrf = InterpolateVector(
            Track.ForceIcrf,
            LowerIndex,
            UpperIndex,
            Alpha);
        const double Magnitude = ForceIcrf.Length();
        const double LinearFraction = MaximumForceMagnitude > 0.0
            ? Magnitude / MaximumForceMagnitude
            : 0.0;
        const double DisplayFraction = Magnitude > 0.0
            ? FMath::Max(
                MinimumFraction,
                FMath::Pow(
                    FMath::Clamp(LinearFraction, 0.0, 1.0),
                    CompressionExponent))
            : 0.0;
        const double DisplayLength = MaximumLength * DisplayFraction;
        const double LinearLength = MaximumLength * LinearFraction;
        const double RelativeAmplification = LinearLength > 1.0e-12
            ? DisplayLength / LinearLength
            : 1.0;
        UpdateArrow(
            FName(*(TEXT("force_") + Track.Key)),
            ForceIcrf,
            DisplayLength,
            0.0,
            Magnitude,
            RelativeAmplification);
    }

    const FQuat BodyToIcrf = InterpolateQuaternion(
        SpacecraftQuaternionW,
        SpacecraftQuaternionX,
        SpacecraftQuaternionY,
        SpacecraftQuaternionZ,
        LowerIndex,
        UpperIndex,
        Alpha);
    const auto EvaluatePhysicalVectorIcrf = [this,
                                              LowerIndex,
                                              UpperIndex,
                                              Alpha,
                                              &BodyToIcrf](
        const FPhysicalVectorTrack& Track)
    {
        const FVector Components = InterpolateVector(
            Track.Components,
            LowerIndex,
            UpperIndex,
            Alpha);
        return Track.bComponentsInBodyFrame
            ? BodyToIcrf.RotateVector(Components)
            : Components;
    };

    const ETGVisualizationArrowQuantity PhysicalQuantities[] = {
        ETGVisualizationArrowQuantity::Torque,
        ETGVisualizationArrowQuantity::LinearVelocity,
        ETGVisualizationArrowQuantity::LinearMomentum,
        ETGVisualizationArrowQuantity::AngularMomentum,
        ETGVisualizationArrowQuantity::AngularVelocity};
    for (const ETGVisualizationArrowQuantity Quantity : PhysicalQuantities)
    {
        double MaximumMagnitude = 0.0;
        for (const FPhysicalVectorTrack& Track : PhysicalVectorTracks)
        {
            if (Track.Quantity == Quantity)
            {
                MaximumMagnitude = FMath::Max(
                    MaximumMagnitude,
                    EvaluatePhysicalVectorIcrf(Track).Length());
            }
        }

        for (const FPhysicalVectorTrack& Track : PhysicalVectorTracks)
        {
            if (Track.Quantity != Quantity)
            {
                continue;
            }
            const FVector VectorIcrf = EvaluatePhysicalVectorIcrf(Track);
            const double Magnitude = VectorIcrf.Length();
            const double LinearFraction = MaximumMagnitude > 0.0
                ? Magnitude / MaximumMagnitude
                : 0.0;
            const double DisplayFraction = Magnitude > 0.0
                ? FMath::Max(
                    MinimumFraction,
                    FMath::Pow(
                        FMath::Clamp(LinearFraction, 0.0, 1.0),
                        CompressionExponent))
                : 0.0;
            const double DisplayLength = MaximumLength * DisplayFraction;
            const double LinearLength = MaximumLength * LinearFraction;
            const double RelativeAmplification = LinearLength > 1.0e-12
                ? DisplayLength / LinearLength
                : 1.0;
            UpdateArrow(
                Track.ArrowId,
                VectorIcrf,
                DisplayLength,
                0.0,
                Magnitude,
                RelativeAmplification);
        }
    }

    if (BodyReticleLineBatch != nullptr)
    {
        constexpr double MinimumAngularRadiusRadians =
            1.0 * UE_DOUBLE_PI / 180.0;
        constexpr double ApparentRadiusMargin = 1.05;
        constexpr double MaximumAngularRadiusRadians =
            80.0 * UE_DOUBLE_PI / 180.0;
        constexpr int32 CircleSegmentCount = 96;

        const FVector Origin = PlaybackRoot != nullptr
            ? PlaybackRoot->GetComponentLocation()
            : GetActorLocation();
        TArray<FBatchedLine> ReticleSegments;
        for (FTGVisualizationArrowInfo& Info :
             VisualizationArrowInfos)
        {
            if (!Info.bVisible ||
                Info.Quantity !=
                    ETGVisualizationArrowQuantity::BodyDirection ||
                !Info.bBodyTargetAvailable)
            {
                continue;
            }

            const FVector Normal =
                Info.CurrentBodyDirectionUnreal.GetSafeNormal();
            const double DistanceWorldUnits = FVector::Distance(
                Origin,
                Info.CurrentBodyWorldLocation);
            if (Normal.IsNearlyZero() ||
                DistanceWorldUnits <= UE_DOUBLE_SMALL_NUMBER)
            {
                continue;
            }

            const double AngularRadius = FMath::Clamp(
                FMath::Max(
                    MinimumAngularRadiusRadians,
                    Info.CurrentBodyAngularRadiusRadians *
                        ApparentRadiusMargin),
                MinimumAngularRadiusRadians,
                MaximumAngularRadiusRadians);
            const double RadiusWorldUnits =
                DistanceWorldUnits * FMath::Tan(AngularRadius);

            FVector Right = FVector::CrossProduct(
                FVector::UpVector,
                Normal).GetSafeNormal();
            if (Right.IsNearlyZero())
            {
                Right = FVector::CrossProduct(
                    FVector::ForwardVector,
                    Normal).GetSafeNormal();
            }
            const FVector Up = FVector::CrossProduct(
                Normal,
                Right).GetSafeNormal();
            if (Right.IsNearlyZero() || Up.IsNearlyZero())
            {
                continue;
            }

            FVector PreviousPoint = Info.CurrentBodyWorldLocation +
                Right * RadiusWorldUnits;
            for (int32 Segment = 1;
                 Segment <= CircleSegmentCount;
                 ++Segment)
            {
                const double Angle = 2.0 * UE_DOUBLE_PI *
                    static_cast<double>(Segment) /
                    static_cast<double>(CircleSegmentCount);
                const FVector CurrentPoint =
                    Info.CurrentBodyWorldLocation +
                    (Right * FMath::Cos(Angle) +
                        Up * FMath::Sin(Angle)) * RadiusWorldUnits;
                ReticleSegments.Emplace(
                    PreviousPoint,
                    CurrentPoint,
                    Info.DisplayColor,
                    -1.0f,
                    1.0f,
                    0);
                PreviousPoint = CurrentPoint;
            }

        }

        if (!ReticleSegments.IsEmpty())
        {
            BodyReticleLineBatch->DrawLines(ReticleSegments);
        }
    }

}

bool ATGSimulationPlaybackActor::BuildSpacecraftTrajectorySamples(
    const FCelestialTrack* TranslationReferenceBody,
    const double StartTime,
    const double EndTime,
    const int32 MaximumSampleCount,
    const bool bIncludeCurrentTime,
    TArray<FVector>& OutPositionsIcrfMeters,
    TArray<double>* OutSampleTimes) const
{
    OutPositionsIcrfMeters.Reset();
    if (OutSampleTimes != nullptr)
    {
        OutSampleTimes->Reset();
    }
    if (EndTime <= StartTime || MaximumSampleCount < 3)
    {
        return false;
    }

    int32 RangeStartLower = INDEX_NONE;
    int32 RangeStartUpper = INDEX_NONE;
    int32 RangeEndLower = INDEX_NONE;
    int32 RangeEndUpper = INDEX_NONE;
    double RangeStartAlpha = 0.0;
    double RangeEndAlpha = 0.0;
    if (
        !FindInterpolationRows(
            StartTime,
            RangeStartLower,
            RangeStartUpper,
            RangeStartAlpha) ||
        !FindInterpolationRows(
            EndTime,
            RangeEndLower,
            RangeEndUpper,
            RangeEndAlpha))
    {
        return false;
    }
    (void)RangeStartUpper;
    (void)RangeEndLower;
    (void)RangeStartAlpha;
    (void)RangeEndAlpha;

    const int32 CoveredCsvIntervals = FMath::Max(
        1,
        RangeEndUpper - RangeStartLower);
    const int32 MinimumIntervals = FMath::Max(
        2,
        LocalTrajectorySampleCountPerSide * 2);
    const int32 MaximumIntervals = FMath::Max(
        2,
        MaximumSampleCount - 1);
    const int64 DesiredIntervals = FMath::Max<int64>(
        MinimumIntervals,
        static_cast<int64>(CoveredCsvIntervals) *
            FMath::Max(1, LocalTrajectorySubdivisionsPerCsvInterval));
    const int32 IntervalCount = static_cast<int32>(FMath::Clamp<int64>(
        DesiredIntervals,
        2,
        MaximumIntervals));

    TArray<double> SampleTimes;
    SampleTimes.Reserve(IntervalCount + 2);
    for (int32 Sample = 0; Sample <= IntervalCount; ++Sample)
    {
        SampleTimes.Add(FMath::Lerp(
            StartTime,
            EndTime,
            static_cast<double>(Sample) /
                static_cast<double>(IntervalCount)));
    }

    const double CurrentTime = GetCurrentEphemerisTimeTdbSeconds();
    if (
        bIncludeCurrentTime &&
        CurrentTime > StartTime &&
        CurrentTime < EndTime &&
        !SampleTimes.ContainsByPredicate([CurrentTime](const double Time)
        {
            return FMath::IsNearlyEqual(Time, CurrentTime, 1.0e-9);
        }))
    {
        SampleTimes.Add(CurrentTime);
        SampleTimes.Sort();
    }

    if (OutSampleTimes != nullptr)
    {
        *OutSampleTimes = SampleTimes;
    }

    OutPositionsIcrfMeters.Reserve(SampleTimes.Num());
    for (const double Time : SampleTimes)
    {
        FVector SpacecraftPositionAtTime;
        if (!EvaluateHermitePositionAtEphemerisTime(
                SpacecraftPosition,
                SpacecraftVelocity,
                Time,
                SpacecraftPositionAtTime))
        {
            OutPositionsIcrfMeters.Reset();
            return false;
        }

        if (TranslationReferenceBody == nullptr)
        {
            OutPositionsIcrfMeters.Add(SpacecraftPositionAtTime);
            continue;
        }

        FVector BodyPositionAtTime;
        if (!EvaluateHermitePositionAtEphemerisTime(
                TranslationReferenceBody->Position,
                TranslationReferenceBody->Velocity,
                Time,
                BodyPositionAtTime))
        {
            OutPositionsIcrfMeters.Reset();
            return false;
        }
        OutPositionsIcrfMeters.Add(
            SpacecraftPositionAtTime - BodyPositionAtTime);
    }
    return OutPositionsIcrfMeters.Num() >= 2;
}

void ATGSimulationPlaybackActor::UpdateLocalTrajectory()
{
    if (TrajectoryLineBatch == nullptr)
    {
        return;
    }
    TrajectoryLineBatch->Flush();
    const ETGTrajectoryFrameMode FrameMode =
        GetLocalTrajectoryFrameMode();
    if (
        FrameMode == ETGTrajectoryFrameMode::Hidden ||
        LocalTrajectorySampleCountPerSide < 1)
    {
        return;
    }

    const double CurrentTime = GetCurrentEphemerisTimeTdbSeconds();
    const double StartTime = GetStartEphemerisTimeTdbSeconds();
    const double EndTime = GetEndEphemerisTimeTdbSeconds();
    const double SelectedStartTime = FMath::Lerp(
        StartTime,
        EndTime,
        FMath::Clamp(
            LocalTrajectoryDisplayStartNormalized,
            0.0,
            1.0));
    const double SelectedEndTime = FMath::Lerp(
        StartTime,
        EndTime,
        FMath::Clamp(
            LocalTrajectoryDisplayEndNormalized,
            0.0,
            1.0));
    if (SelectedEndTime <= SelectedStartTime)
    {
        return;
    }

    const FCelestialTrack* TranslationReferenceBody = nullptr;
    if (FrameMode == ETGTrajectoryFrameMode::ClosestBody)
    {
        int32 LowerIndex = INDEX_NONE;
        int32 UpperIndex = INDEX_NONE;
        double Alpha = 0.0;
        FVector CurrentSpacecraftPositionIcrf;
        if (
            !FindInterpolationRows(
                CurrentTime,
                LowerIndex,
                UpperIndex,
                Alpha) ||
            !EvaluateHermitePositionAtEphemerisTime(
                SpacecraftPosition,
                SpacecraftVelocity,
                CurrentTime,
                CurrentSpacecraftPositionIcrf))
        {
            return;
        }

        TranslationReferenceBody = FindClosestPhysicalBodyTrack(
            CurrentSpacecraftPositionIcrf,
            LowerIndex,
            UpperIndex,
            Alpha);
        if (TranslationReferenceBody == nullptr)
        {
            return;
        }
    }

    TArray<FVector> TrajectoryPath;
    TArray<double> SampleTimes;
    if (!BuildSpacecraftTrajectorySamples(
            TranslationReferenceBody,
            SelectedStartTime,
            SelectedEndTime,
            LocalTrajectoryMaximumSampleCount,
            true,
            TrajectoryPath,
            &SampleTimes) ||
        TrajectoryPath.Num() != SampleTimes.Num())
    {
        return;
    }

    int32 CurrentPointIndex = 0;
    double SmallestCurrentTimeDifference =
        TNumericLimits<double>::Max();
    for (int32 PointIndex = 0;
         PointIndex < SampleTimes.Num();
         ++PointIndex)
    {
        const double Difference = FMath::Abs(
            SampleTimes[PointIndex] - CurrentTime);
        if (Difference < SmallestCurrentTimeDifference)
        {
            SmallestCurrentTimeDifference = Difference;
            CurrentPointIndex = PointIndex;
        }
    }
    const FVector CurrentPathPosition = TrajectoryPath[CurrentPointIndex];

    TArray<FVector> DisplayPoints;
    DisplayPoints.Reserve(TrajectoryPath.Num());
    for (const FVector& PathPoint : TrajectoryPath)
    {
        // In both modes the axes remain ICRF-oriented. Closest-body mode has
        // already removed that body's simultaneous translation at each time.
        // Subtract the current path point to keep the spacecraft at the
        // visualization origin.
        const FVector LocalMeters = ConvertIcrfVectorToUnreal(
            PathPoint - CurrentPathPosition);
        DisplayPoints.Add(GetActorTransform().TransformPositionNoScale(
            LocalMeters * WorldUnitsPerMeter));
    }

    const float DisplayThicknessPixels = static_cast<float>(FMath::Max(
        0.1,
        TrajectoryLineThickness));

    TArray<FBatchedLine> TrajectorySegments;
    TrajectorySegments.Reserve(FMath::Max(0, DisplayPoints.Num() - 1));
    const FLinearColor DisplayColor =
        LocalTrajectoryColorMode == ETGTrajectoryColorMode::Red
            ? FLinearColor::Red
            : FLinearColor::White;

    for (int32 PointIndex = 1;
         PointIndex < DisplayPoints.Num();
         ++PointIndex)
    {
        const FVector& From = DisplayPoints[PointIndex - 1];
        const FVector& To = DisplayPoints[PointIndex];
        if (From.Equals(To, UE_DOUBLE_SMALL_NUMBER))
        {
            continue;
        }
        TrajectorySegments.Emplace(
            From,
            To,
            DisplayColor,
            -1.0f,
            DisplayThicknessPixels,
            0);
    }

    if (!TrajectorySegments.IsEmpty())
    {
        // Submit the complete path in one render-state update. The custom
        // component interprets thickness as pixels while preserving depth.
        TrajectoryLineBatch->DrawLines(TrajectorySegments);
    }
}

double ATGSimulationPlaybackActor::InterpolateColumn(
    const int32 Column,
    const int32 LowerIndex,
    const int32 UpperIndex,
    const double Alpha,
    const double Fallback) const
{
    if (
        Column == INDEX_NONE ||
        !NumericRows.IsValidIndex(LowerIndex) ||
        !NumericRows.IsValidIndex(UpperIndex) ||
        !NumericRows[LowerIndex].IsValidIndex(Column) ||
        !NumericRows[UpperIndex].IsValidIndex(Column))
    {
        return Fallback;
    }
    return FMath::Lerp(
        NumericRows[LowerIndex][Column],
        NumericRows[UpperIndex][Column],
        Alpha);
}

bool ATGSimulationPlaybackActor::EvaluateVectorAtEphemerisTime(
    const FColumnVector& Columns,
    const double EphemerisTime,
    FVector& OutValue) const
{
    OutValue = FVector::ZeroVector;
    if (!Columns.IsComplete())
    {
        return false;
    }
    int32 Lower = INDEX_NONE;
    int32 Upper = INDEX_NONE;
    double Alpha = 0.0;
    if (!FindInterpolationRows(EphemerisTime, Lower, Upper, Alpha))
    {
        return false;
    }
    OutValue = InterpolateVector(Columns, Lower, Upper, Alpha);
    return true;
}

bool ATGSimulationPlaybackActor::EvaluateHermitePositionAtEphemerisTime(
    const FColumnVector& PositionColumns,
    const FColumnVector& VelocityColumns,
    const double EphemerisTime,
    FVector& OutPosition) const
{
    OutPosition = FVector::ZeroVector;
    if (!PositionColumns.IsComplete())
    {
        return false;
    }

    int32 Lower = INDEX_NONE;
    int32 Upper = INDEX_NONE;
    double Alpha = 0.0;
    if (!FindInterpolationRows(EphemerisTime, Lower, Upper, Alpha))
    {
        return false;
    }

    const FVector Position0 = InterpolateVector(
        PositionColumns,
        Lower,
        Lower,
        0.0);
    if (Lower == Upper || !VelocityColumns.IsComplete())
    {
        OutPosition = Lower == Upper
            ? Position0
            : InterpolateVector(PositionColumns, Lower, Upper, Alpha);
        return true;
    }

    const double Time0 = NumericRows[Lower][TimeColumn];
    const double Time1 = NumericRows[Upper][TimeColumn];
    const double TimeSpan = Time1 - Time0;
    if (TimeSpan <= UE_DOUBLE_SMALL_NUMBER)
    {
        OutPosition = Position0;
        return true;
    }

    const FVector Position1 = InterpolateVector(
        PositionColumns,
        Upper,
        Upper,
        0.0);
    const FVector Velocity0 = InterpolateVector(
        VelocityColumns,
        Lower,
        Lower,
        0.0);
    const FVector Velocity1 = InterpolateVector(
        VelocityColumns,
        Upper,
        Upper,
        0.0);

    const double U = FMath::Clamp(
        (EphemerisTime - Time0) / TimeSpan,
        0.0,
        1.0);
    const double U2 = U * U;
    const double U3 = U2 * U;

    // Cubic Hermite position interpolation:
    // p(u) = h00 p0 + h10 Dt v0 + h01 p1 + h11 Dt v1.
    const double H00 = 2.0 * U3 - 3.0 * U2 + 1.0;
    const double H10 = U3 - 2.0 * U2 + U;
    const double H01 = -2.0 * U3 + 3.0 * U2;
    const double H11 = U3 - U2;
    OutPosition =
        H00 * Position0 +
        H10 * TimeSpan * Velocity0 +
        H01 * Position1 +
        H11 * TimeSpan * Velocity1;
    return true;
}

void ATGSimulationPlaybackActor::GetCurrentTelemetryItems(
    TArray<FTGVisualizationTelemetryItem>& OutItems) const
{
    using namespace TGSimulationPlaybackPresentationPrivate;
    OutItems.Reset();

    const auto AddScalar = [&OutItems](
        const TCHAR* Id,
        const TCHAR* Group,
        const TCHAR* Label,
        const double Value,
        const TCHAR* Unit = TEXT(""),
        const TCHAR* Note = TEXT(""))
    {
        FTGVisualizationTelemetryItem& Item =
            OutItems.AddDefaulted_GetRef();
        Item.ItemId = FName(Id);
        Item.Group = Group;
        Item.DisplayName = Label;
        Item.Unit = Unit;
        Item.Type = ETGVisualizationTelemetryType::Scalar;
        Item.ScalarValue = Value;
        Item.Note = Note;
    };
    const auto AddVector = [&OutItems](
        const TCHAR* Id,
        const TCHAR* Group,
        const TCHAR* Label,
        const FVector& Value,
        const TCHAR* Unit)
    {
        FTGVisualizationTelemetryItem& Item =
            OutItems.AddDefaulted_GetRef();
        Item.ItemId = FName(Id);
        Item.Group = Group;
        Item.DisplayName = Label;
        Item.Unit = Unit;
        Item.Type = ETGVisualizationTelemetryType::Vector3;
        Item.VectorValue = Value;
    };
    const auto AddVector4 = [&OutItems](
        const TCHAR* Id,
        const TCHAR* Group,
        const TCHAR* Label,
        const FQuat& Value,
        const TCHAR* Unit = TEXT(""))
    {
        FTGVisualizationTelemetryItem& Item =
            OutItems.AddDefaulted_GetRef();
        Item.ItemId = FName(Id);
        Item.Group = Group;
        Item.DisplayName = Label;
        Item.Unit = Unit;
        Item.Type = ETGVisualizationTelemetryType::Vector4;
        Item.Vector4Value = FVector4(
            Value.X,
            Value.Y,
            Value.Z,
            Value.W);
    };
    const auto AddMatrix3 = [&OutItems](
        const TCHAR* Id,
        const TCHAR* Group,
        const TCHAR* Label,
        const FVector& RowX,
        const FVector& RowY,
        const FVector& RowZ,
        const TCHAR* Unit = TEXT(""))
    {
        FTGVisualizationTelemetryItem& Item =
            OutItems.AddDefaulted_GetRef();
        Item.ItemId = FName(Id);
        Item.Group = Group;
        Item.DisplayName = Label;
        Item.Unit = Unit;
        Item.Type = ETGVisualizationTelemetryType::Matrix3;
        Item.MatrixValues = {
            RowX.X, RowX.Y, RowX.Z,
            RowY.X, RowY.Y, RowY.Z,
            RowZ.X, RowZ.Y, RowZ.Z};
    };
    const auto AddText = [&OutItems](
        const TCHAR* Id,
        const TCHAR* Group,
        const TCHAR* Label,
        const FString& Value,
        const ETGVisualizationTelemetryType Type =
            ETGVisualizationTelemetryType::Text)
    {
        FTGVisualizationTelemetryItem& Item =
            OutItems.AddDefaulted_GetRef();
        Item.ItemId = FName(Id);
        Item.Group = Group;
        Item.DisplayName = Label;
        Item.Type = Type;
        Item.TextValue = Value;
    };

    if (!bResultLoaded)
    {
        AddText(
            TEXT("result_status"),
            TEXT("Simulation"),
            TEXT("Result"),
            TEXT("No simulation result loaded"),
            ETGVisualizationTelemetryType::Status);
        return;
    }

    int32 Lower = INDEX_NONE;
    int32 Upper = INDEX_NONE;
    double Alpha = 0.0;
    if (!FindInterpolationRows(
            GetCurrentEphemerisTimeTdbSeconds(),
            Lower,
            Upper,
            Alpha))
    {
        return;
    }

    const auto FindColumn = [this](const TCHAR* Name) -> int32
    {
        const int32* Column = ColumnIndices.Find(Name);
        return Column != nullptr ? *Column : INDEX_NONE;
    };

    TArray<const FTGComponentConfig*> VariableMassComponents;
    TArray<const FTGJointDofConfig*> JointDofs;
    for (const FTGComponentConfig& Component : ScenarioSnapshot.Components)
    {
        if (Component.bVariableMass)
        {
            VariableMassComponents.Add(&Component);
        }
        for (const FTGJointDofConfig& Dof : Component.DegreesOfFreedom)
        {
            JointDofs.Add(&Dof);
        }
    }

    const FVector PositionIcrf = InterpolateVector(
        SpacecraftPosition,
        Lower,
        Upper,
        Alpha);
    const FVector VelocityIcrf = InterpolateVector(
        SpacecraftVelocity,
        Lower,
        Upper,
        Alpha);
    const FVector OmegaBody = InterpolateVector(
        SpacecraftAngularVelocityBody,
        Lower,
        Upper,
        Alpha);
    const FQuat BodyToIcrf = InterpolateQuaternion(
        SpacecraftQuaternionW,
        SpacecraftQuaternionX,
        SpacecraftQuaternionY,
        SpacecraftQuaternionZ,
        Lower,
        Upper,
        Alpha);
    const FVector OmegaIcrf = BodyToIcrf.RotateVector(OmegaBody);

    AddScalar(
        TEXT("mass"),
        TEXT("Spacecraft"),
        TEXT("Mass"),
        InterpolateColumn(MassColumn, Lower, Upper, Alpha),
        TEXT("kg"));
    AddScalar(
        TEXT("mass_rate"),
        TEXT("Spacecraft"),
        TEXT("Mass Rate"),
        InterpolateColumn(MassRateColumn, Lower, Upper, Alpha),
        TEXT("kg/s"));

    for (int32 Index = 0; Index < VariableMassComponents.Num(); ++Index)
    {
        const FString ColumnName = FString::Printf(
            TEXT("variable_component_mass_%d_kg"),
            Index);
        const int32 Column = FindColumn(*ColumnName);
        if (Column != INDEX_NONE)
        {
            AddScalar(
                *ColumnName,
                TEXT("Variable Mass"),
                *(VariableMassComponents[Index]->Name + TEXT(" Mass")),
                InterpolateColumn(Column, Lower, Upper, Alpha),
                TEXT("kg"));
        }
    }

    AddVector(
        TEXT("position_icrf"),
        TEXT("State (ICRF)"),
        TEXT("Position (ICRF)"),
        PositionIcrf,
        TEXT("m"));
    AddVector(
        TEXT("velocity_icrf"),
        TEXT("State (ICRF)"),
        TEXT("Velocity (ICRF)"),
        VelocityIcrf,
        TEXT("m/s"));
    AddScalar(
        TEXT("speed_icrf"),
        TEXT("State (ICRF)"),
        TEXT("Speed (ICRF)"),
        VelocityIcrf.Length(),
        TEXT("m/s"));
    AddVector(
        TEXT("angular_velocity_body"),
        TEXT("Attitude"),
        TEXT("Angular Velocity (Body Frame)"),
        OmegaBody,
        TEXT("rad/s"));
    AddVector(
        TEXT("angular_velocity_icrf"),
        TEXT("Attitude"),
        TEXT("Angular Velocity (ICRF)"),
        OmegaIcrf,
        TEXT("rad/s"));
    AddVector4(
        TEXT("attitude_quaternion"),
        TEXT("Attitude"),
        TEXT("Body-to-ICRF Quaternion"),
        BodyToIcrf);

    AddVector(
        TEXT("center_of_mass_body"),
        TEXT("Mass Properties"),
        TEXT("Center of Mass (Body Frame)"),
        InterpolateVector(
            SpacecraftCenterOfMassBody,
            Lower,
            Upper,
            Alpha),
        TEXT("m"));
    const FColumnVector InertiaRowX = {
        FindColumn(TEXT("inertia_body_xx_kgm2")),
        FindColumn(TEXT("inertia_body_xy_kgm2")),
        FindColumn(TEXT("inertia_body_xz_kgm2"))};
    const FColumnVector InertiaRowY = {
        FindColumn(TEXT("inertia_body_yx_kgm2")),
        FindColumn(TEXT("inertia_body_yy_kgm2")),
        FindColumn(TEXT("inertia_body_yz_kgm2"))};
    const FColumnVector InertiaRowZ = {
        FindColumn(TEXT("inertia_body_zx_kgm2")),
        FindColumn(TEXT("inertia_body_zy_kgm2")),
        FindColumn(TEXT("inertia_body_zz_kgm2"))};
    if (InertiaRowX.IsComplete() &&
        InertiaRowY.IsComplete() &&
        InertiaRowZ.IsComplete())
    {
        AddMatrix3(
            TEXT("inertia_body"),
            TEXT("Mass Properties"),
            TEXT("Inertia Matrix (Body Frame)"),
            InterpolateVector(InertiaRowX, Lower, Upper, Alpha),
            InterpolateVector(InertiaRowY, Lower, Upper, Alpha),
            InterpolateVector(InertiaRowZ, Lower, Upper, Alpha),
            TEXT("kg m^2"));
    }

    const FCelestialTrack* Closest = FindClosestPhysicalBodyTrack(
        PositionIcrf,
        Lower,
        Upper,
        Alpha);
    if (Closest != nullptr)
    {
        const FVector BodyPosition = InterpolateVector(
            Closest->Position,
            Lower,
            Upper,
            Alpha);
        const FVector BodyVelocity = Closest->Velocity.IsComplete()
            ? InterpolateVector(Closest->Velocity, Lower, Upper, Alpha)
            : FVector::ZeroVector;
        const double Radius = InterpolateColumn(
            Closest->ReferenceRadius,
            Lower,
            Upper,
            Alpha);
        const FVector RelativePositionIcrf = PositionIcrf - BodyPosition;
        FVector RelativePositionFixed = RelativePositionIcrf;
        FVector RelativeVelocityFixed = VelocityIcrf - BodyVelocity;
        FVector RelativeOmegaFixed = OmegaIcrf;

        if (Closest->Rotation.IsComplete())
        {
            const FQuat FixedToIcrf = InterpolateRotationMatrix(
                Closest->Rotation,
                Lower,
                Upper,
                Alpha);
            FVector BodyOmegaIcrf = FVector::ZeroVector;
            int32 RotationLower = Lower;
            int32 RotationUpper = Upper;
            if (RotationLower == RotationUpper && NumericRows.Num() > 1)
            {
                if (RotationUpper + 1 < NumericRows.Num())
                {
                    ++RotationUpper;
                }
                else
                {
                    --RotationLower;
                }
            }
            const double RotationDt =
                NumericRows[RotationUpper][TimeColumn] -
                NumericRows[RotationLower][TimeColumn];
            if (RotationDt > 0.0)
            {
                const FQuat Q0 = QuaternionFromMatrixValues(
                    NumericRows[RotationLower],
                    Closest->Rotation);
                const FQuat Q1 = QuaternionFromMatrixValues(
                    NumericRows[RotationUpper],
                    Closest->Rotation);
                FQuat Delta = (Q1 * Q0.Inverse()).GetNormalized();
                if (Delta.W < 0.0)
                {
                    Delta = Delta * -1.0;
                }
                FVector Axis = FVector::ZeroVector;
                double Angle = 0.0;
                Delta.ToAxisAndAngle(Axis, Angle);
                if (!Axis.ContainsNaN())
                {
                    BodyOmegaIcrf = Axis * (Angle / RotationDt);
                }
            }

            RelativePositionFixed =
                FixedToIcrf.Inverse().RotateVector(RelativePositionIcrf);
            RelativeVelocityFixed = FixedToIcrf.Inverse().RotateVector(
                (VelocityIcrf - BodyVelocity) -
                FVector::CrossProduct(
                    BodyOmegaIcrf,
                    RelativePositionIcrf));
            RelativeOmegaFixed = FixedToIcrf.Inverse().RotateVector(
                OmegaIcrf - BodyOmegaIcrf);
        }

        AddText(
            TEXT("closest_body"),
            TEXT("Closest Body"),
            TEXT("Closest Body"),
            ResolveCelestialDisplayName(Closest->Key));
        AddScalar(
            TEXT("altitude"),
            TEXT("Closest Body"),
            TEXT("Altitude"),
            RelativePositionIcrf.Length() - Radius,
            TEXT("m"),
            TEXT("(Estimate from Average Body Radius)"));
        AddVector(
            TEXT("position_body_fixed"),
            TEXT("Closest Body"),
            TEXT("Position (Body-Fixed)"),
            RelativePositionFixed,
            TEXT("m"));
        AddVector(
            TEXT("velocity_body_fixed"),
            TEXT("Closest Body"),
            TEXT("Velocity (Body-Fixed)"),
            RelativeVelocityFixed,
            TEXT("m/s"));
        AddVector(
            TEXT("angular_velocity_body_fixed"),
            TEXT("Closest Body"),
            TEXT("Angular Velocity (Body-Fixed)"),
            RelativeOmegaFixed,
            TEXT("rad/s"));
    }

    for (const FForceTrack& Track : ForceTracks)
    {
        FString Label;
        if (Track.Key == TEXT("total"))
        {
            Label = TEXT("Total Force");
        }
        else if (Track.Key == TEXT("srp"))
        {
            Label = TEXT("Solar Radiation Pressure Force (SRP)");
        }
        else
        {
            Label = PrettifyKey(Track.Key) + TEXT(" Force");
        }
        AddVector(
            *(TEXT("force_") + Track.Key + TEXT("_icrf")),
            TEXT("Forces (ICRF)"),
            *Label,
            InterpolateVector(Track.ForceIcrf, Lower, Upper, Alpha),
            TEXT("N"));
    }
    const TCHAR* TorqueKeys[] = {
        TEXT("total"),
        TEXT("gravity"),
        TEXT("thrust"),
        TEXT("srp"),
        TEXT("aerodynamic"),
        TEXT("control")};
    for (const TCHAR* Key : TorqueKeys)
    {
        const FString Prefix = FString::Printf(
            TEXT("torque_%s_body_"),
            Key);
        const FColumnVector Torque = {
            FindColumn(*(Prefix + TEXT("x_nm"))),
            FindColumn(*(Prefix + TEXT("y_nm"))),
            FindColumn(*(Prefix + TEXT("z_nm")))};
        if (!Torque.IsComplete())
        {
            continue;
        }
        const FString KeyString(Key);
        FString Label;
        if (KeyString == TEXT("total"))
        {
            Label = TEXT("Total Torque");
        }
        else if (KeyString == TEXT("srp"))
        {
            Label = TEXT("Solar Radiation Pressure Torque (SRP)");
        }
        else
        {
            Label = PrettifyKey(KeyString) + TEXT(" Torque");
        }
        AddVector(
            *(TEXT("torque_") + KeyString + TEXT("_body")),
            TEXT("Torques (Body Frame)"),
            *Label,
            InterpolateVector(Torque, Lower, Upper, Alpha),
            TEXT("N m"));
    }

    const FColumnVector BaseAcceleration = {
        FindColumn(TEXT("base_origin_acceleration_body_x_mps2")),
        FindColumn(TEXT("base_origin_acceleration_body_y_mps2")),
        FindColumn(TEXT("base_origin_acceleration_body_z_mps2"))};
    if (BaseAcceleration.IsComplete())
    {
        AddVector(
            TEXT("base_origin_acceleration_body"),
            TEXT("Dynamics"),
            TEXT("Base-Origin Acceleration (Body Frame)"),
            InterpolateVector(BaseAcceleration, Lower, Upper, Alpha),
            TEXT("m/s^2"));
    }
    const FColumnVector LinearMomentum = {
        FindColumn(TEXT("linear_momentum_icrf_x_kgmps")),
        FindColumn(TEXT("linear_momentum_icrf_y_kgmps")),
        FindColumn(TEXT("linear_momentum_icrf_z_kgmps"))};
    if (LinearMomentum.IsComplete())
    {
        AddVector(
            TEXT("linear_momentum_icrf"),
            TEXT("Dynamics"),
            TEXT("Linear Momentum (ICRF)"),
            InterpolateVector(LinearMomentum, Lower, Upper, Alpha),
            TEXT("kg m/s"));
    }
    const FColumnVector AngularMomentum = {
        FindColumn(TEXT("angular_momentum_about_cm_icrf_x_kgm2ps")),
        FindColumn(TEXT("angular_momentum_about_cm_icrf_y_kgm2ps")),
        FindColumn(TEXT("angular_momentum_about_cm_icrf_z_kgm2ps"))};
    if (AngularMomentum.IsComplete())
    {
        AddVector(
            TEXT("angular_momentum_about_cm_icrf"),
            TEXT("Dynamics"),
            TEXT("Angular Momentum about CM (ICRF)"),
            InterpolateVector(AngularMomentum, Lower, Upper, Alpha),
            TEXT("kg m^2/s"));
    }

    AddScalar(
        TEXT("visible_sun_fraction"),
        TEXT("Environment"),
        TEXT("Visible Sun Fraction"),
        InterpolateColumn(
            VisibleSunFractionColumn,
            Lower,
            Upper,
            Alpha));

    const int32 DynamicPressure = FindColumn(
        TEXT("aerodynamic_dynamic_pressure_pa"));
    const int32 SpeedRatio = FindColumn(
        TEXT("aerodynamic_molecular_speed_ratio"));
    const int32 Knudsen = FindColumn(
        TEXT("aerodynamic_knudsen_number"));
    const int32 AerodynamicsOutsideValidity = FindColumn(
        TEXT("aerodynamics_outside_validity"));
    if (DynamicPressure != INDEX_NONE)
    {
        AddScalar(
            TEXT("aerodynamic_dynamic_pressure"),
            TEXT("Environment"),
            TEXT("Dynamic Pressure"),
            InterpolateColumn(DynamicPressure, Lower, Upper, Alpha),
            TEXT("Pa"));
        if (SpeedRatio != INDEX_NONE)
        {
            AddScalar(
                TEXT("aerodynamic_molecular_speed_ratio"),
                TEXT("Environment"),
                TEXT("Molecular Speed Ratio"),
                InterpolateColumn(SpeedRatio, Lower, Upper, Alpha));
        }
        if (Knudsen != INDEX_NONE)
        {
            AddScalar(
                TEXT("aerodynamic_knudsen_number"),
                TEXT("Environment"),
                TEXT("Knudsen Number"),
                InterpolateColumn(Knudsen, Lower, Upper, Alpha));
        }
    }
    if (AerodynamicsOutsideValidity != INDEX_NONE)
    {
        const bool bOutsideValidity = InterpolateColumn(
            AerodynamicsOutsideValidity,
            Lower,
            Upper,
            Alpha) >= 0.5;
        AddText(
            TEXT("aerodynamics_validity"),
            TEXT("Environment"),
            TEXT("Aerodynamics Validity"),
            bOutsideValidity
                ? TEXT("Outside Configured Validity")
                : TEXT("Within Configured Validity"),
            ETGVisualizationTelemetryType::Status);
    }
    const FColumnVector AerodynamicForceCoefficients = {
        FindColumn(TEXT("aerodynamic_force_coefficient_body_x")),
        FindColumn(TEXT("aerodynamic_force_coefficient_body_y")),
        FindColumn(TEXT("aerodynamic_force_coefficient_body_z"))};
    if (AerodynamicForceCoefficients.IsComplete())
    {
        AddVector(
            TEXT("aerodynamic_force_coefficients_body"),
            TEXT("Environment"),
            TEXT("Aerodynamic Force Coefficient (Body Frame)"),
            InterpolateVector(
                AerodynamicForceCoefficients,
                Lower,
                Upper,
                Alpha),
            TEXT(""));
    }
    const FColumnVector AerodynamicMomentCoefficients = {
        FindColumn(
            TEXT("aerodynamic_moment_coefficient_about_cm_body_x")),
        FindColumn(
            TEXT("aerodynamic_moment_coefficient_about_cm_body_y")),
        FindColumn(
            TEXT("aerodynamic_moment_coefficient_about_cm_body_z"))};
    if (AerodynamicMomentCoefficients.IsComplete())
    {
        AddVector(
            TEXT("aerodynamic_moment_coefficients_body"),
            TEXT("Environment"),
            TEXT("Aerodynamic Moment Coefficient about CM (Body Frame)"),
            InterpolateVector(
                AerodynamicMomentCoefficients,
                Lower,
                Upper,
                Alpha),
            TEXT(""));
    }
    for (int32 Index = 0; Index < JointDofs.Num(); ++Index)
    {
        const FTGJointDofConfig& Dof = *JointDofs[Index];
        const FString DofName = Dof.Name.IsEmpty()
            ? FString::Printf(TEXT("Joint %d"), Index)
            : Dof.Name;
        const bool bRotation =
            Dof.MotionType == ETGJointMotionType::Rotation;
        struct FJointTelemetryColumn
        {
            const TCHAR* Prefix;
            const TCHAR* ColumnSuffix;
            const TCHAR* LabelSuffix;
            const TCHAR* RotationUnit;
            const TCHAR* TranslationUnit;
        };
        const FJointTelemetryColumn JointColumns[] = {
            {TEXT("articulation_coordinate_"),
             TEXT("_rad_or_m"), TEXT(" Coordinate"),
             TEXT("rad"), TEXT("m")},
            {TEXT("articulation_rate_"),
             TEXT("_radps_or_mps"), TEXT(" Rate"),
             TEXT("rad/s"), TEXT("m/s")},
            {TEXT("applied_joint_effort_"),
             TEXT("_nm_or_n"), TEXT(" Applied Effort"),
             TEXT("N m"), TEXT("N")},
            {TEXT("joint_constraint_effort_"),
             TEXT("_nm_or_n"), TEXT(" Constraint Effort"),
             TEXT("N m"), TEXT("N")}};
        for (const FJointTelemetryColumn& Definition : JointColumns)
        {
            const FString ColumnName = FString::Printf(
                TEXT("%s%d%s"),
                Definition.Prefix,
                Index,
                Definition.ColumnSuffix);
            const int32 Column = FindColumn(*ColumnName);
            if (Column == INDEX_NONE)
            {
                continue;
            }
            AddScalar(
                *ColumnName,
                TEXT("Joints"),
                *(DofName + Definition.LabelSuffix),
                InterpolateColumn(Column, Lower, Upper, Alpha),
                bRotation
                    ? Definition.RotationUnit
                    : Definition.TranslationUnit);
        }
    }

    for (int32 Index = 0; Index < ScenarioSnapshot.Thrusters.Num(); ++Index)
    {
        const FString ColumnName = FString::Printf(
            TEXT("thruster_thrust_%d_n"),
            Index);
        const int32 Column = FindColumn(*ColumnName);
        if (Column == INDEX_NONE)
        {
            continue;
        }
        const FString ThrusterName =
            ScenarioSnapshot.Thrusters[Index].Name.IsEmpty()
                ? FString::Printf(TEXT("Thruster %d"), Index + 1)
                : ScenarioSnapshot.Thrusters[Index].Name;
        AddScalar(
            *ColumnName,
            TEXT("Thrusters"),
            *(ThrusterName + TEXT(" Thrust")),
            InterpolateColumn(Column, Lower, Upper, Alpha),
            TEXT("N"));
    }

    for (int32 Index = 0;
         Index < ScenarioSnapshot.ReactionWheels.Num();
         ++Index)
    {
        const FString ColumnName = FString::Printf(
            TEXT("reaction_wheel_momentum_%d_nms"),
            Index);
        const int32 Column = FindColumn(*ColumnName);
        if (Column != INDEX_NONE)
        {
            AddScalar(
                *ColumnName,
                TEXT("Reaction Wheels"),
                *(ScenarioSnapshot.ReactionWheels[Index].Name +
                    TEXT(" Momentum")),
                InterpolateColumn(Column, Lower, Upper, Alpha),
                TEXT("N m s"));
        }
    }

    for (const FComponentTrack& Track : ComponentTracks)
    {
        const FTGComponentConfig* Component =
            ScenarioSnapshot.Components.FindByPredicate(
                [this, &Track](const FTGComponentConfig& Candidate)
                {
                    return NormalizeColumnKey(Candidate.Name) == Track.Key;
                });
        const FString ComponentName = Component != nullptr
            ? Component->Name
            : PrettifyKey(Track.Key);
        AddVector(
            *(TEXT("component_") + Track.Key + TEXT("_origin_body")),
            TEXT("Component Poses"),
            *(ComponentName + TEXT(" Origin (Body Frame)")),
            InterpolateVector(Track.Origin, Lower, Upper, Alpha),
            TEXT("m"));

        const FQuat ComponentToBody = InterpolateRotationMatrix(
            Track.Rotation,
            Lower,
            Upper,
            Alpha);
        AddVector4(
            *(TEXT("component_") + Track.Key +
                TEXT("_orientation")),
            TEXT("Component Poses"),
            *(ComponentName + TEXT(" Orientation (Component-to-Body)")),
            ComponentToBody);
    }
}

void ATGSimulationPlaybackActor::GetCurrentTelemetryLines(
    TArray<FString>& OutLines) const
{
    OutLines.Reset();
    TArray<FTGVisualizationTelemetryItem> Items;
    GetCurrentTelemetryItems(Items);

    const auto PlainNumber = [](const double Value)
    {
        FString Result = FString::Printf(TEXT("%.9f"), Value);
        while (Result.Contains(TEXT(".")) && Result.EndsWith(TEXT("0")))
        {
            Result.LeftChopInline(1);
        }
        if (Result.EndsWith(TEXT(".")))
        {
            Result.LeftChopInline(1);
        }
        return Result == TEXT("-0") ? FString(TEXT("0")) : Result;
    };

    for (const FTGVisualizationTelemetryItem& Item : Items)
    {
        FString Value;
        switch (Item.Type)
        {
        case ETGVisualizationTelemetryType::Scalar:
            Value = PlainNumber(Item.ScalarValue);
            break;
        case ETGVisualizationTelemetryType::Vector3:
            Value = FString::Printf(
                TEXT("X %s  Y %s  Z %s"),
                *PlainNumber(Item.VectorValue.X),
                *PlainNumber(Item.VectorValue.Y),
                *PlainNumber(Item.VectorValue.Z));
            break;
        case ETGVisualizationTelemetryType::Vector4:
            Value = FString::Printf(
                TEXT("W %s  X %s  Y %s  Z %s"),
                *PlainNumber(Item.Vector4Value.W),
                *PlainNumber(Item.Vector4Value.X),
                *PlainNumber(Item.Vector4Value.Y),
                *PlainNumber(Item.Vector4Value.Z));
            break;
        case ETGVisualizationTelemetryType::Matrix3:
            if (Item.MatrixValues.Num() >= 9)
            {
                Value = FString::Printf(
                    TEXT("[%s %s %s; %s %s %s; %s %s %s]"),
                    *PlainNumber(Item.MatrixValues[0]),
                    *PlainNumber(Item.MatrixValues[1]),
                    *PlainNumber(Item.MatrixValues[2]),
                    *PlainNumber(Item.MatrixValues[3]),
                    *PlainNumber(Item.MatrixValues[4]),
                    *PlainNumber(Item.MatrixValues[5]),
                    *PlainNumber(Item.MatrixValues[6]),
                    *PlainNumber(Item.MatrixValues[7]),
                    *PlainNumber(Item.MatrixValues[8]));
            }
            break;
        case ETGVisualizationTelemetryType::Text:
        case ETGVisualizationTelemetryType::Status:
            Value = Item.TextValue;
            break;
        default:
            break;
        }
        if (!Item.Unit.IsEmpty())
        {
            Value += TEXT(" ") + Item.Unit;
        }
        OutLines.Add(Item.DisplayName + TEXT(": ") + Value);
    }
}

FVector ATGSimulationPlaybackActor::ConvertIcrfVectorToUnreal(
    const FVector& VectorIcrf)
{
    return FVector(VectorIcrf.X, -VectorIcrf.Y, VectorIcrf.Z);
}

FLinearColor ATGSimulationPlaybackActor::ResolveBodyColor(
    const FString& Key)
{
    const FString Normalized = NormalizeColumnKey(Key);
    if (Normalized == TEXT("sun")) return FLinearColor(1.0f, 0.78f, 0.16f);
    if (Normalized == TEXT("mercury")) return FLinearColor(0.58f, 0.55f, 0.50f);
    if (Normalized == TEXT("venus")) return FLinearColor(0.93f, 0.66f, 0.24f);
    if (Normalized == TEXT("earth")) return FLinearColor(0.12f, 0.48f, 0.95f);
    if (Normalized == TEXT("moon")) return FLinearColor(0.78f, 0.80f, 0.84f);
    if (Normalized == TEXT("mars")) return FLinearColor(0.83f, 0.22f, 0.12f);
    if (Normalized == TEXT("jupiter")) return FLinearColor(0.82f, 0.56f, 0.32f);
    if (Normalized == TEXT("saturn")) return FLinearColor(0.91f, 0.79f, 0.46f);
    if (Normalized == TEXT("uranus")) return FLinearColor(0.28f, 0.82f, 0.84f);
    if (Normalized == TEXT("neptune")) return FLinearColor(0.18f, 0.30f, 0.88f);
    if (Normalized == TEXT("pluto")) return FLinearColor(0.70f, 0.56f, 0.48f);
    const uint32 Hash = GetTypeHash(Normalized);
    return FLinearColor::MakeFromHSV8(
        static_cast<uint8>(Hash % 255),
        150,
        235);
}

FLinearColor ATGSimulationPlaybackActor::ResolveForceColor(
    const FString& Key)
{
    const FString Normalized = NormalizeColumnKey(Key);
    if (Normalized == TEXT("total")) return FLinearColor::White;
    if (Normalized == TEXT("gravity")) return FLinearColor(0.18f, 0.66f, 1.0f);
    if (Normalized == TEXT("thrust")) return FLinearColor(1.0f, 0.24f, 0.10f);
    if (Normalized == TEXT("srp")) return FLinearColor(1.0f, 0.84f, 0.12f);
    if (Normalized == TEXT("aerodynamic")) return FLinearColor(0.16f, 0.86f, 0.46f);
    const uint32 Hash = GetTypeHash(Normalized);
    return FLinearColor::MakeFromHSV8(
        static_cast<uint8>(Hash % 255),
        185,
        245);
}

FLinearColor ATGSimulationPlaybackActor::ResolveVectorColor(
    const ETGVisualizationArrowQuantity Quantity,
    const FString& Key)
{
    if (Quantity == ETGVisualizationArrowQuantity::Force)
    {
        return ResolveForceColor(Key);
    }
    if (Quantity == ETGVisualizationArrowQuantity::Torque)
    {
        const FString Normalized = NormalizeColumnKey(Key);
        if (Normalized == TEXT("total"))
        {
            return FLinearColor(0.82f, 0.84f, 0.88f);
        }
        if (Normalized == TEXT("gravity"))
        {
            return FLinearColor(0.38f, 0.48f, 1.0f);
        }
        if (Normalized == TEXT("thrust"))
        {
            return FLinearColor(1.0f, 0.38f, 0.16f);
        }
        if (Normalized == TEXT("srp"))
        {
            return FLinearColor(1.0f, 0.72f, 0.18f);
        }
        if (Normalized == TEXT("aerodynamic"))
        {
            return FLinearColor(0.12f, 0.72f, 0.52f);
        }
        if (Normalized == TEXT("control"))
        {
            return FLinearColor(0.90f, 0.32f, 0.82f);
        }
    }
    if (Quantity == ETGVisualizationArrowQuantity::LinearMomentum)
    {
        return FLinearColor(0.10f, 0.82f, 0.86f);
    }
    if (Quantity == ETGVisualizationArrowQuantity::LinearVelocity)
    {
        return FLinearColor(0.18f, 0.62f, 0.92f);
    }
    if (Quantity == ETGVisualizationArrowQuantity::AngularMomentum)
    {
        return FLinearColor(0.68f, 0.42f, 0.94f);
    }
    if (Quantity == ETGVisualizationArrowQuantity::AngularVelocity)
    {
        return FLinearColor(0.96f, 0.52f, 0.18f);
    }
    return FLinearColor::White;
}
