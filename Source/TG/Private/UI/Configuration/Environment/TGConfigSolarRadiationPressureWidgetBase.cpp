// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Environment/TGConfigSolarRadiationPressureWidgetBase.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Simulation/TGSimulationSubsystem.h"
#include "UI/Common/TGSearchBox.h"
#include "UI/Configuration/Environment/TGSolarRadiationPressureEditingLibrary.h"
#include "UI/Configuration/Environment/TGSrpComponentRowWidget.h"
#include "UI/Theme/TGUiTheme.h"
#include "UI/TGHudFormattingLibrary.h"
#include "Visualization/TGSpacecraftPreviewPawn.h"
#include "Visualization/TGSpacecraftVisualActor.h"

namespace TGConfigSolarRadiationPressurePrivate
{
    const FString AutomaticResolutionText = TEXT("Automatic");
    const FString CustomResolutionText =
        TEXT("Custom Target Triangle Count");
    const FString LogicalRegionModeText = TEXT("Logical Region");
    const FString ProxyTriangleModeText = TEXT("Proxy Triangle");
    constexpr float MessageLifetimeSeconds = 8.0f;

    void ResetCombo(
        UComboBoxString* Combo,
        const TArray<FString>& Options,
        const FString& Selected)
    {
        if (Combo == nullptr)
        {
            return;
        }

        Combo->ClearOptions();
        for (const FString& Option : Options)
        {
            Combo->AddOption(Option);
        }

        if (!Selected.IsEmpty() && Options.Contains(Selected))
        {
            Combo->SetSelectedOption(Selected);
        }
        else if (!Options.IsEmpty())
        {
            Combo->SetSelectedIndex(0);
        }
    }

    FString TriangleOptionText(int32 StableTriangleIndex)
    {
        return FString::Printf(
            TEXT("Triangle %d"),
            StableTriangleIndex);
    }

    bool ParseTriangleOption(
        const FString& Option,
        int32& OutStableTriangleIndex)
    {
        const FString Prefix = TEXT("Triangle ");
        if (!Option.StartsWith(Prefix))
        {
            OutStableTriangleIndex = INDEX_NONE;
            return false;
        }

        const FString IndexText = Option.RightChop(Prefix.Len());
        if (!IndexText.IsNumeric())
        {
            OutStableTriangleIndex = INDEX_NONE;
            return false;
        }

        OutStableTriangleIndex = FCString::Atoi(*IndexText);
        return OutStableTriangleIndex >= 0;
    }

    FTGSrpOpticalProperties GetInheritedProperties(
        const FTGSimulationScenario& Scenario,
        const FTGComponentConfig& Component)
    {
        return
            Component.SolarRadiationPressure
                .bUseGlobalFallbackOpticalProperties
                ? Scenario.SolarRadiationPressure
                    .GlobalFallbackOpticalProperties
                : Component.SolarRadiationPressure
                    .ComponentOpticalProperties;
    }
}

void UTGConfigSolarRadiationPressureWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();

    // WBP_SimulationConfig gives this panel keyboard focus when it activates
    // the shared Game+UI preview input mode. UserWidget defaults to not
    // focusable, which otherwise produces an InputMode warning on PIE exit.
    SetIsFocusable(true);

    if (CHECK_EnableSrp != nullptr)
    {
        CHECK_EnableSrp->OnCheckStateChanged.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleEnableSrpChanged);
    }

    if (CHECK_ComputeCelestialEclipses != nullptr)
    {
        CHECK_ComputeCelestialEclipses->
            OnCheckStateChanged.AddUniqueDynamic(
                this,
                &UTGConfigSolarRadiationPressureWidgetBase::HandleComputeCelestialEclipsesChanged);
    }

    if (BTN_AddOccultingBody != nullptr)
    {
        BTN_AddOccultingBody->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleAddOccultingBodyClicked);
    }

    if (BTN_RemoveOccultingBody != nullptr)
    {
        BTN_RemoveOccultingBody->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleRemoveOccultingBodyClicked);
    }

    if (BTN_ClearOccultingBodies != nullptr)
    {
        BTN_ClearOccultingBodies->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleClearOccultingBodiesClicked);
    }

    if (CHECK_ComputeComponentShadows != nullptr)
    {
        CHECK_ComputeComponentShadows->
            OnCheckStateChanged.AddUniqueDynamic(
                this,
                &UTGConfigSolarRadiationPressureWidgetBase::HandleComputeComponentShadowsChanged);
    }

    if (BTN_ApplyGlobalOpticalProperties != nullptr)
    {
        BTN_ApplyGlobalOpticalProperties->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleApplyGlobalOpticalPropertiesClicked);
    }

    if (INPUT_GlobalAbsorption != nullptr)
    {
        INPUT_GlobalAbsorption->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleGlobalOpticalPropertiesCommitted);
    }
    if (INPUT_GlobalSpecularReflection != nullptr)
    {
        INPUT_GlobalSpecularReflection->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleGlobalOpticalPropertiesCommitted);
    }
    if (INPUT_GlobalDiffuseReflection != nullptr)
    {
        INPUT_GlobalDiffuseReflection->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleGlobalOpticalPropertiesCommitted);
    }

    if (INPUT_ComponentSearch != nullptr)
    {
        INPUT_ComponentSearch->OnTextChanged.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleComponentSearchChanged);
    }

    if (COMBO_SrpComponent != nullptr)
    {
        COMBO_SrpComponent->OnSelectionChanged.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleComponentSelectionChanged);
    }

    if (CHECK_IncludeComponentInSrpProxy != nullptr)
    {
        CHECK_IncludeComponentInSrpProxy->
            OnCheckStateChanged.AddUniqueDynamic(
                this,
                &UTGConfigSolarRadiationPressureWidgetBase::HandleIncludeComponentChanged);
    }

    if (COMBO_ProxyResolutionMode != nullptr)
    {
        COMBO_ProxyResolutionMode->OnSelectionChanged.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleProxyResolutionModeChanged);
    }

    if (INPUT_CustomTargetTriangleCount != nullptr)
    {
        INPUT_CustomTargetTriangleCount->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleCustomTargetTriangleCountCommitted);
    }


    if (CHECK_UseGlobalFallbackOpticalProperties != nullptr)
    {
        CHECK_UseGlobalFallbackOpticalProperties->
            OnCheckStateChanged.AddUniqueDynamic(
                this,
                &UTGConfigSolarRadiationPressureWidgetBase::HandleUseGlobalFallbackChanged);
    }

    if (CHECK_ApplyOneOpticalConfiguration != nullptr)
    {
        CHECK_ApplyOneOpticalConfiguration->
            OnCheckStateChanged.AddUniqueDynamic(
                this,
                &UTGConfigSolarRadiationPressureWidgetBase::HandleApplyOneOpticalConfigurationChanged);
    }

    if (BTN_ApplyComponentOpticalProperties != nullptr)
    {
        BTN_ApplyComponentOpticalProperties->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleApplyComponentOpticalPropertiesClicked);
    }

    if (INPUT_ComponentAbsorption != nullptr)
    {
        INPUT_ComponentAbsorption->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleComponentOpticalPropertiesCommitted);
    }
    if (INPUT_ComponentSpecularReflection != nullptr)
    {
        INPUT_ComponentSpecularReflection->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleComponentOpticalPropertiesCommitted);
    }
    if (INPUT_ComponentDiffuseReflection != nullptr)
    {
        INPUT_ComponentDiffuseReflection->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleComponentOpticalPropertiesCommitted);
    }

    if (COMBO_OverrideTargetMode != nullptr)
    {
        COMBO_OverrideTargetMode->OnSelectionChanged.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleOverrideTargetModeChanged);
    }

    if (COMBO_OverrideTarget != nullptr)
    {
        COMBO_OverrideTarget->OnSelectionChanged.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleOverrideTargetChanged);
    }

    if (BTN_SelectOverrideInPreview != nullptr)
    {
        BTN_SelectOverrideInPreview->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleSelectOverrideInPreviewClicked);
    }

    if (BTN_ApplyOverrideOpticalProperties != nullptr)
    {
        BTN_ApplyOverrideOpticalProperties->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleApplyOverrideOpticalPropertiesClicked);
    }

    if (BTN_ClearOverrideOpticalProperties != nullptr)
    {
        BTN_ClearOverrideOpticalProperties->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleClearOverrideOpticalPropertiesClicked);
        BTN_ClearOverrideOpticalProperties->SetToolTipText(
            FText::FromString(
                TEXT(
                    "Remove all custom face optical properties from the "
                    "selected component and restore its inherited values.")));
    }

    if (BTN_RemoveSelectedOverrideOpticalProperties != nullptr)
    {
        BTN_RemoveSelectedOverrideOpticalProperties->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleRemoveSelectedOverrideOpticalPropertiesClicked);
        BTN_RemoveSelectedOverrideOpticalProperties->SetToolTipText(
            FText::FromString(
                TEXT(
                    "Remove custom optical properties from the selected "
                    "faces and restore their inherited values.")));
    }

    if (BTN_ClearSrpFaceSelection != nullptr)
    {
        BTN_ClearSrpFaceSelection->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleClearFaceSelectionClicked);
    }

    if (BTN_ConfirmProxyInvalidation != nullptr)
    {
        BTN_ConfirmProxyInvalidation->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleConfirmProxyInvalidationClicked);
    }

    if (BTN_CancelProxyInvalidation != nullptr)
    {
        BTN_CancelProxyInvalidation->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleCancelProxyInvalidationClicked);
    }

    SetProxyInvalidationConfirmationVisible(false);
    RefreshFromCurrentDraft();
}

void UTGConfigSolarRadiationPressureWidgetBase::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LeftMessageTimerHandle);
        World->GetTimerManager().ClearTimer(InspectorMessageTimerHandle);
    }
    DeactivateSolarRadiationPressurePreview();
    Super::NativeDestruct();
}

void UTGConfigSolarRadiationPressureWidgetBase::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bRefreshing)
    {
        return;
    }

    if (bPreviewIntegrationActive)
    {
        RefreshPreviewIntegration();
    }

    if (
        const UTGSimulationSubsystem* Subsystem =
            GetSimulationSubsystem())
    {
        if (
            Subsystem->GetCurrentScenarioDraftRevision() !=
            LastObservedDraftRevision)
        {
            if (bPreviewIntegrationActive &&
                IsValid(PreviewPawn) &&
                IsValid(PreviewSpacecraftActor))
            {
                ATGSpacecraftPreviewPawn* ActivePreviewPawn =
                    PreviewPawn;
                ATGSpacecraftVisualActor* ActiveSpacecraftActor =
                    PreviewSpacecraftActor;
                ActivateSolarRadiationPressurePreview(
                    ActivePreviewPawn,
                    ActiveSpacecraftActor);
            }
            else
            {
                RefreshFromCurrentDraft();
            }
        }
    }
}

UTGSimulationSubsystem*
UTGConfigSolarRadiationPressureWidgetBase::
    GetSimulationSubsystem() const
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return nullptr;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    if (GameInstance == nullptr)
    {
        return nullptr;
    }

    return GameInstance->GetSubsystem<UTGSimulationSubsystem>();
}

bool UTGConfigSolarRadiationPressureWidgetBase::
    PullWorkingScenarioFromDraft()
{
    UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    if (Subsystem == nullptr)
    {
        ShowError(
            FText::FromString(
                TEXT("The PHAROS simulation service is unavailable.")));
        return false;
    }

    WorkingScenario = Subsystem->GetCurrentScenarioDraft();
    LastObservedDraftRevision =
        Subsystem->GetCurrentScenarioDraftRevision();

    GlobalOpticalInputWeights =
        WorkingScenario.SolarRadiationPressure
            .GlobalFallbackOpticalProperties;
    bHasGlobalOpticalInputWeights = true;
    ComponentOpticalInputWeights.Reset();
    for (const FTGComponentConfig& Component : WorkingScenario.Components)
    {
        ComponentOpticalInputWeights.Add(
            Component.ComponentId,
            Component.SolarRadiationPressure.ComponentOpticalProperties);
    }
    return true;
}

bool UTGConfigSolarRadiationPressureWidgetBase::
    CommitWorkingScenarioToDraft()
{
    UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    if (Subsystem == nullptr)
    {
        ShowError(
            FText::FromString(
                TEXT("The PHAROS simulation service is unavailable.")));
        return false;
    }

    Subsystem->SetCurrentScenarioDraft(WorkingScenario);
    LastObservedDraftRevision =
        Subsystem->GetCurrentScenarioDraftRevision();
    return true;
}

void UTGConfigSolarRadiationPressureWidgetBase::
    RefreshFromCurrentDraft()
{
    ClearMessages();
    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    const FTGSimulationScenario ScenarioBeforeNormalization =
        WorkingScenario;

    UTGSolarRadiationPressureEditingLibrary::
        NormalizeSolarRadiationPressureScenario(
            WorkingScenario);

    if (!FTGSimulationScenario::StaticStruct()->CompareScriptStruct(
            &ScenarioBeforeNormalization,
            &WorkingScenario,
            0))
    {
        CommitWorkingScenarioToDraft();
    }
    RefreshUiFromWorkingScenario();
}

bool UTGConfigSolarRadiationPressureWidgetBase::
NavigateToScenarioReviewIssue_Implementation(
    const FTGScenarioReviewIssue& Issue)
{
    RefreshFromCurrentDraft();

    if (Issue.ComponentId.IsValid())
    {
        HandleComponentRowSelected(Issue.ComponentId);

        ClearFaceSelection();
        if (Issue.StableTriangleIndex != INDEX_NONE)
        {
            SelectedTriangleIndex = Issue.StableTriangleIndex;
            SelectedTriangleIndices.Add(Issue.StableTriangleIndex);
            bEditingTriangleOverride = true;
        }
        else if (Issue.LogicalRegion > 0
            && Issue.LogicalRegion <= static_cast<uint8>(
                ETGSrpLogicalRegion::CylinderNegativeCap))
        {
            SelectedLogicalRegion =
                static_cast<ETGSrpLogicalRegion>(Issue.LogicalRegion);
            SelectedLogicalRegions.Add(SelectedLogicalRegion);
            bEditingTriangleOverride = false;
        }
        RefreshUiFromWorkingScenario();
        RefreshPreviewSelectionHighlight();
    }

    const FString& Path = Issue.Path;
    UWidget* Target = nullptr;
    if (Path.Contains(TEXT("bEnabled"))) Target = CHECK_EnableSrp;
    else if (Path.Contains(TEXT("bComputeEclipse"))) Target = CHECK_ComputeCelestialEclipses;
    else if (Path.Contains(TEXT("bComputeComponentShadows"))) Target = CHECK_ComputeComponentShadows;
    else if (Path.Contains(TEXT("GlobalFallbackOpticalProperties"))) Target = INPUT_GlobalAbsorption;
    else if (Path.Contains(TEXT("ProxyResolutionMode"))) Target = COMBO_ProxyResolutionMode;
    else if (Path.Contains(TEXT("CustomTargetTriangleCount"))) Target = INPUT_CustomTargetTriangleCount;
    else if (Path.Contains(TEXT("bUseGlobalFallback"))) Target = CHECK_UseGlobalFallbackOpticalProperties;
    else if (Path.Contains(TEXT("ComponentOpticalProperties"))) Target = INPUT_ComponentAbsorption;
    else if (Path.Contains(TEXT("LogicalRegionOverrides"))
        || Path.Contains(TEXT("TriangleOverrides"))) Target = INPUT_OverrideAbsorption;
    else if (Issue.ComponentId.IsValid()) Target = CHECK_IncludeComponentInSrpProxy;

    if (Issue.Severity == ETGScenarioReviewSeverity::Error)
        ShowError(Issue.Message);
    else
        ShowWarning(Issue.Message);

    if (Target == nullptr)
        return false;
    Target->SetIsEnabled(true);
    Target->SetKeyboardFocus();
    return true;
}

bool UTGConfigSolarRadiationPressureWidgetBase::
    ActivateSolarRadiationPressurePreview(
        ATGSpacecraftPreviewPawn* InPreviewPawn,
        ATGSpacecraftVisualActor* InSpacecraftActor)
{
    DeactivateSolarRadiationPressurePreview();
    ClearMessages();

    if (!IsValid(InPreviewPawn) ||
        !IsValid(InSpacecraftActor))
    {
        ShowError(FText::FromString(
            TEXT(
                "The shared spacecraft preview pawn or visual actor is not "
                "assigned. Use WBP_SimulationConfig's existing preview "
                "actor references when opening the SRP page.")));
        return false;
    }

    PreviewPawn = InPreviewPawn;
    PreviewSpacecraftActor = InSpacecraftActor;

    if (!PullWorkingScenarioFromDraft())
    {
        DeactivateSolarRadiationPressurePreview();
        return false;
    }

    const FTGSimulationScenario ScenarioBeforeNormalization =
        WorkingScenario;
    UTGSolarRadiationPressureEditingLibrary::
        NormalizeSolarRadiationPressureScenario(
            WorkingScenario);
    if (!FTGSimulationScenario::StaticStruct()->CompareScriptStruct(
            &ScenarioBeforeNormalization,
            &WorkingScenario,
            0) &&
        !CommitWorkingScenarioToDraft())
    {
        DeactivateSolarRadiationPressurePreview();
        return false;
    }
    RefreshUiFromWorkingScenario();

    PreviewPawn->SetPreviewSpacecraftActor(
        PreviewSpacecraftActor);

    FText BuildError;
    if (!PreviewSpacecraftActor->BuildFromScenario(
            WorkingScenario,
            BuildError))
    {
        const FText DisplayError =
            BuildError.IsEmpty()
                ? FText::FromString(
                    TEXT("The frontend spacecraft preview could not be built."))
                : BuildError;
        DeactivateSolarRadiationPressurePreview();
        ShowError(DisplayError);
        return false;
    }

    PreviewSpacecraftActor->SetActorHiddenInGame(false);
    PreviewPawn->OnPreviewSrpFacetClicked.AddUniqueDynamic(
        this,
        &UTGConfigSolarRadiationPressureWidgetBase::HandlePreviewSrpFacetClicked);
    PreviewPawn->OnPreviewSrpPrimitiveSurfaceClicked.AddUniqueDynamic(
        this,
        &UTGConfigSolarRadiationPressureWidgetBase::HandlePreviewSrpPrimitiveSurfaceClicked);
    PreviewPawn->SetSrpFacetSelectionEnabled(true);
    PreviewPawn->Set3DInteractionEnabled(true);
    bPreviewIntegrationActive = true;

    FText PreviewError;
    if (!PreviewSpacecraftActor->ShowSrpProxyPreview(
            WorkingScenario,
            PreviewError))
    {
        ShowWarning(
            PreviewError.IsEmpty()
                ? FText::FromString(
                    TEXT("The stored surface preview could not be shown."))
                : PreviewError);
    }

    if (!PreviewPawn->FrameSpacecraft(PreviewSpacecraftActor))
    {
        ShowWarning(FText::FromString(
            TEXT("The spacecraft preview is active but could not be framed.")));
    }

    RefreshPreviewSelectionHighlight();

    APlayerController* PlayerController = GetOwningPlayer();
    if (PlayerController == nullptr && GetWorld() != nullptr)
    {
        PlayerController = GetWorld()->GetFirstPlayerController();
    }
    if (PlayerController != nullptr)
    {
        UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(
            PlayerController,
            this,
            EMouseLockMode::DoNotLock,
            false,
            false);
        SetKeyboardFocus();
    }

    return true;
}

void UTGConfigSolarRadiationPressureWidgetBase::
    DeactivateSolarRadiationPressurePreview()
{
    const bool bHadPreviewState =
        bPreviewIntegrationActive ||
        IsValid(PreviewPawn) ||
        IsValid(PreviewSpacecraftActor);
    bPreviewIntegrationActive = false;

    if (IsValid(PreviewPawn))
    {
        PreviewPawn->OnPreviewSrpFacetClicked.RemoveDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandlePreviewSrpFacetClicked);
        PreviewPawn->OnPreviewSrpPrimitiveSurfaceClicked.RemoveDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandlePreviewSrpPrimitiveSurfaceClicked);
        PreviewPawn->SetSrpFacetSelectionEnabled(false);
        PreviewPawn->Set3DInteractionEnabled(false);
        PreviewPawn->ClearPreviewComponentSelection();
    }

    if (IsValid(PreviewSpacecraftActor))
    {
        PreviewSpacecraftActor->ClearSrpProxyPreview();
        PreviewSpacecraftActor->SetActorHiddenInGame(true);
    }

    APlayerController* PlayerController = GetOwningPlayer();
    if (PlayerController == nullptr && GetWorld() != nullptr)
    {
        PlayerController = GetWorld()->GetFirstPlayerController();
    }
    if (bHadPreviewState && PlayerController != nullptr)
    {
        UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(
            PlayerController,
            nullptr,
            EMouseLockMode::DoNotLock,
            false);
    }

    PreviewPawn = nullptr;
    PreviewSpacecraftActor = nullptr;
    HiddenPreviewComponentIds.Reset();
}

FTGComponentConfig*
UTGConfigSolarRadiationPressureWidgetBase::GetSelectedComponent()
{
    return WorkingScenario.Components.FindByPredicate(
        [this](const FTGComponentConfig& Component)
        {
            return Component.ComponentId == SelectedComponentId;
        });
}

const FTGComponentConfig*
UTGConfigSolarRadiationPressureWidgetBase::
    GetSelectedComponent() const
{
    return WorkingScenario.Components.FindByPredicate(
        [this](const FTGComponentConfig& Component)
        {
            return Component.ComponentId == SelectedComponentId;
        });
}

void UTGConfigSolarRadiationPressureWidgetBase::
    RefreshUiFromWorkingScenario()
{
    ClearMessages();

    const FTGSimulationScenario ScenarioBeforePreparation =
        WorkingScenario;
    FText PreparationSummary;
    FText PreparationWarning;
    FText PreparationError;
    const bool bGeometryReady =
        UTGSolarRadiationPressureEditingLibrary::
            PrepareSolarRadiationPressureGeometry(
                WorkingScenario,
                PreparationSummary,
                PreparationWarning,
                PreparationError);
    if (!FTGSimulationScenario::StaticStruct()->CompareScriptStruct(
            &ScenarioBeforePreparation,
            &WorkingScenario,
            0) &&
        !CommitWorkingScenarioToDraft())
    {
        return;
    }

    TGuardValue<bool> RefreshGuard(bRefreshing, true);

    const FTGSolarRadiationPressureConfig& Srp =
        WorkingScenario.SolarRadiationPressure;

    if (CHECK_EnableSrp != nullptr)
    {
        CHECK_EnableSrp->SetIsChecked(Srp.bEnabled);
    }

    if (VBOX_SrpSettings != nullptr)
    {
        VBOX_SrpSettings->SetIsEnabled(Srp.bEnabled);
    }

    if (CHECK_ComputeCelestialEclipses != nullptr)
    {
        CHECK_ComputeCelestialEclipses->SetIsChecked(
            Srp.bComputeEclipse);
    }

    if (VBOX_OccultingBodiesDetails != nullptr)
    {
        // The current product UI authors an empty list (all physical non-Sun
        // bodies), while an explicit subset imported from TGSCN is preserved.
        VBOX_OccultingBodiesDetails->SetVisibility(
            ESlateVisibility::Collapsed);
    }

    if (CHECK_ComputeComponentShadows != nullptr)
    {
        CHECK_ComputeComponentShadows->SetIsChecked(
            Srp.bComputeComponentShadows);
    }

    const FTGSrpOpticalProperties& DisplayedGlobalProperties =
        bHasGlobalOpticalInputWeights
            ? GlobalOpticalInputWeights
            : Srp.GlobalFallbackOpticalProperties;
    SetOpticalPropertyInputTexts(
        INPUT_GlobalAbsorption,
        INPUT_GlobalSpecularReflection,
        INPUT_GlobalDiffuseReflection,
        DisplayedGlobalProperties);

    if (TXT_GlobalOpticalPropertiesStatus != nullptr)
    {
        TXT_GlobalOpticalPropertiesStatus->SetText(FText::GetEmpty());
        TXT_GlobalOpticalPropertiesStatus->SetVisibility(
            ESlateVisibility::Collapsed);
    }

    const int32 TotalTriangleCount =
        UTGSolarRadiationPressureEditingLibrary::
            GetTotalGeneratedTriangleCount(
                WorkingScenario);

    if (TXT_TotalProxyTriangleCount != nullptr)
    {
        TXT_TotalProxyTriangleCount->SetText(FText::GetEmpty());
        TXT_TotalProxyTriangleCount->SetVisibility(
            ESlateVisibility::Collapsed);
    }

    if (TXT_SrpProxyPerformanceWarning != nullptr)
    {
        const bool bShowPerformanceWarning =
            TotalTriangleCount > 2000;

        TXT_SrpProxyPerformanceWarning->SetVisibility(
            bShowPerformanceWarning
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);

        if (bShowPerformanceWarning)
        {
            int32 IncludedComponentCount = 0;
            for (const FTGComponentConfig& Component :
                 WorkingScenario.Components)
            {
                IncludedComponentCount +=
                    Component.SolarRadiationPressure.bIncludedInProxy
                        ? 1
                        : 0;
            }
            const int64 ApproximateShadowChecks =
                3ll * TotalTriangleCount *
                FMath::Max(1, IncludedComponentCount);
            TXT_SrpProxyPerformanceWarning->SetText(
                FText::FromString(
                    FString::Printf(
                        TEXT(
                            "High-detail surface geometry (%d triangles) may "
                            "noticeably increase propagation time when "
                            "spacecraft self-shadowing is enabled (up to "
                            "about %lld shadow checks per force evaluation)."),
                        TotalTriangleCount,
                        ApproximateShadowChecks)));
        }
    }

    RefreshOccultingBodyControls();
    RefreshComponentControls();
    RefreshComponentRows();
    RefreshPreviewIntegration();
    if (bPreviewIntegrationActive &&
        IsValid(PreviewSpacecraftActor))
    {
        FText PreviewError;
        if (!PreviewSpacecraftActor->ShowSrpProxyPreview(
                WorkingScenario,
                PreviewError))
        {
            ShowWarning(
                PreviewError.IsEmpty()
                    ? FText::FromString(
                        TEXT("The stored surface preview could not be shown."))
                    : PreviewError);
        }
    }
    if (bPreviewIntegrationActive)
    {
        RefreshPreviewSelectionHighlight();
    }
    if (!PreparationWarning.IsEmpty())
    {
        ShowWarning(PreparationWarning);
    }
    if (!bGeometryReady && !PreparationError.IsEmpty())
    {
        ShowError(PreparationError);
    }
}

void UTGConfigSolarRadiationPressureWidgetBase::
    RefreshOccultingBodyControls()
{
    const TArray<FString>& SelectedBodies =
        WorkingScenario.SolarRadiationPressure
            .OccultingBodyNames;

    TArray<FString> AvailableBodies;
    for (
        const FString& Candidate :
        UTGSolarRadiationPressureEditingLibrary::
            GetAvailableOccultingBodyNames())
    {
        const bool bAlreadySelected =
            SelectedBodies.ContainsByPredicate(
                [&Candidate](const FString& Existing)
                {
                    return Existing.Equals(
                        Candidate,
                        ESearchCase::IgnoreCase);
                });

        if (!bAlreadySelected)
        {
            AvailableBodies.Add(Candidate);
        }
    }

    const FString PreviouslyAvailable =
        COMBO_AvailableOccultingBody != nullptr
            ? COMBO_AvailableOccultingBody->GetSelectedOption()
            : FString();

    TGConfigSolarRadiationPressurePrivate::ResetCombo(
        COMBO_AvailableOccultingBody,
        AvailableBodies,
        PreviouslyAvailable);

    const FString PreviouslySelected =
        COMBO_SelectedOccultingBody != nullptr
            ? COMBO_SelectedOccultingBody->GetSelectedOption()
            : FString();

    TGConfigSolarRadiationPressurePrivate::ResetCombo(
        COMBO_SelectedOccultingBody,
        SelectedBodies,
        PreviouslySelected);

    if (BTN_AddOccultingBody != nullptr)
    {
        BTN_AddOccultingBody->SetIsEnabled(!AvailableBodies.IsEmpty());
    }

    if (BTN_RemoveOccultingBody != nullptr)
    {
        BTN_RemoveOccultingBody->SetIsEnabled(!SelectedBodies.IsEmpty());
    }

    if (BTN_ClearOccultingBodies != nullptr)
    {
        BTN_ClearOccultingBodies->SetIsEnabled(!SelectedBodies.IsEmpty());
    }

    if (TXT_OccultingBodiesStatus != nullptr)
    {
        TXT_OccultingBodiesStatus->SetText(
            FText::FromString(
                SelectedBodies.IsEmpty()
                    ? TEXT(
                        "All physical non-Sun catalog bodies may occult "
                        "the Sun.")
                    : FString::Printf(
                        TEXT("Selected occulters: %s"),
                        *FString::Join(
                            SelectedBodies,
                            TEXT(", ")))));
    }
}

void UTGConfigSolarRadiationPressureWidgetBase::RefreshComponentRows()
{
    ComponentRows.Reset();
    if (VBOX_SrpComponentRows == nullptr)
    {
        return;
    }

    VBOX_SrpComponentRows->ClearChildren();

    TSet<FGuid> CurrentComponentIds;
    for (const FTGComponentConfig& Component : WorkingScenario.Components)
    {
        CurrentComponentIds.Add(Component.ComponentId);
    }
    for (auto It = HiddenPreviewComponentIds.CreateIterator(); It; ++It)
    {
        if (!CurrentComponentIds.Contains(*It))
        {
            It.RemoveCurrent();
        }
    }

    const FString SearchTerm =
        ComponentSearchText.TrimStartAndEnd();
    for (const FTGComponentConfig& Component : WorkingScenario.Components)
    {
        if (!SearchTerm.IsEmpty() &&
            !Component.Name.Contains(
                SearchTerm,
                ESearchCase::IgnoreCase))
        {
            continue;
        }

        UTGSrpComponentRowWidget* Row =
            CreateWidget<UTGSrpComponentRowWidget>(
                GetWorld(),
                UTGSrpComponentRowWidget::StaticClass());
        if (Row == nullptr)
        {
            continue;
        }

        Row->Configure(
            Component.ComponentId,
            Component.Name,
            Component.SolarRadiationPressure.bIncludedInProxy,
            !HiddenPreviewComponentIds.Contains(Component.ComponentId),
            Component.ComponentId == SelectedComponentId);
        Row->OnComponentSelected.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleComponentRowSelected);
        Row->OnIncludeChanged.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleComponentRowIncludeChanged);
        Row->OnPreviewVisibilityChanged.AddUniqueDynamic(
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::HandleComponentRowVisibilityChanged);

        VBOX_SrpComponentRows->AddChildToVerticalBox(Row);
        ComponentRows.Add(Row);
    }

}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleComponentSearchChanged(const FText& Text)
{
    if (bRefreshing)
    {
        return;
    }

    ComponentSearchText = Text.ToString();
    RefreshComponentRows();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    RefreshComponentControls()
{
    TArray<FString> ComponentNames;
    ComponentNames.Reserve(WorkingScenario.Components.Num());

    const FTGComponentConfig* SelectedComponent =
        GetSelectedComponent();

    if (
        SelectedComponent == nullptr &&
        !WorkingScenario.Components.IsEmpty())
    {
        SelectedComponentId =
            WorkingScenario.Components[0].ComponentId;
        SelectedComponent = &WorkingScenario.Components[0];
    }

    for (const FTGComponentConfig& Component
         : WorkingScenario.Components)
    {
        ComponentNames.Add(Component.Name);
    }

    TGConfigSolarRadiationPressurePrivate::ResetCombo(
        COMBO_SrpComponent,
        ComponentNames,
        SelectedComponent != nullptr
            ? SelectedComponent->Name
            : FString());

    const bool bHasSelection = SelectedComponent != nullptr;

    if (TXT_NoSrpComponentSelection != nullptr)
    {
        TXT_NoSrpComponentSelection->SetVisibility(
            bHasSelection
                ? ESlateVisibility::Collapsed
                : ESlateVisibility::Visible);
    }

    if (VBOX_SelectedSrpComponentEditor != nullptr)
    {
        VBOX_SelectedSrpComponentEditor->SetVisibility(
            bHasSelection
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
        VBOX_SelectedSrpComponentEditor->SetIsEnabled(
            WorkingScenario.SolarRadiationPressure.bEnabled);
    }

    if (!bHasSelection)
    {
        if (TXT_SelectedSrpComponentTitle != nullptr)
        {
            TXT_SelectedSrpComponentTitle->SetText(FText::GetEmpty());
        }
        if (TXT_SelectedSrpGeometrySource != nullptr)
        {
            TXT_SelectedSrpGeometrySource->SetText(FText::GetEmpty());
        }
        return;
    }

    const FTGComponentSrpConfig& Config =
        SelectedComponent->SolarRadiationPressure;
    if (TXT_SelectedSrpComponentTitle != nullptr)
    {
        TXT_SelectedSrpComponentTitle->SetText(
            FText::FromString(SelectedComponent->Name));
    }

    if (TXT_SelectedSrpGeometrySource != nullptr)
    {
        TXT_SelectedSrpGeometrySource->SetText(
            FText::FromString(
                GeometryDescription(*SelectedComponent)));
    }

    if (CHECK_IncludeComponentInSrpProxy != nullptr)
    {
        CHECK_IncludeComponentInSrpProxy->SetIsChecked(
            Config.bIncludedInProxy);
    }

    if (BORDER_ProxySettingsSection != nullptr)
    {
        BORDER_ProxySettingsSection->SetVisibility(
            ESlateVisibility::Visible);
    }

    TGConfigSolarRadiationPressurePrivate::ResetCombo(
        COMBO_ProxyResolutionMode,
        {
            TGConfigSolarRadiationPressurePrivate::
                AutomaticResolutionText,
            TGConfigSolarRadiationPressurePrivate::
                CustomResolutionText
        },
        Config.ProxyResolutionMode ==
            ETGSrpProxyResolutionMode::Automatic
            ? TGConfigSolarRadiationPressurePrivate::
                AutomaticResolutionText
            : TGConfigSolarRadiationPressurePrivate::
                CustomResolutionText);

    if (VBOX_CustomTargetTriangleCountDetails != nullptr)
    {
        VBOX_CustomTargetTriangleCountDetails->SetVisibility(
            Config.ProxyResolutionMode ==
                ETGSrpProxyResolutionMode::CustomTargetTriangleCount
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }

    if (INPUT_CustomTargetTriangleCount != nullptr)
    {
        INPUT_CustomTargetTriangleCount->SetText(
            FormatInteger(Config.CustomTargetTriangleCount));
    }

    if (TXT_ComponentProxyTriangleCount != nullptr)
    {
        TXT_ComponentProxyTriangleCount->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT("Current Surface: %d triangles"),
                    Config.GeneratedTriangleCount)));
    }

    if (TXT_ComponentProxyStatus != nullptr)
    {
        TXT_ComponentProxyStatus->SetText(FText::GetEmpty());
        TXT_ComponentProxyStatus->SetVisibility(
            ESlateVisibility::Collapsed);
    }

    if (CHECK_UseGlobalFallbackOpticalProperties != nullptr)
    {
        CHECK_UseGlobalFallbackOpticalProperties->SetIsChecked(
            Config.bUseGlobalFallbackOpticalProperties);
    }

    if (VBOX_ComponentOpticalPropertiesDetails != nullptr)
    {
        VBOX_ComponentOpticalPropertiesDetails->SetVisibility(
            Config.bUseGlobalFallbackOpticalProperties
                ? ESlateVisibility::Collapsed
                : ESlateVisibility::Visible);
    }

    const FTGSrpOpticalProperties* CachedComponentProperties =
        ComponentOpticalInputWeights.Find(
            SelectedComponent->ComponentId);
    const FTGSrpOpticalProperties& DisplayedComponentProperties =
        CachedComponentProperties != nullptr
            ? *CachedComponentProperties
            : Config.ComponentOpticalProperties;
    SetOpticalPropertyInputTexts(
        INPUT_ComponentAbsorption,
        INPUT_ComponentSpecularReflection,
        INPUT_ComponentDiffuseReflection,
        DisplayedComponentProperties);

    if (TXT_ComponentOpticalPropertiesStatus != nullptr)
    {
        TXT_ComponentOpticalPropertiesStatus->SetText(FText::GetEmpty());
        TXT_ComponentOpticalPropertiesStatus->SetVisibility(
            ESlateVisibility::Collapsed);
    }

    const TArray<FString> RegionNames =
        UTGSolarRadiationPressureEditingLibrary::
            GetLogicalRegionNames(*SelectedComponent);

    const bool bSphere =
        SelectedComponent->Visual.GeometrySource ==
            ETGComponentGeometrySource::Primitive &&
        SelectedComponent->Visual.PrimitiveType ==
            ETGPrimitiveGeometryType::Sphere;

    const bool bTriangleOverridesAvailable =
        !bSphere &&
        Config.GeneratedTriangleCount > 0 &&
        Config.GeneratedTriangleCount <= 32;

    const bool bHasAnyOverrideTarget =
        !RegionNames.IsEmpty() || bTriangleOverridesAvailable;

    if (TXT_PerFaceOpticsSummary != nullptr)
    {
        FString Summary;
        if (bSphere)
        {
            Summary = TEXT(
                "Per-face optical properties are unavailable for spheres; "
                "whole-component properties are applied.");
        }
        else if (Config.bProxyGenerationRequired)
        {
            Summary = TEXT(
                "Per-face editing will become available after the surface "
                "geometry is prepared.");
        }
        else if (!RegionNames.IsEmpty())
        {
            Summary = TEXT(
                "Optical properties can be assigned to individual surface "
                "regions in the 3D preview.");
        }
        else if (bTriangleOverridesAvailable)
        {
            Summary = TEXT(
                "Optical properties can be assigned to individual STL "
                "triangles in the 3D preview.");
        }
        else
        {
            Summary = FString::Printf(
                TEXT(
                    "This surface contains %d triangles; per-triangle "
                    "editing is available for surfaces with no more than "
                    "32 triangles."),
                Config.GeneratedTriangleCount);
        }
        TXT_PerFaceOpticsSummary->SetText(FText::FromString(Summary));
    }

    if (CHECK_ApplyOneOpticalConfiguration != nullptr)
    {
        CHECK_ApplyOneOpticalConfiguration->SetIsChecked(
            bHasAnyOverrideTarget
                ? Config.bApplyOneOpticalConfigurationToEntireComponent
                : true);
        CHECK_ApplyOneOpticalConfiguration->SetIsEnabled(
            bHasAnyOverrideTarget);
    }

    if (HBOX_ApplyOne != nullptr)
    {
        HBOX_ApplyOne->SetVisibility(
            bHasAnyOverrideTarget
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }

    const bool bShowFaceOptics =
        bHasAnyOverrideTarget &&
        !Config.bApplyOneOpticalConfigurationToEntireComponent;
    if (VBOX_OpticalOverrideDetails != nullptr)
    {
        VBOX_OpticalOverrideDetails->SetVisibility(
            bShowFaceOptics
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }
    if (BORDER_FaceOverridesSection != nullptr)
    {
        BORDER_FaceOverridesSection->SetVisibility(
            bShowFaceOptics
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }

    RefreshOverrideControls(*SelectedComponent);
}

void UTGConfigSolarRadiationPressureWidgetBase::
    RefreshOverrideControls(
        const FTGComponentConfig& Component)
{
    const FTGComponentSrpConfig& Config =
        Component.SolarRadiationPressure;
    const TArray<FString> RegionNames =
        UTGSolarRadiationPressureEditingLibrary::
            GetLogicalRegionNames(Component);
    const bool bSphere =
        Component.Visual.GeometrySource ==
            ETGComponentGeometrySource::Primitive &&
        Component.Visual.PrimitiveType ==
            ETGPrimitiveGeometryType::Sphere;

    TSet<int32> AvailableTriangleIndices;
    if (!bSphere &&
        !Config.bProxyGenerationRequired &&
        Config.GeneratedTriangleCount > 0 &&
        Config.GeneratedTriangleCount <= 32)
    {
        for (const FTGOpticalFacetConfig& Facet
             : WorkingScenario.SolarRadiationPressure.OpticalFacets)
        {
            if (Facet.ComponentId == Component.ComponentId &&
                Facet.LogicalRegion == ETGSrpLogicalRegion::None)
            {
                AvailableTriangleIndices.Add(Facet.StableTriangleIndex);
            }
        }
    }

    SelectedLogicalRegions.Remove(
        ETGSrpLogicalRegion::None);
    for (auto It = SelectedLogicalRegions.CreateIterator(); It; ++It)
    {
        const FString Name =
            UTGSolarRadiationPressureEditingLibrary::
                GetLogicalRegionDisplayName(*It);
        if (!RegionNames.Contains(Name))
        {
            It.RemoveCurrent();
        }
    }
    for (auto It = SelectedTriangleIndices.CreateIterator(); It; ++It)
    {
        if (!AvailableTriangleIndices.Contains(*It))
        {
            It.RemoveCurrent();
        }
    }

    SelectedLogicalRegion = ETGSrpLogicalRegion::None;
    for (const ETGSrpLogicalRegion Region : SelectedLogicalRegions)
    {
        SelectedLogicalRegion = Region;
        break;
    }
    SelectedTriangleIndex = INDEX_NONE;
    for (const int32 TriangleIndex : SelectedTriangleIndices)
    {
        SelectedTriangleIndex = TriangleIndex;
        break;
    }
    bEditingTriangleOverride = !SelectedTriangleIndices.IsEmpty();

    const int32 SelectionCount =
        SelectedLogicalRegions.Num() + SelectedTriangleIndices.Num();
    const bool bHasSelection = SelectionCount > 0;

    if (TXT_OverrideAvailability != nullptr)
    {
        FString Availability;
        if (bSphere)
        {
            Availability = TEXT(
                "This component uses one optical-property set.");
        }
        else if (!RegionNames.IsEmpty())
        {
            Availability = TEXT(
                "Click one or more faces in the 3D preview to select them.");
        }
        else if (Config.bProxyGenerationRequired)
        {
            Availability = TEXT(
                "Face selection is unavailable until the surface is ready.");
        }
        else if (Config.GeneratedTriangleCount > 32)
        {
            Availability = TEXT(
                "Individual face properties are available for meshes with "
                "32 triangles or fewer.");
        }
        else
        {
            Availability = TEXT(
                "Click one or more faces in the 3D preview to select them.");
        }

        TXT_OverrideAvailability->SetText(
            FText::FromString(Availability));
    }

    if (COMBO_OverrideTargetMode != nullptr)
    {
        COMBO_OverrideTargetMode->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (COMBO_OverrideTarget != nullptr)
    {
        COMBO_OverrideTarget->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (BTN_SelectOverrideInPreview != nullptr)
    {
        BTN_SelectOverrideInPreview->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (TXT_SelectedOverrideStableTriangleIndex != nullptr)
    {
        TXT_SelectedOverrideStableTriangleIndex->SetVisibility(
            ESlateVisibility::Collapsed);
    }

    TArray<FString> SelectionLabels;
    for (const ETGSrpLogicalRegion Region : SelectedLogicalRegions)
    {
        SelectionLabels.Add(
            UTGSolarRadiationPressureEditingLibrary::
                GetLogicalRegionDisplayName(Region));
    }
    TArray<int32> SortedTriangleIndices = SelectedTriangleIndices.Array();
    SortedTriangleIndices.Sort();
    for (const int32 TriangleIndex : SortedTriangleIndices)
    {
        SelectionLabels.Add(FString::Printf(TEXT("Triangle %d"), TriangleIndex));
    }
    SelectionLabels.Sort();

    if (TXT_SelectedSrpFaces != nullptr)
    {
        TXT_SelectedSrpFaces->SetText(
            FText::FromString(
                SelectionLabels.IsEmpty()
                    ? TEXT("No faces selected")
                    : FString::Printf(
                        TEXT("%d selected: %s"),
                        SelectionCount,
                        *FString::Join(SelectionLabels, TEXT(", ")))));
    }
    if (BTN_ClearSrpFaceSelection != nullptr)
    {
        BTN_ClearSrpFaceSelection->SetIsEnabled(bHasSelection);
    }

    FTGSrpOpticalProperties DisplayedProperties =
        TGConfigSolarRadiationPressurePrivate::
            GetInheritedProperties(
                WorkingScenario,
                Component);

    bool bHasStoredOverride = false;
    bool bAllSelectedFacesHaveStoredOverrides = false;
    bool bSelectedOverridesShareProperties = false;

    if (SelectionCount == 1 && SelectedTriangleIndex != INDEX_NONE)
    {
        const TArray<FTGOpticalFacetConfig>& GeneratedFacets =
            WorkingScenario.SolarRadiationPressure.OpticalFacets;
        const FTGOpticalFacetConfig* SelectedFacet =
            GeneratedFacets.FindByPredicate(
                    [this, &Component](
                        const FTGOpticalFacetConfig& Candidate)
                    {
                        return
                            Candidate.ComponentId == Component.ComponentId &&
                            Candidate.StableTriangleIndex ==
                                SelectedTriangleIndex;
                    });

        if (SelectedFacet != nullptr)
        {
            if (
                const FTGSrpLogicalRegionOverride* RegionOverride =
                    Config.LogicalRegionOverrides.FindByPredicate(
                        [SelectedFacet](
                            const FTGSrpLogicalRegionOverride& Candidate)
                        {
                            return Candidate.Region ==
                                SelectedFacet->LogicalRegion;
                        }))
            {
                DisplayedProperties =
                    RegionOverride->OpticalProperties;
            }
        }

        if (
            const FTGSrpTriangleOverride* Override =
                Config.TriangleOverrides.FindByPredicate(
                    [this](const FTGSrpTriangleOverride& Candidate)
                    {
                        return
                            Candidate.ProxyTriangleIndex ==
                            SelectedTriangleIndex;
                    }))
        {
            DisplayedProperties = Override->OpticalProperties;
            bHasStoredOverride = true;
        }
    }
    else if (SelectionCount == 1 &&
             SelectedLogicalRegion != ETGSrpLogicalRegion::None)
    {
        if (
            const FTGSrpLogicalRegionOverride* Override =
                Config.LogicalRegionOverrides.FindByPredicate(
                    [this](const FTGSrpLogicalRegionOverride& Candidate)
                    {
                        return
                            Candidate.Region ==
                            SelectedLogicalRegion;
                    }))
        {
            DisplayedProperties = Override->OpticalProperties;
            bHasStoredOverride = true;
        }
    }

    if (SelectionCount > 1)
    {
        bAllSelectedFacesHaveStoredOverrides = true;
        bSelectedOverridesShareProperties = true;
        bool bFoundFirstOverride = false;
        FTGSrpOpticalProperties CommonProperties;

        const auto AccumulateOverride =
            [&bHasStoredOverride,
             &bAllSelectedFacesHaveStoredOverrides,
             &bSelectedOverridesShareProperties,
             &bFoundFirstOverride,
             &CommonProperties](
                const FTGSrpOpticalProperties* OverrideProperties)
            {
                if (OverrideProperties == nullptr)
                {
                    bAllSelectedFacesHaveStoredOverrides = false;
                    return;
                }

                bHasStoredOverride = true;
                if (!bFoundFirstOverride)
                {
                    CommonProperties = *OverrideProperties;
                    bFoundFirstOverride = true;
                    return;
                }

                constexpr double ComparisonTolerance = 1.0e-12;
                bSelectedOverridesShareProperties =
                    bSelectedOverridesShareProperties &&
                    FMath::IsNearlyEqual(
                        CommonProperties.AbsorptionFraction,
                        OverrideProperties->AbsorptionFraction,
                        ComparisonTolerance) &&
                    FMath::IsNearlyEqual(
                        CommonProperties.SpecularReflectionFraction,
                        OverrideProperties->SpecularReflectionFraction,
                        ComparisonTolerance) &&
                    FMath::IsNearlyEqual(
                        CommonProperties.DiffuseReflectionFraction,
                        OverrideProperties->DiffuseReflectionFraction,
                        ComparisonTolerance);
            };

        for (const ETGSrpLogicalRegion Region : SelectedLogicalRegions)
        {
            const FTGSrpLogicalRegionOverride* Override =
                Config.LogicalRegionOverrides.FindByPredicate(
                    [Region](const FTGSrpLogicalRegionOverride& Candidate)
                    {
                        return Candidate.Region == Region;
                    });
            AccumulateOverride(
                Override != nullptr
                    ? &Override->OpticalProperties
                    : nullptr);
        }

        for (const int32 TriangleIndex : SelectedTriangleIndices)
        {
            const FTGSrpTriangleOverride* Override =
                Config.TriangleOverrides.FindByPredicate(
                    [TriangleIndex](const FTGSrpTriangleOverride& Candidate)
                    {
                        return Candidate.ProxyTriangleIndex == TriangleIndex;
                    });
            AccumulateOverride(
                Override != nullptr
                    ? &Override->OpticalProperties
                    : nullptr);
        }

        if (bAllSelectedFacesHaveStoredOverrides &&
            bSelectedOverridesShareProperties &&
            bFoundFirstOverride)
        {
            DisplayedProperties = CommonProperties;
        }
    }
    else if (SelectionCount == 1)
    {
        bAllSelectedFacesHaveStoredOverrides = bHasStoredOverride;
        bSelectedOverridesShareProperties = bHasStoredOverride;
    }

    if (INPUT_OverrideAbsorption != nullptr)
    {
        INPUT_OverrideAbsorption->SetText(
            FormatDouble(
                DisplayedProperties.AbsorptionFraction));
        INPUT_OverrideAbsorption->SetIsEnabled(bHasSelection);
    }

    if (INPUT_OverrideSpecularReflection != nullptr)
    {
        INPUT_OverrideSpecularReflection->SetText(
            FormatDouble(
                DisplayedProperties.SpecularReflectionFraction));
        INPUT_OverrideSpecularReflection->SetIsEnabled(bHasSelection);
    }

    if (INPUT_OverrideDiffuseReflection != nullptr)
    {
        INPUT_OverrideDiffuseReflection->SetText(
            FormatDouble(
                DisplayedProperties.DiffuseReflectionFraction));
        INPUT_OverrideDiffuseReflection->SetIsEnabled(bHasSelection);
    }

    if (BTN_ApplyOverrideOpticalProperties != nullptr)
    {
        BTN_ApplyOverrideOpticalProperties->SetIsEnabled(bHasSelection);
    }

    if (BTN_ClearOverrideOpticalProperties != nullptr)
    {
        const bool bComponentHasStoredOverrides =
            !Config.LogicalRegionOverrides.IsEmpty() ||
            !Config.TriangleOverrides.IsEmpty();
        BTN_ClearOverrideOpticalProperties->SetIsEnabled(
            bComponentHasStoredOverrides);
    }

    if (BTN_RemoveSelectedOverrideOpticalProperties != nullptr)
    {
        BTN_RemoveSelectedOverrideOpticalProperties->SetIsEnabled(
            bHasSelection && bHasStoredOverride);
    }

    if (TXT_OverrideOpticalPropertiesStatus != nullptr)
    {
        TArray<FString> OverrideLines;
        OverrideLines.Reserve(
            Config.LogicalRegionOverrides.Num() +
            Config.TriangleOverrides.Num());

        const auto FormatOverrideLine =
            [](const FString& FaceLabel,
               const FTGSrpOpticalProperties& Properties)
            {
                return FString::Printf(
                    TEXT(
                        "%s: Absorption %s | Specular %s | Diffuse %s"),
                    *FaceLabel,
                    *FormatDouble(Properties.AbsorptionFraction).ToString(),
                    *FormatDouble(
                        Properties.SpecularReflectionFraction).ToString(),
                    *FormatDouble(
                        Properties.DiffuseReflectionFraction).ToString());
            };

        for (const FTGSrpLogicalRegionOverride& Override
             : Config.LogicalRegionOverrides)
        {
            OverrideLines.Add(
                FormatOverrideLine(
                    UTGSolarRadiationPressureEditingLibrary::
                        GetLogicalRegionDisplayName(Override.Region),
                    Override.OpticalProperties));
        }

        for (const FTGSrpTriangleOverride& Override
             : Config.TriangleOverrides)
        {
            OverrideLines.Add(
                FormatOverrideLine(
                    FString::Printf(
                        TEXT("Triangle %d"),
                        Override.ProxyTriangleIndex),
                    Override.OpticalProperties));
        }

        OverrideLines.Sort();
        const FString Status = OverrideLines.IsEmpty()
            ? FString()
            : FString::Printf(
                TEXT("Face Overrides\n%s"),
                *FString::Join(OverrideLines, TEXT("\n")));

        TXT_OverrideOpticalPropertiesStatus->SetText(
            FText::FromString(Status));
        TXT_OverrideOpticalPropertiesStatus->SetVisibility(
            Status.IsEmpty()
                ? ESlateVisibility::Collapsed
                : ESlateVisibility::Visible);
    }
}

void UTGConfigSolarRadiationPressureWidgetBase::
    RefreshValidationStatus()
{
    FText Warning;
    FText Error;

    if (!UTGSolarRadiationPressureEditingLibrary::
            ValidateSolarRadiationPressureAuthoring(
                WorkingScenario,
                Warning,
                Error))
    {
        ShowError(Error);
    }

    if (!Warning.IsEmpty())
    {
        ShowWarning(Warning);
    }
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleEnableSrpChanged(bool bIsChecked)
{
    if (bRefreshing)
    {
        return;
    }

    WorkingScenario.SolarRadiationPressure.bEnabled = bIsChecked;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleComputeCelestialEclipsesChanged(bool bIsChecked)
{
    if (bRefreshing)
    {
        return;
    }

    WorkingScenario.SolarRadiationPressure.bComputeEclipse =
        bIsChecked;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleAddOccultingBodyClicked()
{
    if (
        bRefreshing ||
        COMBO_AvailableOccultingBody == nullptr)
    {
        return;
    }

    const FString BodyName =
        COMBO_AvailableOccultingBody->GetSelectedOption();

    if (!BodyName.IsEmpty())
    {
        WorkingScenario.SolarRadiationPressure
            .OccultingBodyNames.AddUnique(BodyName);
        CommitWorkingScenarioToDraft();
        RefreshUiFromWorkingScenario();
    }
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleRemoveOccultingBodyClicked()
{
    if (
        bRefreshing ||
        COMBO_SelectedOccultingBody == nullptr)
    {
        return;
    }

    const FString BodyName =
        COMBO_SelectedOccultingBody->GetSelectedOption();

    WorkingScenario.SolarRadiationPressure
        .OccultingBodyNames.RemoveAll(
            [&BodyName](const FString& Candidate)
            {
                return Candidate.Equals(
                    BodyName,
                    ESearchCase::IgnoreCase);
            });

    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleClearOccultingBodiesClicked()
{
    if (bRefreshing)
    {
        return;
    }

    WorkingScenario.SolarRadiationPressure
        .OccultingBodyNames.Reset();
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleComputeComponentShadowsChanged(bool bIsChecked)
{
    if (bRefreshing)
    {
        return;
    }

    WorkingScenario.SolarRadiationPressure
        .bComputeComponentShadows = bIsChecked;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleApplyGlobalOpticalPropertiesClicked()
{
    CommitGlobalOpticalPropertyInputs(true);
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleGlobalOpticalPropertiesCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod)
{
    (void)Text;
    if (bRefreshing || CommitMethod == ETextCommit::OnCleared)
    {
        return;
    }

    CommitGlobalOpticalPropertyInputs(false);
}

bool UTGConfigSolarRadiationPressureWidgetBase::
    CommitGlobalOpticalPropertyInputs(bool bNormalizeDisplayedValues)
{
    const FTGSrpOpticalProperties PreviousInput =
        bHasGlobalOpticalInputWeights
            ? GlobalOpticalInputWeights
            : WorkingScenario.SolarRadiationPressure
                .GlobalFallbackOpticalProperties;

    FTGSrpOpticalProperties InputWeights;
    FText Error;
    if (!ParseOpticalPropertyWeights(
            INPUT_GlobalAbsorption,
            INPUT_GlobalSpecularReflection,
            INPUT_GlobalDiffuseReflection,
            InputWeights,
            Error))
    {
        SetOpticalPropertyInputTexts(
            INPUT_GlobalAbsorption,
            INPUT_GlobalSpecularReflection,
            INPUT_GlobalDiffuseReflection,
            PreviousInput);
        ShowError(Error);
        return false;
    }

    FTGSrpOpticalProperties NormalizedProperties = InputWeights;
    UTGSolarRadiationPressureEditingLibrary::
        NormalizeOpticalPropertyWeights(NormalizedProperties);

    const FTGSimulationScenario PreviousScenario = WorkingScenario;
    WorkingScenario.SolarRadiationPressure
        .GlobalFallbackOpticalProperties = NormalizedProperties;

    for (const FTGComponentConfig& Component : WorkingScenario.Components)
    {
        FText ResolveError;
        if (!UTGSolarRadiationPressureEditingLibrary::
                ResolveComponentOpticalProperties(
                    WorkingScenario,
                    Component.ComponentId,
                    ResolveError))
        {
            WorkingScenario = PreviousScenario;
            SetOpticalPropertyInputTexts(
                INPUT_GlobalAbsorption,
                INPUT_GlobalSpecularReflection,
                INPUT_GlobalDiffuseReflection,
                PreviousInput);
            ShowError(ResolveError);
            return false;
        }
    }

    if (!CommitWorkingScenarioToDraft())
    {
        WorkingScenario = PreviousScenario;
        SetOpticalPropertyInputTexts(
            INPUT_GlobalAbsorption,
            INPUT_GlobalSpecularReflection,
            INPUT_GlobalDiffuseReflection,
            PreviousInput);
        return false;
    }

    GlobalOpticalInputWeights =
        bNormalizeDisplayedValues
            ? NormalizedProperties
            : InputWeights;
    bHasGlobalOpticalInputWeights = true;
    RefreshUiFromWorkingScenario();
    return true;
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleComponentSelectionChanged(
        FString SelectedItem,
        ESelectInfo::Type SelectionType)
{
    if (bRefreshing || SelectionType == ESelectInfo::Direct)
    {
        return;
    }

    const FTGComponentConfig* Component =
        WorkingScenario.Components.FindByPredicate(
            [&SelectedItem](const FTGComponentConfig& Candidate)
            {
                return Candidate.Name == SelectedItem;
            });

    SelectedComponentId =
        Component != nullptr
            ? Component->ComponentId
            : FGuid();
    ClearFaceSelection();

    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleComponentRowSelected(FGuid ComponentId)
{
    if (bRefreshing || !ComponentId.IsValid())
    {
        return;
    }

    SelectedComponentId = ComponentId;
    ClearFaceSelection();

    bool bPreviewSelectionSucceeded = true;
    FText SelectionError;
    if (bPreviewIntegrationActive && IsValid(PreviewPawn))
    {
        bPreviewSelectionSucceeded =
            PreviewPawn->SelectPreviewComponent(
            ComponentId,
            SelectionError);
    }

    RefreshUiFromWorkingScenario();
    if (!bPreviewSelectionSucceeded)
    {
        ShowInspectorWarning(
            SelectionError.IsEmpty()
                ? FText::FromString(
                    TEXT("The selected component could not be highlighted in the 3D preview."))
                : SelectionError);
    }
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleComponentRowIncludeChanged(
        FGuid ComponentId,
        bool bIncluded)
{
    if (bRefreshing)
    {
        return;
    }

    SelectedComponentId = ComponentId;
    HandleIncludeComponentChanged(bIncluded);
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleComponentRowVisibilityChanged(
        FGuid ComponentId,
        bool bVisible)
{
    if (bRefreshing || !ComponentId.IsValid())
    {
        return;
    }

    if (bVisible)
    {
        HiddenPreviewComponentIds.Remove(ComponentId);
    }
    else
    {
        HiddenPreviewComponentIds.Add(ComponentId);
        if (SelectedComponentId == ComponentId)
        {
            ClearFaceSelection();
            if (IsValid(PreviewPawn))
            {
                PreviewPawn->ClearPreviewComponentSelection();
            }
        }
    }

    if (IsValid(PreviewSpacecraftActor))
    {
        FText VisibilityError;
        if (!PreviewSpacecraftActor->SetComponentVisualVisibility(
                ComponentId,
                bVisible,
                VisibilityError))
        {
            if (bVisible)
            {
                HiddenPreviewComponentIds.Add(ComponentId);
            }
            else
            {
                HiddenPreviewComponentIds.Remove(ComponentId);
            }
            ShowInspectorWarning(
                VisibilityError.IsEmpty()
                    ? FText::FromString(
                        TEXT("The component visibility could not be changed."))
                    : VisibilityError);
        }
    }

    RefreshComponentRows();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandlePreviewSrpFacetClicked(
        FGuid ComponentId,
        int32 StableTriangleIndex)
{
    if (!ComponentId.IsValid() || StableTriangleIndex < 0)
    {
        return;
    }

    FTGComponentConfig* Component =
        WorkingScenario.Components.FindByPredicate(
            [&ComponentId](const FTGComponentConfig& Candidate)
            {
                return Candidate.ComponentId == ComponentId;
            });
    const FTGOpticalFacetConfig* Facet =
        WorkingScenario.SolarRadiationPressure.OpticalFacets.FindByPredicate(
            [&ComponentId, StableTriangleIndex](
                const FTGOpticalFacetConfig& Candidate)
            {
                return Candidate.ComponentId == ComponentId &&
                    Candidate.StableTriangleIndex == StableTriangleIndex;
            });
    if (Component == nullptr || Facet == nullptr)
    {
        return;
    }

    const bool bSphere =
        Component->Visual.GeometrySource ==
            ETGComponentGeometrySource::Primitive &&
        Component->Visual.PrimitiveType ==
            ETGPrimitiveGeometryType::Sphere;
    if (bSphere)
    {
        ShowInspectorWarning(FText::FromString(
            TEXT("Spheres use whole-component optical properties.")));
        return;
    }

    if (SelectedComponentId != ComponentId)
    {
        SelectedComponentId = ComponentId;
        ClearFaceSelection();
    }

    FTGComponentSrpConfig& Config =
        Component->SolarRadiationPressure;
    if (Config.bProxyGenerationRequired)
    {
        ShowInspectorWarning(FText::FromString(
            TEXT(
                "This component's surface geometry is not ready for face "
                "selection.")));
        return;
    }

    Config.bApplyOneOpticalConfigurationToEntireComponent = false;

    if (Facet->LogicalRegion != ETGSrpLogicalRegion::None)
    {
        SelectedTriangleIndices.Reset();
        if (SelectedLogicalRegions.Contains(Facet->LogicalRegion))
        {
            SelectedLogicalRegions.Remove(Facet->LogicalRegion);
        }
        else
        {
            SelectedLogicalRegions.Add(Facet->LogicalRegion);
        }
    }
    else
    {
        if (Config.GeneratedTriangleCount > 32)
        {
            ShowInspectorWarning(FText::FromString(
                TEXT("Per-triangle optical editing is limited to surfaces with 32 triangles or fewer.")));
            return;
        }

        SelectedLogicalRegions.Reset();
        if (SelectedTriangleIndices.Contains(StableTriangleIndex))
        {
            SelectedTriangleIndices.Remove(StableTriangleIndex);
        }
        else
        {
            SelectedTriangleIndices.Add(StableTriangleIndex);
        }
    }

    FText ResolveError;
    UTGSolarRadiationPressureEditingLibrary::
        ResolveComponentOpticalProperties(
            WorkingScenario,
            ComponentId,
            ResolveError);
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandlePreviewSrpPrimitiveSurfaceClicked(
        FGuid ComponentId,
        FVector PrimitiveLocalNormal)
{
    FTGComponentConfig* Component =
        WorkingScenario.Components.FindByPredicate(
            [&ComponentId](const FTGComponentConfig& Candidate)
            {
                return Candidate.ComponentId == ComponentId;
            });
    if (Component == nullptr ||
        Component->Visual.GeometrySource !=
            ETGComponentGeometrySource::Primitive)
    {
        ShowInspectorWarning(FText::FromString(
            TEXT(
                "STL face selection becomes available after the component's "
                "surface geometry is prepared.")));
        return;
    }

    if (Component->Visual.PrimitiveType ==
        ETGPrimitiveGeometryType::Sphere)
    {
        ShowInspectorWarning(FText::FromString(
            TEXT("Sphere optics are intentionally whole-component only.")));
        return;
    }

    ETGSrpLogicalRegion Region = ETGSrpLogicalRegion::None;
    if (Component->Visual.PrimitiveType ==
        ETGPrimitiveGeometryType::Cylinder)
    {
        if (FMath::Abs(PrimitiveLocalNormal.Z) > 0.7)
        {
            Region = PrimitiveLocalNormal.Z >= 0.0
                ? ETGSrpLogicalRegion::CylinderPositiveCap
                : ETGSrpLogicalRegion::CylinderNegativeCap;
        }
        else
        {
            Region = ETGSrpLogicalRegion::CylinderSide;
        }
    }
    else
    {
        const FVector AbsoluteNormal(
            FMath::Abs(PrimitiveLocalNormal.X),
            FMath::Abs(PrimitiveLocalNormal.Y),
            FMath::Abs(PrimitiveLocalNormal.Z));
        if (AbsoluteNormal.X >= AbsoluteNormal.Y &&
            AbsoluteNormal.X >= AbsoluteNormal.Z)
        {
            Region = PrimitiveLocalNormal.X >= 0.0
                ? ETGSrpLogicalRegion::BoxPositiveX
                : ETGSrpLogicalRegion::BoxNegativeX;
        }
        else if (AbsoluteNormal.Y >= AbsoluteNormal.Z)
        {
            // Unreal visual space reflects the component's right-handed Y.
            Region = PrimitiveLocalNormal.Y >= 0.0
                ? ETGSrpLogicalRegion::BoxNegativeY
                : ETGSrpLogicalRegion::BoxPositiveY;
        }
        else
        {
            Region = PrimitiveLocalNormal.Z >= 0.0
                ? ETGSrpLogicalRegion::BoxPositiveZ
                : ETGSrpLogicalRegion::BoxNegativeZ;
        }
    }

    if (SelectedComponentId != ComponentId)
    {
        SelectedComponentId = ComponentId;
        ClearFaceSelection();
    }

    SelectedTriangleIndices.Reset();
    if (SelectedLogicalRegions.Contains(Region))
    {
        SelectedLogicalRegions.Remove(Region);
    }
    else
    {
        SelectedLogicalRegions.Add(Region);
    }

    Component->SolarRadiationPressure
        .bApplyOneOpticalConfigurationToEntireComponent = false;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleClearFaceSelectionClicked()
{
    ClearFaceSelection();
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::ClearFaceSelection()
{
    SelectedLogicalRegions.Reset();
    SelectedTriangleIndices.Reset();
    SelectedLogicalRegion = ETGSrpLogicalRegion::None;
    SelectedTriangleIndex = INDEX_NONE;
    bEditingTriangleOverride = false;
    RefreshPreviewSelectionHighlight();
}

bool UTGConfigSolarRadiationPressureWidgetBase::
    IsLogicalRegionSelected(ETGSrpLogicalRegion Region) const
{
    return SelectedLogicalRegions.Contains(Region);
}

TArray<int32> UTGConfigSolarRadiationPressureWidgetBase::
    GetSelectedFacetIndices() const
{
    TSet<int32> Result = SelectedTriangleIndices;
    for (const FTGOpticalFacetConfig& Facet
         : WorkingScenario.SolarRadiationPressure.OpticalFacets)
    {
        if (Facet.ComponentId == SelectedComponentId &&
            SelectedLogicalRegions.Contains(Facet.LogicalRegion))
        {
            Result.Add(Facet.StableTriangleIndex);
        }
    }
    TArray<int32> Sorted = Result.Array();
    Sorted.Sort();
    return Sorted;
}

void UTGConfigSolarRadiationPressureWidgetBase::
    RefreshPreviewIntegration()
{
    if (!bPreviewIntegrationActive)
    {
        return;
    }

    if (!IsValid(PreviewPawn) ||
        !IsValid(PreviewSpacecraftActor))
    {
        DeactivateSolarRadiationPressurePreview();
        ShowWarning(FText::FromString(
            TEXT(
                "The explicitly assigned spacecraft preview became "
                "unavailable. Reopen the SRP page to reconnect it.")));
        return;
    }

    if (PreviewPawn->GetPreviewSpacecraftActor() !=
        PreviewSpacecraftActor)
    {
        PreviewPawn->SetPreviewSpacecraftActor(
            PreviewSpacecraftActor);
    }

    for (const FGuid& ComponentId : HiddenPreviewComponentIds)
    {
        FText VisibilityError;
        PreviewSpacecraftActor->SetComponentVisualVisibility(
            ComponentId,
            false,
            VisibilityError);
    }
    PreviewPawn->SetSrpFacetSelectionEnabled(true);
}

void UTGConfigSolarRadiationPressureWidgetBase::
    RefreshPreviewSelectionHighlight()
{
    if (!bPreviewIntegrationActive ||
        !IsValid(PreviewSpacecraftActor))
    {
        return;
    }

    FText HighlightError;
    if (!SelectedLogicalRegions.IsEmpty())
    {
        TArray<ETGSrpLogicalRegion> Regions =
            SelectedLogicalRegions.Array();
        Regions.Sort(
            [](ETGSrpLogicalRegion Left, ETGSrpLogicalRegion Right)
            {
                return static_cast<uint8>(Left) <
                    static_cast<uint8>(Right);
            });

        if (!PreviewSpacecraftActor->
                SetSrpPrimitiveRegionSelectionHighlight(
                WorkingScenario,
                SelectedComponentId,
                Regions,
                HighlightError) &&
            !HighlightError.IsEmpty())
        {
            ShowInspectorWarning(HighlightError);
        }
    }
    else
    {
        if (!PreviewSpacecraftActor->SetSrpFacetSelectionHighlight(
                SelectedComponentId,
                SelectedTriangleIndices.Array(),
                HighlightError) &&
            !HighlightError.IsEmpty())
        {
            ShowInspectorWarning(HighlightError);
        }
    }
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleIncludeComponentChanged(bool bIsChecked)
{
    if (bRefreshing)
    {
        return;
    }

    FTGComponentConfig* Component = GetSelectedComponent();
    if (Component == nullptr)
    {
        return;
    }

    Component->SolarRadiationPressure.bIncludedInProxy = bIsChecked;

    if (bIsChecked)
    {
        FText Error;
        UTGSolarRadiationPressureEditingLibrary::
            MarkComponentProxyGenerationRequired(
                WorkingScenario,
                Component->ComponentId,
                TEXT("Surface geometry will be prepared automatically."),
                Error);
    }
    else
    {
        UTGSolarRadiationPressureEditingLibrary::
            NormalizeSolarRadiationPressureScenario(
                WorkingScenario);
    }

    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleProxyResolutionModeChanged(
        FString SelectedItem,
        ESelectInfo::Type SelectionType)
{
    if (bRefreshing || SelectionType == ESelectInfo::Direct)
    {
        return;
    }

    FTGComponentConfig* Component = GetSelectedComponent();
    if (Component == nullptr)
    {
        return;
    }

    const ETGSrpProxyResolutionMode NewMode =
        SelectedItem ==
            TGConfigSolarRadiationPressurePrivate::
                CustomResolutionText
            ? ETGSrpProxyResolutionMode::CustomTargetTriangleCount
            : ETGSrpProxyResolutionMode::Automatic;

    if (
        Component->SolarRadiationPressure.ProxyResolutionMode ==
        NewMode)
    {
        return;
    }

    if (!Component->SolarRadiationPressure.TriangleOverrides.IsEmpty())
    {
        PendingProxyInvalidationComponentId = Component->ComponentId;
        PendingProxyInputChange =
            ETGSrpPendingProxyInputChange::ResolutionMode;
        PendingProxyResolutionMode = NewMode;
        SetProxyInvalidationConfirmationVisible(
            true,
            FString::Printf(
                TEXT(
                    "Changing the surface detail for '%s' will discard "
                    "%d per-triangle override(s). Primitive surface-region "
                    "overrides will be preserved. Continue?"),
                *Component->Name,
                Component->SolarRadiationPressure.TriangleOverrides.Num()));
        return;
    }

    Component->SolarRadiationPressure.ProxyResolutionMode = NewMode;

    FText Error;
    if (!UTGSolarRadiationPressureEditingLibrary::
            DiscardTriangleOverridesAndGeneratedProxy(
                WorkingScenario,
                Component->ComponentId,
                Error))
    {
        RefreshUiFromWorkingScenario();
        ShowInspectorError(Error);
        return;
    }

    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleCustomTargetTriangleCountCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod)
{
    if (bRefreshing || CommitMethod == ETextCommit::OnCleared)
    {
        return;
    }

    FTGComponentConfig* Component = GetSelectedComponent();
    if (Component == nullptr)
    {
        return;
    }

    int32 TargetTriangleCount = 0;
    FText Error;
    if (!UTGHudFormattingLibrary::ParseHudInteger(
            Text,
            TargetTriangleCount,
            Error) ||
        TargetTriangleCount <= 0)
    {
        RefreshUiFromWorkingScenario();
        ShowInspectorError(
            FText::FromString(
                TEXT(
                    "Custom target triangle count must be a positive "
                    "integer.")));
        return;
    }

    if (
        Component->SolarRadiationPressure.CustomTargetTriangleCount ==
        TargetTriangleCount)
    {
        return;
    }

    if (!Component->SolarRadiationPressure.TriangleOverrides.IsEmpty())
    {
        PendingProxyInvalidationComponentId = Component->ComponentId;
        PendingProxyInputChange =
            ETGSrpPendingProxyInputChange::CustomTargetTriangleCount;
        PendingCustomTargetTriangleCount = TargetTriangleCount;
        SetProxyInvalidationConfirmationVisible(
            true,
            FString::Printf(
                TEXT(
                    "Changing the custom target for '%s' will discard %d "
                    "per-triangle override(s). Primitive surface-region "
                    "overrides will be preserved. Continue?"),
                *Component->Name,
                Component->SolarRadiationPressure.TriangleOverrides.Num()));
        return;
    }

    Component->SolarRadiationPressure.CustomTargetTriangleCount =
        TargetTriangleCount;

    if (!UTGSolarRadiationPressureEditingLibrary::
            DiscardTriangleOverridesAndGeneratedProxy(
                WorkingScenario,
                Component->ComponentId,
                Error))
    {
        RefreshUiFromWorkingScenario();
        ShowInspectorError(Error);
        return;
    }

    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleUseGlobalFallbackChanged(bool bIsChecked)
{
    if (bRefreshing)
    {
        return;
    }

    FTGComponentConfig* Component = GetSelectedComponent();
    if (Component == nullptr)
    {
        return;
    }

    Component->SolarRadiationPressure
        .bUseGlobalFallbackOpticalProperties = bIsChecked;

    FText Error;
    if (!UTGSolarRadiationPressureEditingLibrary::
            ResolveComponentOpticalProperties(
                WorkingScenario,
                Component->ComponentId,
                Error))
    {
        RefreshUiFromWorkingScenario();
        ShowInspectorError(Error);
        return;
    }

    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleApplyOneOpticalConfigurationChanged(bool bIsChecked)
{
    if (bRefreshing)
    {
        return;
    }

    FTGComponentConfig* Component = GetSelectedComponent();
    if (Component == nullptr)
    {
        return;
    }

    Component->SolarRadiationPressure
        .bApplyOneOpticalConfigurationToEntireComponent = bIsChecked;

    if (bIsChecked)
    {
        ClearFaceSelection();
    }

    FText Error;
    UTGSolarRadiationPressureEditingLibrary::
        ResolveComponentOpticalProperties(
            WorkingScenario,
            Component->ComponentId,
            Error);

    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleApplyComponentOpticalPropertiesClicked()
{
    CommitComponentOpticalPropertyInputs(true);
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleComponentOpticalPropertiesCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod)
{
    (void)Text;
    if (bRefreshing || CommitMethod == ETextCommit::OnCleared)
    {
        return;
    }

    CommitComponentOpticalPropertyInputs(false);
}

bool UTGConfigSolarRadiationPressureWidgetBase::
    CommitComponentOpticalPropertyInputs(bool bNormalizeDisplayedValues)
{
    FTGComponentConfig* Component = GetSelectedComponent();
    if (Component == nullptr)
    {
        return false;
    }

    const FGuid ComponentId = Component->ComponentId;
    const FTGSrpOpticalProperties* CachedInput =
        ComponentOpticalInputWeights.Find(ComponentId);
    const FTGSrpOpticalProperties PreviousInput =
        CachedInput != nullptr
            ? *CachedInput
            : Component->SolarRadiationPressure
                .ComponentOpticalProperties;

    FTGSrpOpticalProperties InputWeights;
    FText Error;
    if (!ParseOpticalPropertyWeights(
            INPUT_ComponentAbsorption,
            INPUT_ComponentSpecularReflection,
            INPUT_ComponentDiffuseReflection,
            InputWeights,
            Error))
    {
        SetOpticalPropertyInputTexts(
            INPUT_ComponentAbsorption,
            INPUT_ComponentSpecularReflection,
            INPUT_ComponentDiffuseReflection,
            PreviousInput);
        ShowInspectorError(Error);
        return false;
    }

    FTGSrpOpticalProperties NormalizedProperties = InputWeights;
    UTGSolarRadiationPressureEditingLibrary::
        NormalizeOpticalPropertyWeights(NormalizedProperties);

    const FTGSimulationScenario PreviousScenario = WorkingScenario;
    Component->SolarRadiationPressure.ComponentOpticalProperties =
        NormalizedProperties;

    if (!UTGSolarRadiationPressureEditingLibrary::
            ResolveComponentOpticalProperties(
                WorkingScenario,
                ComponentId,
                Error))
    {
        WorkingScenario = PreviousScenario;
        SetOpticalPropertyInputTexts(
            INPUT_ComponentAbsorption,
            INPUT_ComponentSpecularReflection,
            INPUT_ComponentDiffuseReflection,
            PreviousInput);
        ShowInspectorError(Error);
        return false;
    }

    if (!CommitWorkingScenarioToDraft())
    {
        WorkingScenario = PreviousScenario;
        SetOpticalPropertyInputTexts(
            INPUT_ComponentAbsorption,
            INPUT_ComponentSpecularReflection,
            INPUT_ComponentDiffuseReflection,
            PreviousInput);
        return false;
    }

    ComponentOpticalInputWeights.Add(
        ComponentId,
        bNormalizeDisplayedValues
            ? NormalizedProperties
            : InputWeights);
    RefreshUiFromWorkingScenario();
    return true;
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleOverrideTargetModeChanged(
        FString SelectedItem,
        ESelectInfo::Type SelectionType)
{
    if (bRefreshing || SelectionType == ESelectInfo::Direct)
    {
        return;
    }

    bEditingTriangleOverride =
        SelectedItem ==
        TGConfigSolarRadiationPressurePrivate::
            ProxyTriangleModeText;
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleOverrideTargetChanged(
        FString SelectedItem,
        ESelectInfo::Type SelectionType)
{
    if (bRefreshing || SelectionType == ESelectInfo::Direct)
    {
        return;
    }

    if (bEditingTriangleOverride)
    {
        TGConfigSolarRadiationPressurePrivate::ParseTriangleOption(
            SelectedItem,
            SelectedTriangleIndex);
    }
    else
    {
        UTGSolarRadiationPressureEditingLibrary::
            TryParseLogicalRegionDisplayName(
                SelectedItem,
                SelectedLogicalRegion);
    }

    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleSelectOverrideInPreviewClicked()
{
    if (
        SelectedComponentId.IsValid() &&
        bEditingTriangleOverride &&
        SelectedTriangleIndex != INDEX_NONE)
    {
        OnSrpProxyTrianglePreviewRequested.Broadcast(
            SelectedComponentId,
            SelectedTriangleIndex);

        if (!OnSrpProxyTrianglePreviewRequested.IsBound())
        {
            ShowInspectorWarning(
                FText::FromString(
                    TEXT(
                        "The selected surface cannot be highlighted in the "
                        "current preview.")));
        }
    }
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleApplyOverrideOpticalPropertiesClicked()
{
    FTGComponentConfig* Component = GetSelectedComponent();
    if (Component == nullptr)
    {
        return;
    }

    FTGSrpOpticalProperties Properties;
    FText Error;

    if (!ParseOpticalProperties(
            INPUT_OverrideAbsorption,
            INPUT_OverrideSpecularReflection,
            INPUT_OverrideDiffuseReflection,
            Properties,
            Error))
    {
        RefreshUiFromWorkingScenario();
        ShowInspectorError(Error);
        return;
    }

    FTGComponentSrpConfig& Config =
        Component->SolarRadiationPressure;

    if (SelectedLogicalRegions.IsEmpty() &&
        SelectedTriangleIndices.IsEmpty())
    {
        ShowInspectorError(FText::FromString(
            TEXT(
                "Select one or more surface regions or triangles in the 3D "
                "preview first.")));
        return;
    }

    if (!SelectedTriangleIndices.IsEmpty() &&
        (Config.GeneratedTriangleCount <= 0 ||
         Config.GeneratedTriangleCount > 32 ||
         Config.bProxyGenerationRequired))
    {
        ShowInspectorError(FText::FromString(
            TEXT("Triangle overrides require a current surface with at most 32 triangles.")));
        return;
    }

    for (const ETGSrpLogicalRegion Region : SelectedLogicalRegions)
    {
        FTGSrpLogicalRegionOverride* Override =
            Config.LogicalRegionOverrides.FindByPredicate(
                [Region](const FTGSrpLogicalRegionOverride& Candidate)
                {
                    return Candidate.Region == Region;
                });
        if (Override == nullptr)
        {
            FTGSrpLogicalRegionOverride NewOverride;
            NewOverride.Region = Region;
            Config.LogicalRegionOverrides.Add(NewOverride);
            Override = &Config.LogicalRegionOverrides.Last();
        }
        Override->OpticalProperties = Properties;
    }

    for (const int32 TriangleIndex : SelectedTriangleIndices)
    {
        FTGSrpTriangleOverride* Override =
            Config.TriangleOverrides.FindByPredicate(
                [TriangleIndex](const FTGSrpTriangleOverride& Candidate)
                {
                    return Candidate.ProxyTriangleIndex == TriangleIndex;
                });
        if (Override == nullptr)
        {
            FTGSrpTriangleOverride NewOverride;
            NewOverride.ProxyTriangleIndex = TriangleIndex;
            Config.TriangleOverrides.Add(NewOverride);
            Override = &Config.TriangleOverrides.Last();
        }
        Override->OpticalProperties = Properties;
    }

    Config.bApplyOneOpticalConfigurationToEntireComponent = false;

    if (!UTGSolarRadiationPressureEditingLibrary::
            ResolveComponentOpticalProperties(
                WorkingScenario,
                Component->ComponentId,
                Error))
    {
        RefreshUiFromWorkingScenario();
        ShowInspectorError(Error);
        return;
    }

    if (!CommitWorkingScenarioToDraft())
    {
        return;
    }
    ClearFaceSelection();
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleRemoveSelectedOverrideOpticalPropertiesClicked()
{
    FTGComponentConfig* Component = GetSelectedComponent();
    if (Component == nullptr)
    {
        return;
    }

    if (SelectedLogicalRegions.IsEmpty() &&
        SelectedTriangleIndices.IsEmpty())
    {
        ShowInspectorError(FText::FromString(
            TEXT("Select one or more faces whose overrides should be removed.")));
        return;
    }

    FTGComponentSrpConfig& Config =
        Component->SolarRadiationPressure;
    const TArray<FTGSrpTriangleOverride> PreviousTriangleOverrides =
        Config.TriangleOverrides;
    const TArray<FTGSrpLogicalRegionOverride> PreviousLogicalOverrides =
        Config.LogicalRegionOverrides;

    const int32 RemovedLogicalCount =
        Config.LogicalRegionOverrides.RemoveAll(
            [this](const FTGSrpLogicalRegionOverride& Override)
            {
                return SelectedLogicalRegions.Contains(Override.Region);
            });
    const int32 RemovedTriangleCount =
        Config.TriangleOverrides.RemoveAll(
            [this](const FTGSrpTriangleOverride& Override)
            {
                return SelectedTriangleIndices.Contains(
                    Override.ProxyTriangleIndex);
            });

    if (RemovedLogicalCount + RemovedTriangleCount == 0)
    {
        RefreshUiFromWorkingScenario();
        return;
    }

    FText Error;
    if (!UTGSolarRadiationPressureEditingLibrary::
            ResolveComponentOpticalProperties(
                WorkingScenario,
                Component->ComponentId,
                Error))
    {
        FTGComponentConfig* RestoredComponent = GetSelectedComponent();
        if (RestoredComponent != nullptr)
        {
            RestoredComponent->SolarRadiationPressure.TriangleOverrides =
                PreviousTriangleOverrides;
            RestoredComponent->SolarRadiationPressure.LogicalRegionOverrides =
                PreviousLogicalOverrides;
        }
        RefreshUiFromWorkingScenario();
        ShowInspectorError(Error);
        return;
    }

    if (!CommitWorkingScenarioToDraft())
    {
        return;
    }
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleClearOverrideOpticalPropertiesClicked()
{
    FTGComponentConfig* Component = GetSelectedComponent();
    if (Component == nullptr)
    {
        return;
    }

    FTGComponentSrpConfig& Config =
        Component->SolarRadiationPressure;

    if (Config.TriangleOverrides.IsEmpty() &&
        Config.LogicalRegionOverrides.IsEmpty())
    {
        RefreshUiFromWorkingScenario();
        return;
    }

    const TArray<FTGSrpTriangleOverride> PreviousTriangleOverrides =
        Config.TriangleOverrides;
    const TArray<FTGSrpLogicalRegionOverride> PreviousLogicalOverrides =
        Config.LogicalRegionOverrides;

    Config.TriangleOverrides.Reset();
    Config.LogicalRegionOverrides.Reset();

    FText Error;
    if (!UTGSolarRadiationPressureEditingLibrary::
            ResolveComponentOpticalProperties(
                WorkingScenario,
                Component->ComponentId,
                Error))
    {
        FTGComponentConfig* RestoredComponent = GetSelectedComponent();
        if (RestoredComponent != nullptr)
        {
            RestoredComponent->SolarRadiationPressure.TriangleOverrides =
                PreviousTriangleOverrides;
            RestoredComponent->SolarRadiationPressure.LogicalRegionOverrides =
                PreviousLogicalOverrides;
        }
        RefreshUiFromWorkingScenario();
        ShowInspectorError(Error);
        return;
    }

    if (!CommitWorkingScenarioToDraft())
    {
        return;
    }
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleConfirmProxyInvalidationClicked()
{
    const FGuid ComponentId =
        PendingProxyInvalidationComponentId;
    const ETGSrpPendingProxyInputChange InputChange =
        PendingProxyInputChange;

    if (!ComponentId.IsValid() ||
        InputChange == ETGSrpPendingProxyInputChange::None)
    {
        PendingProxyInvalidationComponentId.Invalidate();
        PendingProxyInputChange =
            ETGSrpPendingProxyInputChange::None;
        SetProxyInvalidationConfirmationVisible(false);
        RefreshUiFromWorkingScenario();
        return;
    }

    SelectedComponentId = ComponentId;

    FText Error;
    if (!UTGSolarRadiationPressureEditingLibrary::
            DiscardTriangleOverridesAndGeneratedProxy(
                WorkingScenario,
                ComponentId,
                Error))
    {
        PendingProxyInvalidationComponentId.Invalidate();
        PendingProxyInputChange =
            ETGSrpPendingProxyInputChange::None;
        SetProxyInvalidationConfirmationVisible(false);
        RefreshUiFromWorkingScenario();
        ShowInspectorError(Error);
        return;
    }

    FTGComponentConfig* Component = GetSelectedComponent();
    if (Component == nullptr)
    {
        PendingProxyInvalidationComponentId.Invalidate();
        PendingProxyInputChange =
            ETGSrpPendingProxyInputChange::None;
        SetProxyInvalidationConfirmationVisible(false);
        RefreshUiFromWorkingScenario();
        ShowInspectorError(FText::FromString(
            TEXT("The selected solar radiation pressure component no longer exists.")));
        return;
    }

    switch (InputChange)
    {
        case ETGSrpPendingProxyInputChange::ResolutionMode:
            Component->SolarRadiationPressure.ProxyResolutionMode =
                PendingProxyResolutionMode;
            break;

        case ETGSrpPendingProxyInputChange::
            CustomTargetTriangleCount:
            Component->SolarRadiationPressure
                .CustomTargetTriangleCount =
                PendingCustomTargetTriangleCount;
            break;

        case ETGSrpPendingProxyInputChange::None:
        default:
            break;
    }

    PendingProxyInvalidationComponentId.Invalidate();
    PendingProxyInputChange =
        ETGSrpPendingProxyInputChange::None;
    SetProxyInvalidationConfirmationVisible(false);
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
    ShowInspectorWarning(FText::FromString(
        TEXT(
            "The previous surface geometry was discarded and rebuilt using "
            "the updated settings.")));
}

void UTGConfigSolarRadiationPressureWidgetBase::
    HandleCancelProxyInvalidationClicked()
{
    PendingProxyInvalidationComponentId.Invalidate();
    PendingProxyInputChange =
        ETGSrpPendingProxyInputChange::None;
    SetProxyInvalidationConfirmationVisible(false);
    RefreshUiFromWorkingScenario();
}

void UTGConfigSolarRadiationPressureWidgetBase::
    SetProxyInvalidationConfirmationVisible(
        bool bVisible,
        const FString& Message)
{
    if (BORDER_ProxyInvalidationConfirmation != nullptr)
    {
        BORDER_ProxyInvalidationConfirmation->SetVisibility(
            bVisible
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }

    if (
        bVisible &&
        TXT_ProxyInvalidationConfirmation != nullptr)
    {
        TXT_ProxyInvalidationConfirmation->SetText(
            FText::FromString(Message));
    }
}

bool UTGConfigSolarRadiationPressureWidgetBase::
    ParseOpticalProperties(
        const UEditableText* AbsorptionInput,
        const UEditableText* SpecularInput,
        const UEditableText* DiffuseInput,
        FTGSrpOpticalProperties& OutProperties,
        FText& OutError) const
{
    if (!ParseOpticalPropertyWeights(
            AbsorptionInput,
            SpecularInput,
            DiffuseInput,
            OutProperties,
            OutError))
    {
        return false;
    }

    UTGSolarRadiationPressureEditingLibrary::
        NormalizeOpticalPropertyWeights(OutProperties);
    return true;
}

bool UTGConfigSolarRadiationPressureWidgetBase::
    ParseOpticalPropertyWeights(
        const UEditableText* AbsorptionInput,
        const UEditableText* SpecularInput,
        const UEditableText* DiffuseInput,
        FTGSrpOpticalProperties& OutProperties,
        FText& OutError) const
{
    OutProperties = FTGSrpOpticalProperties();
    OutError = FText::GetEmpty();

    if (
        AbsorptionInput == nullptr ||
        SpecularInput == nullptr ||
        DiffuseInput == nullptr)
    {
        OutError = FText::FromString(
            TEXT("One or more optical-property input widgets are missing."));
        return false;
    }

    if (!UTGHudFormattingLibrary::ParseHudDouble(
            AbsorptionInput->GetText(),
            OutProperties.AbsorptionFraction,
            OutError))
    {
        OutError = FText::FromString(
            FString::Printf(
                TEXT("Absorption: %s"),
                *OutError.ToString()));
        return false;
    }

    if (!UTGHudFormattingLibrary::ParseHudDouble(
            SpecularInput->GetText(),
            OutProperties.SpecularReflectionFraction,
            OutError))
    {
        OutError = FText::FromString(
            FString::Printf(
                TEXT("Specular reflection: %s"),
                *OutError.ToString()));
        return false;
    }

    if (!UTGHudFormattingLibrary::ParseHudDouble(
            DiffuseInput->GetText(),
            OutProperties.DiffuseReflectionFraction,
            OutError))
    {
        OutError = FText::FromString(
            FString::Printf(
                TEXT("Diffuse reflection: %s"),
                *OutError.ToString()));
        return false;
    }

    const double Sum =
        OutProperties.AbsorptionFraction +
        OutProperties.SpecularReflectionFraction +
        OutProperties.DiffuseReflectionFraction;
    if (!FMath::IsFinite(Sum))
    {
        OutError = FText::FromString(
            TEXT("Optical-property weights must be finite numbers."));
        return false;
    }
    if (OutProperties.AbsorptionFraction < 0.0 ||
        OutProperties.SpecularReflectionFraction < 0.0 ||
        OutProperties.DiffuseReflectionFraction < 0.0)
    {
        OutError = FText::FromString(
            TEXT("Optical-property weights cannot be negative."));
        return false;
    }
    if (Sum <= UE_DOUBLE_SMALL_NUMBER)
    {
        OutError = FText::FromString(
            TEXT(
                "At least one optical-property weight must be greater than "
                "zero."));
        return false;
    }
    return true;
}

void UTGConfigSolarRadiationPressureWidgetBase::
    SetOpticalPropertyInputTexts(
        UEditableText* AbsorptionInput,
        UEditableText* SpecularInput,
        UEditableText* DiffuseInput,
        const FTGSrpOpticalProperties& Properties) const
{
    if (AbsorptionInput != nullptr)
    {
        AbsorptionInput->SetText(
            FormatDouble(Properties.AbsorptionFraction));
    }
    if (SpecularInput != nullptr)
    {
        SpecularInput->SetText(
            FormatDouble(Properties.SpecularReflectionFraction));
    }
    if (DiffuseInput != nullptr)
    {
        DiffuseInput->SetText(
            FormatDouble(Properties.DiffuseReflectionFraction));
    }
}

FText UTGConfigSolarRadiationPressureWidgetBase::
    FormatDouble(double Value)
{
    return UTGHudFormattingLibrary::FormatDoubleForHud(
        Value,
        9);
}

FText UTGConfigSolarRadiationPressureWidgetBase::
    FormatInteger(int32 Value)
{
    return UTGHudFormattingLibrary::FormatIntegerForHud(Value);
}

FString UTGConfigSolarRadiationPressureWidgetBase::
    GeometryDescription(const FTGComponentConfig& Component)
{
    const FTGComponentVisualConfig& Visual = Component.Visual;

    if (
        Visual.GeometrySource ==
        ETGComponentGeometrySource::NoGeometry)
    {
        return TEXT(
            "Geometry: None");
    }

    if (
        Visual.GeometrySource ==
        ETGComponentGeometrySource::CustomStl)
    {
        return TEXT("Geometry: Custom STL");
    }

    switch (Visual.PrimitiveType)
    {
        case ETGPrimitiveGeometryType::Box:
            return FString::Printf(
                TEXT(
                    "Geometry: Box (%.6g x %.6g x %.6g m)"),
                Visual.BoxDimensionsMeters.X,
                Visual.BoxDimensionsMeters.Y,
                Visual.BoxDimensionsMeters.Z);

        case ETGPrimitiveGeometryType::Sphere:
            return FString::Printf(
                TEXT("Geometry: Sphere (radius %.6g m)"),
                Visual.SphereRadiusMeters);

        case ETGPrimitiveGeometryType::Cylinder:
            return FString::Printf(
                TEXT(
                    "Geometry: Cylinder (radius %.6g m, length %.6g m)"),
                Visual.CylinderRadiusMeters,
                Visual.CylinderLengthMeters);
    }

    return TEXT("Geometry: Unsupported");
}

void UTGConfigSolarRadiationPressureWidgetBase::ClearMessages()
{
    ClearLeftMessages();
    ClearInspectorMessages();
}

void UTGConfigSolarRadiationPressureWidgetBase::ClearLeftMessages()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LeftMessageTimerHandle);
    }

    if (TXT_SrpWarning != nullptr)
    {
        TXT_SrpWarning->SetText(FText::GetEmpty());
        TXT_SrpWarning->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (TXT_SrpError != nullptr)
    {
        TXT_SrpError->SetText(FText::GetEmpty());
        TXT_SrpError->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UTGConfigSolarRadiationPressureWidgetBase::ClearInspectorMessages()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(InspectorMessageTimerHandle);
    }

    if (TXT_SrpInspectorWarning != nullptr)
    {
        TXT_SrpInspectorWarning->SetText(FText::GetEmpty());
        TXT_SrpInspectorWarning->SetVisibility(
            ESlateVisibility::Collapsed);
    }
    if (TXT_SrpInspectorError != nullptr)
    {
        TXT_SrpInspectorError->SetText(FText::GetEmpty());
        TXT_SrpInspectorError->SetVisibility(
            ESlateVisibility::Collapsed);
    }
}

void UTGConfigSolarRadiationPressureWidgetBase::
    ShowError(const FText& Error)
{
    if (TXT_SrpError == nullptr || Error.IsEmpty())
    {
        return;
    }

    TXT_SrpError->SetText(Error);
    TXT_SrpError->SetVisibility(ESlateVisibility::Visible);
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            LeftMessageTimerHandle,
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::ClearLeftMessages,
            TGConfigSolarRadiationPressurePrivate::MessageLifetimeSeconds,
            false);
    }
}

void UTGConfigSolarRadiationPressureWidgetBase::
    ShowWarning(const FText& Warning)
{
    if (TXT_SrpWarning == nullptr || Warning.IsEmpty())
    {
        return;
    }

    FString Message = Warning.ToString();
    const FString Existing = TXT_SrpWarning->GetText().ToString();
    if (!Existing.IsEmpty() && !Existing.Contains(Message))
    {
        Message = Existing + TEXT("\n") + Message;
    }

    TXT_SrpWarning->SetText(FText::FromString(Message));
    TXT_SrpWarning->SetVisibility(ESlateVisibility::Visible);
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            LeftMessageTimerHandle,
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::ClearLeftMessages,
            TGConfigSolarRadiationPressurePrivate::MessageLifetimeSeconds,
            false);
    }
}

void UTGConfigSolarRadiationPressureWidgetBase::
    ShowInspectorError(const FText& Error)
{
    if (TXT_SrpInspectorError == nullptr || Error.IsEmpty())
    {
        return;
    }

    TXT_SrpInspectorError->SetText(Error);
    TXT_SrpInspectorError->SetVisibility(ESlateVisibility::Visible);
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            InspectorMessageTimerHandle,
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::ClearInspectorMessages,
            TGConfigSolarRadiationPressurePrivate::MessageLifetimeSeconds,
            false);
    }
}

void UTGConfigSolarRadiationPressureWidgetBase::
    ShowInspectorWarning(const FText& Warning)
{
    if (TXT_SrpInspectorWarning == nullptr || Warning.IsEmpty())
    {
        return;
    }

    FString Message = Warning.ToString();
    const FString Existing =
        TXT_SrpInspectorWarning->GetText().ToString();
    if (!Existing.IsEmpty() && !Existing.Contains(Message))
    {
        Message = Existing + TEXT("\n") + Message;
    }

    TXT_SrpInspectorWarning->SetText(FText::FromString(Message));
    TXT_SrpInspectorWarning->SetVisibility(ESlateVisibility::Visible);
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            InspectorMessageTimerHandle,
            this,
            &UTGConfigSolarRadiationPressureWidgetBase::ClearInspectorMessages,
            TGConfigSolarRadiationPressurePrivate::MessageLifetimeSeconds,
            false);
    }
}
