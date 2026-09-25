// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/TGScalarProfileCsvLibrary.h"

#include "Containers/UnrealString.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
    bool ParseScalarProfileStrictFiniteDouble(
        const FString& Text,
        double& OutValue)
    {
        const FString TrimmedText =
            Text.TrimStartAndEnd();

        if (TrimmedText.IsEmpty())
        {
            return false;
        }

        return LexTryParseString(
                   OutValue,
                   *TrimmedText)
            && FMath::IsFinite(OutValue);
    }

    FString NormalizeProfilePath(
        const FString& FilePath)
    {
        FString NormalizedPath =
            FPaths::ConvertRelativePathToFull(
                FilePath.TrimStartAndEnd());

        FPaths::NormalizeFilename(NormalizedPath);
        return NormalizedPath;
    }

    void AddLineError(
        FTGScalarProfileCsvInspection& Inspection,
        int32 LineNumber,
        const FString& Message)
    {
        Inspection.ErrorMessages.Add(
            FString::Printf(
                TEXT("Line %d: %s"),
                LineNumber,
                *Message));
    }

    FString GetQuantityName(
        ETGScalarProfileQuantity Quantity)
    {
        switch (Quantity)
        {
        case ETGScalarProfileQuantity::Thrust:
            return TEXT("Thrust");

        case ETGScalarProfileQuantity::SpecificImpulse:
            return TEXT("Specific-impulse");

        default:
            return TEXT("Scalar profile");
        }
    }

    FString GetQuantityUnit(
        ETGScalarProfileQuantity Quantity)
    {
        switch (Quantity)
        {
        case ETGScalarProfileQuantity::Thrust:
            return TEXT("N");

        case ETGScalarProfileQuantity::SpecificImpulse:
            return TEXT("s");

        default:
            return TEXT("");
        }
    }

    bool IsValueValidForQuantity(
        ETGScalarProfileQuantity Quantity,
        double Value,
        FString& OutError)
    {
        switch (Quantity)
        {
        case ETGScalarProfileQuantity::Thrust:
            if (Value < 0.0)
            {
                OutError =
                    TEXT(
                        "thrust values must be nonnegative.");
                return false;
            }

            return true;

        case ETGScalarProfileQuantity::SpecificImpulse:
            if (Value <= 0.0)
            {
                OutError =
                    TEXT(
                        "specific-impulse values must be positive.");
                return false;
            }

            return true;

        default:
            OutError =
                TEXT("unsupported scalar-profile quantity.");
            return false;
        }
    }

    void CalculateRanges(
        FTGScalarProfileCsvInspection& Inspection)
    {
        if (Inspection.Samples.IsEmpty())
        {
            return;
        }

        Inspection.FirstTimeSeconds =
            Inspection.Samples[0].TimeSeconds;

        Inspection.LastTimeSeconds =
            Inspection.Samples.Last().TimeSeconds;

        Inspection.MinimumValue =
            Inspection.Samples[0].Value;

        Inspection.MaximumValue =
            Inspection.Samples[0].Value;

        for (const FTGScalarCurveSample& Sample
             : Inspection.Samples)
        {
            Inspection.MinimumValue =
                FMath::Min(
                    Inspection.MinimumValue,
                    Sample.Value);

            Inspection.MaximumValue =
                FMath::Max(
                    Inspection.MaximumValue,
                    Sample.Value);
        }
    }
}

FTGScalarProfileCsvInspection
UTGScalarProfileCsvLibrary::
InspectScalarProfileCsv(
    const FString& FilePath,
    ETGScalarProfileQuantity Quantity)
{
    FTGScalarProfileCsvInspection Inspection;
    Inspection.Quantity = Quantity;

    if (FilePath.TrimStartAndEnd().IsEmpty())
    {
        Inspection.ErrorMessages.Add(
            TEXT("No CSV file was selected."));
        return Inspection;
    }

    Inspection.NormalizedFilePath =
        NormalizeProfilePath(FilePath);

    if (!FPaths::GetExtension(
             Inspection.NormalizedFilePath,
             true)
             .Equals(
                 TEXT(".csv"),
                 ESearchCase::IgnoreCase))
    {
        Inspection.ErrorMessages.Add(
            TEXT("The selected file must use the .csv extension."));
        return Inspection;
    }

    if (!IFileManager::Get().FileExists(
            *Inspection.NormalizedFilePath))
    {
        Inspection.ErrorMessages.Add(
            TEXT("The selected CSV file does not exist."));
        return Inspection;
    }

    FString FileContents;

    if (!FFileHelper::LoadFileToString(
            FileContents,
            *Inspection.NormalizedFilePath))
    {
        Inspection.ErrorMessages.Add(
            TEXT("The selected CSV file could not be read."));
        return Inspection;
    }

    TArray<FString> Lines;
    FileContents.ParseIntoArrayLines(
        Lines,
        false);

    bool bFoundNonemptyRow = false;
    bool bHasPreviousTime = false;
    double PreviousTimeSeconds = 0.0;

    for (int32 LineIndex = 0;
         LineIndex < Lines.Num();
         ++LineIndex)
    {
        const int32 LineNumber = LineIndex + 1;

        const FString TrimmedLine =
            Lines[LineIndex].TrimStartAndEnd();

        if (TrimmedLine.IsEmpty())
        {
            continue;
        }

        bFoundNonemptyRow = true;

        TArray<FString> Cells;
        TrimmedLine.ParseIntoArray(
            Cells,
            TEXT(","),
            false);

        if (Cells.Num() != 2)
        {
            AddLineError(
                Inspection,
                LineNumber,
                TEXT(
                    "expected exactly two columns: "
                    "time_seconds,value."));
            continue;
        }

        double TimeSeconds = 0.0;
        double Value = 0.0;

        if (!ParseScalarProfileStrictFiniteDouble(
                Cells[0],
                TimeSeconds))
        {
            AddLineError(
                Inspection,
                LineNumber,
                TEXT(
                    "time must be a finite numeric value."));
            continue;
        }

        if (!ParseScalarProfileStrictFiniteDouble(
                Cells[1],
                Value))
        {
            AddLineError(
                Inspection,
                LineNumber,
                TEXT(
                    "profile value must be a finite numeric value."));
            continue;
        }

        if (bHasPreviousTime
            && TimeSeconds <= PreviousTimeSeconds)
        {
            AddLineError(
                Inspection,
                LineNumber,
                TEXT(
                    "time values must be strictly increasing."));
            continue;
        }

        FString QuantityError;

        if (!IsValueValidForQuantity(
                Quantity,
                Value,
                QuantityError))
        {
            AddLineError(
                Inspection,
                LineNumber,
                QuantityError);
            continue;
        }

        FTGScalarCurveSample Sample;
        Sample.TimeSeconds = TimeSeconds;
        Sample.Value = Value;

        Inspection.Samples.Add(Sample);

        PreviousTimeSeconds = TimeSeconds;
        bHasPreviousTime = true;
    }

    if (!bFoundNonemptyRow)
    {
        Inspection.ErrorMessages.Add(
            TEXT("The selected CSV contains no sample rows."));
    }

    Inspection.SampleCount =
        Inspection.Samples.Num();

    if (Quantity
        == ETGScalarProfileQuantity::Thrust)
    {
        if (Inspection.SampleCount < 2)
        {
            Inspection.ErrorMessages.Add(
                TEXT(
                    "A thrust profile requires at least "
                    "two valid sample rows."));
        }

        if (Inspection.SampleCount > 0)
        {
            if (Inspection.Samples[0].Value != 0.0)
            {
                Inspection.ErrorMessages.Add(
                    TEXT(
                        "The first thrust value must equal zero."));
            }

            if (Inspection.Samples.Last().Value != 0.0)
            {
                Inspection.ErrorMessages.Add(
                    TEXT(
                        "The last thrust value must equal zero."));
            }
        }
    }

    CalculateRanges(Inspection);

    Inspection.bValid =
        Inspection.ErrorMessages.IsEmpty();

    return Inspection;
}

FText
UTGScalarProfileCsvLibrary::
FormatScalarProfileCsvInspectionForHud(
    const FTGScalarProfileCsvInspection& Inspection)
{
    const FString QuantityName =
        GetQuantityName(Inspection.Quantity);

    if (!Inspection.bValid)
    {
        const FString JoinedErrors =
            FString::Join(
                Inspection.ErrorMessages,
                TEXT("\n"));

        return FText::FromString(
            FString::Printf(
                TEXT(
                    "%s CSV rejected.\n%s"),
                *QuantityName,
                *JoinedErrors));
    }

    const FString Unit =
        GetQuantityUnit(Inspection.Quantity);

    return FText::FromString(
        FString::Printf(
            TEXT(
                "%s CSV accepted.\n"
                "Samples: %d\n"
                "Time range: %.9g to %.9g s\n"
                "Value range: %.9g to %.9g %s\n"
                "Interpolation: piecewise linear\n"
                "Outside range: endpoint clamping\n"
                "File: %s"),
            *QuantityName,
            Inspection.SampleCount,
            Inspection.FirstTimeSeconds,
            Inspection.LastTimeSeconds,
            Inspection.MinimumValue,
            Inspection.MaximumValue,
            *Unit,
            *Inspection.NormalizedFilePath));
}

bool
UTGScalarProfileCsvLibrary::
ValidateScalarProfileConstant(
    ETGScalarProfileQuantity Quantity,
    double Value,
    FText& OutErrorText)
{
    if (!FMath::IsFinite(Value))
    {
        OutErrorText =
            FText::FromString(
                TEXT(
                    "The constant value must be finite."));
        return false;
    }

    FString QuantityError;

    if (!IsValueValidForQuantity(
            Quantity,
            Value,
            QuantityError))
    {
        OutErrorText =
            FText::FromString(QuantityError);
        return false;
    }

    OutErrorText = FText::GetEmpty();
    return true;
}