// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TGFileDialogLibrary.generated.h"

/**
 * Shared HUD utility for selecting files from the operating system.
 *
 * The function is intentionally independent of CSV parsing, gravity,
 * aerodynamic databases, controller files, and every other panel-specific use.
 */
UCLASS()
class TG_API UTGFileDialogLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Opens a Windows file-selection dialog and returns one existing file.
     *
     * AllowedExtensions must contain extensions without requiring a leading dot:
     *     csv
     *     txt
     *     json
     *
     * Inputs such as ".csv" and "*.csv" are also normalized automatically.
     *
     * Returning false means that the user cancelled the dialog or that no
     * acceptable file was selected.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|HUD|Files",
        meta = (
            DisplayName = "Open Single File Dialog",
            AutoCreateRefTerm = "AllowedExtensions",
            AdvancedDisplay = "DefaultDirectory,DefaultFileName,bIncludeAllFiles"
        ))
    static bool OpenSingleFileDialog(
        const FString& DialogTitle,
        const FString& DefaultDirectory,
        const FString& DefaultFileName,
        const FString& FileTypeDescription,
        const TArray<FString>& AllowedExtensions,
        bool bIncludeAllFiles,
        FString& SelectedFilePath);
};