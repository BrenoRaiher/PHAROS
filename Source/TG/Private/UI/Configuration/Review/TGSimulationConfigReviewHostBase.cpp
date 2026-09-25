// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Review/TGSimulationConfigReviewHostBase.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Reply.h"
#include "Simulation/TGSimulationSubsystem.h"
#include "TimerManager.h"
#include "UI/Common/TGLoadingUiLibrary.h"
#include "UI/Configuration/TGExecutionTimeLimitDialogWidget.h"
#include "UI/Configuration/Review/TGConfigReviewWidgetBase.h"
#include "UI/Configuration/Review/TGScenarioReviewNavigationTarget.h"
#include "UI/Configuration/TGUnsavedChangesDialogWidgetBase.h"
#include "UI/Theme/TGUiTheme.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    struct FConfigNavigationBinding
    {
        const TCHAR* ButtonName;
        const TCHAR* PanelName;
    };

    constexpr FConfigNavigationBinding ConfigNavigationBindings[] =
    {
        { TEXT("BTN_ScenarioSolver"), TEXT("BORDER_PageScenarioSolver") },
        { TEXT("BTN_InitialState"), TEXT("BORDER_PageInitialState") },
        { TEXT("BTN_ComponentTree"), TEXT("ComponentTreePanel") },
        { TEXT("BTN_Actuators"), TEXT("BORDER_PageActuators") },
        { TEXT("BTN_Controls"), TEXT("BORDER_PageControls") },
        { TEXT("BTN_GravityBodies"), TEXT("BORDER_PageGravityBodies") },
        {
            TEXT("BTN_SolarRadiationPressure"),
            TEXT("WBP_Config_SolarRadiationPressure")
        },
        { TEXT("BTN_Atmosphere"), TEXT("BORDER_PageAtmosphere") },
        { TEXT("BTN_Aerodynamics"), TEXT("BORDER_PageAerodynamics") },
        { TEXT("BTN_Review"), TEXT("BORDER_PageReview") }
    };

    const FSlateBrush& ContextMenuBrush()
    {
        const FTGUiPalette& Palette = TGUiTheme::GetPalette();
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            Palette.Panel,
            0.0f,
            Palette.BorderStrong,
            1.0f);
        return Brush;
    }

    const FButtonStyle& ContextMenuButtonStyle()
    {
        static const FButtonStyle Style = []
        {
            const FTGUiPalette& Palette = TGUiTheme::GetPalette();
            FButtonStyle Result = TGUiTheme::MakeButtonStyle(
                ETGUiButtonStyle::Quiet);
            Result.SetNormal(TGUiTheme::MakeRoundedBrush(
                    FLinearColor::Transparent,
                    0.0f))
                .SetHovered(TGUiTheme::MakeRoundedBrush(
                    Palette.AccentSubtle,
                    0.0f))
                .SetPressed(TGUiTheme::MakeRoundedBrush(
                    Palette.Selection,
                    0.0f))
                .SetDisabled(TGUiTheme::MakeRoundedBrush(
                    FLinearColor::Transparent,
                    0.0f));
            return Result;
        }();
        return Style;
    }

    const TCHAR* GetPanelNameForReviewSection(
        ETGScenarioReviewSection Section)
    {
        switch (Section)
        {
        case ETGScenarioReviewSection::ScenarioAndSolver:
            return TEXT("BORDER_PageScenarioSolver");
        case ETGScenarioReviewSection::InitialState:
            return TEXT("BORDER_PageInitialState");
        case ETGScenarioReviewSection::ComponentsAndJoints:
            return TEXT("ComponentTreePanel");
        case ETGScenarioReviewSection::Actuators:
            return TEXT("BORDER_PageActuators");
        case ETGScenarioReviewSection::Controller:
            return TEXT("BORDER_PageControls");
        case ETGScenarioReviewSection::Gravity:
            return TEXT("BORDER_PageGravityBodies");
        case ETGScenarioReviewSection::SolarRadiationPressure:
            return TEXT("WBP_Config_SolarRadiationPressure");
        case ETGScenarioReviewSection::Atmosphere:
            return TEXT("BORDER_PageAtmosphere");
        case ETGScenarioReviewSection::Aerodynamics:
            return TEXT("BORDER_PageAerodynamics");
        default:
            return nullptr;
        }
    }

    template <typename WidgetType>
    WidgetType* FindWidgetDescendantOfType(UWidget* Root)
    {
        if (WidgetType* Match = Cast<WidgetType>(Root))
        {
            return Match;
        }

        if (UPanelWidget* Panel = Cast<UPanelWidget>(Root))
        {
            for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
            {
                if (WidgetType* Match = FindWidgetDescendantOfType<WidgetType>(
                        Panel->GetChildAt(Index)))
                {
                    return Match;
                }
            }
        }
        return nullptr;
    }

    UWidget* FindReviewNavigationTarget(UWidget* Root)
    {
        if (Root == nullptr)
        {
            return nullptr;
        }
        if (Root->GetClass()->ImplementsInterface(
                UTGScenarioReviewNavigationTarget::StaticClass()))
        {
            return Root;
        }

        if (UPanelWidget* Panel = Cast<UPanelWidget>(Root))
        {
            for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
            {
                if (UWidget* Match = FindReviewNavigationTarget(
                        Panel->GetChildAt(Index)))
                {
                    return Match;
                }
            }
        }
        return nullptr;
    }
}

UTGSimulationConfigReviewHostBase::UTGSimulationConfigReviewHostBase(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FClassFinder<
        UTGUnsavedChangesDialogWidgetBase> DialogClassFinder(
            TEXT("/Game/UI/WBP_UnsavedChangesDialog"));
    if (DialogClassFinder.Succeeded())
    {
        UnsavedChangesDialogClass = DialogClassFinder.Class;
    }
}

void UTGSimulationConfigReviewHostBase::NativeConstruct()
{
    Super::NativeConstruct();

    ApplyProductBranding();

    BoundReviewPanel = FindReviewPanel();
    if (BoundReviewPanel != nullptr)
    {
        BoundReviewPanel->OnIssueNavigationRequested.AddUniqueDynamic(
            this,
            &UTGSimulationConfigReviewHostBase::HandleReviewIssueNavigationRequested);
    }

    LastPresentedPanelIndex = INDEX_NONE;
    RefreshNavigationVisualState();

    bHasPresentedDirtyState = false;
    RefreshScenarioDirtyVisualState();
    RefreshExecutionPolicyMirror();

    if (SIZE_NavigationRail != nullptr)
    {
        SIZE_NavigationRail->ClearMinDesiredWidth();
        SIZE_NavigationRail->SetClipping(EWidgetClipping::ClipToBounds);
    }

    if (BORDER_NavigationResizeHandle != nullptr)
    {
        BORDER_NavigationResizeHandle->SetCursor(
            EMouseCursor::ResizeLeftRight);
    }

}

void UTGSimulationConfigReviewHostBase::ApplyProductBranding()
{
    if (WidgetTree == nullptr)
    {
        return;
    }

    WidgetTree->ForEachWidget([](UWidget* Widget)
    {
        if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
        {
            const FString CurrentText = TextBlock->GetText().ToString();
            if (CurrentText.Equals(TEXT("Import TGSCN"),
                    ESearchCase::CaseSensitive))
            {
                TextBlock->SetText(FText::FromString(
                    TEXT("Import PHAROS Scenario")));
            }
        }

        const FString CurrentToolTip = Widget->GetToolTipText().ToString();
        if (CurrentToolTip.Contains(TEXT("TGSCN"),
                ESearchCase::CaseSensitive))
        {
            Widget->SetToolTipText(FText::FromString(
                CurrentToolTip.Replace(
                    TEXT("TGSCN"),
                    TEXT("PHAROS scenario"),
                    ESearchCase::CaseSensitive)));
        }
    });
}

void UTGSimulationConfigReviewHostBase::NativeDestruct()
{
    if (BoundReviewPanel != nullptr)
    {
        BoundReviewPanel->OnIssueNavigationRequested.RemoveDynamic(
            this,
            &UTGSimulationConfigReviewHostBase::HandleReviewIssueNavigationRequested);
    }
    BoundReviewPanel = nullptr;

    LastPresentedPanelIndex = INDEX_NONE;
    bHasPresentedDirtyState = false;
    bResizingNavigationRail = false;
    PendingNavigationDestination = EPendingNavigationDestination::None;
    bScenarioSaveRequestInProgress = false;
    bConfigPanelSwitchInProgress = false;

    CloseUnsavedChangesDialog();
    if (ActiveExecutionTimeLimitDialog != nullptr)
    {
        ActiveExecutionTimeLimitDialog->RemoveFromParent();
        ActiveExecutionTimeLimitDialog = nullptr;
    }

    Super::NativeDestruct();
}

void UTGSimulationConfigReviewHostBase::NativeTick(
    const FGeometry& MyGeometry,
    const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshNavigationVisualState();
    RefreshScenarioDirtyVisualState();
    RefreshExecutionPolicyMirror();
}

FReply UTGSimulationConfigReviewHostBase::NativeOnMouseButtonDown(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
        && SIZE_NavigationRail != nullptr
        && IsNavigationResizeHit(InMouseEvent.GetScreenSpacePosition()))
    {
        bResizingNavigationRail = true;
        ResizeStartScreenPosition = InMouseEvent.GetScreenSpacePosition();

        ResizeStartWidth = SIZE_NavigationRail->GetWidthOverride();

        return FReply::Handled().CaptureMouse(TakeWidget());
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UTGSimulationConfigReviewHostBase::NativeOnPreviewMouseButtonDown(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton &&
        SimulateButton != nullptr &&
        IsScreenPositionInsideWidget(
            SimulateButton,
            InMouseEvent.GetScreenSpacePosition()))
    {
        ShowSimulationContextMenu(
            InMouseEvent.GetScreenSpacePosition());
        return FReply::Handled();
    }

    return Super::NativeOnPreviewMouseButtonDown(
        InGeometry,
        InMouseEvent);
}

FReply UTGSimulationConfigReviewHostBase::NativeOnMouseMove(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    if (bResizingNavigationRail && SIZE_NavigationRail != nullptr)
    {
        const FVector2D StartLocal =
            InGeometry.AbsoluteToLocal(ResizeStartScreenPosition);
        const FVector2D CurrentLocal =
            InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

        const float DeltaX =
            static_cast<float>(CurrentLocal.X - StartLocal.X);

        SIZE_NavigationRail->SetWidthOverride(
            FMath::Clamp(
                ResizeStartWidth + DeltaX,
                NavigationRailMinimumWidth,
                NavigationRailMaximumWidth));

        return FReply::Handled();
    }

    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UTGSimulationConfigReviewHostBase::NativeOnMouseButtonUp(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    if (bResizingNavigationRail
        && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bResizingNavigationRail = false;
        return FReply::Handled().ReleaseMouseCapture();
    }

    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UTGSimulationConfigReviewHostBase::RequestBackWithUnsavedGuard()
{
    RequestNavigationWithUnsavedGuard(
        EPendingNavigationDestination::MainMenu);
}

void UTGSimulationConfigReviewHostBase::
    RequestScenarioLibraryWithUnsavedGuard()
{
    RequestNavigationWithUnsavedGuard(
        EPendingNavigationDestination::ScenarioLibrary);
}

void UTGSimulationConfigReviewHostBase::
    RequestSaveCurrentScenarioWithLoading()
{
    BeginScenarioSave(false);
}

void UTGSimulationConfigReviewHostBase::
    RequestConfigPanelSwitchWithLoading(
        const int32 TargetPanelIndex,
        const FText& PanelName)
{
    if (bConfigPanelSwitchInProgress)
    {
        return;
    }

    bConfigPanelSwitchInProgress = true;
    const FString Name = PanelName.IsEmpty()
        ? TEXT("Configuration Panel")
        : PanelName.ToString();
    UTGLoadingUiLibrary::ShowLoadingPopup(
        this,
        FText::FromString(FString::Printf(TEXT("Opening %s"), *Name)),
        FText::FromString(TEXT("Preparing the 3D editor.")));

    if (UWorld* World = GetWorld())
    {
        // A timer queued from Slate input may fire before the overlay's first
        // paint. Cross two tick boundaries so one complete frame presents it.
        World->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateWeakLambda(
                this,
                [this, TargetPanelIndex]()
                {
                    if (UWorld* DeferredWorld = GetWorld())
                    {
                        DeferredWorld->GetTimerManager().SetTimerForNextTick(
                            FTimerDelegate::CreateWeakLambda(
                                this,
                                [this, TargetPanelIndex]()
                                {
                                    ExecuteConfigPanelSwitch(
                                        TargetPanelIndex);
                                }));
                        return;
                    }

                    ExecuteConfigPanelSwitch(TargetPanelIndex);
                }));
        return;
    }

    ExecuteConfigPanelSwitch(TargetPanelIndex);
}

void UTGSimulationConfigReviewHostBase::RequestNavigationWithUnsavedGuard(
    const EPendingNavigationDestination Destination)
{
    PendingNavigationDestination = Destination;

    UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    if (Subsystem == nullptr || !Subsystem->IsCurrentScenarioDirty())
    {
        CompletePendingNavigation();
        return;
    }

    ShowUnsavedChangesDialog();
}

void UTGSimulationConfigReviewHostBase::RefreshScenarioDirtyVisualState()
{
    UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    const bool bIsDirty =
        Subsystem != nullptr && Subsystem->IsCurrentScenarioDirty();

    if (bHasPresentedDirtyState && bLastPresentedDirtyState == bIsDirty)
    {
        return;
    }

    bHasPresentedDirtyState = true;
    bLastPresentedDirtyState = bIsDirty;

    if (BTN_SaveScenario == nullptr)
    {
        return;
    }

    UTextBlock* Label = Cast<UTextBlock>(BTN_SaveScenario->GetContent());
    if (Label == nullptr)
    {
        return;
    }

    Label->SetText(
        bIsDirty
            ? FText::FromString(TEXT("Save Scenario *"))
            : FText::FromString(TEXT("Save Scenario")));
}

void UTGSimulationConfigReviewHostBase::RefreshExecutionPolicyMirror()
{
    if (INPUT_MaximumWallClockRuntimeSeconds == nullptr)
    {
        return;
    }

    const UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    if (Subsystem == nullptr)
    {
        return;
    }

    const double MaximumSeconds =
        Subsystem->GetCurrentScenarioMaximumWallClockRuntimeSeconds();
    if (!FMath::IsFinite(MaximumSeconds) || MaximumSeconds <= 0.0)
    {
        return;
    }

    const FText ExpectedText = FText::FromString(
        FString::SanitizeFloat(MaximumSeconds, 0));
    if (!INPUT_MaximumWallClockRuntimeSeconds->GetText().EqualTo(
            ExpectedText))
    {
        INPUT_MaximumWallClockRuntimeSeconds->SetText(ExpectedText);
    }
}

UTGSimulationSubsystem*
UTGSimulationConfigReviewHostBase::GetSimulationSubsystem() const
{
    UGameInstance* GameInstance = GetGameInstance();
    return GameInstance != nullptr
        ? GameInstance->GetSubsystem<UTGSimulationSubsystem>()
        : nullptr;
}

void UTGSimulationConfigReviewHostBase::ShowUnsavedChangesDialog()
{
    if (ActiveUnsavedChangesDialog != nullptr)
    {
        return;
    }

    TSubclassOf<UTGUnsavedChangesDialogWidgetBase> DialogClass =
        UnsavedChangesDialogClass;

    if (DialogClass == nullptr)
    {
        DialogClass = LoadClass<UTGUnsavedChangesDialogWidgetBase>(
            nullptr,
            TEXT(
                "/Game/UI/WBP_UnsavedChangesDialog."
                "WBP_UnsavedChangesDialog_C"));
    }

    if (DialogClass == nullptr)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("PHAROS: The unsaved-changes dialog could not be loaded."));
        return;
    }

    ActiveUnsavedChangesDialog =
        CreateWidget<UTGUnsavedChangesDialogWidgetBase>(
            GetOwningPlayer(),
            DialogClass);

    if (ActiveUnsavedChangesDialog == nullptr)
    {
        return;
    }

    if (PendingNavigationDestination ==
        EPendingNavigationDestination::ScenarioLibrary)
    {
        ActiveUnsavedChangesDialog->ConfigureForScenarioLibrary();
    }

    ActiveUnsavedChangesDialog->OnSaveAndBackRequested.AddDynamic(
        this,
        &UTGSimulationConfigReviewHostBase::HandleDialogSaveAndBack);

    ActiveUnsavedChangesDialog->OnDiscardAndBackRequested.AddDynamic(
        this,
        &UTGSimulationConfigReviewHostBase::HandleDialogDiscardAndBack);

    ActiveUnsavedChangesDialog->OnCancelRequested.AddDynamic(
        this,
        &UTGSimulationConfigReviewHostBase::HandleDialogCancel);

    ActiveUnsavedChangesDialog->AddToViewport(1000);
    ActiveUnsavedChangesDialog->SetKeyboardFocus();
}

void UTGSimulationConfigReviewHostBase::CloseUnsavedChangesDialog()
{
    if (ActiveUnsavedChangesDialog == nullptr)
    {
        return;
    }

    ActiveUnsavedChangesDialog->RemoveFromParent();
    ActiveUnsavedChangesDialog = nullptr;
}

void UTGSimulationConfigReviewHostBase::CompletePendingNavigation()
{
    const EPendingNavigationDestination Destination =
        PendingNavigationDestination;
    PendingNavigationDestination = EPendingNavigationDestination::None;

    if (Destination == EPendingNavigationDestination::None)
    {
        return;
    }

    const bool bOpeningLibrary = Destination ==
        EPendingNavigationDestination::ScenarioLibrary;
    UTGLoadingUiLibrary::ShowLoadingPopup(
        this,
        FText::FromString(
            bOpeningLibrary
                ? TEXT("Opening Scenario Library")
                : TEXT("Opening Main Menu")),
        FText::FromString(TEXT("Preparing the requested screen.")));

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateWeakLambda(
                this,
                [this, Destination]()
                {
                    ExecuteNavigation(Destination);
                }));
        return;
    }

    ExecuteNavigation(Destination);
}

void UTGSimulationConfigReviewHostBase::ExecuteNavigation(
    const EPendingNavigationDestination Destination)
{
    switch (Destination)
    {
    case EPendingNavigationDestination::MainMenu:
        PerformMainMenuNavigation();
        break;
    case EPendingNavigationDestination::ScenarioLibrary:
        OnScenarioLibraryRequested.Broadcast();
        break;
    case EPendingNavigationDestination::None:
    default:
        break;
    }

    UTGLoadingUiLibrary::HideLoadingPopupAfterNextTick(this);
}

void UTGSimulationConfigReviewHostBase::BeginScenarioSave(
    const bool bNavigateAfterSave)
{
    if (bScenarioSaveRequestInProgress)
    {
        return;
    }

    UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    if (Subsystem == nullptr)
    {
        return;
    }

    bScenarioSaveRequestInProgress = true;
    UTGLoadingUiLibrary::ShowLoadingPopup(
        this,
        FText::FromString(TEXT("Saving Scenario")),
        FText::FromString(TEXT(
            "Writing the scenario and updating its recovery backup.")));

    const TWeakObjectPtr<UTGSimulationConfigReviewHostBase> WeakThis(this);
    const auto StartSave =
        [WeakThis, Subsystem, bNavigateAfterSave]()
        {
            UTGSimulationConfigReviewHostBase* Host = WeakThis.Get();
            if (Host == nullptr)
            {
                return;
            }

            const bool bStarted = Subsystem->BeginSaveCurrentScenarioDraft(
                [WeakThis, bNavigateAfterSave](
                    const bool bSucceeded,
                    const FGuid& ScenarioId)
                {
                    if (UTGSimulationConfigReviewHostBase* CurrentHost =
                            WeakThis.Get())
                    {
                        CurrentHost->FinishScenarioSave(
                            bSucceeded,
                            ScenarioId,
                            bNavigateAfterSave);
                    }
                });
            if (!bStarted)
            {
                Host->FinishScenarioSave(
                    false,
                    FGuid{},
                    bNavigateAfterSave);
            }
        };

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateLambda(StartSave));
        return;
    }
    StartSave();
}

void UTGSimulationConfigReviewHostBase::FinishScenarioSave(
    const bool bSucceeded,
    const FGuid& ScenarioId,
    const bool bNavigateAfterSave)
{
    (void)ScenarioId;
    bScenarioSaveRequestInProgress = false;
    RefreshScenarioDirtyVisualState();

    if (bSucceeded && bNavigateAfterSave)
    {
        CloseUnsavedChangesDialog();
        CompletePendingNavigation();
        return;
    }

    UTGLoadingUiLibrary::HideLoadingPopupAfterNextTick(this);
}

void UTGSimulationConfigReviewHostBase::ExecuteConfigPanelSwitch(
    const int32 TargetPanelIndex)
{
    PerformConfigPanelSwitch(TargetPanelIndex);
    RefreshNavigationVisualState();
    bConfigPanelSwitchInProgress = false;
    UTGLoadingUiLibrary::HideLoadingPopupAfterNextTick(this);
}

void UTGSimulationConfigReviewHostBase::HandleDialogSaveAndBack()
{
    BeginScenarioSave(true);
}

void UTGSimulationConfigReviewHostBase::HandleDialogDiscardAndBack()
{
    if (UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem())
    {
        const FGuid ScenarioId = Subsystem->GetCurrentScenarioId();

        if (ScenarioId.IsValid())
        {
            // Restore the exact last saved authored scenario. The authoritative
            // subsystem owns draft revision and Review invalidation semantics.
            Subsystem->LoadScenarioDraft(ScenarioId);
        }
        else
        {
            Subsystem->ClearCurrentScenarioDraft();
        }
    }

    CloseUnsavedChangesDialog();
    CompletePendingNavigation();
}

void UTGSimulationConfigReviewHostBase::HandleDialogCancel()
{
    CloseUnsavedChangesDialog();
    PendingNavigationDestination = EPendingNavigationDestination::None;
}

bool UTGSimulationConfigReviewHostBase::SaveCurrentScenarioForBackNavigation()
{
    UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    if (Subsystem == nullptr)
    {
        return false;
    }

    FGuid ScenarioId;
    return Subsystem->SaveCurrentScenarioDraft(ScenarioId);
}

bool UTGSimulationConfigReviewHostBase::IsNavigationResizeHit(
    const FVector2D& ScreenPosition) const
{
    if (SIZE_NavigationRail == nullptr)
    {
        return false;
    }

    const FGeometry& RailGeometry = SIZE_NavigationRail->GetCachedGeometry();
    const FVector2D TopLeft =
        RailGeometry.LocalToAbsolute(FVector2D::ZeroVector);
    const FVector2D BottomRight =
        RailGeometry.LocalToAbsolute(RailGeometry.GetLocalSize());

    return ScreenPosition.X >= BottomRight.X - NavigationResizeHitWidth
        && ScreenPosition.X <= BottomRight.X + NavigationResizeHitWidth
        && ScreenPosition.Y >= TopLeft.Y
        && ScreenPosition.Y <= BottomRight.Y;
}

bool UTGSimulationConfigReviewHostBase::IsScreenPositionInsideWidget(
    const UWidget* Widget,
    const FVector2D& ScreenPosition) const
{
    if (Widget == nullptr || Widget->GetVisibility() ==
        ESlateVisibility::Collapsed)
    {
        return false;
    }

    const FGeometry& Geometry = Widget->GetCachedGeometry();
    const FVector2D LocalPosition =
        Geometry.AbsoluteToLocal(ScreenPosition);
    const FVector2D LocalSize = Geometry.GetLocalSize();
    return LocalPosition.X >= 0.0 && LocalPosition.Y >= 0.0 &&
        LocalPosition.X <= LocalSize.X &&
        LocalPosition.Y <= LocalSize.Y;
}

void UTGSimulationConfigReviewHostBase::ShowSimulationContextMenu(
    const FVector2D& ScreenPosition)
{
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    FSlateFontInfo MenuFont = TGUiTheme::GetSlateFont(
        ETGUiTextStyle::Caption);
    MenuFont.Size = 9;

    const TSharedRef<SWidget> MenuContent =
        SNew(SBox)
        .WidthOverride(210.0f)
        [
            SNew(SBorder)
            .BorderImage(&ContextMenuBrush())
            .Padding(0.0f)
            [
                SNew(SButton)
                .ButtonStyle(&ContextMenuButtonStyle())
                .ContentPadding(FMargin(10.0f, 5.0f))
                .HAlign(HAlign_Left)
                .ToolTipText(FText::FromString(TEXT(
                    "Set the maximum real-time duration allowed for a "
                    "simulation run.")))
                .OnClicked(FOnClicked::CreateUObject(
                    this,
                    &UTGSimulationConfigReviewHostBase::
                        HandleExecutionTimeLimitMenuClicked))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(
                        TEXT("Limit Execution Time")))
                    .Font(MenuFont)
                    .ColorAndOpacity(Palette.TextPrimary)
                ]
            ]
        ];

    MenuContent->SlatePrepass(
        FSlateApplication::Get().GetApplicationScale());
    FVector2D MenuPosition = ScreenPosition;
    MenuPosition.Y -= MenuContent->GetDesiredSize().Y;

    FSlateApplication::Get().PushMenu(
        TakeWidget(),
        FWidgetPath(),
        MenuContent,
        MenuPosition,
        FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}

void UTGSimulationConfigReviewHostBase::ShowExecutionTimeLimitDialog()
{
    if (ActiveExecutionTimeLimitDialog != nullptr &&
        ActiveExecutionTimeLimitDialog->IsInViewport())
    {
        ActiveExecutionTimeLimitDialog->SetKeyboardFocus();
        return;
    }

    ActiveExecutionTimeLimitDialog =
        CreateWidget<UTGExecutionTimeLimitDialogWidget>(
            GetOwningPlayer(),
            UTGExecutionTimeLimitDialogWidget::StaticClass());
    if (ActiveExecutionTimeLimitDialog == nullptr)
    {
        return;
    }

    ActiveExecutionTimeLimitDialog->AddToViewport(5000);
    ActiveExecutionTimeLimitDialog->SetKeyboardFocus();
}

FReply UTGSimulationConfigReviewHostBase::
    HandleExecutionTimeLimitMenuClicked()
{
    FSlateApplication::Get().DismissAllMenus();
    ShowExecutionTimeLimitDialog();
    return FReply::Handled();
}

void UTGSimulationConfigReviewHostBase::
    HandleReviewIssueNavigationRequested(
        FTGScenarioReviewIssue Issue)
{
    if (ConfigPanelSwitcher == nullptr)
    {
        return;
    }

    UWidget* TargetPanel = FindConfigPanelForReviewSection(Issue.Section);
    if (TargetPanel == nullptr)
    {
        return;
    }

    const int32 TargetPanelIndex =
        ConfigPanelSwitcher->GetChildIndex(TargetPanel);
    if (TargetPanelIndex == INDEX_NONE)
    {
        return;
    }

    PerformConfigPanelSwitch(TargetPanelIndex);
    RefreshNavigationVisualState();

    UWidget* NavigationTarget = FindReviewNavigationTarget(TargetPanel);
    const bool bExactNavigation = NavigationTarget != nullptr
        && ITGScenarioReviewNavigationTarget::
            Execute_NavigateToScenarioReviewIssue(NavigationTarget, Issue);

    if (!bExactNavigation)
    {
        TargetPanel->SetIsEnabled(true);
        TargetPanel->SetKeyboardFocus();
        TargetPanel->SetToolTipText(Issue.Message);
    }
}

UWidget* UTGSimulationConfigReviewHostBase::FindConfigPanelForReviewSection(
    ETGScenarioReviewSection Section) const
{
    const TCHAR* PanelName = GetPanelNameForReviewSection(Section);
    return PanelName != nullptr
        ? GetWidgetFromName(FName(PanelName))
        : nullptr;
}

void UTGSimulationConfigReviewHostBase::RefreshNavigationVisualState()
{
    if (ConfigPanelSwitcher == nullptr)
    {
        return;
    }

    const int32 ActiveIndex = ConfigPanelSwitcher->GetActiveWidgetIndex();
    if (ActiveIndex == LastPresentedPanelIndex)
    {
        return;
    }

    UWidget* ActivePanel = ConfigPanelSwitcher->GetActiveWidget();
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    for (const FConfigNavigationBinding& Binding : ConfigNavigationBindings)
    {
        UButton* Button = Cast<UButton>(GetWidgetFromName(
            FName(Binding.ButtonName)));
        if (Button == nullptr)
        {
            continue;
        }

        const bool bSelected =
            GetWidgetFromName(FName(Binding.PanelName)) == ActivePanel;
        FButtonStyle NavigationStyle =
            TGUiTheme::MakeButtonStyle(ETGUiButtonStyle::Quiet);
        if (bSelected)
        {
            NavigationStyle.SetNormal(TGUiTheme::MakeRoundedBrush(
                    Palette.Selection,
                    4.0f,
                    Palette.Accent,
                    1.0f))
                .SetHovered(TGUiTheme::MakeRoundedBrush(
                    Palette.SurfaceRaised,
                    4.0f,
                    Palette.AccentHover,
                    1.0f))
                .SetPressed(TGUiTheme::MakeRoundedBrush(
                    Palette.Selection,
                    4.0f,
                    Palette.AccentPressed,
                    1.0f));
        }
        Button->SetStyle(NavigationStyle);
        Button->SetBackgroundColor(FLinearColor::White);

        if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
        {
            Label->SetColorAndOpacity(FSlateColor(
                bSelected ? Palette.Accent : Palette.TextSecondary));
        }
    }

    LastPresentedPanelIndex = ActiveIndex;
}

void UTGSimulationConfigReviewHostBase::PerformConfigPanelSwitch_Implementation(
    int32 TargetPanelIndex)
{
    if (ConfigPanelSwitcher != nullptr
        && TargetPanelIndex >= 0
        && TargetPanelIndex < ConfigPanelSwitcher->GetNumWidgets())
    {
        ConfigPanelSwitcher->SetActiveWidgetIndex(TargetPanelIndex);
    }
}

UTGConfigReviewWidgetBase*
UTGSimulationConfigReviewHostBase::FindReviewPanel() const
{
    if (ConfigPanelSwitcher == nullptr)
    {
        return nullptr;
    }

    for (int32 Index = 0;
         Index < ConfigPanelSwitcher->GetNumWidgets();
         ++Index)
    {
        if (UTGConfigReviewWidgetBase* ReviewPanel =
                FindWidgetDescendantOfType<UTGConfigReviewWidgetBase>(
                    ConfigPanelSwitcher->GetWidgetAtIndex(Index)))
        {
            return ReviewPanel;
        }
    }
    return nullptr;
}
