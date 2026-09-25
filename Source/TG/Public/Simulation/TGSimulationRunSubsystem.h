// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "TGSimulationRunSubsystem.generated.h"

struct FTGSimulationFinalState;

UENUM(BlueprintType)
enum class ETGSimulationRunState : uint8
{
    Idle,
    PreparingScenario,
    LaunchingRunner,
    Running,
    Cancelling,
    Finalizing,
    Succeeded,
    Failed,
    Cancelled,
    TimedOut
};

USTRUCT(BlueprintType)
struct TG_API FTGCompletedSimulationRun
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    FGuid RunId;

    /** Valid only when the run used the exact, unmodified saved scenario. */
    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    FGuid SourceScenarioId;

    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    FString ScenarioName;

    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    FString RunDirectory;

    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    FString ScenarioFilePath;

    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    FString ResultCsvFilePath;

    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    FString SummaryFilePath;

    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    FString LogFilePath;

    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    FDateTime StartedUtc;

    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    FDateTime CompletedUtc;

    /** Immutable configuration used by the isolated backend process. */
    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    FTGSimulationScenario InputSnapshot;

    bool IsUsable() const
    {
        return RunId.IsValid() &&
            !ScenarioFilePath.IsEmpty() &&
            !ResultCsvFilePath.IsEmpty();
    }
};

USTRUCT(BlueprintType)
struct TG_API FTGSimulationRunError
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    ETGSimulationRunState State = ETGSimulationRunState::Failed;

    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    FName Stage;

    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    FText Message;

    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    FString TechnicalDetails;

    UPROPERTY(BlueprintReadOnly, Category = "PHAROS|Simulation Run")
    int32 ProcessExitCode = INDEX_NONE;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FTGSimulationRunStateChanged,
    ETGSimulationRunState,
    NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FTGSimulationRunProgress,
    float,
    Percent,
    FText,
    Status);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FTGSimulationRunCompleted,
    FTGCompletedSimulationRun,
    CompletedRun);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FTGSimulationRunFailed,
    FTGSimulationRunError,
    Error);

/**
 * Persistent Unreal-side coordinator for one isolated PHAROS Scenario Runner process.
 *
 * The subsystem survives map travel, while the backend process owns all
 * propagation and user-controller execution. UMG observes only the reflected
 * state/delegates below; no widget or UObject crosses the process boundary.
 */
UCLASS(BlueprintType)
class TG_API UTGSimulationRunSubsystem final
    : public UGameInstanceSubsystem
    , public FTickableGameObject
{
    GENERATED_BODY()

public:
    UTGSimulationRunSubsystem();

    virtual void Initialize(
        FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override;
    virtual bool IsTickableWhenPaused() const override;
    virtual TStatId GetStatId() const override;
    virtual UWorld* GetTickableGameObjectWorld() const override;

    /** Runs the exact currently reviewed scenario through PHAROS Scenario Runner. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Run")
    bool StartCurrentSimulation(FText& OutError);

    /** Requests cooperative cancellation, then force-stops after a short grace period. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Run")
    void CancelSimulation();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Run")
    void ResetRunState();

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Run")
    bool IsSimulationRunning() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Run")
    ETGSimulationRunState GetRunState() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Run")
    float GetProgressPercent() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Run")
    FText GetProgressStatus() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Run")
    FTGSimulationRunError GetLastRunError() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Run")
    bool HasCompletedRun() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Run")
    FTGCompletedSimulationRun GetLatestCompletedRun() const;

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Run|Travel")
    bool OpenLatestRunVisualization(FText& OutError);

    /** Opens the newest persisted result belonging to one saved scenario. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Run|Travel")
    bool OpenLatestRunVisualizationForScenario(
        const FGuid& ScenarioId,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Run|Travel")
    bool ReturnToConfigurationLevel(FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Run|Travel")
    bool ReturnToScenarioLibraryLevel(FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Run|Travel")
    bool ReturnToMainMenuLevel(FText& OutError);

    /** MainMenu consumes this once to restore the configuration HUD after travel. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Run|Travel")
    bool ConsumePendingConfigurationRestore();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Run|Development")
    void SetRunnerExecutableOverride(const FString& ExecutablePath);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Run|Travel")
    void SetAutoOpenVisualizationOnSuccess(bool bEnabled);

    UPROPERTY(BlueprintAssignable, Category = "PHAROS|Simulation Run")
    FTGSimulationRunStateChanged OnRunStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "PHAROS|Simulation Run")
    FTGSimulationRunProgress OnRunProgress;

    UPROPERTY(BlueprintAssignable, Category = "PHAROS|Simulation Run")
    FTGSimulationRunCompleted OnRunCompleted;

    UPROPERTY(BlueprintAssignable, Category = "PHAROS|Simulation Run")
    FTGSimulationRunFailed OnRunFailed;

private:
    UPROPERTY(Transient)
    ETGSimulationRunState RunState = ETGSimulationRunState::Idle;

    UPROPERTY(Transient)
    float ProgressPercent = 0.0f;

    UPROPERTY(Transient)
    FText ProgressStatus;

    UPROPERTY(Transient)
    FTGSimulationRunError LastRunError;

    UPROPERTY(Transient)
    FTGCompletedSimulationRun CurrentRun;

    UPROPERTY(Transient)
    FTGCompletedSimulationRun LatestCompletedRun;

    UPROPERTY(Transient)
    bool bHasLatestCompletedRun = false;

    UPROPERTY(Transient)
    bool bPendingConfigurationRestore = false;

    enum class EPendingMenuDestination : uint8
    {
        None,
        MainMenu,
        Configuration,
        ScenarioLibrary
    };

    EPendingMenuDestination PendingMenuDestination =
        EPendingMenuDestination::None;

    UPROPERTY(Transient)
    bool bAutoOpenVisualizationOnSuccess = true;

    UPROPERTY(Transient)
    FString RunnerExecutableOverride;

    UPROPERTY(Transient)
    TSoftObjectPtr<UWorld> ConfigurationLevel;

    UPROPERTY(Transient)
    TSoftObjectPtr<UWorld> VisualizationLevel;

    FProcHandle RunnerProcess;
    uint32 RunnerProcessId = 0;
    void* RunnerOutputReadPipe = nullptr;
    void* RunnerOutputWritePipe = nullptr;
    void* RunnerInputReadPipe = nullptr;
    void* RunnerInputWritePipe = nullptr;

    FString PendingRunnerOutput;
    FString RunnerLog;
    FString LastRunnerResultPath;
    FString LastRunnerErrorMessage;

    double MaximumWallClockRuntimeSeconds = 0.0;
    double RunnerStartedMonotonicSeconds = 0.0;
    double ForceStopMonotonicSeconds = 0.0;
    bool bStopRequested = false;
    bool bStopWasTimeout = false;
    bool bStopWasUserCancellation = false;
    bool bScenarioPreparationInFlight = false;
    uint64 ScenarioPreparationGeneration = 0;
    bool bResultFinalizationInFlight = false;
    uint64 ResultFinalizationGeneration = 0;

    void SetRunState(ETGSimulationRunState NewState);
    void PublishProgress(float Percent, const FText& Status);
    void FailBeforeLaunch(
        FName Stage,
        const FText& Message,
        const FString& TechnicalDetails = FString{});
    bool CreateRunFiles(FText& OutError);
    bool LaunchRunner(FText& OutError);
    void HandleScenarioPreparationCompleted(
        uint64 PreparationGeneration,
        bool bSucceeded,
        FTGSimulationScenario&& PreparedScenario,
        const FText& PreparationWarning,
        const FText& ExportMessage,
        const FText& Error);
    FString ResolveRunnerExecutable(FString& OutSearchedPaths) const;
    void PollRunnerOutput();
    void ProcessRunnerOutputLine(const FString& Line);
    void RequestRunnerStop(bool bTimedOut);
    void FinalizeExitedRunner();
    void HandleSuccessfulRunnerFinalization(
        uint64 FinalizationGeneration,
        int32 ExitCode,
        bool bCsvValid,
        const FString& CsvError,
        FTGSimulationScenario&& InputSnapshot,
        const FTGSimulationFinalState& FinalState,
        int64 StoredSampleCount,
        double StartEphemerisTime,
        double FinalEphemerisTime,
        const FString& SnapshotError);
    void FinishUnsuccessfulRun(
        ETGSimulationRunState TerminalState,
        FName Stage,
        const FText& Message,
        int32 ExitCode);
    void CloseRunnerResources();
    void RemoveIncompleteResultFiles() const;
    void SaveRunnerLog() const;
    static bool ValidateCompletedCsvFile(
        const FString& FilePath,
        FString& OutError);
    void PersistCompletedRunMetadata(
        const FTGSimulationFinalState& FinalState,
        int64 StoredSampleCount,
        double StartEphemerisTime,
        double FinalEphemerisTime,
        const FString& SnapshotError);
    bool BeginReturnToConfigurationMap(
        EPendingMenuDestination Destination,
        FText& OutError);
    bool ApplyPendingMenuDestination();
    static FText DescribeRunnerExitCode(int32 ExitCode);
};
