// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/Export.h"
#include "TGSim/Core/ModelTypes.h"

#include <string>
#include <vector>

namespace tgsim::scenario
{
    struct CelestialCatalogEntry
    {
        std::string group_key;
        std::string group_display_name;
        std::string catalog_key;
        std::string display_name;
        std::string spice_target;
        int naif_id = 0;
        GravitySourceRole source_role = GravitySourceRole::Independent;
        bool supports_harmonic_gravity = false;
        bool is_principal_moon = false;
    };

    TGSIMCORE_API const std::vector<CelestialCatalogEntry>&
    GetCelestialCatalog();
    TGSIMCORE_API const CelestialCatalogEntry* FindCelestialCatalogEntry(
        const std::string& catalog_key);
}
