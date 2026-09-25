// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGGravityCoverageLibrary.generated.h"

/**
 * Result category for the compact principal-moon SPICE catalog interval.
 */
UENUM(BlueprintType)
enum class ETGCompactMoonCatalogCoverageStatus : uint8
{
    NotRequired
    UMETA(DisplayName = "Not Required"),

    RequiredAndSatisfied
    UMETA(DisplayName = "Required and Satisfied"),

    RequiredButOutsideCatalog
    UMETA(DisplayName = "Required but Outside Catalog"),

    CannotEvaluateScenarioInterval
    UMETA(DisplayName = "Cannot Evaluate Scenario Interval")
};

/**
 * Reusable result for HUD presentation and final scenario validation.
 */
USTRUCT(BlueprintType)
struct TG_API FTGCompactMoonCatalogCoverageEvaluation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Coverage")
    ETGCompactMoonCatalogCoverageStatus Status =
        ETGCompactMoonCatalogCoverageStatus::NotRequired;

    UPROPERTY(BlueprintReadOnly, Category = "Coverage")
    bool bCompactCatalogRequired = false;

    UPROPERTY(BlueprintReadOnly, Category = "Coverage")
    bool bScenarioIntervalResolved = false;

    UPROPERTY(BlueprintReadOnly, Category = "Coverage")
    bool bCoverageSatisfied = true;

    UPROPERTY(BlueprintReadOnly, Category = "Coverage")
    FDateTime EffectiveStartUtc;

    UPROPERTY(BlueprintReadOnly, Category = "Coverage")
    FDateTime EffectiveFinalUtc;
};

/**
 * Shared gravity-coverage validation.
 *
 * This class contains no HUD state and modifies no scenario values.
 */
UCLASS()
class TG_API UTGGravityCoverageLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Simulation|Gravity",
        meta = (
            DisplayName =
                "Evaluate Compact Moon Catalog Coverage"
        ))
    static FTGCompactMoonCatalogCoverageEvaluation
    EvaluateCompactMoonCatalogCoverage(
        const FTGSimulationScenario& Scenario);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Simulation|Gravity",
        meta = (
            DisplayName =
                "Format Compact Moon Catalog Coverage For HUD"
        ))
    static FText FormatCompactMoonCatalogCoverageForHud(
        const FTGCompactMoonCatalogCoverageEvaluation& Evaluation);
};