// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "TGControllerHelpDialogWidget.generated.h"

class SWidget;

/** Concise in-application reference for authoring user controller commands. */
UCLASS(BlueprintType, meta = (DisplayName = "PHAROS Controller Help Dialog"))
class TG_API UTGControllerHelpDialogWidget final : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PHAROS|UI|Controllers")
    void CloseDialog();

protected:
    virtual void NativeConstruct() override;

    virtual FReply NativeOnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent) override;

    virtual TSharedRef<SWidget> RebuildWidget() override;

    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
    FReply HandleCloseClicked();

    TSharedPtr<SWidget> RootSlateWidget;
};
