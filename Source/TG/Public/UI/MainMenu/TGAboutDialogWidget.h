// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TGAboutDialogWidget.generated.h"

class SWidget;

/** Native modal containing PHAROS product and documentation information. */
UCLASS(BlueprintType, meta = (DisplayName = "PHAROS About Dialog"))
class TG_API UTGAboutDialogWidget final : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetRepositoryUrl(const FString& InRepositoryUrl);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|UI|Main Menu")
    void CloseDialog();

protected:
    virtual void NativeConstruct() override;

    virtual FReply NativeOnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent) override;

    virtual TSharedRef<SWidget> RebuildWidget() override;

    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
    bool HasUsableRepositoryUrl() const;
    void HandleRepositoryNavigate();
    void HandleEulaNavigate();
    void HandleNoticesNavigate();
    FReply HandleCloseClicked();

    FString RepositoryUrl;
    TSharedPtr<SWidget> RootSlateWidget;
};
