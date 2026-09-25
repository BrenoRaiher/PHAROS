// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Controls/TGControllerListEntryWidgetBase.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/Configuration/Controls/TGConfigControlsWidgetBase.h"
#include "UI/Theme/TGUiTheme.h"

void UTGControllerListEntryWidgetBase::InitializeControllerListEntry(
    const FName InControllerId,
    const FString& InDisplayName,
    const ETGControllerBuildStatus InBuildStatus,
    const bool bInTrusted,
    const bool bInSelected,
    UTGConfigControlsWidgetBase* InOwnerPanel)
{
    ControllerId = InControllerId;
    OwnerPanel = InOwnerPanel;

    if (BTN_SelectController != nullptr)
    {
        BTN_SelectController->SetStyle(
            TGUiTheme::MakeButtonStyle(
                bInSelected
                    ? ETGUiButtonStyle::Secondary
                    : ETGUiButtonStyle::Quiet));
    }

    if (TXT_ControllerRowName != nullptr)
    {
        TXT_ControllerRowName->SetText(FText::FromString(InDisplayName));
    }

    if (TXT_ControllerRowStatus != nullptr)
    {
        FString StatusText;

        switch (InBuildStatus)
        {
        case ETGControllerBuildStatus::NotBuilt:
            StatusText = TEXT("Not built");
            break;

        case ETGControllerBuildStatus::Building:
            StatusText = TEXT("Building");
            break;

        case ETGControllerBuildStatus::Ready:
            StatusText = TEXT("Ready");
            break;

        case ETGControllerBuildStatus::Failed:
            StatusText = TEXT("Failed");
            break;

        case ETGControllerBuildStatus::IncompatibleController:
            StatusText = TEXT("Incompatible controller");
            break;

        default:
            StatusText = TEXT("Unknown");
            break;
        }

        TXT_ControllerRowStatus->SetText(FText::FromString(StatusText));

        const FTGUiPalette& Palette = TGUiTheme::GetPalette();
        FLinearColor StatusColor = Palette.TextSecondary;
        if (InBuildStatus == ETGControllerBuildStatus::Ready)
        {
            StatusColor = Palette.Success;
        }
        else if (InBuildStatus == ETGControllerBuildStatus::Building)
        {
            StatusColor = Palette.Info;
        }
        else if (InBuildStatus == ETGControllerBuildStatus::Failed ||
                 InBuildStatus ==
                     ETGControllerBuildStatus::IncompatibleController)
        {
            StatusColor = Palette.Error;
        }
        TXT_ControllerRowStatus->SetColorAndOpacity(StatusColor);
    }

    if (TXT_ControllerRowTrust != nullptr)
    {
        TXT_ControllerRowTrust->SetText(
            FText::FromString(
                bInTrusted
                    ? TEXT("Trusted")
                    : TEXT("Untrusted")));
        TXT_ControllerRowTrust->SetColorAndOpacity(
            bInTrusted
                ? TGUiTheme::GetPalette().TextSecondary
                : TGUiTheme::GetPalette().Warning);
    }
}

void UTGControllerListEntryWidgetBase::RequestSelectController()
{
    if (ControllerId.IsNone() || OwnerPanel == nullptr)
    {
        return;
    }

    OwnerPanel->SelectLibraryController(ControllerId);
}
