// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "TGScenarioReviewTypes.generated.h"

UENUM(BlueprintType)
enum class ETGScenarioReviewSeverity : uint8
{
    Error UMETA(DisplayName = "Error"),
    Warning UMETA(DisplayName = "Warning")
};

UENUM(BlueprintType)
enum class ETGScenarioReviewSection : uint8
{
    ScenarioAndSolver UMETA(DisplayName = "Scenario and Solver"),
    InitialState UMETA(DisplayName = "Initial State"),
    ComponentsAndJoints UMETA(DisplayName = "Components and Joints"),
    Actuators UMETA(DisplayName = "Actuators"),
    Controller UMETA(DisplayName = "Controller"),
    Gravity UMETA(DisplayName = "Gravity and Bodies"),
    SolarRadiationPressure UMETA(DisplayName = "Solar Radiation Pressure"),
    Atmosphere UMETA(DisplayName = "Atmosphere"),
    Aerodynamics UMETA(DisplayName = "Aerodynamics"),
    CrossSystemAndSpice UMETA(DisplayName = "Cross-System and SPICE")
};

/** One stable, user-facing authoring-review diagnostic. */
USTRUCT(BlueprintType)
struct TG_API FTGScenarioReviewIssue
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Review")
    ETGScenarioReviewSeverity Severity = ETGScenarioReviewSeverity::Error;

    UPROPERTY(BlueprintReadOnly, Category = "Review")
    FName Code = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Review")
    FString Path;

    UPROPERTY(BlueprintReadOnly, Category = "Review")
    FText Message;

    UPROPERTY(BlueprintReadOnly, Category = "Review")
    ETGScenarioReviewSection Section =
        ETGScenarioReviewSection::ScenarioAndSolver;

    UPROPERTY(BlueprintReadOnly, Category = "Review")
    FGuid ComponentId;

    UPROPERTY(BlueprintReadOnly, Category = "Review")
    FGuid DofId;

    UPROPERTY(BlueprintReadOnly, Category = "Review")
    int32 ArrayIndex = INDEX_NONE;

    /** Optional stable identity used by controller-panel navigation. */
    UPROPERTY(BlueprintReadOnly, Category = "Review")
    FName ObjectId = NAME_None;

    /** Optional primitive logical-region identity used by SRP navigation. */
    UPROPERTY(BlueprintReadOnly, Category = "Review")
    uint8 LogicalRegion = 0;

    /** Optional generated-facet identity used by SRP navigation. */
    UPROPERTY(BlueprintReadOnly, Category = "Review")
    int32 StableTriangleIndex = INDEX_NONE;

    FText ToDisplayText() const;
};

/** Complete result for one immutable draft snapshot. */
USTRUCT(BlueprintType)
struct TG_API FTGScenarioReviewReport
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Review")
    bool bValidationCompleted = false;

    UPROPERTY(BlueprintReadOnly, Category = "Review")
    int32 ErrorCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Review")
    int32 WarningCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Review")
    TArray<FTGScenarioReviewIssue> Issues;

    bool IsAcceptable() const
    {
        return bValidationCompleted && ErrorCount == 0;
    }
};
