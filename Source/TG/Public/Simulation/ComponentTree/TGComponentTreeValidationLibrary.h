// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGComponentTreeValidationLibrary.generated.h"

/**
 * Severity of one component-tree validation issue.
 */
UENUM(BlueprintType)
enum class ETGComponentTreeIssueSeverity : uint8
{
    Error UMETA(DisplayName = "Error"),
    Warning UMETA(DisplayName = "Warning")
};

/**
 * One validation message associated with the tree, a component, or a DOF.
 */
USTRUCT(BlueprintType)
struct TG_API FTGComponentTreeValidationIssue
{
    GENERATED_BODY()

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Validation")
    ETGComponentTreeIssueSeverity Severity =
        ETGComponentTreeIssueSeverity::Error;

    /**
     * Human-readable location such as:
     *
     * Components[1].Mass
     * Components[2].DegreesOfFreedom[0].Axis
     */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Validation")
    FString Path;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Validation")
    FGuid ComponentId;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Validation")
    FGuid DofId;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Validation")
    FText Message;
};

/**
 * Complete result of validating a component tree.
 */
USTRUCT(BlueprintType)
struct TG_API FTGComponentTreeValidationReport
{
    GENERATED_BODY()

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Validation")
    bool bIsValid = false;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Validation")
    int32 ErrorCount = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Validation")
    int32 WarningCount = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Validation")
    FText Summary;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Validation")
    TArray<FTGComponentTreeValidationIssue> Issues;
};

/**
 * Validation operations for the physical and visual spacecraft hierarchy.
 *
 * This library reports errors and warnings but does not silently modify the
 * scenario. Editing and normalization remain separate operations.
 */
UCLASS()
class TG_API UTGComponentTreeValidationLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Validates the complete physical hierarchy and persistent visual data.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Validation",
        meta = (DisplayName = "Validate Scenario Component Tree"))
    static FTGComponentTreeValidationReport
        ValidateScenarioComponentTree(
            const FTGSimulationScenario& Scenario);

    /**
     * Validates a positive-semidefinite symmetric centroidal inertia tensor
     * and calculates its sorted principal moments. Zero principal moments are
     * permitted; complete-spacecraft dynamics still require a nonsingular
     * assembled inertia tensor.
     *
     * Output order:
     * X = smallest principal moment
     * Y = intermediate principal moment
     * Z = largest principal moment
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Validation",
        meta = (DisplayName = "Validate Symmetric Inertia"))
    static bool ValidateSymmetricInertia(
        const FTGSymmetricInertia& Inertia,
        FVector& OutPrincipalMomentsKilogramMetersSquared,
        FText& OutErrorText);

    /**
     * Creates a readable multi-line representation for the HUD.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Validation",
        meta = (DisplayName =
            "Format Component Tree Validation Report For HUD"))
    static FText FormatComponentTreeValidationReportForHud(
        const FTGComponentTreeValidationReport& Report,
        int32 MaximumIssues = 20);
};
