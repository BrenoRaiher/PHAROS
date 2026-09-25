// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Visualization/TGSimulationPlaybackActor.h"
#include "TGVisualizationDockWorkspaceWidget.generated.h"

class ATGSimulationPlaybackActor;
class SWidget;
class STGVisualizationDockWorkspace;
class UDataTable;
class UTGSolarSystemOverviewWidget;

/** One constellation exposed by the visualization workspace. */
struct FTGVisualizationConstellationEntry
{
    FString Id;
    FString DisplayName;
};

/**
 * UMG bridge for the native runtime visualization workspace.
 *
 * The Slate implementation supplies scrollable Constellations, Vectors, and
 * Telemetry and Solar System tabs. Tabs can float inside the viewport or dock
 * to the left, right, or top edge; tabs dropped on one edge share that dock.
 * Independent edge rails resize/collapse each dock, and the persistent Window
 * menu restores closed tabs. A separate native timeline is fixed to the bottom
 * edge; the docking system intentionally has no bottom drop target. Layout is
 * saved in the user's settings config.
 */
UCLASS(BlueprintType, meta = (DisplayName = "PHAROS Visualization Dock Workspace"))
class TG_API UTGVisualizationDockWorkspaceWidget : public UWidget
{
    GENERATED_BODY()

public:
    UTGVisualizationDockWorkspaceWidget();

    void InitializeForPlayback(ATGSimulationPlaybackActor* InPlaybackActor);
    /** Playback source used by the native fixed timeline. */
    ATGSimulationPlaybackActor* GetPlaybackActor() const;
    bool IsPointerOverInteractiveArea() const;

    const TArray<FTGVisualizationConstellationEntry>&
        GetConstellations() const;
    TArray<FTGVisualizationArrowInfo> GetVisualizationArrows() const;
    bool GetVisualizationArrowInfo(
        FName ArrowId,
        FTGVisualizationArrowInfo& OutInfo) const;
    void GetTelemetryItems(
        TArray<FTGVisualizationTelemetryItem>& OutItems) const;

    bool IsConstellationOutlineVisible(const FString& Id) const;
    void SetConstellationOutlineVisible(const FString& Id, bool bVisible);
    bool IsVisualizationArrowVisible(FName ArrowId) const;
    void SetVisualizationArrowVisible(FName ArrowId, bool bVisible);

    TSharedPtr<SWidget> GetSolarSystemOverviewSlateWidget();
    TSharedPtr<SWidget> GetClosestBodyOverviewSlateWidget();
    void FocusSolarSystemOnSpacecraft();
    void ResetSolarSystemView();
    void RefreshSolarSystemOverviewGraphs();

    /** Source of the constellation identifier and display-name catalog. */
    UPROPERTY(EditAnywhere, Category = "PHAROS|Visualization")
    TSoftObjectPtr<UDataTable> ConstellationTable;

    /** Concrete graph widget hosted by the dockable Solar System tab. */
    UPROPERTY(EditAnywhere, Category = "PHAROS|Visualization")
    TSoftClassPtr<UTGSolarSystemOverviewWidget>
        SolarSystemOverviewWidgetClass;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    virtual void SynchronizeProperties() override;
    virtual void BeginDestroy() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<ATGSimulationPlaybackActor> PlaybackActor;

    UPROPERTY(Transient)
    TObjectPtr<UTGSolarSystemOverviewWidget> SolarSystemOverviewWidget;

    UPROPERTY(Transient)
    TObjectPtr<UTGSolarSystemOverviewWidget> ClosestBodyOverviewWidget;

    TArray<FTGVisualizationConstellationEntry> Constellations;
    TMap<FString, bool> ConstellationOutlineStates;
    TSharedPtr<STGVisualizationDockWorkspace> WorkspaceWidget;
    double LastTelemetryRefreshRealSeconds = -1.0;

    void RebuildConstellationCatalog();
    void EnsureSolarSystemOverviewWidget();

    UFUNCTION()
    void HandlePlaybackTimeChanged(
        double ElapsedSimulationSeconds,
        double EphemerisTimeTdbSeconds,
        double NormalizedTime);
};
