// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

// UnrealBuildTool defines TGSIMCORE_API when TGSimCore is built as a UE module.
// Standalone CMake builds use a static library and therefore need no annotation.
#ifndef TGSIMCORE_API
#define TGSIMCORE_API
#endif
