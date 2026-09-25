// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/TGSimulationRunSubsystem.h"

#include "Async/Async.h"
#include "Containers/StringConv.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MoviePlayer.h"
#include "Components/WidgetSwitcher.h"
#include "Simulation/Control/TGControllerLibrarySubsystem.h"
#include "Simulation/TGScenarioDocumentAdapter.h"
#include "Simulation/TGScenarioFileLibrary.h"
#include "Simulation/TGSimulationSubsystem.h"
#include "Styling/CoreStyle.h"
#include "UI/Configuration/Environment/TGSolarRadiationPressureEditingLibrary.h"
#include "UI/Theme/TGUiTheme.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"

#include "TGSim/Scenario/ScenarioDiagnostics.h"
#include "TGSim/Scenario/ScenarioFile.h"

#include <filesystem>

namespace
{
    constexpr double kCancellationGraceSeconds = 1.5;

    std::filesystem::path ToScenarioPath(const FString& Value)
    {
#if PLATFORM_WINDOWS
        return std::filesystem::path(std::wstring(*Value));
#else
        return std::filesystem::u8path(
            std::string(TCHAR_TO_UTF8(*Value)));
#endif
    }

    FString FromScenarioUtf8(const std::string& Value)
    {
        return FString(UTF8_TO_TCHAR(Value.c_str()));
    }

    bool ResolveControllerDllPath(
        UGameInstance* GameInstance,
        const FTGSimulationScenario& Scenario,
        FString& OutControllerDllPath,
        FText& OutError)
    {
        OutControllerDllPath.Reset();
        OutError = FText::GetEmpty();
        if (Scenario.Control.Mode !=
            ETGControlMode::CompiledUserController)
        {
            return true;
        }

        if (Scenario.Control.ControllerId.IsNone())
        {
            OutControllerDllPath =
                Scenario.Control.StandaloneControllerDllFilePath;
            if (!OutControllerDllPath.IsEmpty())
            {
                return true;
            }

            OutError = FText::FromString(TEXT(
                "The compiled-controller scenario has no controller DLL to export."));
            return false;
        }

        UTGControllerLibrarySubsystem* ControllerLibrary =
            GameInstance != nullptr
                ? GameInstance->GetSubsystem<
                    UTGControllerLibrarySubsystem>()
                : nullptr;
        FString Error;
        if (ControllerLibrary == nullptr ||
            !ControllerLibrary->ResolveReadyControllerDllPath(
                Scenario.Control.ControllerId,
                OutControllerDllPath,
                Error))
        {
            OutError = FText::FromString(
                Error.IsEmpty()
                    ? TEXT("The selected controller is not ready for simulation.")
                    : Error);
            return false;
        }
        return true;
    }

    bool SavePreparedScenario(
        const FTGSimulationScenario& Scenario,
        const FString& ControllerDllPath,
        const FString& FilePath,
        FText& OutMessage)
    {
        tgsim::scenario::ScenarioDocument Document;
        FTGScenarioDocumentAdapter::ToPortableDocument(
            Scenario,
            ControllerDllPath,
            Document);

        tgsim::scenario::Diagnostics Diagnostics;
        if (!tgsim::scenario::SaveScenarioFile(
                ToScenarioPath(FilePath),
                Document,
                Diagnostics))
        {
            OutMessage = FText::FromString(FromScenarioUtf8(
                tgsim::scenario::FormatDiagnostics(Diagnostics)));
            return false;
        }

        OutMessage = FText::FromString(FString::Printf(
            TEXT("Scenario exported to %s"),
            *FilePath));
        return true;
    }

    struct FScenarioPreparationResult
    {
        bool bSucceeded = false;
        FTGSimulationScenario Scenario;
        FText PreparationWarning;
        FText ExportMessage;
        FText Error;
    };

    struct FRunFinalizationResult
    {
        bool bCsvValid = false;
        FString CsvError;
        FTGSimulationScenario InputSnapshot;
        FTGSimulationFinalState FinalState;
        int64 StoredSampleCount = 0;
        double StartEphemerisTime = 0.0;
        double FinalEphemerisTime = 0.0;
        FString SnapshotError;
    };

    FString RunnerExecutableName()
    {
#if PLATFORM_WINDOWS
        return TEXT("PHAROSScenarioRunner.exe");
#else
        return TEXT("PHAROSScenarioRunner");
#endif
    }

    FString QuoteArgument(const FString& Value)
    {
        FString Escaped = Value;
        Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
        return FString::Printf(TEXT("\"%s\""), *Escaped);
    }

    bool IsRunnerProtocolLine(const FString& Line)
    {
        return Line.StartsWith(TEXT("PHAROS_EVENT\t")) ||
            Line.StartsWith(TEXT("TGSIM_EVENT\t"));
    }

    FString LastNonEmptyLine(const FString& Text)
    {
        TArray<FString> Lines;
        Text.ParseIntoArrayLines(Lines, true);
        for (int32 Index = Lines.Num() - 1; Index >= 0; --Index)
        {
            const FString Candidate = Lines[Index].TrimStartAndEnd();
            if (!Candidate.IsEmpty() &&
                !IsRunnerProtocolLine(Candidate))
            {
                return Candidate;
            }
        }
        return FString{};
    }

    void PrepareMapLoadingScreen()
    {
        if (IsRunningDedicatedServer())
        {
            return;
        }

        FLoadingScreenAttributes Attributes;
        Attributes.bAutoCompleteWhenLoadingCompletes = true;
        Attributes.bMoviesAreSkippable = false;
        Attributes.bWaitForManualStop = false;
        Attributes.MinimumLoadingScreenDisplayTime = 0.15f;
        Attributes.WidgetLoadingScreen =
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(TGUiTheme::GetPalette().Backdrop)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SNew(SThrobber)
                .NumPieces(5)
            ];
        GetMoviePlayer()->SetupLoadingScreen(Attributes);
    }

    bool ParseCsvLine(
        const FString& Line,
        TArray<FString>& OutFields)
    {
        OutFields.Reset();
        FString Current;
        bool bInsideQuotes = false;
        for (int32 Index = 0; Index < Line.Len(); ++Index)
        {
            const TCHAR Character = Line[Index];
            if (Character == TEXT('"'))
            {
                if (bInsideQuotes && Index + 1 < Line.Len() &&
                    Line[Index + 1] == TEXT('"'))
                {
                    Current.AppendChar(TEXT('"'));
                    ++Index;
                }
                else
                {
                    bInsideQuotes = !bInsideQuotes;
                }
            }
            else if (Character == TEXT(',') && !bInsideQuotes)
            {
                OutFields.Add(MoveTemp(Current));
                Current.Reset();
            }
            else
            {
                Current.AppendChar(Character);
            }
        }
        if (bInsideQuotes)
        {
            OutFields.Reset();
            return false;
        }
        OutFields.Add(MoveTemp(Current));
        return true;
    }

    bool ReadCsvBoundaryRows(
        const FString& FilePath,
        FString& OutHeader,
        FString& OutFirstDataRow,
        FString& OutFinalDataRow,
        int64& OutSampleCount,
        FString& OutError)
    {
        OutHeader.Reset();
        OutFirstDataRow.Reset();
        OutFinalDataRow.Reset();
        OutSampleCount = 0;
        OutError.Reset();

        TUniquePtr<FArchive> Reader(
            IFileManager::Get().CreateFileReader(*FilePath));
        if (!Reader)
        {
            OutError = FString::Printf(
                TEXT("The result CSV could not be opened: %s"),
                *FilePath);
            return false;
        }

        constexpr int32 ChunkSize = 64 * 1024;
        TArray<uint8> Chunk;
        Chunk.SetNumUninitialized(ChunkSize);
        TArray<uint8> LineBytes;
        LineBytes.Reserve(128 * 1024);

        auto FlushLine = [&]()
        {
            while (!LineBytes.IsEmpty() &&
                   LineBytes.Last() == static_cast<uint8>('\r'))
            {
                LineBytes.Pop(EAllowShrinking::No);
            }
            if (LineBytes.IsEmpty())
            {
                return;
            }

            const FUTF8ToTCHAR Converted(
                reinterpret_cast<const ANSICHAR*>(LineBytes.GetData()),
                LineBytes.Num());
            FString Line(Converted.Length(), Converted.Get());
            if (OutHeader.IsEmpty())
            {
                Line.RemoveFromStart(TEXT("\xFEFF"));
                OutHeader = MoveTemp(Line);
            }
            else
            {
                if (OutFirstDataRow.IsEmpty())
                {
                    OutFirstDataRow = Line;
                }
                OutFinalDataRow = MoveTemp(Line);
                ++OutSampleCount;
            }
            LineBytes.Reset();
        };

        while (!Reader->AtEnd())
        {
            const int64 Remaining = Reader->TotalSize() - Reader->Tell();
            const int32 BytesToRead = static_cast<int32>(
                FMath::Min<int64>(ChunkSize, Remaining));
            Reader->Serialize(Chunk.GetData(), BytesToRead);
            if (Reader->IsError())
            {
                OutError = FString::Printf(
                    TEXT("The result CSV could not be read: %s"),
                    *FilePath);
                return false;
            }
            for (int32 Index = 0; Index < BytesToRead; ++Index)
            {
                if (Chunk[Index] == static_cast<uint8>('\n'))
                {
                    FlushLine();
                }
                else
                {
                    LineBytes.Add(Chunk[Index]);
                }
            }
        }
        FlushLine();

        if (OutHeader.IsEmpty() || OutSampleCount <= 0)
        {
            OutError = TEXT("The result CSV has no header and final data row.");
            return false;
        }
        return true;
    }

    bool ParseFiniteCell(
        const TArray<FString>& Fields,
        const TMap<FString, int32>& Columns,
        const FString& Name,
        double& OutValue,
        FString& OutError)
    {
        const int32* Index = Columns.Find(Name);
        if (Index == nullptr || !Fields.IsValidIndex(*Index))
        {
            OutError = FString::Printf(
                TEXT("The result CSV is missing final-state column '%s'."),
                *Name);
            return false;
        }
        if (!LexTryParseString(OutValue, *Fields[*Index]) ||
            !FMath::IsFinite(OutValue))
        {
            OutError = FString::Printf(
                TEXT("The final value in result column '%s' is not finite."),
                *Name);
            return false;
        }
        return true;
    }

    bool BuildFinalStateSnapshot(
        const FString& CsvPath,
        const FTGSimulationScenario& Scenario,
        FTGSimulationFinalState& OutFinalState,
        int64& OutSampleCount,
        double& OutStartEt,
        double& OutFinalEt,
        FString& OutError)
    {
        OutFinalState = FTGSimulationFinalState{};
        OutStartEt = 0.0;
        OutFinalEt = 0.0;

        FString HeaderLine;
        FString FirstLine;
        FString FinalLine;
        if (!ReadCsvBoundaryRows(
                CsvPath,
                HeaderLine,
                FirstLine,
                FinalLine,
                OutSampleCount,
                OutError))
        {
            return false;
        }

        TArray<FString> Headers;
        TArray<FString> FirstFields;
        TArray<FString> FinalFields;
        if (!ParseCsvLine(HeaderLine, Headers) ||
            !ParseCsvLine(FirstLine, FirstFields) ||
            !ParseCsvLine(FinalLine, FinalFields) ||
            Headers.Num() != FirstFields.Num() ||
            Headers.Num() != FinalFields.Num())
        {
            OutError = TEXT("The result CSV boundary rows are malformed.");
            return false;
        }

        TMap<FString, int32> Columns;
        for (int32 Index = 0; Index < Headers.Num(); ++Index)
        {
            FString Name = Headers[Index].TrimStartAndEnd();
            if (Name.IsEmpty() || Columns.Contains(Name))
            {
                OutError = TEXT("The result CSV has an empty or duplicate header.");
                return false;
            }
            Columns.Add(MoveTemp(Name), Index);
        }

        const FString TimeColumn =
            TEXT("ephemeris_time_tdb_seconds_past_j2000");
        if (!ParseFiniteCell(
                FirstFields, Columns, TimeColumn, OutStartEt, OutError) ||
            !ParseFiniteCell(
                FinalFields, Columns, TimeColumn, OutFinalEt, OutError))
        {
            return false;
        }

        FTGInitialSpacecraftState& State = OutFinalState.SpacecraftState;
        if (!ParseFiniteCell(FinalFields, Columns,
                TEXT("position_icrf_x_m"), State.PositionMeters.X, OutError) ||
            !ParseFiniteCell(FinalFields, Columns,
                TEXT("position_icrf_y_m"), State.PositionMeters.Y, OutError) ||
            !ParseFiniteCell(FinalFields, Columns,
                TEXT("position_icrf_z_m"), State.PositionMeters.Z, OutError) ||
            !ParseFiniteCell(FinalFields, Columns,
                TEXT("velocity_icrf_x_mps"), State.VelocityMetersPerSecond.X, OutError) ||
            !ParseFiniteCell(FinalFields, Columns,
                TEXT("velocity_icrf_y_mps"), State.VelocityMetersPerSecond.Y, OutError) ||
            !ParseFiniteCell(FinalFields, Columns,
                TEXT("velocity_icrf_z_mps"), State.VelocityMetersPerSecond.Z, OutError) ||
            !ParseFiniteCell(FinalFields, Columns,
                TEXT("quaternion_body_to_icrf_w"), State.AttitudeBodyToIcrf.W, OutError) ||
            !ParseFiniteCell(FinalFields, Columns,
                TEXT("quaternion_body_to_icrf_x"), State.AttitudeBodyToIcrf.X, OutError) ||
            !ParseFiniteCell(FinalFields, Columns,
                TEXT("quaternion_body_to_icrf_y"), State.AttitudeBodyToIcrf.Y, OutError) ||
            !ParseFiniteCell(FinalFields, Columns,
                TEXT("quaternion_body_to_icrf_z"), State.AttitudeBodyToIcrf.Z, OutError) ||
            !ParseFiniteCell(FinalFields, Columns,
                TEXT("angular_velocity_body_x_radps"),
                State.AngularVelocityBodyRadiansPerSecond.X, OutError) ||
            !ParseFiniteCell(FinalFields, Columns,
                TEXT("angular_velocity_body_y_radps"),
                State.AngularVelocityBodyRadiansPerSecond.Y, OutError) ||
            !ParseFiniteCell(FinalFields, Columns,
                TEXT("angular_velocity_body_z_radps"),
                State.AngularVelocityBodyRadiansPerSecond.Z, OutError))
        {
            return false;
        }
        State.AttitudeBodyToIcrf.Normalize();

        int32 VariableMassIndex = 0;
        int32 JointIndex = 0;
        for (const FTGComponentConfig& Component : Scenario.Components)
        {
            FTGFinalComponentMass ComponentMass;
            ComponentMass.ComponentName = Component.Name;
            ComponentMass.MassKilograms = Component.InitialMassKilograms;
            if (Component.bVariableMass)
            {
                if (!ParseFiniteCell(
                        FinalFields,
                        Columns,
                        FString::Printf(
                            TEXT("variable_component_mass_%d_kg"),
                            VariableMassIndex),
                        ComponentMass.MassKilograms,
                        OutError))
                {
                    return false;
                }
                ++VariableMassIndex;
            }
            OutFinalState.ComponentMasses.Add(MoveTemp(ComponentMass));

            for (const FTGJointDofConfig& Dof :
                 Component.DegreesOfFreedom)
            {
                FTGFinalJointState Joint;
                Joint.ComponentName = Component.Name;
                Joint.DegreeOfFreedomName = Dof.Name;
                if (!ParseFiniteCell(
                        FinalFields,
                        Columns,
                        FString::Printf(
                            TEXT("articulation_coordinate_%d_rad_or_m"),
                            JointIndex),
                        Joint.Coordinate,
                        OutError) ||
                    !ParseFiniteCell(
                        FinalFields,
                        Columns,
                        FString::Printf(
                            TEXT("articulation_rate_%d_radps_or_mps"),
                            JointIndex),
                        Joint.Rate,
                        OutError))
                {
                    return false;
                }
                OutFinalState.JointStates.Add(MoveTemp(Joint));
                ++JointIndex;
            }
        }

        for (int32 Index = 0;
             Index < Scenario.ReactionWheels.Num();
             ++Index)
        {
            FTGFinalReactionWheelState Wheel;
            Wheel.ReactionWheelName = Scenario.ReactionWheels[Index].Name;
            if (!ParseFiniteCell(
                    FinalFields,
                    Columns,
                    FString::Printf(
                        TEXT("reaction_wheel_momentum_%d_nms"), Index),
                    Wheel.MomentumNewtonMeterSeconds,
                    OutError))
            {
                return false;
            }
            OutFinalState.ReactionWheelStates.Add(MoveTemp(Wheel));
        }

        OutFinalState.bIsValid = true;
        OutFinalState.EphemerisTimeTdbSeconds = OutFinalEt;
        return true;
    }
}

UTGSimulationRunSubsystem::UTGSimulationRunSubsystem()
{
    ConfigurationLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(
        TEXT("/Game/Maps/MainMenu.MainMenu")));
    VisualizationLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(
        TEXT("/Game/Maps/SolarSystem.SolarSystem")));
}

void UTGSimulationRunSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    SetRunState(ETGSimulationRunState::Idle);
}

void UTGSimulationRunSubsystem::Deinitialize()
{
    ++ScenarioPreparationGeneration;
    bScenarioPreparationInFlight = false;
    ++ResultFinalizationGeneration;
    bResultFinalizationInFlight = false;
    if (RunnerProcess.IsValid() &&
        FPlatformProcess::IsProcRunning(RunnerProcess))
    {
        FPlatformProcess::TerminateProc(RunnerProcess, true);
    }
    CloseRunnerResources();
    Super::Deinitialize();
}

void UTGSimulationRunSubsystem::Tick(float DeltaTime)
{
    (void)DeltaTime;
    ApplyPendingMenuDestination();

    if (!RunnerProcess.IsValid())
    {
        return;
    }

    PollRunnerOutput();

    if (!FPlatformProcess::IsProcRunning(RunnerProcess))
    {
        PollRunnerOutput();
        FinalizeExitedRunner();
        return;
    }

    const double Now = FPlatformTime::Seconds();
    if (!bStopRequested &&
        MaximumWallClockRuntimeSeconds > 0.0 &&
        Now - RunnerStartedMonotonicSeconds >=
            MaximumWallClockRuntimeSeconds)
    {
        RequestRunnerStop(true);
    }

    if (bStopRequested && Now >= ForceStopMonotonicSeconds &&
        FPlatformProcess::IsProcRunning(RunnerProcess))
    {
        RunnerLog += TEXT("Hard-stopping PHAROS Scenario Runner after the cancellation grace period.\n");
        FPlatformProcess::TerminateProc(RunnerProcess, true);
    }
}

bool UTGSimulationRunSubsystem::IsTickable() const
{
    return RunnerProcess.IsValid() ||
        PendingMenuDestination != EPendingMenuDestination::None;
}

bool UTGSimulationRunSubsystem::IsTickableWhenPaused() const
{
    return true;
}

TStatId UTGSimulationRunSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(
        UTGSimulationRunSubsystem,
        STATGROUP_Tickables);
}

UWorld* UTGSimulationRunSubsystem::GetTickableGameObjectWorld() const
{
    return GetGameInstance() != nullptr
        ? GetGameInstance()->GetWorld()
        : nullptr;
}

bool UTGSimulationRunSubsystem::StartCurrentSimulation(FText& OutError)
{
    OutError = FText::GetEmpty();
    if (IsSimulationRunning())
    {
        OutError = FText::FromString(
            TEXT("A simulation is already running."));
        return false;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UTGSimulationSubsystem* ScenarioSubsystem = GameInstance != nullptr
        ? GameInstance->GetSubsystem<UTGSimulationSubsystem>()
        : nullptr;
    if (ScenarioSubsystem == nullptr)
    {
        OutError = FText::FromString(
            TEXT("The scenario library is unavailable."));
        FailBeforeLaunch(TEXT("Scenario"), OutError);
        return false;
    }

    FText GateReason;
    if (!ScenarioSubsystem->CanSimulateCurrentScenario(GateReason))
    {
        OutError = GateReason;
        FailBeforeLaunch(TEXT("Review"), OutError);
        return false;
    }

    FTGSimulationScenario Scenario =
        ScenarioSubsystem->GetCurrentScenarioDraft();
    MaximumWallClockRuntimeSeconds =
        Scenario.ScenarioAndSolver.MaximumWallClockRuntimeSeconds;
    if (!FMath::IsFinite(MaximumWallClockRuntimeSeconds) ||
        MaximumWallClockRuntimeSeconds <= 0.0)
    {
        OutError = FText::FromString(
            TEXT("Maximum backend runtime must be a positive finite number of seconds."));
        FailBeforeLaunch(TEXT("Execution policy"), OutError);
        return false;
    }

    FString ControllerDllPath;
    if (!ResolveControllerDllPath(
            GameInstance,
            Scenario,
            ControllerDllPath,
            OutError))
    {
        FailBeforeLaunch(TEXT("Controller"), OutError);
        return false;
    }

    LastRunError = FTGSimulationRunError{};
    CurrentRun = FTGCompletedSimulationRun{};
    CurrentRun.RunId = FGuid::NewGuid();
    CurrentRun.SourceScenarioId =
        ScenarioSubsystem->IsCurrentScenarioSaved() &&
        !ScenarioSubsystem->IsCurrentScenarioDirty()
            ? ScenarioSubsystem->GetCurrentScenarioId()
            : FGuid{};
    CurrentRun.ScenarioName =
        Scenario.ScenarioAndSolver.ScenarioName.TrimStartAndEnd();
    CurrentRun.StartedUtc = FDateTime::UtcNow();
    CurrentRun.InputSnapshot = FTGSimulationScenario{};
    PendingRunnerOutput.Reset();
    RunnerLog.Reset();
    LastRunnerResultPath.Reset();
    LastRunnerErrorMessage.Reset();
    bStopRequested = false;
    bStopWasTimeout = false;
    bStopWasUserCancellation = false;

    SetRunState(ETGSimulationRunState::PreparingScenario);
    PublishProgress(0.0f, FText::FromString(
        TEXT("Preparing scenario snapshot")));

    if (!CreateRunFiles(OutError))
    {
        FailBeforeLaunch(TEXT("Scenario export"), OutError, RunnerLog);
        return false;
    }

    bScenarioPreparationInFlight = true;
    const uint64 PreparationGeneration =
        ++ScenarioPreparationGeneration;
    const FString ScenarioFilePath = CurrentRun.ScenarioFilePath;
    const TWeakObjectPtr<UTGSimulationRunSubsystem> WeakThis(this);
    Async(
        EAsyncExecution::ThreadPool,
        [WeakThis,
         PreparationGeneration,
         Scenario = MoveTemp(Scenario),
         ControllerDllPath,
         ScenarioFilePath]() mutable
        {
            FScenarioPreparationResult Result;
            Result.Scenario = MoveTemp(Scenario);

            FText PreparationSummary;
            Result.bSucceeded =
                UTGSolarRadiationPressureEditingLibrary::
                    PrepareSolarRadiationPressureGeometry(
                        Result.Scenario,
                        PreparationSummary,
                        Result.PreparationWarning,
                        Result.Error);
            if (Result.bSucceeded)
            {
                Result.bSucceeded = SavePreparedScenario(
                    Result.Scenario,
                    ControllerDllPath,
                    ScenarioFilePath,
                    Result.ExportMessage);
                if (!Result.bSucceeded)
                {
                    Result.Error = Result.ExportMessage;
                }
            }

            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis,
                 PreparationGeneration,
                 Result = MoveTemp(Result)]() mutable
                {
                    if (UTGSimulationRunSubsystem* Subsystem =
                            WeakThis.Get())
                    {
                        Subsystem->HandleScenarioPreparationCompleted(
                            PreparationGeneration,
                            Result.bSucceeded,
                            MoveTemp(Result.Scenario),
                            Result.PreparationWarning,
                            Result.ExportMessage,
                            Result.Error);
                    }
                });
        });
    return true;
}

void UTGSimulationRunSubsystem::CancelSimulation()
{
    if (RunState == ETGSimulationRunState::PreparingScenario &&
        bScenarioPreparationInFlight)
    {
        ++ScenarioPreparationGeneration;
        bScenarioPreparationInFlight = false;
        bStopWasUserCancellation = true;
        RemoveIncompleteResultFiles();
        FinishUnsuccessfulRun(
            ETGSimulationRunState::Cancelled,
            TEXT("Cancellation"),
            FText::FromString(TEXT("The simulation was cancelled.")),
            INDEX_NONE);
        return;
    }

    if (IsSimulationRunning() && !bStopRequested)
    {
        RequestRunnerStop(false);
    }
}

void UTGSimulationRunSubsystem::HandleScenarioPreparationCompleted(
    const uint64 PreparationGeneration,
    const bool bSucceeded,
    FTGSimulationScenario&& PreparedScenario,
    const FText& PreparationWarning,
    const FText& ExportMessage,
    const FText& Error)
{
    if (PreparationGeneration != ScenarioPreparationGeneration ||
        !bScenarioPreparationInFlight)
    {
        return;
    }

    bScenarioPreparationInFlight = false;
    if (!bSucceeded)
    {
        const FText Failure = Error.IsEmpty()
            ? FText::FromString(TEXT(
                "The scenario could not be prepared for simulation."))
            : Error;
        FailBeforeLaunch(TEXT("Scenario preparation"), Failure, RunnerLog);
        return;
    }

    CurrentRun.InputSnapshot = MoveTemp(PreparedScenario);
    if (!PreparationWarning.IsEmpty())
    {
        RunnerLog += PreparationWarning.ToString() + TEXT("\n");
    }
    if (!ExportMessage.IsEmpty())
    {
        RunnerLog += ExportMessage.ToString() + TEXT("\n");
    }

    SetRunState(ETGSimulationRunState::LaunchingRunner);
    PublishProgress(1.0f, FText::FromString(
        TEXT("Launching isolated backend")));

    FText LaunchError;
    if (!LaunchRunner(LaunchError))
    {
        FailBeforeLaunch(
            TEXT("Runner launch"),
            LaunchError,
            RunnerLog);
        return;
    }

    SetRunState(ETGSimulationRunState::Running);
}

void UTGSimulationRunSubsystem::ResetRunState()
{
    if (IsSimulationRunning())
    {
        return;
    }
    LastRunError = FTGSimulationRunError{};
    ProgressPercent = 0.0f;
    ProgressStatus = FText::GetEmpty();
    SetRunState(ETGSimulationRunState::Idle);
}

bool UTGSimulationRunSubsystem::IsSimulationRunning() const
{
    return RunState == ETGSimulationRunState::PreparingScenario ||
        RunState == ETGSimulationRunState::LaunchingRunner ||
        RunState == ETGSimulationRunState::Running ||
        RunState == ETGSimulationRunState::Cancelling ||
        RunState == ETGSimulationRunState::Finalizing;
}

ETGSimulationRunState UTGSimulationRunSubsystem::GetRunState() const
{
    return RunState;
}

float UTGSimulationRunSubsystem::GetProgressPercent() const
{
    return ProgressPercent;
}

FText UTGSimulationRunSubsystem::GetProgressStatus() const
{
    return ProgressStatus;
}

FTGSimulationRunError UTGSimulationRunSubsystem::GetLastRunError() const
{
    return LastRunError;
}

bool UTGSimulationRunSubsystem::HasCompletedRun() const
{
    return bHasLatestCompletedRun && LatestCompletedRun.IsUsable();
}

FTGCompletedSimulationRun
UTGSimulationRunSubsystem::GetLatestCompletedRun() const
{
    return LatestCompletedRun;
}

bool UTGSimulationRunSubsystem::OpenLatestRunVisualization(FText& OutError)
{
    OutError = FText::GetEmpty();
    if (!HasCompletedRun())
    {
        OutError = FText::FromString(
            TEXT("No completed simulation is available for visualization."));
        return false;
    }
    if (VisualizationLevel.IsNull())
    {
        OutError = FText::FromString(
            TEXT("The visualization level is not configured."));
        return false;
    }

    PrepareMapLoadingScreen();
    UGameplayStatics::OpenLevelBySoftObjectPtr(
        this, VisualizationLevel, true);
    return true;
}

bool UTGSimulationRunSubsystem::OpenLatestRunVisualizationForScenario(
    const FGuid& ScenarioId,
    FText& OutError)
{
    OutError = FText::GetEmpty();
    UGameInstance* GameInstance = GetGameInstance();
    UTGSimulationSubsystem* ScenarioSubsystem = GameInstance != nullptr
        ? GameInstance->GetSubsystem<UTGSimulationSubsystem>()
        : nullptr;
    if (ScenarioSubsystem == nullptr)
    {
        OutError = FText::FromString(
            TEXT("The scenario library is unavailable."));
        return false;
    }

    FTGSavedSimulationRun SavedRun;
    if (!ScenarioSubsystem->GetLatestCompletedRunForScenario(
            ScenarioId,
            SavedRun))
    {
        OutError = FText::FromString(
            TEXT("This scenario has no available completed result."));
        return false;
    }

    const FString ResultPath =
        UTGSimulationSubsystem::ResolveSavedRunResultPath(
            SavedRun.ResultRelativePath);
    const FString RunDirectory = FPaths::GetPath(ResultPath);
    const FString ScenarioPath = FPaths::Combine(
        RunDirectory,
        TEXT("scenario.tgscn"));
    if (!IFileManager::Get().FileExists(*ResultPath) ||
        !IFileManager::Get().FileExists(*ScenarioPath))
    {
        OutError = FText::FromString(
            TEXT("The saved simulation result files are no longer available."));
        return false;
    }

    LatestCompletedRun = FTGCompletedSimulationRun{};
    LatestCompletedRun.RunId = SavedRun.RunId;
    LatestCompletedRun.SourceScenarioId = SavedRun.SourceScenarioId;
    LatestCompletedRun.ScenarioName = SavedRun.RunName;
    LatestCompletedRun.RunDirectory = RunDirectory;
    LatestCompletedRun.ScenarioFilePath = ScenarioPath;
    LatestCompletedRun.ResultCsvFilePath = ResultPath;
    LatestCompletedRun.SummaryFilePath = FPaths::Combine(
        RunDirectory,
        TEXT("scenario_summary.txt"));
    LatestCompletedRun.LogFilePath = FPaths::Combine(
        RunDirectory,
        TEXT("run.log"));
    LatestCompletedRun.StartedUtc = SavedRun.CreatedUtc;
    LatestCompletedRun.CompletedUtc = SavedRun.CreatedUtc;
    LatestCompletedRun.InputSnapshot = SavedRun.InputSnapshot;
    bHasLatestCompletedRun = true;
    if (!ScenarioSubsystem->MarkScenarioRecentlyUsed(ScenarioId))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("PHAROS: Prepared visualization for scenario %s, but its recent-use timestamp could not be saved."),
            *ScenarioId.ToString());
    }
    return OpenLatestRunVisualization(OutError);
}

bool UTGSimulationRunSubsystem::ReturnToConfigurationLevel(FText& OutError)
{
    return BeginReturnToConfigurationMap(
        EPendingMenuDestination::Configuration,
        OutError);
}

bool UTGSimulationRunSubsystem::ReturnToScenarioLibraryLevel(FText& OutError)
{
    return BeginReturnToConfigurationMap(
        EPendingMenuDestination::ScenarioLibrary,
        OutError);
}

bool UTGSimulationRunSubsystem::ReturnToMainMenuLevel(FText& OutError)
{
    return BeginReturnToConfigurationMap(
        EPendingMenuDestination::MainMenu,
        OutError);
}

bool UTGSimulationRunSubsystem::BeginReturnToConfigurationMap(
    const EPendingMenuDestination Destination,
    FText& OutError)
{
    OutError = FText::GetEmpty();
    if (IsSimulationRunning())
    {
        OutError = FText::FromString(
            TEXT("Wait for the active simulation to finish or cancel it before changing levels."));
        return false;
    }
    if (ConfigurationLevel.IsNull())
    {
        OutError = FText::FromString(
            TEXT("The configuration level is not configured."));
        return false;
    }

    bPendingConfigurationRestore =
        Destination == EPendingMenuDestination::Configuration;
    PendingMenuDestination = Destination;
    PrepareMapLoadingScreen();
    UGameplayStatics::OpenLevelBySoftObjectPtr(
        this, ConfigurationLevel, true);
    return true;
}

bool UTGSimulationRunSubsystem::ApplyPendingMenuDestination()
{
    if (PendingMenuDestination == EPendingMenuDestination::None)
    {
        return false;
    }

    UWorld* World = GetWorld();
    if (World == nullptr ||
        !World->GetMapName().EndsWith(TEXT("MainMenu")))
    {
        return false;
    }

    for (TObjectIterator<UWidgetSwitcher> It; It; ++It)
    {
        UWidgetSwitcher* Switcher = *It;
        if (!IsValid(Switcher) ||
            Switcher->GetWorld() != World ||
            Switcher->GetFName() != TEXT("ScreenSwitcher"))
        {
            continue;
        }

        int32 DestinationIndex = 0;
        switch (PendingMenuDestination)
        {
        case EPendingMenuDestination::Configuration:
            DestinationIndex = 1;
            break;
        case EPendingMenuDestination::ScenarioLibrary:
            DestinationIndex = 2;
            break;
        case EPendingMenuDestination::MainMenu:
        case EPendingMenuDestination::None:
        default:
            DestinationIndex = 0;
            break;
        }

        if (Switcher->GetChildrenCount() <= DestinationIndex)
        {
            return false;
        }

        Switcher->SetActiveWidgetIndex(DestinationIndex);
        PendingMenuDestination = EPendingMenuDestination::None;
        return true;
    }

    return false;
}

bool UTGSimulationRunSubsystem::ConsumePendingConfigurationRestore()
{
    const bool bWasPending = bPendingConfigurationRestore;
    bPendingConfigurationRestore = false;
    return bWasPending;
}

void UTGSimulationRunSubsystem::SetRunnerExecutableOverride(
    const FString& ExecutablePath)
{
    if (!IsSimulationRunning())
    {
        RunnerExecutableOverride = ExecutablePath.TrimStartAndEnd();
    }
}

void UTGSimulationRunSubsystem::SetAutoOpenVisualizationOnSuccess(
    const bool bEnabled)
{
    bAutoOpenVisualizationOnSuccess = bEnabled;
}

void UTGSimulationRunSubsystem::SetRunState(
    const ETGSimulationRunState NewState)
{
    if (RunState == NewState)
    {
        return;
    }
    RunState = NewState;
    OnRunStateChanged.Broadcast(RunState);
}

void UTGSimulationRunSubsystem::PublishProgress(
    const float Percent,
    const FText& Status)
{
    ProgressPercent = FMath::Clamp(Percent, 0.0f, 100.0f);
    ProgressStatus = Status;
    OnRunProgress.Broadcast(ProgressPercent, ProgressStatus);
}

void UTGSimulationRunSubsystem::FailBeforeLaunch(
    const FName Stage,
    const FText& Message,
    const FString& TechnicalDetails)
{
    LastRunError.State = ETGSimulationRunState::Failed;
    LastRunError.Stage = Stage;
    LastRunError.Message = Message;
    LastRunError.TechnicalDetails = TechnicalDetails;
    LastRunError.ProcessExitCode = INDEX_NONE;
    SetRunState(ETGSimulationRunState::Failed);
    OnRunFailed.Broadcast(LastRunError);
}

bool UTGSimulationRunSubsystem::CreateRunFiles(FText& OutError)
{
    FString SafeScenarioName = FPaths::MakeValidFileName(
        CurrentRun.ScenarioName);
    if (SafeScenarioName.IsEmpty())
    {
        SafeScenarioName = TEXT("UntitledScenario");
    }

    const FString Timestamp = CurrentRun.StartedUtc.ToString(
        TEXT("%Y%m%dT%H%M%SZ"));
    const FString RunToken = CurrentRun.RunId.ToString(
        EGuidFormats::Short);
    CurrentRun.RunDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("SimulationResults"),
            SafeScenarioName,
            Timestamp + TEXT("_") + RunToken));

    if (!IFileManager::Get().MakeDirectory(
            *CurrentRun.RunDirectory, true))
    {
        OutError = FText::FromString(FString::Printf(
            TEXT("Could not create simulation output directory: %s"),
            *CurrentRun.RunDirectory));
        return false;
    }

    CurrentRun.ScenarioFilePath = FPaths::Combine(
        CurrentRun.RunDirectory, TEXT("scenario.tgscn"));
    CurrentRun.ResultCsvFilePath = FPaths::Combine(
        CurrentRun.RunDirectory, TEXT("scenario_solution.csv"));
    CurrentRun.SummaryFilePath = FPaths::Combine(
        CurrentRun.RunDirectory, TEXT("scenario_summary.txt"));
    CurrentRun.LogFilePath = FPaths::Combine(
        CurrentRun.RunDirectory, TEXT("run.log"));
    return true;
}

bool UTGSimulationRunSubsystem::LaunchRunner(FText& OutError)
{
    FString SearchedPaths;
    const FString RunnerPath = ResolveRunnerExecutable(SearchedPaths);
    if (RunnerPath.IsEmpty())
    {
        OutError = FText::FromString(
            TEXT("PHAROS Scenario Runner was not found. Build it before starting a simulation."));
        RunnerLog += TEXT("Searched runner paths:\n") + SearchedPaths;
        return false;
    }

    if (!FPlatformProcess::CreatePipe(
            RunnerOutputReadPipe, RunnerOutputWritePipe) ||
        !FPlatformProcess::CreatePipe(
            RunnerInputReadPipe, RunnerInputWritePipe, true))
    {
        OutError = FText::FromString(
            TEXT("Could not create communication pipes for PHAROS Scenario Runner."));
        CloseRunnerResources();
        return false;
    }

    const FString KernelDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectContentDir(), TEXT("SPICEKernels")));
    const FString Arguments = FString::Printf(
        TEXT("%s --output %s --kernel-dir %s"),
        *QuoteArgument(CurrentRun.ScenarioFilePath),
        *QuoteArgument(CurrentRun.RunDirectory),
        *QuoteArgument(KernelDirectory));
    const FString WorkingDirectory = FPaths::GetPath(RunnerPath);

    RunnerProcess = FPlatformProcess::CreateProc(
        *RunnerPath,
        *Arguments,
        false,
        true,
        true,
        &RunnerProcessId,
        0,
        *WorkingDirectory,
        RunnerOutputWritePipe,
        RunnerInputReadPipe,
        RunnerOutputWritePipe);
    if (!RunnerProcess.IsValid())
    {
        OutError = FText::FromString(FString::Printf(
            TEXT("Could not launch PHAROS Scenario Runner: %s"),
            *RunnerPath));
        CloseRunnerResources();
        return false;
    }

    RunnerLog += FString::Printf(
        TEXT("Runner: %s\nArguments: %s\nProcess ID: %u\n"),
        *RunnerPath, *Arguments, RunnerProcessId);
    RunnerStartedMonotonicSeconds = FPlatformTime::Seconds();
    return true;
}

FString UTGSimulationRunSubsystem::ResolveRunnerExecutable(
    FString& OutSearchedPaths) const
{
    TArray<FString> Candidates;
    if (!RunnerExecutableOverride.IsEmpty())
    {
        Candidates.Add(RunnerExecutableOverride);
    }
    Candidates.Add(FPaths::Combine(
        FPlatformProcess::BaseDir(), RunnerExecutableName()));
    Candidates.Add(FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Binaries"), TEXT("Win64"),
        RunnerExecutableName()));
    Candidates.Add(FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Tools"), TEXT("TGScenarioRunner"),
        TEXT("bin"), RunnerExecutableName()));

    for (FString Candidate : Candidates)
    {
        Candidate = FPaths::ConvertRelativePathToFull(Candidate);
        OutSearchedPaths += Candidate + TEXT("\n");
        if (IFileManager::Get().FileExists(*Candidate))
        {
            return Candidate;
        }
    }
    return FString{};
}

void UTGSimulationRunSubsystem::PollRunnerOutput()
{
    if (RunnerOutputReadPipe == nullptr)
    {
        return;
    }

    PendingRunnerOutput += FPlatformProcess::ReadPipe(
        RunnerOutputReadPipe);
    int32 NewlineIndex = INDEX_NONE;
    while (PendingRunnerOutput.FindChar(TEXT('\n'), NewlineIndex))
    {
        FString Line = PendingRunnerOutput.Left(NewlineIndex);
        PendingRunnerOutput.RightChopInline(
            NewlineIndex + 1, EAllowShrinking::No);
        Line.RemoveFromEnd(TEXT("\r"));
        ProcessRunnerOutputLine(Line);
    }
}

void UTGSimulationRunSubsystem::ProcessRunnerOutputLine(
    const FString& Line)
{
    if (Line.IsEmpty())
    {
        return;
    }

    RunnerLog += Line + TEXT("\n");
    if (!IsRunnerProtocolLine(Line))
    {
        return;
    }

    TArray<FString> Fields;
    Line.ParseIntoArray(Fields, TEXT("\t"), false);
    if (Fields.Num() < 3)
    {
        return;
    }

    if (Fields[1] == TEXT("PROGRESS") && Fields.Num() >= 4)
    {
        double Percent = 0.0;
        if (LexTryParseString(Percent, *Fields[2]))
        {
            PublishProgress(
                FMath::Min(static_cast<float>(Percent), 97.0f),
                FText::FromString(Fields[3]));
        }
    }
    else if (Fields[1] == TEXT("LOG"))
    {
        ProgressStatus = FText::FromString(Fields.Last());
    }
    else if (Fields[1] == TEXT("RESULT"))
    {
        LastRunnerResultPath = Fields.Last();
    }
    else if (Fields[1] == TEXT("ERROR"))
    {
        LastRunnerErrorMessage = Fields.Last();
    }
}

void UTGSimulationRunSubsystem::RequestRunnerStop(const bool bTimedOut)
{
    if (!RunnerProcess.IsValid() || bStopRequested)
    {
        return;
    }

    bStopRequested = true;
    bStopWasTimeout = bTimedOut;
    bStopWasUserCancellation = !bTimedOut;
    SetRunState(ETGSimulationRunState::Cancelling);
    PublishProgress(
        ProgressPercent,
        FText::FromString(
            bTimedOut
                ? TEXT("Runtime limit reached; stopping backend")
                : TEXT("Cancelling simulation")));

    if (RunnerInputWritePipe != nullptr)
    {
        FPlatformProcess::WritePipe(
            RunnerInputWritePipe, TEXT("CANCEL\n"));
    }
    ForceStopMonotonicSeconds =
        FPlatformTime::Seconds() + kCancellationGraceSeconds;
}

void UTGSimulationRunSubsystem::FinalizeExitedRunner()
{
    SetRunState(ETGSimulationRunState::Finalizing);
    PublishProgress(98.0f, FText::FromString(
        TEXT("Finalizing simulation result")));
    if (!PendingRunnerOutput.IsEmpty())
    {
        ProcessRunnerOutputLine(PendingRunnerOutput);
        PendingRunnerOutput.Reset();
    }

    int32 ExitCode = INDEX_NONE;
    FPlatformProcess::GetProcReturnCode(RunnerProcess, &ExitCode);
    SaveRunnerLog();

    if (bStopWasTimeout)
    {
        RemoveIncompleteResultFiles();
        CloseRunnerResources();
        FinishUnsuccessfulRun(
            ETGSimulationRunState::TimedOut,
            TEXT("Runtime limit"),
            FText::FromString(FString::Printf(
                TEXT(
                    "The simulation did not finish within %.3f real-time "
                    "seconds. Right-click Simulate and choose Limit "
                    "Execution Time to increase the limit."),
                MaximumWallClockRuntimeSeconds)),
            ExitCode);
        return;
    }
    if (bStopWasUserCancellation)
    {
        RemoveIncompleteResultFiles();
        CloseRunnerResources();
        FinishUnsuccessfulRun(
            ETGSimulationRunState::Cancelled,
            TEXT("Cancellation"),
            FText::FromString(TEXT("The simulation was cancelled.")),
            ExitCode);
        return;
    }

    if (ExitCode != 0)
    {
        const FText ExitMessage = DescribeRunnerExitCode(ExitCode);
        const FString SpecificMessage = LastRunnerErrorMessage.IsEmpty()
            ? LastNonEmptyLine(RunnerLog)
            : LastRunnerErrorMessage;
        CloseRunnerResources();
        FinishUnsuccessfulRun(
            ETGSimulationRunState::Failed,
            TEXT("Backend"),
            SpecificMessage.IsEmpty()
                ? ExitMessage
                : FText::FromString(SpecificMessage),
            ExitCode);
        return;
    }

    CloseRunnerResources();
    bResultFinalizationInFlight = true;
    const uint64 FinalizationGeneration =
        ++ResultFinalizationGeneration;
    const FString ResultCsvFilePath = CurrentRun.ResultCsvFilePath;
    const bool bBuildContinuationSnapshot =
        CurrentRun.SourceScenarioId.IsValid();
    FTGSimulationScenario InputSnapshot =
        MoveTemp(CurrentRun.InputSnapshot);
    const TWeakObjectPtr<UTGSimulationRunSubsystem> WeakThis(this);

    Async(
        EAsyncExecution::ThreadPool,
        [WeakThis,
         FinalizationGeneration,
         ExitCode,
         ResultCsvFilePath,
         bBuildContinuationSnapshot,
         InputSnapshot = MoveTemp(InputSnapshot)]() mutable
        {
            FRunFinalizationResult Result;
            Result.InputSnapshot = MoveTemp(InputSnapshot);
            Result.bCsvValid =
                UTGSimulationRunSubsystem::ValidateCompletedCsvFile(
                    ResultCsvFilePath,
                    Result.CsvError);
            if (Result.bCsvValid && bBuildContinuationSnapshot)
            {
                BuildFinalStateSnapshot(
                    ResultCsvFilePath,
                    Result.InputSnapshot,
                    Result.FinalState,
                    Result.StoredSampleCount,
                    Result.StartEphemerisTime,
                    Result.FinalEphemerisTime,
                    Result.SnapshotError);
            }

            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis,
                 FinalizationGeneration,
                 ExitCode,
                 Result = MoveTemp(Result)]() mutable
                {
                    if (UTGSimulationRunSubsystem* Subsystem =
                            WeakThis.Get())
                    {
                        Subsystem->HandleSuccessfulRunnerFinalization(
                            FinalizationGeneration,
                            ExitCode,
                            Result.bCsvValid,
                            Result.CsvError,
                            MoveTemp(Result.InputSnapshot),
                            Result.FinalState,
                            Result.StoredSampleCount,
                            Result.StartEphemerisTime,
                            Result.FinalEphemerisTime,
                            Result.SnapshotError);
                    }
                });
        });
}

void UTGSimulationRunSubsystem::HandleSuccessfulRunnerFinalization(
    const uint64 FinalizationGeneration,
    const int32 ExitCode,
    const bool bCsvValid,
    const FString& CsvError,
    FTGSimulationScenario&& InputSnapshot,
    const FTGSimulationFinalState& FinalState,
    const int64 StoredSampleCount,
    const double StartEphemerisTime,
    const double FinalEphemerisTime,
    const FString& SnapshotError)
{
    if (FinalizationGeneration != ResultFinalizationGeneration ||
        !bResultFinalizationInFlight)
    {
        return;
    }

    bResultFinalizationInFlight = false;
    CurrentRun.InputSnapshot = MoveTemp(InputSnapshot);
    if (!bCsvValid)
    {
        FinishUnsuccessfulRun(
            ETGSimulationRunState::Failed,
            TEXT("Result validation"),
            FText::FromString(CsvError),
            ExitCode);
        return;
    }

    CurrentRun.CompletedUtc = FDateTime::UtcNow();
    PersistCompletedRunMetadata(
        FinalState,
        StoredSampleCount,
        StartEphemerisTime,
        FinalEphemerisTime,
        SnapshotError);
    SaveRunnerLog();
    LatestCompletedRun = CurrentRun;
    bHasLatestCompletedRun = true;
    PublishProgress(100.0f, FText::FromString(
        TEXT("Simulation complete")));
    SetRunState(ETGSimulationRunState::Succeeded);
    OnRunCompleted.Broadcast(LatestCompletedRun);

    if (bAutoOpenVisualizationOnSuccess)
    {
        FText TravelError;
        if (!OpenLatestRunVisualization(TravelError))
        {
            FinishUnsuccessfulRun(
                ETGSimulationRunState::Failed,
                TEXT("Visualization travel"),
                TravelError,
                ExitCode);
        }
    }
}

void UTGSimulationRunSubsystem::FinishUnsuccessfulRun(
    const ETGSimulationRunState TerminalState,
    const FName Stage,
    const FText& Message,
    const int32 ExitCode)
{
    LastRunError.State = TerminalState;
    LastRunError.Stage = Stage;
    LastRunError.Message = Message;
    LastRunError.TechnicalDetails = RunnerLog;
    LastRunError.ProcessExitCode = ExitCode;
    SetRunState(TerminalState);
    OnRunFailed.Broadcast(LastRunError);
}

void UTGSimulationRunSubsystem::CloseRunnerResources()
{
    if (RunnerProcess.IsValid())
    {
        FPlatformProcess::CloseProc(RunnerProcess);
        RunnerProcess.Reset();
    }
    if (RunnerOutputReadPipe != nullptr ||
        RunnerOutputWritePipe != nullptr)
    {
        FPlatformProcess::ClosePipe(
            RunnerOutputReadPipe, RunnerOutputWritePipe);
    }
    if (RunnerInputReadPipe != nullptr ||
        RunnerInputWritePipe != nullptr)
    {
        FPlatformProcess::ClosePipe(
            RunnerInputReadPipe, RunnerInputWritePipe);
    }
    RunnerOutputReadPipe = nullptr;
    RunnerOutputWritePipe = nullptr;
    RunnerInputReadPipe = nullptr;
    RunnerInputWritePipe = nullptr;
    RunnerProcessId = 0;
}

void UTGSimulationRunSubsystem::RemoveIncompleteResultFiles() const
{
    const FString Paths[] = {
        CurrentRun.ResultCsvFilePath,
        CurrentRun.ResultCsvFilePath + TEXT(".tmp"),
        CurrentRun.SummaryFilePath,
        CurrentRun.SummaryFilePath + TEXT(".tmp")};
    for (const FString& Path : Paths)
    {
        IFileManager::Get().Delete(*Path, false, true, true);
    }
}

void UTGSimulationRunSubsystem::SaveRunnerLog() const
{
    if (!CurrentRun.LogFilePath.IsEmpty())
    {
        FFileHelper::SaveStringToFile(
            RunnerLog,
            *CurrentRun.LogFilePath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

bool UTGSimulationRunSubsystem::ValidateCompletedCsvFile(
    const FString& FilePath,
    FString& OutError)
{
    const int64 FileSize = IFileManager::Get().FileSize(
        *FilePath);
    if (FileSize <= 0)
    {
        OutError = FString::Printf(
            TEXT("PHAROS Scenario Runner exited successfully but did not produce a valid CSV: %s"),
            *FilePath);
        return false;
    }

    TUniquePtr<FArchive> Reader(
        IFileManager::Get().CreateFileReader(
            *FilePath));
    if (!Reader)
    {
        OutError = FString::Printf(
            TEXT("The generated CSV could not be opened: %s"),
            *FilePath);
        return false;
    }

    // Wide result tables can have headers much larger than 4 KiB. Scan the
    // beginning incrementally until both the header and first data row have
    // been observed, without loading the complete result file into memory.
    constexpr int64 MaxValidationBytes = 8LL * 1024LL * 1024LL;
    constexpr int32 ValidationChunkBytes = 64 * 1024;
    const int64 BytesToInspect = FMath::Min(FileSize, MaxValidationBytes);
    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(ValidationChunkBytes);

    bool bHeaderContainsComma = false;
    bool bHeaderEnded = false;
    bool bDataRowContainsContent = false;
    int64 BytesInspected = 0;
    while (BytesInspected < BytesToInspect)
    {
        const int32 ChunkBytes = static_cast<int32>(FMath::Min<int64>(
            ValidationChunkBytes,
            BytesToInspect - BytesInspected));
        Reader->Serialize(Buffer.GetData(), ChunkBytes);
        if (Reader->IsError())
        {
            OutError = FString::Printf(
                TEXT("The generated CSV could not be read: %s"),
                *FilePath);
            return false;
        }

        for (int32 Index = 0; Index < ChunkBytes; ++Index)
        {
            const uint8 Character = Buffer[Index];
            if (!bHeaderEnded)
            {
                bHeaderContainsComma =
                    bHeaderContainsComma || Character == ',';
                if (Character == '\n')
                {
                    bHeaderEnded = true;
                }
                continue;
            }

            if (Character == '\n')
            {
                if (bDataRowContainsContent && bHeaderContainsComma)
                {
                    return true;
                }
                continue;
            }
            if (Character != '\r' && Character != ' ' && Character != '\t')
            {
                bDataRowContainsContent = true;
            }
        }
        BytesInspected += ChunkBytes;
    }

    // Accept a final data row without a terminating newline.
    if (bHeaderEnded && bHeaderContainsComma && bDataRowContainsContent)
    {
        return true;
    }

    OutError = bHeaderEnded
        ? TEXT("The generated result file contains a CSV header but no data row.")
        : TEXT("The generated result file has no complete CSV header within the first 8 MiB.");
    return false;
}

void UTGSimulationRunSubsystem::PersistCompletedRunMetadata(
    const FTGSimulationFinalState& FinalState,
    const int64 StoredSampleCount,
    const double StartEphemerisTime,
    const double FinalEphemerisTime,
    const FString& SnapshotError)
{
    // Only an exact saved draft owns a library result. A run made after
    // unsaved edits remains available in this session, but must not replace
    // the result associated with the older saved scenario.
    if (!CurrentRun.SourceScenarioId.IsValid())
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UTGSimulationSubsystem* ScenarioSubsystem = GameInstance != nullptr
        ? GameInstance->GetSubsystem<UTGSimulationSubsystem>()
        : nullptr;
    if (ScenarioSubsystem == nullptr)
    {
        RunnerLog += TEXT(
            "Completed-run metadata was not saved because the scenario library is unavailable.\n");
        return;
    }

    FTGSavedSimulationRun SavedRun;
    SavedRun.RunId = CurrentRun.RunId;
    SavedRun.SourceScenarioId = CurrentRun.SourceScenarioId;
    SavedRun.RunName = CurrentRun.ScenarioName;
    SavedRun.CreatedUtc = CurrentRun.CompletedUtc;
    SavedRun.Status = ETGSimulationRunStatus::Completed;
    SavedRun.InputSnapshot = CurrentRun.InputSnapshot;
    SavedRun.ResultFormatVersion = 1;

    FString StoredResultPath = CurrentRun.ResultCsvFilePath;
    const FString SavedDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectSavedDir());
    if (!FPaths::MakePathRelativeTo(
            StoredResultPath,
            *SavedDirectory))
    {
        StoredResultPath = CurrentRun.ResultCsvFilePath;
    }
    FPaths::NormalizeFilename(StoredResultPath);
    SavedRun.ResultRelativePath = MoveTemp(StoredResultPath);

    SavedRun.FinalState = FinalState;
    SavedRun.StoredSampleCount = StoredSampleCount;
    SavedRun.StartEphemerisTimeTdbSeconds = StartEphemerisTime;
    SavedRun.FinalEphemerisTimeTdbSeconds = FinalEphemerisTime;
    if (!SnapshotError.IsEmpty())
    {
        SavedRun.FinalState = FTGSimulationFinalState{};
        SavedRun.StatusMessage = FString::Printf(
            TEXT("Visualization is available, but continuation is disabled: %s"),
            *SnapshotError);
        RunnerLog += SavedRun.StatusMessage + TEXT("\n");
    }

    if (!ScenarioSubsystem->RecordCompletedSimulationRun(SavedRun))
    {
        RunnerLog += TEXT(
            "The simulation completed, but its library metadata could not be saved.\n");
    }
}

FText UTGSimulationRunSubsystem::DescribeRunnerExitCode(
    const int32 ExitCode)
{
    switch (ExitCode)
    {
    case 2:
        return FText::FromString(TEXT("PHAROS Scenario Runner rejected its command line."));
    case 3:
        return FText::FromString(TEXT("The generated .tgscn file could not be parsed."));
    case 4:
        return FText::FromString(TEXT("The selected controller DLL could not be loaded."));
    case 5:
        return FText::FromString(TEXT("The required SPICE kernels could not be loaded."));
    case 6:
        return FText::FromString(TEXT("The scenario could not compile to a valid SimulationRequest."));
    case 7:
        return FText::FromString(TEXT("The backend could not create its output directory."));
    case 8:
        return FText::FromString(TEXT("The backend could not write its result files."));
    case 9:
        return FText::FromString(TEXT("The PHAROS simulation engine returned a failed result."));
    default:
        return FText::FromString(FString::Printf(
            TEXT("PHAROS Scenario Runner exited unexpectedly with code %d."),
            ExitCode));
    }
}
