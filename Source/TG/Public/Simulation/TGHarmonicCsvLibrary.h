// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TGHarmonicCsvLibrary.generated.h"

/**
 * Read-only result produced when inspecting one harmonic-gravity CSV file.
 *
 * This structure contains only file metadata and validation information.
 * It does not construct gravity physics objects.
 */
USTRUCT(BlueprintType)
struct TG_API FTGHarmonicCsvInspection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Harmonic CSV")
    bool bValid = false;

    UPROPERTY(BlueprintReadOnly, Category = "Harmonic CSV")
    FString NormalizedFilePath;

    UPROPERTY(BlueprintReadOnly, Category = "Harmonic CSV")
    double ModelGravitationalParameterM3PerS2 = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Harmonic CSV")
    double ModelReferenceRadiusMeters = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Harmonic CSV")
    int32 MaximumAvailableDegree = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Harmonic CSV")
    int32 MaximumAvailableOrder = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Harmonic CSV")
    int32 CoefficientCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Harmonic CSV")
    TArray<FString> ErrorMessages;
};

/**
 * Shared parser and validator for harmonic-gravity model CSV files.
 *
 * The library may later be reused by:
 * - the HUD;
 * - complete-scenario validation;
 * - the Unreal-to-TGSim converter.
 */
UCLASS()
class TG_API UTGHarmonicCsvLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Simulation|Gravity",
        meta = (DisplayName = "Inspect Harmonic Model CSV"))
    static bool InspectHarmonicModelCsv(
        const FString& FilePath,
        FTGHarmonicCsvInspection& Inspection);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Simulation|Gravity",
        meta = (
            DisplayName = "Build Harmonic Model CSV Preview",
            CPP_Default_MaximumCoefficientRows = "100"
        ))
    static bool BuildHarmonicModelCsvPreview(
        const FString& FilePath,
        int32 MaximumCoefficientRows,
        FTGHarmonicCsvInspection& OutInspection,
        FText& OutPreview,
        FText& OutError);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Simulation|Gravity",
        meta = (DisplayName = "Format Harmonic CSV Inspection For HUD"))
    static FText FormatHarmonicCsvInspectionForHud(
        const FTGHarmonicCsvInspection& Inspection);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Simulation|Gravity",
        meta = (DisplayName = "Harmonic CSV Supports Maximum Degree"))
    static bool SupportsMaximumDegree(
        const FTGHarmonicCsvInspection& Inspection,
        int32 RequestedMaximumDegree);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Simulation|Gravity",
        meta = (
            DisplayName =
                "Format Harmonic Degree Compatibility Failure For HUD"
        ))
    static FText FormatDegreeCompatibilityFailureForHud(
        const FTGHarmonicCsvInspection& Inspection,
        int32 RequestedMaximumDegree);
};
