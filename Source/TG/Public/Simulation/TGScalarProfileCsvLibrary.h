// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGScalarProfileCsvLibrary.generated.h"

/*
 * Determines which quantity-specific validation rules are applied to a
 * generic two-column scalar-profile CSV.
 */
UENUM(BlueprintType)
enum class ETGScalarProfileQuantity : uint8
{
    Thrust
    UMETA(DisplayName = "Thrust [N]"),

    SpecificImpulse
    UMETA(DisplayName = "Specific Impulse [s]")
};

/*
 * Complete inspection result used by:
 *
 * - the reusable scalar-profile input widget;
 * - its curve graph;
 * - final scenario validation;
 * - the future Unreal-to-TGSim converter.
 */
USTRUCT(BlueprintType)
struct TG_API FTGScalarProfileCsvInspection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "CSV")
    bool bValid = false;

    UPROPERTY(BlueprintReadOnly, Category = "CSV")
    ETGScalarProfileQuantity Quantity =
        ETGScalarProfileQuantity::Thrust;

    UPROPERTY(BlueprintReadOnly, Category = "CSV")
    FString NormalizedFilePath;

    UPROPERTY(BlueprintReadOnly, Category = "CSV")
    int32 SampleCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "CSV")
    double FirstTimeSeconds = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "CSV")
    double LastTimeSeconds = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "CSV")
    double MinimumValue = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "CSV")
    double MaximumValue = 0.0;

    /*
     * These samples are suitable for feeding the future graph widget.
     */
    UPROPERTY(BlueprintReadOnly, Category = "CSV")
    TArray<FTGScalarCurveSample> Samples;

    UPROPERTY(BlueprintReadOnly, Category = "CSV")
    TArray<FString> ErrorMessages;
};

UCLASS()
class TG_API UTGScalarProfileCsvLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Simulation|Thrusters|Profiles",
        meta = (
            DisplayName =
                "Inspect Scalar Profile CSV"))
    static FTGScalarProfileCsvInspection
    InspectScalarProfileCsv(
        const FString& FilePath,
        ETGScalarProfileQuantity Quantity);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Simulation|Thrusters|Profiles",
        meta = (
            DisplayName =
                "Format Scalar Profile CSV Inspection For HUD"))
    static FText FormatScalarProfileCsvInspectionForHud(
        const FTGScalarProfileCsvInspection& Inspection);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Simulation|Thrusters|Profiles",
        meta = (
            DisplayName =
                "Validate Scalar Profile Constant"))
    static bool ValidateScalarProfileConstant(
        ETGScalarProfileQuantity Quantity,
        double Value,
        FText& OutErrorText);
};