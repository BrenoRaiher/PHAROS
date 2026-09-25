// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Environment/TGSrpComponentRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "UI/Theme/TGUiTheme.h"

void UTGSrpComponentRowWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
    {
        return;
    }

    RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    const FTGUiSpacing& Spacing = TGUiTheme::GetSpacing();

    RootBorder->SetPadding(FMargin(0.0f));
    WidgetTree->RootWidget = RootBorder;

    UHorizontalBox* Row =
        WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    RootBorder->SetContent(Row);

    SelectButton =
        WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    TGUiTheme::ApplyRowButtonStyle(*SelectButton);
    SelectButton->SetClipping(EWidgetClipping::ClipToBounds);
    UHorizontalBoxSlot* ButtonSlot = Row->AddChildToHorizontalBox(SelectButton);
    ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ButtonSlot->SetVerticalAlignment(VAlign_Fill);

    NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TGUiTheme::ApplyTextStyle(
        *NameText,
        ETGUiTextStyle::FieldLabel,
        Palette.TextPrimary);
    NameText->SetJustification(ETextJustify::Left);
    NameText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    NameText->SetClipping(EWidgetClipping::ClipToBounds);

    USizeBox* NameBounds =
        WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    NameBounds->SetMaxDesiredWidth(280.0f);
    NameBounds->SetClipping(EWidgetClipping::ClipToBounds);
    NameBounds->AddChild(NameText);
    if (USizeBoxSlot* TextSlot = Cast<USizeBoxSlot>(NameText->Slot))
    {
        TextSlot->SetHorizontalAlignment(HAlign_Fill);
        TextSlot->SetVerticalAlignment(VAlign_Center);
    }

    SelectButton->SetContent(NameBounds);
    if (UButtonSlot* NameSlot = Cast<UButtonSlot>(NameBounds->Slot))
    {
        NameSlot->SetPadding(
            FMargin(Spacing.Regular, Spacing.Small));
        NameSlot->SetHorizontalAlignment(HAlign_Fill);
        NameSlot->SetVerticalAlignment(VAlign_Center);
    }

    USizeBox* VisibilityWidth =
        WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    VisibilityWidth->SetWidthOverride(58.0f);
    UHorizontalBoxSlot* VisibilityWidthSlot =
        Row->AddChildToHorizontalBox(VisibilityWidth);
    VisibilityWidthSlot->SetPadding(
        FMargin(Spacing.ExtraSmall, Spacing.ExtraSmall));
    VisibilityWidthSlot->SetVerticalAlignment(VAlign_Fill);

    VisibilityButton =
        WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    FButtonStyle VisibilityButtonStyle =
        TGUiTheme::MakeButtonStyle(ETGUiButtonStyle::Quiet);
    VisibilityButtonStyle.Normal.OutlineSettings.Color =
        FSlateColor(Palette.Border);
    VisibilityButtonStyle.Normal.OutlineSettings.Width = 1.0f;
    VisibilityButtonStyle.Normal.OutlineSettings.bUseBrushTransparency = false;
    VisibilityButtonStyle.Hovered.OutlineSettings.Color =
        FSlateColor(Palette.Accent);
    VisibilityButtonStyle.Hovered.OutlineSettings.Width = 1.0f;
    VisibilityButtonStyle.Hovered.OutlineSettings.bUseBrushTransparency = false;
    VisibilityButtonStyle.Pressed.OutlineSettings.Color =
        FSlateColor(Palette.AccentPressed);
    VisibilityButtonStyle.Pressed.OutlineSettings.Width = 1.0f;
    VisibilityButtonStyle.Pressed.OutlineSettings.bUseBrushTransparency = false;
    VisibilityButton->SetStyle(VisibilityButtonStyle);
    VisibilityWidth->AddChild(VisibilityButton);

    VisibilityText =
        WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TGUiTheme::ApplyTextStyle(
        *VisibilityText,
        ETGUiTextStyle::Caption,
        Palette.TextPrimary);
    VisibilityText->SetJustification(ETextJustify::Center);
    VisibilityButton->SetContent(VisibilityText);

    IncludeCheckBox =
        WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
    TGUiTheme::ApplyCompactCheckBoxStyle(*IncludeCheckBox);
    UHorizontalBoxSlot* IncludeSlot =
        Row->AddChildToHorizontalBox(IncludeCheckBox);
    IncludeSlot->SetVerticalAlignment(VAlign_Center);
    IncludeSlot->SetPadding(
        FMargin(Spacing.ExtraSmall, 0.0f, Spacing.Small, 0.0f));

    SelectButton->OnClicked.AddUniqueDynamic(
        this,
        &UTGSrpComponentRowWidget::HandleSelectClicked);
    IncludeCheckBox->OnCheckStateChanged.AddUniqueDynamic(
        this,
        &UTGSrpComponentRowWidget::HandleIncludeChanged);
    VisibilityButton->OnClicked.AddUniqueDynamic(
        this,
        &UTGSrpComponentRowWidget::HandleVisibilityClicked);
}

void UTGSrpComponentRowWidget::Configure(
    FGuid InComponentId,
    const FString& ComponentName,
    bool bIncluded,
    bool bPreviewVisible,
    bool bSelected)
{
    TGuardValue<bool> Guard(bUpdating, true);
    ComponentId = InComponentId;
    bIsPreviewVisible = bPreviewVisible;

    if (NameText != nullptr)
    {
        NameText->SetText(FText::FromString(ComponentName));
    }
    if (IncludeCheckBox != nullptr)
    {
        IncludeCheckBox->SetIsChecked(bIncluded);
        IncludeCheckBox->SetToolTipText(FText::FromString(
            TEXT(
                "Include this component in solar radiation pressure and "
                "spacecraft self-shadowing calculations.")));
    }
    if (VisibilityText != nullptr)
    {
        VisibilityText->SetText(
            FText::FromString(
                bIsPreviewVisible ? TEXT("HIDE") : TEXT("SHOW")));
    }
    if (VisibilityButton != nullptr)
    {
        VisibilityButton->SetToolTipText(
            FText::FromString(
                bIsPreviewVisible
                    ? TEXT("Temporarily hide this component in the 3D preview.")
                    : TEXT("Show this component in the 3D preview.")));
    }
    if (RootBorder != nullptr)
    {
        RootBorder->SetBrushColor(
            bSelected
                ? FLinearColor(
                    0.034340f,
                    0.054480f,
                    0.080220f,
                    1.0f)
                : FLinearColor(
                    0.019382f,
                    0.030713f,
                    0.045186f,
                    1.0f));
    }
}

void UTGSrpComponentRowWidget::HandleSelectClicked()
{
    if (ComponentId.IsValid())
    {
        OnComponentSelected.Broadcast(ComponentId);
    }
}

void UTGSrpComponentRowWidget::HandleIncludeChanged(bool bIsChecked)
{
    if (!bUpdating && ComponentId.IsValid())
    {
        OnIncludeChanged.Broadcast(ComponentId, bIsChecked);
    }
}

void UTGSrpComponentRowWidget::HandleVisibilityClicked()
{
    if (!bUpdating && ComponentId.IsValid())
    {
        OnPreviewVisibilityChanged.Broadcast(
            ComponentId,
            !bIsPreviewVisible);
    }
}
