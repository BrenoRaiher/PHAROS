// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/InitialState/TGConfigInitialStateWidgetBase.h"

#include "UI/Configuration/InitialState/TGConfigInitialStateFrameWidgetBase.h"

void UTGConfigInitialStateWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshFromCurrentDraft();
}

void UTGConfigInitialStateWidgetBase::RefreshFromCurrentDraft()
{
    if (WBP_Config_InitialStateFrame != nullptr)
    {
        WBP_Config_InitialStateFrame->RefreshFromCurrentDraft();
    }
}
