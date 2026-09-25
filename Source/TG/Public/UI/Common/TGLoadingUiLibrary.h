// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TGLoadingUiLibrary.generated.h"

/** Blueprint entry points for the shared PHAROS loading presentation. */
UCLASS()
class TG_API UTGLoadingUiLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|UI|Loading",
        meta = (WorldContext = "WorldContextObject"))
    static void ShowLoadingPopup(
        const UObject* WorldContextObject,
        const FText& Title,
        const FText& Message);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|UI|Loading",
        meta = (WorldContext = "WorldContextObject"))
    static void UpdateLoadingPopup(
        const UObject* WorldContextObject,
        const FText& Title,
        const FText& Message);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|UI|Loading",
        meta = (WorldContext = "WorldContextObject"))
    static void HideLoadingPopup(const UObject* WorldContextObject);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|UI|Loading",
        meta = (WorldContext = "WorldContextObject"))
    static void HideLoadingPopupAfterNextTick(
        const UObject* WorldContextObject);

    static class UTGLoadingSubsystem* Resolve(
        const UObject* WorldContextObject);
};
