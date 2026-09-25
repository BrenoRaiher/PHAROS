// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "TGVisualizationTimelineSplitterWidget.generated.h"

class STGVisualizationTimelineSplitter;

/** Thin native resize rail controlling the fixed bottom Timeline panel. */
UCLASS(BlueprintType, meta = (DisplayName = "PHAROS Timeline Splitter"))
class TG_API UTGVisualizationTimelineSplitterWidget : public UWidget
{
    GENERATED_BODY()

public:
    void InitializeForTimeline(UWidget* InTimelinePanel, UWidget* InTimelineBody);
    void CollapseTimeline();
    void ExpandTimeline();

    UPROPERTY(EditAnywhere, Category = "PHAROS|Visualization|Timeline", meta = (ClampMin = "4.0"))
    float CollapsedPanelHeight = 8.0f;

    UPROPERTY(EditAnywhere, Category = "PHAROS|Visualization|Timeline", meta = (ClampMin = "40.0"))
    float MinimumExpandedHeight = 126.0f;

    UPROPERTY(EditAnywhere, Category = "PHAROS|Visualization|Timeline", meta = (ClampMin = "1.0"))
    float CollapseSnapDistance = 56.0f;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UWidget> TimelinePanel;

    UPROPERTY(Transient)
    TObjectPtr<UWidget> TimelineBody;

    TSharedPtr<STGVisualizationTimelineSplitter> SplitterWidget;
    float LastExpandedHeight = 156.0f;

    float GetTimelineHeight() const;
    void SetTimelineHeight(float Height);
    void HandleResizeDelta(float DeltaScreenY);
    void HandleResizeFinished();
    void HandleToggleRequested();
};
