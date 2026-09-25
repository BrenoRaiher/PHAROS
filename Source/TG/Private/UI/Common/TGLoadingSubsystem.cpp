// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Common/TGLoadingSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "UI/Common/TGLoadingOverlayWidget.h"

void UTGLoadingSubsystem::Deinitialize()
{
    Hide();
    Super::Deinitialize();
}

uint64 UTGLoadingSubsystem::Show(
    const FText& Title,
    const FText& Message)
{
    ++OperationGeneration;

    if (ActiveOverlay == nullptr)
    {
        UGameInstance* GameInstance = GetGameInstance();
        APlayerController* OwningPlayer = GameInstance != nullptr
            ? GameInstance->GetFirstLocalPlayerController()
            : nullptr;
        ActiveOverlay = OwningPlayer != nullptr
            ? CreateWidget<UTGLoadingOverlayWidget>(
                OwningPlayer,
                UTGLoadingOverlayWidget::StaticClass())
            : CreateWidget<UTGLoadingOverlayWidget>(
                GameInstance,
                UTGLoadingOverlayWidget::StaticClass());
    }

    if (ActiveOverlay != nullptr)
    {
        ActiveOverlay->Configure(Title, Message);
        if (!ActiveOverlay->IsInViewport())
        {
            ActiveOverlay->AddToViewport(20000);
        }
        ActiveOverlay->SetKeyboardFocus();
    }

    return OperationGeneration;
}

void UTGLoadingSubsystem::Update(
    const FText& Title,
    const FText& Message)
{
    if (ActiveOverlay != nullptr)
    {
        ActiveOverlay->Configure(Title, Message);
    }
}

void UTGLoadingSubsystem::Hide()
{
    ++OperationGeneration;
    if (ActiveOverlay != nullptr)
    {
        ActiveOverlay->RemoveFromParent();
        ActiveOverlay = nullptr;
    }
}

void UTGLoadingSubsystem::HideIfCurrent(
    const uint64 InOperationGeneration)
{
    if (OperationGeneration == InOperationGeneration)
    {
        Hide();
    }
}

bool UTGLoadingSubsystem::IsShowing() const
{
    return ActiveOverlay != nullptr && ActiveOverlay->IsInViewport();
}

uint64 UTGLoadingSubsystem::GetOperationGeneration() const
{
    return OperationGeneration;
}
