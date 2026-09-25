// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"

namespace TGLegalDocumentUtils
{
    bool ResolvePath(
        const FString& FileName,
        FString& OutPath);

    bool LoadText(
        const FString& FileName,
        FString& OutText,
        FString& OutPath);

    bool Open(const FString& FileName);
}
