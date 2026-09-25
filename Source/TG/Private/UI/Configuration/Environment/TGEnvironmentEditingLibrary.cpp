// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Environment/TGEnvironmentEditingLibrary.h"

#include "Components/WidgetSwitcher.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Simulation/TGCelestialCatalogLibrary.h"
#include "UI/Configuration/Environment/TGConfigAerodynamicsWidgetBase.h"
#include "UI/Configuration/Environment/TGConfigAtmosphereWidgetBase.h"
#include "UI/Configuration/Environment/TGConfigSolarRadiationPressureWidgetBase.h"

namespace TGEnvironmentEditingPrivate
{
    constexpr double UnitVectorTolerance = 1.0e-6;
    constexpr double ZeroVectorTolerance = 1.0e-12;

    FString NormalizePath(const FString& InputPath)
    {
        FString Result = InputPath.TrimStartAndEnd();
        if (Result.IsEmpty())
        {
            return Result;
        }

        Result = FPaths::ConvertRelativePathToFull(Result);
        FPaths::NormalizeFilename(Result);
        return Result;
    }

    bool IsFiniteVector(const FVector& Value)
    {
        return
            FMath::IsFinite(Value.X) &&
            FMath::IsFinite(Value.Y) &&
            FMath::IsFinite(Value.Z);
    }

    bool ParseFiniteDouble(
        const FString& Text,
        double& OutValue)
    {
        const FString Trimmed = Text.TrimStartAndEnd();
        return
            !Trimmed.IsEmpty() &&
            LexTryParseString(OutValue, *Trimmed) &&
            FMath::IsFinite(OutValue);
    }

    bool LoadCsvLines(
        const FString& CsvFilePath,
        TArray<FString>& OutNonEmptyLines,
        FString& OutNormalizedPath,
        FText& OutError)
    {
        OutNonEmptyLines.Reset();
        OutError = FText::GetEmpty();
        OutNormalizedPath = NormalizePath(CsvFilePath);

        if (OutNormalizedPath.IsEmpty())
        {
            OutError = FText::FromString(TEXT("Select a CSV file."));
            return false;
        }

        if (!FPaths::GetExtension(OutNormalizedPath).Equals(
                TEXT("csv"),
                ESearchCase::IgnoreCase))
        {
            OutError = FText::FromString(TEXT("Choose a file in CSV format (.csv)."));
            return false;
        }

        if (!FPaths::FileExists(OutNormalizedPath))
        {
            OutError = FText::FromString(
                TEXT("The selected file is no longer available. Choose it again or select another file."));
            return false;
        }

        TArray<FString> RawLines;
        if (!FFileHelper::LoadFileToStringArray(
                RawLines,
                *OutNormalizedPath))
        {
            OutError = FText::FromString(
                TEXT("The selected file could not be read. Confirm that it is accessible and not locked by another application."));
            return false;
        }

        for (const FString& RawLine : RawLines)
        {
            const FString Trimmed = RawLine.TrimStartAndEnd();
            if (!Trimmed.IsEmpty())
            {
                OutNonEmptyLines.Add(Trimmed);
            }
        }

        return true;
    }

    bool ParseExactColumnCount(
        const FString& Line,
        int32 ExpectedColumns,
        int32 RowNumber,
        TArray<double>& OutValues,
        FText& OutError)
    {
        TArray<FString> Fields;
        Line.ParseIntoArray(Fields, TEXT(","), false);

        if (Fields.Num() != ExpectedColumns)
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT("Row %d contains %d values; %d are required."),
                    RowNumber,
                    Fields.Num(),
                    ExpectedColumns));
            return false;
        }

        OutValues.SetNumUninitialized(ExpectedColumns);
        for (int32 ColumnIndex = 0;
             ColumnIndex < ExpectedColumns;
             ++ColumnIndex)
        {
            if (!ParseFiniteDouble(
                    Fields[ColumnIndex],
                    OutValues[ColumnIndex]))
            {
                OutError = FText::FromString(
                    FString::Printf(
                        TEXT("Row %d, column %d must contain a finite number."),
                        RowNumber,
                        ColumnIndex + 1));
                return false;
            }
        }

        return true;
    }

    FString PadLeft(const FString& Value, int32 Width)
    {
        return FString::ChrN(
                   FMath::Max(0, Width - Value.Len()),
                   TEXT(' ')) +
            Value;
    }

    FString PadRight(const FString& Value, int32 Width)
    {
        return Value + FString::ChrN(
            FMath::Max(0, Width - Value.Len()),
            TEXT(' '));
    }

    bool IsAtmosphereCentralBodyName(const FString& Candidate)
    {
        const FString Trimmed = Candidate.TrimStartAndEnd();
        for (const FTGCelestialCatalogEntry& Entry
             : UTGCelestialCatalogLibrary::GetCelestialCatalog())
        {
            if (Entry.SourceRole == ETGCelestialSourceRole::SystemBarycenter)
            {
                continue;
            }

            if (Entry.DisplayName.ToString().Equals(
                    Trimmed,
                    ESearchCase::IgnoreCase))
            {
                return true;
            }
        }

        return false;
    }

    bool ValidatePositiveFinite(
        double Value,
        const TCHAR* FieldName,
        FText& OutError)
    {
        if (!FMath::IsFinite(Value) || Value <= 0.0)
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT("%s must be a positive finite value."),
                    FieldName));
            return false;
        }

        return true;
    }

    bool ValidateNonNegativeFinite(
        double Value,
        const TCHAR* FieldName,
        FText& OutError)
    {
        if (!FMath::IsFinite(Value) || Value < 0.0)
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT("%s must be a nonnegative finite value."),
                    FieldName));
            return false;
        }

        return true;
    }
}

TArray<FString>
UTGEnvironmentEditingLibrary::GetAtmosphereCentralBodyNames()
{
    TArray<FString> Result;

    for (const FTGCelestialCatalogEntry& Entry
         : UTGCelestialCatalogLibrary::GetCelestialCatalog())
    {
        if (Entry.SourceRole == ETGCelestialSourceRole::SystemBarycenter)
        {
            continue;
        }

        Result.Add(Entry.DisplayName.ToString());
    }

    return Result;
}

int32
UTGEnvironmentEditingLibrary::GetFlattenedArticulationDofCount(
    const FTGSimulationScenario& Scenario)
{
    int32 Result = 0;
    for (const FTGComponentConfig& Component : Scenario.Components)
    {
        Result += Component.DegreesOfFreedom.Num();
    }

    return Result;
}

bool UTGEnvironmentEditingLibrary::BuildCsvPreview(
    const FString& CsvFilePath,
    const TArray<FString>& ColumnHeadings,
    int32 MaximumRows,
    FText& OutPreview,
    FText& OutError)
{
    OutPreview = FText::GetEmpty();
    OutError = FText::GetEmpty();

    if (ColumnHeadings.IsEmpty())
    {
        OutError = FText::FromString(
            TEXT("The CSV preview requires at least one column heading."));
        return false;
    }

    if (MaximumRows <= 0)
    {
        OutError = FText::FromString(
            TEXT("The CSV preview row limit must be positive."));
        return false;
    }

    TArray<FString> Lines;
    FString NormalizedPath;
    if (!TGEnvironmentEditingPrivate::LoadCsvLines(
            CsvFilePath,
            Lines,
            NormalizedPath,
            OutError))
    {
        return false;
    }

    if (Lines.IsEmpty())
    {
        OutError = FText::FromString(
            TEXT("The selected CSV file contains no data rows."));
        return false;
    }

    const int32 DisplayedRowCount = FMath::Min(MaximumRows, Lines.Num());
    TArray<TArray<FString>> Rows;
    Rows.Reserve(DisplayedRowCount);

    TArray<int32> ColumnWidths;
    ColumnWidths.Reserve(ColumnHeadings.Num());
    for (const FString& Heading : ColumnHeadings)
    {
        ColumnWidths.Add(Heading.Len());
    }

    for (int32 RowIndex = 0; RowIndex < DisplayedRowCount; ++RowIndex)
    {
        TArray<FString> Fields;
        Lines[RowIndex].ParseIntoArray(Fields, TEXT(","), false);
        if (Fields.Num() != ColumnHeadings.Num())
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT("CSV preview row %d contains %d values; %d are required."),
                    RowIndex + 1,
                    Fields.Num(),
                    ColumnHeadings.Num()));
            return false;
        }

        for (int32 ColumnIndex = 0;
             ColumnIndex < Fields.Num();
             ++ColumnIndex)
        {
            Fields[ColumnIndex] = Fields[ColumnIndex].TrimStartAndEnd();
            ColumnWidths[ColumnIndex] = FMath::Max(
                ColumnWidths[ColumnIndex],
                Fields[ColumnIndex].Len());
        }
        Rows.Add(MoveTemp(Fields));
    }

    const int32 RowNumberWidth = FMath::Max(
        3,
        FString::FromInt(Lines.Num()).Len());

    FString Result;
    Result.Reserve(DisplayedRowCount * 128);
    Result += DisplayedRowCount == Lines.Num()
        ? FString::Printf(TEXT("Showing all %d rows.\n\n"), Lines.Num())
        : FString::Printf(
            TEXT("Showing the first %d of %d rows.\n\n"),
            DisplayedRowCount,
            Lines.Num());

    Result += TGEnvironmentEditingPrivate::PadRight(
        TEXT("Row"),
        RowNumberWidth);
    for (int32 ColumnIndex = 0;
         ColumnIndex < ColumnHeadings.Num();
         ++ColumnIndex)
    {
        Result += TEXT(" | ");
        Result += TGEnvironmentEditingPrivate::PadRight(
            ColumnHeadings[ColumnIndex],
            ColumnWidths[ColumnIndex]);
    }
    Result += TEXT("\n");

    Result += FString::ChrN(RowNumberWidth, TEXT('-'));
    for (const int32 ColumnWidth : ColumnWidths)
    {
        Result += TEXT("-+-");
        Result += FString::ChrN(ColumnWidth, TEXT('-'));
    }
    Result += TEXT("\n");

    for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
    {
        Result += TGEnvironmentEditingPrivate::PadLeft(
            FString::FromInt(RowIndex + 1),
            RowNumberWidth);
        for (int32 ColumnIndex = 0;
             ColumnIndex < Rows[RowIndex].Num();
             ++ColumnIndex)
        {
            Result += TEXT(" | ");
            Result += TGEnvironmentEditingPrivate::PadLeft(
                Rows[RowIndex][ColumnIndex],
                ColumnWidths[ColumnIndex]);
        }
        if (RowIndex + 1 < Rows.Num())
        {
            Result += TEXT("\n");
        }
    }

    OutPreview = FText::FromString(MoveTemp(Result));
    return true;
}

bool
UTGEnvironmentEditingLibrary::ValidateGeneralAtmosphereProfileCsv(
    const FString& CsvFilePath,
    FText& OutSummary,
    FText& OutError)
{
    OutSummary = FText::GetEmpty();
    OutError = FText::GetEmpty();

    TArray<FString> Lines;
    FString NormalizedPath;
    if (!TGEnvironmentEditingPrivate::LoadCsvLines(
            CsvFilePath,
            Lines,
            NormalizedPath,
            OutError))
    {
        return false;
    }

    if (Lines.Num() < 2)
    {
        OutError = FText::FromString(
            TEXT("The selected atmosphere profile must contain at least two populated rows."));
        return false;
    }

    double FirstAltitude = 0.0;
    double LastAltitude = 0.0;
    double PreviousAltitude = 0.0;

    for (int32 RowIndex = 0; RowIndex < Lines.Num(); ++RowIndex)
    {
        TArray<double> Values;
        if (!TGEnvironmentEditingPrivate::ParseExactColumnCount(
                Lines[RowIndex],
                5,
                RowIndex + 1,
                Values,
                OutError))
        {
            return false;
        }

        const double Altitude = Values[0];
        if (RowIndex > 0 && Altitude <= PreviousAltitude)
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT("Altitude must increase from one row to the next. Check row %d."),
                    RowIndex + 1));
            return false;
        }

        static const TCHAR* PropertyNames[] =
        {
            TEXT("density"),
            TEXT("temperature"),
            TEXT("mean particle mass"),
            TEXT("effective collision cross section")
        };
        for (int32 PropertyColumn = 1; PropertyColumn < 5; ++PropertyColumn)
        {
            if (Values[PropertyColumn] <= 0.0)
            {
                OutError = FText::FromString(
                    FString::Printf(
                        TEXT("Row %d contains an invalid %s value. Enter a value greater than zero."),
                        RowIndex + 1,
                        PropertyNames[PropertyColumn - 1]));
                return false;
            }
        }

        if (RowIndex == 0)
        {
            FirstAltitude = Altitude;
        }

        LastAltitude = Altitude;
        PreviousAltitude = Altitude;
    }

    OutSummary = FText::FromString(
        FString::Printf(
            TEXT("Valid profile: %d rows, altitude %.6g to %.6g m."),
            Lines.Num(),
            FirstAltitude,
            LastAltitude));
    return true;
}

bool
UTGEnvironmentEditingLibrary::ValidateChpCoefficientCsv(
    const FString& CsvFilePath,
    FText& OutSummary,
    FText& OutError)
{
    OutSummary = FText::GetEmpty();
    OutError = FText::GetEmpty();

    TArray<FString> Lines;
    FString NormalizedPath;
    if (!TGEnvironmentEditingPrivate::LoadCsvLines(
            CsvFilePath,
            Lines,
            NormalizedPath,
            OutError))
    {
        return false;
    }

    if (Lines.Num() != 50)
    {
        OutError = FText::FromString(
            FString::Printf(
                TEXT("The density-envelope coefficient table requires 50 populated rows. The selected file contains %d."),
                Lines.Num()));
        return false;
    }

    for (int32 RowIndex = 0; RowIndex < Lines.Num(); ++RowIndex)
    {
        TArray<double> Values;
        if (!TGEnvironmentEditingPrivate::ParseExactColumnCount(
                Lines[RowIndex],
                8,
                RowIndex + 1,
                Values,
                OutError))
        {
            return false;
        }
    }

    OutSummary = FText::FromString(TEXT("Ready: 50 altitude rows with eight density-envelope coefficients per row."));
    return true;
}

bool
UTGEnvironmentEditingLibrary::ValidateChpMolecularProfileCsv(
    const FString& CsvFilePath,
    FText& OutSummary,
    FText& OutError)
{
    OutSummary = FText::GetEmpty();
    OutError = FText::GetEmpty();

    TArray<FString> Lines;
    FString NormalizedPath;
    if (!TGEnvironmentEditingPrivate::LoadCsvLines(
            CsvFilePath,
            Lines,
            NormalizedPath,
            OutError))
    {
        return false;
    }

    if (Lines.Num() < 2)
    {
        OutError = FText::FromString(
            TEXT("The thermodynamic and molecular profile must contain at least two populated rows."));
        return false;
    }

    double FirstAltitude = 0.0;
    double LastAltitude = 0.0;
    double PreviousAltitude = 0.0;

    for (int32 RowIndex = 0; RowIndex < Lines.Num(); ++RowIndex)
    {
        TArray<double> Values;
        if (!TGEnvironmentEditingPrivate::ParseExactColumnCount(
                Lines[RowIndex],
                4,
                RowIndex + 1,
                Values,
                OutError))
        {
            return false;
        }

        const double Altitude = Values[0];
        if (RowIndex > 0 && Altitude <= PreviousAltitude)
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT("Altitude must increase from one row to the next. Check row %d."),
                    RowIndex + 1));
            return false;
        }

        static const TCHAR* PropertyNames[] =
        {
            TEXT("temperature"),
            TEXT("mean particle mass"),
            TEXT("effective collision cross section")
        };
        for (int32 PropertyColumn = 1; PropertyColumn < 4; ++PropertyColumn)
        {
            if (Values[PropertyColumn] <= 0.0)
            {
                OutError = FText::FromString(
                    FString::Printf(
                        TEXT("Row %d contains an invalid %s value. Enter a value greater than zero."),
                        RowIndex + 1,
                        PropertyNames[PropertyColumn - 1]));
                return false;
            }
        }

        if (RowIndex == 0)
        {
            FirstAltitude = Altitude;
        }

        LastAltitude = Altitude;
        PreviousAltitude = Altitude;
    }

    OutSummary = FText::FromString(
        FString::Printf(
            TEXT("Ready: %d profile rows spanning %.6g to %.6g m altitude."),
            Lines.Num(),
            FirstAltitude,
            LastAltitude));
    return true;
}

bool
UTGEnvironmentEditingLibrary::ValidateAerodynamicDatabaseCsv(
    const FString& CsvFilePath,
    int32 ExpectedArticulationCoordinateCount,
    FText& OutSummary,
    FText& OutError)
{
    OutSummary = FText::GetEmpty();
    OutError = FText::GetEmpty();

    if (ExpectedArticulationCoordinateCount < 0)
    {
        OutError = FText::FromString(TEXT("The expected articulation-coordinate count cannot be negative."));
        return false;
    }

    TArray<FString> Lines;
    FString NormalizedPath;
    if (!TGEnvironmentEditingPrivate::LoadCsvLines(
            CsvFilePath,
            Lines,
            NormalizedPath,
            OutError))
    {
        return false;
    }

    if (Lines.IsEmpty())
    {
        OutError = FText::FromString(TEXT("The aerodynamic coefficient database contains no data rows."));
        return false;
    }

    const int32 ExpectedColumns =
        ExpectedArticulationCoordinateCount + 11;

    TSet<FString> IndependentPointKeys;

    for (int32 RowIndex = 0; RowIndex < Lines.Num(); ++RowIndex)
    {
        TArray<double> Values;
        if (!TGEnvironmentEditingPrivate::ParseExactColumnCount(
                Lines[RowIndex],
                ExpectedColumns,
                RowIndex + 1,
                Values,
                OutError))
        {
            return false;
        }

        if (Values[0] < 0.0)
        {
            OutError = FText::FromString(
                FString::Printf(TEXT("Aerodynamic database row %d has a negative molecular speed ratio."), RowIndex + 1));
            return false;
        }

        if (Values[1] <= 0.0)
        {
            OutError = FText::FromString(
                FString::Printf(TEXT("Aerodynamic database row %d must have a positive Knudsen number."), RowIndex + 1));
            return false;
        }

        const FVector FlowDirection(
            Values[2],
            Values[3],
            Values[4]);

        const double FlowNorm = FlowDirection.Size();
        if (!FMath::IsFinite(FlowNorm) || FlowNorm <= TGEnvironmentEditingPrivate::ZeroVectorTolerance)
        {
            OutError = FText::FromString(
                FString::Printf(TEXT("Aerodynamic database row %d has a zero or invalid gas-flow direction."), RowIndex + 1));
            return false;
        }

        if (FMath::Abs(FlowNorm - 1.0) > TGEnvironmentEditingPrivate::UnitVectorTolerance)
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT("Aerodynamic database row %d must have a unit-length gas-flow direction; its magnitude is %.9g."),
                    RowIndex + 1,
                    FlowNorm));
            return false;
        }

        FString Key;
        const int32 IndependentValueCount =
            5 + ExpectedArticulationCoordinateCount;
        for (int32 ValueIndex = 0;
             ValueIndex < IndependentValueCount;
             ++ValueIndex)
        {
            Key += FString::Printf(TEXT("%.17g|"), Values[ValueIndex]);
        }

        if (IndependentPointKeys.Contains(Key))
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT("Aerodynamic database row %d duplicates an earlier independent-variable point."),
                    RowIndex + 1));
            return false;
        }

        IndependentPointKeys.Add(Key);
    }

    OutSummary = FText::FromString(
        FString::Printf(
            TEXT("Ready: %d rows with %d articulation-coordinate columns and %d total columns."),
            Lines.Num(),
            ExpectedArticulationCoordinateCount,
            ExpectedColumns));
    return true;
}

bool
UTGEnvironmentEditingLibrary::ValidateAtmosphereScenario(
    const FTGSimulationScenario& Scenario,
    FText& OutWarning,
    FText& OutError)
{
    OutWarning = FText::GetEmpty();
    OutError = FText::GetEmpty();

    const FTGAtmosphereConfig& Atmosphere = Scenario.Atmosphere;
    if (!Atmosphere.bEnabled)
    {
        return true;
    }

    if (!TGEnvironmentEditingPrivate::IsAtmosphereCentralBodyName(
            Atmosphere.CentralBodyName))
    {
        OutError = FText::FromString(TEXT("Atmosphere central body must resolve to a physical celestial-body catalog entry."));
        return false;
    }

    FText Summary;
    if (Atmosphere.Model == ETGAtmosphereModel::UploadedProfile)
    {
        return ValidateGeneralAtmosphereProfileCsv(
            Atmosphere.GeneralProfileCsvPath,
            Summary,
            OutError);
    }

    if (!Atmosphere.CentralBodyName.Equals(TEXT("Earth"), ESearchCase::IgnoreCase))
    {
        OutError = FText::FromString(TEXT("Cubic Harris-Priester is Earth-only; select Earth as the atmosphere central body."));
        return false;
    }

    if (!TGEnvironmentEditingPrivate::ValidatePositiveFinite(
            Atmosphere.CenteredAverageF107SolarFluxUnits,
            TEXT("Centered 81-day average F10.7"),
            OutError))
    {
        return false;
    }

    if (!ValidateChpCoefficientCsv(
            Atmosphere.ChpCoefficientCsvPath,
            Summary,
            OutError))
    {
        return false;
    }

    return ValidateChpMolecularProfileCsv(
        Atmosphere.ChpMolecularProfileCsvPath,
        Summary,
        OutError);
}

bool
UTGEnvironmentEditingLibrary::ValidateAerodynamicsScenario(
    const FTGSimulationScenario& Scenario,
    FText& OutWarning,
    FText& OutError)
{
    OutWarning = FText::GetEmpty();
    OutError = FText::GetEmpty();

    const FTGAerodynamicsConfig& Aero = Scenario.Aerodynamics;
    if (!Aero.bEnabled)
    {
        return true;
    }

    if (!Scenario.Atmosphere.bEnabled)
    {
        OutError = FText::FromString(TEXT("Aerodynamics requires Atmosphere to be enabled."));
        return false;
    }

    if (!TGEnvironmentEditingPrivate::ValidatePositiveFinite(
            Aero.ReferenceAreaSquareMeters,
            TEXT("Aerodynamic reference area"),
            OutError) ||
        !TGEnvironmentEditingPrivate::ValidatePositiveFinite(
            Aero.ReferenceLengthMeters,
            TEXT("Aerodynamic reference length"),
            OutError) ||
        !TGEnvironmentEditingPrivate::ValidateNonNegativeFinite(
            Aero.MinimumDynamicPressurePascals,
            TEXT("Minimum dynamic pressure"),
            OutError) ||
        !TGEnvironmentEditingPrivate::ValidateNonNegativeFinite(
            Aero.MaximumValidDynamicPressurePascals,
            TEXT("Maximum valid dynamic pressure"),
            OutError))
    {
        return false;
    }

    if (Aero.MaximumValidDynamicPressurePascals <
        Aero.MinimumDynamicPressurePascals)
    {
        OutError = FText::FromString(TEXT("Maximum valid dynamic pressure must be greater than or equal to minimum dynamic pressure."));
        return false;
    }

    if (!Aero.Database.bEnabled &&
        !Aero.bEnableConstantDragFallback)
    {
        OutError = FText::FromString(TEXT("Enable the aerodynamic coefficient database, the constant-drag fallback, or both."));
        return false;
    }

    if (Aero.bEnableConstantDragFallback &&
        !TGEnvironmentEditingPrivate::ValidatePositiveFinite(
            Aero.FallbackDragCoefficient,
            TEXT("Fallback drag coefficient"),
            OutError))
    {
        return false;
    }

    if (Aero.Database.bEnabled)
    {
        if (Aero.Database.Interpolation ==
            ETGAerodynamicDatabaseInterpolation::InverseDistance)
        {
            if (Aero.Database.NeighborCount <= 0)
            {
                OutError = FText::FromString(TEXT("Aerodynamic database neighbor count must be a positive integer."));
                return false;
            }

            if (!TGEnvironmentEditingPrivate::ValidatePositiveFinite(
                    Aero.Database.InverseDistancePower,
                    TEXT("Inverse-distance power"),
                    OutError))
            {
                return false;
            }
        }

        if (Aero.Database.bUseMaximumNormalizedNeighborDistance &&
            !TGEnvironmentEditingPrivate::ValidatePositiveFinite(
                Aero.Database.MaximumNormalizedNeighborDistance,
                TEXT("Maximum normalized neighbor distance"),
                OutError))
        {
            return false;
        }

        if (!TGEnvironmentEditingPrivate::IsFiniteVector(
                Aero.Database.MomentReferenceCenterBodyMeters))
        {
            OutError = FText::FromString(TEXT("Aerodynamic moment reference center must contain finite values."));
            return false;
        }

        if (Aero.Database.Extrapolation ==
                ETGAerodynamicDatabaseExtrapolation::ConstantDragFallback &&
            !Aero.bEnableConstantDragFallback)
        {
            OutError = FText::FromString(TEXT("Database extrapolation is set to Constant-Drag Fallback, but the constant-drag fallback is disabled."));
            return false;
        }

        FText Summary;
        if (!ValidateAerodynamicDatabaseCsv(
                Aero.Database.CsvFilePath,
                GetFlattenedArticulationDofCount(Scenario),
                Summary,
                OutError))
        {
            return false;
        }
    }

    if (Aero.bEnableConstantDragFallback)
    {
        OutWarning = FText::FromString(
            TEXT("Constant-drag fallback is translation-only: it produces no aerodynamic attitude torque or articulated-joint load."));
    }

    return true;
}

bool
UTGEnvironmentEditingLibrary::
    RefreshSolarRadiationPressurePanelInSwitcher(
        UWidgetSwitcher* Switcher)
{
    if (Switcher == nullptr)
    {
        return false;
    }

    for (
        int32 Index = 0;
        Index < Switcher->GetNumWidgets();
        ++Index)
    {
        if (
            UTGConfigSolarRadiationPressureWidgetBase* Panel =
                Cast<UTGConfigSolarRadiationPressureWidgetBase>(
                    Switcher->GetWidgetAtIndex(Index)))
        {
            Panel->RefreshFromCurrentDraft();
            return true;
        }
    }

    return false;
}

bool
UTGEnvironmentEditingLibrary::
    ActivateSolarRadiationPressurePanelInSwitcher(
        UWidgetSwitcher* Switcher,
        ATGSpacecraftPreviewPawn* PreviewPawn,
        ATGSpacecraftVisualActor* SpacecraftActor)
{
    if (Switcher == nullptr ||
        PreviewPawn == nullptr ||
        SpacecraftActor == nullptr)
    {
        return false;
    }

    for (
        int32 Index = 0;
        Index < Switcher->GetNumWidgets();
        ++Index)
    {
        if (
            UTGConfigSolarRadiationPressureWidgetBase* Panel =
                Cast<UTGConfigSolarRadiationPressureWidgetBase>(
                    Switcher->GetWidgetAtIndex(Index)))
        {
            return Panel->ActivateSolarRadiationPressurePreview(
                PreviewPawn,
                SpacecraftActor);
        }
    }

    return false;
}

bool
UTGEnvironmentEditingLibrary::
    DeactivateSolarRadiationPressurePanelInSwitcher(
        UWidgetSwitcher* Switcher)
{
    if (Switcher == nullptr)
    {
        return false;
    }

    for (
        int32 Index = 0;
        Index < Switcher->GetNumWidgets();
        ++Index)
    {
        if (
            UTGConfigSolarRadiationPressureWidgetBase* Panel =
                Cast<UTGConfigSolarRadiationPressureWidgetBase>(
                    Switcher->GetWidgetAtIndex(Index)))
        {
            Panel->DeactivateSolarRadiationPressurePreview();
            return true;
        }
    }

    return false;
}

bool
UTGEnvironmentEditingLibrary::RefreshAtmospherePanelInSwitcher(
    UWidgetSwitcher* Switcher)
{
    if (Switcher == nullptr || Switcher->GetNumWidgets() <= 7)
    {
        return false;
    }

    UTGConfigAtmosphereWidgetBase* Panel =
        Cast<UTGConfigAtmosphereWidgetBase>(
            Switcher->GetWidgetAtIndex(7));

    if (Panel == nullptr)
    {
        return false;
    }

    Panel->RefreshFromCurrentDraft();
    return true;
}

bool
UTGEnvironmentEditingLibrary::RefreshAerodynamicsPanelInSwitcher(
    UWidgetSwitcher* Switcher)
{
    if (Switcher == nullptr || Switcher->GetNumWidgets() <= 8)
    {
        return false;
    }

    UTGConfigAerodynamicsWidgetBase* Panel =
        Cast<UTGConfigAerodynamicsWidgetBase>(
            Switcher->GetWidgetAtIndex(8));

    if (Panel == nullptr)
    {
        return false;
    }

    Panel->RefreshFromCurrentDraft();
    return true;
}
