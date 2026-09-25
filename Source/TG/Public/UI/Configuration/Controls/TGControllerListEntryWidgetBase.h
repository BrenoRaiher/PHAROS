// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Simulation/Control/TGControllerLibrarySubsystem.h"

#include "TGControllerListEntryWidgetBase.generated.h"

class UButton;
class UTextBlock;
class UTGConfigControlsWidgetBase;

/** One lightweight Controller Library row. */
UCLASS(Blueprintable)
class TG_API UTGControllerListEntryWidgetBase : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void InitializeControllerListEntry(
        FName InControllerId,
        const FString& InDisplayName,
        ETGControllerBuildStatus InBuildStatus,
        bool bInTrusted,
        bool bInSelected,
        UTGConfigControlsWidgetBase* InOwnerPanel);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void RequestSelectController();

private:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_SelectController;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ControllerRowName;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ControllerRowStatus;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ControllerRowTrust;

    UPROPERTY()
    TObjectPtr<UTGConfigControlsWidgetBase> OwnerPanel;

    FName ControllerId = NAME_None;
};
