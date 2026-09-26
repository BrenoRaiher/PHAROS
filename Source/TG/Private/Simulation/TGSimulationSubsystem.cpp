// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/TGSimulationSubsystem.h"

#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Simulation/TGGravityEditingLibrary.h"
#include "Simulation/TGSimulationAdapter.h"
#include "UI/Configuration/Environment/TGSolarRadiationPressureEditingLibrary.h"

namespace
{
    constexpr int32 MaximumLibraryBackupCount = 10;

    FString GetLibrarySaveFilePath(const FString& SaveSlotName)
    {
        return FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("SaveGames"),
            SaveSlotName + TEXT(".sav"));
    }

    FString GetLibraryBackupDirectory(const bool bRecoveryCopy)
    {
        return FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("SaveGames"),
            bRecoveryCopy ? TEXT("Recovery") : TEXT("Backups"));
    }

    void PruneOldLibraryBackups(const FString& BackupDirectory)
    {
        IFileManager& FileManager = IFileManager::Get();
        TArray<FString> BackupNames;
        FileManager.FindFiles(
            BackupNames,
            *FPaths::Combine(BackupDirectory, TEXT("*.sav")),
            true,
            false);

        BackupNames.Sort(
            [&FileManager, &BackupDirectory](
                const FString& Left,
                const FString& Right)
            {
                return FileManager.GetTimeStamp(
                           *FPaths::Combine(BackupDirectory, Left)) >
                       FileManager.GetTimeStamp(
                           *FPaths::Combine(BackupDirectory, Right));
            });

        for (int32 Index = MaximumLibraryBackupCount;
             Index < BackupNames.Num();
             ++Index)
        {
            const FString BackupPath = FPaths::Combine(
                BackupDirectory,
                BackupNames[Index]);
            if (!FileManager.Delete(*BackupPath, false, true, true))
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("PHAROS could not prune the old scenario-library backup '%s'."),
                    *BackupPath);
            }
        }
    }

    bool PreserveExistingLibraryFile(
        const FString& SaveSlotName,
        const FString& Reason,
        const bool bRecoveryCopy,
        FString& OutBackupPath)
    {
        OutBackupPath.Reset();

        IFileManager& FileManager = IFileManager::Get();
        const FString SourcePath =
            GetLibrarySaveFilePath(SaveSlotName);
        if (!FileManager.FileExists(*SourcePath))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("PHAROS found an existing scenario-library slot but could not locate its save file at '%s'."),
                *SourcePath);
            return false;
        }

        const FString BackupDirectory =
            GetLibraryBackupDirectory(bRecoveryCopy);
        if (!FileManager.MakeDirectory(*BackupDirectory, true))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("PHAROS could not create the scenario-library backup directory '%s'."),
                *BackupDirectory);
            return false;
        }

        const FDateTime NowUtc = FDateTime::UtcNow();
        const FString Timestamp = FString::Printf(
            TEXT("%s_%03d"),
            *NowUtc.ToString(TEXT("%Y%m%dT%H%M%S")),
            NowUtc.GetMillisecond());
        const FString SafeReason = FPaths::MakeValidFileName(Reason, TEXT('_'));
        const FString UniqueSuffix =
            FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
        OutBackupPath = FPaths::Combine(
            BackupDirectory,
            FString::Printf(
                TEXT("%s_%s_%s_%s.sav"),
                *SaveSlotName,
                *SafeReason,
                *Timestamp,
                *UniqueSuffix));

        const uint32 CopyResult = FileManager.Copy(
            *OutBackupPath,
            *SourcePath,
            false,
            true);
        if (CopyResult != COPY_OK)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("PHAROS refused to replace the scenario library because its recovery backup could not be written to '%s' (copy result %u)."),
                *OutBackupPath,
                CopyResult);
            OutBackupPath.Reset();
            return false;
        }

        if (!bRecoveryCopy)
        {
            PruneOldLibraryBackups(BackupDirectory);
        }
        return true;
    }

    bool IsSupportedLibraryFormat(
        const UTGSimulationSaveGame& Library)
    {
        return Library.LibraryFormatVersion >=
                   UTGSimulationSaveGame::
                       OldestSupportedLibraryFormatVersion &&
               Library.LibraryFormatVersion <=
                   UTGSimulationSaveGame::
                       CurrentLibraryFormatVersion;
    }

    /* New-draft initialization only. Generic set/load/save paths must
     * preserve current-schema authored data exactly so Review can report
     * malformed gravity catalogs.
     */
    void InitializeScenarioGravity(
        FTGSimulationScenario& Scenario)
    {
        Scenario.CelestialBodies =
            UTGGravityEditingLibrary::
                MakeDefaultCelestialBodyConfigs();
    }

    bool MigrateStoredProjectPath(
        FString& Path,
        const FString& SourceProjectRoot,
        const FString& CurrentProjectRoot)
    {
        if (Path.IsEmpty() || FPaths::IsRelative(Path))
        {
            return false;
        }

        FString NormalizedPath =
            FPaths::ConvertRelativePathToFull(Path);
        FPaths::NormalizeFilename(NormalizedPath);

        FString SourcePrefix = SourceProjectRoot;
        FPaths::NormalizeDirectoryName(SourcePrefix);
        SourcePrefix += TEXT("/");
        if (!NormalizedPath.StartsWith(
                SourcePrefix,
                ESearchCase::IgnoreCase))
        {
            return false;
        }

        const FString RelativePath =
            NormalizedPath.Mid(SourcePrefix.Len());
        FString CandidatePath =
            FPaths::Combine(CurrentProjectRoot, RelativePath);
        FPaths::NormalizeFilename(CandidatePath);

        if (!IFileManager::Get().FileExists(*CandidatePath) &&
            !IFileManager::Get().DirectoryExists(*CandidatePath))
        {
            return false;
        }

        Path = MoveTemp(CandidatePath);
        return true;
    }

    bool MigrateScenarioProjectPaths(
        FTGSimulationScenario& Scenario,
        const FString& SourceProjectRoot,
        const FString& CurrentProjectRoot)
    {
        bool bChanged = false;

        for (FTGComponentConfig& Component : Scenario.Components)
        {
            bChanged |= MigrateStoredProjectPath(
                Component.Visual.StlFilePath,
                SourceProjectRoot,
                CurrentProjectRoot);
            bChanged |= MigrateStoredProjectPath(
                Component.Visual.BaseColorTextureFilePath,
                SourceProjectRoot,
                CurrentProjectRoot);
            bChanged |= MigrateStoredProjectPath(
                Component.Visual.NormalTextureFilePath,
                SourceProjectRoot,
                CurrentProjectRoot);
            bChanged |= MigrateStoredProjectPath(
                Component.Visual.RoughnessTextureFilePath,
                SourceProjectRoot,
                CurrentProjectRoot);
            bChanged |= MigrateStoredProjectPath(
                Component.Visual.MetallicTextureFilePath,
                SourceProjectRoot,
                CurrentProjectRoot);
        }

        for (FTGThrusterConfig& Thruster : Scenario.Thrusters)
        {
            bChanged |= MigrateStoredProjectPath(
                Thruster.PrescribedThrust.CsvFilePath,
                SourceProjectRoot,
                CurrentProjectRoot);
            bChanged |= MigrateStoredProjectPath(
                Thruster.PrescribedSpecificImpulse.CsvFilePath,
                SourceProjectRoot,
                CurrentProjectRoot);
        }

        bChanged |= MigrateStoredProjectPath(
            Scenario.Control.StandaloneControllerDllFilePath,
            SourceProjectRoot,
            CurrentProjectRoot);

        for (FTGCelestialBodyConfig& Body : Scenario.CelestialBodies)
        {
            bChanged |= MigrateStoredProjectPath(
                Body.HarmonicModelCsvFilePath,
                SourceProjectRoot,
                CurrentProjectRoot);
        }

        bChanged |= MigrateStoredProjectPath(
            Scenario.Atmosphere.GeneralProfileCsvPath,
            SourceProjectRoot,
            CurrentProjectRoot);
        bChanged |= MigrateStoredProjectPath(
            Scenario.Atmosphere.ChpCoefficientCsvPath,
            SourceProjectRoot,
            CurrentProjectRoot);
        bChanged |= MigrateStoredProjectPath(
            Scenario.Atmosphere.ChpMolecularProfileCsvPath,
            SourceProjectRoot,
            CurrentProjectRoot);
        bChanged |= MigrateStoredProjectPath(
            Scenario.Aerodynamics.Database.CsvFilePath,
            SourceProjectRoot,
            CurrentProjectRoot);

        return bChanged;
    }

    bool MigrateLibraryProjectPaths(
        UTGSimulationSaveGame& Library)
    {
        FString CurrentProjectRoot =
            FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
        FPaths::NormalizeDirectoryName(CurrentProjectRoot);
        if (!FPaths::GetCleanFilename(CurrentProjectRoot).Equals(
                TEXT("PHAROS"),
                ESearchCase::IgnoreCase))
        {
            return false;
        }

        FString SourceProjectRoot = FPaths::Combine(
            FPaths::GetPath(CurrentProjectRoot),
            TEXT("TG"));
        FPaths::NormalizeDirectoryName(SourceProjectRoot);

        bool bChanged = false;
        for (FTGSavedScenarioRecord& Record : Library.SavedScenarios)
        {
            bChanged |= MigrateScenarioProjectPaths(
                Record.Scenario,
                SourceProjectRoot,
                CurrentProjectRoot);
        }
        for (FTGSavedSimulationRun& Run : Library.SavedRuns)
        {
            bChanged |= MigrateScenarioProjectPaths(
                Run.InputSnapshot,
                SourceProjectRoot,
                CurrentProjectRoot);
        }

        return bChanged;
    }

    bool DiscardDisabledSrpGeometryCache(
        FTGSimulationScenario& Scenario)
    {
        if (Scenario.SolarRadiationPressure.bEnabled)
        {
            return false;
        }

        bool bChanged =
            !Scenario.SolarRadiationPressure.OpticalFacets.IsEmpty();
        for (const FTGComponentConfig& Component : Scenario.Components)
        {
            const FTGComponentSrpConfig& Config =
                Component.SolarRadiationPressure;
            bChanged |=
                Config.GeneratedTriangleCount != 0 ||
                !Config.GeneratedGeometrySignature.IsEmpty() ||
                Config.bProxyGenerationRequired != Config.bIncludedInProxy;
        }

        UTGSolarRadiationPressureEditingLibrary::
            NormalizeSolarRadiationPressureScenario(Scenario);
        return bChanged;
    }

    bool DiscardDisabledSrpGeometryCaches(
        UTGSimulationSaveGame& Library)
    {
        bool bChanged = false;
        for (FTGSavedScenarioRecord& Record : Library.SavedScenarios)
        {
            bChanged |= DiscardDisabledSrpGeometryCache(Record.Scenario);
        }
        for (FTGSavedSimulationRun& Run : Library.SavedRuns)
        {
            bChanged |= DiscardDisabledSrpGeometryCache(Run.InputSnapshot);
        }
        return bChanged;
    }
}

const FString UTGSimulationSubsystem::SaveSlotName =
    TEXT("TGSimulationLibrary");

void UTGSimulationSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    LoadLibrary();
    CreateNewScenarioDraft();
}

void UTGSimulationSubsystem::Deinitialize()
{
    ++AsyncSaveGeneration;
    PendingSaveCompletions.Reset();

    if (SimulationLibrary != nullptr && !bScenarioSaveInProgress)
    {
        SaveLibrary();
    }

    bScenarioSaveInProgress = false;

    SimulationLibrary = nullptr;

    Super::Deinitialize();
}

// ============================================================================
// Current scenario draft
// ============================================================================

void UTGSimulationSubsystem::CreateNewScenarioDraft()
{
    CurrentScenarioDraft = FTGSimulationScenario{};

    InitializeScenarioGravity(CurrentScenarioDraft);

    CurrentScenarioId = FGuid{};
    bHasCurrentScenarioDraft = true;
    bCurrentScenarioDirty = true;
    ++CurrentScenarioDraftRevision;
    bHasAcceptedScenarioReview = false;
}

void UTGSimulationSubsystem::SetCurrentScenarioDraft(
    const FTGSimulationScenario& Scenario)
{
    CurrentScenarioDraft = Scenario;
    DiscardDisabledSrpGeometryCache(CurrentScenarioDraft);

    bHasCurrentScenarioDraft = true;
    bCurrentScenarioDirty = true;
    ++CurrentScenarioDraftRevision;
    bHasAcceptedScenarioReview = false;
}

void UTGSimulationSubsystem::SetImportedScenarioDraft(
    const FTGSimulationScenario& Scenario)
{
    CurrentScenarioDraft = Scenario;
    DiscardDisabledSrpGeometryCache(CurrentScenarioDraft);
    CurrentScenarioId = FGuid{};
    bHasCurrentScenarioDraft = true;
    bCurrentScenarioDirty = true;
    ++CurrentScenarioDraftRevision;
    bHasAcceptedScenarioReview = false;
}

FTGSimulationScenario
UTGSimulationSubsystem::GetCurrentScenarioDraft() const
{
    return CurrentScenarioDraft;
}

bool UTGSimulationSubsystem::HasCurrentScenarioDraft() const
{
    return bHasCurrentScenarioDraft;
}

void UTGSimulationSubsystem::ClearCurrentScenarioDraft()
{
    CurrentScenarioDraft = FTGSimulationScenario{};
    CurrentScenarioId = FGuid{};
    bHasCurrentScenarioDraft = false;
    bCurrentScenarioDirty = false;
    ++CurrentScenarioDraftRevision;
    bHasAcceptedScenarioReview = false;
}

void UTGSimulationSubsystem::MarkCurrentScenarioDirty()
{
    if (bHasCurrentScenarioDraft)
    {
        bCurrentScenarioDirty = true;
    }
}

bool UTGSimulationSubsystem::IsCurrentScenarioDirty() const
{
    return bCurrentScenarioDirty;
}

FGuid UTGSimulationSubsystem::GetCurrentScenarioId() const
{
    return CurrentScenarioId;
}

bool UTGSimulationSubsystem::IsCurrentScenarioSaved() const
{
    return CurrentScenarioId.IsValid();
}

uint64 UTGSimulationSubsystem::
    GetCurrentScenarioDraftRevision() const
{
    return CurrentScenarioDraftRevision;
}

bool UTGSimulationSubsystem::AcceptCurrentScenarioReview(
    uint64 ValidatedRevision,
    FText& OutError)
{
    OutError = FText::GetEmpty();

    if (!bHasCurrentScenarioDraft)
    {
        OutError = FText::FromString(
            TEXT("There is no current scenario draft to accept."));
        return false;
    }

    if (ValidatedRevision != CurrentScenarioDraftRevision)
    {
        OutError = FText::FromString(
            TEXT("The scenario changed after validation. Validate it again."));
        return false;
    }

    AcceptedScenarioReviewRevision = ValidatedRevision;
    bHasAcceptedScenarioReview = true;
    return true;
}

bool UTGSimulationSubsystem::IsCurrentScenarioReviewAccepted() const
{
    return bHasCurrentScenarioDraft
        && bHasAcceptedScenarioReview
        && AcceptedScenarioReviewRevision == CurrentScenarioDraftRevision;
}

void UTGSimulationSubsystem::InvalidateCurrentScenarioReview()
{
    AcceptedScenarioReviewRevision = 0;
    bHasAcceptedScenarioReview = false;
}

bool UTGSimulationSubsystem::CanSimulateCurrentScenario(
    FText& OutReason) const
{
    OutReason = FText::GetEmpty();

    if (!bHasCurrentScenarioDraft)
    {
        OutReason = FText::FromString(
            TEXT("Create or load a scenario before starting the simulation."));
        return false;
    }

    if (!IsCurrentScenarioReviewAccepted())
    {
        OutReason = FText::FromString(
            TEXT("Validate and accept the current scenario in the Review panel before simulation."));
        return false;
    }

    return true;
}

bool UTGSimulationSubsystem::
    SetCurrentScenarioMaximumWallClockRuntimeSeconds(
        const double MaximumSeconds,
        FText& OutError)
{
    OutError = FText::GetEmpty();

    if (!bHasCurrentScenarioDraft)
    {
        OutError = FText::FromString(
            TEXT("Create or load a scenario before setting its runtime limit."));
        return false;
    }

    if (!FMath::IsFinite(MaximumSeconds) || MaximumSeconds <= 0.0)
    {
        OutError = FText::FromString(
            TEXT("Maximum backend runtime must be a positive finite number of seconds."));
        return false;
    }

    double& StoredMaximum = CurrentScenarioDraft.ScenarioAndSolver.
        MaximumWallClockRuntimeSeconds;
    if (!FMath::IsNearlyEqual(StoredMaximum, MaximumSeconds))
    {
        StoredMaximum = MaximumSeconds;
        bCurrentScenarioDirty = true;
    }
    return true;
}

double UTGSimulationSubsystem::
    GetCurrentScenarioMaximumWallClockRuntimeSeconds() const
{
    return bHasCurrentScenarioDraft
        ? CurrentScenarioDraft.ScenarioAndSolver.
            MaximumWallClockRuntimeSeconds
        : 0.0;
}

// ============================================================================
// Saved scenarios
// ============================================================================

bool UTGSimulationSubsystem::SaveCurrentScenarioDraft(
    FGuid& OutScenarioId)
{
    OutScenarioId = FGuid{};

    uint64 DraftRevision = 0;
    if (!PrepareCurrentScenarioRecordForSave(
            OutScenarioId,
            DraftRevision))
    {
        return false;
    }

    if (!SaveLibrary())
    {
        return false;
    }

    if (CurrentScenarioId == OutScenarioId &&
        CurrentScenarioDraftRevision == DraftRevision)
    {
        bCurrentScenarioDirty = false;
    }
    return true;
}

bool UTGSimulationSubsystem::SaveCurrentScenarioDraftAsync()
{
    return BeginSaveCurrentScenarioDraft({});
}

bool UTGSimulationSubsystem::IsScenarioSaveInProgress() const
{
    return bScenarioSaveInProgress;
}

bool UTGSimulationSubsystem::BeginSaveCurrentScenarioDraft(
    TFunction<void(bool, const FGuid&)> Completion)
{
    if (bScenarioSaveInProgress)
    {
        return false;
    }

    FGuid ScenarioId;
    uint64 DraftRevision = 0;
    if (!PrepareCurrentScenarioRecordForSave(
            ScenarioId,
            DraftRevision))
    {
        return false;
    }

    if (Completion)
    {
        PendingSaveCompletions.Add(MoveTemp(Completion));
    }

    bScenarioSaveInProgress = true;
    const uint64 SaveGeneration = ++AsyncSaveGeneration;
    SimulationLibrary->LibraryFormatVersion =
        UTGSimulationSaveGame::CurrentLibraryFormatVersion;
    DiscardDisabledSrpGeometryCaches(*SimulationLibrary);

    const bool bNeedsBackup = UGameplayStatics::DoesSaveGameExist(
        SaveSlotName,
        SaveUserIndex);
    if (!bNeedsBackup)
    {
        BeginAsyncLibraryWrite(
            ScenarioId,
            DraftRevision,
            FString{});
        return true;
    }

    const TWeakObjectPtr<UTGSimulationSubsystem> WeakThis(this);
    Async(
        EAsyncExecution::ThreadPool,
        [WeakThis, ScenarioId, DraftRevision, SaveGeneration]()
        {
            FString BackupPath;
            const bool bBackupSucceeded = PreserveExistingLibraryFile(
                SaveSlotName,
                TEXT("before_save"),
                false,
                BackupPath);

            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis,
                 ScenarioId,
                 DraftRevision,
                 SaveGeneration,
                 bBackupSucceeded,
                 BackupPath = MoveTemp(BackupPath)]()
                {
                    UTGSimulationSubsystem* Subsystem = WeakThis.Get();
                    if (Subsystem == nullptr ||
                        Subsystem->AsyncSaveGeneration != SaveGeneration)
                    {
                        return;
                    }

                    if (!bBackupSucceeded)
                    {
                        Subsystem->FinishAsyncScenarioSave(
                            false,
                            ScenarioId,
                            DraftRevision);
                        return;
                    }

                    Subsystem->BeginAsyncLibraryWrite(
                        ScenarioId,
                        DraftRevision,
                        BackupPath);
                });
        });
    return true;
}

bool UTGSimulationSubsystem::PrepareCurrentScenarioRecordForSave(
    FGuid& OutScenarioId,
    uint64& OutDraftRevision)
{
    OutScenarioId = FGuid{};
    OutDraftRevision = CurrentScenarioDraftRevision;
    if (!bHasCurrentScenarioDraft)
    {
        return false;
    }
    if (SimulationLibrary == nullptr && !LoadLibrary())
    {
        return false;
    }

    DiscardDisabledSrpGeometryCache(CurrentScenarioDraft);
    const FDateTime NowUtc = FDateTime::UtcNow();
    if (!CurrentScenarioId.IsValid())
    {
        CurrentScenarioId = FGuid::NewGuid();
    }

    const int32 ExistingIndex =
        SimulationLibrary->SavedScenarios.IndexOfByPredicate(
            [this](const FTGSavedScenarioRecord& Record)
            {
                return Record.ScenarioId == CurrentScenarioId;
            });
    if (ExistingIndex != INDEX_NONE)
    {
        FTGSavedScenarioRecord& ExistingRecord =
            SimulationLibrary->SavedScenarios[ExistingIndex];
        ExistingRecord.LastModifiedUtc = NowUtc;
        ExistingRecord.Scenario = CurrentScenarioDraft;
    }
    else
    {
        FTGSavedScenarioRecord NewRecord;
        NewRecord.ScenarioId = CurrentScenarioId;
        NewRecord.CreatedUtc = NowUtc;
        NewRecord.LastModifiedUtc = NowUtc;
        NewRecord.ScenarioFormatVersion = 1;
        NewRecord.Scenario = CurrentScenarioDraft;
        SimulationLibrary->SavedScenarios.Add(MoveTemp(NewRecord));
    }

    OutScenarioId = CurrentScenarioId;
    OutDraftRevision = CurrentScenarioDraftRevision;
    return true;
}

void UTGSimulationSubsystem::BeginAsyncLibraryWrite(
    const FGuid& ScenarioId,
    const uint64 DraftRevision,
    const FString& BackupPath)
{
    const uint64 SaveGeneration = AsyncSaveGeneration;
    UGameplayStatics::AsyncSaveGameToSlot(
        SimulationLibrary,
        SaveSlotName,
        SaveUserIndex,
        FAsyncSaveGameToSlotDelegate::CreateWeakLambda(
            this,
            [this,
             ScenarioId,
             DraftRevision,
             SaveGeneration,
             BackupPath](
                const FString& SlotName,
                const int32 UserIndex,
                const bool bSucceeded)
            {
                (void)SlotName;
                (void)UserIndex;
                if (AsyncSaveGeneration != SaveGeneration)
                {
                    return;
                }

                if (!bSucceeded && !BackupPath.IsEmpty())
                {
                    const FString SavePath =
                        GetLibrarySaveFilePath(SaveSlotName);
                    const TWeakObjectPtr<UTGSimulationSubsystem>
                        WeakThis(this);
                    Async(
                        EAsyncExecution::ThreadPool,
                        [WeakThis,
                         ScenarioId,
                         DraftRevision,
                         SaveGeneration,
                         SavePath,
                         BackupPath]()
                        {
                            const uint32 RestoreResult =
                                IFileManager::Get().Copy(
                                    *SavePath,
                                    *BackupPath,
                                    true,
                                    true);
                            if (RestoreResult != COPY_OK)
                            {
                                UE_LOG(
                                    LogTemp,
                                    Error,
                                    TEXT("PHAROS could not restore the prior scenario library from '%s' (copy result %u)."),
                                    *BackupPath,
                                    RestoreResult);
                            }

                            AsyncTask(
                                ENamedThreads::GameThread,
                                [WeakThis,
                                 ScenarioId,
                                 DraftRevision,
                                 SaveGeneration]()
                                {
                                    UTGSimulationSubsystem* Subsystem =
                                        WeakThis.Get();
                                    if (Subsystem != nullptr &&
                                        Subsystem->AsyncSaveGeneration ==
                                            SaveGeneration)
                                    {
                                        Subsystem->FinishAsyncScenarioSave(
                                            false,
                                            ScenarioId,
                                            DraftRevision);
                                    }
                                });
                        });
                    return;
                }

                FinishAsyncScenarioSave(
                    bSucceeded,
                    ScenarioId,
                    DraftRevision);
            }));
}

void UTGSimulationSubsystem::FinishAsyncScenarioSave(
    const bool bSucceeded,
    const FGuid& ScenarioId,
    const uint64 DraftRevision)
{
    if (bSucceeded &&
        CurrentScenarioId == ScenarioId &&
        CurrentScenarioDraftRevision == DraftRevision)
    {
        bCurrentScenarioDirty = false;
    }

    if (!bSucceeded)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("PHAROS could not save the scenario library."));
    }

    bScenarioSaveInProgress = false;
    OnCurrentScenarioSaveCompleted.Broadcast(bSucceeded, ScenarioId);

    TArray<TFunction<void(bool, const FGuid&)>> Completions =
        MoveTemp(PendingSaveCompletions);
    PendingSaveCompletions.Reset();
    for (TFunction<void(bool, const FGuid&)>& Completion : Completions)
    {
        if (Completion)
        {
            Completion(bSucceeded, ScenarioId);
        }
    }
}

bool UTGSimulationSubsystem::LoadScenarioDraft(
    const FGuid& ScenarioId)
{
    if (!ScenarioId.IsValid())
    {
        return false;
    }

    if (SimulationLibrary == nullptr && !LoadLibrary())
    {
        return false;
    }

    const FTGSavedScenarioRecord* FoundRecord =
        SimulationLibrary->SavedScenarios.FindByPredicate(
            [&ScenarioId](
                const FTGSavedScenarioRecord& Record)
            {
                return Record.ScenarioId == ScenarioId;
            });

    if (FoundRecord == nullptr)
    {
        return false;
    }

    CurrentScenarioDraft = FoundRecord->Scenario;
    DiscardDisabledSrpGeometryCache(CurrentScenarioDraft);

    CurrentScenarioId = FoundRecord->ScenarioId;
    bHasCurrentScenarioDraft = true;
    bCurrentScenarioDirty = false;
    ++CurrentScenarioDraftRevision;
    bHasAcceptedScenarioReview = false;

    if (!MarkScenarioRecentlyUsed(ScenarioId))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("PHAROS: Loaded scenario %s, but its recent-use timestamp could not be saved."),
            *ScenarioId.ToString());
    }

    return true;
}

bool UTGSimulationSubsystem::DeleteSavedScenario(
    const FGuid& ScenarioId)
{
    if (!ScenarioId.IsValid() || SimulationLibrary == nullptr)
    {
        return false;
    }

    const int32 RemovedCount =
        SimulationLibrary->SavedScenarios.RemoveAll(
            [&ScenarioId](
                const FTGSavedScenarioRecord& Record)
            {
                return Record.ScenarioId == ScenarioId;
            });

    if (RemovedCount == 0)
    {
        return false;
    }

    if (CurrentScenarioId == ScenarioId)
    {
        CurrentScenarioId = FGuid{};
        bCurrentScenarioDirty =
            bHasCurrentScenarioDraft;
    }

    return SaveLibrary();
}

bool UTGSimulationSubsystem::RenameSavedScenario(
    const FGuid& ScenarioId,
    const FString& NewName,
    FText& OutError)
{
    OutError = FText::GetEmpty();

    const FString TrimmedName = NewName.TrimStartAndEnd();
    if (!ScenarioId.IsValid())
    {
        OutError = FText::FromString(TEXT(
            "The selected scenario no longer has a valid library identifier."));
        return false;
    }
    if (TrimmedName.IsEmpty())
    {
        OutError = FText::FromString(TEXT(
            "Scenario names cannot be empty."));
        return false;
    }
    if (TrimmedName.Len() > 128)
    {
        OutError = FText::FromString(TEXT(
            "Scenario names cannot exceed 128 characters."));
        return false;
    }
    if (SimulationLibrary == nullptr && !LoadLibrary())
    {
        OutError = FText::FromString(TEXT(
            "The scenario library could not be loaded."));
        return false;
    }

    FTGSavedScenarioRecord* Record =
        SimulationLibrary->SavedScenarios.FindByPredicate(
            [&ScenarioId](const FTGSavedScenarioRecord& Candidate)
            {
                return Candidate.ScenarioId == ScenarioId;
            });
    if (Record == nullptr)
    {
        OutError = FText::FromString(TEXT(
            "The selected scenario no longer exists in the library."));
        return false;
    }

    FString& StoredName =
        Record->Scenario.ScenarioAndSolver.ScenarioName;
    if (StoredName == TrimmedName)
    {
        return true;
    }

    const FString PreviousStoredName = StoredName;
    const FDateTime PreviousModifiedUtc = Record->LastModifiedUtc;
    const bool bRenamingCurrentDraft =
        bHasCurrentScenarioDraft && CurrentScenarioId == ScenarioId;
    const FString PreviousDraftName = bRenamingCurrentDraft
        ? CurrentScenarioDraft.ScenarioAndSolver.ScenarioName
        : FString{};

    StoredName = TrimmedName;
    Record->LastModifiedUtc = FDateTime::UtcNow();
    if (bRenamingCurrentDraft)
    {
        CurrentScenarioDraft.ScenarioAndSolver.ScenarioName = TrimmedName;
    }

    if (!SaveLibrary())
    {
        StoredName = PreviousStoredName;
        Record->LastModifiedUtc = PreviousModifiedUtc;
        if (bRenamingCurrentDraft)
        {
            CurrentScenarioDraft.ScenarioAndSolver.ScenarioName =
                PreviousDraftName;
        }
        OutError = FText::FromString(TEXT(
            "The renamed scenario could not be saved."));
        return false;
    }

    return true;
}

TArray<FTGSavedScenarioRecord>
UTGSimulationSubsystem::GetSavedScenarios() const
{
    if (SimulationLibrary == nullptr)
    {
        return {};
    }

    return SimulationLibrary->SavedScenarios;
}

bool UTGSimulationSubsystem::MarkScenarioRecentlyUsed(
    const FGuid& ScenarioId)
{
    if (!ScenarioId.IsValid())
    {
        return false;
    }
    if (SimulationLibrary == nullptr && !LoadLibrary())
    {
        return false;
    }

    FTGSavedScenarioRecord* Record =
        SimulationLibrary->SavedScenarios.FindByPredicate(
            [&ScenarioId](const FTGSavedScenarioRecord& Candidate)
            {
                return Candidate.ScenarioId == ScenarioId;
            });
    if (Record == nullptr)
    {
        return false;
    }

    Record->LastAccessedUtc = FDateTime::UtcNow();
    return SaveLibrary();
}

FString UTGSimulationSubsystem::ResolveSavedRunResultPath(
    const FString& ResultRelativePath)
{
    if (ResultRelativePath.IsEmpty())
    {
        return FString{};
    }

    return FPaths::ConvertRelativePathToFull(
        FPaths::IsRelative(ResultRelativePath)
            ? FPaths::Combine(
                FPaths::ProjectSavedDir(),
                ResultRelativePath)
            : ResultRelativePath);
}

bool UTGSimulationSubsystem::GetLatestCompletedRunForScenario(
    const FGuid& ScenarioId,
    FTGSavedSimulationRun& OutRun) const
{
    OutRun = FTGSavedSimulationRun{};
    if (!ScenarioId.IsValid() || SimulationLibrary == nullptr)
    {
        return false;
    }

    const FTGSavedSimulationRun* Latest = nullptr;
    for (const FTGSavedSimulationRun& Candidate :
         SimulationLibrary->SavedRuns)
    {
        if (Candidate.SourceScenarioId != ScenarioId ||
            Candidate.Status != ETGSimulationRunStatus::Completed)
        {
            continue;
        }

        const FString ResultPath = ResolveSavedRunResultPath(
            Candidate.ResultRelativePath);
        const FString ScenarioPath = FPaths::Combine(
            FPaths::GetPath(ResultPath),
            TEXT("scenario.tgscn"));
        if (!IFileManager::Get().FileExists(*ResultPath) ||
            !IFileManager::Get().FileExists(*ScenarioPath))
        {
            continue;
        }

        if (Latest == nullptr || Candidate.CreatedUtc > Latest->CreatedUtc)
        {
            Latest = &Candidate;
        }
    }

    if (Latest == nullptr)
    {
        return false;
    }

    OutRun = *Latest;
    return true;
}

bool UTGSimulationSubsystem::HasVisualizableRunForScenario(
    const FGuid& ScenarioId) const
{
    FTGSavedSimulationRun IgnoredRun;
    return GetLatestCompletedRunForScenario(ScenarioId, IgnoredRun);
}

bool UTGSimulationSubsystem::CanContinueScenarioFromLatestRun(
    const FGuid& ScenarioId) const
{
    FTGSavedSimulationRun Run;
    return GetLatestCompletedRunForScenario(ScenarioId, Run) &&
        Run.FinalState.bIsValid;
}

bool UTGSimulationSubsystem::RecordCompletedSimulationRun(
    const FTGSavedSimulationRun& Run)
{
    if (!Run.RunId.IsValid())
    {
        return false;
    }
    if (SimulationLibrary == nullptr && !LoadLibrary())
    {
        return false;
    }

    const int32 ExistingIndex =
        SimulationLibrary->SavedRuns.IndexOfByPredicate(
            [&Run](const FTGSavedSimulationRun& Candidate)
            {
                return Candidate.RunId == Run.RunId;
            });
    if (ExistingIndex == INDEX_NONE)
    {
        SimulationLibrary->SavedRuns.Add(Run);
    }
    else
    {
        SimulationLibrary->SavedRuns[ExistingIndex] = Run;
    }

    if (Run.SourceScenarioId.IsValid())
    {
        FTGSavedScenarioRecord* SourceRecord =
            SimulationLibrary->SavedScenarios.FindByPredicate(
                [&Run](const FTGSavedScenarioRecord& Candidate)
                {
                    return Candidate.ScenarioId == Run.SourceScenarioId;
                });
        if (SourceRecord != nullptr)
        {
            const FDateTime RunActivityUtc = Run.CreatedUtc.GetTicks() > 0
                ? Run.CreatedUtc
                : FDateTime::UtcNow();
            if (RunActivityUtc > SourceRecord->LastAccessedUtc)
            {
                SourceRecord->LastAccessedUtc = RunActivityUtc;
            }
        }
    }
    return SaveLibrary();
}

bool UTGSimulationSubsystem::CreateContinuationDraftFromLatestRun(
    const FGuid& ScenarioId,
    FText& OutError)
{
    OutError = FText::GetEmpty();

    FTGSavedSimulationRun Run;
    if (!GetLatestCompletedRunForScenario(ScenarioId, Run))
    {
        OutError = FText::FromString(
            TEXT("This scenario has no available completed result."));
        return false;
    }
    if (!Run.FinalState.bIsValid)
    {
        OutError = FText::FromString(
            TEXT("The completed result has no valid final-state snapshot."));
        return false;
    }

    FString FinalUtcString;
    FString TimeError;
    if (!FTGSimulationAdapter::ConvertEphemerisTimeTdbSecondsToUtc(
            Run.FinalState.EphemerisTimeTdbSeconds,
            FinalUtcString,
            TimeError))
    {
        OutError = FText::FromString(FString::Printf(
            TEXT("The final epoch could not be converted to UTC: %s"),
            *TimeError));
        return false;
    }

    FDateTime ContinuationStartUtc;
    if (!FDateTime::ParseIso8601(
            *FinalUtcString,
            ContinuationStartUtc))
    {
        OutError = FText::FromString(FString::Printf(
            TEXT("SPICE returned an unsupported final UTC value: %s"),
            *FinalUtcString));
        return false;
    }

    FTGSimulationScenario Continuation = Run.InputSnapshot;
    FTGScenarioSolverConfig& Solver = Continuation.ScenarioAndSolver;
    const FTimespan PreviousFinalUtcSpan =
        Solver.FinalUtc - Solver.StartUtc;
    Solver.StartUtc = ContinuationStartUtc;
    if (Solver.EndMode == ETGSimulationEndMode::FinalUtc &&
        PreviousFinalUtcSpan > FTimespan::Zero())
    {
        Solver.FinalUtc = ContinuationStartUtc + PreviousFinalUtcSpan;
    }
    Solver.ScenarioName = Solver.ScenarioName.TrimStartAndEnd() +
        TEXT(" - Continuation");
    Continuation.InitialState = Run.FinalState.SpacecraftState;

    for (const FTGFinalComponentMass& FinalMass :
         Run.FinalState.ComponentMasses)
    {
        FTGComponentConfig* Component =
            Continuation.Components.FindByPredicate(
                [&FinalMass](const FTGComponentConfig& Candidate)
                {
                    return Candidate.Name == FinalMass.ComponentName;
                });
        if (Component == nullptr || !Component->bVariableMass)
        {
            continue;
        }

        const double OldInitialMass = Component->InitialMassKilograms;
        const double NewInitialMass = FMath::Max(
            Component->MinimumMassKilograms,
            FinalMass.MassKilograms);
        if (OldInitialMass > 0.0 && FMath::IsFinite(NewInitialMass))
        {
            const double InertiaScale = NewInitialMass / OldInitialMass;
            Component->CentroidalInertia.IxxKilogramMetersSquared *= InertiaScale;
            Component->CentroidalInertia.IyyKilogramMetersSquared *= InertiaScale;
            Component->CentroidalInertia.IzzKilogramMetersSquared *= InertiaScale;
            Component->CentroidalInertia.IxyKilogramMetersSquared *= InertiaScale;
            Component->CentroidalInertia.IxzKilogramMetersSquared *= InertiaScale;
            Component->CentroidalInertia.IyzKilogramMetersSquared *= InertiaScale;
            Component->InitialMassKilograms = NewInitialMass;
        }
    }

    for (const FTGFinalJointState& FinalJoint :
         Run.FinalState.JointStates)
    {
        FTGComponentConfig* Component =
            Continuation.Components.FindByPredicate(
                [&FinalJoint](const FTGComponentConfig& Candidate)
                {
                    return Candidate.Name == FinalJoint.ComponentName;
                });
        if (Component == nullptr)
        {
            continue;
        }
        FTGJointDofConfig* Dof =
            Component->DegreesOfFreedom.FindByPredicate(
                [&FinalJoint](const FTGJointDofConfig& Candidate)
                {
                    return Candidate.Name ==
                        FinalJoint.DegreeOfFreedomName;
                });
        if (Dof != nullptr)
        {
            Dof->InitialCoordinate = FinalJoint.Coordinate;
            Dof->InitialRate = FinalJoint.Rate;
        }
    }

    for (const FTGFinalReactionWheelState& FinalWheel :
         Run.FinalState.ReactionWheelStates)
    {
        FTGReactionWheelConfig* Wheel =
            Continuation.ReactionWheels.FindByPredicate(
                [&FinalWheel](const FTGReactionWheelConfig& Candidate)
                {
                    return Candidate.Name == FinalWheel.ReactionWheelName;
                });
        if (Wheel != nullptr)
        {
            Wheel->InitialMomentumNewtonMeterSeconds =
                FinalWheel.MomentumNewtonMeterSeconds;
        }
    }

    SetImportedScenarioDraft(Continuation);
    if (!MarkScenarioRecentlyUsed(ScenarioId))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("PHAROS: Created a continuation from scenario %s, but its recent-use timestamp could not be saved."),
            *ScenarioId.ToString());
    }
    return true;
}

// ============================================================================
// Persistent library
// ============================================================================

bool UTGSimulationSubsystem::LoadLibrary()
{
    const bool bExistingLibrary =
        UGameplayStatics::DoesSaveGameExist(
            SaveSlotName,
            SaveUserIndex);
    FString RecoveryBackupPath;
    if (bExistingLibrary)
    {
        USaveGame* LoadedObject =
            UGameplayStatics::LoadGameFromSlot(
                SaveSlotName,
                SaveUserIndex);

        SimulationLibrary =
            Cast<UTGSimulationSaveGame>(
                LoadedObject);

        if (SimulationLibrary != nullptr &&
            IsSupportedLibraryFormat(*SimulationLibrary))
        {
            const bool bProjectPathsChanged =
                MigrateLibraryProjectPaths(*SimulationLibrary);
            const bool bSrpCachesChanged =
                DiscardDisabledSrpGeometryCaches(*SimulationLibrary);
            const bool bLibraryChanged =
                bProjectPathsChanged || bSrpCachesChanged;
            if (bLibraryChanged &&
                !SaveLibrary())
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("PHAROS migrated saved scenario data, but could not persist the updated library."));
            }
            return true;
        }

        const FString RecoveryReason = SimulationLibrary == nullptr
            ? TEXT("unreadable")
            : FString::Printf(
                TEXT("unsupported_format_%d"),
                SimulationLibrary->LibraryFormatVersion);
        const int32 UnsupportedVersion = SimulationLibrary != nullptr
            ? SimulationLibrary->LibraryFormatVersion
            : INDEX_NONE;
        SimulationLibrary = nullptr;

        if (!PreserveExistingLibraryFile(
                SaveSlotName,
                RecoveryReason,
                true,
                RecoveryBackupPath))
        {
            return false;
        }

        if (UnsupportedVersion == INDEX_NONE)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("PHAROS could not read the existing scenario library. The original was preserved at '%s' before a new library was created."),
                *RecoveryBackupPath);
        }
        else
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("PHAROS cannot open scenario-library format %d. The original was preserved at '%s' before a new library was created."),
                UnsupportedVersion,
                *RecoveryBackupPath);
        }
    }

    SimulationLibrary =
        Cast<UTGSimulationSaveGame>(
            UGameplayStatics::
                CreateSaveGameObject(
                    UTGSimulationSaveGame::
                        StaticClass()));

    if (SimulationLibrary == nullptr)
    {
        return false;
    }

    SimulationLibrary->LibraryFormatVersion =
        UTGSimulationSaveGame::CurrentLibraryFormatVersion;

    if (bExistingLibrary)
    {
        const bool bSaved = UGameplayStatics::SaveGameToSlot(
            SimulationLibrary,
            SaveSlotName,
            SaveUserIndex);
        if (!bSaved)
        {
            if (!RecoveryBackupPath.IsEmpty())
            {
                const uint32 RestoreResult = IFileManager::Get().Copy(
                    *GetLibrarySaveFilePath(SaveSlotName),
                    *RecoveryBackupPath,
                    true,
                    true);
                if (RestoreResult != COPY_OK)
                {
                    UE_LOG(
                        LogTemp,
                        Error,
                        TEXT("PHAROS could not restore the preserved scenario library from '%s' (copy result %u)."),
                        *RecoveryBackupPath,
                        RestoreResult);
                }
            }
            SimulationLibrary = nullptr;
        }
        return bSaved;
    }

    return SaveLibrary();
}

bool UTGSimulationSubsystem::SaveLibrary()
{
    if (SimulationLibrary == nullptr)
    {
        return false;
    }

    SimulationLibrary->LibraryFormatVersion =
        UTGSimulationSaveGame::CurrentLibraryFormatVersion;
    DiscardDisabledSrpGeometryCaches(*SimulationLibrary);

    FString BackupPath;
    if (UGameplayStatics::DoesSaveGameExist(
            SaveSlotName,
            SaveUserIndex) &&
        !PreserveExistingLibraryFile(
            SaveSlotName,
            TEXT("before_save"),
            false,
            BackupPath))
    {
        return false;
    }

    const bool bSaved = UGameplayStatics::SaveGameToSlot(
        SimulationLibrary,
        SaveSlotName,
        SaveUserIndex);
    if (bSaved)
    {
        return true;
    }

    UE_LOG(
        LogTemp,
        Error,
        TEXT("PHAROS could not save the scenario library."));

    if (!BackupPath.IsEmpty())
    {
        IFileManager& FileManager = IFileManager::Get();
        const FString SavePath = GetLibrarySaveFilePath(SaveSlotName);
        const uint32 RestoreResult = FileManager.Copy(
            *SavePath,
            *BackupPath,
            true,
            true);
        if (RestoreResult != COPY_OK)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("PHAROS could not restore the prior scenario library from '%s' (copy result %u)."),
                *BackupPath,
                RestoreResult);
        }
    }

    return false;
}

UTGSimulationSaveGame*
UTGSimulationSubsystem::GetLibrary() const
{
    return SimulationLibrary;
}
