// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Visualization/TGVisualizationHudWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/DataTable.h"
#include "UI/Theme/TGUiTheme.h"
#include "UI/Visualization/TGSolarSystemOverviewWidget.h"
#include "UI/Visualization/TGVisualizationArrowRowWidget.h"
#include "UI/Visualization/TGVisualizationConstellationRowWidget.h"
#include "UI/Visualization/TGVisualizationDockWorkspaceWidget.h"
#include "UI/Visualization/TGVisualizationTelemetryCardWidget.h"
#include "UObject/UnrealType.h"
#include "Visualization/TGSimulationPlaybackActor.h"
#include "Simulation/TGSimulationRunSubsystem.h"

namespace TGVisualizationHudPrivate
{
    FString ReadConstellationName(
        const UScriptStruct& RowStruct,
        const uint8* RowData,
        const FName RowName)
    {
        for (TFieldIterator<FProperty> PropertyIt(
                 &RowStruct,
                 EFieldIteratorFlags::IncludeSuper);
             PropertyIt;
             ++PropertyIt)
        {
            FProperty* Property = *PropertyIt;
            const FString PropertyName = Property->GetName();
            if (!PropertyName.StartsWith(TEXT("Constellation_")) &&
                PropertyName != TEXT("Constellation"))
            {
                continue;
            }
            if (const FStrProperty* StringProperty =
                    CastField<FStrProperty>(Property))
            {
                return StringProperty->GetPropertyValue_InContainer(RowData);
            }
            if (const FNameProperty* NameProperty =
                    CastField<FNameProperty>(Property))
            {
                return NameProperty->GetPropertyValue_InContainer(RowData)
                    .ToString();
            }
            if (const FTextProperty* TextProperty =
                    CastField<FTextProperty>(Property))
            {
                return TextProperty->GetPropertyValue_InContainer(RowData)
                    .ToString();
            }
        }
        return RowName.ToString();
    }

    FString ReadConstellationId(
        const UScriptStruct& RowStruct,
        const uint8* RowData,
        const FString& FallbackId)
    {
        for (TFieldIterator<FProperty> PropertyIt(
                 &RowStruct,
                 EFieldIteratorFlags::IncludeSuper);
             PropertyIt;
             ++PropertyIt)
        {
            FProperty* Property = *PropertyIt;
            if (!Property->GetName().StartsWith(TEXT("ConstellationId")))
            {
                continue;
            }
            if (const FEnumProperty* EnumProperty =
                    CastField<FEnumProperty>(Property))
            {
                const void* Value = Property->ContainerPtrToValuePtr<void>(
                    RowData);
                const int64 EnumValue = EnumProperty->GetUnderlyingProperty()
                    ->GetSignedIntPropertyValue(Value);
                return EnumProperty->GetEnum()->GetNameStringByValue(
                    EnumValue);
            }
            if (const FByteProperty* ByteProperty =
                    CastField<FByteProperty>(Property))
            {
                const uint8 Value = ByteProperty->GetPropertyValue_InContainer(
                    RowData);
                return ByteProperty->Enum != nullptr
                    ? ByteProperty->Enum->GetNameStringByValue(Value)
                    : FString::FromInt(Value);
            }
            if (const FNameProperty* NameProperty =
                    CastField<FNameProperty>(Property))
            {
                return NameProperty->GetPropertyValue_InContainer(RowData)
                    .ToString();
            }
            if (const FStrProperty* StringProperty =
                    CastField<FStrProperty>(Property))
            {
                return StringProperty->GetPropertyValue_InContainer(RowData);
            }
        }
        return FallbackId;
    }

    FLinearColor ResolveTelemetryAccent(const FString& Label)
    {
        if (Label.Contains(TEXT("Mass")) ||
            Label.Contains(TEXT("Speed")) ||
            Label.Contains(TEXT("Altitude")) ||
            Label == TEXT("Closest body"))
        {
            return TGUiTheme::GetPalette().Accent;
        }
        if (Label.Contains(TEXT("ICRF")))
        {
            return FLinearColor(0.26f, 0.52f, 0.96f);
        }
        if (Label.Contains(TEXT("body-fixed")) ||
            Label.EndsWith(TEXT(" B")))
        {
            return FLinearColor(0.24f, 0.78f, 0.52f);
        }
        if (Label.Contains(TEXT("Sun")) ||
            Label.Contains(TEXT("Aerodynamic")) ||
            Label.Contains(TEXT("Knudsen")) ||
            Label.Contains(TEXT("speed ratio")))
        {
            return FLinearColor(0.96f, 0.66f, 0.16f);
        }
        if (Label.Contains(TEXT("articulation"), ESearchCase::IgnoreCase) ||
            Label.Contains(TEXT("wheel"), ESearchCase::IgnoreCase) ||
            Label.Contains(TEXT("joint"), ESearchCase::IgnoreCase))
        {
            return FLinearColor(0.72f, 0.42f, 0.90f);
        }
        return TGUiTheme::GetPalette().TextSecondary;
    }
}

UTGVisualizationHudWidget::UTGVisualizationHudWidget(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ArrowRowWidgetClass =
        TSoftClassPtr<UTGVisualizationArrowRowWidget>(FSoftObjectPath(
            TEXT("/Game/UI/Visualization/WBP_VisualizationArrowRow."
                 "WBP_VisualizationArrowRow_C")));
    ConstellationRowWidgetClass =
        TSoftClassPtr<UTGVisualizationConstellationRowWidget>(FSoftObjectPath(
            TEXT("/Game/UI/Visualization/TGVisualizationConstellationRowWidget."
                 "TGVisualizationConstellationRowWidget_C")));
    TelemetryCardWidgetClass =
        TSoftClassPtr<UTGVisualizationTelemetryCardWidget>(FSoftObjectPath(
            TEXT("/Game/UI/Visualization/WBP_VisualizationTelemetryCard."
                 "WBP_VisualizationTelemetryCard_C")));
    ConstellationTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
        TEXT("/Game/Data/DT_ConstellationsVisible."
             "DT_ConstellationsVisible")));
}

void UTGVisualizationHudWidget::InitializeForPlayback(
    ATGSimulationPlaybackActor* InPlaybackActor)
{
    if (IsValid(PlaybackActor))
    {
        PlaybackActor->OnPlaybackTimeChanged.RemoveDynamic(
            this,
            &UTGVisualizationHudWidget::HandlePlaybackTimeChanged);
    }

    PlaybackActor = InPlaybackActor;
    if (IsValid(PlaybackActor))
    {
        PlaybackActor->OnPlaybackTimeChanged.AddUniqueDynamic(
            this,
            &UTGVisualizationHudWidget::HandlePlaybackTimeChanged);
    }
    if (WORKSPACE_Dock != nullptr)
    {
        WORKSPACE_Dock->InitializeForPlayback(PlaybackActor);
    }
    else if (GRAPH_SolarSystem != nullptr)
    {
        GRAPH_SolarSystem->InitializeForPlayback(PlaybackActor);
    }

    if (WORKSPACE_Dock == nullptr)
    {
        RebuildArrowRows();
    }
    RefreshDisplayedValues();
}

void UTGVisualizationHudWidget::NativeConstruct()
{
    Super::NativeConstruct();

    EnsurePauseMenuNavigationControls();
    ApplyVisualizationTheme();
    CacheTelemetryBindings();
    BindNativeControls();
    BTN_ReturnToConfiguration->SetIsEnabled(true);
    BTN_GoToScenarioLibrary->SetIsEnabled(true);
    BTN_ReturnToMainMenu->SetIsEnabled(true);
    OVERLAY_PauseMenu->SetVisibility(
        bPauseMenuOpen
            ? ESlateVisibility::Visible
            : ESlateVisibility::Collapsed);

    if (IsValid(PlaybackActor))
    {
        PlaybackActor->OnPlaybackTimeChanged.AddUniqueDynamic(
            this,
            &UTGVisualizationHudWidget::HandlePlaybackTimeChanged);
    }
    if (WORKSPACE_Dock != nullptr)
    {
        WORKSPACE_Dock->InitializeForPlayback(PlaybackActor);
        if (PANEL_Display != nullptr)
        {
            PANEL_Display->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (PANEL_Telemetry != nullptr)
        {
            PANEL_Telemetry->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (PANEL_Overview != nullptr)
        {
            PANEL_Overview->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    else
    {
        if (GRAPH_SolarSystem != nullptr)
        {
            GRAPH_SolarSystem->InitializeForPlayback(PlaybackActor);
        }
        RebuildConstellationRows();
        RebuildArrowRows();
    }
    if (BTN_CollapseOverview != nullptr)
    {
        BTN_CollapseOverview->SetVisibility(ESlateVisibility::Collapsed);
    }
    RefreshDisplayedValues();
}

void UTGVisualizationHudWidget::EnsurePauseMenuNavigationControls()
{
    if (WidgetTree == nullptr)
    {
        return;
    }

    if (UTextBlock* ConfigurationText = Cast<UTextBlock>(
            WidgetTree->FindWidget(TEXT("TXT_ReturnToConfiguration"))))
    {
        ConfigurationText->SetText(
            FText::FromString(TEXT("Go to Configuration")));
    }

    VBOX_PauseMenu = Cast<UVerticalBox>(
        WidgetTree->FindWidget(TEXT("VBOX_PauseMenu")));
    if (VBOX_PauseMenu == nullptr)
    {
        return;
    }

    const auto EnsureButton = [this](
        TObjectPtr<UButton>& Button,
        const FName ButtonName,
        const FName TextName,
        const TCHAR* Label)
    {
        Button = Cast<UButton>(WidgetTree->FindWidget(ButtonName));
        if (Button != nullptr)
        {
            if (UTextBlock* Text = Cast<UTextBlock>(
                    WidgetTree->FindWidget(TextName)))
            {
                Text->SetText(FText::FromString(Label));
            }
            return;
        }

        Button = WidgetTree->ConstructWidget<UButton>(
            UButton::StaticClass(), ButtonName);
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), TextName);
        Text->SetText(FText::FromString(Label));
        Text->SetJustification(ETextJustify::Center);

        if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(
                Button->AddChild(Text)))
        {
            ButtonSlot->SetPadding(FMargin(12.0f, 9.0f));
            ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
            ButtonSlot->SetVerticalAlignment(VAlign_Center);
        }
        if (UVerticalBoxSlot* VerticalSlot =
                VBOX_PauseMenu->AddChildToVerticalBox(Button))
        {
            VerticalSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
            VerticalSlot->SetHorizontalAlignment(HAlign_Fill);
        }
    };

    EnsureButton(
        BTN_GoToScenarioLibrary,
        TEXT("BTN_GoToScenarioLibrary"),
        TEXT("TXT_GoToScenarioLibrary"),
        TEXT("Go to Scenario Library"));
    EnsureButton(
        BTN_ReturnToMainMenu,
        TEXT("BTN_ReturnToMainMenu"),
        TEXT("TXT_ReturnToMainMenu"),
        TEXT("Return to Main Menu"));
}

void UTGVisualizationHudWidget::ApplyVisualizationTheme()
{
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();

    if (UBorder* PauseOverlay = Cast<UBorder>(OVERLAY_PauseMenu))
    {
        PauseOverlay->SetBrush(TGUiTheme::MakeRoundedBrush(
            Palette.Canvas.CopyWithNewOpacity(0.72f),
            0.0f));
    }
    if (PANEL_PauseMenu != nullptr)
    {
        PANEL_PauseMenu->SetBrush(TGUiTheme::MakeRoundedBrush(
            Palette.Panel,
            5.0f,
            Palette.Border,
            1.0f));
    }

    const auto ApplyButton = [](UButton* Button, const ETGUiButtonStyle Style)
    {
        if (Button != nullptr)
        {
            Button->SetStyle(TGUiTheme::GetButtonStyle(Style));
        }
    };
    ApplyButton(BTN_StayInVisualization, ETGUiButtonStyle::Primary);
    ApplyButton(BTN_ReturnToConfiguration, ETGUiButtonStyle::Secondary);
    ApplyButton(BTN_GoToScenarioLibrary, ETGUiButtonStyle::Secondary);
    ApplyButton(BTN_ReturnToMainMenu, ETGUiButtonStyle::Secondary);
    ApplyButton(BTN_CollapseTelemetry, ETGUiButtonStyle::Quiet);
    ApplyButton(BTN_CollapseDisplay, ETGUiButtonStyle::Quiet);
    ApplyButton(BTN_CollapseOverview, ETGUiButtonStyle::Quiet);
    ApplyButton(BTN_TabConstellations, ETGUiButtonStyle::Secondary);
    ApplyButton(BTN_TabVectors, ETGUiButtonStyle::Secondary);
    ApplyButton(BTN_FocusSpacecraft, ETGUiButtonStyle::Secondary);
    ApplyButton(BTN_SolarOverview, ETGUiButtonStyle::Secondary);

    const auto ApplyInput = [](UEditableTextBox* Input)
    {
        if (Input != nullptr)
        {
            // UEditableTextBox::SetWidgetStyle forwards the address of its
            // argument to the live SEditableTextBox. The style must therefore
            // outlive this call; a temporary leaves Slate with a dangling
            // pointer that fails during the next layout prepass.
            Input->SetWidgetStyle(
                TGUiTheme::GetEditableTextBoxStyle());
        }
    };
    ApplyInput(INPUT_ConstellationSearch);
    ApplyInput(INPUT_VectorSearch);


    const auto ApplyText = [](
        UTextBlock* Text,
        const ETGUiTextStyle Style,
        const FLinearColor& Color)
    {
        if (Text != nullptr)
        {
            TGUiTheme::ApplyTextStyle(*Text, Style, Color);
        }
    };
    ApplyText(TXT_CollapseDisplay, ETGUiTextStyle::Caption,
        Palette.TextSecondary);
    ApplyText(TXT_CollapseTelemetry, ETGUiTextStyle::Caption,
        Palette.TextSecondary);
    ApplyText(TXT_CollapseOverview, ETGUiTextStyle::Caption,
        Palette.TextSecondary);

    const auto FindText = [this](const FName WidgetName)
    {
        return WidgetTree != nullptr
            ? Cast<UTextBlock>(WidgetTree->FindWidget(WidgetName))
            : nullptr;
    };
    ApplyText(FindText(TEXT("TXT_PauseTitle")),
        ETGUiTextStyle::ScreenTitle, Palette.TextPrimary);
    ApplyText(FindText(TEXT("TXT_StayInVisualization")),
        ETGUiTextStyle::BodyStrong, Palette.Canvas);
    ApplyText(FindText(TEXT("TXT_ReturnToConfiguration")),
        ETGUiTextStyle::Body, Palette.TextPrimary);
    ApplyText(FindText(TEXT("TXT_GoToScenarioLibrary")),
        ETGUiTextStyle::Body, Palette.TextPrimary);
    ApplyText(FindText(TEXT("TXT_ReturnToMainMenu")),
        ETGUiTextStyle::Body, Palette.TextPrimary);

    UTextBlock* const TelemetryTexts[] = {
        TXT_TelemetryMass,
        TXT_TelemetryMassRate,
        TXT_TelemetrySpeedIcrf,
        TXT_TelemetryPositionIcrf,
        TXT_TelemetryVelocityIcrf,
        TXT_TelemetryAngularVelocityBody,
        TXT_TelemetryAngularVelocityIcrf,
        TXT_TelemetryClosestBody,
        TXT_TelemetryAltitude,
        TXT_TelemetryPositionBodyFixed,
        TXT_TelemetryVelocityBodyFixed};
    for (UTextBlock* Text : TelemetryTexts)
    {
        ApplyText(Text, ETGUiTextStyle::Numeric, Palette.TextPrimary);
    }
}

void UTGVisualizationHudWidget::NativeDestruct()
{
    UnbindNativeControls();
    if (WORKSPACE_Dock != nullptr)
    {
        WORKSPACE_Dock->InitializeForPlayback(nullptr);
    }
    if (IsValid(PlaybackActor))
    {
        PlaybackActor->OnPlaybackTimeChanged.RemoveDynamic(
            this,
            &UTGVisualizationHudWidget::HandlePlaybackTimeChanged);
    }
    Super::NativeDestruct();
}

void UTGVisualizationHudWidget::BindNativeControls()
{
    // Remove first so reconstructing a Widget Blueprint cannot duplicate work.
    UnbindNativeControls();

    if (INPUT_ConstellationSearch != nullptr)
    {
        INPUT_ConstellationSearch->OnTextChanged.AddUniqueDynamic(
            this,
            &UTGVisualizationHudWidget::HandleConstellationSearchChanged);
    }
    if (INPUT_VectorSearch != nullptr)
    {
        INPUT_VectorSearch->OnTextChanged.AddUniqueDynamic(
            this,
            &UTGVisualizationHudWidget::HandleVectorSearchChanged);
    }

    if (BTN_TabConstellations != nullptr)
    {
        BTN_TabConstellations->OnClicked.AddUniqueDynamic(
            this,
            &UTGVisualizationHudWidget::HandleConstellationsTabClicked);
    }
    if (BTN_TabVectors != nullptr)
    {
        BTN_TabVectors->OnClicked.AddUniqueDynamic(
            this,
            &UTGVisualizationHudWidget::HandleVectorsTabClicked);
    }
    if (BTN_FocusSpacecraft != nullptr)
    {
        BTN_FocusSpacecraft->OnClicked.AddUniqueDynamic(
            this,
            &UTGVisualizationHudWidget::HandleOverviewFocusSpacecraft);
    }
    if (BTN_SolarOverview != nullptr)
    {
        BTN_SolarOverview->OnClicked.AddUniqueDynamic(
            this,
            &UTGVisualizationHudWidget::HandleOverviewReset);
    }
    BTN_StayInVisualization->OnClicked.AddUniqueDynamic(
        this,
        &UTGVisualizationHudWidget::HandleStayInVisualization);
    BTN_ReturnToConfiguration->OnClicked.AddUniqueDynamic(
        this,
        &UTGVisualizationHudWidget::HandleReturnToConfiguration);
    BTN_GoToScenarioLibrary->OnClicked.AddUniqueDynamic(
        this,
        &UTGVisualizationHudWidget::HandleGoToScenarioLibrary);
    BTN_ReturnToMainMenu->OnClicked.AddUniqueDynamic(
        this,
        &UTGVisualizationHudWidget::HandleReturnToMainMenu);
}

void UTGVisualizationHudWidget::UnbindNativeControls()
{
    if (INPUT_ConstellationSearch != nullptr)
    {
        INPUT_ConstellationSearch->OnTextChanged.RemoveDynamic(
            this,
            &UTGVisualizationHudWidget::HandleConstellationSearchChanged);
    }
    if (INPUT_VectorSearch != nullptr)
    {
        INPUT_VectorSearch->OnTextChanged.RemoveDynamic(
            this,
            &UTGVisualizationHudWidget::HandleVectorSearchChanged);
    }
    if (BTN_TabConstellations != nullptr)
    {
        BTN_TabConstellations->OnClicked.RemoveDynamic(
            this,
            &UTGVisualizationHudWidget::HandleConstellationsTabClicked);
    }
    if (BTN_TabVectors != nullptr)
    {
        BTN_TabVectors->OnClicked.RemoveDynamic(
            this,
            &UTGVisualizationHudWidget::HandleVectorsTabClicked);
    }
    if (BTN_FocusSpacecraft != nullptr)
    {
        BTN_FocusSpacecraft->OnClicked.RemoveDynamic(
            this,
            &UTGVisualizationHudWidget::HandleOverviewFocusSpacecraft);
    }
    if (BTN_SolarOverview != nullptr)
    {
        BTN_SolarOverview->OnClicked.RemoveDynamic(
            this,
            &UTGVisualizationHudWidget::HandleOverviewReset);
    }
    if (BTN_StayInVisualization != nullptr)
    {
        BTN_StayInVisualization->OnClicked.RemoveDynamic(
            this,
            &UTGVisualizationHudWidget::HandleStayInVisualization);
    }
    if (BTN_ReturnToConfiguration != nullptr)
    {
        BTN_ReturnToConfiguration->OnClicked.RemoveDynamic(
            this,
            &UTGVisualizationHudWidget::HandleReturnToConfiguration);
    }
    if (BTN_GoToScenarioLibrary != nullptr)
    {
        BTN_GoToScenarioLibrary->OnClicked.RemoveDynamic(
            this,
            &UTGVisualizationHudWidget::HandleGoToScenarioLibrary);
    }
    if (BTN_ReturnToMainMenu != nullptr)
    {
        BTN_ReturnToMainMenu->OnClicked.RemoveDynamic(
            this,
            &UTGVisualizationHudWidget::HandleReturnToMainMenu);
    }
}

void UTGVisualizationHudWidget::CacheTelemetryBindings()
{
    TelemetryValueTexts.Reset();
    const auto AddBinding = [this](
        const TCHAR* Key,
        UTextBlock* ValueText)
    {
        if (ValueText != nullptr)
        {
            TelemetryValueTexts.Add(Key, ValueText);
        }
    };
    AddBinding(TEXT("Mass"), TXT_TelemetryMass);
    AddBinding(TEXT("Mass rate"), TXT_TelemetryMassRate);
    AddBinding(TEXT("Speed ICRF"), TXT_TelemetrySpeedIcrf);
    AddBinding(TEXT("Position ICRF"), TXT_TelemetryPositionIcrf);
    AddBinding(TEXT("Velocity ICRF"), TXT_TelemetryVelocityIcrf);
    AddBinding(
        TEXT("Angular velocity B"),
        TXT_TelemetryAngularVelocityBody);
    AddBinding(
        TEXT("Angular velocity ICRF"),
        TXT_TelemetryAngularVelocityIcrf);
    AddBinding(TEXT("Closest body"), TXT_TelemetryClosestBody);
    AddBinding(TEXT("Altitude"), TXT_TelemetryAltitude);
    AddBinding(
        TEXT("Position in body-fixed frame"),
        TXT_TelemetryPositionBodyFixed);
    AddBinding(
        TEXT("Velocity in body-fixed frame"),
        TXT_TelemetryVelocityBodyFixed);
}

void UTGVisualizationHudWidget::RebuildArrowRows()
{
    if (VBOX_VectorRows == nullptr)
    {
        return;
    }
    VBOX_VectorRows->ClearChildren();
    ArrowRows.Reset();
    if (!IsValid(PlaybackActor))
    {
        return;
    }

    UClass* RowClass = ArrowRowWidgetClass.LoadSynchronous();
    if (RowClass == nullptr)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Visualization arrows need "
                 "/Game/UI/Visualization/WBP_VisualizationArrowRow."));
        return;
    }

    TArray<FTGVisualizationArrowInfo> ArrowInfos =
        PlaybackActor->GetVisualizationArrows();
    ArrowInfos.Sort([](
        const FTGVisualizationArrowInfo& A,
        const FTGVisualizationArrowInfo& B)
    {
        return A.DisplayName < B.DisplayName;
    });
    for (const FTGVisualizationArrowInfo& ArrowInfo : ArrowInfos)
    {
        UTGVisualizationArrowRowWidget* Row =
            CreateWidget<UTGVisualizationArrowRowWidget>(
                GetOwningPlayer(),
                RowClass);
        if (Row != nullptr)
        {
            Row->InitializeArrowRow(PlaybackActor, ArrowInfo);
            VBOX_VectorRows->AddChild(Row);
            ArrowRows.Add(Row);
        }
    }
    ApplyVectorSearchFilter(
        INPUT_VectorSearch != nullptr
            ? INPUT_VectorSearch->GetText().ToString()
            : FString());
}

void UTGVisualizationHudWidget::RebuildConstellationRows()
{
    UPanelWidget* RowHost = VBOX_ConstellationRows != nullptr
        ? VBOX_ConstellationRows.Get()
        : HOST_Constellations.Get();
    if (RowHost == nullptr)
    {
        return;
    }
    RowHost->ClearChildren();
    ConstellationRows.Reset();

    UDataTable* Table = ConstellationTable.LoadSynchronous();
    UClass* RowClass = ConstellationRowWidgetClass.LoadSynchronous();
    if (Table == nullptr || Table->GetRowStruct() == nullptr ||
        RowClass == nullptr)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Constellation controls need DT_ConstellationsVisible and "
                 "/Game/UI/Visualization/"
                 "TGVisualizationConstellationRowWidget."));
        return;
    }

    struct FConstellationListItem
    {
        FString Id;
        FString Name;
    };
    TArray<FConstellationListItem> Items;
    for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
    {
        FConstellationListItem& Item = Items.AddDefaulted_GetRef();
        Item.Id = TGVisualizationHudPrivate::ReadConstellationId(
            *Table->GetRowStruct(),
            Pair.Value,
            Pair.Key.ToString());
        Item.Name = TGVisualizationHudPrivate::ReadConstellationName(
            *Table->GetRowStruct(),
            Pair.Value,
            Pair.Key);
    }
    Items.Sort([](
        const FConstellationListItem& A,
        const FConstellationListItem& B)
    {
        return A.Name < B.Name;
    });

    for (const FConstellationListItem& Item : Items)
    {
        UTGVisualizationConstellationRowWidget* Row =
            CreateWidget<UTGVisualizationConstellationRowWidget>(
                GetOwningPlayer(),
                RowClass);
        if (Row == nullptr)
        {
            continue;
        }
        Row->InitializeConstellationRow(Item.Id, Item.Name);
        RowHost->AddChild(Row);
        ConstellationRows.Add(Row);
    }
    ApplyConstellationSearchFilter(
        INPUT_ConstellationSearch != nullptr
            ? INPUT_ConstellationSearch->GetText().ToString()
            : FString());
}

void UTGVisualizationHudWidget::ApplyVectorSearchFilter(
    const FString& SearchText)
{
    const FString Needle = SearchText.TrimStartAndEnd();
    for (UTGVisualizationArrowRowWidget* Row : ArrowRows)
    {
        if (Row == nullptr)
        {
            continue;
        }
        const bool bMatches = Needle.IsEmpty() ||
            Row->GetArrowLabel().Contains(Needle, ESearchCase::IgnoreCase);
        Row->SetVisibility(
            bMatches ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UTGVisualizationHudWidget::ApplyConstellationSearchFilter(
    const FString& SearchText)
{
    const FString Needle = SearchText.TrimStartAndEnd();
    for (UTGVisualizationConstellationRowWidget* Row : ConstellationRows)
    {
        if (Row == nullptr)
        {
            continue;
        }
        const bool bMatches = Needle.IsEmpty() ||
            Row->GetConstellationName().Contains(
                Needle,
                ESearchCase::IgnoreCase);
        Row->SetVisibility(
            bMatches ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UTGVisualizationHudWidget::RefreshDisplayedValues()
{
    // Timeline presentation is native to WORKSPACE_Dock. Text telemetry
    // remains available only when the native workspace is absent.
    RefreshTelemetryValues();
}

void UTGVisualizationHudWidget::RefreshTelemetryValues()
{
    if (WORKSPACE_Dock != nullptr)
    {
        return;
    }
    if (!IsValid(PlaybackActor))
    {
        return;
    }

    for (TPair<FString, TObjectPtr<UTextBlock>>& Pair : TelemetryValueTexts)
    {
        if (Pair.Value != nullptr)
        {
            Pair.Value->SetText(FText::FromString(TEXT("--")));
        }
    }
    for (TPair<FString, TObjectPtr<UTGVisualizationTelemetryCardWidget>>& Pair :
         TelemetryCards)
    {
        if (Pair.Value != nullptr)
        {
            Pair.Value->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    TArray<FString> Lines;
    PlaybackActor->GetCurrentTelemetryLines(Lines);
    UClass* CardClass = VBOX_TelemetryCards != nullptr
        ? TelemetryCardWidgetClass.LoadSynchronous()
        : nullptr;
    for (const FString& Line : Lines)
    {
        int32 SeparatorIndex = INDEX_NONE;
        if (!Line.FindChar(TEXT(':'), SeparatorIndex))
        {
            continue;
        }
        const FString Key = Line.Left(SeparatorIndex).TrimStartAndEnd();
        const FString Value = Line.Mid(SeparatorIndex + 1).TrimStartAndEnd();
        if (TObjectPtr<UTextBlock>* ValueText = TelemetryValueTexts.Find(Key))
        {
            if (*ValueText != nullptr)
            {
                (*ValueText)->SetText(FText::FromString(Value));
            }
        }

        if (VBOX_TelemetryCards != nullptr && CardClass != nullptr)
        {
            TObjectPtr<UTGVisualizationTelemetryCardWidget>* ExistingCard =
                TelemetryCards.Find(Key);
            UTGVisualizationTelemetryCardWidget* Card =
                ExistingCard != nullptr ? ExistingCard->Get() : nullptr;
            if (Card == nullptr)
            {
                Card = CreateWidget<UTGVisualizationTelemetryCardWidget>(
                    GetOwningPlayer(),
                    CardClass);
                if (Card != nullptr)
                {
                    VBOX_TelemetryCards->AddChild(Card);
                    TelemetryCards.Add(Key, Card);
                }
            }
            if (Card != nullptr)
            {
                Card->SetTelemetryValue(
                    Key,
                    Value,
                    TGVisualizationHudPrivate::ResolveTelemetryAccent(Key));
                Card->SetVisibility(ESlateVisibility::Visible);
            }
        }
    }
}

void UTGVisualizationHudWidget::HandlePlaybackTimeChanged(
    double ElapsedSimulationSeconds,
    double EphemerisTimeTdbSeconds,
    double NormalizedTime)
{
    RefreshDisplayedValues();
}

void UTGVisualizationHudWidget::HandleConstellationSearchChanged(
    const FText& Text)
{
    ApplyConstellationSearchFilter(Text.ToString());
}

void UTGVisualizationHudWidget::HandleVectorSearchChanged(const FText& Text)
{
    ApplyVectorSearchFilter(Text.ToString());
}

void UTGVisualizationHudWidget::HandleConstellationsTabClicked()
{
    if (SWITCHER_Display != nullptr)
    {
        SWITCHER_Display->SetActiveWidgetIndex(0);
    }
}

void UTGVisualizationHudWidget::HandleVectorsTabClicked()
{
    if (SWITCHER_Display != nullptr)
    {
        SWITCHER_Display->SetActiveWidgetIndex(1);
    }
}

void UTGVisualizationHudWidget::HandleOverviewFocusSpacecraft()
{
    if (GRAPH_SolarSystem != nullptr)
    {
        GRAPH_SolarSystem->FocusSpacecraftPath();
    }
}

void UTGVisualizationHudWidget::HandleOverviewReset()
{
    if (GRAPH_SolarSystem != nullptr)
    {
        GRAPH_SolarSystem->ResetSolarSystemView();
    }
}

void UTGVisualizationHudWidget::TogglePauseMenu()
{
    if (!IsValid(PlaybackActor))
    {
        return;
    }

    bPauseMenuOpen = !bPauseMenuOpen;
    if (bPauseMenuOpen)
    {
        bResumeAfterMenu = PlaybackActor->IsPlaying();
        PlaybackActor->Pause();
        OVERLAY_PauseMenu->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        OVERLAY_PauseMenu->SetVisibility(ESlateVisibility::Collapsed);
        if (bResumeAfterMenu)
        {
            PlaybackActor->Play();
        }
        bResumeAfterMenu = false;
    }
    RefreshDisplayedValues();
}

bool UTGVisualizationHudWidget::IsPauseMenuOpen() const
{
    return bPauseMenuOpen;
}

bool UTGVisualizationHudWidget::IsPointerOverControls() const
{
    const auto IsHoveredAndVisible = [](const UWidget* Widget)
    {
        return Widget != nullptr &&
            Widget->GetVisibility() != ESlateVisibility::Collapsed &&
            Widget->GetVisibility() != ESlateVisibility::Hidden &&
            Widget->IsHovered();
    };

    return (WORKSPACE_Dock != nullptr &&
            WORKSPACE_Dock->IsPointerOverInteractiveArea()) ||
        IsHoveredAndVisible(PANEL_Display) ||
        IsHoveredAndVisible(PANEL_Telemetry) ||
        IsHoveredAndVisible(PANEL_Overview) ||
        IsHoveredAndVisible(OVERLAY_PauseMenu);
}

void UTGVisualizationHudWidget::HandleStayInVisualization()
{
    if (bPauseMenuOpen)
    {
        TogglePauseMenu();
    }
}

void UTGVisualizationHudWidget::HandleReturnToConfiguration()
{
    UGameInstance* GameInstance = GetGameInstance();
    UTGSimulationRunSubsystem* RunSubsystem =
        GameInstance != nullptr
            ? GameInstance->GetSubsystem<UTGSimulationRunSubsystem>()
            : nullptr;
    FText Error;
    if (RunSubsystem == nullptr ||
        !RunSubsystem->ReturnToConfigurationLevel(Error))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Could not return to configuration: %s"),
            *Error.ToString());
    }
}

void UTGVisualizationHudWidget::HandleGoToScenarioLibrary()
{
    UGameInstance* GameInstance = GetGameInstance();
    UTGSimulationRunSubsystem* RunSubsystem =
        GameInstance != nullptr
            ? GameInstance->GetSubsystem<UTGSimulationRunSubsystem>()
            : nullptr;
    FText Error;
    if (RunSubsystem == nullptr ||
        !RunSubsystem->ReturnToScenarioLibraryLevel(Error))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Could not open the scenario library: %s"),
            *Error.ToString());
    }
}

void UTGVisualizationHudWidget::HandleReturnToMainMenu()
{
    UGameInstance* GameInstance = GetGameInstance();
    UTGSimulationRunSubsystem* RunSubsystem =
        GameInstance != nullptr
            ? GameInstance->GetSubsystem<UTGSimulationRunSubsystem>()
            : nullptr;
    FText Error;
    if (RunSubsystem == nullptr ||
        !RunSubsystem->ReturnToMainMenuLevel(Error))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Could not return to the main menu: %s"),
            *Error.ToString());
    }
}
