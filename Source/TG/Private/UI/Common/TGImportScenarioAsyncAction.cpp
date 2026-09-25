// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Common/TGImportScenarioAsyncAction.h"

#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Simulation/TGScenarioFileLibrary.h"
#include "Simulation/TGSimulationSubsystem.h"
#include "TimerManager.h"
#include "UI/Common/TGFileDialogLibrary.h"
#include "UI/Common/TGLoadingUiLibrary.h"

namespace TGImportScenarioAsyncPrivate
{
    struct FResult
    {
        bool bSucceeded = false;
        FTGSimulationScenario Scenario;
        FText Message;
    };
}

UTGImportScenarioAsyncAction*
UTGImportScenarioAsyncAction::ImportPharosScenarioAsync(
    UObject* WorldContextObject)
{
    UTGImportScenarioAsyncAction* Action =
        NewObject<UTGImportScenarioAsyncAction>();
    Action->ContextObject = WorldContextObject;
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UTGImportScenarioAsyncAction::Activate()
{
    if (ContextObject == nullptr)
    {
        CompleteWithoutImport(FText::FromString(
            TEXT("The application context is unavailable.")));
        return;
    }

    if (!UTGFileDialogLibrary::OpenSingleFileDialog(
            TEXT("Import PHAROS Scenario"),
            FPaths::ProjectSavedDir(),
            FString{},
            TEXT("PHAROS scenario"),
            {TEXT("tgscn")},
            false,
            SelectedFilePath))
    {
        CompleteWithoutImport(FText::FromString(
            TEXT("No PHAROS scenario file was selected.")));
        return;
    }

    UTGLoadingUiLibrary::ShowLoadingPopup(
        ContextObject,
        FText::FromString(TEXT("Importing Scenario")),
        FText::FromString(TEXT(
            "Reading and preparing the selected PHAROS scenario.")));

    UWorld* World = ContextObject->GetWorld();
    if (World != nullptr)
    {
        World->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateUObject(
                this,
                &UTGImportScenarioAsyncAction::BeginBackgroundImport));
        return;
    }

    BeginBackgroundImport();
}

void UTGImportScenarioAsyncAction::BeginBackgroundImport()
{
    const FString FilePath = SelectedFilePath;
    const TWeakObjectPtr<UTGImportScenarioAsyncAction> WeakThis(this);

    Async(
        EAsyncExecution::ThreadPool,
        [WeakThis, FilePath]()
        {
            TGImportScenarioAsyncPrivate::FResult Result;
            Result.bSucceeded =
                UTGScenarioFileLibrary::ImportScenarioFromTgscn(
                    nullptr,
                    FilePath,
                    Result.Scenario,
                    Result.Message);

            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis, FilePath, Result = MoveTemp(Result)]() mutable
                {
                    UTGImportScenarioAsyncAction* Action = WeakThis.Get();
                    if (Action == nullptr)
                    {
                        return;
                    }

                    bool bSucceeded = Result.bSucceeded;
                    if (bSucceeded)
                    {
                        UWorld* World = Action->ContextObject != nullptr
                            ? Action->ContextObject->GetWorld()
                            : nullptr;
                        UGameInstance* GameInstance = World != nullptr
                            ? World->GetGameInstance()
                            : nullptr;
                        UTGSimulationSubsystem* ScenarioSubsystem =
                            GameInstance != nullptr
                                ? GameInstance->GetSubsystem<
                                    UTGSimulationSubsystem>()
                                : nullptr;
                        if (ScenarioSubsystem != nullptr)
                        {
                            ScenarioSubsystem->SetImportedScenarioDraft(
                                Result.Scenario);
                        }
                        else
                        {
                            bSucceeded = false;
                            Result.Message = FText::FromString(TEXT(
                                "The simulation scenario subsystem is unavailable."));
                        }
                    }

                    if (bSucceeded)
                    {
                        Action->Succeeded.Broadcast(
                            FilePath,
                            Result.Message);
                    }
                    else
                    {
                        Action->Failed.Broadcast(
                            FilePath,
                            Result.Message);
                    }

                    UTGLoadingUiLibrary::HideLoadingPopupAfterNextTick(
                        Action->ContextObject);
                    Action->SetReadyToDestroy();
                });
        });
}

void UTGImportScenarioAsyncAction::CompleteWithoutImport(
    const FText& Message)
{
    Failed.Broadcast(FString{}, Message);
    SetReadyToDestroy();
}
