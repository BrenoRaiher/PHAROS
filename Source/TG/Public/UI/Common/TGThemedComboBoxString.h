// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "Components/ComboBoxString.h"
#include "TGThemedComboBoxString.generated.h"

/** Combo box initialized with the approved WBP_Config_Atmosphere style. */
UCLASS(meta = (DisplayName = "PHAROS Themed Combo Box (String)"))
class TG_API UTGThemedComboBoxString : public UComboBoxString
{
    GENERATED_BODY()

public:
    UTGThemedComboBoxString(
        const FObjectInitializer& ObjectInitializer);
};
