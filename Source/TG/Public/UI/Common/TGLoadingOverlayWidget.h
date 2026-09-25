// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TGLoadingOverlayWidget.generated.h"

class STextBlock;
class SWidget;

/** Modal, asset-free loading presentation shared by PHAROS workflows. */
UCLASS(Transient, NotBlueprintable)
class TG_API UTGLoadingOverlayWidget final : public UUserWidget
{
    GENERATED_BODY()

public:
    void Configure(const FText& InTitle, const FText& InMessage);

protected:
    virtual void NativeConstruct() override;

    virtual FReply NativeOnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent) override;

    virtual FReply NativeOnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    virtual FReply NativeOnMouseButtonUp(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
    FText Title;
    FText Message;
    TSharedPtr<STextBlock> TitleWidget;
    TSharedPtr<STextBlock> MessageWidget;
    TSharedPtr<SWidget> RootSlateWidget;
};
