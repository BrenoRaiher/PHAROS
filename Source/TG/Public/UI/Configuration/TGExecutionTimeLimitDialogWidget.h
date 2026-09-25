// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "TGExecutionTimeLimitDialogWidget.generated.h"

class SEditableTextBox;
class STextBlock;
class SWidget;
class UTGSimulationSubsystem;

/** Native modal editor for the backend wall-clock execution limit. */
UCLASS(BlueprintType, meta = (DisplayName = "PHAROS Execution Time Limit Dialog"))
class TG_API UTGExecutionTimeLimitDialogWidget final : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PHAROS|UI|Configuration")
    void CloseDialog();

protected:
    virtual void NativeConstruct() override;

    virtual FReply NativeOnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent) override;

    virtual TSharedRef<SWidget> RebuildWidget() override;

    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
    UTGSimulationSubsystem* GetSimulationSubsystem() const;
    bool ApplyExecutionTimeLimit();
    void ShowInputError(const FText& Message);

    FReply HandleApplyClicked();
    FReply HandleCancelClicked();
    void HandleTextCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    TSharedPtr<SEditableTextBox> TimeoutInput;
    TSharedPtr<STextBlock> ErrorText;
    TSharedPtr<SWidget> RootSlateWidget;
};
