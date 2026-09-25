// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Common/TGLoadingUiLibrary.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/Common/TGLoadingSubsystem.h"

UTGLoadingSubsystem* UTGLoadingUiLibrary::Resolve(
    const UObject* WorldContextObject)
{
    if (WorldContextObject == nullptr || GEngine == nullptr)
    {
        return nullptr;
    }

    const UWorld* World = GEngine->GetWorldFromContextObject(
        WorldContextObject,
        EGetWorldErrorMode::ReturnNull);
    UGameInstance* GameInstance = World != nullptr
        ? World->GetGameInstance()
        : Cast<UGameInstance>(const_cast<UObject*>(WorldContextObject));
    return GameInstance != nullptr
        ? GameInstance->GetSubsystem<UTGLoadingSubsystem>()
        : nullptr;
}

void UTGLoadingUiLibrary::ShowLoadingPopup(
    const UObject* WorldContextObject,
    const FText& Title,
    const FText& Message)
{
    if (UTGLoadingSubsystem* Subsystem = Resolve(WorldContextObject))
    {
        Subsystem->Show(Title, Message);
    }
}

void UTGLoadingUiLibrary::UpdateLoadingPopup(
    const UObject* WorldContextObject,
    const FText& Title,
    const FText& Message)
{
    if (UTGLoadingSubsystem* Subsystem = Resolve(WorldContextObject))
    {
        Subsystem->Update(Title, Message);
    }
}

void UTGLoadingUiLibrary::HideLoadingPopup(
    const UObject* WorldContextObject)
{
    if (UTGLoadingSubsystem* Subsystem = Resolve(WorldContextObject))
    {
        Subsystem->Hide();
    }
}

void UTGLoadingUiLibrary::HideLoadingPopupAfterNextTick(
    const UObject* WorldContextObject)
{
    UTGLoadingSubsystem* Subsystem = Resolve(WorldContextObject);
    UWorld* World = WorldContextObject != nullptr
        ? WorldContextObject->GetWorld()
        : nullptr;
    if (Subsystem == nullptr || World == nullptr)
    {
        return;
    }

    const uint64 Generation = Subsystem->GetOperationGeneration();
    const TWeakObjectPtr<UTGLoadingSubsystem> WeakSubsystem(Subsystem);
    World->GetTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateLambda(
            [WeakSubsystem, Generation]()
            {
                if (UTGLoadingSubsystem* Loading = WeakSubsystem.Get())
                {
                    Loading->HideIfCurrent(Generation);
                }
            }));
}
