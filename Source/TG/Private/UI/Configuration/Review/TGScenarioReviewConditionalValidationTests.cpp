// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Simulation/TGGravityEditingLibrary.h"
#include "Simulation/TGScenarioDocumentAdapter.h"
#include "UI/Configuration/Environment/TGSolarRadiationPressureEditingLibrary.h"
#include "UI/Configuration/Review/TGScenarioReviewValidationLibrary.h"

#include <algorithm>

namespace TGScenarioReviewConditionalValidationTests
{
    FTGSimulationScenario MakeSolverScenario()
    {
        FTGSimulationScenario Scenario;
        FTGScenarioSolverConfig& Solver = Scenario.ScenarioAndSolver;
        Solver.ScenarioName = TEXT("Conditional validation test");
        Solver.SimulationKind = ETGSimulationKind::Spacecraft6Dof;
        Solver.StartUtc = FDateTime(2025, 1, 1);
        Solver.EndMode = ETGSimulationEndMode::Duration;
        Solver.DurationSeconds = 10.0;
        Solver.IntegratorKind = ETGIntegratorKind::FixedStepRK4;
        Solver.MaximumIntegratorStepSeconds = 1.0;
        Solver.InitialIntegratorStepSeconds = 0.0;
        Solver.AbsoluteTolerance = 0.0;
        Solver.RelativeTolerance = 0.0;
        Solver.OutputMode = ETGOutputMode::EveryIntegratorStep;
        Solver.OutputStepSeconds = 0.0;
        Solver.MaximumIntegrationSteps = 100;
        Solver.MaximumOutputSamples = 100;
        return Scenario;
    }

    bool HasCode(
        const FTGScenarioReviewReport& Report,
        const TCHAR* Code)
    {
        const FName Expected(Code);
        return Report.Issues.ContainsByPredicate(
            [Expected](const FTGScenarioReviewIssue& Issue)
            {
                return Issue.Code == Expected;
            });
    }

    bool HasSrpError(const FTGScenarioReviewReport& Report)
    {
        return Report.Issues.ContainsByPredicate(
            [](const FTGScenarioReviewIssue& Issue)
            {
                return Issue.Section
                        == ETGScenarioReviewSection::SolarRadiationPressure
                    && Issue.Severity
                        == ETGScenarioReviewSeverity::Error;
            });
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGReviewIntegratorConditionalValidationTest,
    "TG.UI.Review.Conditional.IntegratorAndOutputFields",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGReviewIntegratorConditionalValidationTest::RunTest(
    const FString& Parameters)
{
    using namespace TGScenarioReviewConditionalValidationTests;

    FTGSimulationScenario Scenario = MakeSolverScenario();
    FTGScenarioReviewReport Report =
        UTGScenarioReviewValidationLibrary::
            ValidateScenarioForAuthoringReview(nullptr, Scenario);

    TestFalse(TEXT("RK4 ignores the initial adaptive step"),
        HasCode(Report, TEXT("SOL-011")));
    TestFalse(TEXT("RK4 ignores absolute tolerance"),
        HasCode(Report, TEXT("SOL-013")));
    TestFalse(TEXT("RK4 ignores relative tolerance"),
        HasCode(Report, TEXT("SOL-014")));
    TestFalse(TEXT("Every-step output ignores output interval"),
        HasCode(Report, TEXT("SOL-016")));

    Scenario.ScenarioAndSolver.IntegratorKind =
        ETGIntegratorKind::AdaptiveDormandPrince54;
    Scenario.ScenarioAndSolver.OutputMode = ETGOutputMode::FixedInterval;
    Report = UTGScenarioReviewValidationLibrary::
        ValidateScenarioForAuthoringReview(nullptr, Scenario);

    TestTrue(TEXT("Adaptive mode requires the initial step"),
        HasCode(Report, TEXT("SOL-011")));
    TestTrue(TEXT("Adaptive mode requires absolute tolerance"),
        HasCode(Report, TEXT("SOL-013")));
    TestTrue(TEXT("Adaptive mode requires relative tolerance"),
        HasCode(Report, TEXT("SOL-014")));
    TestTrue(TEXT("Fixed-interval output requires an interval"),
        HasCode(Report, TEXT("SOL-016")));

    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGReviewSrpConditionalValidationTest,
    "TG.UI.Review.Conditional.SrpActivationAndOverrides",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGReviewSrpConditionalValidationTest::RunTest(
    const FString& Parameters)
{
    using namespace TGScenarioReviewConditionalValidationTests;

    FTGSimulationScenario Scenario = MakeSolverScenario();
    Scenario.SolarRadiationPressure.bEnabled = false;
    Scenario.SolarRadiationPressure.GlobalFallbackOpticalProperties
        .AbsorptionFraction = 0.25;

    FTGComponentConfig Component;
    Component.ComponentId = FGuid::NewGuid();
    Component.Name = TEXT("Bus");
    Component.Visual.GeometrySource =
        ETGComponentGeometrySource::NoGeometry;
    Scenario.Components.Add(Component);

    FTGScenarioReviewReport Report =
        UTGScenarioReviewValidationLibrary::
            ValidateScenarioForAuthoringReview(nullptr, Scenario);
    TestFalse(TEXT("Disabled SRP has no SRP errors"),
        HasSrpError(Report));

    Scenario.SolarRadiationPressure.bEnabled = true;
    Scenario.SolarRadiationPressure.GlobalFallbackOpticalProperties =
        FTGSrpOpticalProperties{};
    Component = Scenario.Components[0];
    Component.Visual.GeometrySource =
        ETGComponentGeometrySource::Primitive;
    Component.Visual.PrimitiveType = ETGPrimitiveGeometryType::Box;
    Component.Visual.BoxDimensionsMeters = FVector(1.0, 1.0, 1.0);
    Component.SolarRadiationPressure
        .bApplyOneOpticalConfigurationToEntireComponent = true;

    FTGSrpLogicalRegionOverride IgnoredOverride;
    IgnoredOverride.Region = ETGSrpLogicalRegion::None;
    IgnoredOverride.OpticalProperties.AbsorptionFraction = 0.25;
    Component.SolarRadiationPressure.LogicalRegionOverrides.Add(
        IgnoredOverride);
    Scenario.Components[0] = Component;

    Report = UTGScenarioReviewValidationLibrary::
        ValidateScenarioForAuthoringReview(nullptr, Scenario);
    TestFalse(TEXT("Whole-component optics ignore dormant override errors"),
        HasCode(Report, TEXT("SRP-C05"))
            || HasCode(Report, TEXT("SRP-C07")));
    TestTrue(TEXT("Ignored stored overrides remain disclosed"),
        HasCode(Report, TEXT("SRP-C13")));

    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGGravityCompactCoverageSelectionTest,
    "TG.UI.Review.Conditional.GravityCompactCoverageSelection",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGGravityCompactCoverageSelectionTest::RunTest(
    const FString& Parameters)
{
    TArray<FTGCelestialBodyConfig> Configs =
        UTGGravityEditingLibrary::MakeDefaultCelestialBodyConfigs();
    Configs = UTGGravityEditingLibrary::SetGravityEnabledWithExclusivity(
        Configs, TEXT("Moon"), true);
    TestFalse(
        TEXT("Earth's Moon uses DE442 rather than the compact moon interval"),
        UTGGravityEditingLibrary::RequiresCompactMoonCatalogInterval(Configs));

    Configs = UTGGravityEditingLibrary::MakeDefaultCelestialBodyConfigs();
    Configs = UTGGravityEditingLibrary::SetGravityEnabledWithExclusivity(
        Configs, TEXT("Phobos"), true);
    TestTrue(
        TEXT("A compact-kernel moon still requires the compact interval"),
        UTGGravityEditingLibrary::RequiresCompactMoonCatalogInterval(Configs));

    Configs = UTGGravityEditingLibrary::MakeDefaultCelestialBodyConfigs();
    Configs = UTGGravityEditingLibrary::SetGravityEnabledWithExclusivity(
        Configs, TEXT("EarthMoonBarycenter"), true);
    Configs = UTGGravityEditingLibrary::SetBarycenterResolutionRadiusMeters(
        Configs, TEXT("EarthMoonBarycenter"), 1.0);
    TestFalse(
        TEXT("Earth-Moon barycenter resolution is covered by DE442"),
        UTGGravityEditingLibrary::RequiresCompactMoonCatalogInterval(Configs));

    FTGSimulationScenario ApolloLikeScenario;
    ApolloLikeScenario.CelestialBodies =
        UTGGravityEditingLibrary::MakeDefaultCelestialBodyConfigs();
    ApolloLikeScenario.CelestialBodies =
        UTGGravityEditingLibrary::SetGravityEnabledWithExclusivity(
            ApolloLikeScenario.CelestialBodies, TEXT("Earth"), true);
    ApolloLikeScenario.CelestialBodies =
        UTGGravityEditingLibrary::SetGravityEnabledWithExclusivity(
            ApolloLikeScenario.CelestialBodies, TEXT("Moon"), true);

    tgsim::scenario::ScenarioDocument PortableDocument;
    FTGScenarioDocumentAdapter::ToPortableDocument(
        ApolloLikeScenario, FString{}, PortableDocument);
    TestEqual(
        TEXT("Only Sun, Earth-Moon barycenter, Earth, and Moon are exported"),
        static_cast<int32>(PortableDocument.celestial_bodies.size()),
        4);
    TestFalse(
        TEXT("An unchecked compact moon is omitted from SimulationRequest input"),
        PortableDocument.celestial_bodies.end() !=
            std::find_if(
                PortableDocument.celestial_bodies.begin(),
                PortableDocument.celestial_bodies.end(),
                [](const tgsim::scenario::CelestialBody& Body)
                {
                    return Body.catalog_key == "Phobos";
                }));

    ApolloLikeScenario.SolarRadiationPressure.bEnabled = true;
    ApolloLikeScenario.SolarRadiationPressure.bComputeEclipse = true;
    ApolloLikeScenario.SolarRadiationPressure.OccultingBodyNames = {
        TEXT("Earth"),
        TEXT("Moon"),
        TEXT("Venus")};
    UTGSolarRadiationPressureEditingLibrary::
        NormalizeSolarRadiationPressureScenario(ApolloLikeScenario);
    TestEqual(
        TEXT("SRP normalization preserves an imported occulter subset"),
        ApolloLikeScenario.SolarRadiationPressure.OccultingBodyNames.Num(),
        3);

    FTGScenarioDocumentAdapter::ToPortableDocument(
        ApolloLikeScenario, FString{}, PortableDocument);
    TestFalse(
        TEXT("An unrelated compact moon is not exported as an SRP occulter"),
        PortableDocument.celestial_bodies.end() !=
            std::find_if(
                PortableDocument.celestial_bodies.begin(),
                PortableDocument.celestial_bodies.end(),
                [](const tgsim::scenario::CelestialBody& Body)
                {
                    return Body.catalog_key == "Io";
                }));

    return !HasAnyErrors();
}

#endif
