// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Components/MultiLineEditableTextBox.h"

#include "TGControllerSourceEditor.generated.h"

/** Controller source editor with the shared code-preview surface and scrollbar. */
UCLASS(meta = (DisplayName = "PHAROS Controller Source Editor"))
class TG_API UTGControllerSourceEditor final
    : public UMultiLineEditableTextBox
{
    GENERATED_BODY()

public:
    explicit UTGControllerSourceEditor(
        const FObjectInitializer& ObjectInitializer);

    virtual void SynchronizeProperties() override;

private:
    void ApplyControllerEditorStyle();
};
