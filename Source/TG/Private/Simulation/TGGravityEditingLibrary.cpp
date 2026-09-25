// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/TGGravityEditingLibrary.h"

#include "Simulation/TGCelestialCatalogLibrary.h"

namespace
{
    const FName SunCatalogKey(TEXT("Sun"));

    FTGCelestialBodyConfig MakeDefaultConfig(
        const FTGCelestialCatalogEntry& CatalogEntry)
    {
        FTGCelestialBodyConfig Config;

        Config.CatalogKey = CatalogEntry.CatalogKey;

        // A new scenario starts with the Sun only. In particular, no
        // barycenter is selected on the user's behalf.
        Config.bGravityEnabled =
            CatalogEntry.CatalogKey == SunCatalogKey;

        Config.AutomaticActivationRadiusMeters = 0.0;
        Config.BarycenterResolutionRadiusMeters = 0.0;
        Config.HarmonicModelCsvFilePath.Reset();
        Config.MaximumHarmonicDegreeUsed = 0;

        return Config;
    }

    FTGCelestialBodyConfig MakeMissingConfig(
        const FTGCelestialCatalogEntry& CatalogEntry)
    {
        FTGCelestialBodyConfig Config =
            MakeDefaultConfig(CatalogEntry);

        // Normalization repairs the fixed HUD catalog shape. It must not turn
        // a source omitted by an imported/authored scenario into active
        // gravity merely because that source has a new-draft default.
        Config.bGravityEnabled = false;
        return Config;
    }

    void RemoveUnsupportedFields(
        const FTGCelestialCatalogEntry& CatalogEntry,
        FTGCelestialBodyConfig& Config)
    {
        Config.CatalogKey = CatalogEntry.CatalogKey;

        if (CatalogEntry.SourceRole
            != ETGCelestialSourceRole::SystemBarycenter)
        {
            Config.BarycenterResolutionRadiusMeters = 0.0;
        }

        if (!CatalogEntry.bSupportsHarmonicGravity)
        {
            Config.HarmonicModelCsvFilePath.Reset();
            Config.MaximumHarmonicDegreeUsed = 0;
        }
    }

    int32 FindConfigIndex(
        const TArray<FTGCelestialBodyConfig>& Configs,
        const FName CatalogKey)
    {
        return Configs.IndexOfByPredicate(
            [&CatalogKey](
                const FTGCelestialBodyConfig& Config)
            {
                return Config.CatalogKey == CatalogKey;
            });
    }

    template<typename TEditFunction>
    TArray<FTGCelestialBodyConfig> EditConfig(
        const TArray<FTGCelestialBodyConfig>& Configs,
        const FName CatalogKey,
        TEditFunction&& EditFunction)
    {
        TArray<FTGCelestialBodyConfig> Result =
            UTGGravityEditingLibrary::
                NormalizeCelestialBodyConfigs(Configs);

        const int32 ConfigIndex =
            FindConfigIndex(Result, CatalogKey);

        if (ConfigIndex == INDEX_NONE)
        {
            return Result;
        }

        FTGCelestialCatalogEntry CatalogEntry;

        if (!UTGCelestialCatalogLibrary::
            FindCelestialCatalogEntry(
                CatalogKey,
                CatalogEntry))
        {
            return Result;
        }

        EditFunction(Result[ConfigIndex]);
        RemoveUnsupportedFields(
            CatalogEntry,
            Result[ConfigIndex]);

        return Result;
    }

    bool CanSourceBecomeActive(
        const FTGCelestialBodyConfig& Config)
    {
        return
            Config.bGravityEnabled
            || Config.AutomaticActivationRadiusMeters > 0.0;
    }
}

TArray<FTGCelestialBodyConfig>
UTGGravityEditingLibrary::MakeDefaultCelestialBodyConfigs()
{
    const TArray<FTGCelestialCatalogEntry> Catalog =
        UTGCelestialCatalogLibrary::GetCelestialCatalog();

    TArray<FTGCelestialBodyConfig> Result;
    Result.Reserve(Catalog.Num());

    for (const FTGCelestialCatalogEntry& CatalogEntry
         : Catalog)
    {
        Result.Add(MakeDefaultConfig(CatalogEntry));
    }

    return Result;
}

TArray<FTGCelestialBodyConfig>
UTGGravityEditingLibrary::NormalizeCelestialBodyConfigs(
    const TArray<FTGCelestialBodyConfig>& ExistingConfigs)
{
    TMap<FName, FTGCelestialBodyConfig> FirstConfigByKey;

    for (const FTGCelestialBodyConfig& ExistingConfig
         : ExistingConfigs)
    {
        if (ExistingConfig.CatalogKey.IsNone())
        {
            continue;
        }

        if (!FirstConfigByKey.Contains(
            ExistingConfig.CatalogKey))
        {
            FirstConfigByKey.Add(
                ExistingConfig.CatalogKey,
                ExistingConfig);
        }
    }

    const TArray<FTGCelestialCatalogEntry> Catalog =
        UTGCelestialCatalogLibrary::GetCelestialCatalog();

    TArray<FTGCelestialBodyConfig> Result;
    Result.Reserve(Catalog.Num());

    for (const FTGCelestialCatalogEntry& CatalogEntry
         : Catalog)
    {
        FTGCelestialBodyConfig Config;

        if (const FTGCelestialBodyConfig* ExistingConfig =
            FirstConfigByKey.Find(CatalogEntry.CatalogKey))
        {
            Config = *ExistingConfig;
        }
        else
        {
            Config = MakeMissingConfig(CatalogEntry);
        }

        RemoveUnsupportedFields(CatalogEntry, Config);
        Result.Add(MoveTemp(Config));
    }

    return Result;
}

bool UTGGravityEditingLibrary::FindCelestialBodyConfig(
    const TArray<FTGCelestialBodyConfig>& Configs,
    const FName CatalogKey,
    FTGCelestialBodyConfig& OutConfig)
{
    const TArray<FTGCelestialBodyConfig> NormalizedConfigs =
        NormalizeCelestialBodyConfigs(Configs);

    const int32 ConfigIndex =
        FindConfigIndex(NormalizedConfigs, CatalogKey);

    if (ConfigIndex == INDEX_NONE)
    {
        OutConfig = FTGCelestialBodyConfig{};
        return false;
    }

    OutConfig = NormalizedConfigs[ConfigIndex];
    return true;
}

TArray<FTGCelestialBodyConfig>
UTGGravityEditingLibrary::ReplaceCelestialBodyConfig(
    const TArray<FTGCelestialBodyConfig>& Configs,
    const FTGCelestialBodyConfig& UpdatedConfig)
{
    return EditConfig(
        Configs,
        UpdatedConfig.CatalogKey,
        [&UpdatedConfig](
            FTGCelestialBodyConfig& ExistingConfig)
        {
            ExistingConfig = UpdatedConfig;
        });
}

TArray<FTGCelestialBodyConfig>
UTGGravityEditingLibrary::SetGravityEnabledWithExclusivity(
    const TArray<FTGCelestialBodyConfig>& Configs,
    const FName CatalogKey,
    const bool bGravityEnabled)
{
    TArray<FTGCelestialBodyConfig> Result =
        NormalizeCelestialBodyConfigs(Configs);

    const int32 TargetIndex =
        FindConfigIndex(Result, CatalogKey);

    if (TargetIndex == INDEX_NONE)
    {
        return Result;
    }

    FTGCelestialCatalogEntry TargetCatalogEntry;

    if (!UTGCelestialCatalogLibrary::
        FindCelestialCatalogEntry(
            CatalogKey,
            TargetCatalogEntry))
    {
        return Result;
    }

    Result[TargetIndex].bGravityEnabled =
        bGravityEnabled;

    if (!bGravityEnabled)
    {
        return Result;
    }

    const TArray<FTGCelestialCatalogEntry> Catalog =
        UTGCelestialCatalogLibrary::GetCelestialCatalog();

    if (TargetCatalogEntry.SourceRole
        == ETGCelestialSourceRole::PhysicalSystemMember)
    {
        for (const FTGCelestialCatalogEntry& CatalogEntry
             : Catalog)
        {
            if (
                CatalogEntry.SystemKey
                    == TargetCatalogEntry.SystemKey
                && CatalogEntry.SourceRole
                    == ETGCelestialSourceRole::
                        SystemBarycenter)
            {
                const int32 BarycenterIndex =
                    FindConfigIndex(
                        Result,
                        CatalogEntry.CatalogKey);

                if (BarycenterIndex != INDEX_NONE)
                {
                    Result[BarycenterIndex].
                        bGravityEnabled = false;
                }
            }
        }
    }
    else if (
        TargetCatalogEntry.SourceRole
        == ETGCelestialSourceRole::SystemBarycenter)
    {
        for (const FTGCelestialCatalogEntry& CatalogEntry
             : Catalog)
        {
            if (
                CatalogEntry.SystemKey
                    == TargetCatalogEntry.SystemKey
                && CatalogEntry.SourceRole
                    == ETGCelestialSourceRole::
                        PhysicalSystemMember)
            {
                const int32 MemberIndex =
                    FindConfigIndex(
                        Result,
                        CatalogEntry.CatalogKey);

                if (MemberIndex != INDEX_NONE)
                {
                    Result[MemberIndex].
                        bGravityEnabled = false;
                }
            }
        }
    }

    return Result;
}

TArray<FTGCelestialBodyConfig>
UTGGravityEditingLibrary::
SetAutomaticActivationRadiusMeters(
    const TArray<FTGCelestialBodyConfig>& Configs,
    const FName CatalogKey,
    const double RadiusMeters)
{
    return EditConfig(
        Configs,
        CatalogKey,
        [RadiusMeters](
            FTGCelestialBodyConfig& Config)
        {
            Config.AutomaticActivationRadiusMeters =
                RadiusMeters;
        });
}

TArray<FTGCelestialBodyConfig>
UTGGravityEditingLibrary::
SetBarycenterResolutionRadiusMeters(
    const TArray<FTGCelestialBodyConfig>& Configs,
    const FName CatalogKey,
    const double RadiusMeters)
{
    return EditConfig(
        Configs,
        CatalogKey,
        [RadiusMeters](
            FTGCelestialBodyConfig& Config)
        {
            Config.BarycenterResolutionRadiusMeters =
                RadiusMeters;
        });
}

TArray<FTGCelestialBodyConfig>
UTGGravityEditingLibrary::
SetHarmonicModelCsvFilePath(
    const TArray<FTGCelestialBodyConfig>& Configs,
    const FName CatalogKey,
    const FString& CsvFilePath)
{
    return EditConfig(
        Configs,
        CatalogKey,
        [&CsvFilePath](
            FTGCelestialBodyConfig& Config)
        {
            Config.HarmonicModelCsvFilePath =
                CsvFilePath;
        });
}

TArray<FTGCelestialBodyConfig>
UTGGravityEditingLibrary::
SetMaximumHarmonicDegreeUsed(
    const TArray<FTGCelestialBodyConfig>& Configs,
    const FName CatalogKey,
    const int32 MaximumDegreeUsed)
{
    return EditConfig(
        Configs,
        CatalogKey,
        [MaximumDegreeUsed](
            FTGCelestialBodyConfig& Config)
        {
            Config.MaximumHarmonicDegreeUsed =
                MaximumDegreeUsed;
        });
}

bool UTGGravityEditingLibrary::
RequiresCompactMoonCatalogInterval(
    const TArray<FTGCelestialBodyConfig>& Configs)
{
    const TArray<FTGCelestialBodyConfig> NormalizedConfigs =
        NormalizeCelestialBodyConfigs(Configs);

    const TArray<FTGCelestialCatalogEntry> Catalog =
        UTGCelestialCatalogLibrary::GetCelestialCatalog();

    for (const FTGCelestialCatalogEntry& CatalogEntry
         : Catalog)
    {
        const int32 ConfigIndex =
            FindConfigIndex(
                NormalizedConfigs,
                CatalogEntry.CatalogKey);

        if (ConfigIndex == INDEX_NONE)
        {
            continue;
        }

        const FTGCelestialBodyConfig& Config =
            NormalizedConfigs[ConfigIndex];

        if (
            CatalogEntry.bIsPrincipalMoon
            && CanSourceBecomeActive(Config))
        {
            return true;
        }

        if (
            CatalogEntry.SourceRole
                == ETGCelestialSourceRole::
                    SystemBarycenter
            && Config.BarycenterResolutionRadiusMeters
                > 0.0
            && CanSourceBecomeActive(Config)
            && UTGCelestialCatalogLibrary::
                SystemContainsPrincipalMoon(
                    CatalogEntry.SystemKey))
        {
            return true;
        }
    }

    return false;
}
