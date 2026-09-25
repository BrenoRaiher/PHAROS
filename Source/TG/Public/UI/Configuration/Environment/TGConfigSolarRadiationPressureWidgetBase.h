// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "UI/Configuration/Review/TGScenarioReviewNavigationTarget.h"
#include "TGConfigSolarRadiationPressureWidgetBase.generated.h"

class UBorder;
class UButton;
class UCheckBox;
class UComboBoxString;
class UEditableText;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;
class ATGSpacecraftPreviewPawn;
class ATGSpacecraftVisualActor;
class UTGSearchBox;
class UTGSrpComponentRowWidget;
class UTGSimulationSubsystem;

enum class ETGSrpPendingProxyInputChange : uint8
{
    None,
    ResolutionMode,
    CustomTargetTriangleCount
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FTGSrpProxyTrianglePreviewRequested,
    FGuid,
    ComponentId,
    int32,
    StableTriangleIndex);

/** Native behavior for WBP_Config_SolarRadiationPressure. */
UCLASS(Blueprintable)
class TG_API UTGConfigSolarRadiationPressureWidgetBase
    : public UUserWidget,
      public ITGScenarioReviewNavigationTarget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|SRP")
    void RefreshFromCurrentDraft();

    virtual bool NavigateToScenarioReviewIssue_Implementation(
        const FTGScenarioReviewIssue& Issue) override;

    /** Activates the shared Component Tree preview actors for SRP authoring. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|SRP")
    bool ActivateSolarRadiationPressurePreview(
        ATGSpacecraftPreviewPawn* InPreviewPawn,
        ATGSpacecraftVisualActor* InSpacecraftActor);

    /** Releases SRP picking and hides the shared frontend preview actors. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|SRP")
    void DeactivateSolarRadiationPressurePreview();

    /** Visualization hook for the read-only stable triangle selection. */
    UPROPERTY(BlueprintAssignable, Category = "PHAROS|Environment|SRP")
    FTGSrpProxyTrianglePreviewRequested OnSrpProxyTrianglePreviewRequested;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(
        const FGeometry& MyGeometry,
        float InDeltaTime) override;

private:
    UFUNCTION()
    void HandleEnableSrpChanged(bool bIsChecked);

    UFUNCTION()
    void HandleComputeCelestialEclipsesChanged(bool bIsChecked);

    UFUNCTION()
    void HandleAddOccultingBodyClicked();

    UFUNCTION()
    void HandleRemoveOccultingBodyClicked();

    UFUNCTION()
    void HandleClearOccultingBodiesClicked();

    UFUNCTION()
    void HandleComputeComponentShadowsChanged(bool bIsChecked);

    UFUNCTION()
    void HandleApplyGlobalOpticalPropertiesClicked();

    UFUNCTION()
    void HandleGlobalOpticalPropertiesCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    UFUNCTION()
    void HandleComponentSearchChanged(const FText& Text);

    UFUNCTION()
    void HandleComponentRowSelected(FGuid ComponentId);

    UFUNCTION()
    void HandleComponentRowIncludeChanged(
        FGuid ComponentId,
        bool bIncluded);

    UFUNCTION()
    void HandleComponentRowVisibilityChanged(
        FGuid ComponentId,
        bool bVisible);

    UFUNCTION()
    void HandlePreviewSrpFacetClicked(
        FGuid ComponentId,
        int32 StableTriangleIndex);

    UFUNCTION()
    void HandlePreviewSrpPrimitiveSurfaceClicked(
        FGuid ComponentId,
        FVector PrimitiveLocalNormal);

    UFUNCTION()
    void HandleClearFaceSelectionClicked();

    UFUNCTION()
    void HandleComponentSelectionChanged(
        FString SelectedItem,
        ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleIncludeComponentChanged(bool bIsChecked);

    UFUNCTION()
    void HandleProxyResolutionModeChanged(
        FString SelectedItem,
        ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleCustomTargetTriangleCountCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    UFUNCTION()
    void HandleUseGlobalFallbackChanged(bool bIsChecked);

    UFUNCTION()
    void HandleApplyOneOpticalConfigurationChanged(bool bIsChecked);

    UFUNCTION()
    void HandleApplyComponentOpticalPropertiesClicked();

    UFUNCTION()
    void HandleComponentOpticalPropertiesCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    UFUNCTION()
    void HandleOverrideTargetModeChanged(
        FString SelectedItem,
        ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleOverrideTargetChanged(
        FString SelectedItem,
        ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleSelectOverrideInPreviewClicked();

    UFUNCTION()
    void HandleApplyOverrideOpticalPropertiesClicked();

    UFUNCTION()
    void HandleClearOverrideOpticalPropertiesClicked();

    UFUNCTION()
    void HandleRemoveSelectedOverrideOpticalPropertiesClicked();

    UFUNCTION()
    void HandleConfirmProxyInvalidationClicked();

    UFUNCTION()
    void HandleCancelProxyInvalidationClicked();

    UTGSimulationSubsystem* GetSimulationSubsystem() const;
    bool PullWorkingScenarioFromDraft();
    bool CommitWorkingScenarioToDraft();

    FTGComponentConfig* GetSelectedComponent();
    const FTGComponentConfig* GetSelectedComponent() const;

    void RefreshUiFromWorkingScenario();
    void RefreshOccultingBodyControls();
    void RefreshComponentRows();
    void RefreshComponentControls();
    void RefreshOverrideControls(const FTGComponentConfig& Component);
    void RefreshValidationStatus();

    void RefreshPreviewIntegration();
    void RefreshPreviewSelectionHighlight();
    void ClearFaceSelection();
    bool IsLogicalRegionSelected(ETGSrpLogicalRegion Region) const;
    TArray<int32> GetSelectedFacetIndices() const;
    void SetProxyInvalidationConfirmationVisible(
        bool bVisible,
        const FString& Message = FString());

    bool ParseOpticalProperties(
        const UEditableText* AbsorptionInput,
        const UEditableText* SpecularInput,
        const UEditableText* DiffuseInput,
        FTGSrpOpticalProperties& OutProperties,
        FText& OutError) const;

    bool ParseOpticalPropertyWeights(
        const UEditableText* AbsorptionInput,
        const UEditableText* SpecularInput,
        const UEditableText* DiffuseInput,
        FTGSrpOpticalProperties& OutProperties,
        FText& OutError) const;

    bool CommitGlobalOpticalPropertyInputs(bool bNormalizeDisplayedValues);
    bool CommitComponentOpticalPropertyInputs(bool bNormalizeDisplayedValues);
    void SetOpticalPropertyInputTexts(
        UEditableText* AbsorptionInput,
        UEditableText* SpecularInput,
        UEditableText* DiffuseInput,
        const FTGSrpOpticalProperties& Properties) const;

    static FText FormatDouble(double Value);
    static FText FormatInteger(int32 Value);
    static FString GeometryDescription(const FTGComponentConfig& Component);

    void ClearMessages();
    void ClearLeftMessages();
    void ClearInspectorMessages();
    void ShowError(const FText& Error);
    void ShowWarning(const FText& Warning);
    void ShowInspectorError(const FText& Error);
    void ShowInspectorWarning(const FText& Warning);

private:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_EnableSrp;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_SrpSettings;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_ComputeCelestialEclipses;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> VBOX_OccultingBodiesDetails;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> COMBO_AvailableOccultingBody;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_AddOccultingBody;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> COMBO_SelectedOccultingBody;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_RemoveOccultingBody;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_ClearOccultingBodies;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_OccultingBodiesStatus;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_ComputeComponentShadows;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_GlobalAbsorption;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_GlobalSpecularReflection;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_GlobalDiffuseReflection;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ApplyGlobalOpticalProperties;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_GlobalOpticalPropertiesStatus;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_TotalProxyTriangleCount;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SrpProxyPerformanceWarning;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> COMBO_SrpComponent;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_SrpComponentRows;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTGSearchBox> INPUT_ComponentSearch;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_NoSrpComponentSelection;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_SelectedSrpComponentEditor;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> BORDER_ProxySettingsSection;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> BORDER_FaceOverridesSection;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SelectedSrpComponentTitle;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SelectedSrpGeometrySource;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_IncludeComponentInSrpProxy;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_ProxyResolutionMode;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_CustomTargetTriangleCountDetails;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_CustomTargetTriangleCount;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ComponentProxyTriangleCount;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ComponentProxyStatus;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_UseGlobalFallbackOpticalProperties;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_ApplyOneOpticalConfiguration;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> HBOX_ApplyOne;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_PerFaceOpticsSummary;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_ComponentOpticalPropertiesDetails;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_ComponentAbsorption;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_ComponentSpecularReflection;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_ComponentDiffuseReflection;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ApplyComponentOpticalProperties;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ComponentOpticalPropertiesStatus;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_OpticalOverrideDetails;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_OverrideAvailability;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> COMBO_OverrideTargetMode;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> COMBO_OverrideTarget;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_SelectedOverrideStableTriangleIndex;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_SelectOverrideInPreview;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SelectedSrpFaces;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ClearSrpFaceSelection;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_OverrideAbsorption;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_OverrideSpecularReflection;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_OverrideDiffuseReflection;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ApplyOverrideOpticalProperties;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ClearOverrideOpticalProperties;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_RemoveSelectedOverrideOpticalProperties;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_OverrideOpticalPropertiesStatus;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SrpWarning;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SrpError;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SrpInspectorWarning;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SrpInspectorError;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BORDER_ProxyInvalidationConfirmation;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ProxyInvalidationConfirmation;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ConfirmProxyInvalidation;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_CancelProxyInvalidation;

    UPROPERTY(Transient)
    FTGSimulationScenario WorkingScenario;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTGSrpComponentRowWidget>> ComponentRows;

    UPROPERTY(Transient)
    TObjectPtr<ATGSpacecraftPreviewPawn> PreviewPawn;

    UPROPERTY(Transient)
    TObjectPtr<ATGSpacecraftVisualActor> PreviewSpacecraftActor;

    FGuid SelectedComponentId;
    FGuid PendingProxyInvalidationComponentId;
    ETGSrpPendingProxyInputChange PendingProxyInputChange =
        ETGSrpPendingProxyInputChange::None;
    ETGSrpProxyResolutionMode PendingProxyResolutionMode =
        ETGSrpProxyResolutionMode::Automatic;
    int32 PendingCustomTargetTriangleCount = 0;
    ETGSrpLogicalRegion SelectedLogicalRegion = ETGSrpLogicalRegion::None;
    int32 SelectedTriangleIndex = INDEX_NONE;
    TSet<ETGSrpLogicalRegion> SelectedLogicalRegions;
    TSet<int32> SelectedTriangleIndices;
    TSet<FGuid> HiddenPreviewComponentIds;
    TMap<FGuid, FTGSrpOpticalProperties> ComponentOpticalInputWeights;
    FTGSrpOpticalProperties GlobalOpticalInputWeights;
    FString ComponentSearchText;
    FTimerHandle LeftMessageTimerHandle;
    FTimerHandle InspectorMessageTimerHandle;
    bool bEditingTriangleOverride = false;
    bool bHasGlobalOpticalInputWeights = false;
    bool bRefreshing = false;
    bool bPreviewIntegrationActive = false;
    uint64 LastObservedDraftRevision = 0;
};
