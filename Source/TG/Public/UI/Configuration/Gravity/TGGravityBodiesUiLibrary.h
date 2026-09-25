// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TGGravityBodiesUiLibrary.generated.h"

class UUserWidget;

/** UI helpers shared by the manually authored Gravity and Bodies panel. */
UCLASS()
class TG_API UTGGravityBodiesUiLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Validates the selected harmonic model and applies the complete preview
     * presentation state to the named widgets in the panel.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Configuration|Gravity",
        meta = (DisplayName = "Apply Harmonic CSV Preview State"))
    static bool ApplyHarmonicCsvPreviewState(
        UUserWidget* OwnerWidget,
        const FString& FilePath,
        int32 RequestedMaximumDegree);

    /** Shows a panel-level harmonic CSV error and hides stale preview data. */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Configuration|Gravity",
        meta = (
            DisplayName = "Show Harmonic CSV Error",
            AutoCreateRefTerm = "ErrorText"
        ))
    static void ShowHarmonicCsvError(
        UUserWidget* OwnerWidget,
        const FText& ErrorText);
};
