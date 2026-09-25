// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/MainMenu/TGScenarioLibraryWidgetBase.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Simulation/TGSimulationRunSubsystem.h"
#include "Simulation/TGSimulationSubsystem.h"
#include "Styling/SlateTypes.h"
#include "TGSimulationSaveGame.h"
#include "UI/MainMenu/TGScenarioLibraryEntryWidgetBase.h"
#include "UI/MainMenu/TGScenarioLibraryTypes.h"
#include "UI/Theme/TGUiTheme.h"
#include "TimerManager.h"

namespace TGScenarioLibraryPrivate
{
    const FString SortLastModified = TEXT("Last Modified");
    const FString SortCreated = TEXT("Created");
    const FString SortName = TEXT("Name");
    const FString SortResultStatus = TEXT("Result Status");

    FText FormatLocalDateTime(const FDateTime& UtcDateTime)
    {
        if (UtcDateTime.GetTicks() <= 0)
        {
            return FText::FromString(TEXT("Not available"));
        }

        // An empty time-zone argument uses the operating system's local zone.
        return FText::AsDateTime(
            UtcDateTime,
            EDateTimeStyle::Medium,
            EDateTimeStyle::Short);
    }

    FString ScenarioNameFromRecord(const FTGSavedScenarioRecord& Record)
    {
        FString Name =
            Record.Scenario.ScenarioAndSolver.ScenarioName.TrimStartAndEnd();
        if (Name.IsEmpty())
        {
            Name = TEXT("Untitled Scenario");
        }
        return Name;
    }
}

UTGScenarioLibraryListView::UTGScenarioLibraryListView(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ScrollBarStyle = TGUiTheme::MakeScrollBarStyle();
    ShadowBrushStyle = TGUiTheme::MakeScrollBoxStyle();
    bEnableShadowBrush = false;
    ConsumeMouseWheel = EConsumeMouseWheel::Always;
    bClearSelectionOnClick = true;
    bIsFocusable = true;
    bEnableScrollAnimation = true;
    WheelScrollMultiplier = 4.0f;
    SetScrollBarPadding(FMargin(6.0f, 4.0f, 4.0f, 4.0f));
    SetVerticalEntrySpacing(2.0f);
}

bool UTGScenarioLibraryWidgetBase::Initialize()
{
    if (!Super::Initialize())
    {
        return false;
    }

    ApplyPrepassSafeStyles();

    return true;
}

void UTGScenarioLibraryWidgetBase::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    ApplyPrepassSafeStyles();
}

void UTGScenarioLibraryWidgetBase::OnWidgetRebuilt()
{
    Super::OnWidgetRebuilt();

    // Blueprint construction can restore serialized widget properties after
    // SynchronizeProperties. Apply the safe style to the completed widget tree.
    ApplyPrepassSafeStyles();
}

void UTGScenarioLibraryWidgetBase::ApplyPrepassSafeStyles()
{
    if (INPUT_Search == nullptr && WidgetTree != nullptr)
    {
        INPUT_Search = Cast<UEditableTextBox>(
            WidgetTree->FindWidget(TEXT("INPUT_Search")));
    }

    // Clipboard-created editable text boxes can retain an editor-only default
    // font until their first Slate build. Replace it before the first prepass.
    if (INPUT_Search != nullptr)
    {
        INPUT_Search->SetWidgetStyle(
            TGUiTheme::GetEditableTextBoxStyle());
    }
}

void UTGScenarioLibraryWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();
    ConfigureControls();
    RefreshScenarioList();
}

void UTGScenarioLibraryWidgetBase::NativeDestruct()
{
    ActiveRenameEntry.Reset();
    LIST_Scenarios->OnItemSelectionChanged().RemoveAll(this);
    LIST_Scenarios->OnItemClicked().RemoveAll(this);
    LIST_Scenarios->OnItemDoubleClicked().RemoveAll(this);
    LIST_Recent->OnItemSelectionChanged().RemoveAll(this);
    LIST_Recent->OnItemDoubleClicked().RemoveAll(this);

    INPUT_Search->OnTextChanged.RemoveDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleSearchTextChanged);
    INPUT_SortBy->OnSelectionChanged.RemoveDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleSortFieldChanged);

    BTN_SortDirection->OnClicked.RemoveDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleSortDirectionClicked);
    BTN_OpenScenario->OnClicked.RemoveDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleOpenScenarioClicked);
    BTN_OpenVisualization->OnClicked.RemoveDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleOpenVisualizationClicked);
    BTN_ContinueFromEnd->OnClicked.RemoveDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleContinueClicked);
    BTN_DeleteSelected->OnClicked.RemoveDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleDeleteClicked);
    BTN_ConfirmDelete->OnClicked.RemoveDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleConfirmDeleteClicked);
    BTN_CancelDelete->OnClicked.RemoveDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleCancelDeleteClicked);
    BTN_Refresh->OnClicked.RemoveDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleRefreshClicked);
    BTN_Back->OnClicked.RemoveDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleBackClicked);

    Super::NativeDestruct();
}

FReply UTGScenarioLibraryWidgetBase::NativeOnMouseButtonDown(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        if (UTGScenarioLibraryEntryWidgetBase* ActiveEntry =
                ActiveRenameEntry.Get())
        {
            ActiveRenameEntry.Reset();
            ActiveEntry->CommitInlineRename();
        }
        bUpdatingSelection = true;
        LIST_Scenarios->ClearSelection();
        LIST_Recent->ClearSelection();
        bUpdatingSelection = false;
        CloseAllInlineRenameEditors();
        PendingRenameItem.Reset();
        PendingRenameClickSeconds = -1.0;
        UpdateActionState();
        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UTGScenarioLibraryWidgetBase::ConfigureControls()
{
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();

    const auto ConfigureHeaderColumn = [this](
        const FName SizeBoxName,
        const float Width)
    {
        if (USizeBox* SizeBox = Cast<USizeBox>(
                WidgetTree->FindWidget(SizeBoxName)))
        {
            SizeBox->SetWidthOverride(Width);
            if (USizeBoxSlot* ColumnSlot = Cast<USizeBoxSlot>(
                    SizeBox->GetContentSlot()))
            {
                ColumnSlot->SetHorizontalAlignment(HAlign_Left);
            }
        }
    };
    ConfigureHeaderColumn(TEXT("SIZE_RecentColumnResult"), 210.0f);
    ConfigureHeaderColumn(TEXT("SIZE_RecentColumnModified"), 190.0f);
    ConfigureHeaderColumn(TEXT("SIZE_RecentColumnCreated"), 190.0f);
    ConfigureHeaderColumn(TEXT("SIZE_ColumnResult"), 210.0f);
    ConfigureHeaderColumn(TEXT("SIZE_ColumnModified"), 190.0f);
    ConfigureHeaderColumn(TEXT("SIZE_ColumnCreated"), 190.0f);

    for (const FName TextName : {
             FName(TEXT("TXT_RecentColumnScenario")),
             FName(TEXT("TXT_RecentColumnResult")),
             FName(TEXT("TXT_RecentColumnModified")),
             FName(TEXT("TXT_RecentColumnCreated")),
             FName(TEXT("TXT_ColumnScenario")),
             FName(TEXT("TXT_ColumnResult")),
             FName(TEXT("TXT_ColumnModified")),
             FName(TEXT("TXT_ColumnCreated"))})
    {
        if (UTextBlock* HeaderText = Cast<UTextBlock>(
                WidgetTree->FindWidget(TextName)))
        {
            HeaderText->SetJustification(ETextJustify::Left);
        }
    }

    for (const FName HeaderName : {
             FName(TEXT("BORDER_RecentColumnHeader")),
             FName(TEXT("BORDER_ColumnHeader"))})
    {
        if (UBorder* Header = Cast<UBorder>(
                WidgetTree->FindWidget(HeaderName)))
        {
            Header->SetPadding(FMargin(16.0f, 8.0f, 34.0f, 8.0f));
        }
    }

    INPUT_Search->SetWidgetStyle(
        TGUiTheme::GetEditableTextBoxStyle());
    INPUT_Search->SetHintText(FText::FromString(TEXT("Search scenarios")));
    INPUT_Search->SetText(FText::GetEmpty());
    INPUT_Search->OnTextChanged.RemoveDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleSearchTextChanged);
    INPUT_Search->OnTextChanged.AddDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleSearchTextChanged);

    INPUT_SortBy->SetWidgetStyle(TGUiTheme::MakeComboBoxStyle());
    INPUT_SortBy->SetItemStyle(TGUiTheme::MakeTableRowStyle());
    INPUT_SortBy->SetContentPadding(FMargin(10.0f, 7.0f));
    INPUT_SortBy->SetMaxListHeight(320.0f);
    INPUT_SortBy->OnSelectionChanged.RemoveDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleSortFieldChanged);

    bConfiguringControls = true;
    INPUT_SortBy->ClearOptions();
    INPUT_SortBy->AddOption(TGScenarioLibraryPrivate::SortLastModified);
    INPUT_SortBy->AddOption(TGScenarioLibraryPrivate::SortCreated);
    INPUT_SortBy->AddOption(TGScenarioLibraryPrivate::SortName);
    INPUT_SortBy->AddOption(TGScenarioLibraryPrivate::SortResultStatus);
    INPUT_SortBy->SetSelectedOption(
        TGScenarioLibraryPrivate::SortLastModified);
    bConfiguringControls = false;

    INPUT_SortBy->OnSelectionChanged.AddDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleSortFieldChanged);

    BTN_OpenScenario->SetStyle(
        TGUiTheme::GetButtonStyle(ETGUiButtonStyle::Secondary));
    BTN_OpenVisualization->SetStyle(
        TGUiTheme::GetButtonStyle(ETGUiButtonStyle::Secondary));
    BTN_ContinueFromEnd->SetStyle(
        TGUiTheme::GetButtonStyle(ETGUiButtonStyle::Secondary));
    BTN_DeleteSelected->SetStyle(
        TGUiTheme::MakeButtonStyle(ETGUiButtonStyle::Destructive));
    BTN_Refresh->SetStyle(
        TGUiTheme::GetButtonStyle(ETGUiButtonStyle::Secondary));
    BTN_Back->SetStyle(
        TGUiTheme::GetButtonStyle(ETGUiButtonStyle::Secondary));
    BTN_SortDirection->SetStyle(
        TGUiTheme::MakeButtonStyle(ETGUiButtonStyle::Secondary));
    BTN_ConfirmDelete->SetStyle(
        TGUiTheme::MakeButtonStyle(ETGUiButtonStyle::Destructive));
    BTN_CancelDelete->SetStyle(
        TGUiTheme::MakeButtonStyle(ETGUiButtonStyle::Secondary));

    BTN_SortDirection->OnClicked.AddUniqueDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleSortDirectionClicked);
    BTN_OpenScenario->OnClicked.AddUniqueDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleOpenScenarioClicked);
    BTN_OpenVisualization->OnClicked.AddUniqueDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleOpenVisualizationClicked);
    BTN_ContinueFromEnd->OnClicked.AddUniqueDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleContinueClicked);
    BTN_DeleteSelected->OnClicked.AddUniqueDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleDeleteClicked);
    BTN_ConfirmDelete->OnClicked.AddUniqueDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleConfirmDeleteClicked);
    BTN_CancelDelete->OnClicked.AddUniqueDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleCancelDeleteClicked);
    BTN_Refresh->OnClicked.AddUniqueDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleRefreshClicked);
    BTN_Back->OnClicked.AddUniqueDynamic(
        this,
        &UTGScenarioLibraryWidgetBase::HandleBackClicked);

    LIST_Scenarios->SetSelectionMode(ESelectionMode::Multi);
    LIST_Scenarios->SetAllowOverScroll(false);
    LIST_Scenarios->SetWheelScrollMultiplier(4.0f);
    LIST_Scenarios->SetScrollbarVisibility(ESlateVisibility::Visible);
    LIST_Scenarios->SetToolTipText(FText::GetEmpty());
    LIST_Scenarios->OnItemSelectionChanged().RemoveAll(this);
    LIST_Scenarios->OnItemSelectionChanged().AddUObject(
        this,
        &UTGScenarioLibraryWidgetBase::HandleCatalogSelectionChanged);
    LIST_Scenarios->OnItemClicked().RemoveAll(this);
    LIST_Scenarios->OnItemClicked().AddUObject(
        this,
        &UTGScenarioLibraryWidgetBase::HandleCatalogItemClicked);
    LIST_Scenarios->OnItemDoubleClicked().RemoveAll(this);
    LIST_Scenarios->OnItemDoubleClicked().AddUObject(
        this,
        &UTGScenarioLibraryWidgetBase::HandleItemDoubleClicked);

    LIST_Recent->SetSelectionMode(ESelectionMode::Single);
    LIST_Recent->SetAllowOverScroll(false);
    LIST_Recent->SetWheelScrollMultiplier(4.0f);
    LIST_Recent->SetScrollbarVisibility(ESlateVisibility::Hidden);
    LIST_Recent->SetToolTipText(FText::GetEmpty());
    LIST_Recent->OnItemSelectionChanged().RemoveAll(this);
    LIST_Recent->OnItemSelectionChanged().AddUObject(
        this,
        &UTGScenarioLibraryWidgetBase::HandleRecentSelectionChanged);
    LIST_Recent->OnItemDoubleClicked().RemoveAll(this);
    LIST_Recent->OnItemDoubleClicked().AddUObject(
        this,
        &UTGScenarioLibraryWidgetBase::HandleItemDoubleClicked);

    TGUiTheme::ApplyTextStyle(
        *TXT_SortDirection,
        ETGUiTextStyle::FieldLabel,
        Palette.TextPrimary);
    TGUiTheme::ApplyTextStyle(
        *TXT_ListSummary,
        ETGUiTextStyle::Caption,
        Palette.TextMuted);
    TGUiTheme::ApplyTextStyle(
        *TXT_RecentEmpty,
        ETGUiTextStyle::Body,
        Palette.TextMuted);
    TGUiTheme::ApplyTextStyle(
        *TXT_LibraryEmpty,
        ETGUiTextStyle::Body,
        Palette.TextMuted);
    TGUiTheme::ApplyTextStyle(
        *TXT_Status,
        ETGUiTextStyle::Body,
        Palette.TextSecondary);
    TGUiTheme::ApplyTextStyle(
        *TXT_DeleteConfirmationTitle,
        ETGUiTextStyle::PanelTitle,
        Palette.TextPrimary);
    TGUiTheme::ApplyTextStyle(
        *TXT_DeleteConfirmationBody,
        ETGUiTextStyle::Body,
        Palette.TextSecondary);

    BORDER_DeleteConfirmation->SetBrushColor(Palette.Overlay);
    BORDER_DeleteDialog->SetBrushColor(Palette.Panel);
    SetDeleteConfirmationVisible(false);
    ClearStatusMessage();
    UpdateSortDirectionText();
}

void UTGScenarioLibraryWidgetBase::RefreshScenarioList()
{
    ClearStatusMessage();
    ScenarioItems.Reset();

    UTGSimulationSubsystem* SimulationSubsystem =
        GetSimulationSubsystem();
    if (SimulationSubsystem == nullptr)
    {
        LIST_Scenarios->ClearListItems();
        LIST_Recent->ClearListItems();
        SetStatusMessage(
            FText::FromString(TEXT("The scenario library is unavailable.")),
            true);
        UpdateActionState();
        return;
    }

    const UTGSimulationSaveGame* Library =
        SimulationSubsystem->GetLibrary();
    const TArray<FTGSavedScenarioRecord> Records =
        SimulationSubsystem->GetSavedScenarios();

    ScenarioItems.Reserve(Records.Num());
    for (const FTGSavedScenarioRecord& Record : Records)
    {
        UTGScenarioLibraryItem* Item =
            NewObject<UTGScenarioLibraryItem>(this);
        Item->Record = Record;
        Item->ScenarioName = FText::FromString(
            TGScenarioLibraryPrivate::ScenarioNameFromRecord(Record));
        Item->LastModified =
            TGScenarioLibraryPrivate::FormatLocalDateTime(
                Record.LastModifiedUtc);
        Item->Created = TGScenarioLibraryPrivate::FormatLocalDateTime(
            Record.CreatedUtc);
        Item->bCanVisualize =
            SimulationSubsystem->HasVisualizableRunForScenario(
                Record.ScenarioId);
        Item->bCanContinue =
            SimulationSubsystem->CanContinueScenarioFromLatestRun(
                Record.ScenarioId);

        if (Item->bCanContinue)
        {
            Item->Status =
                ETGScenarioLibraryResultStatus::ResultAndContinuation;
            Item->ResultStatus = FText::FromString(
                TEXT("Result available"));
            Item->ResultStatusDetail = FText::FromString(TEXT(
                "A completed result and final-state snapshot are available."));
        }
        else if (Item->bCanVisualize)
        {
            Item->Status =
                ETGScenarioLibraryResultStatus::ResultWithoutContinuation;
            Item->ResultStatus = FText::FromString(
                TEXT("Result, no continuation"));
            Item->ResultStatusDetail = FText::FromString(TEXT(
                "A completed result is available, but it has no valid final-state snapshot for continuation."));
        }
        else
        {
            Item->Status = ETGScenarioLibraryResultStatus::ScenarioOnly;
            Item->ResultStatus = FText::FromString(TEXT("No result"));
            Item->ResultStatusDetail = FText::FromString(TEXT(
                "No completed result is available for this scenario."));
        }

        Item->LatestActivityUtc = Record.LastAccessedUtc;
        Item->OnRenameRequested.AddUObject(
            this,
            &UTGScenarioLibraryWidgetBase::HandleRenameRequested);
        if (Library != nullptr)
        {
            for (const FTGSavedSimulationRun& Run : Library->SavedRuns)
            {
                if (Run.SourceScenarioId == Record.ScenarioId &&
                    Run.CreatedUtc > Item->LatestActivityUtc)
                {
                    Item->LatestActivityUtc = Run.CreatedUtc;
                }
            }
        }

        ScenarioItems.Add(Item);
    }

    RebuildRecentList();
    RebuildVisibleLists();
}

void UTGScenarioLibraryWidgetBase::RebuildVisibleLists()
{
    TArray<TObjectPtr<UTGScenarioLibraryItem>> VisibleItems;
    VisibleItems.Reserve(ScenarioItems.Num());

    for (UTGScenarioLibraryItem* Item : ScenarioItems)
    {
        if (Item == nullptr)
        {
            continue;
        }

        if (SearchText.IsEmpty() ||
            Item->ScenarioName.ToString().Contains(
                SearchText,
                ESearchCase::IgnoreCase) ||
            Item->ResultStatus.ToString().Contains(
                SearchText,
                ESearchCase::IgnoreCase))
        {
            VisibleItems.Add(Item);
        }
    }

    VisibleItems.Sort(
        [this](
            const UTGScenarioLibraryItem& Left,
            const UTGScenarioLibraryItem& Right)
        {
            int32 Comparison = 0;

            switch (SortField)
            {
                case ESortField::Created:
                    Comparison = Left.Record.CreatedUtc == Right.Record.CreatedUtc
                        ? 0
                        : (Left.Record.CreatedUtc < Right.Record.CreatedUtc
                            ? -1
                            : 1);
                    break;

                case ESortField::Name:
                    Comparison = Left.ScenarioName.ToString().Compare(
                        Right.ScenarioName.ToString(),
                        ESearchCase::IgnoreCase);
                    break;

                case ESortField::ResultStatus:
                    Comparison = static_cast<int32>(Left.Status) -
                        static_cast<int32>(Right.Status);
                    break;

                case ESortField::LastModified:
                default:
                    Comparison =
                        Left.Record.LastModifiedUtc == Right.Record.LastModifiedUtc
                        ? 0
                        : (Left.Record.LastModifiedUtc <
                                Right.Record.LastModifiedUtc
                            ? -1
                            : 1);
                    break;
            }

            if (Comparison == 0)
            {
                Comparison = Left.ScenarioName.ToString().Compare(
                    Right.ScenarioName.ToString(),
                    ESearchCase::IgnoreCase);
            }
            if (Comparison == 0)
            {
                Comparison = Left.Record.ScenarioId.ToString().Compare(
                    Right.Record.ScenarioId.ToString(),
                    ESearchCase::CaseSensitive);
            }

            return bSortDescending
                ? Comparison > 0
                : Comparison < 0;
        });

    bUpdatingSelection = true;
    LIST_Scenarios->SetListItems(VisibleItems);
    LIST_Scenarios->ClearSelection();
    bUpdatingSelection = false;

    const bool bNoVisibleItems = VisibleItems.IsEmpty();
    TXT_LibraryEmpty->SetVisibility(
        bNoVisibleItems
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
    TXT_LibraryEmpty->SetText(
        ScenarioItems.IsEmpty()
            ? FText::FromString(TEXT("No saved scenarios yet."))
            : FText::Format(
                FText::FromString(TEXT("No scenarios match \"{0}\".")),
                FText::FromString(SearchText)));

    UpdateActionState();
}

void UTGScenarioLibraryWidgetBase::RebuildRecentList()
{
    TArray<TObjectPtr<UTGScenarioLibraryItem>> RecentItems;
    for (UTGScenarioLibraryItem* Item : ScenarioItems)
    {
        if (Item != nullptr && Item->LatestActivityUtc.GetTicks() > 0)
        {
            RecentItems.Add(Item);
        }
    }

    RecentItems.Sort(
        [](const UTGScenarioLibraryItem& Left,
           const UTGScenarioLibraryItem& Right)
        {
            if (Left.LatestActivityUtc == Right.LatestActivityUtc)
            {
                return Left.ScenarioName.ToString().Compare(
                    Right.ScenarioName.ToString(),
                    ESearchCase::IgnoreCase) < 0;
            }
            return Left.LatestActivityUtc > Right.LatestActivityUtc;
        });
    if (RecentItems.Num() > 3)
    {
        RecentItems.SetNum(3);
    }

    bUpdatingSelection = true;
    LIST_Recent->SetListItems(RecentItems);
    LIST_Recent->ClearSelection();
    bUpdatingSelection = false;

    TXT_RecentEmpty->SetVisibility(
        RecentItems.IsEmpty()
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
}

UTGSimulationSubsystem*
UTGScenarioLibraryWidgetBase::GetSimulationSubsystem() const
{
    const UGameInstance* GameInstance = GetGameInstance();
    return GameInstance != nullptr
        ? GameInstance->GetSubsystem<UTGSimulationSubsystem>()
        : nullptr;
}

TArray<UTGScenarioLibraryItem*>
UTGScenarioLibraryWidgetBase::GetSelectedItems() const
{
    TArray<UObject*> SelectedObjects;
    LIST_Scenarios->GetSelectedItems(SelectedObjects);
    if (SelectedObjects.IsEmpty())
    {
        LIST_Recent->GetSelectedItems(SelectedObjects);
    }

    TArray<UTGScenarioLibraryItem*> Result;
    Result.Reserve(SelectedObjects.Num());
    for (UObject* SelectedObject : SelectedObjects)
    {
        if (UTGScenarioLibraryItem* Item =
                Cast<UTGScenarioLibraryItem>(SelectedObject))
        {
            Result.Add(Item);
        }
    }
    return Result;
}

UTGScenarioLibraryItem*
UTGScenarioLibraryWidgetBase::GetSingleSelectedItem() const
{
    const TArray<UTGScenarioLibraryItem*> SelectedItems =
        GetSelectedItems();
    return SelectedItems.Num() == 1 ? SelectedItems[0] : nullptr;
}

void UTGScenarioLibraryWidgetBase::HandleCatalogSelectionChanged(
    UObject* SelectedItem)
{
    if (bUpdatingSelection)
    {
        return;
    }

    TArray<UObject*> CatalogSelection;
    LIST_Scenarios->GetSelectedItems(CatalogSelection);
    if (!CatalogSelection.IsEmpty())
    {
        bUpdatingSelection = true;
        LIST_Recent->ClearSelection();
        bUpdatingSelection = false;
    }
    UpdateActionState();
}

void UTGScenarioLibraryWidgetBase::HandleRecentSelectionChanged(
    UObject* SelectedItem)
{
    if (bUpdatingSelection)
    {
        return;
    }

    TArray<UObject*> RecentSelection;
    LIST_Recent->GetSelectedItems(RecentSelection);
    if (!RecentSelection.IsEmpty())
    {
        bUpdatingSelection = true;
        LIST_Scenarios->ClearSelection();
        bUpdatingSelection = false;
    }
    UpdateActionState();
}

void UTGScenarioLibraryWidgetBase::HandleCatalogItemClicked(
    UObject* ClickedItem)
{
    UTGScenarioLibraryItem* Item =
        Cast<UTGScenarioLibraryItem>(ClickedItem);
    if (Item == nullptr)
    {
        PendingRenameItem.Reset();
        PendingRenameClickSeconds = -1.0;
        return;
    }

    if (UTGScenarioLibraryEntryWidgetBase* ActiveEntry =
            ActiveRenameEntry.Get())
    {
        ActiveRenameEntry.Reset();
        ActiveEntry->CommitInlineRename();
    }
    CloseAllInlineRenameEditors();

    const FModifierKeysState Modifiers =
        FSlateApplication::Get().GetModifierKeys();

    // Slate's multi-selection state can survive a plain click when the row
    // was already selected during mouse-down. Enforce desktop-list semantics
    // after the click: plain click selects only this row, while Ctrl and Shift
    // retain Slate's native toggle and range behavior.
    if (!Modifiers.IsControlDown() && !Modifiers.IsShiftDown())
    {
        bUpdatingSelection = true;
        LIST_Scenarios->ClearSelection();
        LIST_Scenarios->SetItemSelection(Item, true);
        bUpdatingSelection = false;
        UpdateActionState();
    }
    CloseAllInlineRenameEditors();

    const TArray<UTGScenarioLibraryItem*> SelectedItems =
        GetSelectedItems();
    const double NowSeconds = FPlatformTime::Seconds();
    const double ElapsedSeconds =
        NowSeconds - PendingRenameClickSeconds;
    constexpr double RenameDelaySeconds = 0.5;
    constexpr double RenameWindowSeconds = 1.8;

    const bool bCanBeginRename =
        !Modifiers.IsControlDown() &&
        !Modifiers.IsShiftDown() &&
        SelectedItems.Num() == 1 &&
        SelectedItems[0] == Item &&
        PendingRenameItem.Get() == Item &&
        ElapsedSeconds >= RenameDelaySeconds &&
        ElapsedSeconds <= RenameWindowSeconds;

    if (bCanBeginRename)
    {
        if (UTGScenarioLibraryEntryWidgetBase* Entry =
                LIST_Scenarios->GetEntryWidgetFromItem<
                    UTGScenarioLibraryEntryWidgetBase>(Item))
        {
            Entry->BeginInlineRename();
            ActiveRenameEntry = Entry;
        }
        PendingRenameItem.Reset();
        PendingRenameClickSeconds = -1.0;
        return;
    }

    PendingRenameItem = Item;
    PendingRenameClickSeconds = NowSeconds;
}

void UTGScenarioLibraryWidgetBase::HandleItemDoubleClicked(
    UObject* SelectedItem)
{
    PendingRenameItem.Reset();
    PendingRenameClickSeconds = -1.0;
    HandleOpenScenarioClicked();
}

void UTGScenarioLibraryWidgetBase::HandleRenameRequested(
    UTGScenarioLibraryItem* Item,
    const FString& NewName)
{
    ActiveRenameEntry.Reset();
    UTGSimulationSubsystem* SimulationSubsystem =
        GetSimulationSubsystem();
    if (Item == nullptr || SimulationSubsystem == nullptr)
    {
        SetStatusMessage(
            FText::FromString(TEXT("The scenario library is unavailable.")),
            true);
        return;
    }

    FText Error;
    const FString TrimmedName = NewName.TrimStartAndEnd();
    const FGuid RenamedScenarioId = Item->Record.ScenarioId;
    if (!SimulationSubsystem->RenameSavedScenario(
            RenamedScenarioId,
            TrimmedName,
            Error))
    {
        CloseAllInlineRenameEditors();
        SetStatusMessage(
            Error.IsEmpty()
                ? FText::FromString(TEXT(
                    "The scenario could not be renamed."))
                : Error,
            true);
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateWeakLambda(
                this,
                [this]()
                {
                    FinalizeRenameRefresh();
                }));
        return;
    }

    FinalizeRenameRefresh();
}

void UTGScenarioLibraryWidgetBase::CloseAllInlineRenameEditors()
{
    const auto RestoreEntries = [](
        UTGScenarioLibraryListView* ListView)
    {
        if (ListView == nullptr)
        {
            return;
        }
        for (UUserWidget* EntryWidget :
             ListView->GetDisplayedEntryWidgets())
        {
            if (UTGScenarioLibraryEntryWidgetBase* Entry =
                    Cast<UTGScenarioLibraryEntryWidgetBase>(EntryWidget))
            {
                Entry->RestoreReadOnlyAppearance();
            }
        }
    };

    RestoreEntries(LIST_Scenarios);
    RestoreEntries(LIST_Recent);
}

void UTGScenarioLibraryWidgetBase::FinalizeRenameRefresh()
{
    RefreshScenarioList();
    CloseAllInlineRenameEditors();
}

void UTGScenarioLibraryWidgetBase::HandleSearchTextChanged(
    const FText& Text)
{
    SearchText = Text.ToString().TrimStartAndEnd();
    RebuildVisibleLists();
}

void UTGScenarioLibraryWidgetBase::HandleSortFieldChanged(
    FString SelectedItem,
    ESelectInfo::Type SelectionType)
{
    if (bConfiguringControls)
    {
        return;
    }

    if (SelectedItem == TGScenarioLibraryPrivate::SortCreated)
    {
        SortField = ESortField::Created;
        bSortDescending = true;
    }
    else if (SelectedItem == TGScenarioLibraryPrivate::SortName)
    {
        SortField = ESortField::Name;
        bSortDescending = false;
    }
    else if (SelectedItem == TGScenarioLibraryPrivate::SortResultStatus)
    {
        SortField = ESortField::ResultStatus;
        bSortDescending = false;
    }
    else
    {
        SortField = ESortField::LastModified;
        bSortDescending = true;
    }

    UpdateSortDirectionText();
    RebuildVisibleLists();
}

void UTGScenarioLibraryWidgetBase::HandleSortDirectionClicked()
{
    bSortDescending = !bSortDescending;
    UpdateSortDirectionText();
    RebuildVisibleLists();
}

void UTGScenarioLibraryWidgetBase::UpdateSortDirectionText()
{
    const bool bDateSort =
        SortField == ESortField::LastModified ||
        SortField == ESortField::Created;
    TXT_SortDirection->SetText(FText::FromString(
        bDateSort
            ? (bSortDescending ? TEXT("Newest first") : TEXT("Oldest first"))
            : (bSortDescending ? TEXT("Z to A") : TEXT("A to Z"))));
    BTN_SortDirection->SetToolTipText(FText::FromString(
        TEXT("Reverse the current sort order.")));
}

void UTGScenarioLibraryWidgetBase::UpdateActionState()
{
    const TArray<UTGScenarioLibraryItem*> SelectedItems =
        GetSelectedItems();
    UTGScenarioLibraryItem* SingleItem =
        SelectedItems.Num() == 1 ? SelectedItems[0] : nullptr;

    const bool bSingle = SingleItem != nullptr;
    BTN_OpenScenario->SetIsEnabled(bSingle);
    BTN_OpenVisualization->SetIsEnabled(
        bSingle && SingleItem->bCanVisualize);
    BTN_ContinueFromEnd->SetIsEnabled(
        bSingle && SingleItem->bCanContinue);
    BTN_DeleteSelected->SetIsEnabled(!SelectedItems.IsEmpty());

    if (SelectedItems.IsEmpty())
    {
        BTN_OpenScenario->SetToolTipText(FText::FromString(
            TEXT("Select one scenario to open it for editing.")));
        BTN_OpenVisualization->SetToolTipText(FText::FromString(
            TEXT("Select one scenario with an available completed result.")));
        BTN_ContinueFromEnd->SetToolTipText(FText::FromString(
            TEXT("Select one scenario with an available final-state snapshot.")));
        BTN_DeleteSelected->SetToolTipText(FText::FromString(
            TEXT("Select one or more scenarios to delete.")));
    }
    else if (!bSingle)
    {
        BTN_OpenScenario->SetToolTipText(FText::FromString(
            TEXT("Open Scenario requires exactly one selected row.")));
        BTN_OpenVisualization->SetToolTipText(FText::FromString(
            TEXT("Open Visualization requires exactly one selected row.")));
        BTN_ContinueFromEnd->SetToolTipText(FText::FromString(
            TEXT("Continue from End of Propagation requires exactly one selected row.")));
        BTN_DeleteSelected->SetToolTipText(FText::Format(
            FText::FromString(TEXT("Delete {0} selected scenarios.")),
            FText::AsNumber(SelectedItems.Num())));
    }
    else
    {
        BTN_OpenScenario->SetToolTipText(FText::FromString(
            TEXT("Open the selected saved scenario for editing in Simulation Configuration.")));
        BTN_OpenVisualization->SetToolTipText(
            SingleItem->bCanVisualize
                ? FText::FromString(TEXT(
                    "Open the latest completed result in Visualization."))
                : FText::FromString(TEXT(
                    "This scenario has no completed result available for visualization.")));
        BTN_ContinueFromEnd->SetToolTipText(
            SingleItem->bCanContinue
                ? FText::FromString(TEXT(
                    "Create a new editable scenario from the latest final state."))
                : FText::FromString(TEXT(
                    "The latest result has no valid final-state snapshot for continuation.")));
        BTN_DeleteSelected->SetToolTipText(FText::FromString(
            TEXT("Delete the selected scenario from the library.")));
    }

    const int32 VisibleCount = LIST_Scenarios->GetNumItems();
    if (!SearchText.IsEmpty())
    {
        TXT_ListSummary->SetText(FText::Format(
            FText::FromString(TEXT("{0} of {1} scenarios | {2} selected")),
            FText::AsNumber(VisibleCount),
            FText::AsNumber(ScenarioItems.Num()),
            FText::AsNumber(SelectedItems.Num())));
    }
    else
    {
        TXT_ListSummary->SetText(FText::Format(
            FText::FromString(TEXT("{0} scenarios | {1} selected")),
            FText::AsNumber(VisibleCount),
            FText::AsNumber(SelectedItems.Num())));
    }
}

void UTGScenarioLibraryWidgetBase::HandleOpenScenarioClicked()
{
    UTGScenarioLibraryItem* Item = GetSingleSelectedItem();
    UTGSimulationSubsystem* SimulationSubsystem =
        GetSimulationSubsystem();
    if (Item == nullptr || SimulationSubsystem == nullptr)
    {
        return;
    }

    if (!SimulationSubsystem->LoadScenarioDraft(Item->Record.ScenarioId))
    {
        SetStatusMessage(
            FText::FromString(TEXT(
                "The selected scenario could not be opened from the library.")),
            true);
        return;
    }

    BP_RequestOpenScenarioConfiguration();
}

void UTGScenarioLibraryWidgetBase::HandleOpenVisualizationClicked()
{
    UTGScenarioLibraryItem* Item = GetSingleSelectedItem();
    UGameInstance* GameInstance = GetGameInstance();
    UTGSimulationRunSubsystem* RunSubsystem = GameInstance != nullptr
        ? GameInstance->GetSubsystem<UTGSimulationRunSubsystem>()
        : nullptr;
    if (Item == nullptr || RunSubsystem == nullptr)
    {
        return;
    }

    FText Error;
    if (!RunSubsystem->OpenLatestRunVisualizationForScenario(
            Item->Record.ScenarioId,
            Error))
    {
        SetStatusMessage(
            Error.IsEmpty()
                ? FText::FromString(TEXT(
                    "The selected result could not be opened."))
                : Error,
            true);
    }
}

void UTGScenarioLibraryWidgetBase::HandleContinueClicked()
{
    UTGScenarioLibraryItem* Item = GetSingleSelectedItem();
    UTGSimulationSubsystem* SimulationSubsystem =
        GetSimulationSubsystem();
    if (Item == nullptr || SimulationSubsystem == nullptr)
    {
        return;
    }

    FText Error;
    if (!SimulationSubsystem->CreateContinuationDraftFromLatestRun(
            Item->Record.ScenarioId,
            Error))
    {
        SetStatusMessage(
            Error.IsEmpty()
                ? FText::FromString(TEXT(
                    "A continuation scenario could not be created."))
                : Error,
            true);
        return;
    }

    BP_RequestOpenScenarioConfiguration();
}

void UTGScenarioLibraryWidgetBase::HandleDeleteClicked()
{
    const TArray<UTGScenarioLibraryItem*> SelectedItems =
        GetSelectedItems();
    if (SelectedItems.IsEmpty())
    {
        return;
    }

    PendingDeleteScenarioIds.Reset(SelectedItems.Num());
    for (const UTGScenarioLibraryItem* Item : SelectedItems)
    {
        PendingDeleteScenarioIds.Add(Item->Record.ScenarioId);
    }

    if (SelectedItems.Num() == 1)
    {
        TXT_DeleteConfirmationTitle->SetText(
            FText::FromString(TEXT("Delete scenario?")));
        TXT_DeleteConfirmationBody->SetText(FText::Format(
            FText::FromString(TEXT(
                "Remove \"{0}\" from the Scenario Library? Existing result files will remain on disk.")),
            SelectedItems[0]->ScenarioName));
    }
    else
    {
        TXT_DeleteConfirmationTitle->SetText(
            FText::FromString(TEXT("Delete selected scenarios?")));
        TXT_DeleteConfirmationBody->SetText(FText::Format(
            FText::FromString(TEXT(
                "Remove {0} selected scenarios from the Scenario Library? Existing result files will remain on disk.")),
            FText::AsNumber(SelectedItems.Num())));
    }

    SetDeleteConfirmationVisible(true);
    BTN_CancelDelete->SetKeyboardFocus();
}

void UTGScenarioLibraryWidgetBase::HandleConfirmDeleteClicked()
{
    UTGSimulationSubsystem* SimulationSubsystem =
        GetSimulationSubsystem();
    if (SimulationSubsystem == nullptr)
    {
        SetDeleteConfirmationVisible(false);
        SetStatusMessage(
            FText::FromString(TEXT("The scenario library is unavailable.")),
            true);
        return;
    }

    int32 DeletedCount = 0;
    for (const FGuid& ScenarioId : PendingDeleteScenarioIds)
    {
        if (SimulationSubsystem->DeleteSavedScenario(ScenarioId))
        {
            ++DeletedCount;
        }
    }

    const int32 RequestedCount = PendingDeleteScenarioIds.Num();
    PendingDeleteScenarioIds.Reset();
    SetDeleteConfirmationVisible(false);
    RefreshScenarioList();

    if (DeletedCount != RequestedCount)
    {
        SetStatusMessage(
            FText::Format(
                FText::FromString(TEXT(
                    "Deleted {0} of {1} selected scenarios.")),
                FText::AsNumber(DeletedCount),
                FText::AsNumber(RequestedCount)),
            true);
    }
}

void UTGScenarioLibraryWidgetBase::HandleCancelDeleteClicked()
{
    PendingDeleteScenarioIds.Reset();
    SetDeleteConfirmationVisible(false);
}

void UTGScenarioLibraryWidgetBase::HandleRefreshClicked()
{
    RefreshScenarioList();
}

void UTGScenarioLibraryWidgetBase::HandleBackClicked()
{
    BP_RequestMainMenu();
}

void UTGScenarioLibraryWidgetBase::SetStatusMessage(
    const FText& Message,
    const bool bIsError)
{
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    TXT_Status->SetText(Message);
    TXT_Status->SetColorAndOpacity(
        bIsError ? Palette.Error : Palette.Success);
    TXT_Status->SetVisibility(
        Message.IsEmpty()
            ? ESlateVisibility::Collapsed
            : ESlateVisibility::HitTestInvisible);
}

void UTGScenarioLibraryWidgetBase::ClearStatusMessage()
{
    TXT_Status->SetText(FText::GetEmpty());
    TXT_Status->SetVisibility(ESlateVisibility::Collapsed);
}

void UTGScenarioLibraryWidgetBase::SetDeleteConfirmationVisible(
    const bool bVisible)
{
    BORDER_DeleteConfirmation->SetVisibility(
        bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}
