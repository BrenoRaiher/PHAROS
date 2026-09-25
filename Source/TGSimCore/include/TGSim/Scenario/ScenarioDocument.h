// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

// Portable, authored representation of one .tgscn file. This layer preserves
// both simulation inputs and Unreal-only presentation data, but never depends
// on Unreal Engine. ScenarioCompiler selects only physics fields when creating
// a SimulationRequest.

#include "TGSim/Core/SimulationRequest.h"

#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace tgsim::scenario
{
    enum class EndMode
    {
        FinalUtc,
        Duration
    };

    enum class JointMotion
    {
        Rotation,
        Translation
    };

    enum class GeometrySource
    {
        None,
        Primitive,
        CustomStl
    };

    enum class PrimitiveGeometry
    {
        Box,
        Sphere,
        Cylinder
    };

    enum class StlLengthUnit
    {
        Millimeters,
        Centimeters,
        Meters
    };

    enum class StlRecenterMode
    {
        KeepImportedOrigin,
        CenterOnBounds,
        PlaceBaseAtOrigin
    };

    enum class SurfaceAppearance
    {
        SolidColor,
        Textured
    };

    enum class SrpProxyResolution
    {
        Automatic,
        Custom
    };

    enum class SrpLogicalRegion
    {
        None,
        PositiveX,
        NegativeX,
        PositiveY,
        NegativeY,
        PositiveZ,
        NegativeZ,
        CylinderSide,
        CylinderPositiveCap,
        CylinderNegativeCap
    };

    enum class ScalarProfileSource
    {
        Constant,
        Csv
    };

    enum class ThrusterTimeMode
    {
        Utc,
        Elapsed
    };

    enum class ControlMode
    {
        None,
        CompiledUserController
    };

    enum class AtmosphereModel
    {
        UploadedProfile,
        CubicHarrisPriesterEarth
    };

    enum class AerodynamicInterpolation
    {
        InverseDistance,
        NearestRow
    };

    enum class AerodynamicExtrapolation
    {
        ConstantDragFallback,
        NearestRow
    };

    struct Color
    {
        double r = 1.0;
        double g = 1.0;
        double b = 1.0;
        double a = 1.0;
    };

    struct OpticalProperties
    {
        double absorption = 1.0;
        double specular_reflection = 0.0;
        double diffuse_reflection = 0.0;
    };

    struct ScenarioSolver
    {
        std::string name = "Untitled Scenario";
        std::string start_utc;
        EndMode end_mode = EndMode::Duration;
        std::string final_utc;
        double duration_seconds = 0.0;
        IntegratorKind integrator = IntegratorKind::FixedStepRK4;
        double maximum_integrator_step_seconds = 1.0;
        double initial_integrator_step_seconds = 0.1;
        double absolute_tolerance = 1.0e-9;
        double relative_tolerance = 1.0e-9;
        OutputMode output_mode = OutputMode::EveryIntegratorStep;
        double output_step_seconds = 1.0;
        std::size_t maximum_integration_steps = 10000000;
        std::size_t maximum_output_samples = 1000000;
        // Frontend/runner execution policy. ScenarioCompiler intentionally
        // omits this value from SimulationRequest because it is not physics.
        double maximum_wall_clock_runtime_seconds = 300.0;
        MassFlowConvention mass_flow_convention =
            MassFlowConvention::ThrustIncludesExhaustMomentum;
    };

    struct InitialState
    {
        Vec3d position_icrf_m;
        Vec3d velocity_icrf_mps;
        Quatd attitude_body_to_icrf;
        Vec3d angular_velocity_body_radps;
        // Optional authoring metadata. ScenarioCompiler intentionally ignores
        // it because the physical state above is already canonical ICRF data.
        std::string authoring_frame;
    };

    struct SymmetricInertia
    {
        double ixx_kgm2 = 0.0;
        double iyy_kgm2 = 0.0;
        double izz_kgm2 = 0.0;
        double ixy_kgm2 = 0.0;
        double ixz_kgm2 = 0.0;
        double iyz_kgm2 = 0.0;
    };

    struct JointDof
    {
        std::string id;
        std::string name;
        JointMotion motion = JointMotion::Rotation;
        Vec3d axis = Vec3d::UnitX();

        // Canonical in-memory units remain radians and radians per second for
        // rotation, and metres and metres per second for translation.
        double initial_coordinate = 0.0;
        double initial_rate = 0.0;
        std::optional<double> minimum_coordinate;
        std::optional<double> maximum_coordinate;
        double maximum_absolute_rate =
            std::numeric_limits<double>::infinity();
        double maximum_absolute_effort =
            std::numeric_limits<double>::infinity();
    };

    // This complete visual record is round-tripped for Unreal. ScenarioCompiler
    // deliberately ignores it except where the authored STL path is needed by
    // an Unreal-side SRP-proxy generation workflow.
    struct ComponentVisual
    {
        GeometrySource geometry_source = GeometrySource::Primitive;
        PrimitiveGeometry primitive_type = PrimitiveGeometry::Box;
        Vec3d box_dimensions_m{1.0, 1.0, 1.0};
        double sphere_radius_m = 0.5;
        double cylinder_radius_m = 0.5;
        double cylinder_length_m = 1.0;
        std::string stl_file_path;
        StlLengthUnit stl_length_unit = StlLengthUnit::Millimeters;
        StlRecenterMode stl_recenter_mode =
            StlRecenterMode::KeepImportedOrigin;
        Vec3d visual_offset_m;
        Quatd visual_orientation;
        Vec3d visual_scale{1.0, 1.0, 1.0};
        SurfaceAppearance surface_appearance = SurfaceAppearance::SolidColor;
        Color display_color{0.18, 0.55, 1.0, 1.0};
        Color base_color_tint;
        std::string base_color_texture_file_path;
        std::string normal_texture_file_path;
        std::string roughness_texture_file_path;
        std::string metallic_texture_file_path;
        bool visible = true;
    };

    struct SrpLogicalRegionOverride
    {
        SrpLogicalRegion region = SrpLogicalRegion::None;
        OpticalProperties optical_properties;
    };

    struct SrpTriangleOverride
    {
        int proxy_triangle_index = -1;
        OpticalProperties optical_properties;
    };

    struct ComponentSrpAuthoring
    {
        bool included_in_proxy = true;
        SrpProxyResolution proxy_resolution = SrpProxyResolution::Automatic;
        int custom_target_triangle_count = 200;
        bool use_global_fallback_optical_properties = true;
        bool apply_one_optical_configuration_to_entire_component = true;
        OpticalProperties component_optical_properties;
        std::vector<SrpLogicalRegionOverride> logical_region_overrides;
        std::vector<SrpTriangleOverride> triangle_overrides;

        // Unreal-side generated-proxy cache. These values are preserved in the
        // document but do not become backend configuration by themselves.
        std::string generated_geometry_signature;
        int generated_triangle_count = 0;
        bool proxy_generation_required = true;
        std::string last_proxy_generation_message;
    };

    struct Component
    {
        std::string id;
        std::string name;
        // Zero is valid for an individual component. The complete spacecraft
        // must retain positive initial and minimum reachable aggregate mass.
        double initial_mass_kg = 0.0;
        double minimum_mass_kg = 0.0;
        bool variable_mass = false;
        Vec3d local_center_of_mass_m;
        SymmetricInertia centroidal_inertia;

        // Root placement. Child placement is defined by the joint anchors and
        // zero-orientation below.
        Vec3d origin_body_m;
        Quatd component_to_body;
        std::string parent_component_name;
        Vec3d parent_anchor_m;
        Vec3d child_anchor_m;
        Quatd child_to_parent_zero_orientation;
        std::vector<JointDof> degrees_of_freedom;

        ComponentVisual visual;
        ComponentSrpAuthoring srp;
    };

    struct ScalarProfile
    {
        ScalarProfileSource source = ScalarProfileSource::Constant;
        double constant_value = 0.0;
        std::string csv_file_path;
    };

    struct Thruster
    {
        std::string name;
        ThrusterMode mode = ThrusterMode::PrescribedProfile;
        std::string mount_component_name;
        std::string propellant_component_name;
        Vec3d application_point_component_m;
        Vec3d direction_component = Vec3d::UnitX();
        ThrusterTimeMode ignition_time_mode = ThrusterTimeMode::Elapsed;
        std::string ignition_utc;
        double ignition_elapsed_seconds = 0.0;
        bool never_shuts_down = false;
        ThrusterTimeMode shutdown_time_mode = ThrusterTimeMode::Elapsed;
        std::string shutdown_utc;
        double shutdown_elapsed_seconds = 0.0;
        ScalarProfile prescribed_thrust;
        ScalarProfile prescribed_specific_impulse;
        double maximum_thrust_n = 0.0;
    };

    struct ReactionWheel
    {
        std::string name;
        std::string mount_component_name;
        Vec3d axis_component = Vec3d::UnitX();
        double initial_momentum_nms = 0.0;
        double maximum_absolute_momentum_nms = 0.0;
    };

    struct Control
    {
        ControlMode mode = ControlMode::None;

        // Unreal stores an opaque library ID. A standalone .tgscn may instead
        // name a DLL directly. Keeping both lets the same file round-trip through
        // the HUD and run outside Unreal without translating the controller ABI.
        std::string unreal_controller_id;
        std::string controller_dll_path;
    };

    struct CelestialBody
    {
        std::string catalog_key;
        bool gravity_enabled = false;
        double automatic_activation_radius_m = 0.0;
        double barycenter_resolution_radius_m = 0.0;
        std::string harmonic_model_csv_file_path;
        int maximum_harmonic_degree = 0;
    };

    struct OpticalFacet
    {
        std::string name;
        std::string component_id;
        std::string component_name;
        int stable_triangle_index = -1;
        SrpLogicalRegion logical_region = SrpLogicalRegion::None;
        Vec3d vertex0_component_m;
        Vec3d vertex1_component_m;
        Vec3d vertex2_component_m;
        OpticalProperties optical_properties;
    };

    struct SolarRadiationPressure
    {
        bool enabled = false;
        std::string sun_body_name = "Sun";
        double pressure_at_one_au_pa = 4.5391e-6;
        bool compute_eclipse = true;
        std::vector<std::string> occulting_body_names;
        bool compute_component_shadows = true;
        OpticalProperties global_fallback_optical_properties;
        std::vector<OpticalFacet> optical_facets;
    };

    struct Atmosphere
    {
        bool enabled = false;
        std::string central_body_name = "Earth";
        AtmosphereModel model = AtmosphereModel::UploadedProfile;
        std::string general_profile_csv_path;
        double centered_average_f107_sfu = 0.0;
        std::string chp_coefficient_csv_path;
        std::string chp_molecular_profile_csv_path;
    };

    struct AerodynamicDatabaseRow
    {
        double speed_ratio = 0.0;
        double knudsen_number = 0.0;
        Vec3d gas_flow_direction_body = Vec3d::UnitX();
        std::vector<double> articulation_coordinates;
        Vec3d force_coefficients_body;
        Vec3d moment_coefficients_body;
    };

    struct AerodynamicDatabase
    {
        bool enabled = false;
        std::string csv_file_path;
        AerodynamicInterpolation interpolation =
            AerodynamicInterpolation::InverseDistance;
        AerodynamicExtrapolation extrapolation =
            AerodynamicExtrapolation::ConstantDragFallback;
        std::size_t neighbor_count = 3;
        double inverse_distance_power = 2.0;
        std::optional<double> maximum_normalized_neighbor_distance;
        Vec3d moment_reference_center_body_m;
        std::vector<AerodynamicDatabaseRow> rows;
    };

    struct Aerodynamics
    {
        bool enabled = false;
        double reference_area_m2 = 0.0;
        double reference_length_m = 0.0;
        double minimum_dynamic_pressure_pa = 0.0;
        double maximum_valid_dynamic_pressure_pa = 0.0;
        bool constant_drag_fallback_enabled = false;
        double fallback_drag_coefficient = 0.0;
        AerodynamicDatabase database;
    };

    struct ScenarioDocument
    {
        std::string generator;
        ScenarioSolver scenario;
        InitialState initial_state;
        std::vector<Component> components;
        std::vector<Thruster> thrusters;
        std::vector<ReactionWheel> reaction_wheels;
        Control control;
        bool include_first_post_newtonian_correction = false;
        std::vector<CelestialBody> celestial_bodies;
        SolarRadiationPressure solar_radiation_pressure;
        Atmosphere atmosphere;
        Aerodynamics aerodynamics;
    };
}
