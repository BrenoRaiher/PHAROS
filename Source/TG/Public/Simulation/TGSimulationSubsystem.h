// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGSimulationSaveGame.h"
#include "TGSimulationSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FTGCurrentScenarioSaveCompleted,
    bool,
    bSucceeded,
    FGuid,
    ScenarioId);

UCLASS(BlueprintType)
class TG_API UTGSimulationSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(
        FSubsystemCollectionBase& Collection) override;

    virtual void Deinitialize() override;

    // ------------------------------------------------------------------------
    // Current scenario draft
    // ------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "TGSim|Scenario")
    void CreateNewScenarioDraft();

    /*
     * Updates the current draft while preserving its saved-scenario identity.
     * This is the normal function for configuration widgets to use. It stores
     * current-schema authored data exactly; it does not silently normalize the
     * gravity catalog before Review can validate it.
     */
    UFUNCTION(BlueprintCallable, Category = "TGSim|Scenario")
    void SetCurrentScenarioDraft(
        const FTGSimulationScenario& Scenario);

    /**
     * Replaces the current draft with an externally imported scenario.
     * The imported draft deliberately has no saved-library identity, so a
     * later Save creates a new record instead of overwriting the prior one.
     */
    UFUNCTION(BlueprintCallable, Category = "TGSim|Scenario")
    void SetImportedScenarioDraft(
        const FTGSimulationScenario& Scenario);

    UFUNCTION(BlueprintPure, Category = "TGSim|Scenario")
    FTGSimulationScenario GetCurrentScenarioDraft() const;

    UFUNCTION(BlueprintPure, Category = "TGSim|Scenario")
    bool HasCurrentScenarioDraft() const;

    UFUNCTION(BlueprintCallable, Category = "TGSim|Scenario")
    void ClearCurrentScenarioDraft();

    UFUNCTION(BlueprintCallable, Category = "TGSim|Scenario")
    void MarkCurrentScenarioDirty();

    UFUNCTION(BlueprintPure, Category = "TGSim|Scenario")
    bool IsCurrentScenarioDirty() const;

    UFUNCTION(BlueprintPure, Category = "TGSim|Scenario")
    FGuid GetCurrentScenarioId() const;

    UFUNCTION(BlueprintPure, Category = "TGSim|Scenario")
    bool IsCurrentScenarioSaved() const;

    /**
     * Monotonic in-process revision used by native panels to detect draft
     * replacement while they are inactive in a WidgetSwitcher.
     */
    uint64 GetCurrentScenarioDraftRevision() const;

    /**
     * Records authoring-review acceptance only for the exact current draft
     * revision. Any later draft mutation invalidates the authorization.
     */
    bool AcceptCurrentScenarioReview(
        uint64 ValidatedRevision,
        FText& OutError);

    void InvalidateCurrentScenarioReview();

    bool IsCurrentScenarioReviewAccepted() const;

    UFUNCTION(BlueprintPure, Category = "TGSim|Scenario|Review")
    bool CanSimulateCurrentScenario(FText& OutReason) const;

    /**
     * Updates frontend run policy without invalidating an accepted physics
     * Review. The value is still persisted with the current scenario draft.
     */
    UFUNCTION(BlueprintCallable, Category = "TGSim|Scenario|Execution")
    bool SetCurrentScenarioMaximumWallClockRuntimeSeconds(
        double MaximumSeconds,
        FText& OutError);

    UFUNCTION(BlueprintPure, Category = "TGSim|Scenario|Execution")
    double GetCurrentScenarioMaximumWallClockRuntimeSeconds() const;

    // ------------------------------------------------------------------------
    // Saved scenarios
    // ------------------------------------------------------------------------

    /*
     * Creates a new saved record or updates the record from which the current
     * draft was loaded. OutScenarioId receives the persistent scenario ID.
     */
    UFUNCTION(BlueprintCallable, Category = "TGSim|Persistence")
    bool SaveCurrentScenarioDraft(FGuid& OutScenarioId);

    /** Saves the current draft without blocking on backup or disk I/O. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Persistence")
    bool SaveCurrentScenarioDraftAsync();

    UFUNCTION(BlueprintPure, Category = "PHAROS|Persistence")
    bool IsScenarioSaveInProgress() const;

    UPROPERTY(BlueprintAssignable, Category = "PHAROS|Persistence")
    FTGCurrentScenarioSaveCompleted OnCurrentScenarioSaveCompleted;

    bool BeginSaveCurrentScenarioDraft(
        TFunction<void(bool, const FGuid&)> Completion);

    /** Loads current-schema authored data exactly; migration is a separate operation. */
    UFUNCTION(BlueprintCallable, Category = "TGSim|Persistence")
    bool LoadScenarioDraft(const FGuid& ScenarioId);

    /*
     * Deleting the record does not discard the currently edited draft.
     * If the deleted scenario is current, the draft becomes unsaved.
     */
    UFUNCTION(BlueprintCallable, Category = "TGSim|Persistence")
    bool DeleteSavedScenario(const FGuid& ScenarioId);

    /** Renames one saved scenario without loading it as the current draft. */
    UFUNCTION(BlueprintCallable, Category = "TGSim|Persistence")
    bool RenameSavedScenario(
        const FGuid& ScenarioId,
        const FString& NewName,
        FText& OutError);

    UFUNCTION(BlueprintPure, Category = "TGSim|Persistence")
    TArray<FTGSavedScenarioRecord> GetSavedScenarios() const;

    /** Records use of a saved scenario for the Scenario Library recent list. */
    bool MarkScenarioRecentlyUsed(const FGuid& ScenarioId);

    /** True when the scenario has a completed run whose result files still exist. */
    UFUNCTION(BlueprintPure, Category = "TGSim|Persistence|Runs")
    bool HasVisualizableRunForScenario(const FGuid& ScenarioId) const;

    /** True when the newest visualizable run also contains a valid final state. */
    UFUNCTION(BlueprintPure, Category = "TGSim|Persistence|Runs")
    bool CanContinueScenarioFromLatestRun(const FGuid& ScenarioId) const;

    UFUNCTION(BlueprintPure, Category = "TGSim|Persistence|Runs")
    bool GetLatestCompletedRunForScenario(
        const FGuid& ScenarioId,
        FTGSavedSimulationRun& OutRun) const;

    /**
     * Creates a new unsaved draft from the selected run's final physical state.
     * The original saved scenario and its result remain unchanged.
     */
    UFUNCTION(BlueprintCallable, Category = "TGSim|Persistence|Runs")
    bool CreateContinuationDraftFromLatestRun(
        const FGuid& ScenarioId,
        FText& OutError);

    /** C++ run-coordinator hook; completed runs are kept outside the large CSV. */
    bool RecordCompletedSimulationRun(const FTGSavedSimulationRun& Run);

    static FString ResolveSavedRunResultPath(
        const FString& ResultRelativePath);

    // ------------------------------------------------------------------------
    // Persistent library
    // ------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "TGSim|Persistence")
    bool LoadLibrary();

    UFUNCTION(BlueprintCallable, Category = "TGSim|Persistence")
    bool SaveLibrary();

    UFUNCTION(BlueprintPure, Category = "TGSim|Persistence")
    UTGSimulationSaveGame* GetLibrary() const;

private:
    static const FString SaveSlotName;
    static constexpr int32 SaveUserIndex = 0;

    UPROPERTY(Transient)
    TObjectPtr<UTGSimulationSaveGame> SimulationLibrary;

    UPROPERTY(Transient)
    FTGSimulationScenario CurrentScenarioDraft;

    UPROPERTY(Transient)
    FGuid CurrentScenarioId;

    UPROPERTY(Transient)
    bool bHasCurrentScenarioDraft = false;

    UPROPERTY(Transient)
    bool bCurrentScenarioDirty = false;

    uint64 CurrentScenarioDraftRevision = 0;

    uint64 AcceptedScenarioReviewRevision = 0;

    UPROPERTY(Transient)
    bool bHasAcceptedScenarioReview = false;

    bool PrepareCurrentScenarioRecordForSave(
        FGuid& OutScenarioId,
        uint64& OutDraftRevision);
    void BeginAsyncLibraryWrite(
        const FGuid& ScenarioId,
        uint64 DraftRevision,
        const FString& BackupPath);
    void FinishAsyncScenarioSave(
        bool bSucceeded,
        const FGuid& ScenarioId,
        uint64 DraftRevision);

    bool bScenarioSaveInProgress = false;
    uint64 AsyncSaveGeneration = 0;
    TArray<TFunction<void(bool, const FGuid&)>> PendingSaveCompletions;
};
