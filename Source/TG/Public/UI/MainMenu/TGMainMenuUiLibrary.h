// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TGMainMenuUiLibrary.generated.h"

class UTGAboutDialogWidget;

/** Blueprint entry points for native Main Menu presentation. */
UCLASS()
class TG_API UTGMainMenuUiLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|UI|Main Menu",
        meta = (
            WorldContext = "WorldContextObject",
            DefaultToSelf = "WorldContextObject",
            DisplayName = "Show About and Documentation Dialog"
        ))
    static UTGAboutDialogWidget* ShowAboutDialog(
        UObject* WorldContextObject,
        const FString& RepositoryUrl);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|UI|Main Menu",
        meta = (
            WorldContext = "WorldContextObject",
            DefaultToSelf = "WorldContextObject",
            DisplayName = "Quit PHAROS With Loading"))
    static void QuitPharosWithLoading(UObject* WorldContextObject);
};
