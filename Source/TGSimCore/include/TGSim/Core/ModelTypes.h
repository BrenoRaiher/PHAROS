// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

// User-configurable physical model definitions. These are data contracts only;
// the corresponding calculations live under Environment, Forces, and Vehicle.

#include "TGSim/Core/Types.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tgsim
{
    class IController;
    class IEphemerisProvider;

    constexpr std::size_t kInvalidIndex = std::numeric_limits<std::size_t>::max();

    struct ScalarSample
    {
        double time_seconds = 0.0;
        double value = 0.0;
    };

    /// Policy used before the first sample and after the last sample.
    enum class ScalarExtrapolationMethod
    {
        ClampToEndpoint,
        ExtendEndpointSlope,
        UseDefaultValue
    };

    struct ScalarCurve
    {
        std::vector<ScalarSample> samples;
        double default_value = 0.0;
        ScalarExtrapolationMethod extrapolation = ScalarExtrapolationMethod::ClampToEndpoint;

        /// Returns the linearly interpolated scalar profile at the supplied
        /// curve-local time. It does not advance simulation time or evaluate a
        /// force by itself.
        double ValueAt(double curve_time_seconds) const;

        /// Returns the analytic time derivative of the piecewise-linear curve.
        /// Its units are the curve value's units per second.
        double RateAt(double curve_time_seconds) const;
    };

    /// Physical motion represented by one generalized articulation coordinate.
    enum class ArticulationMotion
    {
        Rotation,   // Coordinate/rate/acceleration units: rad, rad/s, rad/s^2.
        Translation // Coordinate/rate/acceleration units: m, m/s, m/s^2.
    };

    /// Bounds applied to one generalized articulation coordinate and its derivatives.
    struct ArticulationLimits
    {
        double minimum_coordinate = -std::numeric_limits<double>::infinity();
        double maximum_coordinate = std::numeric_limits<double>::infinity();
        double maximum_absolute_rate = std::numeric_limits<double>::infinity();
        // N m for a rotational DOF, N for a translational DOF.
        double maximum_absolute_effort = std::numeric_limits<double>::infinity();
    };

    /// One axis-based DOF in a joint's ordered transform chain.
    /// The axis is expressed in the joint frame after all preceding DOFs are applied.
    struct ArticulationDof
    {
        std::string name;
        ArticulationMotion motion = ArticulationMotion::Rotation;
        Vec3d axis_joint = Vec3d::UnitZ();
        ArticulationLimits limits;
        double initial_coordinate = 0.0;
        double initial_rate = 0.0;
    };

    /// Connection from a parent component to a child component.
    /// Ordered DOFs may freely mix rotation and translation and therefore represent
    /// hinges, gimbals, sliders, Cartesian stages, or compound joints.
    struct ArticulationDefinition
    {
        Vec3d parent_anchor_component_m; // Joint origin in parent component axes.
        Vec3d child_anchor_component_m;  // Matching joint point in child component axes.
        Mat3d child_to_parent_at_zero = Mat3d::Identity();
        std::vector<ArticulationDof> dofs;
    };

    /// One rigid spacecraft component used to assemble total mass properties.
    struct ComponentDefinition
    {
        std::string name;
        // Individual masses may be zero. Validation requires positive aggregate
        // initial mass and positive aggregate reachable mass at all depletion floors.
        double initial_mass_kg = 0.0;
        double minimum_mass_kg = 0.0;
        std::size_t variable_mass_state_index = kInvalidIndex; // kInvalidIndex means constant mass.
        // The main fixed component has no parent and defines the spacecraft body axes.
        // Every child must reference an earlier component, allowing nested articulation trees.
        std::size_t parent_component_index = kInvalidIndex;
        std::size_t articulation_state_offset = kInvalidIndex; // Assigned by the config builder.
        Vec3d origin_body_m; // Used only by the root component.
        Mat3d component_to_body = Mat3d::Identity(); // Used only by the root component.
        ArticulationDefinition articulation_to_parent; // Used only by child components.
        Vec3d center_of_mass_component_m;
        Mat3d inertia_centroid_component_kgm2 = Mat3d::Identity();
    };

    /// One triangle of a converter-generated SRP proxy mesh. Vertex winding is
    /// right-handed in component axes: (v1-v0) x (v2-v0) points outward. The
    /// backend derives center, area, and normal from these authoritative vertices.
    struct OpticalFacet
    {
        std::string name;
        std::size_t component_index = 0;
        std::array<Vec3d, 3> vertices_component_m{};
        // Opaque-surface fractions resolved by the HUD converter after applying
        // global, component, and optional individual-facet overrides.
        double absorption = 1.0;
        double specular_reflection = 0.0;
        double diffuse_reflection = 0.0;
    };

    struct HarmonicCoefficient
    {
        // Fully normalized Cbar_nm and Sbar_nm. C00/S00 are implicit and must not be supplied.
        int degree = 0;
        int order = 0;
        double normalized_c = 0.0;
        double normalized_s = 0.0;
    };

    /// Identifies how one gravity source participates in a planetary-system
    /// barycenter/member switch. Independent sources do not participate.
    enum class GravitySourceRole
    {
        Independent,
        SystemBarycenter,
        SystemMember
    };

    /// Runtime definition of one catalog body. The production SPICE provider
    /// fills point-mass GM and physical shape radius; uploaded harmonic models
    /// carry their own GM and mathematical reference radius.
    struct GravityBody
    {
        std::string name;
        // Fixed SPICE catalog identity. Zero is reserved for synthetic
        // backend-only bodies; nonzero IDs must be unique in one request.
        int naif_id = 0;
        // Fixed catalog metadata, not user input. Members and their barycenter
        // share the same nonempty system name.
        std::string gravity_system_name;
        GravitySourceRole gravity_source_role = GravitySourceRole::Independent;
        // A body may remain in the ephemeris/result catalog while its gravity is
        // unchecked. A positive activation radius re-enables gravity automatically
        // when the spacecraft CM enters that user-supplied distance from this source.
        bool gravity_enabled = true;
        double automatic_gravity_activation_radius_m = 0.0;
        // Barycenter only. An active barycenter is replaced by every member in
        // its system when the spacecraft CM enters this distance. This is
        // separate from automatic activation because the two operations differ.
        double barycenter_resolution_radius_m = 0.0;
        // Runtime metadata populated from the ephemeris/constant provider.
        double gravitational_parameter_m3ps2 = 0.0;
        double reference_radius_m = 0.0;
        // Synthetic fallback data retained for backend-only tests and providers
        // other than the mandatory Unreal-side SPICE implementation.
        Vec3d position_icrf_at_epoch_m;
        Vec3d velocity_icrf_mps;
        Mat3d body_fixed_to_icrf_at_epoch = Mat3d::Identity();
        Vec3d spin_axis_icrf = Vec3d::UnitZ();
        double spin_rate_radps = 0.0;
        double epoch_ephemeris_time_tdb_seconds = 0.0;
        // Parsed from the uploaded no-header gravity-model CSV. These constants
        // belong to the coefficient solution and are intentionally distinct
        // from the SPICE GM and physical shape radius above.
        double harmonic_model_gravitational_parameter_m3ps2 = 0.0;
        double harmonic_model_reference_radius_m = 0.0;
        // Zero selects SPICE-GM point-mass gravity. A positive value selects
        // the uploaded harmonic model, optionally truncated below its file maximum.
        int maximum_harmonic_degree = 0;
        std::vector<HarmonicCoefficient> harmonics;
    };

    /// Selects all bodies and optional high-fidelity corrections used by gravity.
    struct GravitySettings
    {
        std::vector<GravityBody> bodies;
        bool include_first_post_newtonian_correction = false;
        // The Unreal adapter always supplies SPICE. The interface remains injectable
        // so standalone validation can use deterministic synthetic ephemerides.
        std::shared_ptr<const IEphemerisProvider> ephemeris_provider;
    };

    /// Facet SRP configuration. Eclipse visibility is calculated geometrically from
    /// configured celestial-body radii and ephemerides at each dynamics evaluation.
    struct SolarRadiationSettings
    {
        bool enabled = false;
        std::string sun_body_name = "Sun";
        double pressure_at_one_au_pa = 4.5391e-6;
        bool compute_eclipse_shadow = true;
        // When enabled, each illuminated proxy triangle uses three area-weighted
        // rays against the same articulated proxy meshes to estimate visibility.
        bool compute_component_shadows = true;
        // Empty means every configured non-Sun body may occult the Sun.
        std::vector<std::string> occulting_body_names;
        std::vector<OpticalFacet> facets;
    };

    enum class AtmosphereModelKind
    {
        TabulatedProfile,
        CubicHarrisPriesterEarth
    };

    /// One uploaded density station. Between stations the backend interpolates
    /// ln(rho) linearly, which is equivalent to an exponential density segment.
    struct AtmosphereDensitySample
    {
        double altitude_m = 0.0;
        double density_kgpm3 = 0.0;
    };

    /// Molecular properties required to compute mean free path and molecular
    /// speed ratio. Every value is linearly interpolated in altitude.
    struct AtmosphereThermodynamicSample
    {
        double altitude_m = 0.0;
        double temperature_k = 0.0;
        double mean_particle_mass_kg = 0.0;
        double effective_collision_cross_section_m2 = 0.0;
    };

    /// Cubic coefficients for one Harris-Priester altitude station. The arrays
    /// contain [c0,c1,c2,c3] in SI, so
    /// rho_min/max(h_i,F) = c0 + c1 F + c2 F^2 + c3 F^3 [kg/m^3].
    struct CubicHarrisPriesterDensitySample
    {
        double altitude_m = 0.0;
        std::array<double, 4> maximum_density_coefficients_kgpm3{};
        std::array<double, 4> minimum_density_coefficients_kgpm3{};
    };

    struct CubicHarrisPriesterSettings
    {
        // Centered 81-day average F10.7 solar flux, in solar-flux units.
        double centered_average_f107_sfu = 150.0;
        std::vector<CubicHarrisPriesterDensitySample> density_samples;
    };

    /// Atmosphere attached to one configured gravity body. Density comes either
    /// from an uploaded general profile or the Earth-only cubic Harris-Priester
    /// model. Molecular properties always come from the uploaded profile.
    struct AtmosphereSettings
    {
        bool enabled = false;
        std::string central_body_name = "Earth";
        AtmosphereModelKind model_kind = AtmosphereModelKind::TabulatedProfile;
        std::vector<AtmosphereDensitySample> density_profile;
        std::vector<AtmosphereThermodynamicSample> thermodynamic_profile;
        CubicHarrisPriesterSettings cubic_harris_priester;
    };

    /// One scattered row in the AVS-style aerodynamic coefficient database.
    /// Independent coordinates are molecular speed ratio, Kn, incoming gas-flow
    /// direction in B, and the complete flattened articulation vector eta. The
    /// six outputs use B axes and represent the whole spacecraft.
    struct AerodynamicCoefficientSample
    {
        double molecular_speed_ratio = 0.0;
        double knudsen_number = 0.0;
        Vec3d incoming_flow_direction_body = Vec3d::UnitX();
        std::vector<double> articulation_coordinates;
        Vec3d force_coefficients_body;
        Vec3d moment_coefficients_body_about_reference;
    };

    enum class AerodynamicDatabaseInterpolationMethod
    {
        /// Exact Shepard interpolation over nearby scattered samples.
        InverseDistance,
        NearestNeighbor
    };

    enum class AerodynamicDatabaseExtrapolationMethod
    {
        /// If speed ratio, Kn, or eta is outside its sampled interval, use the
        /// whole-spacecraft constant-drag fallback configured below.
        UseConstantDragFallback,
        /// Return the closest database row outside the sampled intervals.
        NearestNeighbor
    };

    struct AerodynamicCoefficientDatabase
    {
        bool enabled = false;
        // Point R is fixed in spacecraft body axes. Database C_M is about R;
        // AerodynamicsModel transports that physical moment to the current CM.
        Vec3d moment_reference_point_body_m;
        std::vector<AerodynamicCoefficientSample> samples;
        AerodynamicDatabaseInterpolationMethod interpolation =
            AerodynamicDatabaseInterpolationMethod::InverseDistance;
        AerodynamicDatabaseExtrapolationMethod extrapolation =
            AerodynamicDatabaseExtrapolationMethod::UseConstantDragFallback;
        std::size_t nearest_neighbor_count = 3;
        double inverse_distance_power = 2.0;
        // Optional coverage guard in normalized input space; infinity disables it.
        double maximum_normalized_neighbor_distance =
            std::numeric_limits<double>::infinity();
    };

    /// Low-density aerodynamic force/moment configuration.
    struct AerodynamicsSettings
    {
        bool enabled = false;
        double reference_area_m2 = 0.0;
        double reference_length_m = 0.0;
        // Preferred model. Each row maps (s, Kn, u_flow^B, eta) to
        // (C_F^B, C_M,R^B) for the complete spacecraft.
        AerodynamicCoefficientDatabase coefficient_database;
        // Deliberately simple fallback: F_D = q S_ref C_D u_flow. It acts at the
        // instantaneous total CM, so it creates no torque or articulated-body load.
        bool constant_drag_fallback_enabled = false;
        double fallback_drag_coefficient = 2.2;
        double minimum_dynamic_pressure_pa = 0.0;
        double maximum_dynamic_pressure_pa = 1000.0;
    };

    enum class ThrusterMode
    {
        PrescribedProfile,
        Commanded
    };

    /// External thruster behavior; no combustion or internal flow is simulated.
    struct ThrusterDefinition
    {
        std::string name;
        ThrusterMode mode = ThrusterMode::PrescribedProfile;
        std::size_t component_index = 0;
        // Required variable-mass component depleted by this thruster. The mount
        // component and propellant component may be different physical bodies.
        std::size_t propellant_component_index = kInvalidIndex;
        Vec3d application_point_component_m;
        Vec3d direction_component = Vec3d::UnitX();
        // Absolute ET values are retained for metadata and controller APIs. Mission-
        // relative scheduling uses the elapsed fields below so short intervals are not
        // rounded away by addition to, and subtraction from, a large SPICE epoch.
        double ignition_ephemeris_time_tdb_seconds = 0.0;
        double shutdown_ephemeris_time_tdb_seconds =
            std::numeric_limits<double>::infinity();
        // NaN preserves compatibility with direct callers that only populate the
        // legacy absolute fields. SimulationConfigBuilder resolves that fallback once.
        double ignition_elapsed_time_seconds =
            std::numeric_limits<double>::quiet_NaN();
        double shutdown_elapsed_time_seconds =
            std::numeric_limits<double>::quiet_NaN();
        // Prescribed mode: use these constants when the corresponding curve is empty.
        double constant_thrust_n = 0.0;
        double constant_specific_impulse_seconds = 0.0;
        ScalarCurve thrust_profile_n; // T(t-t_ignition), linearly interpolated.
        ScalarCurve specific_impulse_profile_seconds; // Isp(t-t_ignition).
        // Commanded mode: T = maximum_thrust_n * clamp(controller throttle, 0, 1).
        double maximum_thrust_n = 0.0;
    };

    /// Returns the authoritative mission-relative ignition boundary. The absolute-
    /// ET subtraction is only a compatibility fallback for legacy direct requests.
    inline double ThrusterIgnitionElapsedTime(
        const ThrusterDefinition& thruster,
        double simulation_start_ephemeris_time_tdb_seconds)
    {
        return std::isnan(thruster.ignition_elapsed_time_seconds)
            ? thruster.ignition_ephemeris_time_tdb_seconds -
                simulation_start_ephemeris_time_tdb_seconds
            : thruster.ignition_elapsed_time_seconds;
    }

    /// Returns the authoritative mission-relative shutdown boundary.
    inline double ThrusterShutdownElapsedTime(
        const ThrusterDefinition& thruster,
        double simulation_start_ephemeris_time_tdb_seconds)
    {
        return std::isnan(thruster.shutdown_elapsed_time_seconds)
            ? thruster.shutdown_ephemeris_time_tdb_seconds -
                simulation_start_ephemeris_time_tdb_seconds
            : thruster.shutdown_elapsed_time_seconds;
    }

    /// Internal momentum-storage actuator mounted on one spacecraft component.
    struct ReactionWheelDefinition
    {
        std::string name;
        std::size_t component_index = 0;
        Vec3d axis_component = Vec3d::UnitZ();
        double initial_momentum_nms = 0.0;
        double maximum_momentum_nms = std::numeric_limits<double>::infinity();
    };

    /// Complete physical definition of the simulated spacecraft.
    struct VehicleSettings
    {
        std::vector<ComponentDefinition> components;
        std::vector<ThrusterDefinition> thrusters;
        std::vector<ReactionWheelDefinition> reaction_wheels;
    };

    /// Output of IController at one time and state.
    struct ControlCommand
    {
        // Commanded-thruster values are written together by
        // ControlCommandWriter::SetThrusterCommand. Prescribed thrusters ignore them.
        std::vector<double> thruster_throttles;
        std::vector<double> thruster_specific_impulses_seconds;
        // Optional total derivatives of actual outward discharge q=T/(g0*Isp),
        // indexed in configured thruster order. Omission contributes zero only
        // to the geometric-CM m_ddot correction for this RHS evaluation.
        std::vector<std::optional<double>>
            thruster_mass_flow_derivatives_kgps2;
        Vec3d external_torque_body_nm;
        // Known actuator torque [N m] or force [N] for each articulation coordinate.
        // Zero represents a passive/unactuated joint.
        std::vector<double> joint_efforts;
        std::vector<double> wheel_momentum_rates_nm; // Commanded h_dot for each wheel.
    };

    /// Optional user-supplied guidance/control implementation. TGSimCore does
    /// not provide a control law; it only constructs the input/output contract.
    struct ControlSettings
    {
        std::shared_ptr<const IController> controller;
    };
}
