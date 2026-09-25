// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Scenario/CelestialCatalog.h"

#include <algorithm>
#include <cctype>

namespace tgsim::scenario
{
    namespace
    {
        bool EqualCaseInsensitive(const std::string& a, const std::string& b)
        {
            return a.size() == b.size() &&
                std::equal(
                    a.begin(), a.end(), b.begin(),
                    [](const unsigned char lhs, const unsigned char rhs)
                    {
                        return std::tolower(lhs) == std::tolower(rhs);
                    });
        }
    }

    const std::vector<CelestialCatalogEntry>& GetCelestialCatalog()
    {
        static const std::vector<CelestialCatalogEntry> catalog = {
            {"Solar", "Solar", "Sun", "Sun", "SUN", 10,
             GravitySourceRole::Independent, true, false},

            {"MercurySystem", "Mercury System", "MercuryBarycenter",
             "Mercury barycenter", "MERCURY BARYCENTER", 1,
             GravitySourceRole::SystemBarycenter, false, false},
            {"MercurySystem", "Mercury System", "Mercury", "Mercury",
             "MERCURY", 199, GravitySourceRole::SystemMember, true, false},

            {"VenusSystem", "Venus System", "VenusBarycenter",
             "Venus barycenter", "VENUS BARYCENTER", 2,
             GravitySourceRole::SystemBarycenter, false, false},
            {"VenusSystem", "Venus System", "Venus", "Venus", "VENUS", 299,
             GravitySourceRole::SystemMember, true, false},

            {"EarthMoonSystem", "Earth-Moon System", "EarthMoonBarycenter",
             "Earth-Moon barycenter", "EARTH BARYCENTER", 3,
             GravitySourceRole::SystemBarycenter, false, false},
            {"EarthMoonSystem", "Earth-Moon System", "Earth", "Earth",
             "EARTH", 399, GravitySourceRole::SystemMember, true, false},
            {"EarthMoonSystem", "Earth-Moon System", "Moon", "Moon", "MOON",
             301, GravitySourceRole::SystemMember, true, false},

            {"MarsSystem", "Mars System", "MarsBarycenter", "Mars barycenter",
             "MARS BARYCENTER", 4, GravitySourceRole::SystemBarycenter, false,
             false},
            {"MarsSystem", "Mars System", "Mars", "Mars", "MARS", 499,
             GravitySourceRole::SystemMember, true, false},
            {"MarsSystem", "Mars System", "Phobos", "Phobos", "PHOBOS", 401,
             GravitySourceRole::SystemMember, true, true},
            {"MarsSystem", "Mars System", "Deimos", "Deimos", "DEIMOS", 402,
             GravitySourceRole::SystemMember, true, true},

            {"JupiterSystem", "Jupiter System", "JupiterBarycenter",
             "Jupiter barycenter", "JUPITER BARYCENTER", 5,
             GravitySourceRole::SystemBarycenter, false, false},
            {"JupiterSystem", "Jupiter System", "Jupiter", "Jupiter",
             "JUPITER", 599, GravitySourceRole::SystemMember, true, false},
            {"JupiterSystem", "Jupiter System", "Io", "Io", "IO", 501,
             GravitySourceRole::SystemMember, true, true},
            {"JupiterSystem", "Jupiter System", "Europa", "Europa", "EUROPA",
             502, GravitySourceRole::SystemMember, true, true},
            {"JupiterSystem", "Jupiter System", "Ganymede", "Ganymede",
             "GANYMEDE", 503, GravitySourceRole::SystemMember, true, true},
            {"JupiterSystem", "Jupiter System", "Callisto", "Callisto",
             "CALLISTO", 504, GravitySourceRole::SystemMember, true, true},

            {"SaturnSystem", "Saturn System", "SaturnBarycenter",
             "Saturn barycenter", "SATURN BARYCENTER", 6,
             GravitySourceRole::SystemBarycenter, false, false},
            {"SaturnSystem", "Saturn System", "Saturn", "Saturn", "SATURN",
             699, GravitySourceRole::SystemMember, true, false},
            {"SaturnSystem", "Saturn System", "Mimas", "Mimas", "MIMAS", 601,
             GravitySourceRole::SystemMember, true, true},
            {"SaturnSystem", "Saturn System", "Enceladus", "Enceladus",
             "ENCELADUS", 602, GravitySourceRole::SystemMember, true, true},
            {"SaturnSystem", "Saturn System", "Tethys", "Tethys", "TETHYS",
             603, GravitySourceRole::SystemMember, true, true},
            {"SaturnSystem", "Saturn System", "Dione", "Dione", "DIONE", 604,
             GravitySourceRole::SystemMember, true, true},
            {"SaturnSystem", "Saturn System", "Rhea", "Rhea", "RHEA", 605,
             GravitySourceRole::SystemMember, true, true},
            {"SaturnSystem", "Saturn System", "Titan", "Titan", "TITAN", 606,
             GravitySourceRole::SystemMember, true, true},
            {"SaturnSystem", "Saturn System", "Iapetus", "Iapetus", "IAPETUS",
             608, GravitySourceRole::SystemMember, true, true},
            {"SaturnSystem", "Saturn System", "Phoebe", "Phoebe", "PHOEBE",
             609, GravitySourceRole::SystemMember, true, true},

            {"UranusSystem", "Uranus System", "UranusBarycenter",
             "Uranus barycenter", "URANUS BARYCENTER", 7,
             GravitySourceRole::SystemBarycenter, false, false},
            {"UranusSystem", "Uranus System", "Uranus", "Uranus", "URANUS",
             799, GravitySourceRole::SystemMember, true, false},
            {"UranusSystem", "Uranus System", "Miranda", "Miranda", "MIRANDA",
             705, GravitySourceRole::SystemMember, true, true},
            {"UranusSystem", "Uranus System", "Ariel", "Ariel", "ARIEL", 701,
             GravitySourceRole::SystemMember, true, true},
            {"UranusSystem", "Uranus System", "Umbriel", "Umbriel", "UMBRIEL",
             702, GravitySourceRole::SystemMember, true, true},
            {"UranusSystem", "Uranus System", "Titania", "Titania", "TITANIA",
             703, GravitySourceRole::SystemMember, true, true},
            {"UranusSystem", "Uranus System", "Oberon", "Oberon", "OBERON",
             704, GravitySourceRole::SystemMember, true, true},

            {"NeptuneSystem", "Neptune System", "NeptuneBarycenter",
             "Neptune barycenter", "NEPTUNE BARYCENTER", 8,
             GravitySourceRole::SystemBarycenter, false, false},
            {"NeptuneSystem", "Neptune System", "Neptune", "Neptune",
             "NEPTUNE", 899, GravitySourceRole::SystemMember, true, false},
            {"NeptuneSystem", "Neptune System", "Triton", "Triton", "TRITON",
             801, GravitySourceRole::SystemMember, true, true},

            {"PlutoSystem", "Pluto System", "PlutoBarycenter",
             "Pluto barycenter", "PLUTO BARYCENTER", 9,
             GravitySourceRole::SystemBarycenter, false, false},
            {"PlutoSystem", "Pluto System", "Pluto", "Pluto", "PLUTO", 999,
             GravitySourceRole::SystemMember, true, false},
            {"PlutoSystem", "Pluto System", "Charon", "Charon", "CHARON", 901,
             GravitySourceRole::SystemMember, true, true},

            {"MainMinorPlanets", "Main Minor Planets", "Ceres", "Ceres",
             "2000001", 2000001, GravitySourceRole::Independent, false, false},
            {"MainMinorPlanets", "Main Minor Planets", "Pallas", "Pallas",
             "2000002", 2000002, GravitySourceRole::Independent, false, false},
            {"MainMinorPlanets", "Main Minor Planets", "Vesta", "Vesta",
             "2000004", 2000004, GravitySourceRole::Independent, false, false}
        };
        return catalog;
    }

    const CelestialCatalogEntry* FindCelestialCatalogEntry(
        const std::string& catalog_key)
    {
        const auto& catalog = GetCelestialCatalog();
        const auto found = std::find_if(
            catalog.begin(), catalog.end(),
            [&catalog_key](const CelestialCatalogEntry& entry)
            {
                return EqualCaseInsensitive(entry.catalog_key, catalog_key);
            });
        return found == catalog.end() ? nullptr : &*found;
    }
}
