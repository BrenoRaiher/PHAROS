// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/MainMenu/TGScenarioLibraryEntryWidgetBase.h"

#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "UI/MainMenu/TGScenarioLibraryTypes.h"
#include "UI/Theme/TGUiTheme.h"

namespace TGScenarioLibraryEntryPrivate
{
    const FEditableTextBoxStyle& GetInlineRenameStyle()
    {
        static const FEditableTextBoxStyle Style = []
        {
            const FTGUiPalette& Palette = TGUiTheme::GetPalette();
            FSlateBrush NoBackground;
            NoBackground.DrawAs = ESlateBrushDrawType::NoDrawType;
            NoBackground.TintColor = FSlateColor(FLinearColor::Transparent);

            FEditableTextBoxStyle Result =
                TGUiTheme::GetEditableTextBoxStyle();
            Result.SetBackgroundImageNormal(NoBackground)
                .SetBackgroundImageHovered(NoBackground)
                .SetBackgroundImageFocused(NoBackground)
                .SetBackgroundImageReadOnly(NoBackground)
                .SetBackgroundColor(FSlateColor(FLinearColor::Transparent))
                .SetPadding(FMargin(0.0f))
                .SetFont(TGUiTheme::GetSlateFont(
                    ETGUiTextStyle::BodyStrong))
                .SetForegroundColor(FSlateColor(Palette.TextPrimary))
                .SetFocusedForegroundColor(FSlateColor(Palette.TextPrimary));
            return Result;
        }();
        return Style;
    }
}

void UTGScenarioLibraryEntryWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();

    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    TGUiTheme::ApplyTextStyle(
        *TXT_ScenarioName,
        ETGUiTextStyle::BodyStrong,
        Palette.TextPrimary);
    TGUiTheme::ApplyTextStyle(
        *TXT_ResultStatus,
        ETGUiTextStyle::FieldLabel,
        Palette.TextSecondary);
    TGUiTheme::ApplyTextStyle(
        *TXT_LastModified,
        ETGUiTextStyle::FieldLabel,
        Palette.TextSecondary);
    TGUiTheme::ApplyTextStyle(
        *TXT_Created,
        ETGUiTextStyle::FieldLabel,
        Palette.TextSecondary);
    TXT_ScenarioName->SetJustification(ETextJustify::Left);
    TXT_ResultStatus->SetJustification(ETextJustify::Left);
    TXT_LastModified->SetJustification(ETextJustify::Left);
    TXT_Created->SetJustification(ETextJustify::Left);
    SIZE_ResultStatus->SetWidthOverride(210.0f);
    SIZE_LastModified->SetWidthOverride(190.0f);
    SIZE_Created->SetWidthOverride(190.0f);
    for (USizeBox* SizeBox : {
             SIZE_ResultStatus.Get(),
             SIZE_LastModified.Get(),
             SIZE_Created.Get()})
    {
        if (USizeBoxSlot* ColumnSlot = Cast<USizeBoxSlot>(
                SizeBox->GetContentSlot()))
        {
            ColumnSlot->SetHorizontalAlignment(HAlign_Left);
        }
    }

    if (INPUT_ScenarioName != nullptr)
    {
        INPUT_ScenarioName->SetWidgetStyle(
            TGScenarioLibraryEntryPrivate::GetInlineRenameStyle());
        INPUT_ScenarioName->SetSelectAllTextWhenFocused(true);
        INPUT_ScenarioName->SetRevertTextOnEscape(true);
        INPUT_ScenarioName->SetClearKeyboardFocusOnCommit(true);
        INPUT_ScenarioName->OnTextCommitted.RemoveDynamic(
            this,
            &UTGScenarioLibraryEntryWidgetBase::
                HandleScenarioNameCommitted);
        INPUT_ScenarioName->OnTextCommitted.AddDynamic(
            this,
            &UTGScenarioLibraryEntryWidgetBase::
                HandleScenarioNameCommitted);
    }

    RefreshContent();
    RefreshBackground();
}

void UTGScenarioLibraryEntryWidgetBase::NativeDestruct()
{
    if (INPUT_ScenarioName != nullptr)
    {
        INPUT_ScenarioName->OnTextCommitted.RemoveDynamic(
            this,
            &UTGScenarioLibraryEntryWidgetBase::
                HandleScenarioNameCommitted);
    }
    Super::NativeDestruct();
}

void UTGScenarioLibraryEntryWidgetBase::BeginInlineRename()
{
    if (Item == nullptr || INPUT_ScenarioName == nullptr)
    {
        return;
    }

    RestoreReadOnlyAppearance();
    bInlineRenameActive = true;
    INPUT_ScenarioName->SetText(Item->ScenarioName);
    TXT_ScenarioName->SetVisibility(ESlateVisibility::Collapsed);
    INPUT_ScenarioName->SetVisibility(ESlateVisibility::Visible);
    INPUT_ScenarioName->SetKeyboardFocus();
}

void UTGScenarioLibraryEntryWidgetBase::CommitInlineRename()
{
    if (!bInlineRenameActive)
    {
        RestoreReadOnlyAppearance();
        return;
    }

    UTGScenarioLibraryItem* RenameItem = Item;
    const FString NewName = INPUT_ScenarioName != nullptr
        ? INPUT_ScenarioName->GetText().ToString()
        : FString{};
    RestoreReadOnlyAppearance();

    if (RenameItem != nullptr)
    {
        RenameItem->OnRenameRequested.Broadcast(RenameItem, NewName);
    }
}

void UTGScenarioLibraryEntryWidgetBase::RestoreReadOnlyAppearance()
{
    bSelected = IsListItemSelected();
    EndInlineRename();
}

void UTGScenarioLibraryEntryWidgetBase::HandleScenarioNameCommitted(
    const FText&,
    const ETextCommit::Type CommitMethod)
{
    if (!bInlineRenameActive)
    {
        RestoreReadOnlyAppearance();
        return;
    }

    if (CommitMethod == ETextCommit::OnCleared)
    {
        RestoreReadOnlyAppearance();
        return;
    }

    CommitInlineRename();
}

void UTGScenarioLibraryEntryWidgetBase::EndInlineRename()
{
    bInlineRenameActive = false;
    if (INPUT_ScenarioName != nullptr)
    {
        INPUT_ScenarioName->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (TXT_ScenarioName != nullptr)
    {
        TXT_ScenarioName->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    RefreshBackground();
}

void UTGScenarioLibraryEntryWidgetBase::NativeOnListItemObjectSet(
    UObject* ListItemObject)
{
    IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
    EndInlineRename();
    Item = Cast<UTGScenarioLibraryItem>(ListItemObject);
    bSelected = IsListItemSelected();
    bHovered = false;
    RefreshContent();
    RefreshBackground();
}

void UTGScenarioLibraryEntryWidgetBase::NativeOnItemSelectionChanged(
    const bool bIsSelected)
{
    IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);
    bSelected = bIsSelected;
    RefreshBackground();
}

void UTGScenarioLibraryEntryWidgetBase::NativeOnEntryReleased()
{
    RestoreReadOnlyAppearance();
    Item = nullptr;
    bSelected = false;
    bHovered = false;
    RefreshBackground();
    IUserListEntry::NativeOnEntryReleased();
}

void UTGScenarioLibraryEntryWidgetBase::NativeOnMouseEnter(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
    bHovered = true;
    RefreshBackground();
}

void UTGScenarioLibraryEntryWidgetBase::NativeOnMouseLeave(
    const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseLeave(InMouseEvent);
    bHovered = false;
    RefreshBackground();
}

void UTGScenarioLibraryEntryWidgetBase::RefreshContent()
{
    if (Item == nullptr ||
        TXT_ScenarioName == nullptr ||
        TXT_ResultStatus == nullptr ||
        TXT_LastModified == nullptr ||
        TXT_Created == nullptr)
    {
        return;
    }

    TXT_ScenarioName->SetText(Item->ScenarioName);
    TXT_ResultStatus->SetText(Item->ResultStatus);
    TXT_LastModified->SetText(Item->LastModified);
    TXT_Created->SetText(Item->Created);

    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    const FLinearColor StatusColor = Item->bCanVisualize
        ? Palette.Success
        : Palette.TextMuted;
    TXT_ResultStatus->SetColorAndOpacity(StatusColor);

    const FText EmptyToolTip = FText::GetEmpty();
    SetToolTipText(EmptyToolTip);
    BORDER_Row->SetToolTipText(EmptyToolTip);
    TXT_ScenarioName->SetToolTipText(EmptyToolTip);
    TXT_ResultStatus->SetToolTipText(EmptyToolTip);
    TXT_LastModified->SetToolTipText(EmptyToolTip);
    TXT_Created->SetToolTipText(EmptyToolTip);
    if (INPUT_ScenarioName != nullptr)
    {
        INPUT_ScenarioName->SetToolTipText(EmptyToolTip);
    }
}

void UTGScenarioLibraryEntryWidgetBase::RefreshBackground()
{
    if (BORDER_Row == nullptr)
    {
        return;
    }

    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    BORDER_Row->SetBrushColor(
        bSelected
            ? Palette.Selection
            : (bHovered ? Palette.SurfaceRaised : Palette.Surface));
}
