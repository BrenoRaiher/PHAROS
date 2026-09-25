// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Components/LineBatchComponent.h"
#include "TGScreenSpaceLineBatchComponent.generated.h"

/**
 * Draws world-space line segments with a constant screen-space thickness.
 *
 * Segment positions still participate in the normal perspective projection
 * and world depth test. Only their rendered width is measured in pixels, so
 * distant trajectory segments remain legible without widening nearby ones.
 */
UCLASS(ClassGroup = (TG))
class TG_API UTGScreenSpaceLineBatchComponent final
    : public ULineBatchComponent
{
    GENERATED_BODY()

public:
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
};
