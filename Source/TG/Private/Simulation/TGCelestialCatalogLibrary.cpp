// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/TGCelestialCatalogLibrary.h"

namespace
{
    struct FCatalogRowSpecification
    {
        const TCHAR* GroupKey;
        const TCHAR* GroupDisplayName;

        const TCHAR* CatalogKey;
        const TCHAR* DisplayName;

        const TCHAR* SpiceTarget;
        int32 NaifId;

        ETGCelestialSourceRole SourceRole;

        bool bSupportsHarmonicGravity;
        bool bIsPrincipalMoon;
    };

    static const FCatalogRowSpecification CatalogSpecifications[] =
    {
        // -----------------------------------------------------------------
        // Solar
        // -----------------------------------------------------------------
        {
            TEXT("Solar"),
            TEXT("Solar"),

            TEXT("Sun"),
            TEXT("Sun"),

            TEXT("SUN"),
            10,

            ETGCelestialSourceRole::IndependentBody,

            true,
            false
        },

        // -----------------------------------------------------------------
        // Mercury
        // -----------------------------------------------------------------
        {
            TEXT("MercurySystem"),
            TEXT("Mercury System"),

            TEXT("MercuryBarycenter"),
            TEXT("Mercury barycenter"),

            TEXT("MERCURY BARYCENTER"),
            1,

            ETGCelestialSourceRole::SystemBarycenter,

            false,
            false
        },
        {
            TEXT("MercurySystem"),
            TEXT("Mercury System"),

            TEXT("Mercury"),
            TEXT("Mercury"),

            TEXT("MERCURY"),
            199,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            false
        },

        // -----------------------------------------------------------------
        // Venus
        // -----------------------------------------------------------------
        {
            TEXT("VenusSystem"),
            TEXT("Venus System"),

            TEXT("VenusBarycenter"),
            TEXT("Venus barycenter"),

            TEXT("VENUS BARYCENTER"),
            2,

            ETGCelestialSourceRole::SystemBarycenter,

            false,
            false
        },
        {
            TEXT("VenusSystem"),
            TEXT("Venus System"),

            TEXT("Venus"),
            TEXT("Venus"),

            TEXT("VENUS"),
            299,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            false
        },

        // -----------------------------------------------------------------
        // Earth-Moon
        // -----------------------------------------------------------------
        {
            TEXT("EarthMoonSystem"),
            TEXT("Earth-Moon System"),

            TEXT("EarthMoonBarycenter"),
            TEXT("Earth-Moon barycenter"),

            TEXT("EARTH BARYCENTER"),
            3,

            ETGCelestialSourceRole::SystemBarycenter,

            false,
            false
        },
        {
            TEXT("EarthMoonSystem"),
            TEXT("Earth-Moon System"),

            TEXT("Earth"),
            TEXT("Earth"),

            TEXT("EARTH"),
            399,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            false
        },
        {
            TEXT("EarthMoonSystem"),
            TEXT("Earth-Moon System"),

            TEXT("Moon"),
            TEXT("Moon"),

            TEXT("MOON"),
            301,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            false
        },

        // -----------------------------------------------------------------
        // Mars
        // -----------------------------------------------------------------
        {
            TEXT("MarsSystem"),
            TEXT("Mars System"),

            TEXT("MarsBarycenter"),
            TEXT("Mars barycenter"),

            TEXT("MARS BARYCENTER"),
            4,

            ETGCelestialSourceRole::SystemBarycenter,

            false,
            false
        },
        {
            TEXT("MarsSystem"),
            TEXT("Mars System"),

            TEXT("Mars"),
            TEXT("Mars"),

            TEXT("MARS"),
            499,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            false
        },
        {
            TEXT("MarsSystem"),
            TEXT("Mars System"),

            TEXT("Phobos"),
            TEXT("Phobos"),

            TEXT("PHOBOS"),
            401,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },
        {
            TEXT("MarsSystem"),
            TEXT("Mars System"),

            TEXT("Deimos"),
            TEXT("Deimos"),

            TEXT("DEIMOS"),
            402,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },

        // -----------------------------------------------------------------
        // Jupiter
        // -----------------------------------------------------------------
        {
            TEXT("JupiterSystem"),
            TEXT("Jupiter System"),

            TEXT("JupiterBarycenter"),
            TEXT("Jupiter barycenter"),

            TEXT("JUPITER BARYCENTER"),
            5,

            ETGCelestialSourceRole::SystemBarycenter,

            false,
            false
        },
        {
            TEXT("JupiterSystem"),
            TEXT("Jupiter System"),

            TEXT("Jupiter"),
            TEXT("Jupiter"),

            TEXT("JUPITER"),
            599,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            false
        },
        {
            TEXT("JupiterSystem"),
            TEXT("Jupiter System"),

            TEXT("Io"),
            TEXT("Io"),

            TEXT("IO"),
            501,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },
        {
            TEXT("JupiterSystem"),
            TEXT("Jupiter System"),

            TEXT("Europa"),
            TEXT("Europa"),

            TEXT("EUROPA"),
            502,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },
        {
            TEXT("JupiterSystem"),
            TEXT("Jupiter System"),

            TEXT("Ganymede"),
            TEXT("Ganymede"),

            TEXT("GANYMEDE"),
            503,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },
        {
            TEXT("JupiterSystem"),
            TEXT("Jupiter System"),

            TEXT("Callisto"),
            TEXT("Callisto"),

            TEXT("CALLISTO"),
            504,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },

        // -----------------------------------------------------------------
        // Saturn
        // -----------------------------------------------------------------
        {
            TEXT("SaturnSystem"),
            TEXT("Saturn System"),

            TEXT("SaturnBarycenter"),
            TEXT("Saturn barycenter"),

            TEXT("SATURN BARYCENTER"),
            6,

            ETGCelestialSourceRole::SystemBarycenter,

            false,
            false
        },
        {
            TEXT("SaturnSystem"),
            TEXT("Saturn System"),

            TEXT("Saturn"),
            TEXT("Saturn"),

            TEXT("SATURN"),
            699,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            false
        },
        {
            TEXT("SaturnSystem"),
            TEXT("Saturn System"),

            TEXT("Mimas"),
            TEXT("Mimas"),

            TEXT("MIMAS"),
            601,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },
        {
            TEXT("SaturnSystem"),
            TEXT("Saturn System"),

            TEXT("Enceladus"),
            TEXT("Enceladus"),

            TEXT("ENCELADUS"),
            602,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },
        {
            TEXT("SaturnSystem"),
            TEXT("Saturn System"),

            TEXT("Tethys"),
            TEXT("Tethys"),

            TEXT("TETHYS"),
            603,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },
        {
            TEXT("SaturnSystem"),
            TEXT("Saturn System"),

            TEXT("Dione"),
            TEXT("Dione"),

            TEXT("DIONE"),
            604,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },
        {
            TEXT("SaturnSystem"),
            TEXT("Saturn System"),

            TEXT("Rhea"),
            TEXT("Rhea"),

            TEXT("RHEA"),
            605,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },
        {
            TEXT("SaturnSystem"),
            TEXT("Saturn System"),

            TEXT("Titan"),
            TEXT("Titan"),

            TEXT("TITAN"),
            606,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },
        {
            TEXT("SaturnSystem"),
            TEXT("Saturn System"),

            TEXT("Iapetus"),
            TEXT("Iapetus"),

            TEXT("IAPETUS"),
            608,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },
        {
            TEXT("SaturnSystem"),
            TEXT("Saturn System"),

            TEXT("Phoebe"),
            TEXT("Phoebe"),

            TEXT("PHOEBE"),
            609,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },

        // -----------------------------------------------------------------
        // Uranus
        // -----------------------------------------------------------------
        {
            TEXT("UranusSystem"),
            TEXT("Uranus System"),

            TEXT("UranusBarycenter"),
            TEXT("Uranus barycenter"),

            TEXT("URANUS BARYCENTER"),
            7,

            ETGCelestialSourceRole::SystemBarycenter,

            false,
            false
        },
        {
            TEXT("UranusSystem"),
            TEXT("Uranus System"),

            TEXT("Uranus"),
            TEXT("Uranus"),

            TEXT("URANUS"),
            799,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            false
        },
        {
            TEXT("UranusSystem"),
            TEXT("Uranus System"),

            TEXT("Miranda"),
            TEXT("Miranda"),

            TEXT("MIRANDA"),
            705,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },
        {
            TEXT("UranusSystem"),
            TEXT("Uranus System"),

            TEXT("Ariel"),
            TEXT("Ariel"),

            TEXT("ARIEL"),
            701,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },
        {
            TEXT("UranusSystem"),
            TEXT("Uranus System"),

            TEXT("Umbriel"),
            TEXT("Umbriel"),

            TEXT("UMBRIEL"),
            702,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },
        {
            TEXT("UranusSystem"),
            TEXT("Uranus System"),

            TEXT("Titania"),
            TEXT("Titania"),

            TEXT("TITANIA"),
            703,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },
        {
            TEXT("UranusSystem"),
            TEXT("Uranus System"),

            TEXT("Oberon"),
            TEXT("Oberon"),

            TEXT("OBERON"),
            704,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },

        // -----------------------------------------------------------------
        // Neptune
        // -----------------------------------------------------------------
        {
            TEXT("NeptuneSystem"),
            TEXT("Neptune System"),

            TEXT("NeptuneBarycenter"),
            TEXT("Neptune barycenter"),

            TEXT("NEPTUNE BARYCENTER"),
            8,

            ETGCelestialSourceRole::SystemBarycenter,

            false,
            false
        },
        {
            TEXT("NeptuneSystem"),
            TEXT("Neptune System"),

            TEXT("Neptune"),
            TEXT("Neptune"),

            TEXT("NEPTUNE"),
            899,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            false
        },
        {
            TEXT("NeptuneSystem"),
            TEXT("Neptune System"),

            TEXT("Triton"),
            TEXT("Triton"),

            TEXT("TRITON"),
            801,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },

        // -----------------------------------------------------------------
        // Pluto
        // -----------------------------------------------------------------
        {
            TEXT("PlutoSystem"),
            TEXT("Pluto System"),

            TEXT("PlutoBarycenter"),
            TEXT("Pluto barycenter"),

            TEXT("PLUTO BARYCENTER"),
            9,

            ETGCelestialSourceRole::SystemBarycenter,

            false,
            false
        },
        {
            TEXT("PlutoSystem"),
            TEXT("Pluto System"),

            TEXT("Pluto"),
            TEXT("Pluto"),

            TEXT("PLUTO"),
            999,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            false
        },
        {
            TEXT("PlutoSystem"),
            TEXT("Pluto System"),

            TEXT("Charon"),
            TEXT("Charon"),

            TEXT("CHARON"),
            901,

            ETGCelestialSourceRole::PhysicalSystemMember,

            true,
            true
        },

        // -----------------------------------------------------------------
        // Main minor planets
        // -----------------------------------------------------------------
        {
            TEXT("MainMinorPlanets"),
            TEXT("Main Minor Planets"),

            TEXT("Ceres"),
            TEXT("Ceres"),

            TEXT("2000001"),
            2000001,

            ETGCelestialSourceRole::IndependentBody,

            false,
            false
        },
        {
            TEXT("MainMinorPlanets"),
            TEXT("Main Minor Planets"),

            TEXT("Pallas"),
            TEXT("Pallas"),

            TEXT("2000002"),
            2000002,

            ETGCelestialSourceRole::IndependentBody,

            false,
            false
        },
        {
            TEXT("MainMinorPlanets"),
            TEXT("Main Minor Planets"),

            TEXT("Vesta"),
            TEXT("Vesta"),

            TEXT("2000004"),
            2000004,

            ETGCelestialSourceRole::IndependentBody,

            false,
            false
        }
    };

    const TArray<FTGCelestialCatalogEntry>&
    GetCatalogInternal()
    {
        static const TArray<FTGCelestialCatalogEntry> Catalog =
            []()
            {
                TArray<FTGCelestialCatalogEntry> Result;
                Result.Reserve(
                    UE_ARRAY_COUNT(CatalogSpecifications));

                for (const FCatalogRowSpecification& Specification
                     : CatalogSpecifications)
                {
                    FTGCelestialCatalogEntry Entry;

                    Entry.CatalogKey =
                        FName(Specification.CatalogKey);

                    Entry.DisplayName =
                        FText::FromString(
                            Specification.DisplayName);

                    Entry.SpiceTarget =
                        Specification.SpiceTarget;

                    Entry.NaifId =
                        Specification.NaifId;

                    Entry.SystemKey =
                        FName(Specification.GroupKey);

                    Entry.SystemDisplayName =
                        FText::FromString(
                            Specification.GroupDisplayName);

                    Entry.SourceRole =
                        Specification.SourceRole;

                    Entry.bSupportsHarmonicGravity =
                        Specification.bSupportsHarmonicGravity;

                    Entry.bIsPrincipalMoon =
                        Specification.bIsPrincipalMoon;

                    Result.Add(MoveTemp(Entry));
                }

                return Result;
            }();

        return Catalog;
    }
}

TArray<FTGCelestialCatalogEntry>
UTGCelestialCatalogLibrary::GetCelestialCatalog()
{
    return GetCatalogInternal();
}

TArray<FTGCelestialCatalogGroup>
UTGCelestialCatalogLibrary::GetCelestialCatalogGroups()
{
    TArray<FTGCelestialCatalogGroup> Groups;

    for (const FTGCelestialCatalogEntry& Entry
         : GetCatalogInternal())
    {
        int32 GroupIndex =
            Groups.IndexOfByPredicate(
                [&Entry](
                    const FTGCelestialCatalogGroup& Group)
                {
                    return Group.GroupKey == Entry.SystemKey;
                });

        if (GroupIndex == INDEX_NONE)
        {
            FTGCelestialCatalogGroup NewGroup;
            NewGroup.GroupKey = Entry.SystemKey;
            NewGroup.DisplayName =
                Entry.SystemDisplayName;

            GroupIndex =
                Groups.Add(MoveTemp(NewGroup));
        }

        Groups[GroupIndex].Sources.Add(Entry);
    }

    return Groups;
}

bool UTGCelestialCatalogLibrary::FindCelestialCatalogEntry(
    const FName CatalogKey,
    FTGCelestialCatalogEntry& OutEntry)
{
    const FTGCelestialCatalogEntry* FoundEntry =
        GetCatalogInternal().FindByPredicate(
            [&CatalogKey](
                const FTGCelestialCatalogEntry& Entry)
            {
                return Entry.CatalogKey == CatalogKey;
            });

    if (FoundEntry == nullptr)
    {
        OutEntry = FTGCelestialCatalogEntry{};
        return false;
    }

    OutEntry = *FoundEntry;
    return true;
}

FText UTGCelestialCatalogLibrary::GetCelestialSourceRoleDisplayText(
    const ETGCelestialSourceRole SourceRole)
{
    switch (SourceRole)
    {
        case ETGCelestialSourceRole::IndependentBody:
            return FText::FromString(TEXT("Independent Body"));
        case ETGCelestialSourceRole::SystemBarycenter:
            return FText::FromString(TEXT("System Barycenter"));
        case ETGCelestialSourceRole::PhysicalSystemMember:
            return FText::FromString(TEXT("Physical System Member"));
        default:
            return FText::GetEmpty();
    }
}

bool UTGCelestialCatalogLibrary::SystemContainsPrincipalMoon(
    const FName SystemKey)
{
    return GetCatalogInternal().ContainsByPredicate(
        [&SystemKey](
            const FTGCelestialCatalogEntry& Entry)
        {
            return
                Entry.SystemKey == SystemKey
                && Entry.bIsPrincipalMoon;
        });
}
