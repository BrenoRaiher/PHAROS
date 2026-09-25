// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Gravity/TGGravityBodiesUiLibrary.h"

#include "Blueprint/UserWidget.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Simulation/TGHarmonicCsvLibrary.h"

namespace TGGravityBodiesUiPrivate
{
    constexpr int32 PreviewCoefficientRowLimit = 100;

    struct FPreviewWidgets
    {
        UTextBlock* Error = nullptr;
        UVerticalBox* PreviewContainer = nullptr;
        UMultiLineEditableTextBox* Preview = nullptr;

        bool IsComplete() const
        {
            return Error != nullptr
                && PreviewContainer != nullptr
                && Preview != nullptr;
        }
    };

    FPreviewWidgets FindPreviewWidgets(UUserWidget* OwnerWidget)
    {
        FPreviewWidgets Widgets;
        if (OwnerWidget == nullptr)
        {
            return Widgets;
        }

        Widgets.Error = Cast<UTextBlock>(
            OwnerWidget->GetWidgetFromName(TEXT("TXT_HarmonicCsvError")));
        Widgets.PreviewContainer = Cast<UVerticalBox>(
            OwnerWidget->GetWidgetFromName(TEXT("VBOX_HarmonicCsvPreview")));
        Widgets.Preview = Cast<UMultiLineEditableTextBox>(
            OwnerWidget->GetWidgetFromName(
                TEXT("VIEW_HarmonicCsvInspection")));
        return Widgets;
    }

    void ClearPreview(const FPreviewWidgets& Widgets)
    {
        if (Widgets.Preview != nullptr)
        {
            Widgets.Preview->SetIsReadOnly(true);
            Widgets.Preview->SetText(FText::GetEmpty());
        }
        if (Widgets.PreviewContainer != nullptr)
        {
            Widgets.PreviewContainer->SetVisibility(
                ESlateVisibility::Collapsed);
        }
    }

    void HideError(const FPreviewWidgets& Widgets)
    {
        if (Widgets.Error != nullptr)
        {
            Widgets.Error->SetText(FText::GetEmpty());
            Widgets.Error->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    void ShowError(
        const FPreviewWidgets& Widgets,
        const FText& ErrorText)
    {
        ClearPreview(Widgets);
        if (Widgets.Error != nullptr)
        {
            Widgets.Error->SetText(ErrorText);
            Widgets.Error->SetVisibility(ESlateVisibility::Visible);
        }
    }
}

bool UTGGravityBodiesUiLibrary::ApplyHarmonicCsvPreviewState(
    UUserWidget* OwnerWidget,
    const FString& FilePath,
    const int32 RequestedMaximumDegree)
{
    const TGGravityBodiesUiPrivate::FPreviewWidgets Widgets =
        TGGravityBodiesUiPrivate::FindPreviewWidgets(OwnerWidget);

    if (!Widgets.IsComplete())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Gravity and Bodies harmonic CSV preview widgets are "
                "missing or have incompatible types."));
        return false;
    }

    const FString NormalizedPath = FilePath.TrimStartAndEnd();
    if (NormalizedPath.IsEmpty())
    {
        TGGravityBodiesUiPrivate::ClearPreview(Widgets);
        if (RequestedMaximumDegree <= 0)
        {
            TGGravityBodiesUiPrivate::HideError(Widgets);
            return RequestedMaximumDegree == 0;
        }

        TGGravityBodiesUiPrivate::ShowError(
            Widgets,
            FText::FromString(
                TEXT(
                    "Select a harmonic-model CSV before requesting a "
                    "positive maximum harmonic degree.")));
        return false;
    }

    if (RequestedMaximumDegree < 0)
    {
        TGGravityBodiesUiPrivate::ShowError(
            Widgets,
            FText::FromString(
                TEXT(
                    "Maximum harmonic degree must be a nonnegative "
                    "integer.")));
        return false;
    }

    FTGHarmonicCsvInspection Inspection;
    FText PreviewText;
    FText PreviewError;
    if (!UTGHarmonicCsvLibrary::BuildHarmonicModelCsvPreview(
            NormalizedPath,
            TGGravityBodiesUiPrivate::PreviewCoefficientRowLimit,
            Inspection,
            PreviewText,
            PreviewError))
    {
        TGGravityBodiesUiPrivate::ShowError(Widgets, PreviewError);
        return false;
    }

    if (!UTGHarmonicCsvLibrary::SupportsMaximumDegree(
            Inspection,
            RequestedMaximumDegree))
    {
        TGGravityBodiesUiPrivate::ShowError(
            Widgets,
            UTGHarmonicCsvLibrary::FormatDegreeCompatibilityFailureForHud(
                Inspection,
                RequestedMaximumDegree));
        return false;
    }

    Widgets.Preview->SetIsReadOnly(true);
    Widgets.Preview->SetText(PreviewText);
    Widgets.PreviewContainer->SetVisibility(ESlateVisibility::Visible);
    TGGravityBodiesUiPrivate::HideError(Widgets);
    return true;
}

void UTGGravityBodiesUiLibrary::ShowHarmonicCsvError(
    UUserWidget* OwnerWidget,
    const FText& ErrorText)
{
    const TGGravityBodiesUiPrivate::FPreviewWidgets Widgets =
        TGGravityBodiesUiPrivate::FindPreviewWidgets(OwnerWidget);
    if (!Widgets.IsComplete())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Gravity and Bodies harmonic CSV preview widgets are "
                "missing or have incompatible types."));
        return;
    }

    TGGravityBodiesUiPrivate::ShowError(Widgets, ErrorText);
}
