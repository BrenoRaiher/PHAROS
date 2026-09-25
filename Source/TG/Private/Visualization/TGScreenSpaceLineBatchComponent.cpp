// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Visualization/TGScreenSpaceLineBatchComponent.h"

#include "MeshElementCollector.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"

namespace
{

/** Render-thread snapshot of the trajectory line segments. */
class FTGScreenSpaceLineBatchSceneProxy final : public FPrimitiveSceneProxy
{
public:
    explicit FTGScreenSpaceLineBatchSceneProxy(
        const UTGScreenSpaceLineBatchComponent* Component)
        : FPrimitiveSceneProxy(Component)
        , Lines(Component->BatchedLines)
    {
        bWillEverBeLit = false;
    }

    virtual SIZE_T GetTypeHash() const override
    {
        static size_t UniquePointer;
        return reinterpret_cast<SIZE_T>(&UniquePointer);
    }

    virtual void GetDynamicMeshElements(
        const TArray<const FSceneView*>& Views,
        const FSceneViewFamily& ViewFamily,
        const uint32 VisibilityMap,
        FMeshElementCollector& Collector) const override
    {
        QUICK_SCOPE_CYCLE_COUNTER(
            STAT_TGScreenSpaceLineBatchSceneProxy_GetDynamicMeshElements);

        for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
        {
            if ((VisibilityMap & (1u << ViewIndex)) == 0)
            {
                continue;
            }

            FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
            for (const FBatchedLine& Line : Lines)
            {
                // The endpoints remain physical world positions. The final
                // argument makes only the width independent of perspective.
                PDI->DrawLine(
                    Line.Start,
                    Line.End,
                    Line.Color,
                    Line.DepthPriority,
                    Line.Thickness,
                    0.0f,
                    true);
            }
        }
    }

    virtual FPrimitiveViewRelevance GetViewRelevance(
        const FSceneView* View) const override
    {
        FPrimitiveViewRelevance Relevance;
        Relevance.bDrawRelevance = IsShown(View);
        Relevance.bDynamicRelevance = true;
        Relevance.bOpaque = true;
        Relevance.bRenderInMainPass = ShouldRenderInMainPass();
        return Relevance;
    }

    virtual uint32 GetMemoryFootprint() const override
    {
        return sizeof(*this) + Lines.GetAllocatedSize();
    }

private:
    TArray<FBatchedLine> Lines;
};

} // namespace

FPrimitiveSceneProxy* UTGScreenSpaceLineBatchComponent::CreateSceneProxy()
{
    return new FTGScreenSpaceLineBatchSceneProxy(this);
}
