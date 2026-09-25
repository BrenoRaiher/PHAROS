// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/TGHarmonicCsvLibrary.h"

#include "Containers/UnrealString.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
    bool ParseStrictFiniteDouble(
        const FString& SourceText,
        double& OutValue)
    {
        FString TrimmedText = SourceText;
        TrimmedText.TrimStartAndEndInline();

        if (TrimmedText.IsEmpty())
        {
            return false;
        }

        if (!LexTryParseString(OutValue, *TrimmedText))
        {
            return false;
        }

        return FMath::IsFinite(OutValue);
    }

    bool ParseStrictInteger(
        const FString& SourceText,
        int32& OutValue)
    {
        FString TrimmedText = SourceText;
        TrimmedText.TrimStartAndEndInline();

        if (TrimmedText.IsEmpty())
        {
            return false;
        }

        /*
         * This deliberately rejects values such as:
         *
         * 2.0
         * 2e0
         * degree
         */
        return LexTryParseString(OutValue, *TrimmedText);
    }

    uint64 MakeDegreeOrderKey(
        const int32 Degree,
        const int32 Order)
    {
        return
            (static_cast<uint64>(static_cast<uint32>(Degree)) << 32)
            | static_cast<uint32>(Order);
    }

    FString FormatDoubleForInspection(const double Value)
    {
        return FString::Printf(TEXT("%.17g"), Value);
    }

    FString PadLeft(
        const FString& Value,
        const int32 Width)
    {
        return FString::ChrN(
                   FMath::Max(0, Width - Value.Len()),
                   TEXT(' '))
            + Value;
    }

    FString PadRight(
        const FString& Value,
        const int32 Width)
    {
        return Value + FString::ChrN(
                           FMath::Max(0, Width - Value.Len()),
                           TEXT(' '));
    }

    FText FormatInspectionErrors(
        const FTGHarmonicCsvInspection& Inspection)
    {
        if (Inspection.ErrorMessages.IsEmpty())
        {
            return FText::FromString(
                TEXT(
                    "The selected harmonic-model CSV could not be "
                    "validated."));
        }

        TArray<FString> Lines;
        Lines.Reserve(Inspection.ErrorMessages.Num() + 1);
        Lines.Add(
            TEXT("The selected harmonic-model CSV is invalid:"));

        for (const FString& Error : Inspection.ErrorMessages)
        {
            Lines.Add(FString::Printf(TEXT("- %s"), *Error));
        }

        return FText::FromString(
            FString::Join(Lines, TEXT("\n")));
    }
}

bool UTGHarmonicCsvLibrary::InspectHarmonicModelCsv(
    const FString& FilePath,
    FTGHarmonicCsvInspection& Inspection)
{
    Inspection = FTGHarmonicCsvInspection();

    if (FilePath.TrimStartAndEnd().IsEmpty())
    {
        Inspection.ErrorMessages.Add(
            TEXT("No file path was provided."));

        return false;
    }

    FString NormalizedPath =
        FPaths::ConvertRelativePathToFull(FilePath);

    FPaths::NormalizeFilename(NormalizedPath);

    Inspection.NormalizedFilePath = NormalizedPath;

    FString FileContents;

    if (!FFileHelper::LoadFileToString(
            FileContents,
            *NormalizedPath))
    {
        Inspection.ErrorMessages.Add(
            TEXT("The file could not be opened or read."));

        return false;
    }

    TArray<FString> Lines;
    FileContents.ParseIntoArrayLines(Lines, false);

    bool bFoundModelConstantsRow = false;
    TSet<uint64> DegreeOrderPairs;

    for (int32 LineIndex = 0;
         LineIndex < Lines.Num();
         ++LineIndex)
    {
        FString Line = Lines[LineIndex];

        /*
         * Remove an optional UTF byte-order mark from the first line.
         */
        if (LineIndex == 0
            && !Line.IsEmpty()
            && Line[0] == static_cast<TCHAR>(0xFEFF))
        {
            Line = Line.RightChop(1);
        }

        Line.TrimStartAndEndInline();

        if (Line.IsEmpty())
        {
            continue;
        }

        const int32 HumanLineNumber = LineIndex + 1;

        TArray<FString> Cells;
        Line.ParseIntoArray(Cells, TEXT(","), false);

        if (!bFoundModelConstantsRow)
        {
            bFoundModelConstantsRow = true;

            if (Cells.Num() != 2)
            {
                Inspection.ErrorMessages.Add(
                    FString::Printf(
                        TEXT(
                            "Line %d: the first nonempty row must "
                            "contain exactly GM and reference radius."
                        ),
                        HumanLineNumber));

                continue;
            }

            double ModelGM = 0.0;
            double ModelRadius = 0.0;

            const bool bValidGM =
                ParseStrictFiniteDouble(Cells[0], ModelGM);

            const bool bValidRadius =
                ParseStrictFiniteDouble(Cells[1], ModelRadius);

            if (!bValidGM || ModelGM <= 0.0)
            {
                Inspection.ErrorMessages.Add(
                    FString::Printf(
                        TEXT(
                            "Line %d: model GM must be a positive "
                            "finite value in m^3/s^2."
                        ),
                        HumanLineNumber));
            }
            else
            {
                Inspection.ModelGravitationalParameterM3PerS2 =
                    ModelGM;
            }

            if (!bValidRadius || ModelRadius <= 0.0)
            {
                Inspection.ErrorMessages.Add(
                    FString::Printf(
                        TEXT(
                            "Line %d: model reference radius must "
                            "be a positive finite value in meters."
                        ),
                        HumanLineNumber));
            }
            else
            {
                Inspection.ModelReferenceRadiusMeters =
                    ModelRadius;
            }

            continue;
        }

        if (Cells.Num() != 4)
        {
            Inspection.ErrorMessages.Add(
                FString::Printf(
                    TEXT(
                        "Line %d: coefficient rows must contain "
                        "exactly n,m,Cbar_nm,Sbar_nm."
                    ),
                    HumanLineNumber));

            continue;
        }

        int32 Degree = 0;
        int32 Order = 0;
        double Cbar = 0.0;
        double Sbar = 0.0;

        bool bRowValid = true;

        if (!ParseStrictInteger(Cells[0], Degree))
        {
            Inspection.ErrorMessages.Add(
                FString::Printf(
                    TEXT("Line %d: degree n must be an integer."),
                    HumanLineNumber));

            bRowValid = false;
        }

        if (!ParseStrictInteger(Cells[1], Order))
        {
            Inspection.ErrorMessages.Add(
                FString::Printf(
                    TEXT("Line %d: order m must be an integer."),
                    HumanLineNumber));

            bRowValid = false;
        }

        if (!ParseStrictFiniteDouble(Cells[2], Cbar))
        {
            Inspection.ErrorMessages.Add(
                FString::Printf(
                    TEXT(
                        "Line %d: Cbar_nm must be a finite number."
                    ),
                    HumanLineNumber));

            bRowValid = false;
        }

        if (!ParseStrictFiniteDouble(Cells[3], Sbar))
        {
            Inspection.ErrorMessages.Add(
                FString::Printf(
                    TEXT(
                        "Line %d: Sbar_nm must be a finite number."
                    ),
                    HumanLineNumber));

            bRowValid = false;
        }

        if (!bRowValid)
        {
            continue;
        }

        if (Degree < 0)
        {
            Inspection.ErrorMessages.Add(
                FString::Printf(
                    TEXT("Line %d: degree n cannot be negative."),
                    HumanLineNumber));

            bRowValid = false;
        }

        if (Order < 0)
        {
            Inspection.ErrorMessages.Add(
                FString::Printf(
                    TEXT("Line %d: order m cannot be negative."),
                    HumanLineNumber));

            bRowValid = false;
        }

        if (Order > Degree)
        {
            Inspection.ErrorMessages.Add(
                FString::Printf(
                    TEXT(
                        "Line %d: order m cannot exceed degree n."
                    ),
                    HumanLineNumber));

            bRowValid = false;
        }

        if (Degree == 0 && Order == 0)
        {
            Inspection.ErrorMessages.Add(
                FString::Printf(
                    TEXT(
                        "Line %d: an explicit coefficient (0,0) "
                        "is not permitted."
                    ),
                    HumanLineNumber));

            bRowValid = false;
        }

        if (!bRowValid)
        {
            continue;
        }

        const uint64 PairKey =
            MakeDegreeOrderKey(Degree, Order);

        if (DegreeOrderPairs.Contains(PairKey))
        {
            Inspection.ErrorMessages.Add(
                FString::Printf(
                    TEXT(
                        "Line %d: duplicate coefficient pair "
                        "(%d,%d)."
                    ),
                    HumanLineNumber,
                    Degree,
                    Order));

            continue;
        }

        DegreeOrderPairs.Add(PairKey);

        Inspection.MaximumAvailableDegree =
            FMath::Max(
                Inspection.MaximumAvailableDegree,
                Degree);

        Inspection.MaximumAvailableOrder =
            FMath::Max(
                Inspection.MaximumAvailableOrder,
                Order);

        ++Inspection.CoefficientCount;
    }

    if (!bFoundModelConstantsRow)
    {
        Inspection.ErrorMessages.Add(
            TEXT("The file contains no nonempty rows."));
    }

    Inspection.bValid =
        Inspection.ErrorMessages.IsEmpty();

    return Inspection.bValid;
}

bool UTGHarmonicCsvLibrary::BuildHarmonicModelCsvPreview(
    const FString& FilePath,
    const int32 MaximumCoefficientRows,
    FTGHarmonicCsvInspection& OutInspection,
    FText& OutPreview,
    FText& OutError)
{
    OutInspection = FTGHarmonicCsvInspection();
    OutPreview = FText::GetEmpty();
    OutError = FText::GetEmpty();

    if (MaximumCoefficientRows <= 0)
    {
        OutError = FText::FromString(
            TEXT(
                "The harmonic-model CSV preview row limit must be "
                "positive."));
        return false;
    }

    if (!InspectHarmonicModelCsv(FilePath, OutInspection))
    {
        OutError = FormatInspectionErrors(OutInspection);
        return false;
    }

    FString FileContents;
    if (!FFileHelper::LoadFileToString(
            FileContents,
            *OutInspection.NormalizedFilePath))
    {
        OutError = FText::FromString(
            TEXT(
                "The selected harmonic-model CSV could not be read "
                "for preview."));
        return false;
    }

    TArray<FString> SourceLines;
    FileContents.ParseIntoArrayLines(SourceLines, false);

    TArray<TArray<FString>> CoefficientRows;
    CoefficientRows.Reserve(OutInspection.CoefficientCount);

    bool bSkippedModelConstants = false;
    for (int32 LineIndex = 0;
         LineIndex < SourceLines.Num();
         ++LineIndex)
    {
        FString Line = SourceLines[LineIndex];
        if (LineIndex == 0 && !Line.IsEmpty() &&
            Line[0] == static_cast<TCHAR>(0xFEFF))
        {
            Line = Line.RightChop(1);
        }

        Line.TrimStartAndEndInline();
        if (Line.IsEmpty())
        {
            continue;
        }

        if (!bSkippedModelConstants)
        {
            bSkippedModelConstants = true;
            continue;
        }

        TArray<FString> Fields;
        Line.ParseIntoArray(Fields, TEXT(","), false);
        if (Fields.Num() != 4)
        {
            OutError = FText::FromString(
                TEXT(
                    "The selected harmonic-model CSV changed while "
                    "its preview was being prepared."));
            OutPreview = FText::GetEmpty();
            return false;
        }

        for (FString& Field : Fields)
        {
            Field.TrimStartAndEndInline();
        }

        CoefficientRows.Add(MoveTemp(Fields));
    }

    const int32 DisplayedRowCount = FMath::Min(
        MaximumCoefficientRows,
        CoefficientRows.Num());

    const TArray<FString> ColumnHeadings =
    {
        TEXT("Degree"),
        TEXT("Order"),
        TEXT("Normalized Cosine"),
        TEXT("Normalized Sine")
    };

    TArray<int32> ColumnWidths;
    ColumnWidths.Reserve(ColumnHeadings.Num());
    for (const FString& Heading : ColumnHeadings)
    {
        ColumnWidths.Add(Heading.Len());
    }

    for (int32 RowIndex = 0;
         RowIndex < DisplayedRowCount;
         ++RowIndex)
    {
        for (int32 ColumnIndex = 0;
             ColumnIndex < ColumnHeadings.Num();
             ++ColumnIndex)
        {
            ColumnWidths[ColumnIndex] = FMath::Max(
                ColumnWidths[ColumnIndex],
                CoefficientRows[RowIndex][ColumnIndex].Len());
        }
    }

    const int32 RowNumberWidth = FMath::Max(
        3,
        FString::FromInt(CoefficientRows.Num()).Len());

    FString Result;
    Result.Reserve(DisplayedRowCount * 128);
    Result += FString::Printf(
        TEXT("Model Gravitational Parameter [m^3/s^2]: %s\n"),
        *FormatDoubleForInspection(
            OutInspection.ModelGravitationalParameterM3PerS2));
    Result += FString::Printf(
        TEXT("Model Reference Radius [m]: %s\n\n"),
        *FormatDoubleForInspection(
            OutInspection.ModelReferenceRadiusMeters));

    Result += DisplayedRowCount == CoefficientRows.Num()
        ? FString::Printf(
            TEXT("Showing all %d coefficient rows.\n\n"),
            CoefficientRows.Num())
        : FString::Printf(
            TEXT(
                "Showing the first %d of %d coefficient rows.\n\n"),
            DisplayedRowCount,
            CoefficientRows.Num());

    Result += PadRight(TEXT("Row"), RowNumberWidth);
    for (int32 ColumnIndex = 0;
         ColumnIndex < ColumnHeadings.Num();
         ++ColumnIndex)
    {
        Result += TEXT(" | ");
        Result += PadRight(
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

    for (int32 RowIndex = 0;
         RowIndex < DisplayedRowCount;
         ++RowIndex)
    {
        Result += PadLeft(
            FString::FromInt(RowIndex + 1),
            RowNumberWidth);

        for (int32 ColumnIndex = 0;
             ColumnIndex < ColumnHeadings.Num();
             ++ColumnIndex)
        {
            Result += TEXT(" | ");
            Result += PadLeft(
                CoefficientRows[RowIndex][ColumnIndex],
                ColumnWidths[ColumnIndex]);
        }

        if (RowIndex + 1 < DisplayedRowCount)
        {
            Result += TEXT("\n");
        }
    }

    OutPreview = FText::FromString(MoveTemp(Result));
    return true;
}

FText UTGHarmonicCsvLibrary::
FormatHarmonicCsvInspectionForHud(
    const FTGHarmonicCsvInspection& Inspection)
{
    TArray<FString> OutputLines;

    if (!Inspection.NormalizedFilePath.IsEmpty())
    {
        OutputLines.Add(
            FString::Printf(
                TEXT("File: %s"),
                *FPaths::GetCleanFilename(
                    Inspection.NormalizedFilePath)));

        OutputLines.Add(
            FString::Printf(
                TEXT("Path: %s"),
                *Inspection.NormalizedFilePath));
    }

    OutputLines.Add(
        FString::Printf(
            TEXT("Status: %s"),
            Inspection.bValid
                ? TEXT("Valid")
                : TEXT("Invalid")));

    OutputLines.Add(TEXT(""));

    if (Inspection.bValid)
    {
        OutputLines.Add(
            FString::Printf(
                TEXT("GM: %s m^3/s^2"),
                *FormatDoubleForInspection(
                    Inspection
                        .ModelGravitationalParameterM3PerS2)));

        OutputLines.Add(
            FString::Printf(
                TEXT("Reference radius: %s m"),
                *FormatDoubleForInspection(
                    Inspection.ModelReferenceRadiusMeters)));

        OutputLines.Add(
            FString::Printf(
                TEXT("Maximum available degree n: %d"),
                Inspection.MaximumAvailableDegree));

        OutputLines.Add(
            FString::Printf(
                TEXT("Maximum available order m: %d"),
                Inspection.MaximumAvailableOrder));

        OutputLines.Add(
            FString::Printf(
                TEXT("Coefficient rows: %d"),
                Inspection.CoefficientCount));
    }
    else
    {
        OutputLines.Add(TEXT("Errors:"));

        for (const FString& ErrorMessage :
             Inspection.ErrorMessages)
        {
            OutputLines.Add(
                FString::Printf(
                    TEXT("- %s"),
                    *ErrorMessage));
        }
    }

    return FText::FromString(
        FString::Join(OutputLines, TEXT("\n")));
}

bool UTGHarmonicCsvLibrary::SupportsMaximumDegree(
    const FTGHarmonicCsvInspection& Inspection,
    const int32 RequestedMaximumDegree)
{
    if (RequestedMaximumDegree < 0)
    {
        return false;
    }

    if (RequestedMaximumDegree == 0)
    {
        return true;
    }

    return
        Inspection.bValid
        && RequestedMaximumDegree
            <= Inspection.MaximumAvailableDegree;
}

FText UTGHarmonicCsvLibrary::
FormatDegreeCompatibilityFailureForHud(
    const FTGHarmonicCsvInspection& Inspection,
    const int32 RequestedMaximumDegree)
{
    const FString Message = FString::Printf(
        TEXT(
            "CSV not accepted.\n\n"
            "Requested maximum degree: %d\n"
            "File maximum degree: %d\n\n"
            "The requested maximum degree must be less than or "
            "equal to the maximum degree available in the CSV."
        ),
        RequestedMaximumDegree,
        Inspection.MaximumAvailableDegree);

    return FText::FromString(Message);
}
