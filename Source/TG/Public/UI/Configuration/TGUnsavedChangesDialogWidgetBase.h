// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TGUnsavedChangesDialogWidgetBase.generated.h"

class UButton;
class UBorder;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTGUnsavedDialogAction);

UCLASS(Abstract, Blueprintable)
class TG_API UTGUnsavedChangesDialogWidgetBase : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    virtual FReply NativeOnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent) override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_SaveAndBack;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_DiscardChanges;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Cancel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Message;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_SaveAndBack;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_DiscardChanges;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> BORDER_DialogCard;

public:
    /** Applies destination-specific copy for Scenario Library navigation. */
    void ConfigureForScenarioLibrary();

    UPROPERTY(BlueprintAssignable, Category = "PHAROS|Navigation")
    FTGUnsavedDialogAction OnSaveAndBackRequested;

    UPROPERTY(BlueprintAssignable, Category = "PHAROS|Navigation")
    FTGUnsavedDialogAction OnDiscardAndBackRequested;

    UPROPERTY(BlueprintAssignable, Category = "PHAROS|Navigation")
    FTGUnsavedDialogAction OnCancelRequested;

private:
    UFUNCTION()
    void HandleSaveAndBack();

    UFUNCTION()
    void HandleDiscardChanges();

    UFUNCTION()
    void HandleCancel();
};
