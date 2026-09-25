// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/TGScenarioDocumentAdapter.h"

#include "Simulation/Control/TGControllerLibrarySubsystem.h"
#include "Simulation/TGCelestialCatalogLibrary.h"
#include "Simulation/TGGravityEditingLibrary.h"
#include "Simulation/TGSimulationAdapter.h"
#include "TGSim/Scenario/ScenarioCompiler.h"
#include "TGSim/Scenario/ScenarioDiagnostics.h"
#include "TGSim/Scenario/ScenarioFile.h"

#include <filesystem>

namespace
{
    using namespace tgsim::scenario;

    std::string ToUtf8(const FString& value)
    {
        return std::string(TCHAR_TO_UTF8(*value));
    }

    FString FromUtf8(const std::string& value)
    {
        return FString(UTF8_TO_TCHAR(value.c_str()));
    }

    tgsim::Vec3d ToPortable(const FVector& value)
    {
        return {value.X, value.Y, value.Z};
    }

    FVector FromPortable(const tgsim::Vec3d& value)
    {
        return FVector(value.x, value.y, value.z);
    }

    tgsim::Quatd ToPortable(const FQuat& value)
    {
        return {value.W, value.X, value.Y, value.Z};
    }

    FQuat FromPortable(const tgsim::Quatd& value)
    {
        return FQuat(value.x, value.y, value.z, value.w);
    }

    bool CatalogEntryMatchesReference(
        const FTGCelestialCatalogEntry& Entry,
        const FString& Reference)
    {
        return Entry.CatalogKey.ToString().Equals(
                Reference, ESearchCase::IgnoreCase) ||
            Entry.DisplayName.ToString().Equals(
                Reference, ESearchCase::IgnoreCase) ||
            Entry.SpiceTarget.Equals(
                Reference, ESearchCase::IgnoreCase);
    }

    TSet<FName> FindPortableCelestialBodyKeys(
        const FTGSimulationScenario& Scenario)
    {
        const TArray<FTGCelestialCatalogEntry> Catalog =
            UTGCelestialCatalogLibrary::GetCelestialCatalog();
        TSet<FName> RetainedKeys;

        for (const FTGCelestialBodyConfig& Config :
             Scenario.CelestialBodies)
        {
            if (Config.bGravityEnabled ||
                Config.AutomaticActivationRadiusMeters != 0.0)
            {
                RetainedKeys.Add(Config.CatalogKey);
            }
        }

        const auto RetainReference =
            [&Catalog, &RetainedKeys](const FString& Reference)
        {
            if (const FTGCelestialCatalogEntry* Entry =
                    Catalog.FindByPredicate(
                        [&Reference](
                            const FTGCelestialCatalogEntry& Candidate)
                        {
                            return CatalogEntryMatchesReference(
                                Candidate, Reference);
                        }))
            {
                RetainedKeys.Add(Entry->CatalogKey);
            }
        };

        const FTGSolarRadiationPressureConfig& Srp =
            Scenario.SolarRadiationPressure;
        if (Srp.bEnabled)
        {
            RetainReference(Srp.SunBodyName);
            if (Srp.bComputeEclipse)
            {
                if (Srp.OccultingBodyNames.IsEmpty())
                {
                    // The HUD contract for an empty list is every supported
                    // physical non-Sun body, not "no occulters."
                    for (const FTGCelestialCatalogEntry& Entry : Catalog)
                    {
                        if (Entry.SourceRole !=
                                ETGCelestialSourceRole::SystemBarycenter &&
                            Entry.CatalogKey != FName(TEXT("Sun")))
                        {
                            RetainedKeys.Add(Entry.CatalogKey);
                        }
                    }
                }
                else
                {
                    for (const FString& Occulter : Srp.OccultingBodyNames)
                        RetainReference(Occulter);
                }
            }
        }

        if (Scenario.Atmosphere.bEnabled)
        {
            RetainReference(Scenario.Atmosphere.CentralBodyName);
            if (Scenario.Atmosphere.Model ==
                ETGAtmosphereModel::CubicHarrisPriesterEarth)
            {
                RetainReference(TEXT("Sun"));
            }
        }

        TSet<FName> SystemsNeedingBarycenter;
        TSet<FName> SystemsNeedingMembers;
        for (const FTGCelestialCatalogEntry& Entry : Catalog)
        {
            if (!RetainedKeys.Contains(Entry.CatalogKey))
                continue;

            if (Entry.SourceRole ==
                ETGCelestialSourceRole::PhysicalSystemMember)
            {
                SystemsNeedingBarycenter.Add(Entry.SystemKey);
            }
            else if (Entry.SourceRole ==
                ETGCelestialSourceRole::SystemBarycenter)
            {
                const FTGCelestialBodyConfig* Config =
                    Scenario.CelestialBodies.FindByPredicate(
                        [&Entry](const FTGCelestialBodyConfig& Candidate)
                        {
                            return Candidate.CatalogKey == Entry.CatalogKey;
                        });
                if (Config != nullptr &&
                    Config->BarycenterResolutionRadiusMeters != 0.0)
                {
                    SystemsNeedingMembers.Add(Entry.SystemKey);
                }
            }
        }

        for (const FTGCelestialCatalogEntry& Entry : Catalog)
        {
            if (Entry.SourceRole ==
                    ETGCelestialSourceRole::SystemBarycenter &&
                SystemsNeedingBarycenter.Contains(Entry.SystemKey))
            {
                RetainedKeys.Add(Entry.CatalogKey);
            }
            if (Entry.SourceRole ==
                    ETGCelestialSourceRole::PhysicalSystemMember &&
                SystemsNeedingMembers.Contains(Entry.SystemKey))
            {
                RetainedKeys.Add(Entry.CatalogKey);
            }
        }

        return RetainedKeys;
    }

    Color ToPortable(const FLinearColor& value)
    {
        return {value.R, value.G, value.B, value.A};
    }

    FLinearColor FromPortable(const Color& value)
    {
        return FLinearColor(
            static_cast<float>(value.r),
            static_cast<float>(value.g),
            static_cast<float>(value.b),
            static_cast<float>(value.a));
    }

    std::string GuidString(const FGuid& value)
    {
        return value.IsValid()
            ? ToUtf8(value.ToString(EGuidFormats::DigitsWithHyphensLower))
            : std::string{};
    }

    FGuid ParseGuidOrCreate(const std::string& value)
    {
        FGuid result;
        if (!value.empty() && FGuid::Parse(FromUtf8(value), result)) return result;
        return FGuid::NewGuid();
    }

    std::string ToUtcString(const FDateTime& value)
    {
        return value.GetTicks() > 0 ? ToUtf8(value.ToIso8601()) : std::string{};
    }

    FDateTime FromUtcString(const std::string& value)
    {
        FDateTime result;
        if (!value.empty()) FDateTime::ParseIso8601(*FromUtf8(value), result);
        return result;
    }

    std::string PathToUtf8(const std::filesystem::path& value)
    {
#if PLATFORM_WINDOWS
        return ToUtf8(FString(value.c_str()));
#else
        const auto encoded = value.u8string();
        return std::string(
            reinterpret_cast<const char*>(encoded.data()),
            encoded.size());
#endif
    }

    std::filesystem::path FStringToPath(const FString& value)
    {
#if PLATFORM_WINDOWS
        return std::filesystem::path(std::wstring(*value));
#else
        return std::filesystem::u8path(ToUtf8(value));
#endif
    }

    std::string ResolveForUnreal(
        const std::string& authored_path,
        const FString& scenario_file_path)
    {
        if (authored_path.empty()) return {};
        const std::filesystem::path source = FStringToPath(scenario_file_path);
        return PathToUtf8(ResolveReferencedPath(source, authored_path));
    }

    OpticalProperties ToPortable(const FTGSrpOpticalProperties& value)
    {
        return {
            value.AbsorptionFraction,
            value.SpecularReflectionFraction,
            value.DiffuseReflectionFraction};
    }

    FTGSrpOpticalProperties FromPortable(const OpticalProperties& value)
    {
        FTGSrpOpticalProperties result;
        result.AbsorptionFraction = value.absorption;
        result.SpecularReflectionFraction = value.specular_reflection;
        result.DiffuseReflectionFraction = value.diffuse_reflection;
        return result;
    }

    SrpLogicalRegion ToPortable(const ETGSrpLogicalRegion value)
    {
        switch (value)
        {
            case ETGSrpLogicalRegion::BoxPositiveX: return SrpLogicalRegion::PositiveX;
            case ETGSrpLogicalRegion::BoxNegativeX: return SrpLogicalRegion::NegativeX;
            case ETGSrpLogicalRegion::BoxPositiveY: return SrpLogicalRegion::PositiveY;
            case ETGSrpLogicalRegion::BoxNegativeY: return SrpLogicalRegion::NegativeY;
            case ETGSrpLogicalRegion::BoxPositiveZ: return SrpLogicalRegion::PositiveZ;
            case ETGSrpLogicalRegion::BoxNegativeZ: return SrpLogicalRegion::NegativeZ;
            case ETGSrpLogicalRegion::CylinderSide: return SrpLogicalRegion::CylinderSide;
            case ETGSrpLogicalRegion::CylinderPositiveCap:
                return SrpLogicalRegion::CylinderPositiveCap;
            case ETGSrpLogicalRegion::CylinderNegativeCap:
                return SrpLogicalRegion::CylinderNegativeCap;
            case ETGSrpLogicalRegion::None:
            default: return SrpLogicalRegion::None;
        }
    }

    ETGSrpLogicalRegion FromPortable(const SrpLogicalRegion value)
    {
        switch (value)
        {
            case SrpLogicalRegion::PositiveX: return ETGSrpLogicalRegion::BoxPositiveX;
            case SrpLogicalRegion::NegativeX: return ETGSrpLogicalRegion::BoxNegativeX;
            case SrpLogicalRegion::PositiveY: return ETGSrpLogicalRegion::BoxPositiveY;
            case SrpLogicalRegion::NegativeY: return ETGSrpLogicalRegion::BoxNegativeY;
            case SrpLogicalRegion::PositiveZ: return ETGSrpLogicalRegion::BoxPositiveZ;
            case SrpLogicalRegion::NegativeZ: return ETGSrpLogicalRegion::BoxNegativeZ;
            case SrpLogicalRegion::CylinderSide: return ETGSrpLogicalRegion::CylinderSide;
            case SrpLogicalRegion::CylinderPositiveCap:
                return ETGSrpLogicalRegion::CylinderPositiveCap;
            case SrpLogicalRegion::CylinderNegativeCap:
                return ETGSrpLogicalRegion::CylinderNegativeCap;
            case SrpLogicalRegion::None:
            default: return ETGSrpLogicalRegion::None;
        }
    }

    ScalarProfile ToPortable(const FTGScalarProfileConfig& value)
    {
        ScalarProfile result;
        result.source = value.Source == ETGScalarProfileSource::CsvProfile
            ? ScalarProfileSource::Csv
            : ScalarProfileSource::Constant;
        result.constant_value = value.ConstantValue;
        result.csv_file_path = ToUtf8(value.CsvFilePath);
        return result;
    }

    FTGScalarProfileConfig FromPortable(
        const ScalarProfile& value,
        const FString& source_file_path)
    {
        FTGScalarProfileConfig result;
        result.Source = value.source == ScalarProfileSource::Csv
            ? ETGScalarProfileSource::CsvProfile
            : ETGScalarProfileSource::Constant;
        result.ConstantValue = value.constant_value;
        result.CsvFilePath = FromUtf8(
            ResolveForUnreal(value.csv_file_path, source_file_path));
        return result;
    }

    FTGComponentVisualConfig FromPortable(
        const ComponentVisual& value,
        const FString& source_file_path)
    {
        FTGComponentVisualConfig result;
        switch (value.geometry_source)
        {
            case GeometrySource::None:
                result.GeometrySource = ETGComponentGeometrySource::NoGeometry;
                break;
            case GeometrySource::CustomStl:
                result.GeometrySource = ETGComponentGeometrySource::CustomStl;
                break;
            case GeometrySource::Primitive:
            default:
                result.GeometrySource = ETGComponentGeometrySource::Primitive;
                break;
        }
        switch (value.primitive_type)
        {
            case PrimitiveGeometry::Sphere:
                result.PrimitiveType = ETGPrimitiveGeometryType::Sphere;
                break;
            case PrimitiveGeometry::Cylinder:
                result.PrimitiveType = ETGPrimitiveGeometryType::Cylinder;
                break;
            case PrimitiveGeometry::Box:
            default:
                result.PrimitiveType = ETGPrimitiveGeometryType::Box;
                break;
        }
        result.BoxDimensionsMeters = FromPortable(value.box_dimensions_m);
        result.SphereRadiusMeters = value.sphere_radius_m;
        result.CylinderRadiusMeters = value.cylinder_radius_m;
        result.CylinderLengthMeters = value.cylinder_length_m;
        result.StlFilePath = FromUtf8(
            ResolveForUnreal(value.stl_file_path, source_file_path));
        switch (value.stl_length_unit)
        {
            case StlLengthUnit::Centimeters:
                result.StlLengthUnit = ETGStlLengthUnit::Centimeters;
                break;
            case StlLengthUnit::Meters:
                result.StlLengthUnit = ETGStlLengthUnit::Meters;
                break;
            case StlLengthUnit::Millimeters:
            default:
                result.StlLengthUnit = ETGStlLengthUnit::Millimeters;
                break;
        }
        switch (value.stl_recenter_mode)
        {
            case StlRecenterMode::CenterOnBounds:
                result.StlRecenterMode = ETGStlRecenterMode::CenterOnBounds;
                break;
            case StlRecenterMode::PlaceBaseAtOrigin:
                result.StlRecenterMode = ETGStlRecenterMode::PlaceBaseAtOrigin;
                break;
            case StlRecenterMode::KeepImportedOrigin:
            default:
                result.StlRecenterMode = ETGStlRecenterMode::KeepImportedOrigin;
                break;
        }
        result.VisualOffsetMeters = FromPortable(value.visual_offset_m);
        result.VisualOrientation = FromPortable(value.visual_orientation);
        result.VisualScale = FromPortable(value.visual_scale);
        result.SurfaceAppearanceMode = value.surface_appearance == SurfaceAppearance::Textured
            ? ETGComponentSurfaceAppearanceMode::Textured
            : ETGComponentSurfaceAppearanceMode::SolidColor;
        result.DisplayColor = FromPortable(value.display_color);
        result.BaseColorTint = FromPortable(value.base_color_tint);
        result.BaseColorTextureFilePath = FromUtf8(
            ResolveForUnreal(value.base_color_texture_file_path, source_file_path));
        result.NormalTextureFilePath = FromUtf8(
            ResolveForUnreal(value.normal_texture_file_path, source_file_path));
        result.RoughnessTextureFilePath = FromUtf8(
            ResolveForUnreal(value.roughness_texture_file_path, source_file_path));
        result.MetallicTextureFilePath = FromUtf8(
            ResolveForUnreal(value.metallic_texture_file_path, source_file_path));
        result.bVisible = value.visible;
        return result;
    }

    ComponentVisual ToPortable(const FTGComponentVisualConfig& value)
    {
        ComponentVisual result;
        switch (value.GeometrySource)
        {
            case ETGComponentGeometrySource::NoGeometry:
                result.geometry_source = GeometrySource::None;
                break;
            case ETGComponentGeometrySource::CustomStl:
                result.geometry_source = GeometrySource::CustomStl;
                break;
            case ETGComponentGeometrySource::Primitive:
            default:
                result.geometry_source = GeometrySource::Primitive;
                break;
        }
        switch (value.PrimitiveType)
        {
            case ETGPrimitiveGeometryType::Sphere:
                result.primitive_type = PrimitiveGeometry::Sphere;
                break;
            case ETGPrimitiveGeometryType::Cylinder:
                result.primitive_type = PrimitiveGeometry::Cylinder;
                break;
            case ETGPrimitiveGeometryType::Box:
            default:
                result.primitive_type = PrimitiveGeometry::Box;
                break;
        }
        result.box_dimensions_m = ToPortable(value.BoxDimensionsMeters);
        result.sphere_radius_m = value.SphereRadiusMeters;
        result.cylinder_radius_m = value.CylinderRadiusMeters;
        result.cylinder_length_m = value.CylinderLengthMeters;
        result.stl_file_path = ToUtf8(value.StlFilePath);
        switch (value.StlLengthUnit)
        {
            case ETGStlLengthUnit::Centimeters:
                result.stl_length_unit = StlLengthUnit::Centimeters;
                break;
            case ETGStlLengthUnit::Meters:
                result.stl_length_unit = StlLengthUnit::Meters;
                break;
            case ETGStlLengthUnit::Millimeters:
            default:
                result.stl_length_unit = StlLengthUnit::Millimeters;
                break;
        }
        switch (value.StlRecenterMode)
        {
            case ETGStlRecenterMode::CenterOnBounds:
                result.stl_recenter_mode = StlRecenterMode::CenterOnBounds;
                break;
            case ETGStlRecenterMode::PlaceBaseAtOrigin:
                result.stl_recenter_mode = StlRecenterMode::PlaceBaseAtOrigin;
                break;
            case ETGStlRecenterMode::KeepImportedOrigin:
            default:
                result.stl_recenter_mode = StlRecenterMode::KeepImportedOrigin;
                break;
        }
        result.visual_offset_m = ToPortable(value.VisualOffsetMeters);
        result.visual_orientation = ToPortable(value.VisualOrientation);
        result.visual_scale = ToPortable(value.VisualScale);
        result.surface_appearance =
            value.SurfaceAppearanceMode == ETGComponentSurfaceAppearanceMode::Textured
                ? SurfaceAppearance::Textured
                : SurfaceAppearance::SolidColor;
        result.display_color = ToPortable(value.DisplayColor);
        result.base_color_tint = ToPortable(value.BaseColorTint);
        result.base_color_texture_file_path = ToUtf8(value.BaseColorTextureFilePath);
        result.normal_texture_file_path = ToUtf8(value.NormalTextureFilePath);
        result.roughness_texture_file_path = ToUtf8(value.RoughnessTextureFilePath);
        result.metallic_texture_file_path = ToUtf8(value.MetallicTextureFilePath);
        result.visible = value.bVisible;
        return result;
    }
}

void FTGScenarioDocumentAdapter::ToPortableDocument(
    const FTGSimulationScenario& Scenario,
    const FString& ResolvedControllerDllPath,
    tgsim::scenario::ScenarioDocument& OutDocument)
{
    using namespace tgsim::scenario;
    OutDocument = ScenarioDocument{};
    OutDocument.generator = "PHAROS";

    const FTGScenarioSolverConfig& solver = Scenario.ScenarioAndSolver;
    OutDocument.scenario.name = ToUtf8(solver.ScenarioName);
    OutDocument.scenario.start_utc = ToUtcString(solver.StartUtc);
    OutDocument.scenario.end_mode = solver.EndMode == ETGSimulationEndMode::FinalUtc
        ? EndMode::FinalUtc
        : EndMode::Duration;
    OutDocument.scenario.final_utc = ToUtcString(solver.FinalUtc);
    OutDocument.scenario.duration_seconds = solver.DurationSeconds;
    OutDocument.scenario.integrator =
        solver.IntegratorKind == ETGIntegratorKind::AdaptiveDormandPrince54
            ? tgsim::IntegratorKind::AdaptiveDormandPrince54
            : tgsim::IntegratorKind::FixedStepRK4;
    OutDocument.scenario.maximum_integrator_step_seconds =
        solver.MaximumIntegratorStepSeconds;
    OutDocument.scenario.initial_integrator_step_seconds =
        solver.InitialIntegratorStepSeconds;
    OutDocument.scenario.absolute_tolerance = solver.AbsoluteTolerance;
    OutDocument.scenario.relative_tolerance = solver.RelativeTolerance;
    OutDocument.scenario.output_mode =
        solver.OutputMode == ETGOutputMode::FixedInterval
            ? tgsim::OutputMode::FixedInterval
            : tgsim::OutputMode::EveryIntegratorStep;
    OutDocument.scenario.output_step_seconds = solver.OutputStepSeconds;
    OutDocument.scenario.maximum_integration_steps =
        solver.MaximumIntegrationSteps > 0
            ? static_cast<std::size_t>(solver.MaximumIntegrationSteps)
            : 0;
    OutDocument.scenario.maximum_output_samples =
        solver.MaximumOutputSamples > 0
            ? static_cast<std::size_t>(solver.MaximumOutputSamples)
            : 0;
    OutDocument.scenario.maximum_wall_clock_runtime_seconds =
        solver.MaximumWallClockRuntimeSeconds;
    OutDocument.scenario.mass_flow_convention =
        tgsim::MassFlowConvention::ThrustIncludesExhaustMomentum;

    OutDocument.initial_state.position_icrf_m =
        ToPortable(Scenario.InitialState.PositionMeters);
    OutDocument.initial_state.velocity_icrf_mps =
        ToPortable(Scenario.InitialState.VelocityMetersPerSecond);
    OutDocument.initial_state.attitude_body_to_icrf =
        ToPortable(Scenario.InitialState.AttitudeBodyToIcrf);
    OutDocument.initial_state.angular_velocity_body_radps =
        ToPortable(Scenario.InitialState.AngularVelocityBodyRadiansPerSecond);
    OutDocument.initial_state.authoring_frame =
        Scenario.InitialState.AuthoringFrameCatalogKey.IsNone()
            ? std::string{}
            : ToUtf8(
                Scenario.InitialState.AuthoringFrameCatalogKey.ToString());

    const bool bPersistGeneratedSrpGeometry =
        Scenario.SolarRadiationPressure.bEnabled;

    for (const FTGComponentConfig& source : Scenario.Components)
    {
        Component target;
        target.id = GuidString(source.ComponentId);
        target.name = ToUtf8(source.Name);
        target.initial_mass_kg = source.InitialMassKilograms;
        target.minimum_mass_kg = source.MinimumMassKilograms;
        target.variable_mass = source.bVariableMass;
        target.local_center_of_mass_m = ToPortable(source.LocalCenterOfMassMeters);
        target.centroidal_inertia = {
            source.CentroidalInertia.IxxKilogramMetersSquared,
            source.CentroidalInertia.IyyKilogramMetersSquared,
            source.CentroidalInertia.IzzKilogramMetersSquared,
            source.CentroidalInertia.IxyKilogramMetersSquared,
            source.CentroidalInertia.IxzKilogramMetersSquared,
            source.CentroidalInertia.IyzKilogramMetersSquared};
        target.origin_body_m = ToPortable(source.OriginInBodyMeters);
        target.component_to_body = ToPortable(source.ComponentToBodyOrientation);
        target.parent_component_name = ToUtf8(source.ParentComponentName);
        target.parent_anchor_m = ToPortable(source.ParentAnchorMeters);
        target.child_anchor_m = ToPortable(source.ChildAnchorMeters);
        target.child_to_parent_zero_orientation =
            ToPortable(source.ChildToParentZeroOrientation);
        for (const FTGJointDofConfig& source_dof : source.DegreesOfFreedom)
        {
            JointDof target_dof;
            target_dof.id = GuidString(source_dof.DofId);
            target_dof.name = ToUtf8(source_dof.Name);
            target_dof.motion = source_dof.MotionType == ETGJointMotionType::Translation
                ? JointMotion::Translation
                : JointMotion::Rotation;
            target_dof.axis = ToPortable(source_dof.Axis);
            target_dof.initial_coordinate = source_dof.InitialCoordinate;
            target_dof.initial_rate = source_dof.InitialRate;
            if (source_dof.bHasMinimumCoordinate)
                target_dof.minimum_coordinate = source_dof.MinimumCoordinate;
            if (source_dof.bHasMaximumCoordinate)
                target_dof.maximum_coordinate = source_dof.MaximumCoordinate;
            target_dof.maximum_absolute_rate = source_dof.MaximumAbsoluteRate;
            target_dof.maximum_absolute_effort = source_dof.MaximumAbsoluteEffort;
            target.degrees_of_freedom.push_back(std::move(target_dof));
        }
        target.visual = ToPortable(source.Visual);
        target.srp.included_in_proxy = source.SolarRadiationPressure.bIncludedInProxy;
        target.srp.proxy_resolution =
            source.SolarRadiationPressure.ProxyResolutionMode ==
                    ETGSrpProxyResolutionMode::CustomTargetTriangleCount
                ? SrpProxyResolution::Custom
                : SrpProxyResolution::Automatic;
        target.srp.custom_target_triangle_count =
            source.SolarRadiationPressure.CustomTargetTriangleCount;
        target.srp.use_global_fallback_optical_properties =
            source.SolarRadiationPressure.bUseGlobalFallbackOpticalProperties;
        target.srp.apply_one_optical_configuration_to_entire_component =
            source.SolarRadiationPressure.
                bApplyOneOpticalConfigurationToEntireComponent;
        target.srp.component_optical_properties =
            ToPortable(source.SolarRadiationPressure.ComponentOpticalProperties);
        for (const FTGSrpLogicalRegionOverride& source_override :
             source.SolarRadiationPressure.LogicalRegionOverrides)
        {
            target.srp.logical_region_overrides.push_back({
                ToPortable(source_override.Region),
                ToPortable(source_override.OpticalProperties)});
        }
        for (const FTGSrpTriangleOverride& source_override :
             source.SolarRadiationPressure.TriangleOverrides)
        {
            target.srp.triangle_overrides.push_back({
                source_override.ProxyTriangleIndex,
                ToPortable(source_override.OpticalProperties)});
        }
        const bool bComponentHasPersistedSrpGeometry =
            bPersistGeneratedSrpGeometry &&
            source.SolarRadiationPressure.bIncludedInProxy;
        target.srp.generated_geometry_signature =
            bComponentHasPersistedSrpGeometry
                ? ToUtf8(
                    source.SolarRadiationPressure.GeneratedGeometrySignature)
                : std::string{};
        target.srp.generated_triangle_count =
            bComponentHasPersistedSrpGeometry
                ? source.SolarRadiationPressure.GeneratedTriangleCount
                : 0;
        target.srp.proxy_generation_required =
            bComponentHasPersistedSrpGeometry
                ? source.SolarRadiationPressure.bProxyGenerationRequired
                : source.SolarRadiationPressure.bIncludedInProxy;
        target.srp.last_proxy_generation_message =
            bComponentHasPersistedSrpGeometry
                ? ToUtf8(
                    source.SolarRadiationPressure.LastProxyGenerationMessage)
                : source.SolarRadiationPressure.bIncludedInProxy
                    ? "Surface geometry will be prepared automatically."
                    : "Component is excluded from the SRP proxy.";
        OutDocument.components.push_back(std::move(target));
    }

    for (const FTGThrusterConfig& source : Scenario.Thrusters)
    {
        Thruster target;
        target.name = ToUtf8(source.Name);
        target.mode = source.Mode == ETGThrusterMode::Commanded
            ? tgsim::ThrusterMode::Commanded
            : tgsim::ThrusterMode::PrescribedProfile;
        target.mount_component_name = ToUtf8(source.MountComponentName);
        target.propellant_component_name = ToUtf8(source.PropellantComponentName);
        target.application_point_component_m = ToPortable(source.ApplicationPointMeters);
        target.direction_component = ToPortable(source.Direction);
        target.ignition_time_mode =
            source.IgnitionTimeMode == ETGThrusterTimeMode::AbsoluteUtc
                ? ThrusterTimeMode::Utc
                : ThrusterTimeMode::Elapsed;
        target.ignition_utc = ToUtcString(source.IgnitionUtc);
        target.ignition_elapsed_seconds = source.IgnitionElapsedSeconds;
        target.never_shuts_down = source.bNeverShutsDown;
        target.shutdown_time_mode =
            source.ShutdownTimeMode == ETGThrusterTimeMode::AbsoluteUtc
                ? ThrusterTimeMode::Utc
                : ThrusterTimeMode::Elapsed;
        target.shutdown_utc = ToUtcString(source.ShutdownUtc);
        target.shutdown_elapsed_seconds = source.ShutdownElapsedSeconds;
        target.prescribed_thrust = ToPortable(source.PrescribedThrust);
        target.prescribed_specific_impulse =
            ToPortable(source.PrescribedSpecificImpulse);
        target.maximum_thrust_n = source.MaximumThrustNewtons;
        OutDocument.thrusters.push_back(std::move(target));
    }

    for (const FTGReactionWheelConfig& source : Scenario.ReactionWheels)
    {
        OutDocument.reaction_wheels.push_back({
            ToUtf8(source.Name),
            ToUtf8(source.MountComponentName),
            ToPortable(source.Axis),
            source.InitialMomentumNewtonMeterSeconds,
            source.MaximumAbsoluteMomentumNewtonMeterSeconds});
    }

    OutDocument.control.mode =
        Scenario.Control.Mode == ETGControlMode::CompiledUserController
            ? ControlMode::CompiledUserController
            : ControlMode::None;
    OutDocument.control.unreal_controller_id =
        Scenario.Control.ControllerId.IsNone()
            ? std::string{}
            : ToUtf8(Scenario.Control.ControllerId.ToString());
    OutDocument.control.controller_dll_path = ToUtf8(
        ResolvedControllerDllPath.IsEmpty()
            ? Scenario.Control.StandaloneControllerDllFilePath
            : ResolvedControllerDllPath);
    OutDocument.include_first_post_newtonian_correction =
        Scenario.GravitySettings.bIncludeFirstPostNewtonianCorrection;

    const TSet<FName> PortableCelestialBodyKeys =
        FindPortableCelestialBodyKeys(Scenario);
    for (const FTGCelestialBodyConfig& source : Scenario.CelestialBodies)
    {
        if (!PortableCelestialBodyKeys.Contains(source.CatalogKey))
            continue;

        OutDocument.celestial_bodies.push_back({
            ToUtf8(source.CatalogKey.ToString()),
            source.bGravityEnabled,
            source.AutomaticActivationRadiusMeters,
            source.BarycenterResolutionRadiusMeters,
            ToUtf8(source.HarmonicModelCsvFilePath),
            source.MaximumHarmonicDegreeUsed});
    }

    const FTGSolarRadiationPressureConfig& srp = Scenario.SolarRadiationPressure;
    OutDocument.solar_radiation_pressure.enabled = srp.bEnabled;
    OutDocument.solar_radiation_pressure.sun_body_name = ToUtf8(srp.SunBodyName);
    OutDocument.solar_radiation_pressure.pressure_at_one_au_pa =
        srp.PressureAtOneAstronomicalUnitPascals;
    OutDocument.solar_radiation_pressure.compute_eclipse = srp.bComputeEclipse;
    for (const FString& name : srp.OccultingBodyNames)
        OutDocument.solar_radiation_pressure.occulting_body_names.push_back(ToUtf8(name));
    OutDocument.solar_radiation_pressure.compute_component_shadows =
        srp.bComputeComponentShadows;
    OutDocument.solar_radiation_pressure.global_fallback_optical_properties =
        ToPortable(srp.GlobalFallbackOpticalProperties);
    TSet<FGuid> IncludedSrpComponentIds;
    if (srp.bEnabled)
    {
        for (const FTGComponentConfig& component : Scenario.Components)
        {
            if (component.SolarRadiationPressure.bIncludedInProxy)
            {
                IncludedSrpComponentIds.Add(component.ComponentId);
            }
        }
    }
    for (const FTGOpticalFacetConfig& source : srp.OpticalFacets)
    {
        if (!IncludedSrpComponentIds.Contains(source.ComponentId))
        {
            continue;
        }

        OpticalFacet target;
        target.name = ToUtf8(source.Name);
        target.component_id = GuidString(source.ComponentId);
        target.component_name = ToUtf8(source.ComponentName);
        target.stable_triangle_index = source.StableTriangleIndex;
        target.logical_region = ToPortable(source.LogicalRegion);
        target.vertex0_component_m = ToPortable(source.Vertex0Meters);
        target.vertex1_component_m = ToPortable(source.Vertex1Meters);
        target.vertex2_component_m = ToPortable(source.Vertex2Meters);
        target.optical_properties = {
            source.AbsorptionFraction,
            source.SpecularReflectionFraction,
            source.DiffuseReflectionFraction};
        OutDocument.solar_radiation_pressure.optical_facets.push_back(
            std::move(target));
    }

    OutDocument.atmosphere.enabled = Scenario.Atmosphere.bEnabled;
    OutDocument.atmosphere.central_body_name =
        ToUtf8(Scenario.Atmosphere.CentralBodyName);
    OutDocument.atmosphere.model =
        Scenario.Atmosphere.Model == ETGAtmosphereModel::CubicHarrisPriesterEarth
            ? AtmosphereModel::CubicHarrisPriesterEarth
            : AtmosphereModel::UploadedProfile;
    OutDocument.atmosphere.general_profile_csv_path =
        ToUtf8(Scenario.Atmosphere.GeneralProfileCsvPath);
    OutDocument.atmosphere.centered_average_f107_sfu =
        Scenario.Atmosphere.CenteredAverageF107SolarFluxUnits;
    OutDocument.atmosphere.chp_coefficient_csv_path =
        ToUtf8(Scenario.Atmosphere.ChpCoefficientCsvPath);
    OutDocument.atmosphere.chp_molecular_profile_csv_path =
        ToUtf8(Scenario.Atmosphere.ChpMolecularProfileCsvPath);

    const FTGAerodynamicsConfig& aero = Scenario.Aerodynamics;
    OutDocument.aerodynamics.enabled = aero.bEnabled;
    OutDocument.aerodynamics.reference_area_m2 = aero.ReferenceAreaSquareMeters;
    OutDocument.aerodynamics.reference_length_m = aero.ReferenceLengthMeters;
    OutDocument.aerodynamics.minimum_dynamic_pressure_pa =
        aero.MinimumDynamicPressurePascals;
    OutDocument.aerodynamics.maximum_valid_dynamic_pressure_pa =
        aero.MaximumValidDynamicPressurePascals;
    OutDocument.aerodynamics.constant_drag_fallback_enabled =
        aero.bEnableConstantDragFallback;
    OutDocument.aerodynamics.fallback_drag_coefficient =
        aero.FallbackDragCoefficient;
    OutDocument.aerodynamics.database.enabled = aero.Database.bEnabled;
    OutDocument.aerodynamics.database.csv_file_path = ToUtf8(aero.Database.CsvFilePath);
    OutDocument.aerodynamics.database.interpolation =
        aero.Database.Interpolation == ETGAerodynamicDatabaseInterpolation::NearestRow
            ? AerodynamicInterpolation::NearestRow
            : AerodynamicInterpolation::InverseDistance;
    OutDocument.aerodynamics.database.extrapolation =
        aero.Database.Extrapolation == ETGAerodynamicDatabaseExtrapolation::NearestRow
            ? AerodynamicExtrapolation::NearestRow
            : AerodynamicExtrapolation::ConstantDragFallback;
    OutDocument.aerodynamics.database.neighbor_count =
        aero.Database.NeighborCount > 0
            ? static_cast<std::size_t>(aero.Database.NeighborCount)
            : 0;
    OutDocument.aerodynamics.database.inverse_distance_power =
        aero.Database.InverseDistancePower;
    if (aero.Database.bUseMaximumNormalizedNeighborDistance)
        OutDocument.aerodynamics.database.maximum_normalized_neighbor_distance =
            aero.Database.MaximumNormalizedNeighborDistance;
    OutDocument.aerodynamics.database.moment_reference_center_body_m =
        ToPortable(aero.Database.MomentReferenceCenterBodyMeters);
    for (const FTGAerodynamicDatabaseRow& source : aero.Database.Rows)
    {
        AerodynamicDatabaseRow target;
        target.speed_ratio = source.SpeedRatio;
        target.knudsen_number = source.KnudsenNumber;
        target.gas_flow_direction_body = ToPortable(source.GasFlowDirectionBody);
        for (const double coordinate : source.ArticulationCoordinates)
            target.articulation_coordinates.push_back(coordinate);
        target.force_coefficients_body = ToPortable(source.BodyForceCoefficients);
        target.moment_coefficients_body = ToPortable(source.BodyMomentCoefficients);
        OutDocument.aerodynamics.database.rows.push_back(std::move(target));
    }
}

bool FTGScenarioDocumentAdapter::FromPortableDocument(
    const tgsim::scenario::ScenarioDocument& Document,
    const FString& SourceScenarioFilePath,
    FTGSimulationScenario& OutScenario,
    FString& OutMessage)
{
    using namespace tgsim::scenario;
    OutScenario = FTGSimulationScenario{};
    OutMessage.Reset();

    if (Document.scenario.mass_flow_convention !=
        tgsim::MassFlowConvention::ThrustIncludesExhaustMomentum)
    {
        OutMessage = TEXT(
            "This .tgscn uses the backend-only MomentumDerivative mass-flow "
            "convention. The current Unreal scenario type supports only "
            "ThrustIncludesExhaustMomentum, so importing it would change the "
            "simulation and has been rejected.");
        return false;
    }

    FTGScenarioSolverConfig& solver = OutScenario.ScenarioAndSolver;
    solver.ScenarioName = FromUtf8(Document.scenario.name);
    solver.SimulationKind = ETGSimulationKind::Spacecraft6Dof;
    solver.StartUtc = FromUtcString(Document.scenario.start_utc);
    solver.EndMode = Document.scenario.end_mode == EndMode::FinalUtc
        ? ETGSimulationEndMode::FinalUtc
        : ETGSimulationEndMode::Duration;
    solver.FinalUtc = FromUtcString(Document.scenario.final_utc);
    solver.DurationSeconds = Document.scenario.duration_seconds;
    solver.IntegratorKind =
        Document.scenario.integrator == tgsim::IntegratorKind::AdaptiveDormandPrince54
            ? ETGIntegratorKind::AdaptiveDormandPrince54
            : ETGIntegratorKind::FixedStepRK4;
    solver.MaximumIntegratorStepSeconds =
        Document.scenario.maximum_integrator_step_seconds;
    solver.InitialIntegratorStepSeconds =
        Document.scenario.initial_integrator_step_seconds;
    solver.AbsoluteTolerance = Document.scenario.absolute_tolerance;
    solver.RelativeTolerance = Document.scenario.relative_tolerance;
    solver.OutputMode = Document.scenario.output_mode == tgsim::OutputMode::FixedInterval
        ? ETGOutputMode::FixedInterval
        : ETGOutputMode::EveryIntegratorStep;
    solver.OutputStepSeconds = Document.scenario.output_step_seconds;
    solver.MaximumIntegrationSteps = static_cast<int32>(FMath::Min<std::size_t>(
        Document.scenario.maximum_integration_steps,
        static_cast<std::size_t>(MAX_int32)));
    solver.MaximumOutputSamples = static_cast<int32>(FMath::Min<std::size_t>(
        Document.scenario.maximum_output_samples,
        static_cast<std::size_t>(MAX_int32)));
    solver.MaximumWallClockRuntimeSeconds =
        Document.scenario.maximum_wall_clock_runtime_seconds;
    solver.MassFlowConvention =
        ETGMassFlowConvention::ThrustIncludesExhaustMomentum;

    OutScenario.InitialState.PositionMeters =
        FromPortable(Document.initial_state.position_icrf_m);
    OutScenario.InitialState.VelocityMetersPerSecond =
        FromPortable(Document.initial_state.velocity_icrf_mps);
    OutScenario.InitialState.AttitudeBodyToIcrf =
        FromPortable(Document.initial_state.attitude_body_to_icrf);
    OutScenario.InitialState.AngularVelocityBodyRadiansPerSecond =
        FromPortable(Document.initial_state.angular_velocity_body_radps);
    OutScenario.InitialState.AuthoringFrameCatalogKey =
        Document.initial_state.authoring_frame.empty()
            ? NAME_None
            : FName(*FromUtf8(Document.initial_state.authoring_frame));

    for (const Component& source : Document.components)
    {
        FTGComponentConfig target;
        target.ComponentId = ParseGuidOrCreate(source.id);
        target.Name = FromUtf8(source.name);
        target.InitialMassKilograms = source.initial_mass_kg;
        target.MinimumMassKilograms = source.minimum_mass_kg;
        target.bVariableMass = source.variable_mass;
        target.LocalCenterOfMassMeters = FromPortable(source.local_center_of_mass_m);
        target.CentroidalInertia.IxxKilogramMetersSquared =
            source.centroidal_inertia.ixx_kgm2;
        target.CentroidalInertia.IyyKilogramMetersSquared =
            source.centroidal_inertia.iyy_kgm2;
        target.CentroidalInertia.IzzKilogramMetersSquared =
            source.centroidal_inertia.izz_kgm2;
        target.CentroidalInertia.IxyKilogramMetersSquared =
            source.centroidal_inertia.ixy_kgm2;
        target.CentroidalInertia.IxzKilogramMetersSquared =
            source.centroidal_inertia.ixz_kgm2;
        target.CentroidalInertia.IyzKilogramMetersSquared =
            source.centroidal_inertia.iyz_kgm2;
        target.OriginInBodyMeters = FromPortable(source.origin_body_m);
        target.ComponentToBodyOrientation = FromPortable(source.component_to_body);
        target.ParentComponentName = FromUtf8(source.parent_component_name);
        target.ParentAnchorMeters = FromPortable(source.parent_anchor_m);
        target.ChildAnchorMeters = FromPortable(source.child_anchor_m);
        target.ChildToParentZeroOrientation =
            FromPortable(source.child_to_parent_zero_orientation);
        for (const JointDof& source_dof : source.degrees_of_freedom)
        {
            FTGJointDofConfig target_dof;
            target_dof.DofId = ParseGuidOrCreate(source_dof.id);
            target_dof.Name = FromUtf8(source_dof.name);
            target_dof.MotionType = source_dof.motion == JointMotion::Translation
                ? ETGJointMotionType::Translation
                : ETGJointMotionType::Rotation;
            target_dof.Axis = FromPortable(source_dof.axis);
            target_dof.InitialCoordinate = source_dof.initial_coordinate;
            target_dof.InitialRate = source_dof.initial_rate;
            target_dof.bHasMinimumCoordinate = source_dof.minimum_coordinate.has_value();
            target_dof.MinimumCoordinate = source_dof.minimum_coordinate.value_or(0.0);
            target_dof.bHasMaximumCoordinate = source_dof.maximum_coordinate.has_value();
            target_dof.MaximumCoordinate = source_dof.maximum_coordinate.value_or(0.0);
            target_dof.MaximumAbsoluteRate = source_dof.maximum_absolute_rate;
            target_dof.MaximumAbsoluteEffort = source_dof.maximum_absolute_effort;
            target.DegreesOfFreedom.Add(MoveTemp(target_dof));
        }
        target.Visual = FromPortable(source.visual, SourceScenarioFilePath);
        target.SolarRadiationPressure.bIncludedInProxy = source.srp.included_in_proxy;
        target.SolarRadiationPressure.ProxyResolutionMode =
            source.srp.proxy_resolution == SrpProxyResolution::Custom
                ? ETGSrpProxyResolutionMode::CustomTargetTriangleCount
                : ETGSrpProxyResolutionMode::Automatic;
        target.SolarRadiationPressure.CustomTargetTriangleCount =
            source.srp.custom_target_triangle_count;
        target.SolarRadiationPressure.bUseGlobalFallbackOpticalProperties =
            source.srp.use_global_fallback_optical_properties;
        target.SolarRadiationPressure.
            bApplyOneOpticalConfigurationToEntireComponent =
            source.srp.apply_one_optical_configuration_to_entire_component;
        target.SolarRadiationPressure.ComponentOpticalProperties =
            FromPortable(source.srp.component_optical_properties);
        for (const SrpLogicalRegionOverride& source_override :
             source.srp.logical_region_overrides)
        {
            FTGSrpLogicalRegionOverride target_override;
            target_override.Region = FromPortable(source_override.region);
            target_override.OpticalProperties =
                FromPortable(source_override.optical_properties);
            target.SolarRadiationPressure.LogicalRegionOverrides.Add(
                MoveTemp(target_override));
        }
        for (const SrpTriangleOverride& source_override : source.srp.triangle_overrides)
        {
            FTGSrpTriangleOverride target_override;
            target_override.ProxyTriangleIndex = source_override.proxy_triangle_index;
            target_override.OpticalProperties =
                FromPortable(source_override.optical_properties);
            target.SolarRadiationPressure.TriangleOverrides.Add(
                MoveTemp(target_override));
        }
        target.SolarRadiationPressure.GeneratedGeometrySignature =
            FromUtf8(source.srp.generated_geometry_signature);
        target.SolarRadiationPressure.GeneratedTriangleCount =
            source.srp.generated_triangle_count;
        target.SolarRadiationPressure.bProxyGenerationRequired =
            source.srp.proxy_generation_required;
        target.SolarRadiationPressure.LastProxyGenerationMessage =
            FromUtf8(source.srp.last_proxy_generation_message);
        OutScenario.Components.Add(MoveTemp(target));
    }

    for (const Thruster& source : Document.thrusters)
    {
        FTGThrusterConfig target;
        target.Name = FromUtf8(source.name);
        target.Mode = source.mode == tgsim::ThrusterMode::Commanded
            ? ETGThrusterMode::Commanded
            : ETGThrusterMode::PrescribedProfile;
        target.MountComponentName = FromUtf8(source.mount_component_name);
        target.PropellantComponentName = FromUtf8(source.propellant_component_name);
        target.ApplicationPointMeters = FromPortable(source.application_point_component_m);
        target.Direction = FromPortable(source.direction_component);
        target.IgnitionTimeMode = source.ignition_time_mode == ThrusterTimeMode::Utc
            ? ETGThrusterTimeMode::AbsoluteUtc
            : ETGThrusterTimeMode::ElapsedSimulationTime;
        target.IgnitionUtc = FromUtcString(source.ignition_utc);
        target.IgnitionElapsedSeconds = source.ignition_elapsed_seconds;
        target.bNeverShutsDown = source.never_shuts_down;
        target.ShutdownTimeMode = source.shutdown_time_mode == ThrusterTimeMode::Utc
            ? ETGThrusterTimeMode::AbsoluteUtc
            : ETGThrusterTimeMode::ElapsedSimulationTime;
        target.ShutdownUtc = FromUtcString(source.shutdown_utc);
        target.ShutdownElapsedSeconds = source.shutdown_elapsed_seconds;
        target.PrescribedThrust = FromPortable(
            source.prescribed_thrust, SourceScenarioFilePath);
        target.PrescribedSpecificImpulse = FromPortable(
            source.prescribed_specific_impulse, SourceScenarioFilePath);
        target.MaximumThrustNewtons = source.maximum_thrust_n;
        OutScenario.Thrusters.Add(MoveTemp(target));
    }

    for (const ReactionWheel& source : Document.reaction_wheels)
    {
        FTGReactionWheelConfig target;
        target.Name = FromUtf8(source.name);
        target.MountComponentName = FromUtf8(source.mount_component_name);
        target.Axis = FromPortable(source.axis_component);
        target.InitialMomentumNewtonMeterSeconds = source.initial_momentum_nms;
        target.MaximumAbsoluteMomentumNewtonMeterSeconds =
            source.maximum_absolute_momentum_nms;
        OutScenario.ReactionWheels.Add(MoveTemp(target));
    }

    OutScenario.Control.Mode = Document.control.mode == ControlMode::CompiledUserController
        ? ETGControlMode::CompiledUserController
        : ETGControlMode::None;
    OutScenario.Control.ControllerId = Document.control.unreal_controller_id.empty()
        ? NAME_None
        : FName(*FromUtf8(Document.control.unreal_controller_id));
    OutScenario.Control.StandaloneControllerDllFilePath = FromUtf8(
        ResolveForUnreal(
            Document.control.controller_dll_path,
            SourceScenarioFilePath));
    OutScenario.GravitySettings.bIncludeFirstPostNewtonianCorrection =
        Document.include_first_post_newtonian_correction;
    for (const CelestialBody& source : Document.celestial_bodies)
    {
        FTGCelestialBodyConfig target;
        target.CatalogKey = FName(*FromUtf8(source.catalog_key));
        target.bGravityEnabled = source.gravity_enabled;
        target.AutomaticActivationRadiusMeters = source.automatic_activation_radius_m;
        target.BarycenterResolutionRadiusMeters = source.barycenter_resolution_radius_m;
        target.HarmonicModelCsvFilePath = FromUtf8(ResolveForUnreal(
            source.harmonic_model_csv_file_path, SourceScenarioFilePath));
        target.MaximumHarmonicDegreeUsed = source.maximum_harmonic_degree;
        OutScenario.CelestialBodies.Add(MoveTemp(target));
    }
    OutScenario.CelestialBodies =
        UTGGravityEditingLibrary::NormalizeCelestialBodyConfigs(
            OutScenario.CelestialBodies);

    const SolarRadiationPressure& srp = Document.solar_radiation_pressure;
    OutScenario.SolarRadiationPressure.bEnabled = srp.enabled;
    OutScenario.SolarRadiationPressure.SunBodyName = FromUtf8(srp.sun_body_name);
    OutScenario.SolarRadiationPressure.PressureAtOneAstronomicalUnitPascals =
        srp.pressure_at_one_au_pa;
    OutScenario.SolarRadiationPressure.bComputeEclipse = srp.compute_eclipse;
    for (const std::string& name : srp.occulting_body_names)
        OutScenario.SolarRadiationPressure.OccultingBodyNames.Add(FromUtf8(name));
    OutScenario.SolarRadiationPressure.bComputeComponentShadows =
        srp.compute_component_shadows;
    OutScenario.SolarRadiationPressure.GlobalFallbackOpticalProperties =
        FromPortable(srp.global_fallback_optical_properties);
    for (const OpticalFacet& source : srp.optical_facets)
    {
        FTGOpticalFacetConfig target;
        target.Name = FromUtf8(source.name);
        target.ComponentId = ParseGuidOrCreate(source.component_id);
        target.ComponentName = FromUtf8(source.component_name);
        target.StableTriangleIndex = source.stable_triangle_index;
        target.LogicalRegion = FromPortable(source.logical_region);
        target.Vertex0Meters = FromPortable(source.vertex0_component_m);
        target.Vertex1Meters = FromPortable(source.vertex1_component_m);
        target.Vertex2Meters = FromPortable(source.vertex2_component_m);
        target.AbsorptionFraction = source.optical_properties.absorption;
        target.SpecularReflectionFraction =
            source.optical_properties.specular_reflection;
        target.DiffuseReflectionFraction =
            source.optical_properties.diffuse_reflection;
        OutScenario.SolarRadiationPressure.OpticalFacets.Add(MoveTemp(target));
    }

    OutScenario.Atmosphere.bEnabled = Document.atmosphere.enabled;
    OutScenario.Atmosphere.CentralBodyName =
        FromUtf8(Document.atmosphere.central_body_name);
    OutScenario.Atmosphere.Model =
        Document.atmosphere.model == AtmosphereModel::CubicHarrisPriesterEarth
            ? ETGAtmosphereModel::CubicHarrisPriesterEarth
            : ETGAtmosphereModel::UploadedProfile;
    OutScenario.Atmosphere.GeneralProfileCsvPath = FromUtf8(ResolveForUnreal(
        Document.atmosphere.general_profile_csv_path, SourceScenarioFilePath));
    OutScenario.Atmosphere.CenteredAverageF107SolarFluxUnits =
        Document.atmosphere.centered_average_f107_sfu;
    OutScenario.Atmosphere.ChpCoefficientCsvPath = FromUtf8(ResolveForUnreal(
        Document.atmosphere.chp_coefficient_csv_path, SourceScenarioFilePath));
    OutScenario.Atmosphere.ChpMolecularProfileCsvPath = FromUtf8(ResolveForUnreal(
        Document.atmosphere.chp_molecular_profile_csv_path, SourceScenarioFilePath));

    const Aerodynamics& aero = Document.aerodynamics;
    OutScenario.Aerodynamics.bEnabled = aero.enabled;
    OutScenario.Aerodynamics.ReferenceAreaSquareMeters = aero.reference_area_m2;
    OutScenario.Aerodynamics.ReferenceLengthMeters = aero.reference_length_m;
    OutScenario.Aerodynamics.MinimumDynamicPressurePascals =
        aero.minimum_dynamic_pressure_pa;
    OutScenario.Aerodynamics.MaximumValidDynamicPressurePascals =
        aero.maximum_valid_dynamic_pressure_pa;
    OutScenario.Aerodynamics.bEnableConstantDragFallback =
        aero.constant_drag_fallback_enabled;
    OutScenario.Aerodynamics.FallbackDragCoefficient = aero.fallback_drag_coefficient;
    OutScenario.Aerodynamics.Database.bEnabled = aero.database.enabled;
    OutScenario.Aerodynamics.Database.CsvFilePath = FromUtf8(ResolveForUnreal(
        aero.database.csv_file_path, SourceScenarioFilePath));
    OutScenario.Aerodynamics.Database.Interpolation =
        aero.database.interpolation == AerodynamicInterpolation::NearestRow
            ? ETGAerodynamicDatabaseInterpolation::NearestRow
            : ETGAerodynamicDatabaseInterpolation::InverseDistance;
    OutScenario.Aerodynamics.Database.Extrapolation =
        aero.database.extrapolation == AerodynamicExtrapolation::NearestRow
            ? ETGAerodynamicDatabaseExtrapolation::NearestRow
            : ETGAerodynamicDatabaseExtrapolation::ConstantDragFallback;
    OutScenario.Aerodynamics.Database.NeighborCount =
        static_cast<int32>(FMath::Min<std::size_t>(
            aero.database.neighbor_count, static_cast<std::size_t>(MAX_int32)));
    OutScenario.Aerodynamics.Database.InverseDistancePower =
        aero.database.inverse_distance_power;
    OutScenario.Aerodynamics.Database.bUseMaximumNormalizedNeighborDistance =
        aero.database.maximum_normalized_neighbor_distance.has_value();
    OutScenario.Aerodynamics.Database.MaximumNormalizedNeighborDistance =
        aero.database.maximum_normalized_neighbor_distance.value_or(0.0);
    OutScenario.Aerodynamics.Database.MomentReferenceCenterBodyMeters =
        FromPortable(aero.database.moment_reference_center_body_m);
    for (const AerodynamicDatabaseRow& source : aero.database.rows)
    {
        FTGAerodynamicDatabaseRow target;
        target.SpeedRatio = source.speed_ratio;
        target.KnudsenNumber = source.knudsen_number;
        target.GasFlowDirectionBody = FromPortable(source.gas_flow_direction_body);
        for (const double coordinate : source.articulation_coordinates)
            target.ArticulationCoordinates.Add(coordinate);
        target.BodyForceCoefficients = FromPortable(source.force_coefficients_body);
        target.BodyMomentCoefficients = FromPortable(source.moment_coefficients_body);
        OutScenario.Aerodynamics.Database.Rows.Add(MoveTemp(target));
    }

    if (Document.control.mode == ControlMode::CompiledUserController &&
        Document.control.unreal_controller_id.empty() &&
        !Document.control.controller_dll_path.empty())
    {
        OutMessage = TEXT(
            "The scenario names a standalone controller DLL but has no Unreal "
            "controller-library ID. Import the DLL into the Controller Library "
            "and select it before simulation; it was not trusted or loaded automatically.");
    }
    else
    {
        OutMessage = TEXT("PHAROS scenario imported successfully.");
    }
    return true;
}

bool FTGScenarioDocumentAdapter::BuildSimulationRequest(
    const FTGSimulationScenario& Scenario,
    const UTGControllerLibrarySubsystem* ControllerLibrary,
    const FString& SourceScenarioFilePath,
    tgsim::SimulationRequest& OutRequest,
    FString& OutMessage)
{
    tgsim::scenario::ScenarioDocument document;
    ToPortableDocument(Scenario, FString{}, document);

    tgsim::SimulationRequest attachments;
    FString attachment_message;
    if (!FTGSimulationAdapter::AttachSpiceEphemerides(
            attachments, attachment_message))
    {
        OutMessage = attachment_message;
        return false;
    }
    if (!FTGSimulationAdapter::AttachController(
            Scenario.Control, ControllerLibrary, attachments, attachment_message))
    {
        OutMessage = attachment_message;
        return false;
    }

    tgsim::scenario::ScenarioCompilerServices services;
    services.ephemeris_provider = attachments.gravity.ephemeris_provider;
    services.controller = attachments.control.controller;
    services.utc_to_ephemeris_time = [](
        const std::string& utc,
        double& ephemeris_time,
        std::string& error)
    {
        FString message;
        const bool success =
            FTGSimulationAdapter::ConvertUtcToEphemerisTimeTdbSeconds(
                FromUtf8(utc), ephemeris_time, message);
        error = ToUtf8(message);
        return success;
    };

    tgsim::scenario::Diagnostics diagnostics;
    const bool success = tgsim::scenario::CompileScenario(
        document,
        FStringToPath(SourceScenarioFilePath),
        services,
        OutRequest,
        diagnostics);
    OutMessage = FromUtf8(tgsim::scenario::FormatDiagnostics(diagnostics));
    if (success && OutMessage.IsEmpty())
        OutMessage = TEXT("SimulationRequest built successfully.");
    return success;
}
