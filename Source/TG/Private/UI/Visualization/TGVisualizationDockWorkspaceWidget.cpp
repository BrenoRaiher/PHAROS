// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Visualization/TGVisualizationDockWorkspaceWidget.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/TimelineComponent.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Input/DragAndDrop.h"
#include "InputCoreTypes.h"
#include "Layout/Clipping.h"
#include "Misc/ConfigCacheIni.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Simulation/TGCelestialCatalogLibrary.h"
#include "SpiceBridge.h"
#include "Styling/SlateTypes.h"
#include "UI/Common/TGUtcDateTimeInput.h"
#include "UI/Theme/TGUiTheme.h"
#include "UI/Visualization/TGSolarSystemOverviewWidget.h"
#include "HAL/PlatformTime.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace TGVisualizationDockPrivate
{
    constexpr TCHAR LayoutSection[] =
        TEXT("TG.Visualization.DockWorkspace.v2");
    // Increment when persisted layout geometry needs a one-time migration.
    constexpr int32 LayoutRevision = 1;
    constexpr float MenuHeight = 42.0f;
    // EDIT HERE: left/right docking hover/drop-zone thickness in pixels.
    constexpr float SideDockSnapDistance = 250.0f;
    // EDIT HERE: top docking hover/drop-zone thickness in pixels.
    constexpr float TopDockSnapDistance = 42.0f;
    // EDIT HERE: total clickable thickness of each dock splitter bar.
    constexpr float EdgeHandleThickness = 15.0f;
    // EDIT HERE: visible line thickness inside each dock splitter bar.
    constexpr float DockSplitterVisualThickness = 15.0f;
    // EDIT HERE: centered collapse/restore glyph hit-region length along splitter.
    constexpr float SplitterCollapseButtonLongSize = 34.0f;
    // EDIT HERE: centered collapse/restore glyph hit-region thickness across splitter.
    constexpr float SplitterCollapseButtonShortSize = 18.0f;
    // EDIT HERE: side-dock width below which the clipped X region is hidden.
    // This prevents the close-button face from becoming a square artifact.
    constexpr float MinSideDockExtentForCloseButton = 110.0f;
    // EDIT HERE: top-left/top-right radius of a freely floating panel header.
    constexpr float FloatingPanelTopCornerRadius = 20.0f;
    constexpr float FloatingResizeHitThickness = 7.0f;
    constexpr float FloatingResizeCornerSize = 14.0f;
    constexpr float MinimizedTabWidth = 156.0f;
    constexpr float MinimizedTabHeight = 32.0f;
    constexpr float EdgeCollapseSnapPixels = 86.0f;
    constexpr float PanelContentPadding = 14.0f;
    constexpr float CompactControlHeight = 42.0f;
    constexpr float CompactRowHeight = 44.0f;

    // EDIT HERE: native fixed-bottom timeline geometry.
    // Former 216 px height minus the 40 px play/pause control height.
    constexpr float TimelineDefaultExpandedHeight = 176.0f;
    constexpr float TimelineHandleThickness = EdgeHandleThickness * 1.2f;
    constexpr float TimelineSplitterVisualThickness =
        DockSplitterVisualThickness * 1.2f;
    constexpr float TimelineCollapsedHeight = TimelineHandleThickness;
    constexpr float TimelineCollapseSnapDistance = 34.0f;
    constexpr float TimelineHorizontalMargin = 0.0f;
    constexpr float TimelineBottomMargin = 0.0f; // EDIT HERE: 0 keeps the fixed timeline flush with the screen bottom edge.
    constexpr float TimelineGoButtonWidth = 100.0f;
    // EDIT HERE: fixed widths of the editable timeline text boxes.
    constexpr float TimelineRateInputWidth = 92.0f;
    constexpr float TimelineMissionInputWidth = 250.0f;
    constexpr float TimelineEtInputWidth = 250.0f;
    constexpr float TimelineUtcInputWidth = 250.0f;
    // EDIT HERE: common height for Play/Pause, Go buttons, and timeline text boxes.
    constexpr float TimelineControlHeight = 40.0f;
    constexpr float TimelinePlayPauseButtonWidth = 54.0f;
    // EDIT HERE: Play/Pause glyph font size.
    constexpr int32 TimelinePlayPauseFontSize = 24;
    constexpr float TimelinePlayPauseGlyphOffsetY = -2.0f;
    constexpr double TimelineErrorDisplaySeconds = 10.0;
    constexpr double TimelineErrorFadeSeconds = 1.0;

    const FTGUiPalette& Palette()
    {
        return TGUiTheme::GetPalette();
    }

    FString FormatMissionTime(const double Seconds)
    {
        const double SafeSeconds = FMath::Max(0.0, Seconds);
        const int64 WholeSeconds = FMath::FloorToInt64(SafeSeconds);
        const int64 Days = WholeSeconds / 86400;
        const int64 Hours = (WholeSeconds % 86400) / 3600;
        const int64 Minutes = (WholeSeconds % 3600) / 60;
        const double RemainingSeconds = SafeSeconds - static_cast<double>(
            Days * 86400 + Hours * 3600 + Minutes * 60);
        return FString::Printf(
            TEXT("%lldd %02lld:%02lld:%06.3f"),
            Days,
            Hours,
            Minutes,
            RemainingSeconds);
    }

    const FSlateBrush& PanelFrameBrush()
    {
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            Palette().Panel,
            5.0f,
            Palette().Border,
            1.0f);
        return Brush;
    }

    const FSlateBrush& FloatingPanelFrameBrush()
    {
        // The outer floating-window frame must share the same rounded top
        // silhouette as the tab/X buttons. Otherwise the square frame behind
        // them remains visible around their rounded corners.
        static const FSlateBrush Brush = []()
        {
            FSlateBrush Result = PanelFrameBrush();
            Result.OutlineSettings.CornerRadii = FVector4(
                FloatingPanelTopCornerRadius,
                FloatingPanelTopCornerRadius,
                5.0f,
                5.0f);
            return Result;
        }();
        return Brush;
    }

    const FSlateBrush& PanelFillBrush()
    {
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            Palette().Panel,
            0.0f);
        return Brush;
    }

    const FSlateBrush& HeaderBrush()
    {
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            Palette().Header,
            0.0f,
            Palette().Border,
            1.0f);
        return Brush;
    }

    const FSlateBrush& HeaderFillBrush()
    {
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            Palette().Header,
            0.0f);
        return Brush;
    }

    const FSlateBrush& SurfaceCardBrush()
    {
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            Palette().Surface,
            5.0f,
            Palette().Border,
            1.0f);
        return Brush;
    }

    const FSlateBrush& RaisedCardBrush()
    {
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            Palette().SurfaceRaised,
            4.0f,
            Palette().Border,
            1.0f);
        return Brush;
    }

    const FSlateBrush& ActiveTabShapeBrush()
    {
        // Use the actual final fill color instead of a white tintable brush.
        // This prevents white brush fragments when a dock is narrower than a tab.
        // Corner order: TopLeft, TopRight, BottomRight, BottomLeft.
        static const FSlateRoundedBoxBrush Brush(
            Palette().Panel,
            FVector4(20.0f, 0.0f, 0.0f, 0.0f));
        return Brush;
    }

    const FSlateBrush& InactiveTabShapeBrush()
    {
        static const FSlateRoundedBoxBrush Brush(
            Palette().Header,
            FVector4(20.0f, 0.0f, 0.0f, 0.0f));
        return Brush;
    }

    const FSlateBrush& HoveredTabBrush()
    {
        return TGUiTheme::GetButtonStyle(
            ETGUiButtonStyle::Tab).Hovered;
    }

    const FButtonStyle& TabCloseButtonStyle()
    {
        static const FButtonStyle Style = []()
        {
            // Exact Quiet-button state colors, reconstructed explicitly so the
            // top-left corner is guaranteed to stay square in EVERY state.
            //
            // Corner order: TopLeft, TopRight, BottomRight, BottomLeft.
            const FVector4 CornerRadii(0.0f, 20.0f, 0.0f, 0.0f);

            FButtonStyle Result =
                TGUiTheme::GetButtonStyle(ETGUiButtonStyle::Quiet);

            Result.SetNormal(FSlateRoundedBoxBrush(
                    FLinearColor::Transparent,
                    CornerRadii))
                .SetHovered(FSlateRoundedBoxBrush(
                    Palette().AccentSubtle,
                    CornerRadii))
                .SetPressed(FSlateRoundedBoxBrush(
                    Palette().Selection,
                    CornerRadii))
                .SetDisabled(FSlateRoundedBoxBrush(
                    Palette().Surface,
                    CornerRadii));

            return Result;
        }();

        return Style;
    }

    const FSlateBrush& ActiveTabCloseShapeBrush()
    {
        // Actual final fill color: no white tintable background can leak when clipped.
        // Corner order: TopLeft, TopRight, BottomRight, BottomLeft.
        static const FSlateRoundedBoxBrush Brush(
            Palette().Panel,
            FVector4(0.0f, 20.0f, 0.0f, 0.0f));
        return Brush;
    }

    const FSlateBrush& InactiveTabCloseShapeBrush()
    {
        static const FSlateRoundedBoxBrush Brush(
            Palette().Header,
            FVector4(0.0f, 20.0f, 0.0f, 0.0f));
        return Brush;
    }

    const FSlateBrush& FloatingHeaderFillBrush()
    {
        // Floating tab controls provide their own visible backgrounds.
        // Keep this backing fully transparent so no square header layer can
        // appear behind their rounded top corners.
        static const FSlateRoundedBoxBrush Brush(
            FLinearColor::Transparent,
            FVector4(
                FloatingPanelTopCornerRadius,
                FloatingPanelTopCornerRadius,
                0.0f,
                0.0f));
        return Brush;
    }

    const FButtonStyle& TabMiddleButtonStyle()
    {
        static const FButtonStyle Style = []()
        {
            const FVector4 CornerRadii(0.0f, 0.0f, 0.0f, 0.0f);
            FButtonStyle Result =
                TGUiTheme::GetButtonStyle(ETGUiButtonStyle::Quiet);
            Result.SetNormal(FSlateRoundedBoxBrush(
                    FLinearColor::Transparent,
                    CornerRadii))
                .SetHovered(FSlateRoundedBoxBrush(
                    Palette().AccentSubtle,
                    CornerRadii))
                .SetPressed(FSlateRoundedBoxBrush(
                    Palette().Selection,
                    CornerRadii))
                .SetDisabled(FSlateRoundedBoxBrush(
                    Palette().Surface,
                    CornerRadii));
            return Result;
        }();
        return Style;
    }

    const FSlateBrush& TabMiddleShapeBrush()
    {
        static const FSlateRoundedBoxBrush Brush(
            FLinearColor::White,
            FVector4(0.0f, 0.0f, 0.0f, 0.0f));
        return Brush;
    }

    const FSlateBrush& AccentIndicatorBrush()
    {
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            Palette().Accent,
            1.0f);
        return Brush;
    }

    const FSlateBrush& TintableRoundedBrush()
    {
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            FLinearColor::White,
            2.0f);
        return Brush;
    }


    const FComboBoxStyle& GraphicsComboBoxStyle()
    {
        static const FComboBoxStyle Style = TGUiTheme::MakeComboBoxStyle();
        return Style;
    }

    const FTableRowStyle& GraphicsComboRowStyle()
    {
        static const FTableRowStyle Style = TGUiTheme::MakeTableRowStyle();
        return Style;
    }

    const FSlateBrush& DockPreviewBrush()
    {
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            Palette().Accent.CopyWithNewOpacity(0.20f),
            6.0f,
            Palette().AccentHover.CopyWithNewOpacity(0.75f),
            2.0f);
        return Brush;
    }

    FString NormalizeConstellationId(FString Value)
    {
        int32 ScopeSeparator = INDEX_NONE;
        if (Value.FindLastChar(TEXT(':'), ScopeSeparator))
        {
            Value = Value.Mid(ScopeSeparator + 1);
        }
        Value.ReplaceInline(TEXT("_"), TEXT(""));
        Value.ReplaceInline(TEXT(" "), TEXT(""));
        return Value.ToLower();
    }

    bool ReadPropertyAsString(
        const FProperty& Property,
        const void* Container,
        FString& OutValue)
    {
        if (const FEnumProperty* EnumProperty =
                CastField<FEnumProperty>(&Property))
        {
            const void* Value = Property.ContainerPtrToValuePtr<void>(
                Container);
            const int64 EnumValue = EnumProperty->GetUnderlyingProperty()
                ->GetSignedIntPropertyValue(Value);
            OutValue = EnumProperty->GetEnum()->GetNameStringByValue(
                EnumValue);
            return true;
        }
        if (const FByteProperty* ByteProperty =
                CastField<FByteProperty>(&Property))
        {
            const uint8 Value = ByteProperty->GetPropertyValue_InContainer(
                Container);
            OutValue = ByteProperty->Enum != nullptr
                ? ByteProperty->Enum->GetNameStringByValue(Value)
                : FString::FromInt(Value);
            return true;
        }
        if (const FNameProperty* NameProperty =
                CastField<FNameProperty>(&Property))
        {
            OutValue = NameProperty->GetPropertyValue_InContainer(Container)
                .ToString();
            return true;
        }
        if (const FStrProperty* StringProperty =
                CastField<FStrProperty>(&Property))
        {
            OutValue = StringProperty->GetPropertyValue_InContainer(Container);
            return true;
        }
        if (const FTextProperty* TextProperty =
                CastField<FTextProperty>(&Property))
        {
            OutValue = TextProperty->GetPropertyValue_InContainer(Container)
                .ToString();
            return true;
        }
        if (const FNumericProperty* NumericProperty =
                CastField<FNumericProperty>(&Property))
        {
            const void* Value = Property.ContainerPtrToValuePtr<void>(
                Container);
            OutValue = NumericProperty->IsInteger()
                ? FString::Printf(
                    TEXT("%lld"),
                    NumericProperty->GetSignedIntPropertyValue(Value))
                : FString::SanitizeFloat(
                    NumericProperty->GetFloatingPointPropertyValue(Value));
            return true;
        }
        return false;
    }

    bool IsConstellationShell(const AActor& Actor)
    {
        // The level owns one global BP_ConstellationShell. Individual
        // constellation IDs belong to the stars, lines, and labels it spawned.
        return Actor.GetClass()->GetName().Contains(TEXT("ConstellationShell"));
    }

    void CompleteActiveConstellationFade(AActor& Shell)
    {
        TArray<UTimelineComponent*> Timelines;
        Shell.GetComponents<UTimelineComponent>(Timelines);
        for (UTimelineComponent* Timeline : Timelines)
        {
            if (
                Timeline == nullptr ||
                !Timeline->GetName().Contains(
                    TEXT("ConstellationFadeInTimeline")) ||
                !Timeline->IsPlaying())
            {
                continue;
            }

            // BP_ConstellationShell has one shared timeline. Complete its
            // current target before the Blueprint reuses it for another ID.
            const float TargetTime = Timeline->IsReversing()
                ? 0.0f
                : Timeline->GetTimelineLength();
            Timeline->SetPlaybackPosition(TargetTime, false, true);
            Timeline->Stop();
        }
    }

    bool InvokeConstellationVisibilityFunction(
        UObject& Target,
        const FName FunctionName,
        const FString& ConstellationId,
        const bool bVisible)
    {
        UFunction* Function = Target.FindFunction(FunctionName);
        if (Function == nullptr)
        {
            return false;
        }

        FStructOnScope Parameters(Function);
        uint8* ParameterMemory = Parameters.GetStructMemory();
        bool bSetId = false;
        bool bSetVisible = false;
        for (TFieldIterator<FProperty> PropertyIt(Function); PropertyIt;
             ++PropertyIt)
        {
            FProperty* Property = *PropertyIt;
            if (!Property->HasAnyPropertyFlags(CPF_Parm) ||
                Property->HasAnyPropertyFlags(CPF_ReturnParm))
            {
                continue;
            }

            // BP_ConstellationShell.SetConstellationOutlineVisible expects
            // (ConstellationId: String, Visible: Bool). Populate the reflected
            // parameter buffer before dispatching the Blueprint event.
            if (FStrProperty* StringProperty =
                    CastField<FStrProperty>(Property))
            {
                StringProperty->SetPropertyValue_InContainer(
                    ParameterMemory,
                    ConstellationId);
                bSetId = true;
            }
            else if (FNameProperty* NameProperty =
                         CastField<FNameProperty>(Property))
            {
                NameProperty->SetPropertyValue_InContainer(
                    ParameterMemory,
                    FName(*ConstellationId));
                bSetId = true;
            }
            else if (FBoolProperty* BooleanProperty =
                    CastField<FBoolProperty>(Property))
            {
                BooleanProperty->SetPropertyValue_InContainer(
                    ParameterMemory,
                    bVisible);
                bSetVisible = true;
            }
        }
        if (!bSetId || !bSetVisible)
        {
            return false;
        }
        Target.ProcessEvent(Function, ParameterMemory);
        return true;
    }

    FString FormatNumber(const double Value)
    {
        if (!FMath::IsFinite(Value))
        {
            return TEXT("--");
        }
        const double Magnitude = FMath::Abs(Value);
        int32 DecimalPlaces = 3;
        if (Magnitude >= 1000000.0)
        {
            DecimalPlaces = 0;
        }
        else if (Magnitude >= 1000.0)
        {
            DecimalPlaces = 2;
        }
        else if (Magnitude >= 1.0)
        {
            DecimalPlaces = 3;
        }
        else if (Magnitude >= 0.01)
        {
            DecimalPlaces = 5;
        }
        else
        {
            DecimalPlaces = 8;
        }

        FString Result = FString::Printf(
            TEXT("%.*f"),
            DecimalPlaces,
            Value);
        while (Result.Contains(TEXT(".")) && Result.EndsWith(TEXT("0")))
        {
            Result.LeftChopInline(1);
        }
        if (Result.EndsWith(TEXT(".")))
        {
            Result.LeftChopInline(1);
        }
        return Result == TEXT("-0") ? FString(TEXT("0")) : Result;
    }

    FString FormatGraphicsScale(const double Value)
    {
        return FMath::IsFinite(Value)
            ? FString::Printf(TEXT("%.12g"), Value)
            : FString(TEXT("--"));
    }

    int32 SelectTelemetryDecimalPlaces(const double ReferenceMagnitude)
    {
        const double Magnitude = FMath::Abs(ReferenceMagnitude);
        if (Magnitude >= 1000000.0)
        {
            return 0;
        }
        if (Magnitude >= 1000.0)
        {
            return 2;
        }
        if (Magnitude >= 1.0)
        {
            return 3;
        }
        if (Magnitude >= 0.01)
        {
            return 5;
        }
        return Magnitude > 0.0 ? 8 : 3;
    }

    FString FormatTelemetryNumber(
        const double Value,
        const int32 DecimalPlaces)
    {
        if (!FMath::IsFinite(Value))
        {
            return TEXT("--");
        }
        const int32 SafeDecimalPlaces = FMath::Clamp(DecimalPlaces, 0, 12);
        const double ZeroThreshold =
            0.5 * FMath::Pow(10.0, -SafeDecimalPlaces);
        const double DisplayValue = FMath::Abs(Value) < ZeroThreshold
            ? 0.0
            : Value;
        return FString::Printf(
            TEXT("%.*f"),
            SafeDecimalPlaces,
            DisplayValue);
    }

    FString FormatResultColumnLabel(const FString& ColumnName)
    {
        if (ColumnName == TEXT("ephemeris_time_tdb_seconds_past_j2000"))
        {
            return TEXT("Ephemeris Time (TDB past J2000) [s]");
        }
        if (ColumnName == TEXT("elapsed_time_seconds"))
        {
            return TEXT("Mission Elapsed Time [s]");
        }

        struct FUnitSuffix
        {
            const TCHAR* Suffix;
            const TCHAR* Unit;
        };
        static const FUnitSuffix UnitSuffixes[] = {
            {TEXT("_radps_or_mps"), TEXT("rad/s or m/s")},
            {TEXT("_kgm2ps"), TEXT("kg*m^2/s")},
            {TEXT("_kgmps"), TEXT("kg*m/s")},
            {TEXT("_nm_or_n"), TEXT("N*m or N")},
            {TEXT("_mps2"), TEXT("m/s^2")},
            {TEXT("_kgm2"), TEXT("kg*m^2")},
            {TEXT("_kgps"), TEXT("kg/s")},
            {TEXT("_rad_or_m"), TEXT("rad or m")},
            {TEXT("_radps"), TEXT("rad/s")},
            {TEXT("_mps"), TEXT("m/s")},
            {TEXT("_pa"), TEXT("Pa")},
            {TEXT("_nm"), TEXT("N*m")},
            {TEXT("_kg"), TEXT("kg")},
            {TEXT("_rad"), TEXT("rad")},
            {TEXT("_m"), TEXT("m")},
            {TEXT("_n"), TEXT("N")}};

        FString Stem = ColumnName;
        FString Unit;
        for (const FUnitSuffix& Candidate : UnitSuffixes)
        {
            if (Stem.EndsWith(Candidate.Suffix))
            {
                Stem.LeftChopInline(FCString::Strlen(Candidate.Suffix));
                Unit = Candidate.Unit;
                break;
            }
        }

        FString ComponentSuffix;
        static const TCHAR* ComponentTokens[] = {
            TEXT("_x"), TEXT("_y"), TEXT("_z"), TEXT("_w")};
        for (const TCHAR* Token : ComponentTokens)
        {
            if (Stem.EndsWith(Token))
            {
                ComponentSuffix = FString(Token + 1).ToUpper();
                Stem.LeftChopInline(FCString::Strlen(Token));
                break;
            }
        }

        TArray<FString> Words;
        Stem.ParseIntoArray(Words, TEXT("_"), true);
        const TArray<FTGCelestialCatalogEntry> CelestialCatalog =
            UTGCelestialCatalogLibrary::GetCelestialCatalog();
        for (int32 Index = 0; Index < Words.Num(); ++Index)
        {
            FString& Word = Words[Index];
            if (const FTGCelestialCatalogEntry* Entry =
                    CelestialCatalog.FindByPredicate(
                        [&Word](const FTGCelestialCatalogEntry& Candidate)
                        {
                            return Candidate.SpiceTarget.Equals(
                                Word,
                                ESearchCase::IgnoreCase);
                        }))
            {
                Word = Entry->DisplayName.ToString();
                continue;
            }

            const FString Lower = Word.ToLower();
            if (Lower == TEXT("icrf") || Lower == TEXT("tdb") ||
                Lower == TEXT("srp") || Lower == TEXT("cm") ||
                Lower == TEXT("csv") || Lower == TEXT("utc") ||
                Lower == TEXT("dof") || Lower == TEXT("et"))
            {
                Word = Lower.ToUpper();
            }
            else if (
                Lower == TEXT("xx") || Lower == TEXT("xy") ||
                Lower == TEXT("xz") || Lower == TEXT("yx") ||
                Lower == TEXT("yy") || Lower == TEXT("yz") ||
                Lower == TEXT("zx") || Lower == TEXT("zy") ||
                Lower == TEXT("zz"))
            {
                Word = Lower.ToUpper();
            }
            else if (
                Index > 0 &&
                (Lower == TEXT("of") || Lower == TEXT("to") ||
                 Lower == TEXT("about") || Lower == TEXT("or") ||
                 Lower == TEXT("and")))
            {
                Word = Lower;
            }
            else if (!Word.IsEmpty())
            {
                Word = Lower;
                Word[0] = FChar::ToUpper(Word[0]);
            }
        }

        FString Label = FString::Join(Words, TEXT(" "));
        Label.ReplaceInline(TEXT("Force Gravity"), TEXT("Gravity Force"));
        Label.ReplaceInline(TEXT("Force Thrust"), TEXT("Thrust Force"));
        Label.ReplaceInline(TEXT("Force SRP"), TEXT("SRP Force"));
        Label.ReplaceInline(
            TEXT("Force Aerodynamic"),
            TEXT("Aerodynamic Force"));
        Label.ReplaceInline(TEXT("Force Total"), TEXT("Total Force"));
        Label.ReplaceInline(TEXT("Torque Gravity"), TEXT("Gravity Torque"));
        Label.ReplaceInline(TEXT("Torque Thrust"), TEXT("Thrust Torque"));
        Label.ReplaceInline(TEXT("Torque SRP"), TEXT("SRP Torque"));
        Label.ReplaceInline(
            TEXT("Torque Aerodynamic"),
            TEXT("Aerodynamic Torque"));
        Label.ReplaceInline(TEXT("Torque Total"), TEXT("Total Torque"));
        if (!ComponentSuffix.IsEmpty())
        {
            Label += TEXT(" - ") + ComponentSuffix;
        }
        if (!Unit.IsEmpty())
        {
            Label += TEXT(" [") + Unit + TEXT("]");
        }
        return Label;
    }

    FString FormatFixedThree(const double Value)
    {
        if (!FMath::IsFinite(Value))
        {
            return TEXT("--");
        }
        const double CleanValue = FMath::Abs(Value) < 0.0005 ? 0.0 : Value;
        return FString::Printf(TEXT("%.3f"), CleanValue);
    }

    FString FormatEngineeringMagnitude(
        const double PhysicalMagnitude,
        const FString& BaseUnit)
    {
        const double Magnitude = FMath::Max(0.0, PhysicalMagnitude);
        if (Magnitude >= 1000.0)
        {
            return FormatFixedThree(Magnitude / 1000.0) +
                TEXT(" k") + BaseUnit;
        }
        if (Magnitude >= 1.0)
        {
            return FormatFixedThree(Magnitude) + TEXT(" ") + BaseUnit;
        }
        if (Magnitude >= 1.0e-3)
        {
            return FormatFixedThree(Magnitude * 1.0e3) +
                TEXT(" m") + BaseUnit;
        }
        if (Magnitude >= 1.0e-6)
        {
            return FormatFixedThree(Magnitude * 1.0e6) +
                TEXT(" u") + BaseUnit;
        }
        if (Magnitude >= 1.0e-9)
        {
            return FormatFixedThree(Magnitude * 1.0e9) +
                TEXT(" n") + BaseUnit;
        }
        if (Magnitude >= 1.0e-12)
        {
            return FormatFixedThree(Magnitude * 1.0e12) +
                TEXT(" p") + BaseUnit;
        }
        if (Magnitude > 0.0)
        {
            return FString::Printf(TEXT("%.3e %s"), Magnitude, *BaseUnit);
        }
        return FormatFixedThree(0.0) + TEXT(" ") + BaseUnit;
    }

    FString FormatVectorMagnitude(
        const FTGVisualizationArrowInfo& Info)
    {
        if (Info.Quantity == ETGVisualizationArrowQuantity::Force ||
            Info.Quantity == ETGVisualizationArrowQuantity::Torque ||
            Info.Quantity == ETGVisualizationArrowQuantity::AngularVelocity)
        {
            return FormatEngineeringMagnitude(
                Info.CurrentMagnitude,
                Info.MagnitudeUnit);
        }

        return FormatFixedThree(Info.CurrentMagnitude) +
            (Info.MagnitudeUnit.IsEmpty()
                ? FString{}
                : TEXT(" ") + Info.MagnitudeUnit);
    }

    FString FormatVisualAmplification(const double Amplification)
    {
        const double Value = FMath::Max(1.0, Amplification);
        auto FormatCompact = [](const double ScaledValue)
        {
            FString Result = FString::Printf(TEXT("%.1f"), ScaledValue);
            if (Result.EndsWith(TEXT(".0")))
            {
                Result.LeftChopInline(2);
            }
            return Result;
        };
        if (Value >= 1.0e9)
        {
            return FormatCompact(Value / 1.0e9) + TEXT("B");
        }
        if (Value >= 1.0e6)
        {
            return FormatCompact(Value / 1.0e6) + TEXT("M");
        }
        if (Value >= 1.0e3)
        {
            return FormatCompact(Value / 1.0e3) + TEXT("k");
        }
        return FormatCompact(Value);
    }

    FLinearColor ResolveTelemetryAccent(
        const FTGVisualizationTelemetryItem& Item)
    {
        if (Item.Group == TEXT("State (ICRF)"))
        {
            return FLinearColor(0.26f, 0.52f, 0.96f);
        }
        if (Item.Group == TEXT("Closest Body"))
        {
            return FLinearColor(0.24f, 0.78f, 0.52f);
        }
        if (Item.Group == TEXT("Environment"))
        {
            return FLinearColor(0.96f, 0.66f, 0.16f);
        }
        if (Item.Group == TEXT("Variable Mass"))
        {
            return FLinearColor(0.72f, 0.42f, 0.90f);
        }
        return Palette().Accent;
    }

    DECLARE_DELEGATE_TwoParams(
        FOnTGRangeSliderChanged,
        float,
        float);

    /** Runtime-safe two-handle range slider used by visualization options. */
    class STGRangeSlider final : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(STGRangeSlider)
            : _LowerValue(0.0f)
            , _UpperValue(1.0f)
        {}
            SLATE_ATTRIBUTE(float, LowerValue)
            SLATE_ATTRIBUTE(float, UpperValue)
            SLATE_EVENT(FOnTGRangeSliderChanged, OnRangeChanged)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            LowerValue = InArgs._LowerValue;
            UpperValue = InArgs._UpperValue;
            OnRangeChanged = InArgs._OnRangeChanged;
        }

        virtual FVector2D ComputeDesiredSize(float) const override
        {
            return FVector2D(220.0f, 24.0f);
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
            const FVector2D Size = AllottedGeometry.GetLocalSize();
            const float LowerX = ValueToLocalX(GetLowerValue(), Size.X);
            const float UpperX = ValueToLocalX(GetUpperValue(), Size.X);
            const float TrackY = (Size.Y - TrackThickness) * 0.5f;
            const ESlateDrawEffect DrawEffects =
                ShouldBeEnabled(bParentEnabled)
                    ? ESlateDrawEffect::None
                    : ESlateDrawEffect::DisabledEffect;
            const FLinearColor WidgetTint =
                InWidgetStyle.GetColorAndOpacityTint();

            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(
                        FMath::Max(1.0f, Size.X - HandleWidth),
                        TrackThickness),
                    FSlateLayoutTransform(FVector2D(
                        HandleWidth * 0.5f,
                        TrackY))),
                &TintableRoundedBrush(),
                DrawEffects,
                Palette().Border * WidgetTint);

            if (UpperX > LowerX)
            {
                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    LayerId + 1,
                    AllottedGeometry.ToPaintGeometry(
                        FVector2D(UpperX - LowerX, TrackThickness),
                        FSlateLayoutTransform(FVector2D(
                            LowerX,
                            TrackY))),
                    &TintableRoundedBrush(),
                    DrawEffects,
                    Palette().Accent * WidgetTint);
            }

            DrawHandle(
                OutDrawElements,
                AllottedGeometry,
                LayerId + 2,
                LowerX,
                EHandle::Lower,
                DrawEffects,
                WidgetTint);
            DrawHandle(
                OutDrawElements,
                AllottedGeometry,
                LayerId + 2,
                UpperX,
                EHandle::Upper,
                DrawEffects,
                WidgetTint);
            return LayerId + 2;
        }

        virtual FReply OnMouseButtonDown(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            if (
                !IsEnabled() ||
                MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
            {
                return FReply::Unhandled();
            }

            const float LocalX = MyGeometry.AbsoluteToLocal(
                MouseEvent.GetScreenSpacePosition()).X;
            ActiveHandle = FindClosestHandle(
                LocalX,
                MyGeometry.GetLocalSize().X);
            CommitHandleValue(
                LocalXToValue(LocalX, MyGeometry.GetLocalSize().X));
            Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled().CaptureMouse(AsShared());
        }

        virtual FReply OnMouseMove(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            const float LocalX = MyGeometry.AbsoluteToLocal(
                MouseEvent.GetScreenSpacePosition()).X;
            if (HasMouseCapture() && ActiveHandle != EHandle::None)
            {
                CommitHandleValue(
                    LocalXToValue(LocalX, MyGeometry.GetLocalSize().X));
                return FReply::Handled();
            }

            const EHandle NewHoveredHandle = FindHoveredHandle(
                LocalX,
                MyGeometry.GetLocalSize().X);
            if (NewHoveredHandle != HoveredHandle)
            {
                HoveredHandle = NewHoveredHandle;
                Invalidate(EInvalidateWidgetReason::Paint);
            }
            return FReply::Unhandled();
        }

        virtual FReply OnMouseButtonUp(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            if (
                MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton ||
                !HasMouseCapture())
            {
                return FReply::Unhandled();
            }

            ActiveHandle = EHandle::None;
            Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled().ReleaseMouseCapture();
        }

        virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
        {
            SLeafWidget::OnMouseLeave(MouseEvent);
            if (!HasMouseCapture() && HoveredHandle != EHandle::None)
            {
                HoveredHandle = EHandle::None;
                Invalidate(EInvalidateWidgetReason::Paint);
            }
        }

        virtual void OnMouseCaptureLost(
            const FCaptureLostEvent& CaptureLostEvent) override
        {
            SLeafWidget::OnMouseCaptureLost(CaptureLostEvent);
            ActiveHandle = EHandle::None;
            Invalidate(EInvalidateWidgetReason::Paint);
        }

    private:
        enum class EHandle : uint8
        {
            None,
            Lower,
            Upper
        };

        static constexpr float HandleWidth = 14.0f;
        static constexpr float HandleHeight = 22.0f;
        static constexpr float TrackThickness = 4.0f;

        TAttribute<float> LowerValue;
        TAttribute<float> UpperValue;
        FOnTGRangeSliderChanged OnRangeChanged;
        EHandle ActiveHandle = EHandle::None;
        EHandle HoveredHandle = EHandle::None;

        float GetLowerValue() const
        {
            return FMath::Clamp(
                FMath::Min(
                    LowerValue.Get(0.0f),
                    UpperValue.Get(1.0f)),
                0.0f,
                1.0f);
        }

        float GetUpperValue() const
        {
            return FMath::Clamp(
                FMath::Max(
                    LowerValue.Get(0.0f),
                    UpperValue.Get(1.0f)),
                0.0f,
                1.0f);
        }

        static float ValueToLocalX(
            const float Value,
            const float Width)
        {
            const float HalfHandle = HandleWidth * 0.5f;
            return HalfHandle +
                FMath::Clamp(Value, 0.0f, 1.0f) *
                FMath::Max(1.0f, Width - HandleWidth);
        }

        static float LocalXToValue(
            const float LocalX,
            const float Width)
        {
            const float HalfHandle = HandleWidth * 0.5f;
            return FMath::Clamp(
                (LocalX - HalfHandle) /
                    FMath::Max(1.0f, Width - HandleWidth),
                0.0f,
                1.0f);
        }

        EHandle FindClosestHandle(
            const float LocalX,
            const float Width) const
        {
            const float LowerX = ValueToLocalX(GetLowerValue(), Width);
            const float UpperX = ValueToLocalX(GetUpperValue(), Width);
            const float LowerDistance = FMath::Abs(LocalX - LowerX);
            const float UpperDistance = FMath::Abs(LocalX - UpperX);
            if (FMath::IsNearlyEqual(LowerDistance, UpperDistance))
            {
                return LocalX <= LowerX
                    ? EHandle::Lower
                    : EHandle::Upper;
            }
            return LowerDistance < UpperDistance
                ? EHandle::Lower
                : EHandle::Upper;
        }

        EHandle FindHoveredHandle(
            const float LocalX,
            const float Width) const
        {
            const EHandle Closest = FindClosestHandle(LocalX, Width);
            const float HandleX = Closest == EHandle::Lower
                ? ValueToLocalX(GetLowerValue(), Width)
                : ValueToLocalX(GetUpperValue(), Width);
            return FMath::Abs(LocalX - HandleX) <= HandleWidth
                ? Closest
                : EHandle::None;
        }

        void CommitHandleValue(const float Value)
        {
            float NewLower = GetLowerValue();
            float NewUpper = GetUpperValue();
            if (ActiveHandle == EHandle::Lower)
            {
                NewLower = FMath::Min(Value, NewUpper);
            }
            else if (ActiveHandle == EHandle::Upper)
            {
                NewUpper = FMath::Max(Value, NewLower);
            }
            OnRangeChanged.ExecuteIfBound(NewLower, NewUpper);
        }

        void DrawHandle(
            FSlateWindowElementList& OutDrawElements,
            const FGeometry& Geometry,
            const int32 Layer,
            const float CenterX,
            const EHandle Handle,
            const ESlateDrawEffect DrawEffects,
            const FLinearColor& WidgetTint) const
        {
            const bool bHighlighted =
                Handle == ActiveHandle || Handle == HoveredHandle;
            const FVector2D Size = Geometry.GetLocalSize();
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                Layer,
                Geometry.ToPaintGeometry(
                    FVector2D(HandleWidth, HandleHeight),
                    FSlateLayoutTransform(FVector2D(
                        CenterX - HandleWidth * 0.5f,
                        (Size.Y - HandleHeight) * 0.5f))),
                &TintableRoundedBrush(),
                DrawEffects,
                (bHighlighted
                    ? Palette().Accent
                    : Palette().TextPrimary) * WidgetTint);
        }
    };

    enum class EGraphicsAxis : uint8
    {
        X,
        Y,
        Z
    };

    /** Native raw-unit 2D/3D plot hosted by the dockable Graphics panel. */
    class STGSimulationDataPlot final : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(STGSimulationDataPlot) {}
            SLATE_ARGUMENT(UTGVisualizationDockWorkspaceWidget*, Owner)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            Owner = InArgs._Owner;
            SetClipping(EWidgetClipping::ClipToBounds);
        }

        void SetPlotData(
            const TArray<FVector>& InSamples,
            const FString& InXColumn,
            const FString& InYColumn,
            const FString& InZColumn,
            const double InXScale,
            const double InYScale,
            const double InZScale,
            const double InStartNormalized,
            const double InEndNormalized)
        {
            Samples = InSamples;
            XColumn = InXColumn;
            YColumn = InYColumn;
            ZColumn = InZColumn;
            bHasZ = !ZColumn.IsEmpty();
            XScale = SafeScale(InXScale);
            YScale = SafeScale(InYScale);
            ZScale = SafeScale(InZScale);
            StartNormalized = FMath::Clamp(
                FMath::Min(InStartNormalized, InEndNormalized),
                0.0,
                1.0);
            EndNormalized = FMath::Clamp(
                FMath::Max(InStartNormalized, InEndNormalized),
                0.0,
                1.0);

            RawMinimum = FVector(
                TNumericLimits<double>::Max(),
                TNumericLimits<double>::Max(),
                TNumericLimits<double>::Max());
            RawMaximum = FVector(
                TNumericLimits<double>::Lowest(),
                TNumericLimits<double>::Lowest(),
                TNumericLimits<double>::Lowest());
            for (const FVector& Sample : Samples)
            {
                RawMinimum = RawMinimum.ComponentMin(Sample);
                RawMaximum = RawMaximum.ComponentMax(Sample);
            }
            if (Samples.IsEmpty())
            {
                RawMinimum = FVector::ZeroVector;
                RawMaximum = FVector::ZeroVector;
            }
            Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
        }

        void RefreshCurrentMarker()
        {
            Invalidate(EInvalidateWidgetReason::Paint);
        }

        virtual FVector2D ComputeDesiredSize(float) const override
        {
            return FVector2D(620.0f, 340.0f);
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
            const FVector2D Size = AllottedGeometry.GetLocalSize();
            if (Size.X <= 4.0 || Size.Y <= 4.0)
            {
                return LayerId;
            }

            const ESlateDrawEffect DrawEffects =
                ShouldBeEnabled(bParentEnabled)
                    ? ESlateDrawEffect::None
                    : ESlateDrawEffect::DisabledEffect;
            const FLinearColor WidgetTint =
                InWidgetStyle.GetColorAndOpacityTint();
            const FSlateFontInfo CaptionFont =
                TGUiTheme::GetSlateFont(ETGUiTextStyle::Caption);
            const FSlateFontInfo LabelFont =
                TGUiTheme::GetSlateFont(ETGUiTextStyle::FieldLabel);
            const TSharedRef<FSlateFontMeasure> FontMeasure =
                FSlateApplication::Get()
                    .GetRenderer()
                    ->GetFontMeasureService();

            // Use an explicitly tinted neutral brush. This avoids the white
            // fallback produced by directly painting a pre-colored box brush.
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId,
                AllottedGeometry.ToPaintGeometry(),
                &TintableRoundedBrush(),
                DrawEffects,
                Palette().Panel * WidgetTint);

            if (Samples.IsEmpty() || XColumn.IsEmpty() || YColumn.IsEmpty())
            {
                FSlateDrawElement::MakeText(
                    OutDrawElements,
                    LayerId + 1,
                    AllottedGeometry.ToOffsetPaintGeometry(FVector2D(
                        18.0f,
                        Size.Y * 0.5f - 8.0f)),
                    TEXT("Select X and Y columns to plot result data"),
                    LabelFont,
                    DrawEffects,
                    Palette().TextSecondary * WidgetTint);
                return LayerId + 1;
            }

            constexpr float LeftMargin = 18.0f;
            constexpr float RightMargin = 18.0f;
            constexpr float TopMargin = 72.0f;
            constexpr float BottomMargin = 26.0f;
            const FVector2D PlotMinimum(LeftMargin, TopMargin);
            const FVector2D PlotMaximum(
                FMath::Max(LeftMargin + 1.0f, Size.X - RightMargin),
                FMath::Max(TopMargin + 1.0f, Size.Y - BottomMargin));
            const FVector2D PlotSize = PlotMaximum - PlotMinimum;
            const FVector2D PlotCenter = (PlotMinimum + PlotMaximum) * 0.5;

            const FVector ScaledMinimum(
                FMath::Min(RawMinimum.X * XScale, RawMaximum.X * XScale),
                FMath::Min(RawMinimum.Y * YScale, RawMaximum.Y * YScale),
                bHasZ
                    ? FMath::Min(
                        RawMinimum.Z * ZScale,
                        RawMaximum.Z * ZScale)
                    : 0.0);
            const FVector ScaledMaximum(
                FMath::Max(RawMinimum.X * XScale, RawMaximum.X * XScale),
                FMath::Max(RawMinimum.Y * YScale, RawMaximum.Y * YScale),
                bHasZ
                    ? FMath::Max(
                        RawMinimum.Z * ZScale,
                        RawMaximum.Z * ZScale)
                    : 0.0);
            const FVector ScaledCenter =
                (ScaledMinimum + ScaledMaximum) * 0.5;
            const auto StableSpan = [](const double Span, const double Center)
            {
                return Span > UE_DOUBLE_SMALL_NUMBER
                    ? Span
                    : FMath::Max(FMath::Abs(Center) * 0.1, 1.0);
            };
            const double SpanX = StableSpan(
                ScaledMaximum.X - ScaledMinimum.X,
                ScaledCenter.X);
            const double SpanY = StableSpan(
                ScaledMaximum.Y - ScaledMinimum.Y,
                ScaledCenter.Y);
            const double SpanZ = bHasZ
                ? StableSpan(
                    ScaledMaximum.Z - ScaledMinimum.Z,
                    ScaledCenter.Z)
                : 0.0;
            const double MaximumSpan = FMath::Max(
                SpanX,
                FMath::Max(SpanY, SpanZ));
            const double UnitsToPixels =
                FMath::Min(PlotSize.X, PlotSize.Y) /
                FMath::Max(MaximumSpan * 1.16, UE_DOUBLE_SMALL_NUMBER) *
                ViewZoom;

            const FQuat CurrentViewRotation = ViewRotation;
            const auto ProjectScaled = [
                this,
                &CurrentViewRotation,
                &ScaledCenter,
                &PlotCenter,
                UnitsToPixels](const FVector& ScaledValue)
            {
                const FVector Relative = ScaledValue - ScaledCenter;
                const FVector Projected = bHasZ
                    ? CurrentViewRotation.RotateVector(Relative)
                    : Relative;
                return FVector2f(
                    static_cast<float>(
                        PlotCenter.X + ViewPanPixels.X +
                        Projected.X * UnitsToPixels),
                    static_cast<float>(
                        PlotCenter.Y + ViewPanPixels.Y -
                        Projected.Y * UnitsToPixels));
            };
            const auto ScaleSample = [this](const FVector& Sample)
            {
                return FVector(
                    Sample.X * XScale,
                    Sample.Y * YScale,
                    bHasZ ? Sample.Z * ZScale : 0.0);
            };

            int32 DrawLayer = LayerId + 1;
            const FPaintGeometry PlotPaintGeometry =
                AllottedGeometry.ToPaintGeometry(
                    PlotSize,
                    FSlateLayoutTransform(PlotMinimum));
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                DrawLayer,
                PlotPaintGeometry,
                &TintableRoundedBrush(),
                DrawEffects,
                Palette().Canvas * WidgetTint);
            ++DrawLayer;

            // Data, axes, ticks, and the live marker belong to the inner graph.
            // A dedicated clip prevents zoomed geometry from entering headers,
            // controls, or the surrounding panel padding.
            OutDrawElements.PushClip(FSlateClippingZone(PlotPaintGeometry));
            const FLinearColor GridColor =
                Palette().BorderStrong.CopyWithNewOpacity(0.17f) * WidgetTint;
            for (int32 GridIndex = 0; GridIndex <= 4; ++GridIndex)
            {
                const float Fraction = static_cast<float>(GridIndex) / 4.0f;
                const float GridX = FMath::Lerp(
                    static_cast<float>(PlotMinimum.X),
                    static_cast<float>(PlotMaximum.X),
                    Fraction);
                const float GridY = FMath::Lerp(
                    static_cast<float>(PlotMinimum.Y),
                    static_cast<float>(PlotMaximum.Y),
                    Fraction);
                DrawLine(
                    OutDrawElements,
                    AllottedGeometry,
                    DrawLayer,
                    FVector2f(GridX, static_cast<float>(PlotMinimum.Y)),
                    FVector2f(GridX, static_cast<float>(PlotMaximum.Y)),
                    GridColor,
                    DrawEffects,
                    1.0f);
                DrawLine(
                    OutDrawElements,
                    AllottedGeometry,
                    DrawLayer,
                    FVector2f(static_cast<float>(PlotMinimum.X), GridY),
                    FVector2f(static_cast<float>(PlotMaximum.X), GridY),
                    GridColor,
                    DrawEffects,
                    1.0f);
            }

            const FLinearColor XColor =
                Palette().Error.CopyWithNewOpacity(0.90f);
            const FLinearColor YColor =
                Palette().Success.CopyWithNewOpacity(0.90f);
            const FLinearColor ZColor =
                Palette().Info.CopyWithNewOpacity(0.90f);
            const FVector AxisOrigin(
                RawMinimum.X * XScale,
                RawMinimum.Y * YScale,
                bHasZ ? RawMinimum.Z * ZScale : 0.0);
            const auto DrawAxis = [
                &OutDrawElements,
                &AllottedGeometry,
                DrawLayer,
                DrawEffects,
                WidgetTint,
                &ProjectScaled,
                &AxisOrigin,
                &CaptionFont,
                &FontMeasure](
                    const int32 AxisIndex,
                    const FLinearColor& Color,
                    const double RawAxisMinimum,
                    const double RawAxisMaximum,
                    const double AxisScale,
                    const float TickLabelSide)
            {
                FVector AxisStart = AxisOrigin;
                FVector AxisEnd = AxisOrigin;
                AxisStart[AxisIndex] = RawAxisMinimum * AxisScale;
                AxisEnd[AxisIndex] = RawAxisMaximum * AxisScale;
                const FVector2f Negative = ProjectScaled(AxisStart);
                const FVector2f Positive = ProjectScaled(AxisEnd);
                DrawLine(
                    OutDrawElements,
                    AllottedGeometry,
                    DrawLayer + 1,
                    Negative,
                    Positive,
                    Color * WidgetTint,
                    DrawEffects,
                    1.35f);

                const FVector2f AxisVector = Positive - Negative;
                const float AxisScreenLength = AxisVector.Size();
                if (AxisScreenLength > 1.0f)
                {
                    const FVector2f TickNormal(
                        -AxisVector.Y / AxisScreenLength,
                        AxisVector.X / AxisScreenLength);
                    const double RawRange =
                        RawAxisMaximum - RawAxisMinimum;
                    const double TickStep = ComputeNiceTickStep(
                        RawRange,
                        AxisScreenLength);
                    const int32 DisplayExponent = ComputeTickDisplayExponent(
                        RawAxisMinimum,
                        RawAxisMaximum);
                    const double DisplayFactor = FMath::Pow(
                        10.0,
                        static_cast<double>(DisplayExponent));
                    const int32 DecimalPlaces = ComputeTickDecimalPlaces(
                        TickStep,
                        DisplayFactor);
                    const auto DrawTick = [
                        &OutDrawElements,
                        &AllottedGeometry,
                        DrawLayer,
                        DrawEffects,
                        WidgetTint,
                        &ProjectScaled,
                        &AxisOrigin,
                        &TickNormal,
                        AxisIndex,
                        AxisScale,
                        TickLabelSide,
                        DisplayFactor,
                        DecimalPlaces,
                        &Color,
                        &CaptionFont,
                        &FontMeasure](const double RawTickValue)
                    {
                        FVector TickPoint = AxisOrigin;
                        TickPoint[AxisIndex] = RawTickValue * AxisScale;
                        const FVector2f TickPosition = ProjectScaled(TickPoint);
                        DrawLine(
                            OutDrawElements,
                            AllottedGeometry,
                            DrawLayer + 1,
                            TickPosition - TickNormal * 3.5f,
                            TickPosition + TickNormal * 3.5f,
                            Color * WidgetTint,
                            DrawEffects,
                            1.0f);
                        const FString TickText = FormatTickValue(
                            RawTickValue,
                            DisplayFactor,
                            DecimalPlaces);
                        const FVector2D MeasuredText = FontMeasure->Measure(
                            TickText,
                            CaptionFont);
                        const FVector2f TextSize(
                            static_cast<float>(MeasuredText.X),
                            static_cast<float>(MeasuredText.Y));
                        const FVector2f LabelNormal =
                            TickNormal * TickLabelSide;
                        const float HalfExtentAlongNormal =
                            FMath::Abs(LabelNormal.X) * TextSize.X * 0.5f +
                            FMath::Abs(LabelNormal.Y) * TextSize.Y * 0.5f;
                        constexpr float TickLabelPadding = 12.0f;
                        const FVector2f LabelCenter = TickPosition +
                            LabelNormal *
                                (TickLabelPadding + HalfExtentAlongNormal);
                        const FVector2f LabelPosition =
                            LabelCenter - TextSize * 0.5f;
                        FSlateDrawElement::MakeText(
                            OutDrawElements,
                            DrawLayer + 2,
                            AllottedGeometry.ToOffsetPaintGeometry(
                                FVector2D(
                                    LabelPosition.X,
                                    LabelPosition.Y)),
                            TickText,
                            CaptionFont,
                            DrawEffects,
                            Color.CopyWithNewOpacity(0.82f) * WidgetTint);
                    };

                    DrawTick(RawAxisMinimum);
                    if (TickStep > 0.0 && RawRange > 0.0)
                    {
                        const double Tolerance = FMath::Max(
                            RawRange * 1.0e-12,
                            TickStep * 1.0e-9);
                        const double FirstInteriorTick =
                            FMath::CeilToDouble(
                                (RawAxisMinimum + Tolerance) / TickStep) *
                            TickStep;
                        constexpr int32 MaximumTickCount = 256;
                        for (int32 TickIndex = 0;
                             TickIndex < MaximumTickCount;
                             ++TickIndex)
                        {
                            const double TickValue = FirstInteriorTick +
                                static_cast<double>(TickIndex) * TickStep;
                            if (TickValue >= RawAxisMaximum - Tolerance)
                            {
                                break;
                            }

                            const float DistanceFromMinimum =
                                static_cast<float>(
                                    (TickValue - RawAxisMinimum) / RawRange) *
                                AxisScreenLength;
                            if (DistanceFromMinimum >= 38.0f &&
                                AxisScreenLength - DistanceFromMinimum >= 38.0f)
                            {
                                DrawTick(TickValue);
                            }
                        }
                    }
                    if (RawRange > 0.0)
                    {
                        DrawTick(RawAxisMaximum);
                    }
                }
            };
            DrawAxis(
                0,
                XColor,
                RawMinimum.X,
                RawMaximum.X,
                XScale,
                1.0f);
            DrawAxis(
                1,
                YColor,
                RawMinimum.Y,
                RawMaximum.Y,
                YScale,
                -1.0f);
            if (bHasZ)
            {
                DrawAxis(
                    2,
                    ZColor,
                    RawMinimum.Z,
                    RawMaximum.Z,
                    ZScale,
                    1.0f);
            }
            DrawLayer += 3;

            TArray<FVector2f> ScreenSamples;
            ScreenSamples.Reserve(Samples.Num());
            for (const FVector& Sample : Samples)
            {
                ScreenSamples.Add(ProjectScaled(ScaleSample(Sample)));
            }
            if (ScreenSamples.Num() >= 2)
            {
                FSlateDrawElement::MakeLines(
                    OutDrawElements,
                    DrawLayer,
                    AllottedGeometry.ToPaintGeometry(),
                    ScreenSamples,
                    DrawEffects,
                    Palette().Accent.CopyWithNewOpacity(0.34f) *
                        WidgetTint,
                    true,
                    6.0f);
                FSlateDrawElement::MakeLines(
                    OutDrawElements,
                    DrawLayer + 1,
                    AllottedGeometry.ToPaintGeometry(),
                    ScreenSamples,
                    DrawEffects,
                    Palette().AccentHover * WidgetTint,
                    true,
                    2.6f);
            }
            DrawLayer += 2;

            FVector CurrentPoint;
            const UTGVisualizationDockWorkspaceWidget* OwnerWidget =
                Owner.Get();
            const ATGSimulationPlaybackActor* Playback =
                OwnerWidget != nullptr
                    ? OwnerWidget->GetPlaybackActor()
                    : nullptr;
            const double CurrentNormalized = Playback != nullptr
                ? Playback->GetNormalizedTime()
                : -1.0;
            if (
                Playback != nullptr &&
                CurrentNormalized >= StartNormalized - 1.0e-9 &&
                CurrentNormalized <= EndNormalized + 1.0e-9 &&
                Playback->EvaluateResultNumericPlotPoint(
                    XColumn,
                    YColumn,
                    ZColumn,
                    CurrentPoint))
            {
                const FVector2f MarkerPosition = ProjectScaled(
                    ScaleSample(CurrentPoint));
                DrawPointMarker(
                    OutDrawElements,
                    AllottedGeometry,
                    DrawLayer + 1,
                    MarkerPosition,
                    Palette().Warning,
                    DrawEffects);
                const FString MarkerText = bHasZ
                    ? FString::Printf(
                        TEXT("(%s, %s, %s)"),
                        *FormatAxisDisplayValue(
                            CurrentPoint.X,
                            RawMinimum.X,
                            RawMaximum.X),
                        *FormatAxisDisplayValue(
                            CurrentPoint.Y,
                            RawMinimum.Y,
                            RawMaximum.Y),
                        *FormatAxisDisplayValue(
                            CurrentPoint.Z,
                            RawMinimum.Z,
                            RawMaximum.Z))
                    : FString::Printf(
                        TEXT("(%s, %s)"),
                        *FormatAxisDisplayValue(
                            CurrentPoint.X,
                            RawMinimum.X,
                            RawMaximum.X),
                        *FormatAxisDisplayValue(
                            CurrentPoint.Y,
                            RawMinimum.Y,
                            RawMaximum.Y));
                FSlateDrawElement::MakeText(
                    OutDrawElements,
                    DrawLayer + 2,
                    AllottedGeometry.ToOffsetPaintGeometry(FVector2D(
                        MarkerPosition.X + 8.0f,
                        MarkerPosition.Y - 9.0f)),
                    MarkerText,
                    CaptionFont,
                    DrawEffects,
                    Palette().TextPrimary * WidgetTint);
            }

            OutDrawElements.PopClip();

            DrawAxisSummary(
                OutDrawElements,
                AllottedGeometry,
                DrawLayer + 3,
                8.0f,
                TEXT("X"),
                XColumn,
                RawMinimum.X,
                RawMaximum.X,
                XScale,
                XColor,
                CaptionFont,
                DrawEffects,
                WidgetTint);
            DrawAxisSummary(
                OutDrawElements,
                AllottedGeometry,
                DrawLayer + 3,
                28.0f,
                TEXT("Y"),
                YColumn,
                RawMinimum.Y,
                RawMaximum.Y,
                YScale,
                YColor,
                CaptionFont,
                DrawEffects,
                WidgetTint);
            if (bHasZ)
            {
                DrawAxisSummary(
                    OutDrawElements,
                    AllottedGeometry,
                    DrawLayer + 3,
                    48.0f,
                    TEXT("Z"),
                    ZColumn,
                    RawMinimum.Z,
                    RawMaximum.Z,
                    ZScale,
                    ZColor,
                    CaptionFont,
                    DrawEffects,
                    WidgetTint);
            }

            return DrawLayer + 3;
        }

        virtual FReply OnMouseButtonDown(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            const FKey Button = MouseEvent.GetEffectingButton();
            if (Button == EKeys::MiddleMouseButton)
            {
                bPanning = true;
                return FReply::Handled().CaptureMouse(AsShared());
            }
            if (bHasZ && Button == EKeys::LeftMouseButton)
            {
                bRotating = true;
                return FReply::Handled().CaptureMouse(AsShared());
            }
            return FReply::Unhandled();
        }

        virtual FReply OnMouseMove(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            if ((!bRotating && !bPanning) || !HasMouseCapture())
            {
                return FReply::Unhandled();
            }
            const FVector2D Delta = MouseEvent.GetCursorDelta();
            if (bPanning)
            {
                ViewPanPixels += Delta;
            }
            else
            {
                constexpr double RadiansPerPixel =
                    0.45 * UE_DOUBLE_PI / 180.0;
                const FQuat HorizontalOrbit(
                    FVector::YAxisVector,
                    -Delta.X * RadiansPerPixel);
                const FQuat VerticalOrbit(
                    FVector::XAxisVector,
                    -Delta.Y * RadiansPerPixel);
                ViewRotation = (
                    VerticalOrbit * HorizontalOrbit * ViewRotation)
                    .GetNormalized();
            }
            Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled();
        }

        virtual FReply OnMouseButtonUp(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            if (!HasMouseCapture())
            {
                return FReply::Unhandled();
            }
            const FKey Button = MouseEvent.GetEffectingButton();
            if (Button == EKeys::MiddleMouseButton && bPanning)
            {
                bPanning = false;
                return FReply::Handled().ReleaseMouseCapture();
            }
            if (Button == EKeys::LeftMouseButton && bRotating)
            {
                bRotating = false;
                return FReply::Handled().ReleaseMouseCapture();
            }
            return FReply::Unhandled();
        }

        virtual void OnMouseCaptureLost(
            const FCaptureLostEvent& CaptureLostEvent) override
        {
            SLeafWidget::OnMouseCaptureLost(CaptureLostEvent);
            bRotating = false;
            bPanning = false;
        }

        virtual FReply OnMouseWheel(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            const double PreviousZoom = ViewZoom;
            ViewZoom = FMath::Clamp(
                PreviousZoom * FMath::Pow(
                    1.25,
                    MouseEvent.GetWheelDelta()),
                0.20,
                8.0);

            // Keep the plotted point under the cursor stationary. The graph has
            // an asymmetric header margin, so use the inner canvas center rather
            // than the center of the complete Slate widget.
            constexpr double LeftMargin = 18.0;
            constexpr double RightMargin = 18.0;
            constexpr double TopMargin = 72.0;
            constexpr double BottomMargin = 26.0;
            const FVector2D WidgetSize = MyGeometry.GetLocalSize();
            const FVector2D PlotMinimum(LeftMargin, TopMargin);
            const FVector2D PlotMaximum(
                FMath::Max(LeftMargin + 1.0, WidgetSize.X - RightMargin),
                FMath::Max(TopMargin + 1.0, WidgetSize.Y - BottomMargin));
            const FVector2D PlotCenter =
                (PlotMinimum + PlotMaximum) * 0.5;
            const FVector2D CursorLocal = MyGeometry.AbsoluteToLocal(
                MouseEvent.GetScreenSpacePosition());
            const FVector2D FromPlotCenter = CursorLocal - PlotCenter;
            const double ZoomRatio = ViewZoom / PreviousZoom;
            ViewPanPixels = FromPlotCenter -
                (FromPlotCenter - ViewPanPixels) * ZoomRatio;
            Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled();
        }

    private:
        TWeakObjectPtr<UTGVisualizationDockWorkspaceWidget> Owner;
        TArray<FVector> Samples;
        FString XColumn;
        FString YColumn;
        FString ZColumn;
        FVector RawMinimum = FVector::ZeroVector;
        FVector RawMaximum = FVector::ZeroVector;
        double XScale = 1.0;
        double YScale = 1.0;
        double ZScale = 1.0;
        double StartNormalized = 0.0;
        double EndNormalized = 1.0;
        FQuat ViewRotation = FQuat(FRotator(38.0, -32.0, 0.0));
        double ViewZoom = 1.0;
        FVector2D ViewPanPixels = FVector2D::ZeroVector;
        bool bHasZ = false;
        bool bRotating = false;
        bool bPanning = false;

        static double SafeScale(const double Value)
        {
            return FMath::IsFinite(Value) && Value != 0.0
                ? Value
                : 1.0;
        }

        static int32 ComputeTickDisplayExponent(
            const double Minimum,
            const double Maximum)
        {
            const double ReferenceMagnitude = FMath::Max(
                FMath::Abs(Minimum),
                FMath::Abs(Maximum));
            if (!FMath::IsFinite(ReferenceMagnitude) ||
                ReferenceMagnitude <= 0.0)
            {
                return 0;
            }

            const int32 Exponent = FMath::FloorToInt(
                FMath::LogX(10.0, ReferenceMagnitude));
            return Exponent >= 4 || Exponent <= -3
                ? FMath::Clamp(Exponent, -18, 18)
                : 0;
        }

        static double ComputeNiceTickStep(
            const double Range,
            const float AxisScreenLength)
        {
            constexpr double MinimumTickStep = 1.0e-18;
            constexpr double MaximumTickStep = 1.0e18;
            if (!FMath::IsFinite(Range) || Range <= 0.0)
            {
                return 0.0;
            }

            const int32 DesiredIntervals = FMath::Clamp(
                FMath::FloorToInt(AxisScreenLength / 86.0f),
                2,
                64);
            const double RoughStep = FMath::Clamp(
                Range / static_cast<double>(DesiredIntervals),
                MinimumTickStep,
                MaximumTickStep);
            const double StepPower = FMath::Pow(
                10.0,
                FMath::FloorToDouble(FMath::LogX(10.0, RoughStep)));
            const double StepFraction = RoughStep / StepPower;
            const double NiceFraction = StepFraction <= 1.0
                ? 1.0
                : (StepFraction <= 2.0
                    ? 2.0
                    : (StepFraction <= 5.0 ? 5.0 : 10.0));
            return FMath::Clamp(
                NiceFraction * StepPower,
                MinimumTickStep,
                MaximumTickStep);
        }

        static int32 ComputeTickDecimalPlaces(
            const double TickStep,
            const double DisplayFactor)
        {
            const double DisplayStep = FMath::Abs(
                TickStep / FMath::Max(DisplayFactor, 1.0e-18));
            if (!FMath::IsFinite(DisplayStep) || DisplayStep <= 0.0)
            {
                return 0;
            }
            return FMath::Clamp(
                FMath::CeilToInt(-FMath::LogX(10.0, DisplayStep)) + 1,
                0,
                12);
        }

        static FString FormatTickValue(
            const double RawValue,
            const double DisplayFactor,
            const int32 DecimalPlaces)
        {
            double DisplayValue = RawValue / DisplayFactor;
            const double ZeroThreshold = 0.5 * FMath::Pow(
                10.0,
                -static_cast<double>(DecimalPlaces));
            if (FMath::Abs(DisplayValue) < ZeroThreshold)
            {
                DisplayValue = 0.0;
            }

            FString Result = FString::Printf(
                TEXT("%.*f"),
                DecimalPlaces,
                DisplayValue);
            while (Result.Contains(TEXT(".")) &&
                   Result.EndsWith(TEXT("0")))
            {
                Result.LeftChopInline(1);
            }
            if (Result.EndsWith(TEXT(".")))
            {
                Result.LeftChopInline(1);
            }
            return Result == TEXT("-0") ? FString(TEXT("0")) : Result;
        }

        static FString FormatAxisDisplayValue(
            const double RawValue,
            const double RawMinimum,
            const double RawMaximum)
        {
            const int32 DisplayExponent = ComputeTickDisplayExponent(
                RawMinimum,
                RawMaximum);
            const double DisplayFactor = FMath::Pow(
                10.0,
                static_cast<double>(DisplayExponent));
            return FormatNumber(RawValue / DisplayFactor);
        }

        static void DrawLine(
            FSlateWindowElementList& Elements,
            const FGeometry& Geometry,
            const int32 Layer,
            const FVector2f& A,
            const FVector2f& B,
            const FLinearColor& Color,
            const ESlateDrawEffect Effects,
            const float Thickness)
        {
            TArray<FVector2f> Points;
            Points.Reserve(2);
            Points.Add(A);
            Points.Add(B);
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

        static void DrawPointMarker(
            FSlateWindowElementList& Elements,
            const FGeometry& Geometry,
            const int32 Layer,
            const FVector2f& Center,
            const FLinearColor& Color,
            const ESlateDrawEffect Effects)
        {
            constexpr int32 SegmentCount = 20;
            constexpr float Radius = 5.0f;
            TArray<FVector2f> Points;
            Points.Reserve(SegmentCount + 3);
            for (int32 Index = -1; Index <= SegmentCount + 1; ++Index)
            {
                const double Angle =
                    2.0 * UE_DOUBLE_PI *
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
                5.0f);
        }

        static void DrawAxisSummary(
            FSlateWindowElementList& Elements,
            const FGeometry& Geometry,
            const int32 Layer,
            const float Y,
            const FString& Axis,
            const FString& Column,
            const double Minimum,
            const double Maximum,
            const double Scale,
            const FLinearColor& Color,
            const FSlateFontInfo& Font,
            const ESlateDrawEffect Effects,
            const FLinearColor& WidgetTint)
        {
            (void)Scale;
            const int32 DisplayExponent = ComputeTickDisplayExponent(
                Minimum,
                Maximum);
            const FString Text = DisplayExponent == 0
                ? FString::Printf(
                    TEXT("%s  |  %s"),
                    *Axis,
                    *FormatResultColumnLabel(Column))
                : FString::Printf(
                    TEXT("%s  |  %s  |  x10^%d"),
                    *Axis,
                    *FormatResultColumnLabel(Column),
                    DisplayExponent);
            FSlateDrawElement::MakeText(
                Elements,
                Layer,
                Geometry.ToOffsetPaintGeometry(FVector2D(8.0f, Y)),
                Text,
                Font,
                Effects,
                Color * WidgetTint);
        }
    };

    enum class EDockSide : uint8
    {
        Left,
        Right,
        Top,
        Floating
    };

    enum class EMinimizedEdge : uint8
    {
        Left,
        Right,
        Top,
        Bottom
    };

    enum class EFloatingResizeEdge : uint8
    {
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    struct FPanelState
    {
        FName Id;
        FText Title;
        EDockSide Side = EDockSide::Floating;
        EDockSide LastSide = EDockSide::Floating;
        bool bOpen = true;
        bool bMinimized = false;
        EMinimizedEdge MinimizedEdge = EMinimizedEdge::Left;
        FVector2D FloatingPosition = FVector2D(80.0f, 80.0f);
        FVector2D FloatingSize = FVector2D(380.0f, 420.0f);
        int32 FloatingZOrder = 1;
        TSharedPtr<SWidget> Content;
    };

    class FPanelDragDropOperation : public FDragDropOperation
    {
    public:
        DRAG_DROP_OPERATOR_TYPE(FPanelDragDropOperation, FDragDropOperation)

        static TSharedRef<FPanelDragDropOperation> New(
            const FName InPanelId,
            const FText& InTitle,
            const FVector2D& InGrabOffset,
            const FVector2D& InPanelSize)
        {
            (void)InTitle;
            (void)InPanelSize;
            TSharedRef<FPanelDragDropOperation> Operation =
                MakeShared<FPanelDragDropOperation>();
            Operation->PanelId = InPanelId;
            Operation->GrabOffset = InGrabOffset;
            // Do not draw a placeholder/decorator. The real panel itself is
            // detached (when necessary) and moved continuously under the cursor.
            Operation->bCreateNewWindow = false;
            Operation->MouseCursor = EMouseCursor::GrabHandClosed;
            Operation->Construct();
            return Operation;
        }

        virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
        {
            return SNullWidget::NullWidget;
        }

        FName PanelId;
        FVector2D GrabOffset = FVector2D::ZeroVector;
    };

    class SPanelDragHandle : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SPanelDragHandle) {}
            SLATE_ARGUMENT(FName, PanelId)
            SLATE_ARGUMENT(FText, Title)
            SLATE_ARGUMENT(FVector2D, DragPreviewSize)
            SLATE_ATTRIBUTE(bool, Active)
            SLATE_EVENT(FSimpleDelegate, OnActivated)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            PanelId = InArgs._PanelId;
            Title = InArgs._Title;
            DragPreviewSize = InArgs._DragPreviewSize;
            Active = InArgs._Active;
            OnActivated = InArgs._OnActivated;
            ChildSlot
            [
                SNew(SBox)
                .MinDesiredHeight(40.0f)
                [
                    SAssignNew(TabBorder, SBorder)
                    .BorderImage(TAttribute<const FSlateBrush*>::CreateLambda(
                        [this]()
                        {
                            if (TabBorder.IsValid() && TabBorder->IsHovered())
                            {
                                // Preserve the exact known-good themed hover brush.
                                return &HoveredTabBrush();
                            }
                            return Active.Get(false)
                                ? &ActiveTabShapeBrush()
                                : &InactiveTabShapeBrush();
                        }))
                    // Brushes above now contain their final colors; white only acts
                    // as the neutral Slate multiplier and is never drawn by itself.
                    .BorderBackgroundColor(FLinearColor::White)
                    .Padding(FMargin(12.0f, 0.0f))
                    [
                        SNew(SOverlay)
                        + SOverlay::Slot()
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Text(Title)
                            .ColorAndOpacity_Lambda([this]()
                            {
                                return Active.Get(false)
                                    ? Palette().TextPrimary
                                    : Palette().TextSecondary;
                            })
                            .Font(TGUiTheme::GetSlateFont(
                                ETGUiTextStyle::FieldLabel))
                        ]
                        + SOverlay::Slot()
                        .VAlign(VAlign_Bottom)
                        [
                            SNew(SBox)
                            .HeightOverride(2.0f)
                            .Visibility_Lambda([this]()
                            {
                                return Active.Get(false)
                                    ? EVisibility::HitTestInvisible
                                    : EVisibility::Collapsed;
                            })
                            [
                                SNew(SBorder)
                                .BorderImage(&AccentIndicatorBrush())
                            ]
                        ]
                    ]
                ]
            ];
        }

        virtual FReply OnMouseButtonDown(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
            {
                OnActivated.ExecuteIfBound();
                return FReply::Handled().DetectDrag(
                    SharedThis(this),
                    EKeys::LeftMouseButton);
            }
            return FReply::Unhandled();
        }

        virtual FReply OnDragDetected(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            return FReply::Handled().BeginDragDrop(
                FPanelDragDropOperation::New(
                    PanelId,
                    Title,
                    MyGeometry.AbsoluteToLocal(
                        MouseEvent.GetScreenSpacePosition()),
                    DragPreviewSize));
        }

    private:
        FName PanelId;
        FText Title;
        FVector2D DragPreviewSize = FVector2D(380.0f, 420.0f);
        TAttribute<bool> Active;
        FSimpleDelegate OnActivated;
        TSharedPtr<SBorder> TabBorder;

    };

    DECLARE_DELEGATE_ThreeParams(
        FOnFloatingPanelResize,
        FName,
        EFloatingResizeEdge,
        FVector2D);

    class SFloatingResizeHandle : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SFloatingResizeHandle) {}
            SLATE_ARGUMENT(FName, PanelId)
            SLATE_ARGUMENT(EFloatingResizeEdge, Edge)
            SLATE_EVENT(FOnFloatingPanelResize, OnResize)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            PanelId = InArgs._PanelId;
            Edge = InArgs._Edge;
            OnResize = InArgs._OnResize;
            ChildSlot
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    SNew(SBorder)
                    .BorderImage(&TintableRoundedBrush())
                    .BorderBackgroundColor(FLinearColor::Transparent)
                ]
                + SOverlay::Slot()
                .HAlign(HAlign_Right)
                .VAlign(VAlign_Bottom)
                [
                    SNew(SBox)
                    .WidthOverride(12.0f)
                    .HeightOverride(12.0f)
                    .Visibility_Lambda([this]()
                    {
                        return Edge == EFloatingResizeEdge::BottomRight
                            ? EVisibility::HitTestInvisible
                            : EVisibility::Collapsed;
                    })
                    [
                        SNew(SOverlay)
                        + SOverlay::Slot()
                        .HAlign(HAlign_Right)
                        .VAlign(VAlign_Bottom)
                        .Padding(FMargin(0.0f, 0.0f, 1.0f, 2.0f))
                        [
                            SNew(SBox)
                            .WidthOverride(10.0f)
                            .HeightOverride(1.0f)
                            [
                                SNew(SBorder)
                                .BorderImage(&TintableRoundedBrush())
                                .BorderBackgroundColor_Lambda([this]()
                                {
                                    return IsHovered()
                                        ? Palette().Accent
                                        : Palette().Border;
                                })
                            ]
                        ]
                        + SOverlay::Slot()
                        .HAlign(HAlign_Right)
                        .VAlign(VAlign_Bottom)
                        .Padding(FMargin(0.0f, 0.0f, 1.0f, 6.0f))
                        [
                            SNew(SBox)
                            .WidthOverride(6.0f)
                            .HeightOverride(1.0f)
                            [
                                SNew(SBorder)
                                .BorderImage(&TintableRoundedBrush())
                                .BorderBackgroundColor_Lambda([this]()
                                {
                                    return IsHovered()
                                        ? Palette().Accent
                                        : Palette().Border;
                                })
                            ]
                        ]
                    ]
                ]
            ];
        }

        virtual FReply OnMouseButtonDown(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
            {
                return FReply::Unhandled();
            }
            PreviousCursorPosition = MouseEvent.GetScreenSpacePosition();
            return FReply::Handled().CaptureMouse(SharedThis(this));
        }

        virtual FReply OnMouseMove(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            if (!HasMouseCapture())
            {
                return FReply::Unhandled();
            }
            const FVector2D Current = MouseEvent.GetScreenSpacePosition();
            OnResize.ExecuteIfBound(
                PanelId,
                Edge,
                Current - PreviousCursorPosition);
            PreviousCursorPosition = Current;
            return FReply::Handled();
        }

        virtual FReply OnMouseButtonUp(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            return HasMouseCapture()
                ? FReply::Handled().ReleaseMouseCapture()
                : FReply::Unhandled();
        }

        virtual FCursorReply OnCursorQuery(
            const FGeometry& MyGeometry,
            const FPointerEvent& CursorEvent) const override
        {
            switch (Edge)
            {
            case EFloatingResizeEdge::Left:
            case EFloatingResizeEdge::Right:
                return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
            case EFloatingResizeEdge::Top:
            case EFloatingResizeEdge::Bottom:
                return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
            case EFloatingResizeEdge::TopRight:
            case EFloatingResizeEdge::BottomLeft:
                return FCursorReply::Cursor(EMouseCursor::ResizeSouthWest);
            case EFloatingResizeEdge::TopLeft:
            case EFloatingResizeEdge::BottomRight:
            default:
                return FCursorReply::Cursor(EMouseCursor::ResizeSouthEast);
            }
        }

    private:
        FName PanelId;
        EFloatingResizeEdge Edge = EFloatingResizeEdge::BottomRight;
        FOnFloatingPanelResize OnResize;
        FVector2D PreviousCursorPosition = FVector2D::ZeroVector;
    };

    DECLARE_DELEGATE_TwoParams(FOnDockEdgeResize, EDockSide, float);
    DECLARE_DELEGATE_OneParam(FOnDockEdgeResizeFinished, EDockSide);
    DECLARE_DELEGATE_OneParam(FOnDockEdgeToggleCollapse, EDockSide);

    /** Independent pixel resize rail for one viewport edge. */
    class SDockEdgeResizeHandle : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SDockEdgeResizeHandle) {}
            SLATE_ARGUMENT(EDockSide, Side)
            SLATE_ATTRIBUTE(bool, Collapsed)
            SLATE_EVENT(FOnDockEdgeResize, OnResize)
            SLATE_EVENT(FOnDockEdgeResizeFinished, OnResizeFinished)
            SLATE_EVENT(FOnDockEdgeToggleCollapse, OnToggleCollapse)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            Side = InArgs._Side;
            Collapsed = InArgs._Collapsed;
            OnResize = InArgs._OnResize;
            OnResizeFinished = InArgs._OnResizeFinished;
            OnToggleCollapse = InArgs._OnToggleCollapse;
            bHorizontalRail = Side == EDockSide::Top;

            // The collapse/restore glyph is drawn DIRECTLY inside this splitter
            // drag handle. There is no nested SButton or separate button face.
            const auto MakeEmbeddedCollapseControl = [this]() -> TSharedRef<SWidget>
            {
                return SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        const bool bIsCollapsed = Collapsed.Get(false);
                        if (Side == EDockSide::Left)
                        {
                            return FText::FromString(
                                bIsCollapsed ? TEXT("▷") : TEXT("◁"));
                        }
                        if (Side == EDockSide::Right)
                        {
                            return FText::FromString(
                                bIsCollapsed ? TEXT("◁") : TEXT("▷"));
                        }
                        return FText::FromString(
                            bIsCollapsed ? TEXT("v") : TEXT("^"));
                    })
                    .Justification(ETextJustify::Center)
                    .ColorAndOpacity(Palette().TextPrimary)
                    .Font(TGUiTheme::GetSlateFont(ETGUiTextStyle::PanelTitle))
                    .ToolTipText(FText::FromString(
                        TEXT("Collapse / restore this dock")))
                    .Visibility(EVisibility::HitTestInvisible);
            };

            if (bHorizontalRail)
            {
                ChildSlot
                [
                    SNew(SBox)
                    .HeightOverride(EdgeHandleThickness)
                    [
                        SNew(SOverlay)
                        + SOverlay::Slot()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                            .HeightOverride(DockSplitterVisualThickness)
                            [
                                SNew(SBorder)
                                .BorderImage(&TintableRoundedBrush())
                                .BorderBackgroundColor_Lambda([this]()
                                {
                                    return IsHovered()
                                        ? Palette().Accent
                                        : Palette().Border;
                                })
                            ]
                        ]
                        + SOverlay::Slot()
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                            .WidthOverride(SplitterCollapseButtonLongSize)
                            .HeightOverride(SplitterCollapseButtonShortSize)
                            .HAlign(HAlign_Center)
                            .VAlign(VAlign_Center)
                            [MakeEmbeddedCollapseControl()]
                        ]
                    ]
                ];
            }
            else
            {
                ChildSlot
                [
                    SNew(SBox)
                    .WidthOverride(EdgeHandleThickness)
                    [
                        SNew(SOverlay)
                        + SOverlay::Slot()
                        .HAlign(HAlign_Center)
                        [
                            SNew(SBox)
                            .WidthOverride(DockSplitterVisualThickness)
                            [
                                SNew(SBorder)
                                .BorderImage(&TintableRoundedBrush())
                                .BorderBackgroundColor_Lambda([this]()
                                {
                                    return IsHovered()
                                        ? Palette().Accent
                                        : Palette().Border;
                                })
                            ]
                        ]
                        + SOverlay::Slot()
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                            .WidthOverride(SplitterCollapseButtonShortSize)
                            .HeightOverride(SplitterCollapseButtonLongSize)
                            .HAlign(HAlign_Center)
                            .VAlign(VAlign_Center)
                            [MakeEmbeddedCollapseControl()]
                        ]
                    ]
                ];
            }
        }

        virtual FReply OnMouseButtonDown(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
            {
                return FReply::Unhandled();
            }

            // Clicking the centered arrow region toggles the dock. Clicking anywhere
            // else on the same handle starts the normal splitter resize drag.
            bCollapseControlPressed = IsOverCollapseControl(MyGeometry, MouseEvent);
            PreviousCursorPosition = MouseEvent.GetScreenSpacePosition();
            return FReply::Handled().CaptureMouse(SharedThis(this));
        }

        virtual FReply OnMouseMove(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            if (!HasMouseCapture())
            {
                return FReply::Unhandled();
            }
            if (bCollapseControlPressed)
            {
                return FReply::Handled();
            }

            const FVector2D Current = MouseEvent.GetScreenSpacePosition();
            const FVector2D Delta = Current - PreviousCursorPosition;
            const float SignedDelta = Side == EDockSide::Left
                ? static_cast<float>(Delta.X)
                : Side == EDockSide::Right
                    ? static_cast<float>(-Delta.X)
                    : static_cast<float>(Delta.Y);
            OnResize.ExecuteIfBound(Side, SignedDelta);
            PreviousCursorPosition = Current;
            return FReply::Handled();
        }

        virtual FReply OnMouseButtonUp(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            if (!HasMouseCapture())
            {
                return FReply::Unhandled();
            }

            if (bCollapseControlPressed)
            {
                if (IsOverCollapseControl(MyGeometry, MouseEvent))
                {
                    OnToggleCollapse.ExecuteIfBound(Side);
                }
                bCollapseControlPressed = false;
                return FReply::Handled().ReleaseMouseCapture();
            }

            OnResizeFinished.ExecuteIfBound(Side);
            return FReply::Handled().ReleaseMouseCapture();
        }

        virtual FCursorReply OnCursorQuery(
            const FGeometry& MyGeometry,
            const FPointerEvent& CursorEvent) const override
        {
            if (IsOverCollapseControl(MyGeometry, CursorEvent))
            {
                return FCursorReply::Cursor(EMouseCursor::Hand);
            }
            return FCursorReply::Cursor(
                Side == EDockSide::Top
                    ? EMouseCursor::ResizeUpDown
                    : EMouseCursor::ResizeLeftRight);
        }

    private:
        bool IsOverCollapseControl(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) const
        {
            const FVector2D Local = MyGeometry.AbsoluteToLocal(
                MouseEvent.GetScreenSpacePosition());
            const FVector2D Size = MyGeometry.GetLocalSize();

            if (bHorizontalRail)
            {
                const double HalfLong = SplitterCollapseButtonLongSize * 0.5;
                const double HalfShort = SplitterCollapseButtonShortSize * 0.5;
                return FMath::Abs(Local.X - Size.X * 0.5) <= HalfLong &&
                    FMath::Abs(Local.Y - Size.Y * 0.5) <= HalfShort;
            }

            const double HalfLong = SplitterCollapseButtonLongSize * 0.5;
            const double HalfShort = SplitterCollapseButtonShortSize * 0.5;
            return FMath::Abs(Local.X - Size.X * 0.5) <= HalfShort &&
                FMath::Abs(Local.Y - Size.Y * 0.5) <= HalfLong;
        }

        EDockSide Side = EDockSide::Left;
        bool bHorizontalRail = false;
        bool bCollapseControlPressed = false;
        TAttribute<bool> Collapsed;
        FOnDockEdgeResize OnResize;
        FOnDockEdgeResizeFinished OnResizeFinished;
        FOnDockEdgeToggleCollapse OnToggleCollapse;
        FVector2D PreviousCursorPosition = FVector2D::ZeroVector;
    };

    DECLARE_DELEGATE_OneParam(FOnTimelineResize, float);

    /** Fixed-bottom timeline resize rail. It is not a dock target. */
    class STimelineResizeHandle : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(STimelineResizeHandle) {}
            SLATE_ATTRIBUTE(bool, Collapsed)
            SLATE_EVENT(FOnTimelineResize, OnResize)
            SLATE_EVENT(FSimpleDelegate, OnResizeFinished)
            SLATE_EVENT(FSimpleDelegate, OnToggleCollapse)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            Collapsed = InArgs._Collapsed;
            OnResize = InArgs._OnResize;
            OnResizeFinished = InArgs._OnResizeFinished;
            OnToggleCollapse = InArgs._OnToggleCollapse;

            ChildSlot
            [
                SNew(SBox)
                .HeightOverride(TimelineHandleThickness)
                [
                    SNew(SOverlay)
                    + SOverlay::Slot()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SBox)
                        .HeightOverride(TimelineSplitterVisualThickness)
                        [
                            SNew(SBorder)
                            .BorderImage(&TintableRoundedBrush())
                            .BorderBackgroundColor_Lambda([this]()
                            {
                                return IsHovered()
                                    ? Palette().Accent
                                    : Palette().Border;
                            })
                        ]
                    ]
                    + SOverlay::Slot()
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    [
                        SNew(SBox)
                        .WidthOverride(SplitterCollapseButtonLongSize)
                        .HeightOverride(SplitterCollapseButtonShortSize)
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Text_Lambda([this]()
                            {
                                return FText::FromString(
                                    Collapsed.Get(false) ? TEXT("△") : TEXT("▽"));
                            })
                            .Justification(ETextJustify::Center)
							.RenderTransform(FSlateRenderTransform(FVector2D(0.0f, -3.0f)))
                            .ColorAndOpacity(Palette().TextPrimary)
                            .Font(TGUiTheme::GetSlateFont(
                                ETGUiTextStyle::PanelTitle))
                            .ToolTipText(FText::FromString(
                                TEXT("Collapse / restore timeline")))
                            .Visibility(EVisibility::HitTestInvisible)
                        ]
                    ]
                ]
            ];
        }

        virtual FReply OnMouseButtonDown(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
            {
                return FReply::Unhandled();
            }
            bCollapseControlPressed = IsOverCollapseControl(MyGeometry, MouseEvent);
            PreviousCursorY = MouseEvent.GetScreenSpacePosition().Y;
            return FReply::Handled().CaptureMouse(SharedThis(this));
        }

        virtual FReply OnMouseMove(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            if (!HasMouseCapture())
            {
                return FReply::Unhandled();
            }
            if (bCollapseControlPressed)
            {
                return FReply::Handled();
            }
            const double CurrentY = MouseEvent.GetScreenSpacePosition().Y;
            OnResize.ExecuteIfBound(static_cast<float>(CurrentY - PreviousCursorY));
            PreviousCursorY = CurrentY;
            return FReply::Handled();
        }

        virtual FReply OnMouseButtonUp(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            if (!HasMouseCapture())
            {
                return FReply::Unhandled();
            }
            if (bCollapseControlPressed)
            {
                if (IsOverCollapseControl(MyGeometry, MouseEvent))
                {
                    OnToggleCollapse.ExecuteIfBound();
                }
                bCollapseControlPressed = false;
                return FReply::Handled().ReleaseMouseCapture();
            }
            OnResizeFinished.ExecuteIfBound();
            return FReply::Handled().ReleaseMouseCapture();
        }

        virtual FReply OnMouseButtonDoubleClick(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override
        {
            if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
            {
                OnToggleCollapse.ExecuteIfBound();
                return FReply::Handled();
            }
            return FReply::Unhandled();
        }

        virtual FCursorReply OnCursorQuery(
            const FGeometry& MyGeometry,
            const FPointerEvent& CursorEvent) const override
        {
            return IsOverCollapseControl(MyGeometry, CursorEvent)
                ? FCursorReply::Cursor(EMouseCursor::Hand)
                : FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
        }

    private:
        bool IsOverCollapseControl(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) const
        {
            const FVector2D Local = MyGeometry.AbsoluteToLocal(
                MouseEvent.GetScreenSpacePosition());
            const FVector2D Size = MyGeometry.GetLocalSize();
            const double HalfLong = SplitterCollapseButtonLongSize * 0.5;
            const double HalfShort = SplitterCollapseButtonShortSize * 0.5;
            return FMath::Abs(Local.X - Size.X * 0.5) <= HalfLong &&
                FMath::Abs(Local.Y - Size.Y * 0.5) <= HalfShort;
        }

        bool bCollapseControlPressed = false;
        double PreviousCursorY = 0.0;
        TAttribute<bool> Collapsed;
        FOnTimelineResize OnResize;
        FSimpleDelegate OnResizeFinished;
        FSimpleDelegate OnToggleCollapse;
    };

    DECLARE_DELEGATE_FourParams(
        FOnPanelDropped,
        FName,
        FVector2D,
        FVector2D,
        FVector2D);
    DECLARE_DELEGATE_FourParams(
        FOnPanelDragOver,
        FName,
        FVector2D,
        FVector2D,
        FVector2D);

    class SWorkspaceDropSurface : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SWorkspaceDropSurface) {}
            SLATE_DEFAULT_SLOT(FArguments, Content)
            SLATE_EVENT(FOnPanelDropped, OnPanelDropped)
            SLATE_EVENT(FOnPanelDragOver, OnPanelDragOver)
            SLATE_EVENT(FSimpleDelegate, OnPanelDragLeave)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            OnPanelDropped = InArgs._OnPanelDropped;
            OnPanelDragOver = InArgs._OnPanelDragOver;
            OnPanelDragLeave = InArgs._OnPanelDragLeave;
            SetVisibility(TAttribute<EVisibility>::CreateLambda([]()
            {
                if (FSlateApplication::IsInitialized())
                {
                    const TSharedPtr<FDragDropOperation> Operation =
                        FSlateApplication::Get().GetDragDroppingContent();
                    if (Operation.IsValid() &&
                        Operation->IsOfType<FPanelDragDropOperation>())
                    {
                        return EVisibility::Visible;
                    }
                }
                // Children remain interactive, but the empty viewport center
                // passes mouse input through to the level and camera.
                return EVisibility::SelfHitTestInvisible;
            }));
            ChildSlot[InArgs._Content.Widget];
        }

        virtual FReply OnDragOver(
            const FGeometry& MyGeometry,
            const FDragDropEvent& DragDropEvent) override
        {
            const TSharedPtr<FPanelDragDropOperation> Operation =
                DragDropEvent.GetOperationAs<FPanelDragDropOperation>();
            if (!Operation.IsValid())
            {
                return FReply::Unhandled();
            }
            OnPanelDragOver.ExecuteIfBound(
                Operation->PanelId,
                MyGeometry.AbsoluteToLocal(
                    DragDropEvent.GetScreenSpacePosition()),
                MyGeometry.GetLocalSize(),
                Operation->GrabOffset);
            return FReply::Handled();
        }

        virtual void OnDragLeave(
            const FDragDropEvent& DragDropEvent) override
        {
            if (DragDropEvent.GetOperationAs<FPanelDragDropOperation>().IsValid())
            {
                OnPanelDragLeave.ExecuteIfBound();
            }
            SCompoundWidget::OnDragLeave(DragDropEvent);
        }

        virtual FReply OnDrop(
            const FGeometry& MyGeometry,
            const FDragDropEvent& DragDropEvent) override
        {
            const TSharedPtr<FPanelDragDropOperation> Operation =
                DragDropEvent.GetOperationAs<FPanelDragDropOperation>();
            if (!Operation.IsValid())
            {
                return FReply::Unhandled();
            }
            OnPanelDragLeave.ExecuteIfBound();
            OnPanelDropped.ExecuteIfBound(
                Operation->PanelId,
                MyGeometry.AbsoluteToLocal(
                    DragDropEvent.GetScreenSpacePosition()),
                MyGeometry.GetLocalSize(),
                Operation->GrabOffset);
            return FReply::Handled();
        }

    private:
        FOnPanelDropped OnPanelDropped;
        FOnPanelDragOver OnPanelDragOver;
        FSimpleDelegate OnPanelDragLeave;
    };
}

class STGVisualizationDockWorkspace : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STGVisualizationDockWorkspace) {}
        SLATE_ARGUMENT(UTGVisualizationDockWorkspaceWidget*, Owner)
    SLATE_END_ARGS()

    ~STGVisualizationDockWorkspace()
    {
        SaveLayoutNow();
    }

    void Construct(const FArguments& InArgs)
    {
        using namespace TGVisualizationDockPrivate;
        Owner = InArgs._Owner;
        // The workspace covers the viewport, but only its actual controls
        // participate in hit testing. Empty space remains available to the
        // level camera and other HUD layers.
        SetVisibility(EVisibility::SelfHitTestInvisible);

        RefreshGraphicsColumnOptions();
        TSharedRef<SWidget> ConstellationsContent =
            BuildConstellationsContent();
        TSharedRef<SWidget> VectorsContent = BuildVectorsContent();
        TSharedRef<SWidget> TelemetryContent = BuildTelemetryContent();
        TSharedRef<SWidget> SolarSystemContent = BuildSolarSystemContent();
        TSharedRef<SWidget> GraphicsContent = BuildGraphicsContent();

        AddPanel(
            TEXT("Constellations"),
            FText::FromString(TEXT("CONSTELLATIONS")),
            ConstellationsContent);
        AddPanel(
            TEXT("Vectors"),
            FText::FromString(TEXT("VECTORS")),
            VectorsContent);
        AddPanel(
            TEXT("Telemetry"),
            FText::FromString(TEXT("TELEMETRY")),
            TelemetryContent);
        AddPanel(
            TEXT("SolarSystem"),
            FText::FromString(TEXT("SOLAR SYSTEM")),
            SolarSystemContent);
        AddPanel(
            TEXT("Graphics"),
            FText::FromString(TEXT("PLOTS")),
            GraphicsContent);
        ApplyDefaultLayout();
        LoadLayout();

        ChildSlot
        [
            SNew(SVerticalBox)
            .Visibility(EVisibility::SelfHitTestInvisible)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                .HeightOverride(MenuHeight)
                [
                    SNew(SBorder)
                    .BorderImage(&HeaderBrush())
                    .Padding(FMargin(14.0f, 4.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                            .MinDesiredWidth(92.0f)
                            .HeightOverride(34.0f)
                            [
                                SAssignNew(WindowMenuAnchor, SMenuAnchor)
                                .Placement(MenuPlacement_BelowAnchor)
                                .OnGetMenuContent(
                                    this,
                                    &STGVisualizationDockWorkspace::
                                        BuildWindowMenu)
                                [
                                    SNew(SButton)
                                    .ButtonStyle(&TGUiTheme::GetButtonStyle(
                                        ETGUiButtonStyle::Quiet))
                                    .HAlign(HAlign_Center)
                                    .VAlign(VAlign_Center)
                                    .ContentPadding(FMargin(0.0f))
                                    .OnClicked(
                                        this,
                                        &STGVisualizationDockWorkspace::
                                            HandleWindowMenuClicked)
                                    [
                                        SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("Windows")))
                                        .ColorAndOpacity(Palette().TextPrimary)
                                        .Font(TGUiTheme::GetSlateFont(
                                            ETGUiTextStyle::FieldLabel))
                                    ]
                                ]
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
                        [
                            SNew(SBox)
                            .MinDesiredWidth(92.0f)
                            .HeightOverride(34.0f)
                            [
                                SAssignNew(OptionsMenuAnchor, SMenuAnchor)
                                .Placement(MenuPlacement_BelowAnchor)
                                .OnGetMenuContent(
                                    this,
                                    &STGVisualizationDockWorkspace::
                                        BuildOptionsMenu)
                                [
                                    SNew(SButton)
                                    .ButtonStyle(&TGUiTheme::GetButtonStyle(
                                        ETGUiButtonStyle::Quiet))
                                    .HAlign(HAlign_Center)
                                    .VAlign(VAlign_Center)
                                    .ContentPadding(FMargin(0.0f))
                                    .OnClicked(
                                        this,
                                        &STGVisualizationDockWorkspace::
                                            HandleOptionsMenuClicked)
                                    [
                                        SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("Options")))
                                        .ColorAndOpacity(Palette().TextPrimary)
                                        .Font(TGUiTheme::GetSlateFont(
                                            ETGUiTextStyle::FieldLabel))
                                    ]
                                ]
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        [
                            SNew(SBorder)
                            .Visibility(EVisibility::SelfHitTestInvisible)
                            .BorderImage(&HeaderFillBrush())
                        ]
                    ]
                ]
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SNew(SWorkspaceDropSurface)
                .OnPanelDropped(
                    this,
                    &STGVisualizationDockWorkspace::HandlePanelDropped)
                .OnPanelDragOver(
                    this,
                    &STGVisualizationDockWorkspace::HandlePanelDragOver)
                .OnPanelDragLeave(FSimpleDelegate::CreateSP(
                    this,
                    &STGVisualizationDockWorkspace::HandlePanelDragLeave))
                [
                    SAssignNew(LayoutHost, SBox)
                    .Visibility(EVisibility::SelfHitTestInvisible)
                ]
            ]
        ];

        RebuildConstellationRows();
        RebuildVectorRows();
        RebuildTelemetryRows();
        RefreshGraphicsPlotData(true);
        RebuildLayout();
    }

    void RefreshAll()
    {
        RebuildConstellationRows();
        RebuildVectorRows();
        RebuildTelemetryRows();
        RefreshGraphicsColumnOptions();
        RefreshTimeline();
    }

    void RefreshTelemetry()
    {
        RebuildTelemetryRows();
    }

    void RefreshTimeline()
    {
        InitializeTimelineInputsForPlayback();
        Invalidate(EInvalidateWidgetReason::Layout);
    }

    bool IsPointerOverInteractiveArea() const
    {
        if (!FSlateApplication::IsInitialized())
        {
            return false;
        }

        const FVector2D CursorPosition =
            FSlateApplication::Get().GetCursorPos();
        const auto ContainsCursor = [&CursorPosition](
            const TSharedPtr<SWidget>& Widget)
        {
            if (!Widget.IsValid() || !Widget->GetVisibility().IsVisible())
            {
                return false;
            }
            const FGeometry& Geometry = Widget->GetCachedGeometry();
            const FVector2D Size = Geometry.GetLocalSize();
            return Size.X > 0.0 && Size.Y > 0.0 &&
                Geometry.IsUnderLocation(CursorPosition);
        };

        if (WindowMenuAnchor.IsValid() &&
            (ContainsCursor(WindowMenuAnchor) || WindowMenuAnchor->IsOpen()))
        {
            return true;
        }
        if (OptionsMenuAnchor.IsValid() &&
            (ContainsCursor(OptionsMenuAnchor) || OptionsMenuAnchor->IsOpen()))
        {
            return true;
        }
        for (const TPair<FName, TWeakPtr<SWidget>>& Pair : PanelWidgets)
        {
            const TSharedPtr<SWidget> Widget = Pair.Value.Pin();
            if (ContainsCursor(Widget))
            {
                return true;
            }
        }
        return ContainsCursor(TimelinePanelWidget);
    }

    virtual void Tick(
        const FGeometry& AllottedGeometry,
        const double InCurrentTime,
        const float InDeltaTime) override
    {
        SCompoundWidget::Tick(
            AllottedGeometry,
            InCurrentTime,
            InDeltaTime);
        if (bLayoutDirty &&
            FPlatformTime::Seconds() - LastLayoutChangeTime > 0.35)
        {
            SaveLayoutNow();
        }
        if (GraphicsPlot.IsValid())
        {
            GraphicsPlot->RefreshCurrentMarker();
        }
        if (bTimelineStatusError &&
            !TimelineStatusMessage.IsEmpty() &&
            TimelineStatusSetRealSeconds >= 0.0)
        {
            const double StatusAge =
                FPlatformTime::Seconds() - TimelineStatusSetRealSeconds;
            if (StatusAge >=
                TGVisualizationDockPrivate::TimelineErrorDisplaySeconds)
            {
                TimelineStatusMessage.Reset();
                bTimelineStatusError = false;
                TimelineStatusSetRealSeconds = -1.0;
                Invalidate(EInvalidateWidgetReason::Layout);
            }
            else if (StatusAge >=
                TGVisualizationDockPrivate::TimelineErrorDisplaySeconds -
                    TGVisualizationDockPrivate::TimelineErrorFadeSeconds)
            {
                Invalidate(EInvalidateWidgetReason::Paint);
            }
        }
    }

private:
    using EDockSide = TGVisualizationDockPrivate::EDockSide;
    using EMinimizedEdge = TGVisualizationDockPrivate::EMinimizedEdge;
    using EFloatingResizeEdge = TGVisualizationDockPrivate::EFloatingResizeEdge;
    using EGraphicsAxis = TGVisualizationDockPrivate::EGraphicsAxis;
    using FPanelState = TGVisualizationDockPrivate::FPanelState;

    TWeakObjectPtr<UTGVisualizationDockWorkspaceWidget> Owner;
    TArray<TSharedPtr<FPanelState>> Panels;
    TMap<FName, TWeakPtr<SWidget>> PanelWidgets;
    TSharedPtr<SBox> LayoutHost;
    TSharedPtr<SMenuAnchor> WindowMenuAnchor;
    TSharedPtr<SMenuAnchor> OptionsMenuAnchor;
    TSharedPtr<SVerticalBox> ConstellationRowsBox;
    TSharedPtr<SVerticalBox> VectorRowsBox;
    TSharedPtr<SVerticalBox> TelemetryRowsBox;
    TSharedPtr<TGVisualizationDockPrivate::STGSimulationDataPlot>
        GraphicsPlot;
    TArray<TSharedPtr<FString>> GraphicsColumnOptions;
    TArray<TSharedPtr<FString>> GraphicsZColumnOptions;
    TSharedPtr<FString> GraphicsXColumn;
    TSharedPtr<FString> GraphicsYColumn;
    TSharedPtr<FString> GraphicsZColumn;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> GraphicsXComboBox;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> GraphicsYComboBox;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> GraphicsZComboBox;
    TSharedPtr<SEditableTextBox> GraphicsStartTimeInput;
    TSharedPtr<SEditableTextBox> GraphicsEndTimeInput;
    TSharedPtr<SEditableTextBox> GraphicsXScaleInput;
    TSharedPtr<SEditableTextBox> GraphicsYScaleInput;
    TSharedPtr<SEditableTextBox> GraphicsZScaleInput;
    FText GraphicsStartTimeText = FText::FromString(TEXT("0"));
    FText GraphicsEndTimeText = FText::FromString(TEXT("0"));
    double GraphicsStartNormalized = 0.0;
    double GraphicsEndNormalized = 1.0;
    double GraphicsXScale = 1.0;
    double GraphicsYScale = 1.0;
    double GraphicsZScale = 1.0;
    bool bRefreshingGraphicsControls = false;
    TSharedPtr<SWidget> TimelinePanelWidget;
    TSharedPtr<SEditableTextBox> TimelineMissionInput;
    TSharedPtr<SEditableTextBox> TimelineEtInput;
    TSharedPtr<STGUtcDateTimeInput> TimelineUtcInput;
    TSharedPtr<SEditableTextBox> TimelineRateInput;
    FText TimelineMissionInputText;
    FText TimelineEtInputText;
    FText TimelineUtcInputText;
    FText TimelineRateInputText = FText::FromString(TEXT("1"));
    FText LastValidTimelineMissionInputText;
    FText LastValidTimelineEtInputText;
    FText LastValidTimelineUtcInputText;
    FText LastValidTimelineRateInputText = FText::FromString(TEXT("1"));
    TWeakObjectPtr<ATGSimulationPlaybackActor> TimelineInputPlaybackActor;
    bool bTimelineInputsInitialized = false;
    FString TimelineStatusMessage;
    bool bTimelineStatusError = false;
    double TimelineStatusSetRealSeconds = -1.0;
    float TimelineHeight = TGVisualizationDockPrivate::TimelineDefaultExpandedHeight;
    FString ConstellationFilter;
    FString VectorFilter;
    float LeftExtent = 504.0f;
    float RightExtent = 468.0f;
    float TopExtent = 280.0f;
    float LastExpandedLeftExtent = 504.0f;
    float LastExpandedRightExtent = 468.0f;
    float LastExpandedTopExtent = 280.0f;
    FName ActiveLeftTab;
    FName ActiveRightTab;
    FName ActiveTopTab;
    int32 HighestFloatingZOrder = 10;
    bool bShowDockPreview = false;
    EDockSide DockPreviewSide = EDockSide::Floating;
    FVector2D DockPreviewAreaSize = FVector2D::ZeroVector;
    bool bLayoutDirty = false;
    double LastLayoutChangeTime = 0.0;

    void AddPanel(
        const FName Id,
        const FText& Title,
        const TSharedRef<SWidget>& Content)
    {
        TSharedPtr<FPanelState> State = MakeShared<FPanelState>();
        State->Id = Id;
        State->Title = Title;
        State->Content = Content;
        Panels.Add(State);
    }

    TSharedPtr<FPanelState> FindPanel(const FName Id) const
    {
        for (const TSharedPtr<FPanelState>& State : Panels)
        {
            if (State.IsValid() && State->Id == Id)
            {
                return State;
            }
        }
        return nullptr;
    }

    void ApplyDefaultLayout()
    {
        LeftExtent = 504.0f;
        RightExtent = 468.0f;
        TopExtent = 280.0f;
        ActiveLeftTab = TEXT("SolarSystem");
        ActiveRightTab = TEXT("Constellations");
        ActiveTopTab = NAME_None;
        HighestFloatingZOrder = 10;
        for (const TSharedPtr<FPanelState>& State : Panels)
        {
            if (State.IsValid())
            {
                State->bMinimized = false;
            }
        }

        if (TSharedPtr<FPanelState> State = FindPanel(TEXT("Telemetry")))
        {
            State->Side = EDockSide::Left;
            State->LastSide = EDockSide::Left;
            State->bOpen = true;
            State->FloatingPosition = FVector2D(24.0f, 64.0f);
            State->FloatingSize = FVector2D(430.0f, 560.0f);
        }
        if (TSharedPtr<FPanelState> State = FindPanel(TEXT("SolarSystem")))
        {
            State->Side = EDockSide::Left;
            State->LastSide = EDockSide::Left;
            State->bOpen = true;
            State->FloatingPosition = FVector2D(40.0f, 80.0f);
            State->FloatingSize = FVector2D(560.0f, 500.0f);
        }
        if (TSharedPtr<FPanelState> State = FindPanel(TEXT("Constellations")))
        {
            State->Side = EDockSide::Right;
            State->LastSide = EDockSide::Right;
            State->bOpen = true;
            State->FloatingPosition = FVector2D(1050.0f, 64.0f);
            State->FloatingSize = FVector2D(390.0f, 430.0f);
        }
        if (TSharedPtr<FPanelState> State = FindPanel(TEXT("Vectors")))
        {
            State->Side = EDockSide::Right;
            State->LastSide = EDockSide::Right;
            State->bOpen = true;
            State->FloatingPosition = FVector2D(1050.0f, 510.0f);
            State->FloatingSize = FVector2D(390.0f, 430.0f);
        }
        if (TSharedPtr<FPanelState> State = FindPanel(TEXT("Graphics")))
        {
            State->Side = EDockSide::Right;
            State->LastSide = EDockSide::Right;
            State->bOpen = true;
            State->FloatingPosition = FVector2D(560.0f, 90.0f);
            State->FloatingSize = FVector2D(760.0f, 720.0f);
        }
    }

    void LoadLayout()
    {
        if (GConfig == nullptr)
        {
            return;
        }
        int32 SavedLayoutRevision = 0;
        GConfig->GetInt(
            TGVisualizationDockPrivate::LayoutSection,
            TEXT("LayoutRevision"),
            SavedLayoutRevision,
            GGameUserSettingsIni);
        const bool bLoadedLeftExtent = GConfig->GetFloat(
            TGVisualizationDockPrivate::LayoutSection,
            TEXT("LeftExtent"),
            LeftExtent,
            GGameUserSettingsIni);
        const bool bLoadedRightExtent = GConfig->GetFloat(
            TGVisualizationDockPrivate::LayoutSection,
            TEXT("RightExtent"),
            RightExtent,
            GGameUserSettingsIni);
        GConfig->GetFloat(
            TGVisualizationDockPrivate::LayoutSection,
            TEXT("TopExtent"),
            TopExtent,
            GGameUserSettingsIni);

        FString ActiveId = ActiveLeftTab.ToString();
        GConfig->GetString(
            TGVisualizationDockPrivate::LayoutSection,
            TEXT("ActiveLeftTab"),
            ActiveId,
            GGameUserSettingsIni);
        ActiveLeftTab = FName(*ActiveId);
        ActiveId = ActiveRightTab.ToString();
        GConfig->GetString(
            TGVisualizationDockPrivate::LayoutSection,
            TEXT("ActiveRightTab"),
            ActiveId,
            GGameUserSettingsIni);
        ActiveRightTab = FName(*ActiveId);
        ActiveId = ActiveTopTab.ToString();
        GConfig->GetString(
            TGVisualizationDockPrivate::LayoutSection,
            TEXT("ActiveTopTab"),
            ActiveId,
            GGameUserSettingsIni);
        ActiveTopTab = FName(*ActiveId);

        for (const TSharedPtr<FPanelState>& State : Panels)
        {
            const FString Prefix = State->Id.ToString() + TEXT(".");
            int32 Side = static_cast<int32>(State->Side);
            int32 LastSide = static_cast<int32>(State->LastSide);
            int32 MinimizedEdge = static_cast<int32>(State->MinimizedEdge);
            GConfig->GetBool(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("Open")),
                State->bOpen,
                GGameUserSettingsIni);
            GConfig->GetBool(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("Minimized")),
                State->bMinimized,
                GGameUserSettingsIni);
            GConfig->GetInt(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("MinimizedEdge")),
                MinimizedEdge,
                GGameUserSettingsIni);
            GConfig->GetInt(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("Side")),
                Side,
                GGameUserSettingsIni);
            GConfig->GetInt(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("LastSide")),
                LastSide,
                GGameUserSettingsIni);
            GConfig->GetDouble(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("X")),
                State->FloatingPosition.X,
                GGameUserSettingsIni);
            GConfig->GetDouble(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("Y")),
                State->FloatingPosition.Y,
                GGameUserSettingsIni);
            GConfig->GetDouble(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("W")),
                State->FloatingSize.X,
                GGameUserSettingsIni);
            GConfig->GetDouble(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("H")),
                State->FloatingSize.Y,
                GGameUserSettingsIni);
            State->Side = static_cast<EDockSide>(FMath::Clamp(Side, 0, 3));
            State->LastSide = static_cast<EDockSide>(
                FMath::Clamp(LastSide, 0, 3));
            State->MinimizedEdge = static_cast<EMinimizedEdge>(
                FMath::Clamp(MinimizedEdge, 0, 3));
            // Per-panel minimize buttons are retired. Splitter collapse now
            // controls the entire dock, so discard old per-panel minimized state.
            State->bMinimized = false;
            State->FloatingSize.X = FMath::Max(280.0f, State->FloatingSize.X);
            State->FloatingSize.Y = FMath::Max(180.0f, State->FloatingSize.Y);
        }

        const bool bMigrateSideExtents =
            SavedLayoutRevision < TGVisualizationDockPrivate::LayoutRevision;
        if (bMigrateSideExtents)
        {
            // Preserve the saved workspace while widening each existing side
            // dock once by the requested 20 percent.
            if (bLoadedLeftExtent && LeftExtent > 1.0f)
            {
                LeftExtent *= 1.2f;
            }
            if (bLoadedRightExtent && RightExtent > 1.0f)
            {
                RightExtent *= 1.2f;
            }
        }

        LeftExtent = FMath::Clamp(LeftExtent, 0.0f, 1400.0f);
        RightExtent = FMath::Clamp(RightExtent, 0.0f, 1400.0f);
        TopExtent = FMath::Clamp(TopExtent, 0.0f, 900.0f);
        LastExpandedLeftExtent = LeftExtent > 1.0f ? LeftExtent : 504.0f;
        LastExpandedRightExtent = RightExtent > 1.0f ? RightExtent : 468.0f;
        LastExpandedTopExtent = TopExtent > 1.0f ? TopExtent : 280.0f;
        EnsureActiveTab(EDockSide::Left);
        EnsureActiveTab(EDockSide::Right);
        EnsureActiveTab(EDockSide::Top);
        if (bMigrateSideExtents)
        {
            MarkLayoutDirty();
        }
    }

    void MarkLayoutDirty()
    {
        bLayoutDirty = true;
        LastLayoutChangeTime = FPlatformTime::Seconds();
    }

    void SaveLayoutNow()
    {
        if (GConfig == nullptr)
        {
            return;
        }
        GConfig->SetInt(
            TGVisualizationDockPrivate::LayoutSection,
            TEXT("LayoutRevision"),
            TGVisualizationDockPrivate::LayoutRevision,
            GGameUserSettingsIni);
        GConfig->SetFloat(
            TGVisualizationDockPrivate::LayoutSection,
            TEXT("LeftExtent"),
            LeftExtent,
            GGameUserSettingsIni);
        GConfig->SetFloat(
            TGVisualizationDockPrivate::LayoutSection,
            TEXT("RightExtent"),
            RightExtent,
            GGameUserSettingsIni);
        GConfig->SetFloat(
            TGVisualizationDockPrivate::LayoutSection,
            TEXT("TopExtent"),
            TopExtent,
            GGameUserSettingsIni);
        GConfig->SetString(
            TGVisualizationDockPrivate::LayoutSection,
            TEXT("ActiveLeftTab"),
            *ActiveLeftTab.ToString(),
            GGameUserSettingsIni);
        GConfig->SetString(
            TGVisualizationDockPrivate::LayoutSection,
            TEXT("ActiveRightTab"),
            *ActiveRightTab.ToString(),
            GGameUserSettingsIni);
        GConfig->SetString(
            TGVisualizationDockPrivate::LayoutSection,
            TEXT("ActiveTopTab"),
            *ActiveTopTab.ToString(),
            GGameUserSettingsIni);
        for (const TSharedPtr<FPanelState>& State : Panels)
        {
            const FString Prefix = State->Id.ToString() + TEXT(".");
            GConfig->SetBool(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("Open")),
                State->bOpen,
                GGameUserSettingsIni);
            GConfig->SetBool(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("Minimized")),
                State->bMinimized,
                GGameUserSettingsIni);
            GConfig->SetInt(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("MinimizedEdge")),
                static_cast<int32>(State->MinimizedEdge),
                GGameUserSettingsIni);
            GConfig->SetInt(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("Side")),
                static_cast<int32>(State->Side),
                GGameUserSettingsIni);
            GConfig->SetInt(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("LastSide")),
                static_cast<int32>(State->LastSide),
                GGameUserSettingsIni);
            GConfig->SetDouble(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("X")),
                State->FloatingPosition.X,
                GGameUserSettingsIni);
            GConfig->SetDouble(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("Y")),
                State->FloatingPosition.Y,
                GGameUserSettingsIni);
            GConfig->SetDouble(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("W")),
                State->FloatingSize.X,
                GGameUserSettingsIni);
            GConfig->SetDouble(
                TGVisualizationDockPrivate::LayoutSection,
                *(Prefix + TEXT("H")),
                State->FloatingSize.Y,
                GGameUserSettingsIni);
        }
        GConfig->Flush(false, GGameUserSettingsIni);
        bLayoutDirty = false;
    }

    bool HasOpenPanelOnSide(const EDockSide Side) const
    {
        return Panels.ContainsByPredicate(
            [Side](const TSharedPtr<FPanelState>& State)
            {
                return State.IsValid() && State->bOpen &&
                    !State->bMinimized && State->Side == Side;
            });
    }

    FName& GetActiveTab(const EDockSide Side)
    {
        if (Side == EDockSide::Right)
        {
            return ActiveRightTab;
        }
        if (Side == EDockSide::Top)
        {
            return ActiveTopTab;
        }
        return ActiveLeftTab;
    }

    const FName& GetActiveTab(const EDockSide Side) const
    {
        if (Side == EDockSide::Right)
        {
            return ActiveRightTab;
        }
        if (Side == EDockSide::Top)
        {
            return ActiveTopTab;
        }
        return ActiveLeftTab;
    }

    void EnsureActiveTab(const EDockSide Side)
    {
        if (Side == EDockSide::Floating)
        {
            return;
        }
        FName& Active = GetActiveTab(Side);
        const TSharedPtr<FPanelState> Current = FindPanel(Active);
        if (Current.IsValid() && Current->bOpen && !Current->bMinimized &&
            Current->Side == Side)
        {
            return;
        }
        Active = NAME_None;
        for (const TSharedPtr<FPanelState>& State : Panels)
        {
            if (State->bOpen && !State->bMinimized && State->Side == Side)
            {
                Active = State->Id;
                return;
            }
        }
    }

    float& GetEdgeExtent(const EDockSide Side)
    {
        if (Side == EDockSide::Right)
        {
            return RightExtent;
        }
        if (Side == EDockSide::Top)
        {
            return TopExtent;
        }
        return LeftExtent;
    }

    float GetEdgeExtent(const EDockSide Side) const
    {
        if (Side == EDockSide::Right)
        {
            return RightExtent;
        }
        if (Side == EDockSide::Top)
        {
            return TopExtent;
        }
        return LeftExtent;
    }

    float GetDefaultEdgeExtent(const EDockSide Side) const
    {
        return Side == EDockSide::Top ? 280.0f
            : Side == EDockSide::Left ? 504.0f
            : 468.0f;
    }

    float& GetLastExpandedEdgeExtent(const EDockSide Side)
    {
        if (Side == EDockSide::Right)
        {
            return LastExpandedRightExtent;
        }
        if (Side == EDockSide::Top)
        {
            return LastExpandedTopExtent;
        }
        return LastExpandedLeftExtent;
    }

    void EnsureEdgeExpanded(const EDockSide Side)
    {
        if (Side != EDockSide::Floating && GetEdgeExtent(Side) < 1.0f)
        {
            GetEdgeExtent(Side) = GetDefaultEdgeExtent(Side);
        }
    }

    TAttribute<FOptionalSize> MakeEdgeExtentAttribute(
        const EDockSide Side)
    {
        return TAttribute<FOptionalSize>::CreateLambda([this, Side]()
        {
            return FOptionalSize(GetEdgeExtent(Side));
        });
    }

    bool HasMinimizedPanelOnEdge(const EMinimizedEdge Edge) const
    {
        return Panels.ContainsByPredicate(
            [Edge](const TSharedPtr<FPanelState>& State)
            {
                return State.IsValid() && State->bOpen &&
                    State->bMinimized && State->MinimizedEdge == Edge;
            });
    }

    EMinimizedEdge ResolveClosestMinimizedEdge(
        const TSharedPtr<FPanelState>& State) const
    {
        if (!State.IsValid())
        {
            return EMinimizedEdge::Left;
        }
        if (State->Side == EDockSide::Left)
        {
            return EMinimizedEdge::Left;
        }
        if (State->Side == EDockSide::Right)
        {
            return EMinimizedEdge::Right;
        }
        if (State->Side == EDockSide::Top)
        {
            return EMinimizedEdge::Top;
        }

        FVector2D AreaSize = FVector2D(1920.0f, 1080.0f);
        if (LayoutHost.IsValid())
        {
            const FVector2D CachedSize = LayoutHost->GetCachedGeometry().GetLocalSize();
            if (CachedSize.X > 1.0 && CachedSize.Y > 1.0)
            {
                AreaSize = CachedSize;
            }
        }

        const double LeftDistance = FMath::Max(0.0, State->FloatingPosition.X);
        const double RightDistance = FMath::Max(
            0.0,
            AreaSize.X -
                (State->FloatingPosition.X + State->FloatingSize.X));
        const double TopDistance = FMath::Max(0.0, State->FloatingPosition.Y);
        const double BottomDistance = FMath::Max(
            0.0,
            AreaSize.Y -
                (State->FloatingPosition.Y + State->FloatingSize.Y));

        double Closest = LeftDistance;
        EMinimizedEdge Edge = EMinimizedEdge::Left;
        if (RightDistance < Closest)
        {
            Closest = RightDistance;
            Edge = EMinimizedEdge::Right;
        }
        if (TopDistance < Closest)
        {
            Closest = TopDistance;
            Edge = EMinimizedEdge::Top;
        }
        if (BottomDistance < Closest)
        {
            Edge = EMinimizedEdge::Bottom;
        }
        return Edge;
    }

    TSharedRef<SWidget> BuildMinimizedRestoreButton(
        const TSharedPtr<FPanelState>& State)
    {
        using namespace TGVisualizationDockPrivate;
        TSharedPtr<SButton> RestoreButton;
        TSharedRef<SWidget> Result =
            SNew(SBox)
            .WidthOverride(MinimizedTabWidth)
            .HeightOverride(MinimizedTabHeight)
            [
                SAssignNew(RestoreButton, SButton)
                .ButtonStyle(&TGUiTheme::GetButtonStyle(
                    ETGUiButtonStyle::Quiet))
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                .ContentPadding(FMargin(10.0f, 0.0f))
                .ToolTipText(FText::FromString(TEXT("Restore panel")))
                .OnClicked_Lambda([this, PanelId = State->Id]()
                {
                    RestoreMinimizedPanel(PanelId);
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(State->Title)
                    .Justification(ETextJustify::Center)
                    .ColorAndOpacity(Palette().TextPrimary)
                    .Font(TGUiTheme::GetSlateFont(
                        ETGUiTextStyle::FieldLabel))
                ]
            ];
        PanelWidgets.Add(State->Id, RestoreButton);
        return Result;
    }

    TSharedRef<SWidget> BuildMinimizedEdgeStrip(const EMinimizedEdge Edge)
    {
        if (Edge == EMinimizedEdge::Top || Edge == EMinimizedEdge::Bottom)
        {
            TSharedRef<SHorizontalBox> Strip = SNew(SHorizontalBox);
            for (const TSharedPtr<FPanelState>& State : Panels)
            {
                if (State.IsValid() && State->bOpen && State->bMinimized &&
                    State->MinimizedEdge == Edge)
                {
                    Strip->AddSlot()
                    .AutoWidth()
                    .Padding(FMargin(3.0f))
                    [BuildMinimizedRestoreButton(State)];
                }
            }
            return Strip;
        }

        TSharedRef<SVerticalBox> Strip = SNew(SVerticalBox);
        for (const TSharedPtr<FPanelState>& State : Panels)
        {
            if (State.IsValid() && State->bOpen && State->bMinimized &&
                State->MinimizedEdge == Edge)
            {
                Strip->AddSlot()
                .AutoHeight()
                .Padding(FMargin(3.0f))
                [BuildMinimizedRestoreButton(State)];
            }
        }
        return Strip;
    }

    TSharedRef<SWidget> BuildMinimizedPanelsOverlay()
    {
        TSharedRef<SOverlay> Overlay = SNew(SOverlay)
            .Visibility(EVisibility::SelfHitTestInvisible);
        if (HasMinimizedPanelOnEdge(EMinimizedEdge::Top))
        {
            Overlay->AddSlot()
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Top)
            .Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
            [BuildMinimizedEdgeStrip(EMinimizedEdge::Top)];
        }
        if (HasMinimizedPanelOnEdge(EMinimizedEdge::Bottom))
        {
            Overlay->AddSlot()
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Bottom)
            .Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f))
            [BuildMinimizedEdgeStrip(EMinimizedEdge::Bottom)];
        }
        if (HasMinimizedPanelOnEdge(EMinimizedEdge::Left))
        {
            Overlay->AddSlot()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Center)
            .Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
            [BuildMinimizedEdgeStrip(EMinimizedEdge::Left)];
        }
        if (HasMinimizedPanelOnEdge(EMinimizedEdge::Right))
        {
            Overlay->AddSlot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
            [BuildMinimizedEdgeStrip(EMinimizedEdge::Right)];
        }
        return Overlay;
    }

    bool ResolveDockTarget(
        const FVector2D& LocalPosition,
        const FVector2D& AreaSize,
        EDockSide& OutSide) const
    {
        const double LeftDistance = LocalPosition.X;
        const double RightDistance = AreaSize.X - LocalPosition.X;
        const double TopDistance = LocalPosition.Y;

        // Trigger thicknesses are edited at the clearly marked constants near
        // the top of this file: SideDockSnapDistance / TopDockSnapDistance.
        double BestNormalizedDistance = 2.0;
        OutSide = EDockSide::Floating;

        if (LeftDistance <= TGVisualizationDockPrivate::SideDockSnapDistance)
        {
            BestNormalizedDistance = LeftDistance /
                TGVisualizationDockPrivate::SideDockSnapDistance;
            OutSide = EDockSide::Left;
        }
        if (RightDistance <= TGVisualizationDockPrivate::SideDockSnapDistance)
        {
            const double Normalized = RightDistance /
                TGVisualizationDockPrivate::SideDockSnapDistance;
            if (Normalized < BestNormalizedDistance)
            {
                BestNormalizedDistance = Normalized;
                OutSide = EDockSide::Right;
            }
        }
        if (TopDistance <= TGVisualizationDockPrivate::TopDockSnapDistance)
        {
            const double Normalized = TopDistance /
                TGVisualizationDockPrivate::TopDockSnapDistance;
            if (Normalized < BestNormalizedDistance)
            {
                OutSide = EDockSide::Top;
            }
        }

        return OutSide != EDockSide::Floating;
    }

    FMargin GetDockPreviewMargin() const
    {
        if (!bShowDockPreview || DockPreviewSide == EDockSide::Floating)
        {
            return FMargin(0.0f);
        }
        const float AreaWidth = static_cast<float>(DockPreviewAreaSize.X);
        const float AreaHeight = static_cast<float>(DockPreviewAreaSize.Y);
        if (DockPreviewSide == EDockSide::Top)
        {
            const float Height = FMath::Min(
                GetDefaultEdgeExtent(EDockSide::Top),
                AreaHeight * 0.48f);
            return FMargin(0.0f, 0.0f, AreaWidth, Height);
        }
        const float Width = FMath::Min(
            GetDefaultEdgeExtent(DockPreviewSide),
            AreaWidth * 0.48f);
        if (DockPreviewSide == EDockSide::Left)
        {
            return FMargin(0.0f, 0.0f, Width, AreaHeight);
        }
        return FMargin(AreaWidth - Width, 0.0f, Width, AreaHeight);
    }

    TSharedRef<SWidget> BuildDockOverlay()
    {
        TSharedRef<SOverlay> Overlay = SNew(SOverlay)
            .Visibility(EVisibility::SelfHitTestInvisible);
        Overlay->AddSlot()
        .Padding(TAttribute<FMargin>::CreateLambda([this]()
        {
            return FMargin(
                0.0f,
                0.0f,
                0.0f,
                TimelineHeight +
                    TGVisualizationDockPrivate::TimelineBottomMargin);
        }))
        [BuildDockArea()];

        TSharedRef<SConstraintCanvas> FloatingCanvas =
            SNew(SConstraintCanvas)
            .Visibility(EVisibility::SelfHitTestInvisible);
        for (const TSharedPtr<FPanelState>& State : Panels)
        {
            if (!State->bOpen || State->bMinimized ||
                State->Side != EDockSide::Floating)
            {
                continue;
            }
            FloatingCanvas->AddSlot()
            .Anchors(FAnchors(0.0f, 0.0f))
            .Alignment(FVector2D::ZeroVector)
            .Offset(TAttribute<FMargin>::CreateLambda([State]()
            {
                return FMargin(
                    State->FloatingPosition.X,
                    State->FloatingPosition.Y,
                    State->FloatingSize.X,
                    State->FloatingSize.Y);
            }))
            .ZOrder(static_cast<float>(State->FloatingZOrder))
            [
                BuildFloatingPanel(State)
            ];
        }
        Overlay->AddSlot()[FloatingCanvas];

        TSharedRef<SConstraintCanvas> PreviewCanvas =
            SNew(SConstraintCanvas)
            .Visibility(EVisibility::HitTestInvisible);
        PreviewCanvas->AddSlot()
        .Anchors(FAnchors(0.0f, 0.0f))
        .Alignment(FVector2D::ZeroVector)
        .Offset(TAttribute<FMargin>::CreateLambda([this]()
        {
            return GetDockPreviewMargin();
        }))
        [
            SNew(SBox)
            .Visibility_Lambda([this]()
            {
                return bShowDockPreview
                    ? EVisibility::HitTestInvisible
                    : EVisibility::Collapsed;
            })
            [
                SNew(SBorder)
                .BorderImage(&TGVisualizationDockPrivate::DockPreviewBrush())
            ]
        ];
        Overlay->AddSlot()[PreviewCanvas];

        // Fixed native timeline. There is deliberately no EDockSide::Bottom, so
        // movable tabs can never attach to this edge.
        Overlay->AddSlot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Bottom)
        .Padding(FMargin(
            TGVisualizationDockPrivate::TimelineHorizontalMargin,
            0.0f,
            TGVisualizationDockPrivate::TimelineHorizontalMargin,
            TGVisualizationDockPrivate::TimelineBottomMargin))
        [
            SNew(SBox)
            .HeightOverride_Lambda([this]()
            {
                return TimelineHeight;
            })
            [BuildTimelinePanel()]
        ];
        return Overlay;
    }

    ATGSimulationPlaybackActor* GetTimelinePlaybackActor() const
    {
        return Owner.IsValid() ? Owner->GetPlaybackActor() : nullptr;
    }

    FText GetTimelineEtText() const
    {
        const ATGSimulationPlaybackActor* Playback = GetTimelinePlaybackActor();
        return Playback != nullptr && Playback->IsResultLoaded()
            ? FText::FromString(FString::Printf(
                TEXT("ET %.6f"),
                Playback->GetCurrentEphemerisTimeTdbSeconds()))
            : FText::FromString(TEXT("ET --"));
    }

    FText GetTimelineMissionText() const
    {
        const ATGSimulationPlaybackActor* Playback = GetTimelinePlaybackActor();
        return Playback != nullptr && Playback->IsResultLoaded()
            ? FText::FromString(
                TEXT("Mission ") + TGVisualizationDockPrivate::FormatMissionTime(
                    Playback->GetElapsedSeconds()))
            : FText::FromString(TEXT("Mission --"));
    }

    FText GetTimelineUtcText() const
    {
        const ATGSimulationPlaybackActor* Playback = GetTimelinePlaybackActor();
        if (Playback == nullptr || !Playback->IsResultLoaded())
        {
            return FText::FromString(TEXT("UTC --"));
        }
        FString Utc;
        FText Error;
        if (!Playback->GetCurrentUtc(Utc, Error))
        {
            return FText::FromString(TEXT("UTC unavailable"));
        }
        if (!Utc.EndsWith(TEXT("Z"), ESearchCase::IgnoreCase))
        {
            Utc.AppendChar(TEXT('Z'));
        }
        return FText::FromString(Utc);
    }

    FText GetTimelinePlayPauseText() const
    {
        const ATGSimulationPlaybackActor* Playback = GetTimelinePlaybackActor();
        return FText::FromString(
            Playback != nullptr && Playback->IsPlaying() ? TEXT("❚❚") : TEXT("▶"));
    }

    float GetTimelineNormalizedValue() const
    {
        const ATGSimulationPlaybackActor* Playback = GetTimelinePlaybackActor();
        return Playback != nullptr && Playback->IsResultLoaded()
            ? static_cast<float>(FMath::Clamp(
                Playback->GetNormalizedTime(), 0.0, 1.0))
            : 0.0f;
    }

    void InitializeTimelineInputsForPlayback()
    {
        ATGSimulationPlaybackActor* Playback = GetTimelinePlaybackActor();
        if (TimelineInputPlaybackActor.Get() != Playback)
        {
            TimelineInputPlaybackActor = Playback;
            bTimelineInputsInitialized = false;
        }
        if (bTimelineInputsInitialized ||
            Playback == nullptr ||
            !Playback->IsResultLoaded())
        {
            return;
        }

        TimelineMissionInputText = FText::GetEmpty();
        TimelineEtInputText = FText::GetEmpty();
        TimelineUtcInputText = FText::GetEmpty();
        TimelineRateInputText = FText::FromString(TEXT("1"));

        LastValidTimelineMissionInputText = TimelineMissionInputText;
        LastValidTimelineEtInputText = TimelineEtInputText;
        LastValidTimelineUtcInputText = FText::GetEmpty();
        LastValidTimelineRateInputText = TimelineRateInputText;

        if (TimelineMissionInput.IsValid())
        {
            TimelineMissionInput->SetText(TimelineMissionInputText);
        }
        if (TimelineEtInput.IsValid())
        {
            TimelineEtInput->SetText(TimelineEtInputText);
        }
        if (TimelineUtcInput.IsValid())
        {
            TimelineUtcInput->SetUtcText(TimelineUtcInputText, true);
        }
        if (TimelineRateInput.IsValid())
        {
            TimelineRateInput->SetText(TimelineRateInputText);
        }
        bTimelineInputsInitialized = true;
    }

    void RestoreTimelineMissionInput()
    {
        TimelineMissionInputText = LastValidTimelineMissionInputText;
        if (TimelineMissionInput.IsValid())
        {
            TimelineMissionInput->SetText(TimelineMissionInputText);
        }
    }

    void RestoreTimelineEtInput()
    {
        TimelineEtInputText = LastValidTimelineEtInputText;
        if (TimelineEtInput.IsValid())
        {
            TimelineEtInput->SetText(TimelineEtInputText);
        }
    }

    void RestoreTimelineUtcInput()
    {
        TimelineUtcInputText = LastValidTimelineUtcInputText;
        if (TimelineUtcInput.IsValid())
        {
            TimelineUtcInput->SetUtcText(TimelineUtcInputText, true);
        }
    }

    void RestoreTimelineRateInput()
    {
        TimelineRateInputText = LastValidTimelineRateInputText;
        if (TimelineRateInput.IsValid())
        {
            TimelineRateInput->SetText(TimelineRateInputText);
        }
    }

    float GetTimelineStatusRenderOpacity() const
    {
        if (!bTimelineStatusError ||
            TimelineStatusMessage.IsEmpty() ||
            TimelineStatusSetRealSeconds < 0.0)
        {
            return 1.0f;
        }

        const double FadeStart =
            TGVisualizationDockPrivate::TimelineErrorDisplaySeconds -
            TGVisualizationDockPrivate::TimelineErrorFadeSeconds;
        const double StatusAge =
            FPlatformTime::Seconds() - TimelineStatusSetRealSeconds;
        if (StatusAge <= FadeStart)
        {
            return 1.0f;
        }
        return static_cast<float>(FMath::Clamp(
            (TGVisualizationDockPrivate::TimelineErrorDisplaySeconds -
                StatusAge) /
                TGVisualizationDockPrivate::TimelineErrorFadeSeconds,
            0.0,
            1.0));
    }

    void SetTimelineStatus(const FString& Message, const bool bError)
    {
        TimelineStatusMessage = Message;
        bTimelineStatusError = bError;
        TimelineStatusSetRealSeconds = bError && !Message.IsEmpty()
            ? FPlatformTime::Seconds()
            : -1.0;
        Invalidate(EInvalidateWidgetReason::Layout);
    }

    FReply HandleTimelinePlayPauseClicked()
    {
        if (ATGSimulationPlaybackActor* Playback = GetTimelinePlaybackActor())
        {
            Playback->TogglePlayPause();
            RefreshTimeline();
        }
        return FReply::Handled();
    }

    void HandleTimelineSliderChanged(const float NewValue)
    {
        ATGSimulationPlaybackActor* Playback = GetTimelinePlaybackActor();
        if (Playback == nullptr)
        {
            return;
        }
        FText Error;
        if (!Playback->SeekToNormalizedTime(NewValue, Error))
        {
            SetTimelineStatus(Error.ToString(), true);
            return;
        }
        SetTimelineStatus(TEXT(""), false);
    }

    FReply HandleTimelineMissionGoClicked()
    {
        double Value = 0.0;
        ATGSimulationPlaybackActor* Playback = GetTimelinePlaybackActor();
        if (Playback == nullptr ||
            !LexTryParseString(Value, *TimelineMissionInputText.ToString()))
        {
            RestoreTimelineMissionInput();
            SetTimelineStatus(
                TEXT("Mission time must be a numeric number of seconds."), true);
            return FReply::Handled();
        }
        FText Error;
        if (!Playback->SeekToElapsedSeconds(Value, Error))
        {
            RestoreTimelineMissionInput();
            SetTimelineStatus(Error.ToString(), true);
            return FReply::Handled();
        }
        LastValidTimelineMissionInputText = TimelineMissionInputText;
        SetTimelineStatus(TEXT(""), false);
        return FReply::Handled();
    }

    FReply HandleTimelineEtGoClicked()
    {
        double Et = 0.0;
        ATGSimulationPlaybackActor* Playback = GetTimelinePlaybackActor();
        if (Playback == nullptr ||
            !LexTryParseString(Et, *TimelineEtInputText.ToString()))
        {
            RestoreTimelineEtInput();
            SetTimelineStatus(
                TEXT("ET must be numeric TDB seconds past J2000."), true);
            return FReply::Handled();
        }
        FText Error;
        if (!Playback->SeekToElapsedSeconds(
                Et - Playback->GetStartEphemerisTimeTdbSeconds(), Error))
        {
            RestoreTimelineEtInput();
            SetTimelineStatus(Error.ToString(), true);
            return FReply::Handled();
        }
        LastValidTimelineEtInputText = TimelineEtInputText;
        SetTimelineStatus(TEXT(""), false);
        return FReply::Handled();
    }

    FReply HandleTimelineUtcGoClicked()
    {
        ATGSimulationPlaybackActor* Playback = GetTimelinePlaybackActor();
        if (Playback == nullptr)
        {
            RestoreTimelineUtcInput();
            SetTimelineStatus(TEXT("No simulation result is loaded."), true);
            return FReply::Handled();
        }
        double Et = 0.0;
        FString Message;
        if (!FSpiceBridge::ConvertUTCToET(
                TimelineUtcInputText.ToString(), Et, Message))
        {
            RestoreTimelineUtcInput();
            SetTimelineStatus(Message, true);
            return FReply::Handled();
        }
        FText Error;
        if (!Playback->SeekToElapsedSeconds(
                Et - Playback->GetStartEphemerisTimeTdbSeconds(), Error))
        {
            RestoreTimelineUtcInput();
            SetTimelineStatus(Error.ToString(), true);
            return FReply::Handled();
        }
        LastValidTimelineUtcInputText = TimelineUtcInputText;
        SetTimelineStatus(TEXT(""), false);
        return FReply::Handled();
    }

    void HandleTimelinePlaybackRateCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod)
    {
        TimelineRateInputText = Text;
        double Rate = 0.0;
        ATGSimulationPlaybackActor* Playback = GetTimelinePlaybackActor();
        if (Playback == nullptr ||
            !LexTryParseString(Rate, *Text.ToString()) || Rate < 0.0)
        {
            RestoreTimelineRateInput();
            SetTimelineStatus(
                TEXT("Playback rate must be a nonnegative number."), true);
            return;
        }
        Playback->SetPlaybackRate(Rate);
        LastValidTimelineRateInputText = TimelineRateInputText;
        SetTimelineStatus(TEXT(""), false);
    }

    void HandleTimelineResize(const float DeltaScreenY)
    {
        TimelineHeight = FMath::Max(
            TGVisualizationDockPrivate::TimelineCollapsedHeight,
            TimelineHeight - DeltaScreenY);
        Invalidate(EInvalidateWidgetReason::Layout);
    }

    void HandleTimelineResizeFinished()
    {
        if (TimelineHeight <=
            TGVisualizationDockPrivate::TimelineCollapsedHeight +
            TGVisualizationDockPrivate::TimelineCollapseSnapDistance)
        {
            TimelineHeight = TGVisualizationDockPrivate::TimelineCollapsedHeight;
        }
        Invalidate(EInvalidateWidgetReason::Layout);
    }

    void HandleTimelineToggleCollapse()
    {
        if (TimelineHeight <=
            TGVisualizationDockPrivate::TimelineCollapsedHeight + 1.0f)
        {
            TimelineHeight =
                TGVisualizationDockPrivate::TimelineDefaultExpandedHeight;
        }
        else
        {
            TimelineHeight = TGVisualizationDockPrivate::TimelineCollapsedHeight;
        }
        Invalidate(EInvalidateWidgetReason::Layout);
    }

    TSharedRef<SWidget> BuildTimelinePanel()
    {
        using namespace TGVisualizationDockPrivate;
        InitializeTimelineInputsForPlayback();

        const auto MakeReadout = [this](
            TAttribute<FText> Text, const ETextJustify::Type Justification)
            -> TSharedRef<SWidget>
        {
            return SNew(STextBlock)
                .Text(Text)
                .Justification(Justification)
                .ColorAndOpacity(Palette().TextPrimary)
                .Font(TGUiTheme::GetSlateFont(ETGUiTextStyle::Numeric));
        };

        const auto MakeGoButton = [this](
            const FOnClicked& OnClicked) -> TSharedRef<SWidget>
        {
            return SNew(SBox)
                .WidthOverride(TimelineGoButtonWidth)
                .HeightOverride(TimelineControlHeight)
                [
                    SNew(SButton)
                    .ButtonStyle(&TGUiTheme::GetButtonStyle(
                        ETGUiButtonStyle::Secondary))
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    .ContentPadding(FMargin(10.0f, 6.0f))
                    .OnClicked(OnClicked)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Go")))
                        .ColorAndOpacity(Palette().TextPrimary)
                        .Font(TGUiTheme::GetSlateFont(
                            ETGUiTextStyle::FieldLabel))
                    ]
                ];
        };

        FSlateFontInfo TimelinePlayPauseFont =
            TGUiTheme::GetSlateFont(ETGUiTextStyle::BodyStrong);
        TimelinePlayPauseFont.Size = TimelinePlayPauseFontSize;

        TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("TIMELINE")))
					.ColorAndOpacity(Palette().TextPrimary)
					.Font(TGUiTheme::GetSlateFont(
						ETGUiTextStyle::PanelTitle))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(FMargin(16.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return FText::FromString(TimelineStatusMessage);
					})
					.Justification(ETextJustify::Right)
					.ColorAndOpacity_Lambda([this]()
					{
						FLinearColor Color = bTimelineStatusError
							? Palette().Error
							: Palette().TextSecondary;
						Color.A *= GetTimelineStatusRenderOpacity();
						return FSlateColor(Color);
					})
					.Font(TGUiTheme::GetSlateFont(ETGUiTextStyle::Caption))
				]
			]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(FMargin(0.0f, 0.0f, 16.0f, 0.0f))
                [MakeReadout(
                    TAttribute<FText>::CreateSP(
                        this, &STGVisualizationDockWorkspace::GetTimelineEtText),
                    ETextJustify::Left)]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(FMargin(8.0f, 0.0f))
                [MakeReadout(
                    TAttribute<FText>::CreateSP(
                        this, &STGVisualizationDockWorkspace::GetTimelineUtcText),
                    ETextJustify::Center)]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(FMargin(16.0f, 0.0f, 0.0f, 0.0f))
                [MakeReadout(
                    TAttribute<FText>::CreateSP(
                        this, &STGVisualizationDockWorkspace::GetTimelineMissionText),
                    ETextJustify::Right)]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))
            [
                SNew(SSlider)
                .Style(&TGUiTheme::GetSliderStyle())
                .Value(TAttribute<float>::CreateSP(
                    this,
                    &STGVisualizationDockWorkspace::GetTimelineNormalizedValue))
                .StepSize(0.001f)
                .OnValueChanged(
                    this,
                    &STGVisualizationDockWorkspace::HandleTimelineSliderChanged)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, 14.0f, 0.0f))
                [
                    SNew(SBox)
                    .WidthOverride(TimelinePlayPauseButtonWidth)
                    .HeightOverride(TimelineControlHeight)
                    [
                        SNew(SButton)
                        .ButtonStyle(&TGUiTheme::GetButtonStyle(
                            ETGUiButtonStyle::Primary))
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        .ContentPadding(FMargin(0.0f))
                        .OnClicked(
                            this,
                            &STGVisualizationDockWorkspace::HandleTimelinePlayPauseClicked)
                        [
                            SNew(STextBlock)
                            .Text(TAttribute<FText>::CreateSP(
                                this,
                                &STGVisualizationDockWorkspace::GetTimelinePlayPauseText))
                            .Justification(ETextJustify::Center)
                            .ColorAndOpacity(Palette().Canvas)
                            .Font(TimelinePlayPauseFont)
                            .RenderTransform(FSlateRenderTransform(FVector2D(
                                0.0f,
                                TimelinePlayPauseGlyphOffsetY)))
                        ]
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Playback Rate")))
                    .ToolTipText(FText::FromString(
                        TEXT("Simulation seconds advanced per real-time second.")))
                    .ColorAndOpacity(Palette().TextSecondary)
                    .Font(TGUiTheme::GetSlateFont(ETGUiTextStyle::FieldLabel))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, 20.0f, 0.0f))
                [
                    SNew(SBox)
                    .WidthOverride(TimelineRateInputWidth)
                    .HeightOverride(TimelineControlHeight)
                    [
                        SAssignNew(TimelineRateInput, SEditableTextBox)
                        .Style(&TGUiTheme::GetEditableTextBoxStyle())
                        .Text(TimelineRateInputText)
                        .SelectAllTextWhenFocused(true)
                        .SelectAllTextOnCommit(true)
                        .OnTextChanged_Lambda([this](const FText& Text)
                        {
                            TimelineRateInputText = Text;
                        })
                        .OnTextCommitted(
                            this,
                            &STGVisualizationDockWorkspace::HandleTimelinePlaybackRateCommitted)
                    ]
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNullWidget::NullWidget
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
                [
                    SNew(SBox)
                    .WidthOverride(TimelineMissionInputWidth)
                    .HeightOverride(TimelineControlHeight)
                    [
                        SAssignNew(TimelineMissionInput, SEditableTextBox)
                        .Style(&TGUiTheme::GetEditableTextBoxStyle())
                        .Text(TimelineMissionInputText)
                        .HintText(FText::FromString(TEXT("Mission seconds")))
                        .SelectAllTextWhenFocused(true)
                        .SelectAllTextOnCommit(true)
                        .OnTextChanged_Lambda([this](const FText& Text)
                        {
                            TimelineMissionInputText = Text;
                        })
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, 20.0f, 0.0f))
                [MakeGoButton(FOnClicked::CreateSP(
                    this,
                    &STGVisualizationDockWorkspace::HandleTimelineMissionGoClicked))]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
                [
                    SNew(SBox)
                    .WidthOverride(TimelineEtInputWidth)
                    .HeightOverride(TimelineControlHeight)
                    [
                        SAssignNew(TimelineEtInput, SEditableTextBox)
                        .Style(&TGUiTheme::GetEditableTextBoxStyle())
                        .Text(TimelineEtInputText)
                        .HintText(FText::FromString(TEXT("ET")))
                        .SelectAllTextWhenFocused(true)
                        .SelectAllTextOnCommit(true)
                        .OnTextChanged_Lambda([this](const FText& Text)
                        {
                            TimelineEtInputText = Text;
                        })
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, 20.0f, 0.0f))
                [MakeGoButton(FOnClicked::CreateSP(
                    this,
                    &STGVisualizationDockWorkspace::HandleTimelineEtGoClicked))]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
                [
                    SNew(SBox)
                    .WidthOverride(TimelineUtcInputWidth)
                    .HeightOverride(TimelineControlHeight)
                    [
                        SAssignNew(TimelineUtcInput, STGUtcDateTimeInput)
                        .Style(&TGUiTheme::GetEditableTextBoxStyle())
                        .Text(TimelineUtcInputText)
                        .HintText(FText::FromString(TEXT("UTC")))
                        .OnTextChanged_Lambda([this](const FText& Text)
                        {
                            TimelineUtcInputText = Text;
                        })
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [MakeGoButton(FOnClicked::CreateSP(
                    this,
                    &STGVisualizationDockWorkspace::HandleTimelineUtcGoClicked))]
            ];

        TSharedPtr<SBorder> PanelBorder;
        TSharedRef<SWidget> Result =
            SAssignNew(PanelBorder, SBorder)
            .BorderImage(&PanelFrameBrush())
            .Padding(FMargin(0.0f))
            .Clipping(EWidgetClipping::ClipToBounds)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STimelineResizeHandle)
                    .Collapsed_Lambda([this]()
                    {
                        return TimelineHeight <= TimelineCollapsedHeight + 1.0f;
                    })
                    .OnResize(
                        this,
                        &STGVisualizationDockWorkspace::HandleTimelineResize)
                    .OnResizeFinished(FSimpleDelegate::CreateSP(
                        this,
                        &STGVisualizationDockWorkspace::HandleTimelineResizeFinished))
                    .OnToggleCollapse(FSimpleDelegate::CreateSP(
                        this,
                        &STGVisualizationDockWorkspace::HandleTimelineToggleCollapse))
                ]
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .Padding(FMargin(16.0f, 12.0f, 16.0f, 14.0f))
                [
                    SNew(SBox)
                    .Visibility_Lambda([this]()
                    {
                        return TimelineHeight <= TimelineCollapsedHeight + 1.0f
                            ? EVisibility::Collapsed
                            : EVisibility::Visible;
                    })
                    [Body]
                ]
            ];
        TimelinePanelWidget = PanelBorder;
        return Result;
    }

    TSharedRef<SWidget> BuildDockArea()
    {
        TSharedRef<SVerticalBox> Outer = SNew(SVerticalBox)
            .Visibility(EVisibility::SelfHitTestInvisible);
        if (HasOpenPanelOnSide(EDockSide::Top))
        {
            Outer->AddSlot().AutoHeight()
            [
                SNew(SBox)
                .HeightOverride(MakeEdgeExtentAttribute(EDockSide::Top))
                [BuildDockedTabGroup(EDockSide::Top)]
            ];
            Outer->AddSlot().AutoHeight()
            [BuildEdgeResizeHandle(EDockSide::Top)];
        }
        Outer->AddSlot().FillHeight(1.0f)
        [BuildMiddleDock()];
        return Outer;
    }

    TSharedRef<SWidget> BuildMiddleDock()
    {
        TSharedRef<SHorizontalBox> Middle = SNew(SHorizontalBox)
            .Visibility(EVisibility::SelfHitTestInvisible);
        const bool bLeft = HasOpenPanelOnSide(EDockSide::Left);
        const bool bRight = HasOpenPanelOnSide(EDockSide::Right);
        if (bLeft)
        {
            Middle->AddSlot().AutoWidth()
            [
                SNew(SBox)
                .WidthOverride(MakeEdgeExtentAttribute(EDockSide::Left))
                [BuildDockedTabGroup(EDockSide::Left)]
            ];
            Middle->AddSlot().AutoWidth()
            [BuildEdgeResizeHandle(EDockSide::Left)];
        }
        Middle->AddSlot().FillWidth(1.0f)
        [
            SNew(SBox)
            .Visibility(EVisibility::SelfHitTestInvisible)
        ];
        if (bRight)
        {
            Middle->AddSlot().AutoWidth()
            [BuildEdgeResizeHandle(EDockSide::Right)];
            Middle->AddSlot().AutoWidth()
            [
                SNew(SBox)
                .WidthOverride(MakeEdgeExtentAttribute(EDockSide::Right))
                [BuildDockedTabGroup(EDockSide::Right)]
            ];
        }
        return Middle;
    }

    TSharedRef<SWidget> BuildEdgeResizeHandle(const EDockSide Side)
    {
        return SNew(TGVisualizationDockPrivate::SDockEdgeResizeHandle)
            .Side(Side)
            .Collapsed_Lambda([this, Side]()
            {
                return GetEdgeExtent(Side) <= 1.0f;
            })
            .OnResize(
                this,
                &STGVisualizationDockWorkspace::HandleDockEdgeResize)
            .OnResizeFinished(
                this,
                &STGVisualizationDockWorkspace::HandleDockEdgeResizeFinished)
            .OnToggleCollapse(
                this,
                &STGVisualizationDockWorkspace::HandleDockEdgeToggleCollapse);
    }

    TSharedRef<SWidget> BuildDockedTabGroup(const EDockSide Side)
    {
        using namespace TGVisualizationDockPrivate;
        EnsureActiveTab(Side);

        TArray<TSharedPtr<FPanelState>> SidePanels;
        for (const TSharedPtr<FPanelState>& State : Panels)
        {
            if (State->bOpen && !State->bMinimized && State->Side == Side)
            {
                SidePanels.Add(State);
            }
        }

        TSharedRef<SHorizontalBox> TabBar = SNew(SHorizontalBox);
        TSharedRef<SOverlay> ContentStack = SNew(SOverlay);
        for (const TSharedPtr<FPanelState>& State : SidePanels)
        {
            TabBar->AddSlot().AutoWidth()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SPanelDragHandle)
                    .PanelId(State->Id)
                    .Title(State->Title)
                    .DragPreviewSize(State->FloatingSize)
                    .Active_Lambda([this, Side, PanelId = State->Id]()
                    {
                        return GetActiveTab(Side) == PanelId;
                    })
                    .OnActivated(FSimpleDelegate::CreateSP(
                        this,
                        &STGVisualizationDockWorkspace::ActivatePanel,
                        State->Id))
                ]
                + SHorizontalBox::Slot().AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(36.0f)
                    .HeightOverride(40.0f)
                    .Visibility_Lambda([this, Side]()
                    {
                        // A left/right dock narrower than the complete tab header
                        // clips the X region into an isolated square. Hide only
                        // that close region until there is enough horizontal room.
                        return Side == EDockSide::Top ||
                            GetEdgeExtent(Side) >= MinSideDockExtentForCloseButton
                            ? EVisibility::Visible
                            : EVisibility::Collapsed;
                    })
                    [
                        SNew(SBorder)
                        .BorderImage(TAttribute<const FSlateBrush*>::CreateLambda(
                            [this, Side, PanelId = State->Id]()
                            {
                                return GetActiveTab(Side) == PanelId
                                    ? &ActiveTabCloseShapeBrush()
                                    : &InactiveTabCloseShapeBrush();
                            }))
                        .BorderBackgroundColor(FLinearColor::White)
                        .Padding(FMargin(0.0f))
                        [
                            SNew(SButton)
                            .ButtonStyle(&TabCloseButtonStyle())
                            .ToolTipText(FText::FromString(TEXT("Close panel")))
                            .ContentPadding(FMargin(0.0f))
                            .OnClicked_Lambda([this, PanelId = State->Id]()
                            {
                                ClosePanel(PanelId);
                                return FReply::Handled();
                            })
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("x")))
                                .Justification(ETextJustify::Center)
                                .ColorAndOpacity(Palette().TextSecondary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::FieldLabel))
                            ]
                        ]
                    ]
                ]
            ];

            ContentStack->AddSlot()
            [
                SNew(SBorder)
                .Visibility_Lambda([this, Side, PanelId = State->Id]()
                {
                    return GetActiveTab(Side) == PanelId
                        ? EVisibility::Visible
                        : EVisibility::Collapsed;
                })
                .BorderImage(&PanelFillBrush())
                .Padding(FMargin(PanelContentPadding))
                .Clipping(EWidgetClipping::ClipToBounds)
                [State->Content.ToSharedRef()]
            ];
        }

        TSharedPtr<SBorder> GroupBorder;
        SAssignNew(GroupBorder, SBorder)
        .BorderImage(&PanelFrameBrush())
        .Padding(FMargin(0.0f))
        .Clipping(EWidgetClipping::ClipToBounds)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SBorder)
                .BorderImage(&HeaderFillBrush())
                .Padding(FMargin(4.0f, 0.0f))
                [
                    SNew(SScrollBox)
                    .Style(&TGUiTheme::GetScrollBoxStyle())
                    .ScrollBarStyle(&TGUiTheme::GetScrollBarStyle())
                    .Orientation(Orient_Horizontal)
                    // The cyan/yellow diagnostic proved the old white square was
                    // the horizontal scrollbar thumb + track. Keep tab scrolling,
                    // but remove that scrollbar from layout/rendering entirely.
                    .ScrollBarVisibility(EVisibility::Collapsed)
                    + SScrollBox::Slot()
                    [TabBar]
                ]
            ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [ContentStack]
        ];
        for (const TSharedPtr<FPanelState>& State : SidePanels)
        {
            PanelWidgets.Add(State->Id, GroupBorder);
        }
        return GroupBorder.ToSharedRef();
    }

    TSharedRef<SWidget> BuildFloatingPanel(
        const TSharedPtr<FPanelState>& State)
    {
        using namespace TGVisualizationDockPrivate;
        TSharedPtr<SBorder> PanelBorder;
        SAssignNew(PanelBorder, SBorder)
        .BorderImage(&FloatingPanelFrameBrush())
        .Padding(FMargin(0.0f))
        .Clipping(EWidgetClipping::ClipToBounds)
        [
            SNew(SOverlay)
            + SOverlay::Slot()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SBorder)
                    .BorderImage(&FloatingHeaderFillBrush())
                    // EDIT HERE: outer floating-header padding. Keep left/right
                    // at 0 so the tab is flush left and the close button flush right.
                    .Padding(FMargin(0.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        [
                            SNew(SPanelDragHandle)
                            .PanelId(State->Id)
                            .Title(State->Title)
                            .DragPreviewSize(State->FloatingSize)
                            .Active(true)
                            .OnActivated(FSimpleDelegate::CreateSP(
                                this,
                                &STGVisualizationDockWorkspace::ActivatePanel,
                                State->Id))
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                            .WidthOverride(36.0f)
                            .HeightOverride(40.0f)
                            [
                                SNew(SBorder)
                                .BorderImage(&ActiveTabCloseShapeBrush())
                                .BorderBackgroundColor(FLinearColor::White)
                                .Padding(FMargin(0.0f))
                                [
                                    SNew(SButton)
                                    .ButtonStyle(&TabCloseButtonStyle())
                                    .ToolTipText(FText::FromString(
                                        TEXT("Close panel")))
                                    .ContentPadding(FMargin(0.0f))
                                    .OnClicked_Lambda(
                                        [this, PanelId = State->Id]()
                                        {
                                            ClosePanel(PanelId);
                                            return FReply::Handled();
                                        })
                                    [
                                        SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("x")))
                                        .Justification(ETextJustify::Center)
                                        .ColorAndOpacity(Palette().TextSecondary)
                                        .Font(TGUiTheme::GetSlateFont(
                                            ETGUiTextStyle::FieldLabel))
                                    ]
                                ]
                            ]
                        ]
                    ]
                ]
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNew(SBorder)
                    .BorderImage(&PanelFillBrush())
                    .Padding(FMargin(PanelContentPadding))
                    [State->Content.ToSharedRef()]
                ]
            ]
            + SOverlay::Slot()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Fill)
            [
                SNew(SBox)
                .WidthOverride(FloatingResizeHitThickness)
                [
                    SNew(SFloatingResizeHandle)
                    .PanelId(State->Id)
                    .Edge(EFloatingResizeEdge::Left)
                    .OnResize(this,
                        &STGVisualizationDockWorkspace::HandleFloatingPanelResize)
                ]
            ]
            + SOverlay::Slot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Fill)
            [
                SNew(SBox)
                .WidthOverride(FloatingResizeHitThickness)
                [
                    SNew(SFloatingResizeHandle)
                    .PanelId(State->Id)
                    .Edge(EFloatingResizeEdge::Right)
                    .OnResize(this,
                        &STGVisualizationDockWorkspace::HandleFloatingPanelResize)
                ]
            ]
            + SOverlay::Slot()
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Top)
            [
                SNew(SBox)
                .HeightOverride(FloatingResizeHitThickness)
                [
                    SNew(SFloatingResizeHandle)
                    .PanelId(State->Id)
                    .Edge(EFloatingResizeEdge::Top)
                    .OnResize(this,
                        &STGVisualizationDockWorkspace::HandleFloatingPanelResize)
                ]
            ]
            + SOverlay::Slot()
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Bottom)
            [
                SNew(SBox)
                .HeightOverride(FloatingResizeHitThickness)
                [
                    SNew(SFloatingResizeHandle)
                    .PanelId(State->Id)
                    .Edge(EFloatingResizeEdge::Bottom)
                    .OnResize(this,
                        &STGVisualizationDockWorkspace::HandleFloatingPanelResize)
                ]
            ]
            + SOverlay::Slot()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Top)
            [
                SNew(SBox)
                .WidthOverride(FloatingResizeCornerSize)
                .HeightOverride(FloatingResizeCornerSize)
                [
                    SNew(SFloatingResizeHandle)
                    .PanelId(State->Id)
                    .Edge(EFloatingResizeEdge::TopLeft)
                    .OnResize(this,
                        &STGVisualizationDockWorkspace::HandleFloatingPanelResize)
                ]
            ]
            + SOverlay::Slot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Top)
            [
                SNew(SBox)
                .WidthOverride(FloatingResizeCornerSize)
                .HeightOverride(FloatingResizeCornerSize)
                [
                    SNew(SFloatingResizeHandle)
                    .PanelId(State->Id)
                    .Edge(EFloatingResizeEdge::TopRight)
                    .OnResize(this,
                        &STGVisualizationDockWorkspace::HandleFloatingPanelResize)
                ]
            ]
            + SOverlay::Slot()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Bottom)
            [
                SNew(SBox)
                .WidthOverride(FloatingResizeCornerSize)
                .HeightOverride(FloatingResizeCornerSize)
                [
                    SNew(SFloatingResizeHandle)
                    .PanelId(State->Id)
                    .Edge(EFloatingResizeEdge::BottomLeft)
                    .OnResize(this,
                        &STGVisualizationDockWorkspace::HandleFloatingPanelResize)
                ]
            ]
            + SOverlay::Slot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Bottom)
            [
                SNew(SBox)
                .WidthOverride(FloatingResizeCornerSize)
                .HeightOverride(FloatingResizeCornerSize)
                [
                    SNew(SFloatingResizeHandle)
                    .PanelId(State->Id)
                    .Edge(EFloatingResizeEdge::BottomRight)
                    .OnResize(this,
                        &STGVisualizationDockWorkspace::HandleFloatingPanelResize)
                ]
            ]
        ];
        PanelWidgets.Add(State->Id, PanelBorder);
        return PanelBorder.ToSharedRef();
    }

    void RebuildLayout()
    {
        if (!LayoutHost.IsValid())
        {
            return;
        }
        // Detach panel contents before reparenting them into the new dock tree.
        LayoutHost->SetContent(SNullWidget::NullWidget);
        PanelWidgets.Reset();
        LayoutHost->SetContent(BuildDockOverlay());
    }

    void HandlePanelDragOver(
        const FName PanelId,
        const FVector2D LocalPosition,
        const FVector2D AreaSize,
        const FVector2D GrabOffset)
    {
        const TSharedPtr<FPanelState> State = FindPanel(PanelId);
        bool bDetachedFromDock = false;
        if (State.IsValid())
        {
            if (State->Side != EDockSide::Floating)
            {
                const EDockSide PreviousSide = State->Side;
                State->Side = EDockSide::Floating;
                State->bMinimized = false;
                State->FloatingZOrder = ++HighestFloatingZOrder;
                EnsureActiveTab(PreviousSide);
                bDetachedFromDock = true;
            }

            // Move the REAL panel continuously instead of leaving the original
            // static and showing a separate drag placeholder.
            State->FloatingPosition.X = FMath::Clamp(
                LocalPosition.X - GrabOffset.X,
                0.0,
                FMath::Max(0.0, AreaSize.X - State->FloatingSize.X));
            State->FloatingPosition.Y = FMath::Clamp(
                LocalPosition.Y - GrabOffset.Y,
                0.0,
                FMath::Max(0.0, AreaSize.Y - State->FloatingSize.Y));

            if (bDetachedFromDock)
            {
                // Rebuild only once, at detachment, so the old docked tab
                // disappears and this same panel becomes the moving window.
                RebuildLayout();
            }
            else
            {
                Invalidate(EInvalidateWidgetReason::Layout);
            }
            MarkLayoutDirty();
        }

        EDockSide TargetSide = EDockSide::Floating;
        const bool bHasDockTarget = ResolveDockTarget(
            LocalPosition,
            AreaSize,
            TargetSide);
        const bool bChanged = bShowDockPreview != bHasDockTarget ||
            DockPreviewSide != TargetSide ||
            !DockPreviewAreaSize.Equals(AreaSize, 0.5);
        bShowDockPreview = bHasDockTarget;
        DockPreviewSide = TargetSide;
        DockPreviewAreaSize = AreaSize;
        if (bChanged)
        {
            Invalidate(EInvalidateWidgetReason::Paint);
        }
    }

    void HandlePanelDragLeave()
    {
        if (!bShowDockPreview)
        {
            return;
        }
        bShowDockPreview = false;
        DockPreviewSide = EDockSide::Floating;
        Invalidate(EInvalidateWidgetReason::Paint);
    }

    void HandlePanelDropped(
        const FName PanelId,
        const FVector2D LocalPosition,
        const FVector2D AreaSize,
        const FVector2D GrabOffset)
    {
        bShowDockPreview = false;
        DockPreviewSide = EDockSide::Floating;

        TSharedPtr<FPanelState> State = FindPanel(PanelId);
        if (!State.IsValid())
        {
            return;
        }

        const EDockSide PreviousSide = State->Side;
        EDockSide TargetSide = EDockSide::Floating;
        if (ResolveDockTarget(LocalPosition, AreaSize, TargetSide))
        {
            State->Side = TargetSide;
            GetActiveTab(State->Side) = State->Id;
            EnsureEdgeExpanded(State->Side);
        }
        else
        {
            State->Side = EDockSide::Floating;
            State->FloatingPosition.X = FMath::Clamp(
                LocalPosition.X - GrabOffset.X,
                0.0,
                FMath::Max(0.0, AreaSize.X - State->FloatingSize.X));
            State->FloatingPosition.Y = FMath::Clamp(
                LocalPosition.Y - GrabOffset.Y,
                0.0,
                FMath::Max(0.0, AreaSize.Y - State->FloatingSize.Y));
            State->FloatingZOrder = ++HighestFloatingZOrder;
        }
        if (PreviousSide != EDockSide::Floating &&
            PreviousSide != State->Side)
        {
            EnsureActiveTab(PreviousSide);
        }
        State->LastSide = State->Side;
        State->bOpen = true;
        State->bMinimized = false;
        MarkLayoutDirty();
        RebuildLayout();
    }

    void HandleFloatingPanelResize(
        const FName PanelId,
        const EFloatingResizeEdge Edge,
        const FVector2D Delta)
    {
        const TSharedPtr<FPanelState> State = FindPanel(PanelId);
        if (!State.IsValid() || State->Side != EDockSide::Floating ||
            State->bMinimized)
        {
            return;
        }

        FVector2D AreaSize = FVector2D(1920.0f, 1080.0f);
        if (LayoutHost.IsValid())
        {
            const FVector2D CachedSize = LayoutHost->GetCachedGeometry().GetLocalSize();
            if (CachedSize.X > 1.0 && CachedSize.Y > 1.0)
            {
                AreaSize = CachedSize;
            }
        }

        const bool bLeft = Edge == EFloatingResizeEdge::Left ||
            Edge == EFloatingResizeEdge::TopLeft ||
            Edge == EFloatingResizeEdge::BottomLeft;
        const bool bRight = Edge == EFloatingResizeEdge::Right ||
            Edge == EFloatingResizeEdge::TopRight ||
            Edge == EFloatingResizeEdge::BottomRight;
        const bool bTop = Edge == EFloatingResizeEdge::Top ||
            Edge == EFloatingResizeEdge::TopLeft ||
            Edge == EFloatingResizeEdge::TopRight;
        const bool bBottom = Edge == EFloatingResizeEdge::Bottom ||
            Edge == EFloatingResizeEdge::BottomLeft ||
            Edge == EFloatingResizeEdge::BottomRight;

        constexpr double MinWidth = 280.0;
        constexpr double MinHeight = 180.0;
        constexpr double MaxWidth = 1400.0;
        constexpr double MaxHeight = 1000.0;

        if (bLeft)
        {
            const double Right = State->FloatingPosition.X +
                State->FloatingSize.X;
            const double MinimumX = FMath::Max(0.0, Right - MaxWidth);
            const double MaximumX = FMath::Max(MinimumX, Right - MinWidth);
            State->FloatingPosition.X = FMath::Clamp(
                State->FloatingPosition.X + Delta.X,
                MinimumX,
                MaximumX);
            State->FloatingSize.X = Right - State->FloatingPosition.X;
        }
        else if (bRight)
        {
            const double MaximumWidth = FMath::Max(
                MinWidth,
                FMath::Min(
                    MaxWidth,
                    AreaSize.X - State->FloatingPosition.X));
            State->FloatingSize.X = FMath::Clamp(
                State->FloatingSize.X + Delta.X,
                MinWidth,
                MaximumWidth);
        }

        if (bTop)
        {
            const double Bottom = State->FloatingPosition.Y +
                State->FloatingSize.Y;
            const double MinimumY = FMath::Max(0.0, Bottom - MaxHeight);
            const double MaximumY = FMath::Max(MinimumY, Bottom - MinHeight);
            State->FloatingPosition.Y = FMath::Clamp(
                State->FloatingPosition.Y + Delta.Y,
                MinimumY,
                MaximumY);
            State->FloatingSize.Y = Bottom - State->FloatingPosition.Y;
        }
        else if (bBottom)
        {
            const double MaximumHeight = FMath::Max(
                MinHeight,
                FMath::Min(
                    MaxHeight,
                    AreaSize.Y - State->FloatingPosition.Y));
            State->FloatingSize.Y = FMath::Clamp(
                State->FloatingSize.Y + Delta.Y,
                MinHeight,
                MaximumHeight);
        }

        Invalidate(EInvalidateWidgetReason::Layout);
        MarkLayoutDirty();
    }

    void ActivatePanel(const FName PanelId)
    {
        const TSharedPtr<FPanelState> State = FindPanel(PanelId);
        if (!State.IsValid())
        {
            return;
        }
        if (State->Side != EDockSide::Floating)
        {
            GetActiveTab(State->Side) = State->Id;
        }
        else
        {
            State->FloatingZOrder = ++HighestFloatingZOrder;
        }
        Invalidate(EInvalidateWidgetReason::Layout);
        MarkLayoutDirty();
    }

    void HandleDockEdgeResize(const EDockSide Side, const float Delta)
    {
        if (Side == EDockSide::Floating)
        {
            return;
        }
        const float Maximum = Side == EDockSide::Top ? 900.0f : 1400.0f;
        GetEdgeExtent(Side) = FMath::Clamp(
            GetEdgeExtent(Side) + Delta,
            0.0f,
            Maximum);
        Invalidate(EInvalidateWidgetReason::Layout);
        MarkLayoutDirty();
    }

    void HandleDockEdgeResizeFinished(const EDockSide Side)
    {
        if (Side == EDockSide::Floating)
        {
            return;
        }

        if (GetEdgeExtent(Side) <=
            TGVisualizationDockPrivate::EdgeCollapseSnapPixels)
        {
            GetEdgeExtent(Side) = 0.0f;
            Invalidate(EInvalidateWidgetReason::Layout);
        }
        else
        {
            GetLastExpandedEdgeExtent(Side) = GetEdgeExtent(Side);
        }
        MarkLayoutDirty();
    }

    void HandleDockEdgeToggleCollapse(const EDockSide Side)
    {
        if (Side == EDockSide::Floating)
        {
            return;
        }

        float& Extent = GetEdgeExtent(Side);
        if (Extent > 1.0f)
        {
            GetLastExpandedEdgeExtent(Side) = Extent;
            Extent = 0.0f;
        }
        else
        {
            Extent = FMath::Max(1.0f, GetLastExpandedEdgeExtent(Side));
        }
        Invalidate(EInvalidateWidgetReason::Layout);
        MarkLayoutDirty();
    }

    void MinimizePanel(const FName PanelId)
    {
        const TSharedPtr<FPanelState> State = FindPanel(PanelId);
        if (!State.IsValid() || !State->bOpen)
        {
            return;
        }
        State->MinimizedEdge = ResolveClosestMinimizedEdge(State);
        State->bMinimized = true;
        if (State->Side != EDockSide::Floating)
        {
            EnsureActiveTab(State->Side);
        }
        MarkLayoutDirty();
        RebuildLayout();
    }

    void RestoreMinimizedPanel(const FName PanelId)
    {
        const TSharedPtr<FPanelState> State = FindPanel(PanelId);
        if (!State.IsValid() || !State->bOpen)
        {
            return;
        }
        State->bMinimized = false;
        if (State->Side == EDockSide::Floating)
        {
            State->FloatingZOrder = ++HighestFloatingZOrder;
        }
        else
        {
            GetActiveTab(State->Side) = State->Id;
            EnsureEdgeExpanded(State->Side);
        }
        MarkLayoutDirty();
        RebuildLayout();
    }

    void ClosePanel(const FName PanelId)
    {
        if (const TSharedPtr<FPanelState> State = FindPanel(PanelId))
        {
            State->LastSide = State->Side;
            State->bOpen = false;
            State->bMinimized = false;
            if (State->Side != EDockSide::Floating)
            {
                EnsureActiveTab(State->Side);
            }
            MarkLayoutDirty();
            RebuildLayout();
        }
    }

    void SetPanelOpen(const FName PanelId, const bool bOpen)
    {
        if (const TSharedPtr<FPanelState> State = FindPanel(PanelId))
        {
            State->bOpen = bOpen;
            if (bOpen)
            {
                State->bMinimized = false;
                State->Side = State->LastSide;
                State->FloatingZOrder = ++HighestFloatingZOrder;
                if (State->Side != EDockSide::Floating)
                {
                    GetActiveTab(State->Side) = State->Id;
                    EnsureEdgeExpanded(State->Side);
                }
            }
            else
            {
                State->bMinimized = false;
                State->LastSide = State->Side;
            }
            MarkLayoutDirty();
            RebuildLayout();
        }
    }

    FReply HandleWindowMenuClicked()
    {
        if (OptionsMenuAnchor.IsValid())
        {
            OptionsMenuAnchor->SetIsOpen(false, false);
        }
        if (WindowMenuAnchor.IsValid())
        {
            WindowMenuAnchor->SetIsOpen(
                !WindowMenuAnchor->IsOpen(),
                false);
        }
        return FReply::Handled();
    }

    FReply HandleOptionsMenuClicked()
    {
        if (WindowMenuAnchor.IsValid())
        {
            WindowMenuAnchor->SetIsOpen(false, false);
        }
        if (OptionsMenuAnchor.IsValid())
        {
            OptionsMenuAnchor->SetIsOpen(
                !OptionsMenuAnchor->IsOpen(),
                false);
        }
        return FReply::Handled();
    }

    TSharedRef<SWidget> BuildWindowMenu()
    {
        using namespace TGVisualizationDockPrivate;
        TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
        for (const TSharedPtr<FPanelState>& State : Panels)
        {
            Menu->AddSlot()
            .AutoHeight()
            [
                SNew(SBox)
                .MinDesiredHeight(44.0f)
                [
                    SNew(SCheckBox)
                    .Style(&TGUiTheme::GetCheckBoxStyle())
                    .Padding(FMargin(10.0f, 8.0f))
                    .IsChecked_Lambda([State]()
                    {
                        return State->bOpen
                            ? ECheckBoxState::Checked
                            : ECheckBoxState::Unchecked;
                    })
                    .OnCheckStateChanged_Lambda(
                        [this, PanelId = State->Id](
                            const ECheckBoxState Value)
                        {
                            SetPanelOpen(
                                PanelId,
                                Value == ECheckBoxState::Checked);
                        })
                    [
                        SNew(STextBlock)
                        .Text(State->Title)
                        .ColorAndOpacity(Palette().TextPrimary)
                        .Font(TGUiTheme::GetSlateFont(
                            ETGUiTextStyle::FieldLabel))
                    ]
                ]
            ];
        }
        Menu->AddSlot()
        .AutoHeight()
        .Padding(FMargin(8.0f, 4.0f))
        [
            SNew(SBox)
            .HeightOverride(1.0f)
            [
                SNew(SBorder)
                .BorderImage(&TintableRoundedBrush())
                .BorderBackgroundColor(Palette().Border)
            ]
        ];
        Menu->AddSlot()
        .AutoHeight()
        .Padding(FMargin(4.0f))
        [
            SNew(SBox)
            .HeightOverride(40.0f)
            [
                SNew(SButton)
                .ButtonStyle(&TGUiTheme::GetButtonStyle(
                    ETGUiButtonStyle::Secondary))
                .ContentPadding(FMargin(0.0f))
				.VAlign(VAlign_Center)
                .OnClicked_Lambda([this]()
                {
                    ApplyDefaultLayout();
                    MarkLayoutDirty();
                    RebuildLayout();
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Reset Layout")))
                    .ColorAndOpacity(Palette().TextPrimary)
                    .Font(TGUiTheme::GetSlateFont(
                        ETGUiTextStyle::FieldLabel))
                    .Justification(ETextJustify::Center)
                ]
            ]
        ];
        return SNew(SBox)
            .MinDesiredWidth(220.0f)
            [
                SNew(SBorder)
                .BorderImage(&PanelFrameBrush())
                .Padding(FMargin(8.0f))
                [Menu]
            ];
    }

    TSharedRef<SWidget> BuildOptionsMenu()
    {
        using namespace TGVisualizationDockPrivate;
        constexpr double MinimumFovDegrees = 20.0;
        constexpr double MaximumFovDegrees = 120.0;
        constexpr double MinimumOrbitSensitivity = 0.05;
        constexpr double MaximumOrbitSensitivity = 2.0;
        constexpr float TrajectoryOptionColumnWidth = 145.0f;

        const TWeakObjectPtr<UTGVisualizationDockWorkspaceWidget> WeakOwner =
            Owner;

        return SNew(SBox)
            .WidthOverride(330.0f)
            [
                SNew(SBorder)
                .BorderImage(&PanelFrameBrush())
                .Padding(FMargin(12.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SBox)
                        .MinDesiredHeight(42.0f)
                        [
                            SNew(SCheckBox)
                            .Style(&TGUiTheme::GetCheckBoxStyle())
                            .Padding(FMargin(8.0f, 6.0f))
                            .IsChecked_Lambda([WeakOwner]()
                            {
                                const auto* Widget = WeakOwner.Get();
                                const auto* Playback = Widget != nullptr
                                    ? Widget->GetPlaybackActor()
                                    : nullptr;
                                return Playback != nullptr &&
                                    Playback->AreBodiesEmissive()
                                        ? ECheckBoxState::Checked
                                        : ECheckBoxState::Unchecked;
                            })
                            .OnCheckStateChanged_Lambda(
                                [WeakOwner](const ECheckBoxState State)
                                {
                                    auto* Widget = WeakOwner.Get();
                                    auto* Playback = Widget != nullptr
                                        ? Widget->GetPlaybackActor()
                                        : nullptr;
                                    if (Playback != nullptr)
                                    {
                                        Playback->SetBodiesEmissive(
                                            State ==
                                                ECheckBoxState::Checked);
                                    }
                                })
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(
                                    TEXT("Bodies Emissive")))
                                .ColorAndOpacity(Palette().TextPrimary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::FieldLabel))
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SBox)
                        .MinDesiredHeight(42.0f)
                        [
                            SNew(SCheckBox)
                            .Style(&TGUiTheme::GetCheckBoxStyle())
                            .Padding(FMargin(8.0f, 6.0f))
                            .IsChecked_Lambda([WeakOwner]()
                            {
                                const auto* Widget = WeakOwner.Get();
                                const auto* Playback = Widget != nullptr
                                    ? Widget->GetPlaybackActor()
                                    : nullptr;
                                return Playback != nullptr &&
                                    Playback->IsSpacecraftEmissive()
                                        ? ECheckBoxState::Checked
                                        : ECheckBoxState::Unchecked;
                            })
                            .OnCheckStateChanged_Lambda(
                                [WeakOwner](const ECheckBoxState State)
                                {
                                    auto* Widget = WeakOwner.Get();
                                    auto* Playback = Widget != nullptr
                                        ? Widget->GetPlaybackActor()
                                        : nullptr;
                                    if (Playback != nullptr)
                                    {
                                        Playback->SetSpacecraftEmissive(
                                            State ==
                                                ECheckBoxState::Checked);
                                    }
                                })
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(
                                    TEXT("Spacecraft Emissive")))
                                .ColorAndOpacity(Palette().TextPrimary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::FieldLabel))
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(4.0f, 8.0f))
                    [
                        SNew(SBox)
                        .HeightOverride(1.0f)
                        [
                            SNew(SBorder)
                            .BorderImage(&TintableRoundedBrush())
                            .BorderBackgroundColor(Palette().Border)
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(8.0f, 4.0f, 8.0f, 8.0f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Camera FOV")))
                                .ColorAndOpacity(Palette().TextPrimary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::FieldLabel))
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(STextBlock)
                                .Text_Lambda([WeakOwner]()
                                {
                                    const auto* Widget = WeakOwner.Get();
                                    const auto* Playback = Widget != nullptr
                                        ? Widget->GetPlaybackActor()
                                        : nullptr;
                                    const double Value = Playback != nullptr
                                        ? Playback->
                                            GetVisualizationCameraFovDegrees()
                                        : 55.0;
                                    return FText::FromString(FString::Printf(
                                        TEXT("%.0f deg"),
                                        Value));
                                })
                                .ColorAndOpacity(Palette().TextSecondary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::Numeric))
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SBox)
                            .HeightOverride(28.0f)
                            [
                                SNew(SSlider)
                                .Style(&TGUiTheme::GetSliderStyle())
                                .StepSize(0.01f)
                                .Value_Lambda([
                                    WeakOwner,
                                    MinimumFovDegrees,
                                    MaximumFovDegrees]()
                                {
                                    const auto* Widget = WeakOwner.Get();
                                    const auto* Playback = Widget != nullptr
                                        ? Widget->GetPlaybackActor()
                                        : nullptr;
                                    const double Value = Playback != nullptr
                                        ? Playback->
                                            GetVisualizationCameraFovDegrees()
                                        : 55.0;
                                    return static_cast<float>(FMath::Clamp(
                                        (Value - MinimumFovDegrees) /
                                            (MaximumFovDegrees -
                                                MinimumFovDegrees),
                                        0.0,
                                        1.0));
                                })
                                .OnValueChanged_Lambda(
                                    [
                                        WeakOwner,
                                        MinimumFovDegrees,
                                        MaximumFovDegrees](
                                            const float NormalizedValue)
                                    {
                                        auto* Widget = WeakOwner.Get();
                                        auto* Playback = Widget != nullptr
                                            ? Widget->GetPlaybackActor()
                                            : nullptr;
                                        if (Playback != nullptr)
                                        {
                                            Playback->
                                                SetVisualizationCameraFovDegrees(
                                                    MinimumFovDegrees +
                                                    static_cast<double>(
                                                        NormalizedValue) *
                                                    (MaximumFovDegrees -
                                                        MinimumFovDegrees));
                                        }
                                    })
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(8.0f, 4.0f, 8.0f, 8.0f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(
                                    TEXT("Camera Sensitivity")))
                                .ColorAndOpacity(Palette().TextPrimary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::FieldLabel))
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(STextBlock)
                                .Text_Lambda([WeakOwner]()
                                {
                                    const auto* Widget = WeakOwner.Get();
                                    const auto* Playback = Widget != nullptr
                                        ? Widget->GetPlaybackActor()
                                        : nullptr;
                                    const double Value = Playback != nullptr
                                        ? Playback->
                                            GetVisualizationOrbitSensitivity()
                                        : 1.0;
                                    return FText::FromString(FString::Printf(
                                        TEXT("%.2f deg/px"),
                                        Value));
                                })
                                .ColorAndOpacity(Palette().TextSecondary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::Numeric))
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SBox)
                            .HeightOverride(28.0f)
                            [
                                SNew(SSlider)
                                .Style(&TGUiTheme::GetSliderStyle())
                                .StepSize(0.01f)
                                .Value_Lambda([
                                    WeakOwner,
                                    MinimumOrbitSensitivity,
                                    MaximumOrbitSensitivity]()
                                {
                                    const auto* Widget = WeakOwner.Get();
                                    const auto* Playback = Widget != nullptr
                                        ? Widget->GetPlaybackActor()
                                        : nullptr;
                                    const double Value = Playback != nullptr
                                        ? Playback->
                                            GetVisualizationOrbitSensitivity()
                                        : 1.0;
                                    return static_cast<float>(FMath::Clamp(
                                        (Value - MinimumOrbitSensitivity) /
                                            (MaximumOrbitSensitivity -
                                                MinimumOrbitSensitivity),
                                        0.0,
                                        1.0));
                                })
                                .OnValueChanged_Lambda(
                                    [
                                        WeakOwner,
                                        MinimumOrbitSensitivity,
                                        MaximumOrbitSensitivity](
                                            const float NormalizedValue)
                                    {
                                        auto* Widget = WeakOwner.Get();
                                        auto* Playback = Widget != nullptr
                                            ? Widget->GetPlaybackActor()
                                            : nullptr;
                                        if (Playback != nullptr)
                                        {
                                            Playback->
                                                SetVisualizationOrbitSensitivity(
                                                    MinimumOrbitSensitivity +
                                                    static_cast<double>(
                                                        NormalizedValue) *
                                                    (MaximumOrbitSensitivity -
                                                        MinimumOrbitSensitivity));
                                        }
                                    })
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(4.0f, 8.0f))
                    [
                        SNew(SBox)
                        .HeightOverride(1.0f)
                        [
                            SNew(SBorder)
                            .BorderImage(&TintableRoundedBrush())
                            .BorderBackgroundColor(Palette().Border)
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(8.0f, 4.0f, 8.0f, 8.0f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(
                                    TEXT("Trajectory Range")))
                                .ColorAndOpacity(Palette().TextPrimary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::FieldLabel))
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("CSV time")))
                                .ColorAndOpacity(Palette().TextSecondary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::Caption))
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SBox)
                            .HeightOverride(28.0f)
                            [
                                SNew(STGRangeSlider)
                                .IsEnabled_Lambda([WeakOwner]()
                                {
                                    const auto* Widget = WeakOwner.Get();
                                    const auto* Playback = Widget != nullptr
                                        ? Widget->GetPlaybackActor()
                                        : nullptr;
                                    return Playback != nullptr &&
                                        Playback->IsResultLoaded();
                                })
                                .LowerValue_Lambda([WeakOwner]()
                                {
                                    const auto* Widget = WeakOwner.Get();
                                    const auto* Playback = Widget != nullptr
                                        ? Widget->GetPlaybackActor()
                                        : nullptr;
                                    return Playback != nullptr
                                        ? static_cast<float>(Playback->
                                            GetLocalTrajectoryDisplayStartNormalized())
                                        : 0.0f;
                                })
                                .UpperValue_Lambda([WeakOwner]()
                                {
                                    const auto* Widget = WeakOwner.Get();
                                    const auto* Playback = Widget != nullptr
                                        ? Widget->GetPlaybackActor()
                                        : nullptr;
                                    return Playback != nullptr
                                        ? static_cast<float>(Playback->
                                            GetLocalTrajectoryDisplayEndNormalized())
                                        : 1.0f;
                                })
                                .OnRangeChanged_Lambda(
                                    [WeakOwner](
                                        const float StartNormalized,
                                        const float EndNormalized)
                                    {
                                        auto* Widget = WeakOwner.Get();
                                        auto* Playback = Widget != nullptr
                                            ? Widget->GetPlaybackActor()
                                            : nullptr;
                                        if (Playback != nullptr)
                                        {
                                            Playback->
                                                SetLocalTrajectoryDisplayRangeNormalized(
                                                    StartNormalized,
                                                    EndNormalized);
                                            Widget->RefreshSolarSystemOverviewGraphs();
                                        }
                                    })
                                .ToolTipText(FText::FromString(
                                    TEXT("Select the first and last CSV times shown by the local trajectory.")))
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            [
                                SNew(STextBlock)
                                .Text_Lambda([WeakOwner]()
                                {
                                    const auto* Widget = WeakOwner.Get();
                                    const auto* Playback = Widget != nullptr
                                        ? Widget->GetPlaybackActor()
                                        : nullptr;
                                    if (
                                        Playback == nullptr ||
                                        !Playback->IsResultLoaded())
                                    {
                                        return FText::FromString(
                                            TEXT("Start --"));
                                    }
                                    const double MissionSeconds =
                                        Playback->GetDurationSeconds() *
                                        Playback->
                                            GetLocalTrajectoryDisplayStartNormalized();
                                    return FText::FromString(
                                        TEXT("Start ") +
                                        FormatMissionTime(MissionSeconds));
                                })
                                .ColorAndOpacity(Palette().TextSecondary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::Caption))
                            ]
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .HAlign(HAlign_Right)
                            [
                                SNew(STextBlock)
                                .Text_Lambda([WeakOwner]()
                                {
                                    const auto* Widget = WeakOwner.Get();
                                    const auto* Playback = Widget != nullptr
                                        ? Widget->GetPlaybackActor()
                                        : nullptr;
                                    if (
                                        Playback == nullptr ||
                                        !Playback->IsResultLoaded())
                                    {
                                        return FText::FromString(TEXT("End --"));
                                    }
                                    const double MissionSeconds =
                                        Playback->GetDurationSeconds() *
                                        Playback->
                                            GetLocalTrajectoryDisplayEndNormalized();
                                    return FText::FromString(
                                        TEXT("End ") +
                                        FormatMissionTime(MissionSeconds));
                                })
                                .ColorAndOpacity(Palette().TextSecondary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::Caption))
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FMargin(0.0f, 14.0f, 0.0f, 4.0f))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(
                                TEXT("Trajectory Color")))
                            .ColorAndOpacity(Palette().TextPrimary)
                            .Font(TGUiTheme::GetSlateFont(
                                ETGUiTextStyle::FieldLabel))
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SBox)
                                .WidthOverride(TrajectoryOptionColumnWidth)
                                .HAlign(HAlign_Left)
                                [
                                    SNew(SCheckBox)
                                    .Style(&TGUiTheme::GetCheckBoxStyle())
                                    .Padding(FMargin(4.0f, 5.0f))
                                    .IsChecked_Lambda([WeakOwner]()
                                    {
                                        const auto* Widget = WeakOwner.Get();
                                        const auto* Playback = Widget != nullptr
                                            ? Widget->GetPlaybackActor()
                                            : nullptr;
                                        return Playback != nullptr &&
                                            Playback->GetLocalTrajectoryColorMode() ==
                                                ETGTrajectoryColorMode::White
                                            ? ECheckBoxState::Checked
                                            : ECheckBoxState::Unchecked;
                                    })
                                    .OnCheckStateChanged_Lambda(
                                        [WeakOwner](const ECheckBoxState State)
                                        {
                                            if (State != ECheckBoxState::Checked)
                                            {
                                                return;
                                            }
                                            auto* Widget = WeakOwner.Get();
                                            auto* Playback = Widget != nullptr
                                                ? Widget->GetPlaybackActor()
                                                : nullptr;
                                            if (Playback != nullptr)
                                            {
                                                Playback->SetLocalTrajectoryColorMode(
                                                    ETGTrajectoryColorMode::White);
                                            }
                                        })
                                    [
                                        SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("White")))
                                        .ColorAndOpacity(Palette().TextPrimary)
                                        .Font(TGUiTheme::GetSlateFont(
                                            ETGUiTextStyle::FieldLabel))
                                    ]
                                ]
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SBox)
                                .WidthOverride(TrajectoryOptionColumnWidth)
                                .HAlign(HAlign_Left)
                                [
                                    SNew(SCheckBox)
                                    .Style(&TGUiTheme::GetCheckBoxStyle())
                                    .Padding(FMargin(4.0f, 5.0f))
                                    .IsChecked_Lambda([WeakOwner]()
                                    {
                                        const auto* Widget = WeakOwner.Get();
                                        const auto* Playback = Widget != nullptr
                                            ? Widget->GetPlaybackActor()
                                            : nullptr;
                                        return Playback != nullptr &&
                                            Playback->GetLocalTrajectoryColorMode() ==
                                                ETGTrajectoryColorMode::Red
                                            ? ECheckBoxState::Checked
                                            : ECheckBoxState::Unchecked;
                                    })
                                    .OnCheckStateChanged_Lambda(
                                        [WeakOwner](const ECheckBoxState State)
                                        {
                                            if (State != ECheckBoxState::Checked)
                                            {
                                                return;
                                            }
                                            auto* Widget = WeakOwner.Get();
                                            auto* Playback = Widget != nullptr
                                                ? Widget->GetPlaybackActor()
                                                : nullptr;
                                            if (Playback != nullptr)
                                            {
                                                Playback->SetLocalTrajectoryColorMode(
                                                    ETGTrajectoryColorMode::Red);
                                            }
                                        })
                                    [
                                        SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("Red")))
                                        .ColorAndOpacity(Palette().TextPrimary)
                                        .Font(TGUiTheme::GetSlateFont(
                                            ETGUiTextStyle::FieldLabel))
                                    ]
                                ]
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FMargin(0.0f, 12.0f, 0.0f, 4.0f))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(
                                TEXT("Trajectory Frame")))
                            .ColorAndOpacity(Palette().TextPrimary)
                            .Font(TGUiTheme::GetSlateFont(
                                ETGUiTextStyle::FieldLabel))
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SBox)
                                .WidthOverride(TrajectoryOptionColumnWidth)
                                .HAlign(HAlign_Left)
                                [
                                    SNew(SCheckBox)
                                    .Style(&TGUiTheme::GetCheckBoxStyle())
                                    .Padding(FMargin(4.0f, 5.0f))
                                    .IsChecked_Lambda([WeakOwner]()
                                    {
                                        const auto* Widget = WeakOwner.Get();
                                        const auto* Playback = Widget != nullptr
                                            ? Widget->GetPlaybackActor()
                                            : nullptr;
                                        return Playback != nullptr &&
                                            Playback->GetLocalTrajectoryFrameMode() ==
                                                ETGTrajectoryFrameMode::Icrf
                                            ? ECheckBoxState::Checked
                                            : ECheckBoxState::Unchecked;
                                    })
                                    .OnCheckStateChanged_Lambda(
                                        [WeakOwner](const ECheckBoxState State)
                                        {
                                            auto* Widget = WeakOwner.Get();
                                            auto* Playback = Widget != nullptr
                                                ? Widget->GetPlaybackActor()
                                                : nullptr;
                                            if (Playback == nullptr)
                                            {
                                                return;
                                            }
                                            if (State == ECheckBoxState::Checked)
                                            {
                                                Playback->SetLocalTrajectoryFrameMode(
                                                    ETGTrajectoryFrameMode::Icrf);
                                            }
                                            else if (
                                                Playback->GetLocalTrajectoryFrameMode() ==
                                                    ETGTrajectoryFrameMode::Icrf)
                                            {
                                                Playback->SetLocalTrajectoryFrameMode(
                                                    ETGTrajectoryFrameMode::Hidden);
                                            }
                                        })
                                    [
                                        SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("ICRF")))
                                        .ColorAndOpacity(Palette().TextPrimary)
                                        .Font(TGUiTheme::GetSlateFont(
                                            ETGUiTextStyle::FieldLabel))
                                    ]
                                ]
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SBox)
                                .WidthOverride(TrajectoryOptionColumnWidth)
                                .HAlign(HAlign_Left)
                                [
                                    SNew(SCheckBox)
                                    .Style(&TGUiTheme::GetCheckBoxStyle())
                                    .Padding(FMargin(4.0f, 5.0f))
                                    .IsChecked_Lambda([WeakOwner]()
                                    {
                                        const auto* Widget = WeakOwner.Get();
                                        const auto* Playback = Widget != nullptr
                                            ? Widget->GetPlaybackActor()
                                            : nullptr;
                                        return Playback != nullptr &&
                                            Playback->GetLocalTrajectoryFrameMode() ==
                                                ETGTrajectoryFrameMode::ClosestBody
                                            ? ECheckBoxState::Checked
                                            : ECheckBoxState::Unchecked;
                                    })
                                    .OnCheckStateChanged_Lambda(
                                        [WeakOwner](const ECheckBoxState State)
                                        {
                                            auto* Widget = WeakOwner.Get();
                                            auto* Playback = Widget != nullptr
                                                ? Widget->GetPlaybackActor()
                                                : nullptr;
                                            if (Playback == nullptr)
                                            {
                                                return;
                                            }
                                            if (State == ECheckBoxState::Checked)
                                            {
                                                Playback->SetLocalTrajectoryFrameMode(
                                                    ETGTrajectoryFrameMode::ClosestBody);
                                            }
                                            else if (
                                                Playback->GetLocalTrajectoryFrameMode() ==
                                                    ETGTrajectoryFrameMode::ClosestBody)
                                            {
                                                Playback->SetLocalTrajectoryFrameMode(
                                                    ETGTrajectoryFrameMode::Hidden);
                                            }
                                        })
                                    [
                                        SNew(STextBlock)
                                        .Text(FText::FromString(
                                            TEXT("Closest Body")))
                                        .ColorAndOpacity(Palette().TextPrimary)
                                        .Font(TGUiTheme::GetSlateFont(
                                            ETGUiTextStyle::FieldLabel))
                                    ]
                                ]
                            ]
                        ]
                    ]
                ]
            ];
    }

    TSharedPtr<FString> FindGraphicsOption(
        const TArray<TSharedPtr<FString>>& Options,
        const FString& Value) const
    {
        for (const TSharedPtr<FString>& Option : Options)
        {
            if (Option.IsValid() && *Option == Value)
            {
                return Option;
            }
        }
        return nullptr;
    }

    TSharedPtr<FString> GetGraphicsSelection(
        const EGraphicsAxis Axis) const
    {
        switch (Axis)
        {
        case EGraphicsAxis::X:
            return GraphicsXColumn;
        case EGraphicsAxis::Y:
            return GraphicsYColumn;
        case EGraphicsAxis::Z:
        default:
            return GraphicsZColumn;
        }
    }

    double GetGraphicsScale(const EGraphicsAxis Axis) const
    {
        switch (Axis)
        {
        case EGraphicsAxis::X:
            return GraphicsXScale;
        case EGraphicsAxis::Y:
            return GraphicsYScale;
        case EGraphicsAxis::Z:
        default:
            return GraphicsZScale;
        }
    }

    TSharedPtr<SEditableTextBox> GetGraphicsScaleInput(
        const EGraphicsAxis Axis) const
    {
        switch (Axis)
        {
        case EGraphicsAxis::X:
            return GraphicsXScaleInput;
        case EGraphicsAxis::Y:
            return GraphicsYScaleInput;
        case EGraphicsAxis::Z:
        default:
            return GraphicsZScaleInput;
        }
    }

    void SyncGraphicsScaleInputs()
    {
        const auto SyncInput = [this](const EGraphicsAxis Axis)
        {
            if (const TSharedPtr<SEditableTextBox> Input =
                    GetGraphicsScaleInput(Axis))
            {
                Input->SetText(FText::FromString(
                    TGVisualizationDockPrivate::FormatGraphicsScale(
                        GetGraphicsScale(Axis))));
            }
        };
        SyncInput(EGraphicsAxis::X);
        SyncInput(EGraphicsAxis::Y);
        SyncInput(EGraphicsAxis::Z);
    }

    static double ComputeAutomaticGraphicsScale(
        const double Minimum,
        const double Maximum)
    {
        if (!FMath::IsFinite(Minimum) || !FMath::IsFinite(Maximum))
        {
            return 1.0;
        }

        const double PeakMagnitude = FMath::Max(
            FMath::Abs(Minimum),
            FMath::Abs(Maximum));
        const double Span = Maximum - Minimum;
        double CharacteristicMagnitude =
            FMath::IsFinite(Span) && Span > 0.0
                ? Span
                : PeakMagnitude;

        // Nearly constant columns use their absolute level instead of a
        // numerically insignificant range caused by floating-point noise.
        if (CharacteristicMagnitude <= PeakMagnitude * 1.0e-12)
        {
            CharacteristicMagnitude = PeakMagnitude;
        }
        if (!FMath::IsFinite(CharacteristicMagnitude) ||
            CharacteristicMagnitude <= 0.0)
        {
            return 1.0;
        }

        const int32 OrderOfMagnitude = FMath::Clamp(
            FMath::FloorToInt(FMath::LogX(10.0, CharacteristicMagnitude)),
            -300,
            300);
        const double Scale = FMath::Pow(
            10.0,
            static_cast<double>(-OrderOfMagnitude));
        return FMath::IsFinite(Scale) && Scale != 0.0 ? Scale : 1.0;
    }

    void ApplyAutomaticGraphicsScales(
        const FVector& Minimum,
        const FVector& Maximum,
        const bool bHasZ)
    {
        GraphicsXScale = ComputeAutomaticGraphicsScale(
            Minimum.X,
            Maximum.X);
        GraphicsYScale = ComputeAutomaticGraphicsScale(
            Minimum.Y,
            Maximum.Y);
        GraphicsZScale = bHasZ
            ? ComputeAutomaticGraphicsScale(Minimum.Z, Maximum.Z)
            : 1.0;
        SyncGraphicsScaleInputs();
    }

    void RefreshGraphicsColumnOptions()
    {
        const FString PreviousX = GraphicsXColumn.IsValid()
            ? *GraphicsXColumn
            : FString();
        const FString PreviousY = GraphicsYColumn.IsValid()
            ? *GraphicsYColumn
            : FString();
        const FString PreviousZ = GraphicsZColumn.IsValid()
            ? *GraphicsZColumn
            : FString(TEXT("None"));

        GraphicsColumnOptions.Reset();
        GraphicsZColumnOptions.Reset();
        GraphicsZColumnOptions.Add(MakeShared<FString>(TEXT("None")));

        TArray<FString> ColumnNames;
        if (const UTGVisualizationDockWorkspaceWidget* OwnerWidget =
                Owner.Get())
        {
            if (const ATGSimulationPlaybackActor* Playback =
                    OwnerWidget->GetPlaybackActor())
            {
                Playback->GetResultNumericColumnNames(ColumnNames);
            }
        }
        for (const FString& ColumnName : ColumnNames)
        {
            GraphicsColumnOptions.Add(MakeShared<FString>(ColumnName));
            GraphicsZColumnOptions.Add(MakeShared<FString>(ColumnName));
        }

        GraphicsXColumn = FindGraphicsOption(
            GraphicsColumnOptions,
            PreviousX);
        GraphicsYColumn = FindGraphicsOption(
            GraphicsColumnOptions,
            PreviousY);
        GraphicsZColumn = FindGraphicsOption(
            GraphicsZColumnOptions,
            PreviousZ);

        if (!GraphicsXColumn.IsValid())
        {
            GraphicsXColumn = FindGraphicsOption(
                GraphicsColumnOptions,
                TEXT("position_icrf_x_m"));
            if (!GraphicsXColumn.IsValid() &&
                !GraphicsColumnOptions.IsEmpty())
            {
                GraphicsXColumn = GraphicsColumnOptions[0];
            }
        }
        if (!GraphicsYColumn.IsValid())
        {
            GraphicsYColumn = FindGraphicsOption(
                GraphicsColumnOptions,
                TEXT("position_icrf_y_m"));
            if (!GraphicsYColumn.IsValid() &&
                GraphicsColumnOptions.Num() >= 2)
            {
                GraphicsYColumn = GraphicsColumnOptions[1];
            }
            else if (!GraphicsYColumn.IsValid() &&
                !GraphicsColumnOptions.IsEmpty())
            {
                GraphicsYColumn = GraphicsColumnOptions[0];
            }
        }
        if (!GraphicsZColumn.IsValid())
        {
            GraphicsZColumn = GraphicsZColumnOptions[0];
        }

        bRefreshingGraphicsControls = true;
        const auto RefreshCombo = [](const auto& Combo, const auto& Selection)
        {
            if (Combo.IsValid())
            {
                Combo->RefreshOptions();
                Combo->SetSelectedItem(Selection);
            }
        };
        RefreshCombo(GraphicsXComboBox, GraphicsXColumn);
        RefreshCombo(GraphicsYComboBox, GraphicsYColumn);
        RefreshCombo(GraphicsZComboBox, GraphicsZColumn);
        bRefreshingGraphicsControls = false;

        SyncGraphicsTimeInputs();
        RefreshGraphicsPlotData(true);
    }

    void HandleGraphicsColumnSelection(
        const EGraphicsAxis Axis,
        const TSharedPtr<FString>& Selection)
    {
        if (!Selection.IsValid())
        {
            return;
        }
        switch (Axis)
        {
        case EGraphicsAxis::X:
            GraphicsXColumn = Selection;
            break;
        case EGraphicsAxis::Y:
            GraphicsYColumn = Selection;
            break;
        case EGraphicsAxis::Z:
            GraphicsZColumn = Selection;
            break;
        }
        if (!bRefreshingGraphicsControls)
        {
            RefreshGraphicsPlotData(true);
        }
    }

    void CommitGraphicsScale(
        const EGraphicsAxis Axis,
        const FText& Text)
    {
        double Value = 0.0;
        if (!LexTryParseString(Value, *Text.ToString()) ||
            !FMath::IsFinite(Value) ||
            Value == 0.0)
        {
            if (const TSharedPtr<SEditableTextBox> Input =
                    GetGraphicsScaleInput(Axis))
            {
                Input->SetText(FText::FromString(
                    TGVisualizationDockPrivate::FormatGraphicsScale(
                        GetGraphicsScale(Axis))));
            }
            return;
        }

        switch (Axis)
        {
        case EGraphicsAxis::X:
            GraphicsXScale = Value;
            break;
        case EGraphicsAxis::Y:
            GraphicsYScale = Value;
            break;
        case EGraphicsAxis::Z:
            GraphicsZScale = Value;
            break;
        }
        if (const TSharedPtr<SEditableTextBox> Input =
                GetGraphicsScaleInput(Axis))
        {
            Input->SetText(FText::FromString(
                TGVisualizationDockPrivate::FormatGraphicsScale(Value)));
        }
        RefreshGraphicsPlotData();
    }

    void SyncGraphicsTimeInputs()
    {
        double Duration = 0.0;
        if (const UTGVisualizationDockWorkspaceWidget* OwnerWidget =
                Owner.Get())
        {
            if (const ATGSimulationPlaybackActor* Playback =
                    OwnerWidget->GetPlaybackActor())
            {
                Duration = Playback->GetDurationSeconds();
            }
        }
        GraphicsStartTimeText = FText::FromString(
            TGVisualizationDockPrivate::FormatNumber(
                Duration * GraphicsStartNormalized));
        GraphicsEndTimeText = FText::FromString(
            TGVisualizationDockPrivate::FormatNumber(
                Duration * GraphicsEndNormalized));
        if (GraphicsStartTimeInput.IsValid())
        {
            GraphicsStartTimeInput->SetText(GraphicsStartTimeText);
        }
        if (GraphicsEndTimeInput.IsValid())
        {
            GraphicsEndTimeInput->SetText(GraphicsEndTimeText);
        }
    }

    void CommitGraphicsTimeBoundary(
        const bool bStart,
        const FText& Text)
    {
        const UTGVisualizationDockWorkspaceWidget* OwnerWidget = Owner.Get();
        const ATGSimulationPlaybackActor* Playback = OwnerWidget != nullptr
            ? OwnerWidget->GetPlaybackActor()
            : nullptr;
        const double Duration = Playback != nullptr
            ? Playback->GetDurationSeconds()
            : 0.0;
        double MissionSeconds = 0.0;
        if (
            Duration <= 0.0 ||
            !LexTryParseString(MissionSeconds, *Text.ToString()) ||
            !FMath::IsFinite(MissionSeconds))
        {
            SyncGraphicsTimeInputs();
            return;
        }

        const double Normalized = FMath::Clamp(
            MissionSeconds / Duration,
            0.0,
            1.0);
        if (bStart)
        {
            GraphicsStartNormalized = FMath::Min(
                Normalized,
                GraphicsEndNormalized);
        }
        else
        {
            GraphicsEndNormalized = FMath::Max(
                Normalized,
                GraphicsStartNormalized);
        }
        SyncGraphicsTimeInputs();
        RefreshGraphicsPlotData();
    }

    void RefreshGraphicsPlotData(const bool bAutoScale = false)
    {
        if (!GraphicsPlot.IsValid())
        {
            return;
        }

        TArray<FVector> Samples;
        FVector Minimum = FVector::ZeroVector;
        FVector Maximum = FVector::ZeroVector;
        bool bHasBounds = false;
        const FString XColumn = GraphicsXColumn.IsValid()
            ? *GraphicsXColumn
            : FString();
        const FString YColumn = GraphicsYColumn.IsValid()
            ? *GraphicsYColumn
            : FString();
        const FString ZColumn =
            GraphicsZColumn.IsValid() && *GraphicsZColumn != TEXT("None")
                ? *GraphicsZColumn
                : FString();
        const UTGVisualizationDockWorkspaceWidget* OwnerWidget = Owner.Get();
        const ATGSimulationPlaybackActor* Playback = OwnerWidget != nullptr
            ? OwnerWidget->GetPlaybackActor()
            : nullptr;
        if (Playback != nullptr && !XColumn.IsEmpty() && !YColumn.IsEmpty())
        {
            bHasBounds = Playback->BuildResultNumericPlotSamples(
                XColumn,
                YColumn,
                ZColumn,
                GraphicsStartNormalized,
                GraphicsEndNormalized,
                Samples,
                Minimum,
                Maximum);
        }
        if (bAutoScale && bHasBounds)
        {
            ApplyAutomaticGraphicsScales(
                Minimum,
                Maximum,
                !ZColumn.IsEmpty());
        }
        GraphicsPlot->SetPlotData(
            Samples,
            XColumn,
            YColumn,
            ZColumn,
            GraphicsXScale,
            GraphicsYScale,
            GraphicsZScale,
            GraphicsStartNormalized,
            GraphicsEndNormalized);
    }

    TSharedRef<SWidget> BuildGraphicsAxisControl(
        const EGraphicsAxis Axis)
    {
        using namespace TGVisualizationDockPrivate;

        const FString AxisName = Axis == EGraphicsAxis::X
            ? TEXT("X")
            : (Axis == EGraphicsAxis::Y ? TEXT("Y") : TEXT("Z"));
        TArray<TSharedPtr<FString>>* Options = Axis == EGraphicsAxis::Z
            ? &GraphicsZColumnOptions
            : &GraphicsColumnOptions;
        TSharedPtr<SComboBox<TSharedPtr<FString>>>* Combo =
            Axis == EGraphicsAxis::X
                ? &GraphicsXComboBox
                : (Axis == EGraphicsAxis::Y
                    ? &GraphicsYComboBox
                    : &GraphicsZComboBox);
        TSharedPtr<SEditableTextBox>* ScaleInput =
            Axis == EGraphicsAxis::X
                ? &GraphicsXScaleInput
                : (Axis == EGraphicsAxis::Y
                    ? &GraphicsYScaleInput
                    : &GraphicsZScaleInput);

        return SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, 5.0f))
            [
                SNew(STextBlock)
                .Text(FText::FromString(AxisName + TEXT("-Data")))
                .Justification(ETextJustify::Left)
                .ColorAndOpacity(Palette().TextPrimary)
                .Font(TGUiTheme::GetSlateFont(
                    ETGUiTextStyle::FieldLabel))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                .HeightOverride(CompactControlHeight)
                [
                    SAssignNew(*Combo, SComboBox<TSharedPtr<FString>>)
                    .ComboBoxStyle(&GraphicsComboBoxStyle())
                    .ItemStyle(&GraphicsComboRowStyle())
                    .ScrollBarStyle(&TGUiTheme::GetScrollBarStyle())
                    .OptionsSource(Options)
                    .MaxListHeight(420.0f)
                    .InitiallySelectedItem(GetGraphicsSelection(Axis))
                    .OnGenerateWidget_Lambda([](
                        const TSharedPtr<FString>& Option)
                    {
                        const FText Text = FText::FromString(
                            Option.IsValid()
                                ? FormatResultColumnLabel(*Option)
                                : FString());
                        return SNew(SBox)
                            .WidthOverride(1.0f)
                            [
                                SNew(STextBlock)
                                .Text(Text)
                                .ToolTipText(Text)
                                .ColorAndOpacity(Palette().TextPrimary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::FieldLabel))
                                .OverflowPolicy(
                                    ETextOverflowPolicy::Ellipsis)
                            ];
                    })
                    .OnSelectionChanged_Lambda([
                        this,
                        Axis](
                            const TSharedPtr<FString>& Selection,
                            ESelectInfo::Type)
                    {
                        HandleGraphicsColumnSelection(Axis, Selection);
                    })
                    [
                        SNew(SBox)
                        .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Text_Lambda([this, Axis]()
                            {
                                const TSharedPtr<FString> Selection =
                                    GetGraphicsSelection(Axis);
                                return FText::FromString(
                                    Selection.IsValid()
                                        ? FormatResultColumnLabel(*Selection)
                                        : FString(TEXT("--")));
                            })
                            .ToolTipText_Lambda([this, Axis]()
                            {
                                const TSharedPtr<FString> Selection =
                                    GetGraphicsSelection(Axis);
                                return FText::FromString(
                                    Selection.IsValid()
                                        ? FormatResultColumnLabel(*Selection)
                                        : FString());
                            })
                            .ColorAndOpacity(Palette().TextPrimary)
                            .Font(TGUiTheme::GetSlateFont(
                                ETGUiTextStyle::FieldLabel))
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        ]
                    ]
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 8.0f, 0.0f, 5.0f))
            [
                SNew(STextBlock)
                .Text(FText::FromString(AxisName + TEXT("-Scale")))
                .ColorAndOpacity(Palette().TextSecondary)
                .Font(TGUiTheme::GetSlateFont(ETGUiTextStyle::Caption))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                .HeightOverride(CompactControlHeight)
                [
                    SAssignNew(*ScaleInput, SEditableTextBox)
                    .Style(&TGUiTheme::GetEditableTextBoxStyle())
                    .Text(FText::FromString(FormatGraphicsScale(
                        GetGraphicsScale(Axis))))
                    .HintText(FText::FromString(TEXT("Nonzero factor")))
                    .SelectAllTextWhenFocused(true)
                    .SelectAllTextOnCommit(true)
                    .OnTextCommitted_Lambda([
                        this,
                        Axis](const FText& Text, ETextCommit::Type)
                    {
                        CommitGraphicsScale(Axis, Text);
                    })
                ]
            ];
    }

    TSharedRef<SWidget> BuildGraphicsContent()
    {
        using namespace TGVisualizationDockPrivate;

        return SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))
            [
                SNew(SBox)
                .MinDesiredHeight(250.0f)
                [
                    SAssignNew(GraphicsPlot, STGSimulationDataPlot)
                    .Owner(Owner.Get())
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBorder)
                .BorderImage(&RaisedCardBrush())
                .Padding(FMargin(12.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
                        [BuildGraphicsAxisControl(EGraphicsAxis::X)]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(FMargin(6.0f, 0.0f))
                        [BuildGraphicsAxisControl(EGraphicsAxis::Y)]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
                        [BuildGraphicsAxisControl(EGraphicsAxis::Z)]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 12.0f, 0.0f, 8.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Plot Time Range")))
                            .ColorAndOpacity(Palette().TextPrimary)
                            .Font(TGUiTheme::GetSlateFont(
                                ETGUiTextStyle::FieldLabel))
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Mission seconds")))
                            .ColorAndOpacity(Palette().TextSecondary)
                            .Font(TGUiTheme::GetSlateFont(
                                ETGUiTextStyle::Caption))
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SBox)
                        .HeightOverride(28.0f)
                        [
                            SNew(STGRangeSlider)
                            .LowerValue_Lambda([this]()
                            {
                                return static_cast<float>(
                                    GraphicsStartNormalized);
                            })
                            .UpperValue_Lambda([this]()
                            {
                                return static_cast<float>(
                                    GraphicsEndNormalized);
                            })
                            .OnRangeChanged_Lambda([this](
                                const float Start,
                                const float End)
                            {
                                GraphicsStartNormalized = Start;
                                GraphicsEndNormalized = End;
                                SyncGraphicsTimeInputs();
                                RefreshGraphicsPlotData();
                            })
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f))
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(
                                    TEXT("Plot Start [s]")))
                                .ColorAndOpacity(Palette().TextSecondary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::Caption))
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SNew(SBox)
                                .HeightOverride(CompactControlHeight)
                                [
                                    SAssignNew(
                                        GraphicsStartTimeInput,
                                        SEditableTextBox)
                                    .Style(&TGUiTheme::
                                        GetEditableTextBoxStyle())
                                    .Text(GraphicsStartTimeText)
                                    .SelectAllTextWhenFocused(true)
                                    .SelectAllTextOnCommit(true)
                                    .OnTextCommitted_Lambda([this](
                                        const FText& Text,
                                        ETextCommit::Type)
                                    {
                                        CommitGraphicsTimeBoundary(true, Text);
                                    })
                                ]
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f))
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(
                                    TEXT("Plot End [s]")))
                                .ColorAndOpacity(Palette().TextSecondary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::Caption))
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SNew(SBox)
                                .HeightOverride(CompactControlHeight)
                                [
                                    SAssignNew(
                                        GraphicsEndTimeInput,
                                        SEditableTextBox)
                                    .Style(&TGUiTheme::
                                        GetEditableTextBoxStyle())
                                    .Text(GraphicsEndTimeText)
                                    .SelectAllTextWhenFocused(true)
                                    .SelectAllTextOnCommit(true)
                                    .OnTextCommitted_Lambda([this](
                                        const FText& Text,
                                        ETextCommit::Type)
                                    {
                                        CommitGraphicsTimeBoundary(false, Text);
                                    })
                                ]
                            ]
                        ]
                    ]
                ]
            ];
    }

    TSharedRef<SWidget> BuildSolarSystemContent()
	{
		using namespace TGVisualizationDockPrivate;

		TSharedPtr<SWidget> SolarSystemGraph;
		TSharedPtr<SWidget> ClosestBodyGraph;
		if (UTGVisualizationDockWorkspaceWidget* OwnerWidget = Owner.Get())
		{
			SolarSystemGraph =
				OwnerWidget->GetSolarSystemOverviewSlateWidget();
			ClosestBodyGraph =
				OwnerWidget->GetClosestBodyOverviewSlateWidget();
		}

		return SNew(SVerticalBox)

			// ============================================================
			// BUTTON ROW
			// ============================================================
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 14.0f, 0.0f, 8.0f))
			[
				SNew(SHorizontalBox)

				// --------------------------------------------------------
				// Refocus both overview graphs
				// --------------------------------------------------------
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SBox)
					.HeightOverride(42.0f)
					[
						SNew(SButton)
						.ButtonStyle(&TGUiTheme::GetButtonStyle(
							ETGUiButtonStyle::Secondary))
						.ContentPadding(FMargin(0.0f))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked_Lambda([WeakOwner = Owner]()
						{
							if (UTGVisualizationDockWorkspaceWidget* Widget =
									WeakOwner.Get())
							{
								Widget->FocusSolarSystemOnSpacecraft();
							}

							return FReply::Handled();
						})
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Refocus")))
							.ColorAndOpacity(Palette().TextPrimary)
							.Font(TGUiTheme::GetSlateFont(
								ETGUiTextStyle::FieldLabel))
							.Justification(ETextJustify::Center)
						]
					]
				]

				// --------------------------------------------------------
				// Solar Overview
				// --------------------------------------------------------
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SBox)
					.HeightOverride(42.0f)
					[
						SNew(SButton)
						.ButtonStyle(&TGUiTheme::GetButtonStyle(
							ETGUiButtonStyle::Secondary))
						.ContentPadding(FMargin(0.0f))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked_Lambda([WeakOwner = Owner]()
						{
							if (UTGVisualizationDockWorkspaceWidget* Widget =
									WeakOwner.Get())
							{
								Widget->ResetSolarSystemView();
							}

							return FReply::Handled();
						})
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Solar Overview")))
							.ColorAndOpacity(Palette().TextPrimary)
							.Font(TGUiTheme::GetSlateFont(
								ETGUiTextStyle::FieldLabel))
							.Justification(ETextJustify::Center)
						]
					]
				]
			]

			// ============================================================
			// SOLAR SYSTEM GRAPH
			// ============================================================
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 5.0f))
			[
				SolarSystemGraph.IsValid()
					? SolarSystemGraph.ToSharedRef()
					: SNullWidget::NullWidget
			]

			// ============================================================
			// CLOSEST-BODY GRAPH
			// ============================================================
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(FMargin(0.0f, 5.0f, 0.0f, 0.0f))
			[
				ClosestBodyGraph.IsValid()
					? ClosestBodyGraph.ToSharedRef()
					: SNullWidget::NullWidget
			];
	}

    TSharedRef<SWidget> BuildConstellationsContent()
    {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))
            [
                SNew(SBox)
                .HeightOverride(
                    TGVisualizationDockPrivate::CompactControlHeight)
                [
                    SNew(SSearchBox)
                    .Style(&TGUiTheme::GetSearchBoxStyle())
                    .HintText(FText::FromString(TEXT("Search constellations")))
                    .OnTextChanged_Lambda([this](const FText& Text)
                    {
                        ConstellationFilter = Text.ToString();
                        RebuildConstellationRows();
                    })
                ]
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SNew(SScrollBox)
                .Style(&TGUiTheme::GetScrollBoxStyle())
                .ScrollBarStyle(&TGUiTheme::GetScrollBarStyle())
                .Orientation(Orient_Vertical)
                .ConsumeMouseWheel(EConsumeMouseWheel::Always)
                .ScrollBarThickness(FVector2f(8.0f, 8.0f))
                .ScrollBarPadding(FMargin(6.0f, 4.0f, 4.0f, 4.0f))
                .AllowOverscroll(EAllowOverscroll::No)
                .AnimateWheelScrolling(true)
                .WheelScrollMultiplier(4.0f)
                + SScrollBox::Slot()
                [
                    SAssignNew(ConstellationRowsBox, SVerticalBox)
                ]
            ];
    }

    TSharedRef<SWidget> BuildVectorsContent()
    {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))
            [
                SNew(SBox)
                .HeightOverride(
                    TGVisualizationDockPrivate::CompactControlHeight)
                [
                    SNew(SSearchBox)
                    .Style(&TGUiTheme::GetSearchBoxStyle())
                    .HintText(FText::FromString(TEXT("Search vectors")))
                    .OnTextChanged_Lambda([this](const FText& Text)
                    {
                        VectorFilter = Text.ToString();
                        RebuildVectorRows();
                    })
                ]
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SNew(SScrollBox)
                .Style(&TGUiTheme::GetScrollBoxStyle())
                .ScrollBarStyle(&TGUiTheme::GetScrollBarStyle())
                .Orientation(Orient_Vertical)
                .ConsumeMouseWheel(EConsumeMouseWheel::Always)
                .ScrollBarThickness(FVector2f(8.0f, 8.0f))
                .ScrollBarPadding(FMargin(6.0f, 4.0f, 4.0f, 4.0f))
                .AllowOverscroll(EAllowOverscroll::No)
                .AnimateWheelScrolling(true)
                .WheelScrollMultiplier(4.0f)
                + SScrollBox::Slot()
                [
                    SAssignNew(VectorRowsBox, SVerticalBox)
                ]
            ];
    }

    TSharedRef<SWidget> BuildTelemetryContent()
    {
        return SNew(SScrollBox)
            .Style(&TGUiTheme::GetScrollBoxStyle())
            .ScrollBarStyle(&TGUiTheme::GetScrollBarStyle())
            .Orientation(Orient_Vertical)
            .ConsumeMouseWheel(EConsumeMouseWheel::Always)
            .ScrollBarThickness(FVector2f(8.0f, 8.0f))
            .ScrollBarPadding(FMargin(6.0f, 4.0f, 4.0f, 4.0f))
            .AllowOverscroll(EAllowOverscroll::No)
            .AnimateWheelScrolling(true)
            .WheelScrollMultiplier(4.0f)
            + SScrollBox::Slot()
            [
                SAssignNew(TelemetryRowsBox, SVerticalBox)
            ];
    }

    void RebuildConstellationRows()
    {
        using namespace TGVisualizationDockPrivate;
        if (!ConstellationRowsBox.IsValid())
        {
            return;
        }
        ConstellationRowsBox->ClearChildren();
        const UTGVisualizationDockWorkspaceWidget* OwnerWidget = Owner.Get();
        if (OwnerWidget == nullptr)
        {
            return;
        }

        const FString Needle = ConstellationFilter.TrimStartAndEnd();
        int32 Added = 0;
        for (const FTGVisualizationConstellationEntry& Entry :
             OwnerWidget->GetConstellations())
        {
            if (!Needle.IsEmpty() &&
                !Entry.DisplayName.Contains(Needle, ESearchCase::IgnoreCase))
            {
                continue;
            }
            ++Added;
            ConstellationRowsBox->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
            [
                SNew(SBox)
                .MinDesiredHeight(CompactRowHeight)
                [
                    SNew(SBorder)
                    .BorderImage(&SurfaceCardBrush())
                    .Padding(FMargin(12.0f, 8.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        .Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(Entry.DisplayName))
                            .ColorAndOpacity(Palette().TextPrimary)
                            .Font(TGUiTheme::GetSlateFont(
                                ETGUiTextStyle::FieldLabel))
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SCheckBox)
                            .Style(&TGUiTheme::GetCheckBoxStyle())
                            .ToolTipText(FText::FromString(
                                TEXT("Show constellation outline")))
                            .IsChecked_Lambda(
                                [WeakOwner = Owner, Id = Entry.Id]()
                                {
                                    const auto* Widget = WeakOwner.Get();
                                    return Widget != nullptr &&
                                        Widget->IsConstellationOutlineVisible(
                                            Id)
                                        ? ECheckBoxState::Checked
                                        : ECheckBoxState::Unchecked;
                                })
                            .OnCheckStateChanged_Lambda(
                                [WeakOwner = Owner, Id = Entry.Id](
                                    const ECheckBoxState State)
                                {
                                    if (auto* Widget = WeakOwner.Get())
                                    {
                                        Widget->SetConstellationOutlineVisible(
                                            Id,
                                            State == ECheckBoxState::Checked);
                                    }
                                })
                        ]
                    ]
                ]
            ];
        }
        if (Added == 0)
        {
            ConstellationRowsBox->AddSlot()
            .AutoHeight()
            .Padding(FMargin(12.0f, 16.0f))
            [
                SNew(STextBlock)
                .Text(FText::FromString(
                    Needle.IsEmpty()
                        ? TEXT("No constellation catalog is available")
                        : TEXT("No matching constellations")))
                .ColorAndOpacity(Palette().TextSecondary)
                .Font(TGUiTheme::GetSlateFont(ETGUiTextStyle::FieldLabel))
                .AutoWrapText(true)
            ];
        }
    }

    void RebuildVectorRows()
    {
        using namespace TGVisualizationDockPrivate;
        if (!VectorRowsBox.IsValid())
        {
            return;
        }
        VectorRowsBox->ClearChildren();
        UTGVisualizationDockWorkspaceWidget* OwnerWidget = Owner.Get();
        if (OwnerWidget == nullptr)
        {
            return;
        }
        TArray<FTGVisualizationArrowInfo> Infos =
            OwnerWidget->GetVisualizationArrows();

        auto GetSectionIndex = [](
            const FTGVisualizationArrowInfo& Info) -> int32
        {
            if (Info.Quantity ==
                ETGVisualizationArrowQuantity::BodyDirection)
            {
                return 0;
            }
            if ((Info.Quantity == ETGVisualizationArrowQuantity::Force &&
                 Info.ArrowId == FName(TEXT("force_total"))) ||
                (Info.Quantity == ETGVisualizationArrowQuantity::Torque &&
                 Info.ArrowId == FName(TEXT("torque_total"))) ||
                (Info.Quantity != ETGVisualizationArrowQuantity::Force &&
                 Info.Quantity != ETGVisualizationArrowQuantity::Torque))
            {
                return 1;
            }
            return Info.Quantity == ETGVisualizationArrowQuantity::Force
                ? 2
                : 3;
        };
        auto GetSectionName = [](const int32 SectionIndex) -> FString
        {
            switch (SectionIndex)
            {
                case 0:
                    return TEXT("BODIES");
                case 1:
                    return TEXT("DYNAMICS AND KINEMATICS");
                case 2:
                    return TEXT("FORCES");
                default:
                    return TEXT("TORQUE");
            }
        };

        Infos.Sort([&GetSectionIndex](
            const FTGVisualizationArrowInfo& A,
            const FTGVisualizationArrowInfo& B)
        {
            const int32 ASection = GetSectionIndex(A);
            const int32 BSection = GetSectionIndex(B);
            if (ASection != BSection)
            {
                return ASection < BSection;
            }
            return A.DisplayName < B.DisplayName;
        });
        const FString Needle = VectorFilter.TrimStartAndEnd();
        int32 Added = 0;
        int32 CurrentSection = INDEX_NONE;
        for (const FTGVisualizationArrowInfo& Info : Infos)
        {
            const int32 SectionIndex = GetSectionIndex(Info);
            const FString SectionName = GetSectionName(SectionIndex);
            if (!Needle.IsEmpty() &&
                !Info.DisplayName.Contains(Needle, ESearchCase::IgnoreCase) &&
                !SectionName.Contains(Needle, ESearchCase::IgnoreCase))
            {
                continue;
            }
            if (SectionIndex != CurrentSection)
            {
                CurrentSection = SectionIndex;
                VectorRowsBox->AddSlot()
                .AutoHeight()
                .Padding(FMargin(
                    2.0f,
                    Added == 0 ? 0.0f : 20.0f,
                    2.0f,
                    8.0f))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(SectionName))
                    .ColorAndOpacity(Palette().TextPrimary)
                    .Font(TGUiTheme::GetSlateFont(
                        ETGUiTextStyle::Section))
                ];
            }
            ++Added;
            VectorRowsBox->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
            [
                SNew(SBox)
                .MinDesiredHeight(CompactRowHeight)
                [
                    SNew(SBorder)
                    .BorderImage(&SurfaceCardBrush())
                    .Padding(FMargin(12.0f, 8.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
                        [
                            SNew(SCheckBox)
                            .Style(&TGUiTheme::GetCheckBoxStyle())
                            .IsChecked_Lambda(
                                [WeakOwner = Owner, Id = Info.ArrowId]()
                                {
                                    const auto* Widget = WeakOwner.Get();
                                    return Widget != nullptr &&
                                        Widget->IsVisualizationArrowVisible(Id)
                                        ? ECheckBoxState::Checked
                                        : ECheckBoxState::Unchecked;
                                })
                            .OnCheckStateChanged_Lambda(
                                [WeakOwner = Owner, Id = Info.ArrowId](
                                    const ECheckBoxState State)
                                {
                                    if (auto* Widget = WeakOwner.Get())
                                    {
                                        Widget->SetVisualizationArrowVisible(
                                            Id,
                                            State == ECheckBoxState::Checked);
                                    }
                                })
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
                        [
                            SNew(SBox)
                            .WidthOverride(14.0f)
                            .HeightOverride(14.0f)
                            [
                                SNew(SBorder)
                                .BorderImage(&TintableRoundedBrush())
                                .BorderBackgroundColor(Info.Color)
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(Info.DisplayName))
                                .ColorAndOpacity(Palette().TextPrimary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::FieldLabel))
                                .OverflowPolicy(
                                    ETextOverflowPolicy::Ellipsis)
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(FMargin(0.0f, 3.0f, 0.0f, 0.0f))
                            [
                                SNew(STextBlock)
                                .Visibility(
                                    Info.Quantity !=
                                            ETGVisualizationArrowQuantity::
                                                BodyDirection
                                        ? EVisibility::Visible
                                        : EVisibility::Collapsed)
                                .Text_Lambda(
                                    [WeakOwner = Owner,
                                     Id = Info.ArrowId]()
                                    {
                                        const auto* Widget = WeakOwner.Get();
                                        FTGVisualizationArrowInfo LiveInfo;
                                        if (
                                            Widget == nullptr ||
                                            !Widget->GetVisualizationArrowInfo(
                                                Id,
                                                LiveInfo))
                                        {
                                            return FText::GetEmpty();
                                        }
                                        FString Detail = TEXT("Magnitude ") +
                                            FormatVectorMagnitude(LiveInfo);
                                        if (
                                            LiveInfo.CurrentMagnitude <=
                                            UE_DOUBLE_SMALL_NUMBER)
                                        {
                                            Detail += TEXT(" | No Direction");
                                        }
                                        else
                                        {
                                            Detail += TEXT(" | Visual Scale x ") +
                                                FormatVisualAmplification(
                                                    LiveInfo.
                                                        RelativeVisualAmplification);
                                        }
                                        return FText::FromString(Detail);
                                    })
                                .ColorAndOpacity(Palette().TextSecondary)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::Caption))
                                .OverflowPolicy(
                                    ETextOverflowPolicy::Ellipsis)
                            ]
                        ]
                    ]
                ]
            ];
        }
        if (Added == 0)
        {
            VectorRowsBox->AddSlot()
            .AutoHeight()
            .Padding(FMargin(12.0f, 16.0f))
            [
                SNew(STextBlock)
                .Text(FText::FromString(
                    Needle.IsEmpty()
                        ? TEXT("No result vectors are available")
                        : TEXT("No matching vectors")))
                .ColorAndOpacity(Palette().TextSecondary)
                .Font(TGUiTheme::GetSlateFont(ETGUiTextStyle::FieldLabel))
                .AutoWrapText(true)
            ];
        }
    }

    TSharedRef<SWidget> MakeAxisValue(
        const TCHAR* Axis,
        const double Value,
        const FLinearColor& AxisColor,
        const int32 DecimalPlaces) const
    {
        using namespace TGVisualizationDockPrivate;
        return SNew(SBorder)
            .BorderImage(&RaisedCardBrush())
            .Padding(FMargin(8.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Axis))
                    .ColorAndOpacity(AxisColor)
                    .Font(TGUiTheme::GetSlateFont(
                        ETGUiTextStyle::Caption))
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FormatTelemetryNumber(
                        Value,
                        DecimalPlaces)))
                    .ColorAndOpacity(Palette().TextPrimary)
                    .Font(TGUiTheme::GetSlateFont(
                        ETGUiTextStyle::Numeric))
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                ]
            ];
    }

    TSharedRef<SWidget> MakeTelemetryItem(
        const FTGVisualizationTelemetryItem& Item) const
    {
        using namespace TGVisualizationDockPrivate;
        const FLinearColor ItemAccent = ResolveTelemetryAccent(Item);
        FSlateFontInfo NoteFont =
            TGUiTheme::GetSlateFont(ETGUiTextStyle::Caption);
        NoteFont.Size = FMath::Max(8, NoteFont.Size - 1);
        TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
        const bool bStructuredValue =
            Item.Type == ETGVisualizationTelemetryType::Vector3 ||
            Item.Type == ETGVisualizationTelemetryType::Vector4 ||
            Item.Type == ETGVisualizationTelemetryType::Matrix3;
        if (bStructuredValue)
        {
            Body->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Item.DisplayName))
                    .ColorAndOpacity(Palette().TextSecondary)
                    .Font(TGUiTheme::GetSlateFont(
                        ETGUiTextStyle::FieldLabel))
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Item.Unit))
                    .ColorAndOpacity(Palette().TextSecondary)
                    .Font(TGUiTheme::GetSlateFont(
                    ETGUiTextStyle::Caption))
                ]
            ];
            if (Item.Type == ETGVisualizationTelemetryType::Vector3)
            {
                const int32 DecimalPlaces = SelectTelemetryDecimalPlaces(
                    FMath::Max3(
                        FMath::Abs(Item.VectorValue.X),
                        FMath::Abs(Item.VectorValue.Y),
                        FMath::Abs(Item.VectorValue.Z)));
                Body->AddSlot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
                    [MakeAxisValue(TEXT("X"), Item.VectorValue.X,
                        FLinearColor(0.93f, 0.31f, 0.28f), DecimalPlaces)]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(FMargin(4.0f, 0.0f))
                    [MakeAxisValue(TEXT("Y"), Item.VectorValue.Y,
                        FLinearColor(0.31f, 0.84f, 0.45f), DecimalPlaces)]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
                    [MakeAxisValue(TEXT("Z"), Item.VectorValue.Z,
                        FLinearColor(0.35f, 0.58f, 0.96f), DecimalPlaces)]
                ];
            }
            else if (Item.Type == ETGVisualizationTelemetryType::Vector4)
            {
                const int32 DecimalPlaces = SelectTelemetryDecimalPlaces(
                    FMath::Max(
                        FMath::Max(
                            FMath::Abs(Item.Vector4Value.W),
                            FMath::Abs(Item.Vector4Value.X)),
                        FMath::Max(
                            FMath::Abs(Item.Vector4Value.Y),
                            FMath::Abs(Item.Vector4Value.Z))));
                Body->AddSlot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(FMargin(0.0f, 0.0f, 3.0f, 0.0f))
                    [MakeAxisValue(TEXT("W"), Item.Vector4Value.W,
                        FLinearColor(0.94f, 0.78f, 0.30f), DecimalPlaces)]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(FMargin(3.0f, 0.0f))
                    [MakeAxisValue(TEXT("X"), Item.Vector4Value.X,
                        FLinearColor(0.93f, 0.31f, 0.28f), DecimalPlaces)]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(FMargin(3.0f, 0.0f))
                    [MakeAxisValue(TEXT("Y"), Item.Vector4Value.Y,
                        FLinearColor(0.31f, 0.84f, 0.45f), DecimalPlaces)]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(FMargin(3.0f, 0.0f, 0.0f, 0.0f))
                    [MakeAxisValue(TEXT("Z"), Item.Vector4Value.Z,
                        FLinearColor(0.35f, 0.58f, 0.96f), DecimalPlaces)]
                ];
            }
            else
            {
                static const TCHAR* MatrixLabels[9] = {
                    TEXT("Ixx"), TEXT("Ixy"), TEXT("Ixz"),
                    TEXT("Iyx"), TEXT("Iyy"), TEXT("Iyz"),
                    TEXT("Izx"), TEXT("Izy"), TEXT("Izz")};
                static const FLinearColor MatrixColumnColors[3] = {
                    FLinearColor(0.93f, 0.31f, 0.28f),
                    FLinearColor(0.31f, 0.84f, 0.45f),
                    FLinearColor(0.35f, 0.58f, 0.96f)};
                double MatrixMagnitude = 0.0;
                for (const double Value : Item.MatrixValues)
                {
                    MatrixMagnitude = FMath::Max(
                        MatrixMagnitude,
                        FMath::Abs(Value));
                }
                const int32 DecimalPlaces =
                    SelectTelemetryDecimalPlaces(MatrixMagnitude);
                TSharedRef<SVerticalBox> MatrixGrid = SNew(SVerticalBox);
                for (int32 Row = 0; Row < 3; ++Row)
                {
                    TSharedRef<SHorizontalBox> MatrixRow =
                        SNew(SHorizontalBox);
                    for (int32 Column = 0; Column < 3; ++Column)
                    {
                        const int32 ValueIndex = Row * 3 + Column;
                        const double Value =
                            Item.MatrixValues.IsValidIndex(ValueIndex)
                                ? Item.MatrixValues[ValueIndex]
                                : 0.0;
                        MatrixRow->AddSlot()
                        .FillWidth(1.0f)
                        .Padding(FMargin(
                            Column == 0 ? 0.0f : 4.0f,
                            0.0f,
                            Column == 2 ? 0.0f : 4.0f,
                            0.0f))
                        [MakeAxisValue(
                            MatrixLabels[ValueIndex],
                            Value,
                            MatrixColumnColors[Column],
                            DecimalPlaces)];
                    }
                    MatrixGrid->AddSlot()
                    .AutoHeight()
                    .Padding(FMargin(
                        0.0f,
                        Row == 0 ? 0.0f : 4.0f,
                        0.0f,
                        Row == 2 ? 0.0f : 4.0f))
                    [MatrixRow];
                }
                Body->AddSlot()
                .AutoHeight()
                [MatrixGrid];
            }
        }
        else
        {
            const FString Value =
                Item.Type == ETGVisualizationTelemetryType::Scalar
                    ? FormatTelemetryNumber(
                        Item.ScalarValue,
                        SelectTelemetryDecimalPlaces(Item.ScalarValue))
                    : Item.TextValue;
            Body->AddSlot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(Item.DisplayName))
                        .ColorAndOpacity(Palette().TextSecondary)
                        .Font(TGUiTheme::GetSlateFont(
                            ETGUiTextStyle::FieldLabel))
                        .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
                    [
                        SNew(STextBlock)
                        .Visibility(
                            Item.Note.IsEmpty()
                                ? EVisibility::Collapsed
                                : EVisibility::Visible)
                        .Text(FText::FromString(Item.Note))
                        .ColorAndOpacity(Palette().TextSecondary)
                        .Font(NoteFont)
                        .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(12.0f, 0.0f, 0.0f, 0.0f))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Value))
                    .ColorAndOpacity(
                        Item.Type == ETGVisualizationTelemetryType::Status &&
                        Item.TextValue.Equals(
                            TEXT("Failed"),
                            ESearchCase::IgnoreCase)
                            ? Palette().Error
                            : Palette().TextPrimary)
                    .Font(TGUiTheme::GetSlateFont(
                        Item.Type == ETGVisualizationTelemetryType::Scalar
                            ? ETGUiTextStyle::Numeric
                            : ETGUiTextStyle::BodyStrong))
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Item.Unit))
                    .ColorAndOpacity(Palette().TextSecondary)
                    .Font(TGUiTheme::GetSlateFont(
                        ETGUiTextStyle::Caption))
                ]
            ];
        }

        return SNew(SBorder)
            .BorderImage(&SurfaceCardBrush())
            .Padding(FMargin(0.0f))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(4.0f, 4.0f, 0.0f, 4.0f))
                [
                    SNew(SBox)
                    .WidthOverride(3.0f)
                    [
                        SNew(SBorder)
                        .BorderImage(&TintableRoundedBrush())
                        .BorderBackgroundColor(ItemAccent)
                    ]
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(FMargin(12.0f))
                [Body]
            ];
    }

    void RebuildTelemetryRows()
    {
        using namespace TGVisualizationDockPrivate;
        if (!TelemetryRowsBox.IsValid())
        {
            return;
        }
        TelemetryRowsBox->ClearChildren();
        const UTGVisualizationDockWorkspaceWidget* OwnerWidget = Owner.Get();
        if (OwnerWidget == nullptr)
        {
            return;
        }
        TArray<FTGVisualizationTelemetryItem> Items;
        OwnerWidget->GetTelemetryItems(Items);
        FString CurrentGroup;
        bool bFirstGroup = true;
        for (const FTGVisualizationTelemetryItem& Item : Items)
        {
            if (Item.Group != CurrentGroup)
            {
                CurrentGroup = Item.Group;
                TelemetryRowsBox->AddSlot()
                .AutoHeight()
                .Padding(FMargin(
                    2.0f,
                    bFirstGroup ? 0.0f : 20.0f,
                    2.0f,
                    8.0f))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(CurrentGroup.ToUpper()))
                    .ColorAndOpacity(Palette().TextPrimary)
                    .Font(TGUiTheme::GetSlateFont(
                        ETGUiTextStyle::Section))
                ];
                bFirstGroup = false;
            }
            TelemetryRowsBox->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
            [MakeTelemetryItem(Item)];
        }
        if (Items.IsEmpty())
        {
            TelemetryRowsBox->AddSlot()
            .AutoHeight()
            .Padding(FMargin(12.0f, 16.0f))
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("No telemetry is available")))
                .ColorAndOpacity(Palette().TextSecondary)
                .Font(TGUiTheme::GetSlateFont(
                    ETGUiTextStyle::FieldLabel))
            ];
        }
    }
};

UTGVisualizationDockWorkspaceWidget::UTGVisualizationDockWorkspaceWidget()
{
    ConstellationTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
        TEXT("/Game/Data/DT_ConstellationsVisible."
             "DT_ConstellationsVisible")));
    SolarSystemOverviewWidgetClass =
        TSoftClassPtr<UTGSolarSystemOverviewWidget>(FSoftObjectPath(
            TEXT("/Game/UI/Visualization/WBP_SolarSystemOverview."
                 "WBP_SolarSystemOverview_C")));
}

void UTGVisualizationDockWorkspaceWidget::InitializeForPlayback(
    ATGSimulationPlaybackActor* InPlaybackActor)
{
    LastTelemetryRefreshRealSeconds = -1.0;
    if (IsValid(PlaybackActor))
    {
        PlaybackActor->OnPlaybackTimeChanged.RemoveDynamic(
            this,
            &UTGVisualizationDockWorkspaceWidget::HandlePlaybackTimeChanged);
    }
    PlaybackActor = InPlaybackActor;
    if (IsValid(PlaybackActor))
    {
        PlaybackActor->OnPlaybackTimeChanged.AddUniqueDynamic(
            this,
            &UTGVisualizationDockWorkspaceWidget::HandlePlaybackTimeChanged);
    }
    EnsureSolarSystemOverviewWidget();
    if (IsValid(SolarSystemOverviewWidget))
    {
        SolarSystemOverviewWidget->InitializeForPlayback(PlaybackActor);
    }
    if (IsValid(ClosestBodyOverviewWidget))
    {
        ClosestBodyOverviewWidget->InitializeForPlayback(PlaybackActor);
    }
    RebuildConstellationCatalog();
    if (WorkspaceWidget.IsValid())
    {
        WorkspaceWidget->RefreshAll();
    }
}

ATGSimulationPlaybackActor*
UTGVisualizationDockWorkspaceWidget::GetPlaybackActor() const
{
    return PlaybackActor;
}

bool UTGVisualizationDockWorkspaceWidget::IsPointerOverInteractiveArea() const
{
    return WorkspaceWidget.IsValid() &&
        WorkspaceWidget->IsPointerOverInteractiveArea();
}

const TArray<FTGVisualizationConstellationEntry>&
UTGVisualizationDockWorkspaceWidget::GetConstellations() const
{
    return Constellations;
}

TArray<FTGVisualizationArrowInfo>
UTGVisualizationDockWorkspaceWidget::GetVisualizationArrows() const
{
    return IsValid(PlaybackActor)
        ? PlaybackActor->GetVisualizationArrows()
        : TArray<FTGVisualizationArrowInfo>();
}

bool UTGVisualizationDockWorkspaceWidget::GetVisualizationArrowInfo(
    const FName ArrowId,
    FTGVisualizationArrowInfo& OutInfo) const
{
    if (!IsValid(PlaybackActor))
    {
        OutInfo = FTGVisualizationArrowInfo{};
        return false;
    }
    return PlaybackActor->GetVisualizationArrowInfo(ArrowId, OutInfo);
}

void UTGVisualizationDockWorkspaceWidget::GetTelemetryItems(
    TArray<FTGVisualizationTelemetryItem>& OutItems) const
{
    OutItems.Reset();
    if (IsValid(PlaybackActor))
    {
        PlaybackActor->GetCurrentTelemetryItems(OutItems);
    }
}

TSharedPtr<SWidget>
UTGVisualizationDockWorkspaceWidget::GetSolarSystemOverviewSlateWidget()
{
    EnsureSolarSystemOverviewWidget();
    return IsValid(SolarSystemOverviewWidget)
        ? SolarSystemOverviewWidget->TakeWidget()
        : TSharedPtr<SWidget>();
}

TSharedPtr<SWidget>
UTGVisualizationDockWorkspaceWidget::GetClosestBodyOverviewSlateWidget()
{
    EnsureSolarSystemOverviewWidget();
    return IsValid(ClosestBodyOverviewWidget)
        ? ClosestBodyOverviewWidget->TakeWidget()
        : TSharedPtr<SWidget>();
}

void UTGVisualizationDockWorkspaceWidget::FocusSolarSystemOnSpacecraft()
{
    EnsureSolarSystemOverviewWidget();
    if (IsValid(SolarSystemOverviewWidget))
    {
        SolarSystemOverviewWidget->FocusSpacecraftPath();
    }
    if (IsValid(ClosestBodyOverviewWidget))
    {
        // The closest body is the origin of this graph. Resetting its view
        // returns that origin to the exact center after any user pan or zoom.
        ClosestBodyOverviewWidget->ResetSolarSystemView();
    }
}

void UTGVisualizationDockWorkspaceWidget::ResetSolarSystemView()
{
    EnsureSolarSystemOverviewWidget();
    if (IsValid(SolarSystemOverviewWidget))
    {
        SolarSystemOverviewWidget->ResetSolarSystemView();
    }
    if (IsValid(ClosestBodyOverviewWidget))
    {
        ClosestBodyOverviewWidget->ResetSolarSystemView();
    }
}

void UTGVisualizationDockWorkspaceWidget::RefreshSolarSystemOverviewGraphs()
{
    if (IsValid(SolarSystemOverviewWidget))
    {
        SolarSystemOverviewWidget->InvalidateLayoutAndVolatility();
    }
    if (IsValid(ClosestBodyOverviewWidget))
    {
        ClosestBodyOverviewWidget->InvalidateLayoutAndVolatility();
    }
}

bool UTGVisualizationDockWorkspaceWidget::IsConstellationOutlineVisible(
    const FString& Id) const
{
    const bool* State = ConstellationOutlineStates.Find(
        TGVisualizationDockPrivate::NormalizeConstellationId(Id));
    return State != nullptr && *State;
}

void UTGVisualizationDockWorkspaceWidget::SetConstellationOutlineVisible(
    const FString& Id,
    const bool bVisible)
{
    ConstellationOutlineStates.Add(
        TGVisualizationDockPrivate::NormalizeConstellationId(Id),
        bVisible);
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }
    for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
    {
        if (TGVisualizationDockPrivate::IsConstellationShell(**ActorIt))
        {
            TGVisualizationDockPrivate::CompleteActiveConstellationFade(
                **ActorIt);
            TGVisualizationDockPrivate::InvokeConstellationVisibilityFunction(
                **ActorIt,
                TEXT("SetConstellationOutlineVisible"),
                Id,
                bVisible);
        }
    }
}

bool UTGVisualizationDockWorkspaceWidget::IsVisualizationArrowVisible(
    const FName ArrowId) const
{
    return IsValid(PlaybackActor) &&
        PlaybackActor->IsVisualizationArrowVisible(ArrowId);
}

void UTGVisualizationDockWorkspaceWidget::SetVisualizationArrowVisible(
    const FName ArrowId,
    const bool bVisible)
{
    if (IsValid(PlaybackActor))
    {
        PlaybackActor->SetVisualizationArrowVisible(ArrowId, bVisible);
    }
}

TSharedRef<SWidget> UTGVisualizationDockWorkspaceWidget::RebuildWidget()
{
    RebuildConstellationCatalog();
    WorkspaceWidget = SNew(STGVisualizationDockWorkspace).Owner(this);
    return WorkspaceWidget.ToSharedRef();
}

void UTGVisualizationDockWorkspaceWidget::ReleaseSlateResources(
    const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    WorkspaceWidget.Reset();
}

void UTGVisualizationDockWorkspaceWidget::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    RebuildConstellationCatalog();
    if (WorkspaceWidget.IsValid())
    {
        WorkspaceWidget->RefreshAll();
    }
}

void UTGVisualizationDockWorkspaceWidget::BeginDestroy()
{
    if (IsValid(PlaybackActor))
    {
        PlaybackActor->OnPlaybackTimeChanged.RemoveDynamic(
            this,
            &UTGVisualizationDockWorkspaceWidget::HandlePlaybackTimeChanged);
    }
    if (IsValid(SolarSystemOverviewWidget))
    {
        SolarSystemOverviewWidget->InitializeForPlayback(nullptr);
    }
    if (IsValid(ClosestBodyOverviewWidget))
    {
        ClosestBodyOverviewWidget->InitializeForPlayback(nullptr);
    }
    Super::BeginDestroy();
}

void UTGVisualizationDockWorkspaceWidget::EnsureSolarSystemOverviewWidget()
{
    if (
        IsValid(SolarSystemOverviewWidget) &&
        IsValid(ClosestBodyOverviewWidget))
    {
        return;
    }
    UClass* WidgetClass = SolarSystemOverviewWidgetClass.LoadSynchronous();
    APlayerController* PlayerController = GetOwningPlayer();
    if (PlayerController == nullptr && GetWorld() != nullptr)
    {
        PlayerController = GetWorld()->GetFirstPlayerController();
    }
    if (WidgetClass == nullptr || PlayerController == nullptr)
    {
        return;
    }
    if (!IsValid(SolarSystemOverviewWidget))
    {
        SolarSystemOverviewWidget =
            CreateWidget<UTGSolarSystemOverviewWidget>(
                PlayerController,
                WidgetClass);
        if (IsValid(SolarSystemOverviewWidget))
        {
            SolarSystemOverviewWidget->SetBodyCenteredView(false);
            SolarSystemOverviewWidget->InitializeForPlayback(PlaybackActor);
        }
    }

    if (!IsValid(ClosestBodyOverviewWidget))
    {
        ClosestBodyOverviewWidget =
            CreateWidget<UTGSolarSystemOverviewWidget>(
                PlayerController,
                WidgetClass);
        if (IsValid(ClosestBodyOverviewWidget))
        {
            ClosestBodyOverviewWidget->SetBodyCenteredView(true);
            ClosestBodyOverviewWidget->InitializeForPlayback(PlaybackActor);
        }
    }
}

void UTGVisualizationDockWorkspaceWidget::RebuildConstellationCatalog()
{
    Constellations.Reset();
    TSet<FString> AddedIds;

    UDataTable* Table = ConstellationTable.LoadSynchronous();
    if (Table != nullptr && Table->GetRowStruct() != nullptr)
    {
        for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
        {
            FString Id;
            FString Name;
            for (TFieldIterator<FProperty> PropertyIt(
                     Table->GetRowStruct(),
                     EFieldIteratorFlags::IncludeSuper);
                 PropertyIt;
                 ++PropertyIt)
            {
                FProperty* Property = *PropertyIt;
                const FString PropertyName = Property->GetName();
                if (Id.IsEmpty() &&
                    PropertyName.StartsWith(TEXT("ConstellationId")))
                {
                    TGVisualizationDockPrivate::ReadPropertyAsString(
                        *Property,
                        Pair.Value,
                        Id);
                }
                else if (Name.IsEmpty() &&
                    (PropertyName.StartsWith(TEXT("Constellation_")) ||
                     PropertyName == TEXT("Constellation")))
                {
                    TGVisualizationDockPrivate::ReadPropertyAsString(
                        *Property,
                        Pair.Value,
                        Name);
                }
            }
            if (Id.IsEmpty())
            {
                Id = Pair.Key.ToString();
            }
            if (Name.IsEmpty())
            {
                Name = Pair.Key.ToString().Replace(TEXT("_"), TEXT(" "));
            }
            const FString Normalized =
                TGVisualizationDockPrivate::NormalizeConstellationId(Id);
            if (!Normalized.IsEmpty() && !AddedIds.Contains(Normalized))
            {
                AddedIds.Add(Normalized);
                FTGVisualizationConstellationEntry& Entry =
                    Constellations.AddDefaulted_GetRef();
                Entry.Id = Id;
                Entry.DisplayName = Name;
            }
        }
    }

    // The packaged workspace still has a complete catalog if the optional
    // DataTable was renamed or omitted. IDs match BP_ConstellationShell's enum.
    if (Constellations.IsEmpty())
    {
        static const TCHAR* FallbackNames[] = {
            TEXT("Andromeda"), TEXT("Antlia"), TEXT("Apus"),
            TEXT("Aquarius"), TEXT("Aquila"), TEXT("Ara"), TEXT("Aries"),
            TEXT("Auriga"), TEXT("Bootes"), TEXT("Caelum"),
            TEXT("Camelopardalis"), TEXT("Cancer"), TEXT("CanesVenatici"),
            TEXT("CanisMajor"), TEXT("CanisMinor"), TEXT("Capricornus"),
            TEXT("Carina"), TEXT("Cassiopeia"), TEXT("Centaurus"),
            TEXT("Cepheus"), TEXT("Cetus"), TEXT("Chamaeleon"),
            TEXT("Circinus"), TEXT("Columba"), TEXT("ComaBerenices"),
            TEXT("CoronaAustralis"), TEXT("CoronaBorealis"),
            TEXT("Corvus"), TEXT("Crater"), TEXT("Crux"), TEXT("Cygnus"),
            TEXT("Delphinus"), TEXT("Dorado"), TEXT("Draco"),
            TEXT("Equuleus"), TEXT("Eridanus"), TEXT("Fornax"),
            TEXT("Gemini"), TEXT("Grus"), TEXT("Hercules"),
            TEXT("Horologium"), TEXT("Hydra"), TEXT("Hydrus"),
            TEXT("Indus"), TEXT("Lacerta"), TEXT("Leo"),
            TEXT("LeoMinor"), TEXT("Lepus"), TEXT("Libra"), TEXT("Lupus"),
            TEXT("Lynx"), TEXT("Lyra"), TEXT("Mensa"),
            TEXT("Microscopium"), TEXT("Monoceros"), TEXT("Musca"),
            TEXT("Norma"), TEXT("Octans"), TEXT("Ophiuchus"),
            TEXT("Orion"), TEXT("Pavo"), TEXT("Pegasus"), TEXT("Perseus"),
            TEXT("Phoenix"), TEXT("Pictor"), TEXT("Pisces"),
            TEXT("PiscisAustrinus"), TEXT("Puppis"), TEXT("Pyxis"),
            TEXT("Reticulum"), TEXT("Sagitta"), TEXT("Sagittarius"),
            TEXT("Scorpius"), TEXT("Sculptor"), TEXT("Scutum"),
            TEXT("Serpens"), TEXT("Sextans"), TEXT("Taurus"),
            TEXT("Telescopium"), TEXT("Triangulum"),
            TEXT("TriangulumAustrale"), TEXT("Tucana"), TEXT("UrsaMajor"),
            TEXT("UrsaMinor"), TEXT("Vela"), TEXT("Virgo"), TEXT("Volans"),
            TEXT("Vulpecula")};
        for (const TCHAR* Id : FallbackNames)
        {
            FTGVisualizationConstellationEntry& Entry =
                Constellations.AddDefaulted_GetRef();
            Entry.Id = Id;
            Entry.DisplayName = Id;
        }
    }

    Constellations.Sort([](
        const FTGVisualizationConstellationEntry& A,
        const FTGVisualizationConstellationEntry& B)
    {
        return A.DisplayName < B.DisplayName;
    });
}

void UTGVisualizationDockWorkspaceWidget::HandlePlaybackTimeChanged(
    double ElapsedSimulationSeconds,
    double EphemerisTimeTdbSeconds,
    double NormalizedTime)
{
    if (WorkspaceWidget.IsValid())
    {
        constexpr double TelemetryRefreshIntervalSeconds = 0.10;
        const double Now = FPlatformTime::Seconds();
        if (
            !IsValid(PlaybackActor) ||
            !PlaybackActor->IsPlaying() ||
            LastTelemetryRefreshRealSeconds < 0.0 ||
            Now - LastTelemetryRefreshRealSeconds >=
                TelemetryRefreshIntervalSeconds)
        {
            WorkspaceWidget->RefreshTelemetry();
            LastTelemetryRefreshRealSeconds = Now;
        }
        WorkspaceWidget->RefreshTimeline();
    }
}
