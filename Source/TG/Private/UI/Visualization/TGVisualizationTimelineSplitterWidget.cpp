// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Visualization/TGVisualizationTimelineSplitterWidget.h"

#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"
#include "UI/Theme/TGUiTheme.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

namespace TGVisualizationTimelineSplitterPrivate
{
    const FSlateBrush& RailBrush()
    {
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            FLinearColor::White,
            1.0f);
        return Brush;
    }
}

class STGVisualizationTimelineSplitter : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STGVisualizationTimelineSplitter) {}
        SLATE_EVENT(FSimpleDelegate, OnToggleRequested)
        SLATE_EVENT(FSimpleDelegate, OnResizeFinished)
        SLATE_EVENT(TDelegate<void(float)>, OnResizeDelta)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        OnToggleRequested = InArgs._OnToggleRequested;
        OnResizeFinished = InArgs._OnResizeFinished;
        OnResizeDelta = InArgs._OnResizeDelta;

        ChildSlot
        [
            SNew(SBox)
            .HeightOverride(8.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .HeightOverride(2.0f)
                    [
                        SNew(SBorder)
                        .BorderImage(
                            &TGVisualizationTimelineSplitterPrivate::
                                RailBrush())
                        .BorderBackgroundColor_Lambda([this]()
                        {
                            const FTGUiPalette& Palette =
                                TGUiTheme::GetPalette();
                            return IsHovered() || HasMouseCapture()
                                ? Palette.Accent
                                : Palette.Border;
                        })
                    ]
                ]
            ]
        ];
    }

    virtual FReply OnMouseButtonDown(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent) override
    {
        if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
        {
            return FReply::Unhandled();
        }
        PreviousCursorY = MouseEvent.GetScreenSpacePosition().Y;
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    virtual FReply OnMouseMove(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent) override
    {
        if (!HasMouseCapture())
        {
            return FReply::Unhandled();
        }
        const double CurrentY = MouseEvent.GetScreenSpacePosition().Y;
        OnResizeDelta.ExecuteIfBound(
            static_cast<float>(CurrentY - PreviousCursorY));
        PreviousCursorY = CurrentY;
        return FReply::Handled();
    }

    virtual FReply OnMouseButtonUp(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent) override
    {
        if (!HasMouseCapture())
        {
            return FReply::Unhandled();
        }
        OnResizeFinished.ExecuteIfBound();
        return FReply::Handled().ReleaseMouseCapture();
    }

    virtual FReply OnMouseButtonDoubleClick(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent) override
    {
        if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
        {
            OnToggleRequested.ExecuteIfBound();
            return FReply::Handled();
        }
        return FReply::Unhandled();
    }

    virtual FCursorReply OnCursorQuery(
        const FGeometry& MyGeometry,
        const FPointerEvent& CursorEvent) const override
    {
        return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
    }

private:
    FSimpleDelegate OnToggleRequested;
    FSimpleDelegate OnResizeFinished;
    TDelegate<void(float)> OnResizeDelta;
    double PreviousCursorY = 0.0;
};

void UTGVisualizationTimelineSplitterWidget::InitializeForTimeline(
    UWidget* InTimelinePanel,
    UWidget* InTimelineBody)
{
    TimelinePanel = InTimelinePanel;
    TimelineBody = InTimelineBody;

    if (UCanvasPanelSlot* CanvasSlot = TimelinePanel != nullptr
            ? Cast<UCanvasPanelSlot>(TimelinePanel->Slot)
            : nullptr)
    {
        CanvasSlot->SetAnchors(FAnchors(0.0f, 1.0f, 1.0f, 1.0f));
        CanvasSlot->SetAlignment(FVector2D(0.0f, 1.0f));
        CanvasSlot->SetZOrder(90);
        FMargin Offsets = CanvasSlot->GetOffsets();
        Offsets.Left = 0.0f;
        Offsets.Top = 0.0f;
        Offsets.Right = 0.0f;
        CanvasSlot->SetOffsets(Offsets);
        const float CurrentHeight = Offsets.Bottom;
        if (CurrentHeight > CollapsedPanelHeight + 1.0f)
        {
            LastExpandedHeight = FMath::Max(
                MinimumExpandedHeight,
                CurrentHeight);
        }
    }
    // The old panel put padding around the entire vertical box. At the
    // collapsed height that padding consumed the resize rail itself. Keep the
    // rail flush to the viewport and apply the visual breathing room only to
    // the timeline body.
    if (UBorder* TimelineBorder = Cast<UBorder>(TimelinePanel))
    {
        TimelineBorder->SetPadding(FMargin(0.0f));
    }
    if (UVerticalBoxSlot* BodySlot = TimelineBody != nullptr
            ? Cast<UVerticalBoxSlot>(TimelineBody->Slot)
            : nullptr)
    {
        BodySlot->SetPadding(FMargin(16.0f, 12.0f, 16.0f, 14.0f));
    }
}

void UTGVisualizationTimelineSplitterWidget::CollapseTimeline()
{
    const float CurrentHeight = GetTimelineHeight();
    if (CurrentHeight > CollapsedPanelHeight + 1.0f)
    {
        LastExpandedHeight = FMath::Max(
            MinimumExpandedHeight,
            CurrentHeight);
    }
    SetTimelineHeight(CollapsedPanelHeight);
}

void UTGVisualizationTimelineSplitterWidget::ExpandTimeline()
{
    SetTimelineHeight(FMath::Max(
        MinimumExpandedHeight,
        LastExpandedHeight));
}

TSharedRef<SWidget> UTGVisualizationTimelineSplitterWidget::RebuildWidget()
{
    SplitterWidget = SNew(STGVisualizationTimelineSplitter)
        .OnResizeDelta(TDelegate<void(float)>::CreateUObject(
            this,
            &UTGVisualizationTimelineSplitterWidget::HandleResizeDelta))
        .OnResizeFinished(FSimpleDelegate::CreateUObject(
            this,
            &UTGVisualizationTimelineSplitterWidget::HandleResizeFinished))
        .OnToggleRequested(FSimpleDelegate::CreateUObject(
            this,
            &UTGVisualizationTimelineSplitterWidget::HandleToggleRequested));
    return SplitterWidget.ToSharedRef();
}

void UTGVisualizationTimelineSplitterWidget::ReleaseSlateResources(
    const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    SplitterWidget.Reset();
}

float UTGVisualizationTimelineSplitterWidget::GetTimelineHeight() const
{
    const UCanvasPanelSlot* CanvasSlot = TimelinePanel != nullptr
        ? Cast<UCanvasPanelSlot>(TimelinePanel->Slot)
        : nullptr;
    return CanvasSlot != nullptr
        ? static_cast<float>(CanvasSlot->GetOffsets().Bottom)
        : LastExpandedHeight;
}

void UTGVisualizationTimelineSplitterWidget::SetTimelineHeight(
    const float Height)
{
    UCanvasPanelSlot* CanvasSlot = TimelinePanel != nullptr
        ? Cast<UCanvasPanelSlot>(TimelinePanel->Slot)
        : nullptr;
    if (CanvasSlot == nullptr)
    {
        return;
    }

    const float SafeHeight = FMath::Max(CollapsedPanelHeight, Height);
    FMargin Offsets = CanvasSlot->GetOffsets();
    Offsets.Bottom = SafeHeight;
    CanvasSlot->SetOffsets(Offsets);
    CanvasSlot->SetZOrder(90);

    if (TimelineBody != nullptr)
    {
        TimelineBody->SetVisibility(
            SafeHeight <= CollapsedPanelHeight + 1.0f
                ? ESlateVisibility::Collapsed
                : ESlateVisibility::Visible);
    }
    TimelinePanel->InvalidateLayoutAndVolatility();
}

void UTGVisualizationTimelineSplitterWidget::HandleResizeDelta(
    const float DeltaScreenY)
{
    const float RequestedHeight = FMath::Max(
        CollapsedPanelHeight,
        GetTimelineHeight() - DeltaScreenY);
    SetTimelineHeight(RequestedHeight);
}

void UTGVisualizationTimelineSplitterWidget::HandleResizeFinished()
{
    const float CurrentHeight = GetTimelineHeight();
    if (CurrentHeight <= CollapsedPanelHeight + CollapseSnapDistance)
    {
        CollapseTimeline();
        return;
    }
    LastExpandedHeight = FMath::Max(MinimumExpandedHeight, CurrentHeight);
    SetTimelineHeight(LastExpandedHeight);
}

void UTGVisualizationTimelineSplitterWidget::HandleToggleRequested()
{
    if (GetTimelineHeight() <= CollapsedPanelHeight + 1.0f)
    {
        ExpandTimeline();
    }
    else
    {
        CollapseTimeline();
    }
}
