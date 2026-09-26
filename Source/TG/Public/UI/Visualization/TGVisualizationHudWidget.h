// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TGVisualizationHudWidget.generated.h"

class ATGSimulationPlaybackActor;
class UBorder;
class UButton;
class UDataTable;
class UEditableTextBox;
class UPanelWidget;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UWidgetSwitcher;
class UTGSolarSystemOverviewWidget;
class UTGVisualizationArrowRowWidget;
class UTGVisualizationConstellationRowWidget;
class UTGVisualizationTelemetryCardWidget;
class UTGVisualizationDockWorkspaceWidget;

/**
 * Native behavior for the result-visualization HUD.
 *
 * The Widget Blueprint owns every layout and style decision. This class only
 * binds the named Designer widgets to playback, timeline, overview-graph,
 * dock-workspace, and pause-menu behavior.
 */
UCLASS(Abstract, Blueprintable)
class TG_API UTGVisualizationHudWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UTGVisualizationHudWidget(const FObjectInitializer& ObjectInitializer);

    void InitializeForPlayback(ATGSimulationPlaybackActor* InPlaybackActor);
    void TogglePauseMenu();
    bool IsPauseMenuOpen() const;
    bool IsPointerOverControls() const;

    /** Concrete Designer row used for each available force/body arrow. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHAROS|Visualization")
    TSoftClassPtr<UTGVisualizationArrowRowWidget> ArrowRowWidgetClass;

    /** Concrete Designer row used for each constellation in the data table. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHAROS|Visualization")
    TSoftClassPtr<UTGVisualizationConstellationRowWidget>
        ConstellationRowWidgetClass;

    /** Concrete Designer card used for each live telemetry value. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHAROS|Visualization")
    TSoftClassPtr<UTGVisualizationTelemetryCardWidget>
        TelemetryCardWidgetClass;

    /** Existing constellation name/identifier source used by the old panel. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHAROS|Visualization")
    TSoftObjectPtr<UDataTable> ConstellationTable;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<ATGSimulationPlaybackActor> PlaybackActor;

    // Display, telemetry, and overview panels ------------------------------
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_CollapseTelemetry;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_CollapseDisplay;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_CollapseOverview;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> BODY_Telemetry;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> BODY_Display;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> BODY_Overview;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> PANEL_Display;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> PANEL_Telemetry;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> PANEL_Overview;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_CollapseDisplay;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_CollapseTelemetry;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_CollapseOverview;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_TabConstellations;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_TabVectors;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidgetSwitcher> SWITCHER_Display;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> HOST_Constellations;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> VBOX_ConstellationRows;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> INPUT_ConstellationSearch;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> VBOX_VectorRows;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> INPUT_VectorSearch;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTGSolarSystemOverviewWidget> GRAPH_SolarSystem;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_FocusSpacecraft;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_SolarOverview;

    // Fixed telemetry value cells. Labels and layout remain Designer-owned.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_TelemetryMass;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_TelemetryMassRate;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_TelemetrySpeedIcrf;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_TelemetryPositionIcrf;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_TelemetryVelocityIcrf;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_TelemetryAngularVelocityBody;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_TelemetryAngularVelocityIcrf;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_TelemetryClosestBody;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_TelemetryAltitude;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_TelemetryPositionBodyFixed;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_TelemetryVelocityBodyFixed;

    /** Optional native telemetry workspace. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> VBOX_TelemetryCards;

    /** Runtime dock/float/split workspace replacing fixed display panels. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTGVisualizationDockWorkspaceWidget> WORKSPACE_Dock;

    // Pause menu ------------------------------------------------------------
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> OVERLAY_PauseMenu;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> PANEL_PauseMenu;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_StayInVisualization;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ReturnToConfiguration;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> VBOX_PauseMenu;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_GoToScenarioLibrary;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_ReturnToMainMenu;

    UPROPERTY(Transient)
    TMap<FString, TObjectPtr<UTextBlock>> TelemetryValueTexts;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTGVisualizationArrowRowWidget>> ArrowRows;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTGVisualizationConstellationRowWidget>>
        ConstellationRows;

    UPROPERTY(Transient)
    TMap<FString, TObjectPtr<UTGVisualizationTelemetryCardWidget>>
        TelemetryCards;

    bool bPauseMenuOpen = false;
    bool bResumeAfterMenu = false;
    void BindNativeControls();
    void UnbindNativeControls();
    void CacheTelemetryBindings();
    void RebuildArrowRows();
    void RebuildConstellationRows();
    void ApplyVectorSearchFilter(const FString& SearchText);
    void ApplyConstellationSearchFilter(const FString& SearchText);
    void RefreshDisplayedValues();
    void RefreshTelemetryValues();
    void EnsurePauseMenuNavigationControls();
    void ApplyVisualizationTheme();

    UFUNCTION()
    void HandlePlaybackTimeChanged(
        double ElapsedSimulationSeconds,
        double EphemerisTimeTdbSeconds,
        double NormalizedTime);

    UFUNCTION()
    void HandleConstellationSearchChanged(const FText& Text);

    UFUNCTION()
    void HandleVectorSearchChanged(const FText& Text);

    UFUNCTION()
    void HandleConstellationsTabClicked();

    UFUNCTION()
    void HandleVectorsTabClicked();

    UFUNCTION()
    void HandleOverviewFocusSpacecraft();

    UFUNCTION()
    void HandleOverviewReset();

    UFUNCTION()
    void HandleStayInVisualization();

    UFUNCTION()
    void HandleReturnToConfiguration();

    UFUNCTION()
    void HandleGoToScenarioLibrary();

    UFUNCTION()
    void HandleReturnToMainMenu();
};
