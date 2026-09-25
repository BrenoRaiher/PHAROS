// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGSimulationSaveGame.generated.h"

// ============================================================================
// Final-state snapshot
// ============================================================================

USTRUCT(BlueprintType)
struct TG_API FTGFinalComponentMass
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Final State")
    FString ComponentName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Final State")
    double MassKilograms = 0.0;
};

USTRUCT(BlueprintType)
struct TG_API FTGFinalJointState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Final State")
    FString ComponentName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Final State")
    FString DegreeOfFreedomName;

    // Radians for rotational DOFs or meters for translational DOFs.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Final State")
    double Coordinate = 0.0;

    // Radians/second for rotational DOFs or meters/second for translational DOFs.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Final State")
    double Rate = 0.0;
};

USTRUCT(BlueprintType)
struct TG_API FTGFinalReactionWheelState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Final State")
    FString ReactionWheelName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Final State")
    double MomentumNewtonMeterSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct TG_API FTGSimulationFinalState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Final State")
    bool bIsValid = false;

    // Exact backend epoch: TDB seconds past J2000.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Final State")
    double EphemerisTimeTdbSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Final State")
    FTGInitialSpacecraftState SpacecraftState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Final State")
    TArray<FTGFinalComponentMass> ComponentMasses;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Final State")
    TArray<FTGFinalJointState> JointStates;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Final State")
    TArray<FTGFinalReactionWheelState> ReactionWheelStates;
};

// ============================================================================
// Saved scenario record
// ============================================================================

USTRUCT(BlueprintType)
struct TG_API FTGSavedScenarioRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Scenario")
    FGuid ScenarioId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Scenario")
    FDateTime CreatedUtc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Scenario")
    FDateTime LastModifiedUtc;

    /*
     * Last time this scenario was opened, visualized, continued, or used for
     * a simulation. Zero for records created before recents were introduced.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Scenario")
    FDateTime LastAccessedUtc;

    /*
     * Version of the individual scenario representation.
     * This is independent of the whole-library format version.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Scenario")
    int32 ScenarioFormatVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Scenario")
    FTGSimulationScenario Scenario;
};

// ============================================================================
// Saved simulation-run record
// ============================================================================

UENUM(BlueprintType)
enum class ETGSimulationRunStatus : uint8
{
    Completed UMETA(DisplayName = "Completed"),
    Failed UMETA(DisplayName = "Failed"),
    Cancelled UMETA(DisplayName = "Cancelled")
};

USTRUCT(BlueprintType)
struct TG_API FTGSavedSimulationRun
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Run")
    FGuid RunId;

    /*
     * May be invalid when the run originated from an unsaved scenario draft.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Run")
    FGuid SourceScenarioId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Run")
    FString RunName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Run")
    FDateTime CreatedUtc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Run")
    ETGSimulationRunStatus Status =
        ETGSimulationRunStatus::Completed;

    /*
     * Immutable copy of the precise UI-facing configuration used for this run.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Run")
    FTGSimulationScenario InputSnapshot;

    /*
     * Path relative to the application's persistent result-data directory.
     * Empty when no result table was produced.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Run")
    FString ResultRelativePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Run")
    int32 ResultFormatVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Run")
    int64 StoredSampleCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Run")
    double StartEphemerisTimeTdbSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Run")
    double FinalEphemerisTimeTdbSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Run")
    FTGSimulationFinalState FinalState;

    /*
     * Populated for failed or cancelled runs when useful information exists.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Run")
    FString StatusMessage;
};

// ============================================================================
// Complete persistent library
// ============================================================================

UCLASS()
class TG_API UTGSimulationSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    static constexpr int32 OldestSupportedLibraryFormatVersion = 1;
    static constexpr int32 CurrentLibraryFormatVersion = 1;

    /*
     * Version of the overall catalog structure.
     * Future loaders can use this for data migration.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Persistence")
    int32 LibraryFormatVersion = CurrentLibraryFormatVersion;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Persistence")
    TArray<FTGSavedScenarioRecord> SavedScenarios;

    /*
     * Contains run metadata, input snapshots and final-state snapshots.
     * Large sample tables remain in separate result files.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Persistence")
    TArray<FTGSavedSimulationRun> SavedRuns;
};
