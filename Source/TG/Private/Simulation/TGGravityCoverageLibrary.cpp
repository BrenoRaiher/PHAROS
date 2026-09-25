// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/TGGravityCoverageLibrary.h"

#include "Simulation/TGGravityEditingLibrary.h"

namespace
{
    const FDateTime& GetCompactCatalogStartUtc()
    {
        static const FDateTime Value(
            2000,
            1,
            1,
            0,
            0,
            0);

        return Value;
    }

    const FDateTime& GetCompactCatalogEndExclusiveUtc()
    {
        static const FDateTime Value(
            2050,
            1,
            1,
            0,
            0,
            0);

        return Value;
    }

    bool TryResolveEffectiveFinalUtc(
        const FTGScenarioSolverConfig& ScenarioSolver,
        FDateTime& OutFinalUtc)
    {
        switch (ScenarioSolver.EndMode)
        {
        case ETGSimulationEndMode::FinalUtc:
            OutFinalUtc = ScenarioSolver.FinalUtc;
            break;

        case ETGSimulationEndMode::Duration:
            if (!FMath::IsFinite(
                    ScenarioSolver.DurationSeconds)
                || ScenarioSolver.DurationSeconds < 0.0)
            {
                return false;
            }

            OutFinalUtc =
                ScenarioSolver.StartUtc
                + FTimespan::FromSeconds(
                    ScenarioSolver.DurationSeconds);

            break;

        case ETGSimulationEndMode::Unspecified:
        default:
            return false;
        }

        return OutFinalUtc >= ScenarioSolver.StartUtc;
    }

    FString FormatUtcForCoverage(
        const FDateTime& DateTime)
    {
        return DateTime.ToIso8601();
    }
}

FTGCompactMoonCatalogCoverageEvaluation
UTGGravityCoverageLibrary::
EvaluateCompactMoonCatalogCoverage(
    const FTGSimulationScenario& Scenario)
{
    FTGCompactMoonCatalogCoverageEvaluation Evaluation;

    Evaluation.EffectiveStartUtc =
        Scenario.ScenarioAndSolver.StartUtc;

    Evaluation.bCompactCatalogRequired =
        UTGGravityEditingLibrary::
        RequiresCompactMoonCatalogInterval(
            Scenario.CelestialBodies);

    Evaluation.bScenarioIntervalResolved =
        TryResolveEffectiveFinalUtc(
            Scenario.ScenarioAndSolver,
            Evaluation.EffectiveFinalUtc);

    /*
     * No principal-moon path means the compact interval does not
     * restrict this scenario. The scenario/solver panel remains
     * responsible for its ordinary date validation.
     */
    if (!Evaluation.bCompactCatalogRequired)
    {
        Evaluation.Status =
            ETGCompactMoonCatalogCoverageStatus::NotRequired;

        Evaluation.bCoverageSatisfied = true;
        return Evaluation;
    }

    if (!Evaluation.bScenarioIntervalResolved)
    {
        Evaluation.Status =
            ETGCompactMoonCatalogCoverageStatus::
            CannotEvaluateScenarioInterval;

        Evaluation.bCoverageSatisfied = false;
        return Evaluation;
    }

    const bool bStartsInsideCatalog =
        Evaluation.EffectiveStartUtc
        >= GetCompactCatalogStartUtc();

    /*
     * The upper boundary is exclusive. A final UTC exactly equal to
     * 2050-01-01 is therefore outside the compact catalog interval.
     */
    const bool bEndsInsideCatalog =
        Evaluation.EffectiveFinalUtc
        < GetCompactCatalogEndExclusiveUtc();

    Evaluation.bCoverageSatisfied =
        bStartsInsideCatalog
        && bEndsInsideCatalog;

    Evaluation.Status =
        Evaluation.bCoverageSatisfied
            ? ETGCompactMoonCatalogCoverageStatus::
                RequiredAndSatisfied
            : ETGCompactMoonCatalogCoverageStatus::
                RequiredButOutsideCatalog;

    return Evaluation;
}

FText UTGGravityCoverageLibrary::
FormatCompactMoonCatalogCoverageForHud(
    const FTGCompactMoonCatalogCoverageEvaluation& Evaluation)
{
    switch (Evaluation.Status)
    {
    case ETGCompactMoonCatalogCoverageStatus::NotRequired:
        return FText::FromString(
            TEXT(
                "Compact principal-moon catalog: not required "
                "by the current gravity configuration."
            ));

    case ETGCompactMoonCatalogCoverageStatus::
        CannotEvaluateScenarioInterval:
        return FText::FromString(
            TEXT(
                "Compact principal-moon catalog: required.\n"
                "Coverage cannot be evaluated because the "
                "scenario end mode or interval is incomplete."
            ));

    case ETGCompactMoonCatalogCoverageStatus::
        RequiredAndSatisfied:
        return FText::FromString(
            FString::Printf(
                TEXT(
                    "Compact principal-moon catalog: required "
                    "and satisfied.\n"
                    "Scenario interval: %s to %s\n"
                    "Available interval: "
                    "[2000-01-01, 2050-01-01)"
                ),
                *FormatUtcForCoverage(
                    Evaluation.EffectiveStartUtc),
                *FormatUtcForCoverage(
                    Evaluation.EffectiveFinalUtc)));

    case ETGCompactMoonCatalogCoverageStatus::
        RequiredButOutsideCatalog:
        return FText::FromString(
            FString::Printf(
                TEXT(
                    "Compact principal-moon catalog: required "
                    "but not satisfied.\n"
                    "Scenario interval: %s to %s\n"
                    "Required interval: "
                    "[2000-01-01, 2050-01-01)"
                ),
                *FormatUtcForCoverage(
                    Evaluation.EffectiveStartUtc),
                *FormatUtcForCoverage(
                    Evaluation.EffectiveFinalUtc)));

    default:
        return FText::FromString(
            TEXT("Compact catalog coverage status is unavailable."));
    }
}