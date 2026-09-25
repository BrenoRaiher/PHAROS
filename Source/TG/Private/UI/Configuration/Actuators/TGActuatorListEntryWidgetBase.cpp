// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Actuators/TGActuatorListEntryWidgetBase.h"

#include "UI/Configuration/Actuators/TGConfigActuatorsWidgetBase.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UTGActuatorListEntryWidgetBase::
InitializeActuatorListEntry(
    const int32 InActuatorIndex,
    const FString& InActuatorName,
    UTGConfigActuatorsWidgetBase* InOwnerPanel,
    const bool bInReactionWheel)
{
    ActuatorIndex = InActuatorIndex;
    OwnerPanel = InOwnerPanel;
    bReactionWheel = bInReactionWheel;

    if (TXT_ActuatorName != nullptr)
    {
        TXT_ActuatorName->SetText(
            FText::FromString(InActuatorName));
    }
}

void UTGActuatorListEntryWidgetBase::
RequestSelectActuator()
{
    if (ActuatorIndex == INDEX_NONE ||
        OwnerPanel == nullptr)
    {
        return;
    }

    if (bReactionWheel)
    {
        OwnerPanel->SelectReactionWheelFromCurrentDraft(
            ActuatorIndex);
    }
    else
    {
        OwnerPanel->SelectThrusterFromCurrentDraft(
            ActuatorIndex);
    }
}

void UTGActuatorListEntryWidgetBase::
SetActuatorListEntrySelected(
    const bool bInSelected)
{
    if (BTN_SelectActuator == nullptr)
    {
        return;
    }

    if (!bUnselectedStyleCaptured)
    {
        UnselectedButtonStyle = BTN_SelectActuator->GetStyle();
        bUnselectedStyleCaptured = true;
    }

    FButtonStyle Style = UnselectedButtonStyle;
    if (bInSelected)
    {
        Style.Normal = UnselectedButtonStyle.Pressed;
        Style.NormalForeground =
            UnselectedButtonStyle.PressedForeground;
    }

    BTN_SelectActuator->SetStyle(Style);
    BTN_SelectActuator->SetIsEnabled(true);
}
