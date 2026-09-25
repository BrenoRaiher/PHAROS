// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/TGUnsavedChangesDialogWidgetBase.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Input/Reply.h"

void UTGUnsavedChangesDialogWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();

    SetIsFocusable(true);

    BTN_SaveAndBack->OnClicked.AddUniqueDynamic(
        this,
        &UTGUnsavedChangesDialogWidgetBase::HandleSaveAndBack);

    BTN_DiscardChanges->OnClicked.AddUniqueDynamic(
        this,
        &UTGUnsavedChangesDialogWidgetBase::HandleDiscardChanges);

    BTN_Cancel->OnClicked.AddUniqueDynamic(
        this,
        &UTGUnsavedChangesDialogWidgetBase::HandleCancel);
}

FReply UTGUnsavedChangesDialogWidgetBase::NativeOnKeyDown(
    const FGeometry& InGeometry,
    const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        HandleCancel();
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UTGUnsavedChangesDialogWidgetBase::ConfigureForScenarioLibrary()
{
    if (BORDER_DialogCard != nullptr)
    {
        if (UCanvasPanelSlot* CardSlot = Cast<UCanvasPanelSlot>(
                BORDER_DialogCard->Slot))
        {
            FVector2D CardSize = CardSlot->GetSize();
            CardSize.X = FMath::Max(CardSize.X, 820.0);
            CardSlot->SetSize(CardSize);
        }
    }

    if (TXT_Message != nullptr)
    {
        TXT_Message->SetText(FText::FromString(TEXT(
            "This scenario has unsaved changes. Choose whether to save or "
            "discard them before opening the Scenario Library.")));
    }
    if (TXT_SaveAndBack != nullptr)
    {
        TXT_SaveAndBack->SetText(FText::FromString(
            TEXT("Save and Open Scenario Library")));
    }
    if (TXT_DiscardChanges != nullptr)
    {
        TXT_DiscardChanges->SetText(FText::FromString(
            TEXT("Discard and Open Scenario Library")));
    }
}

void UTGUnsavedChangesDialogWidgetBase::HandleSaveAndBack()
{
    OnSaveAndBackRequested.Broadcast();
}

void UTGUnsavedChangesDialogWidgetBase::HandleDiscardChanges()
{
    OnDiscardAndBackRequested.Broadcast();
}

void UTGUnsavedChangesDialogWidgetBase::HandleCancel()
{
    OnCancelRequested.Broadcast();
}
