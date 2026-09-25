// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Simulation/TGSimulationScenarioTypes.h"

#include "TGScenarioEditingLibrary.generated.h"

/**
 * Double-valued fields belonging to FTGScenarioSolverConfig.
 */
UENUM(BlueprintType)
enum class ETGScenarioSolverDoubleField : uint8
{
    DurationSeconds
        UMETA(DisplayName = "Duration Seconds"),

    MaximumIntegratorStepSeconds
        UMETA(DisplayName = "Maximum Integrator Step Seconds"),

    InitialIntegratorStepSeconds
        UMETA(DisplayName = "Initial Integrator Step Seconds"),

    AbsoluteTolerance
        UMETA(DisplayName = "Absolute Tolerance"),

    RelativeTolerance
        UMETA(DisplayName = "Relative Tolerance"),

    OutputStepSeconds
        UMETA(DisplayName = "Output Step Seconds")
};

/**
 * Integer-valued fields belonging to FTGScenarioSolverConfig.
 */
UENUM(BlueprintType)
enum class ETGScenarioSolverIntegerField : uint8
{
    MaximumIntegrationSteps
        UMETA(DisplayName = "Maximum Integration Steps"),

    MaximumOutputSamples
        UMETA(DisplayName = "Maximum Output Samples")
};

/**
 * Reusable functions for editing Blueprint-facing TG scenario structures.
 */
UCLASS()
class TG_API UTGScenarioEditingLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Returns a copy of Config with only the selected double field changed.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|HUD|Scenario Solver",
        meta = (DisplayName = "Set Scenario Solver Double Field"))
    static FTGScenarioSolverConfig SetScenarioSolverDoubleField(
        const FTGScenarioSolverConfig& Config,
        ETGScenarioSolverDoubleField Field,
        double Value);

    /**
     * Returns a copy of Config with only the selected integer field changed.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|HUD|Scenario Solver",
        meta = (DisplayName = "Set Scenario Solver Integer Field"))
    static FTGScenarioSolverConfig SetScenarioSolverIntegerField(
        const FTGScenarioSolverConfig& Config,
        ETGScenarioSolverIntegerField Field,
        int32 Value);
};