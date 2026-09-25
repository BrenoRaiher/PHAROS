// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "Components/EditableTextBox.h"
#include "TGSearchBox.generated.h"

/** UMG wrapper around Slate's standard search field with search and clear icons. */
UCLASS(meta = (DisplayName = "PHAROS Search Box"))
class TG_API UTGSearchBox final : public UEditableTextBox
{
    GENERATED_BODY()

public:
    explicit UTGSearchBox(const FObjectInitializer& ObjectInitializer);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};
