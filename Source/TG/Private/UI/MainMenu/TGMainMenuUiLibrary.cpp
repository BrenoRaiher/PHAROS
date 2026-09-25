// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/MainMenu/TGMainMenuUiLibrary.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "UI/Common/TGLoadingUiLibrary.h"
#include "UI/MainMenu/TGAboutDialogWidget.h"

UTGAboutDialogWidget* UTGMainMenuUiLibrary::ShowAboutDialog(
    UObject* WorldContextObject,
    const FString& RepositoryUrl)
{
    if (GEngine == nullptr || WorldContextObject == nullptr)
    {
        return nullptr;
    }

    UWorld* World = GEngine->GetWorldFromContextObject(
        WorldContextObject,
        EGetWorldErrorMode::LogAndReturnNull);

    if (World == nullptr)
    {
        return nullptr;
    }

    APlayerController* OwningPlayer = World->GetFirstPlayerController();
    if (OwningPlayer == nullptr)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("PHAROS: Cannot show the About dialog without a player controller."));
        return nullptr;
    }

    UTGAboutDialogWidget* Dialog =
        CreateWidget<UTGAboutDialogWidget>(
            OwningPlayer,
            UTGAboutDialogWidget::StaticClass());

    if (Dialog == nullptr)
    {
        return nullptr;
    }

    Dialog->SetRepositoryUrl(RepositoryUrl);
    Dialog->AddToViewport(5000);
    Dialog->SetKeyboardFocus();
    return Dialog;
}

void UTGMainMenuUiLibrary::QuitPharosWithLoading(
    UObject* WorldContextObject)
{
    if (WorldContextObject == nullptr)
    {
        return;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (World == nullptr)
    {
        return;
    }

    UTGLoadingUiLibrary::ShowLoadingPopup(
        WorldContextObject,
        FText::FromString(TEXT("Closing PHAROS")),
        FText::FromString(TEXT("Finishing application tasks.")));

    const TWeakObjectPtr<UObject> WeakContext(WorldContextObject);
    World->GetTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateLambda(
            [WeakContext]()
            {
                UObject* Context = WeakContext.Get();
                UWorld* CurrentWorld = Context != nullptr
                    ? Context->GetWorld()
                    : nullptr;
                if (CurrentWorld == nullptr)
                {
                    return;
                }

                UKismetSystemLibrary::QuitGame(
                    Context,
                    CurrentWorld->GetFirstPlayerController(),
                    EQuitPreference::Quit,
                    false);
            }));
}
