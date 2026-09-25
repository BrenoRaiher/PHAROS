// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TGEulaDialogWidget.generated.h"

class SWidget;

/** Modal used for affirmative acceptance of the packaged-product EULA. */
UCLASS(meta = (DisplayName = "PHAROS EULA Dialog"))
class TG_API UTGEulaDialogWidget final : public UUserWidget
{
    GENERATED_BODY()

public:
    void Configure(
        const FString& InEulaVersion,
        const FText& InDocumentText,
        bool bInCanAccept,
        FSimpleDelegate InAccepted,
        FSimpleDelegate InDeclined);

protected:
    virtual void NativeConstruct() override;

    virtual FReply NativeOnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent) override;

    virtual TSharedRef<SWidget> RebuildWidget() override;

    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
    FReply HandleAcceptClicked();
    FReply HandleDeclineClicked();

    FString EulaVersion = TEXT("1.0");
    FText DocumentText;
    bool bCanAccept = false;
    FSimpleDelegate Accepted;
    FSimpleDelegate Declined;
    TSharedPtr<SWidget> RootSlateWidget;
};
