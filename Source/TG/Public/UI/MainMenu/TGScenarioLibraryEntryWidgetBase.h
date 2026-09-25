// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "TGScenarioLibraryEntryWidgetBase.generated.h"

class UBorder;
class UEditableTextBox;
class USizeBox;
class UTextBlock;
class UTGScenarioLibraryItem;

/** Native presentation behavior for a selectable Scenario Library row. */
UCLASS(Abstract, Blueprintable)
class TG_API UTGScenarioLibraryEntryWidgetBase
    : public UUserWidget,
      public IUserObjectListEntry
{
    GENERATED_BODY()

public:
    void BeginInlineRename();
    void CommitInlineRename();
    void RestoreReadOnlyAppearance();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    virtual void NativeOnListItemObjectSet(
        UObject* ListItemObject) override;

    virtual void NativeOnItemSelectionChanged(
        bool bIsSelected) override;

    virtual void NativeOnEntryReleased() override;

    virtual void NativeOnMouseEnter(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    virtual void NativeOnMouseLeave(
        const FPointerEvent& InMouseEvent) override;

private:
    UFUNCTION()
    void HandleScenarioNameCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    void EndInlineRename();
    void RefreshContent();
    void RefreshBackground();

    UPROPERTY(Transient)
    TObjectPtr<UTGScenarioLibraryItem> Item;

    UPROPERTY(Transient)
    bool bSelected = false;

    UPROPERTY(Transient)
    bool bHovered = false;

    UPROPERTY(Transient)
    bool bInlineRenameActive = false;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BORDER_Row;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ScenarioName;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> INPUT_ScenarioName;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ResultStatus;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USizeBox> SIZE_ResultStatus;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_LastModified;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USizeBox> SIZE_LastModified;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Created;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USizeBox> SIZE_Created;
};
