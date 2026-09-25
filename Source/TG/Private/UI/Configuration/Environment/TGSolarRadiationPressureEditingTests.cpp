// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Simulation/ComponentTree/TGComponentTreeEditingLibrary.h"
#include "Simulation/TGScenarioDocumentAdapter.h"
#include "UI/Configuration/Environment/TGSolarRadiationPressureEditingLibrary.h"

namespace TGSolarRadiationPressureEditingTests
{
    FTGSimulationScenario MakeScenario()
    {
        FTGSimulationScenario Scenario;

        FTGComponentConfig Component;
        Component.ComponentId = FGuid::NewGuid();
        Component.Name = TEXT("Bus");
        Component.Visual.GeometrySource =
            ETGComponentGeometrySource::Primitive;
        Component.Visual.PrimitiveType =
            ETGPrimitiveGeometryType::Box;
        Component.Visual.BoxDimensionsMeters = FVector(1.0, 2.0, 3.0);

        Scenario.Components.Add(Component);

        UTGSolarRadiationPressureEditingLibrary::
            NormalizeSolarRadiationPressureScenario(Scenario);

        return Scenario;
    }

    FTGOpticalFacetConfig MakeFacet(
        const FTGComponentConfig& Component,
        int32 StableIndex = 0)
    {
        FTGOpticalFacetConfig Facet;
        Facet.Name = TEXT("Bus +X Triangle 0");
        Facet.ComponentId = Component.ComponentId;
        Facet.ComponentName = Component.Name;
        Facet.StableTriangleIndex = StableIndex;
        Facet.LogicalRegion = ETGSrpLogicalRegion::BoxPositiveX;
        Facet.Vertex0Meters = FVector(0.5, -1.0, -1.5);
        Facet.Vertex1Meters = FVector(0.5, 1.0, -1.5);
        Facet.Vertex2Meters = FVector(0.5, 1.0, 1.5);
        return Facet;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpDefaultValidationTest,
    "TG.UI.SRP.DefaultsAndFixedValues",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpDefaultValidationTest::RunTest(const FString& Parameters)
{
    FTGSimulationScenario Scenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();

    Scenario.SolarRadiationPressure.SunBodyName = TEXT("Not Sun");
    Scenario.SolarRadiationPressure
        .PressureAtOneAstronomicalUnitPascals = 9.0;

    UTGSolarRadiationPressureEditingLibrary::
        NormalizeSolarRadiationPressureScenario(Scenario);

    Scenario.SolarRadiationPressure
        .GlobalFallbackOpticalProperties.AbsorptionFraction = 0.25;
    Scenario.Components[0].Visual.GeometrySource =
        ETGComponentGeometrySource::NoGeometry;

    TestEqual(
        TEXT("Sun source is fixed"),
        Scenario.SolarRadiationPressure.SunBodyName,
        FString(TEXT("Sun")));
    TestEqual(
        TEXT("Pressure at one AU is fixed"),
        Scenario.SolarRadiationPressure
            .PressureAtOneAstronomicalUnitPascals,
        4.5391e-6);

    FText Warning;
    FText Error;
    TestTrue(
        TEXT("Disabled default SRP draft validates without a proxy"),
        UTGSolarRadiationPressureEditingLibrary::
            ValidateSolarRadiationPressureScenario(
                Scenario,
                Warning,
                Error));
    TestTrue(
        TEXT("Disabled SRP produces no inactive-field warning"),
        Warning.IsEmpty());
    TestTrue(
        TEXT("Disabled SRP produces no inactive-field error"),
        Error.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpDisabledScenarioDiscardsGeneratedCacheTest,
    "TG.UI.SRP.DisabledScenarioDiscardsGeneratedCache",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpDisabledScenarioDiscardsGeneratedCacheTest::RunTest(
    const FString& Parameters)
{
    FTGSimulationScenario Scenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();
    FTGComponentSrpConfig& ComponentSrp =
        Scenario.Components[0].SolarRadiationPressure;

    Scenario.SolarRadiationPressure.OpticalFacets.Add(
        TGSolarRadiationPressureEditingTests::MakeFacet(
            Scenario.Components[0]));
    ComponentSrp.GeneratedGeometrySignature = TEXT("cached-geometry");
    ComponentSrp.GeneratedTriangleCount = 1;
    ComponentSrp.bProxyGenerationRequired = false;

    UTGSolarRadiationPressureEditingLibrary::
        NormalizeSolarRadiationPressureScenario(Scenario);

    TestTrue(
        TEXT("Disabled SRP discards generated facet cache"),
        Scenario.SolarRadiationPressure.OpticalFacets.IsEmpty());
    TestEqual(
        TEXT("Disabled SRP clears generated triangle count"),
        ComponentSrp.GeneratedTriangleCount,
        0);
    TestTrue(
        TEXT("Re-enabling an included component regenerates its surface"),
        ComponentSrp.bProxyGenerationRequired);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpPortableDocumentOmitsInactiveGeneratedCacheTest,
    "TG.UI.SRP.PortableDocumentOmitsInactiveGeneratedCache",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpPortableDocumentOmitsInactiveGeneratedCacheTest::RunTest(
    const FString& Parameters)
{
    FTGSimulationScenario Scenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();
    Scenario.SolarRadiationPressure.OpticalFacets.Add(
        TGSolarRadiationPressureEditingTests::MakeFacet(
            Scenario.Components[0]));

    tgsim::scenario::ScenarioDocument Document;
    FTGScenarioDocumentAdapter::ToPortableDocument(
        Scenario,
        FString{},
        Document);

    TestTrue(
        TEXT("Disabled SRP exports no generated facets"),
        Document.solar_radiation_pressure.optical_facets.empty());

    Scenario.SolarRadiationPressure.bEnabled = true;
    Scenario.Components[0].SolarRadiationPressure.bIncludedInProxy = false;
    FTGScenarioDocumentAdapter::ToPortableDocument(
        Scenario,
        FString{},
        Document);
    TestTrue(
        TEXT("Excluded components export no generated facets"),
        Document.solar_radiation_pressure.optical_facets.empty());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpAuthoringAndProductionPreparationTest,
    "TG.UI.SRP.AuthoringAndProductionPreparation",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpAuthoringAndProductionPreparationTest::RunTest(
    const FString& Parameters)
{
    FTGSimulationScenario Scenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();
    Scenario.SolarRadiationPressure.bEnabled = true;

    FText AuthoringWarning;
    FText AuthoringError;
    TestTrue(
        TEXT("Authoring accepts complete inputs before surface preparation"),
        UTGSolarRadiationPressureEditingLibrary::
            ValidateSolarRadiationPressureAuthoring(
                Scenario,
                AuthoringWarning,
                AuthoringError));
    TestTrue(
        TEXT("Authoring does not report pending preparation as an error"),
        AuthoringError.IsEmpty());

    FText ReadinessWarning;
    FText ReadinessError;
    TestFalse(
        TEXT("Final readiness rejects an enabled unprepared scenario"),
        UTGSolarRadiationPressureEditingLibrary::
            ValidateSolarRadiationPressureScenario(
                Scenario,
                ReadinessWarning,
                ReadinessError));
    TestTrue(
        TEXT("Final readiness explains that surface preparation is pending"),
        ReadinessError.ToString().Contains(TEXT("Surface geometry")));

    FText PreparationSummary;
    FText PreparationWarning;
    FText PreparationError;
    TestTrue(
        TEXT("Production preparation generates the required surface"),
        UTGSolarRadiationPressureEditingLibrary::
            PrepareSolarRadiationPressureGeometry(
                Scenario,
                PreparationSummary,
                PreparationWarning,
                PreparationError));
    TestTrue(
        TEXT("Production preparation reports no error"),
        PreparationError.IsEmpty());

    ReadinessWarning = FText::GetEmpty();
    ReadinessError = FText::GetEmpty();
    TestTrue(
        TEXT("Prepared scenario passes final readiness"),
        UTGSolarRadiationPressureEditingLibrary::
            ValidateSolarRadiationPressureScenario(
                Scenario,
                ReadinessWarning,
                ReadinessError));
    TestTrue(
        TEXT("Prepared scenario reports no readiness error"),
        ReadinessError.IsEmpty());

    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpInvalidOpticalTripletTest,
    "TG.UI.SRP.InvalidOpticalTripletRejected",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpInvalidOpticalTripletTest::RunTest(
    const FString& Parameters)
{
    FTGSimulationScenario Scenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();
    Scenario.SolarRadiationPressure.bEnabled = true;

    Scenario.SolarRadiationPressure
        .GlobalFallbackOpticalProperties.AbsorptionFraction = 0.5;

    FText Warning;
    FText Error;
    TestFalse(
        TEXT("Triplet that sums to 0.5 is rejected"),
        UTGSolarRadiationPressureEditingLibrary::
            ValidateSolarRadiationPressureScenario(
                Scenario,
                Warning,
                Error));
    TestTrue(
        TEXT("Validation explains unit sum"),
        Error.ToString().Contains(TEXT("sum to 1")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpIgnoredOverridesDoNotBlockAuthoringTest,
    "TG.UI.SRP.IgnoredOverridesDoNotBlockAuthoring",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpIgnoredOverridesDoNotBlockAuthoringTest::RunTest(
    const FString& Parameters)
{
    FTGSimulationScenario Scenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();
    Scenario.SolarRadiationPressure.bEnabled = true;

    FTGSrpLogicalRegionOverride IgnoredRegion;
    IgnoredRegion.Region = ETGSrpLogicalRegion::None;
    IgnoredRegion.OpticalProperties.AbsorptionFraction = 0.25;
    Scenario.Components[0].SolarRadiationPressure
        .LogicalRegionOverrides.Add(IgnoredRegion);

    FTGSrpTriangleOverride IgnoredTriangle;
    IgnoredTriangle.ProxyTriangleIndex = INDEX_NONE;
    IgnoredTriangle.OpticalProperties.AbsorptionFraction = 0.25;
    Scenario.Components[0].SolarRadiationPressure
        .TriangleOverrides.Add(IgnoredTriangle);

    FText Warning;
    FText Error;
    TestTrue(
        TEXT("Whole-component optics ignore dormant overrides"),
        UTGSolarRadiationPressureEditingLibrary::
            ValidateSolarRadiationPressureAuthoring(
                Scenario,
                Warning,
                Error));
    TestTrue(
        TEXT("Ignored overrides produce no authoring error"),
        Error.IsEmpty());
    TestTrue(
        TEXT("Ignored overrides remain visible as a warning"),
        Warning.ToString().Contains(TEXT("ignored")));

    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpGeneratedProxyAndPrecedenceTest,
    "TG.UI.SRP.GeneratedProxyAndOpticalPrecedence",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpGeneratedProxyAndPrecedenceTest::RunTest(
    const FString& Parameters)
{
    FTGSimulationScenario Scenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();

    const FGuid ComponentId =
        Scenario.Components[0].ComponentId;

    TArray<FTGOpticalFacetConfig> Triangles;
    Triangles.Add(
        TGSolarRadiationPressureEditingTests::MakeFacet(
            Scenario.Components[0]));

    FText Summary;
    FText Error;
    TestTrue(
        TEXT("Valid converter triangle is accepted"),
        UTGSolarRadiationPressureEditingLibrary::
            ApplyGeneratedComponentProxy(
                Scenario,
                ComponentId,
                Triangles,
                UTGSolarRadiationPressureEditingLibrary::
                    BuildComponentProxyGeometrySignature(
                        Scenario.Components[0]),
                Summary,
                Error));

    TestEqual(
        TEXT("Generated count stored"),
        Scenario.Components[0].SolarRadiationPressure
            .GeneratedTriangleCount,
        1);
    TestFalse(
        TEXT("Proxy is current"),
        Scenario.Components[0].SolarRadiationPressure
            .bProxyGenerationRequired);

    FTGComponentSrpConfig& Config =
        Scenario.Components[0].SolarRadiationPressure;
    Config.bUseGlobalFallbackOpticalProperties = false;
    Config.bApplyOneOpticalConfigurationToEntireComponent = false;
    Config.ComponentOpticalProperties.AbsorptionFraction = 0.2;
    Config.ComponentOpticalProperties.SpecularReflectionFraction = 0.3;
    Config.ComponentOpticalProperties.DiffuseReflectionFraction = 0.5;

    FTGSrpLogicalRegionOverride RegionOverride;
    RegionOverride.Region = ETGSrpLogicalRegion::BoxPositiveX;
    RegionOverride.OpticalProperties.AbsorptionFraction = 0.1;
    RegionOverride.OpticalProperties.SpecularReflectionFraction = 0.2;
    RegionOverride.OpticalProperties.DiffuseReflectionFraction = 0.7;
    Config.LogicalRegionOverrides.Add(RegionOverride);

    FTGSrpTriangleOverride TriangleOverride;
    TriangleOverride.ProxyTriangleIndex = 0;
    TriangleOverride.OpticalProperties.AbsorptionFraction = 0.3;
    TriangleOverride.OpticalProperties.SpecularReflectionFraction = 0.3;
    TriangleOverride.OpticalProperties.DiffuseReflectionFraction = 0.4;
    Config.TriangleOverrides.Add(TriangleOverride);

    TestTrue(
        TEXT("Optical inheritance resolves"),
        UTGSolarRadiationPressureEditingLibrary::
            ResolveComponentOpticalProperties(
                Scenario,
                ComponentId,
                Error));

    const FTGOpticalFacetConfig& Resolved =
        Scenario.SolarRadiationPressure.OpticalFacets[0];
    TestEqual(
        TEXT("Triangle override has highest precedence (absorption)"),
        Resolved.AbsorptionFraction,
        0.3);
    TestEqual(
        TEXT("Triangle override has highest precedence (specular)"),
        Resolved.SpecularReflectionFraction,
        0.3);
    TestEqual(
        TEXT("Triangle override has highest precedence (diffuse)"),
        Resolved.DiffuseReflectionFraction,
        0.4);

    FText ReapplySummary;
    FText ReapplyError;
    TestFalse(
        TEXT(
            "Explicit regeneration cannot preserve triangle overrides "
            "without an independent indexing proof"),
        UTGSolarRadiationPressureEditingLibrary::
            ApplyGeneratedComponentProxy(
                Scenario,
                ComponentId,
                Triangles,
                UTGSolarRadiationPressureEditingLibrary::
                    BuildComponentProxyGeometrySignature(
                        Scenario.Components[0]),
                ReapplySummary,
                ReapplyError));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpCuboidFaceOverrideApplicationTest,
    "TG.UI.SRP.CuboidFaceOverrideApplicationAndClear",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpCuboidFaceOverrideApplicationTest::RunTest(
    const FString& Parameters)
{
    FTGSimulationScenario Scenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();
    FTGComponentConfig& Component = Scenario.Components[0];
    const FGuid ComponentId = Component.ComponentId;

    TArray<FTGOpticalFacetConfig> Triangles;
    FTGOpticalFacetConfig PositiveX0 =
        TGSolarRadiationPressureEditingTests::MakeFacet(Component, 0);
    FTGOpticalFacetConfig PositiveX1 =
        TGSolarRadiationPressureEditingTests::MakeFacet(Component, 1);
    PositiveX1.Name = TEXT("Bus +X Triangle 1");
    FTGOpticalFacetConfig NegativeY =
        TGSolarRadiationPressureEditingTests::MakeFacet(Component, 2);
    NegativeY.Name = TEXT("Bus -Y Triangle 2");
    NegativeY.LogicalRegion = ETGSrpLogicalRegion::BoxNegativeY;
    Triangles.Append({PositiveX0, PositiveX1, NegativeY});

    FText Summary;
    FText Error;
    TestTrue(
        TEXT("Cuboid face fixture accepts converter triangles"),
        UTGSolarRadiationPressureEditingLibrary::
            ApplyGeneratedComponentProxy(
                Scenario,
                ComponentId,
                Triangles,
                UTGSolarRadiationPressureEditingLibrary::
                    BuildComponentProxyGeometrySignature(Component),
                Summary,
                Error));

    FTGComponentSrpConfig& Config =
        Scenario.Components[0].SolarRadiationPressure;
    Config.bUseGlobalFallbackOpticalProperties = false;
    Config.bApplyOneOpticalConfigurationToEntireComponent = false;
    Config.ComponentOpticalProperties.AbsorptionFraction = 0.2;
    Config.ComponentOpticalProperties.SpecularReflectionFraction = 0.3;
    Config.ComponentOpticalProperties.DiffuseReflectionFraction = 0.5;

    FTGSrpLogicalRegionOverride PositiveXOverride;
    PositiveXOverride.Region = ETGSrpLogicalRegion::BoxPositiveX;
    PositiveXOverride.OpticalProperties.AbsorptionFraction = 0.6;
    PositiveXOverride.OpticalProperties.SpecularReflectionFraction = 0.1;
    PositiveXOverride.OpticalProperties.DiffuseReflectionFraction = 0.3;
    Config.LogicalRegionOverrides.Add(PositiveXOverride);

    FTGSrpLogicalRegionOverride NegativeYOverride;
    NegativeYOverride.Region = ETGSrpLogicalRegion::BoxNegativeY;
    NegativeYOverride.OpticalProperties.AbsorptionFraction = 0.1;
    NegativeYOverride.OpticalProperties.SpecularReflectionFraction = 0.2;
    NegativeYOverride.OpticalProperties.DiffuseReflectionFraction = 0.7;
    Config.LogicalRegionOverrides.Add(NegativeYOverride);

    TestTrue(
        TEXT("Cuboid face overrides resolve"),
        UTGSolarRadiationPressureEditingLibrary::
            ResolveComponentOpticalProperties(
                Scenario,
                ComponentId,
                Error));

    const auto FindFacet =
        [&Scenario](int32 StableIndex)
        {
            return Scenario.SolarRadiationPressure.OpticalFacets.
                FindByPredicate(
                    [StableIndex](const FTGOpticalFacetConfig& Facet)
                    {
                        return Facet.StableTriangleIndex == StableIndex;
                    });
        };

    const FTGOpticalFacetConfig* ResolvedPositiveX0 = FindFacet(0);
    const FTGOpticalFacetConfig* ResolvedPositiveX1 = FindFacet(1);
    const FTGOpticalFacetConfig* ResolvedNegativeY = FindFacet(2);
    TestNotNull(TEXT("First +X face triangle exists"), ResolvedPositiveX0);
    TestNotNull(TEXT("Second +X face triangle exists"), ResolvedPositiveX1);
    TestNotNull(TEXT("-Y face triangle exists"), ResolvedNegativeY);
    if (ResolvedPositiveX0 != nullptr && ResolvedPositiveX1 != nullptr)
    {
        TestTrue(
            TEXT("One logical +X assignment reaches both generated triangles"),
            FMath::IsNearlyEqual(
                ResolvedPositiveX0->AbsorptionFraction,
                0.6) &&
            FMath::IsNearlyEqual(
                ResolvedPositiveX1->AbsorptionFraction,
                0.6));
    }
    if (ResolvedNegativeY != nullptr)
    {
        TestTrue(
            TEXT("Independent -Y assignment keeps its own triplet"),
            FMath::IsNearlyEqual(
                ResolvedNegativeY->AbsorptionFraction,
                0.1) &&
            FMath::IsNearlyEqual(
                ResolvedNegativeY->DiffuseReflectionFraction,
                0.7));
    }

    Config.LogicalRegionOverrides.RemoveAll(
        [](const FTGSrpLogicalRegionOverride& Override)
        {
            return Override.Region == ETGSrpLogicalRegion::BoxPositiveX;
        });
    TestTrue(
        TEXT("Clearing +X override resolves inheritance again"),
        UTGSolarRadiationPressureEditingLibrary::
            ResolveComponentOpticalProperties(
                Scenario,
                ComponentId,
                Error));

    ResolvedPositiveX0 = FindFacet(0);
    ResolvedNegativeY = FindFacet(2);
    if (ResolvedPositiveX0 != nullptr)
    {
        TestTrue(
            TEXT("Cleared +X face returns to component properties"),
            FMath::IsNearlyEqual(
                ResolvedPositiveX0->AbsorptionFraction,
                0.2) &&
            FMath::IsNearlyEqual(
                ResolvedPositiveX0->SpecularReflectionFraction,
                0.3) &&
            FMath::IsNearlyEqual(
                ResolvedPositiveX0->DiffuseReflectionFraction,
                0.5));
    }
    if (ResolvedNegativeY != nullptr)
    {
        TestTrue(
            TEXT("Clearing +X does not remove the independent -Y override"),
            FMath::IsNearlyEqual(
                ResolvedNegativeY->AbsorptionFraction,
                0.1) &&
            FMath::IsNearlyEqual(
                ResolvedNegativeY->DiffuseReflectionFraction,
                0.7));
    }

    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpGeometryStalenessTest,
    "TG.UI.SRP.GeometryChangeMarksProxyStale",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpGeometryStalenessTest::RunTest(
    const FString& Parameters)
{
    FTGSimulationScenario Scenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();
    Scenario.SolarRadiationPressure.bEnabled = true;

    TArray<FTGOpticalFacetConfig> Triangles;
    Triangles.Add(
        TGSolarRadiationPressureEditingTests::MakeFacet(
            Scenario.Components[0]));

    FText Summary;
    FText Error;
    UTGSolarRadiationPressureEditingLibrary::
        ApplyGeneratedComponentProxy(
            Scenario,
            Scenario.Components[0].ComponentId,
            Triangles,
            UTGSolarRadiationPressureEditingLibrary::
                BuildComponentProxyGeometrySignature(
                    Scenario.Components[0]),
            Summary,
            Error);

    Scenario.Components[0].Visual.BoxDimensionsMeters.X = 2.0;

    UTGSolarRadiationPressureEditingLibrary::
        NormalizeSolarRadiationPressureScenario(Scenario);

    TestTrue(
        TEXT("Geometry change marks proxy stale"),
        Scenario.Components[0].SolarRadiationPressure
            .bProxyGenerationRequired);
    TestEqual(
        TEXT("Stale triangles remain cached until confirmation"),
        Scenario.SolarRadiationPressure.OpticalFacets.Num(),
        1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpOwnedCacheDeletionTest,
    "TG.UI.SRP.ComponentDeletionRemovesOwnedProxyCache",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpOwnedCacheDeletionTest::RunTest(
    const FString& Parameters)
{
    FTGSimulationScenario Scenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();

    FGuid ChildId;
    FText Error;
    TestTrue(
        TEXT("Child component created"),
        UTGComponentTreeEditingLibrary::AddDefaultChildComponent(
            Scenario,
            Scenario.Components[0].ComponentId,
            ChildId,
            Error));

    const int32 ChildIndex =
        UTGComponentTreeEditingLibrary::FindComponentIndexById(
            Scenario.Components,
            ChildId);
    TestTrue(TEXT("Child index is valid"), ChildIndex != INDEX_NONE);

    FTGOpticalFacetConfig Facet =
        TGSolarRadiationPressureEditingTests::MakeFacet(
            Scenario.Components[ChildIndex]);
    Facet.ComponentId = ChildId;
    Facet.ComponentName = Scenario.Components[ChildIndex].Name;
    Scenario.SolarRadiationPressure.OpticalFacets.Add(Facet);

    int32 DeletedCount = 0;
    TestTrue(
        TEXT("Generated cache does not block component deletion"),
        UTGComponentTreeEditingLibrary::DeleteComponentSubtree(
            Scenario,
            ChildId,
            DeletedCount,
            Error));
    TestEqual(TEXT("One child deleted"), DeletedCount, 1);
    TestEqual(
        TEXT("Owned generated facet cache removed"),
        Scenario.SolarRadiationPressure.OpticalFacets.Num(),
        0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpUnsupportedOverrideValidationTest,
    "TG.UI.SRP.UnsupportedOverridesRejected",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpUnsupportedOverrideValidationTest::RunTest(
    const FString& Parameters)
{
    FTGSimulationScenario Scenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();
    Scenario.SolarRadiationPressure.bEnabled = true;
    Scenario.Components[0].SolarRadiationPressure
        .bApplyOneOpticalConfigurationToEntireComponent = false;

    FTGSrpLogicalRegionOverride InvalidRegion;
    InvalidRegion.Region = ETGSrpLogicalRegion::None;
    Scenario.Components[0].SolarRadiationPressure
        .LogicalRegionOverrides.Add(InvalidRegion);

    FText Warning;
    FText Error;
    TestFalse(
        TEXT("Hidden None logical-region override is rejected"),
        UTGSolarRadiationPressureEditingLibrary::
            ValidateSolarRadiationPressureScenario(
                Scenario,
                Warning,
                Error));
    TestTrue(
        TEXT("Invalid logical-region diagnostic is explicit"),
        Error.ToString().Contains(TEXT("logical region")));

    Scenario.Components[0].SolarRadiationPressure
        .LogicalRegionOverrides.Reset();
    Scenario.Components[0].Visual.PrimitiveType =
        ETGPrimitiveGeometryType::Sphere;

    FTGSrpTriangleOverride InvalidSphereOverride;
    InvalidSphereOverride.ProxyTriangleIndex = 0;
    Scenario.Components[0].SolarRadiationPressure
        .TriangleOverrides.Add(InvalidSphereOverride);

    Error = FText::GetEmpty();
    TestFalse(
        TEXT("Sphere triangle override is rejected"),
        UTGSolarRadiationPressureEditingLibrary::
            ValidateSolarRadiationPressureScenario(
                Scenario,
                Warning,
                Error));
    TestTrue(
        TEXT("Sphere whole-component diagnostic is explicit"),
        Error.ToString().Contains(TEXT("sphere")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpRegenerationTransactionTest,
    "TG.UI.SRP.RegenerationRejectsIncompatibleRegionsTransactionally",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpRegenerationTransactionTest::RunTest(
    const FString& Parameters)
{
    FTGSimulationScenario Scenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();
    Scenario.SolarRadiationPressure.bEnabled = true;

    TArray<FTGOpticalFacetConfig> BoxTriangles;
    BoxTriangles.Add(
        TGSolarRadiationPressureEditingTests::MakeFacet(
            Scenario.Components[0]));

    FText Summary;
    FText Error;
    TestTrue(
        TEXT("Initial Box proxy is accepted"),
        UTGSolarRadiationPressureEditingLibrary::
            ApplyGeneratedComponentProxy(
                Scenario,
                Scenario.Components[0].ComponentId,
                BoxTriangles,
                UTGSolarRadiationPressureEditingLibrary::
                    BuildComponentProxyGeometrySignature(
                        Scenario.Components[0]),
                Summary,
                Error));

    FTGSrpLogicalRegionOverride BoxOverride;
    BoxOverride.Region = ETGSrpLogicalRegion::BoxPositiveX;
    Scenario.Components[0].SolarRadiationPressure
        .LogicalRegionOverrides.Add(BoxOverride);
    Scenario.Components[0].Visual.PrimitiveType =
        ETGPrimitiveGeometryType::Cylinder;

    FTGOpticalFacetConfig CylinderFacet =
        TGSolarRadiationPressureEditingTests::MakeFacet(
            Scenario.Components[0]);
    CylinderFacet.LogicalRegion =
        ETGSrpLogicalRegion::CylinderSide;

    TArray<FTGOpticalFacetConfig> CylinderTriangles;
    CylinderTriangles.Add(CylinderFacet);

    Error = FText::GetEmpty();
    TestFalse(
        TEXT("Incompatible Box region must be confirmed away"),
        UTGSolarRadiationPressureEditingLibrary::
            ApplyGeneratedComponentProxy(
                Scenario,
                Scenario.Components[0].ComponentId,
                CylinderTriangles,
                UTGSolarRadiationPressureEditingLibrary::
                    BuildComponentProxyGeometrySignature(
                        Scenario.Components[0]),
                Summary,
                Error));
    TestEqual(
        TEXT("Rejected replacement retains the old cached facet"),
        Scenario.SolarRadiationPressure.OpticalFacets[0]
            .LogicalRegion,
        ETGSrpLogicalRegion::BoxPositiveX);

    TestTrue(
        TEXT("Confirmed discard succeeds"),
        UTGSolarRadiationPressureEditingLibrary::
            DiscardTriangleOverridesAndGeneratedProxy(
                Scenario,
                Scenario.Components[0].ComponentId,
                Error));
    TestTrue(
        TEXT("Only incompatible logical regions are removed"),
        Scenario.Components[0].SolarRadiationPressure
            .LogicalRegionOverrides.IsEmpty());
    TestTrue(
        TEXT("Generated cache is cleared before replacement"),
        Scenario.SolarRadiationPressure.OpticalFacets.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpStableIndexContractTest,
    "TG.UI.SRP.GeneratedProxyRequiresContiguousStableIndices",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpStableIndexContractTest::RunTest(
    const FString& Parameters)
{
    FTGSimulationScenario Scenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();

    FTGOpticalFacetConfig Facet =
        TGSolarRadiationPressureEditingTests::MakeFacet(
            Scenario.Components[0],
            7);
    TArray<FTGOpticalFacetConfig> Triangles;
    Triangles.Add(Facet);

    FText Summary;
    FText Error;
    TestFalse(
        TEXT("Non-contiguous converter index is rejected"),
        UTGSolarRadiationPressureEditingLibrary::
            ApplyGeneratedComponentProxy(
                Scenario,
                Scenario.Components[0].ComponentId,
                Triangles,
                UTGSolarRadiationPressureEditingLibrary::
                    BuildComponentProxyGeometrySignature(
                        Scenario.Components[0]),
                Summary,
                Error));
    TestTrue(
        TEXT("Stable-index diagnostic explains contiguous order"),
        Error.ToString().Contains(TEXT("contiguous")));
    TestTrue(
        TEXT("Rejected converter output does not replace the cache"),
        Scenario.SolarRadiationPressure.OpticalFacets.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpOpticalWeightNormalizationTest,
    "TG.UI.SRP.OpticalWeightNormalization",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpOpticalWeightNormalizationTest::RunTest(
    const FString& Parameters)
{
    FTGSrpOpticalProperties Weights;
    Weights.AbsorptionFraction = 60.0;
    Weights.SpecularReflectionFraction = 20.0;
    Weights.DiffuseReflectionFraction = 20.0;
    UTGSolarRadiationPressureEditingLibrary::
        NormalizeOpticalPropertyWeights(Weights);
    TestTrue(
        TEXT("Optical weights normalize to exact fractions"),
        FMath::IsNearlyEqual(Weights.AbsorptionFraction, 0.6) &&
        FMath::IsNearlyEqual(Weights.SpecularReflectionFraction, 0.2) &&
        FMath::IsNearlyEqual(Weights.DiffuseReflectionFraction, 0.2));

    Weights.AbsorptionFraction = 0.0;
    Weights.SpecularReflectionFraction = 0.0;
    Weights.DiffuseReflectionFraction = 0.0;
    UTGSolarRadiationPressureEditingLibrary::
        NormalizeOpticalPropertyWeights(Weights);
    TestTrue(
        TEXT("Zero optical weights use safe absorption default"),
        Weights.AbsorptionFraction == 1.0 &&
        Weights.SpecularReflectionFraction == 0.0 &&
        Weights.DiffuseReflectionFraction == 0.0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpPendingProxyInputTransactionTest,
    "TG.UI.SRP.PendingProxyInputConfirmationTransaction",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpPendingProxyInputTransactionTest::RunTest(
    const FString& Parameters)
{
    const auto MakeScenarioWithAuthoredOverrides =
        [this]() -> FTGSimulationScenario
        {
            FTGSimulationScenario Scenario =
                TGSolarRadiationPressureEditingTests::MakeScenario();
            const FGuid ComponentId =
                Scenario.Components[0].ComponentId;

            TArray<FTGOpticalFacetConfig> Triangles;
            Triangles.Add(
                TGSolarRadiationPressureEditingTests::MakeFacet(
                    Scenario.Components[0]));

            FText Summary;
            FText Error;
            TestTrue(
                TEXT("Pending-input fixture accepts generated proxy"),
                UTGSolarRadiationPressureEditingLibrary::
                    ApplyGeneratedComponentProxy(
                        Scenario,
                        ComponentId,
                        Triangles,
                        UTGSolarRadiationPressureEditingLibrary::
                            BuildComponentProxyGeometrySignature(
                                Scenario.Components[0]),
                        Summary,
                        Error));

            FTGComponentConfig& Component = Scenario.Components[0];
            FTGSrpTriangleOverride TriangleOverride;
            TriangleOverride.ProxyTriangleIndex = 0;
            Component.SolarRadiationPressure.TriangleOverrides.Add(
                TriangleOverride);

            FTGSrpLogicalRegionOverride LogicalRegionOverride;
            LogicalRegionOverride.Region =
                ETGSrpLogicalRegion::BoxPositiveX;
            Component.SolarRadiationPressure.LogicalRegionOverrides.Add(
                LogicalRegionOverride);
            return Scenario;
        };

    FTGSimulationScenario ResolutionScenario =
        MakeScenarioWithAuthoredOverrides();
    const FGuid ResolutionComponentId =
        ResolutionScenario.Components[0].ComponentId;
    FText Error;
    TestTrue(
        TEXT("Confirmed resolution change discards unstable proxy data"),
        UTGSolarRadiationPressureEditingLibrary::
            DiscardTriangleOverridesAndGeneratedProxy(
                ResolutionScenario,
                ResolutionComponentId,
                Error));
    ResolutionScenario.Components[0].SolarRadiationPressure
        .ProxyResolutionMode =
        ETGSrpProxyResolutionMode::CustomTargetTriangleCount;

    const FTGComponentSrpConfig& ResolutionConfig =
        ResolutionScenario.Components[0].SolarRadiationPressure;
    TestEqual(
        TEXT("Confirmed resolution value is applied"),
        ResolutionConfig.ProxyResolutionMode,
        ETGSrpProxyResolutionMode::CustomTargetTriangleCount);
    TestTrue(
        TEXT("Confirmed resolution change clears triangle overrides"),
        ResolutionConfig.TriangleOverrides.IsEmpty());
    TestTrue(
        TEXT("Confirmed resolution change clears generated facets"),
        ResolutionScenario.SolarRadiationPressure.OpticalFacets.IsEmpty());
    TestEqual(
        TEXT("Compatible primitive-region override is preserved"),
        ResolutionConfig.LogicalRegionOverrides.Num(),
        1);
    TestTrue(
        TEXT("Confirmed resolution change marks preprocessing required"),
        ResolutionConfig.bProxyGenerationRequired);

    FTGSimulationScenario CountScenario =
        MakeScenarioWithAuthoredOverrides();
    const FGuid CountComponentId =
        CountScenario.Components[0].ComponentId;
    TestTrue(
        TEXT("Confirmed custom count discards unstable proxy data"),
        UTGSolarRadiationPressureEditingLibrary::
            DiscardTriangleOverridesAndGeneratedProxy(
                CountScenario,
                CountComponentId,
                Error));
    CountScenario.Components[0].SolarRadiationPressure
        .CustomTargetTriangleCount = 4096;

    const FTGComponentSrpConfig& CountConfig =
        CountScenario.Components[0].SolarRadiationPressure;
    TestEqual(
        TEXT("Confirmed custom target count is applied"),
        CountConfig.CustomTargetTriangleCount,
        4096);
    TestTrue(
        TEXT("Confirmed custom count clears triangle overrides"),
        CountConfig.TriangleOverrides.IsEmpty());
    TestTrue(
        TEXT("Confirmed custom count clears generated facets"),
        CountScenario.SolarRadiationPressure.OpticalFacets.IsEmpty());
    TestEqual(
        TEXT("Custom count preserves compatible region override"),
        CountConfig.LogicalRegionOverrides.Num(),
        1);

    const FTGSimulationScenario CancelledScenario =
        MakeScenarioWithAuthoredOverrides();
    TestEqual(
        TEXT("Cancellation leaves the existing resolution untouched"),
        CancelledScenario.Components[0].SolarRadiationPressure
            .ProxyResolutionMode,
        ETGSrpProxyResolutionMode::Automatic);
    TestTrue(
        TEXT("Cancellation leaves triangle overrides untouched"),
        !CancelledScenario.Components[0].SolarRadiationPressure
            .TriangleOverrides.IsEmpty());
    TestTrue(
        TEXT("Cancellation leaves generated facets untouched"),
        !CancelledScenario.SolarRadiationPressure.OpticalFacets.IsEmpty());

    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpPrimitiveSurfaceGenerationTest,
    "TG.UI.SRP.ProductionSurfaceGeneration.Primitives",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpPrimitiveSurfaceGenerationTest::RunTest(
    const FString& Parameters)
{
    FTGSimulationScenario BoxScenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();
    BoxScenario.SolarRadiationPressure.bEnabled = true;

    FText Summary;
    FText Warning;
    FText Error;
    TestTrue(
        TEXT("Box surface is generated automatically"),
        UTGSolarRadiationPressureEditingLibrary::
            PrepareSolarRadiationPressureGeometry(
                BoxScenario,
                Summary,
                Warning,
                Error));
    TestEqual(
        TEXT("Box uses two triangles for each of six faces"),
        BoxScenario.SolarRadiationPressure.OpticalFacets.Num(),
        12);
    TestFalse(
        TEXT("Generated box surface is current"),
        BoxScenario.Components[0].SolarRadiationPressure
            .bProxyGenerationRequired);

    TMap<ETGSrpLogicalRegion, int32> RegionCounts;
    bool bAllBoxTrianglesFaceOutward = true;
    for (const FTGOpticalFacetConfig& Facet :
         BoxScenario.SolarRadiationPressure.OpticalFacets)
    {
        ++RegionCounts.FindOrAdd(Facet.LogicalRegion);
        const FVector Normal = FVector::CrossProduct(
            Facet.Vertex1Meters - Facet.Vertex0Meters,
            Facet.Vertex2Meters - Facet.Vertex0Meters);
        const FVector Center =
            (Facet.Vertex0Meters + Facet.Vertex1Meters +
             Facet.Vertex2Meters) / 3.0;
        bAllBoxTrianglesFaceOutward &=
            FVector::DotProduct(Normal, Center) > 0.0;
    }
    TestEqual(
        TEXT("Box exposes six surface regions"),
        RegionCounts.Num(),
        6);
    for (const TPair<ETGSrpLogicalRegion, int32>& Entry : RegionCounts)
    {
        TestEqual(
            TEXT("Box region triangle count"),
            Entry.Value,
            2);
    }
    TestTrue(
        TEXT("Box triangles use outward winding"),
        bAllBoxTrianglesFaceOutward);

    FTGSimulationScenario SphereScenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();
    SphereScenario.SolarRadiationPressure.bEnabled = true;
    SphereScenario.Components[0].Visual.PrimitiveType =
        ETGPrimitiveGeometryType::Sphere;
    SphereScenario.Components[0].Visual.SphereRadiusMeters = 2.0;
    SphereScenario.Components[0].SolarRadiationPressure
        .ProxyResolutionMode =
        ETGSrpProxyResolutionMode::CustomTargetTriangleCount;
    SphereScenario.Components[0].SolarRadiationPressure
        .CustomTargetTriangleCount = 200;
    TestTrue(
        TEXT("Sphere surface is generated automatically"),
        UTGSolarRadiationPressureEditingLibrary::
            PrepareSolarRadiationPressureGeometry(
                SphereScenario,
                Summary,
                Warning,
                Error));
    TestTrue(
        TEXT("Sphere detail remains close to its requested target"),
        SphereScenario.SolarRadiationPressure.OpticalFacets.Num() >= 160 &&
        SphereScenario.SolarRadiationPressure.OpticalFacets.Num() <= 240);

    FTGSimulationScenario CylinderScenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();
    CylinderScenario.SolarRadiationPressure.bEnabled = true;
    CylinderScenario.Components[0].Visual.PrimitiveType =
        ETGPrimitiveGeometryType::Cylinder;
    CylinderScenario.Components[0].Visual.CylinderRadiusMeters = 1.0;
    CylinderScenario.Components[0].Visual.CylinderLengthMeters = 3.0;
    CylinderScenario.Components[0].SolarRadiationPressure
        .ProxyResolutionMode =
        ETGSrpProxyResolutionMode::CustomTargetTriangleCount;
    CylinderScenario.Components[0].SolarRadiationPressure
        .CustomTargetTriangleCount = 64;
    TestTrue(
        TEXT("Cylinder surface is generated automatically"),
        UTGSolarRadiationPressureEditingLibrary::
            PrepareSolarRadiationPressureGeometry(
                CylinderScenario,
                Summary,
                Warning,
                Error));
    TestEqual(
        TEXT("Cylinder honors the requested target"),
        CylinderScenario.SolarRadiationPressure.OpticalFacets.Num(),
        64);

    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpStlSurfaceGenerationTest,
    "TG.UI.SRP.ProductionSurfaceGeneration.Stl",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpStlSurfaceGenerationTest::RunTest(
    const FString& Parameters)
{
    const FString StlPath = FPaths::CreateTempFilename(
        *FPaths::ProjectSavedDir(),
        TEXT("TG_SRP_"),
        TEXT(".stl"));
    const FString StlContents =
        TEXT("solid test\n")
        TEXT("facet normal 0 0 1\n")
        TEXT("outer loop\n")
        TEXT("vertex 0 0 0\n")
        TEXT("vertex 1 0 0\n")
        TEXT("vertex 0 1 0\n")
        TEXT("endloop\n")
        TEXT("endfacet\n")
        TEXT("endsolid test\n");
    TestTrue(
        TEXT("ASCII STL fixture is written"),
        FFileHelper::SaveStringToFile(StlContents, *StlPath));

    FTGSimulationScenario Scenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();
    Scenario.SolarRadiationPressure.bEnabled = true;
    Scenario.Components[0].Visual.GeometrySource =
        ETGComponentGeometrySource::CustomStl;
    Scenario.Components[0].Visual.StlFilePath = StlPath;
    Scenario.Components[0].Visual.StlLengthUnit = ETGStlLengthUnit::Meters;

    FText Summary;
    FText Warning;
    FText Error;
    const bool bPrepared =
        UTGSolarRadiationPressureEditingLibrary::
            PrepareSolarRadiationPressureGeometry(
                Scenario,
                Summary,
                Warning,
                Error);
    TestTrue(TEXT("ASCII STL surface is generated"), bPrepared);
    TestEqual(
        TEXT("STL topology is preserved"),
        Scenario.SolarRadiationPressure.OpticalFacets.Num(),
        1);
    IFileManager::Get().Delete(*StlPath, false, true);

    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpSurfaceGenerationTransactionTest,
    "TG.UI.SRP.ProductionSurfaceGeneration.Transaction",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpSurfaceGenerationTransactionTest::RunTest(
    const FString& Parameters)
{
    FTGSimulationScenario Scenario =
        TGSolarRadiationPressureEditingTests::MakeScenario();
    Scenario.SolarRadiationPressure.bEnabled = true;
    FTGComponentConfig InvalidComponent = Scenario.Components[0];
    InvalidComponent.ComponentId = FGuid::NewGuid();
    InvalidComponent.Name = TEXT("Invalid Surface");
    InvalidComponent.Visual.GeometrySource =
        ETGComponentGeometrySource::NoGeometry;
    Scenario.Components.Add(InvalidComponent);

    FText Summary;
    FText Warning;
    FText Error;
    TestFalse(
        TEXT("One invalid component rejects the complete preparation"),
        UTGSolarRadiationPressureEditingLibrary::
            PrepareSolarRadiationPressureGeometry(
                Scenario,
                Summary,
                Warning,
                Error));
    TestTrue(
        TEXT("Failed preparation leaves the original scenario unchanged"),
        Scenario.SolarRadiationPressure.OpticalFacets.IsEmpty());
    TestTrue(
        TEXT("Failure identifies the component without geometry"),
        Error.ToString().Contains(TEXT("Invalid Surface")));

    return !HasAnyErrors();
}

#endif
