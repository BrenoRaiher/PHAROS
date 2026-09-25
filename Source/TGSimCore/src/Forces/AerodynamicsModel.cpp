// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Forces/AerodynamicsModel.h"

// Converts local atmospheric properties into one spacecraft aerodynamic
// wrench, using an aggregate coefficient database or constant-C_D drag.

#include "TGSim/Environment/EnvironmentModels.h"

#include <cmath>

namespace tgsim
{
    namespace
    {
        // Below this value, intermolecular interactions around the vehicle are
        // no longer negligible enough for this low-density model to be trusted.
        constexpr double kRarefiedFlowWarningKnudsenThreshold = 10.0;
    }

    AerodynamicsModel::AerodynamicsModel(const AerodynamicsSettings& settings)
    {
        if (settings.enabled && settings.coefficient_database.enabled)
        {
            interpolator_ = std::make_shared<AerodynamicCoefficientInterpolator>(
                settings.coefficient_database);
        }
    }

    AerodynamicsEvaluation AerodynamicsModel::ComputeLoads(
        double ephemeris_time_tdb_seconds,
        const SpacecraftState& state,
        const SimulationConfig& config,
        const Vec3d& center_of_mass_body_m) const
    {
        AerodynamicsEvaluation result;
        result.component_loads.resize(config.vehicle.components.size());
        if (!config.aerodynamics.enabled || !config.atmosphere.enabled)
            return result;

        const GravityBody* central_body = FindGravityBody(
            config.gravity, config.atmosphere.central_body_name);
        if (!central_body || central_body->reference_radius_m <= 0.0)
        {
            result.outside_validity = true;
            return result;
        }

        const BodyState central_body_state = ResolveBodyState(
            *central_body, config.gravity, ephemeris_time_tdb_seconds);
        const Mat3d central_body_fixed_to_icrf = ResolveBodyFixedToIcrf(
            *central_body, config.gravity, ephemeris_time_tdb_seconds);
        const Vec3d relative_position_icrf =
            state.position_icrf_m - central_body_state.position_icrf_m;
        const Vec3d central_body_relative_inertial_velocity_icrf =
            state.velocity_icrf_mps - central_body_state.velocity_icrf_mps;
        const double spherical_altitude_m =
            relative_position_icrf.Norm() - central_body->reference_radius_m;

        Vec3d sun_direction_from_central_body_icrf;
        if (config.atmosphere.model_kind ==
            AtmosphereModelKind::CubicHarrisPriesterEarth)
        {
            const GravityBody* sun = FindGravityBody(config.gravity, "Sun");
            if (!sun)
            {
                result.outside_validity = true;
                return result;
            }
            const BodyState sun_state = ResolveBodyState(
                *sun, config.gravity, ephemeris_time_tdb_seconds);
            sun_direction_from_central_body_icrf =
                sun_state.position_icrf_m - central_body_state.position_icrf_m;
        }

        // No wind model is added. The gas is assumed to co-rotate rigidly with
        // the central body: v_atm^I = v_body^I + omega_body^I x r_rel^I.
        const Vec3d central_body_angular_velocity_icrf =
            ResolveBodyAngularVelocityIcrf(
                *central_body,
                config.gravity,
                ephemeris_time_tdb_seconds);
        const Vec3d atmosphere_velocity_icrf =
            central_body_state.velocity_icrf_mps +
            Cross(central_body_angular_velocity_icrf, relative_position_icrf);
        const Vec3d relative_velocity_icrf =
            state.velocity_icrf_mps - atmosphere_velocity_icrf;
        const double relative_speed_mps = relative_velocity_icrf.Norm();
        if (relative_speed_mps <= 1.0e-12) return result;

        const AtmosphereState atmosphere_state = EvaluateAtmosphereState(
            config.atmosphere,
            spherical_altitude_m,
            relative_position_icrf,
            central_body_relative_inertial_velocity_icrf,
            central_body_fixed_to_icrf,
            sun_direction_from_central_body_icrf);
        if (!atmosphere_state.within_model_domain ||
            atmosphere_state.density_kgpm3 <= 0.0)
        {
            result.outside_validity = true;
            return result;
        }

        // Dynamic pressure: q = (1/2) rho |v_rel|^2.
        result.dynamic_pressure_pa = 0.5 * atmosphere_state.density_kgpm3 *
            relative_speed_mps * relative_speed_mps;
        // Knudsen number: Kn = lambda/L_ref.
        result.knudsen_number = EvaluateMeanFreePath(atmosphere_state) /
            config.aerodynamics.reference_length_m;
        result.molecular_speed_ratio = EvaluateMolecularSpeedRatio(
            atmosphere_state, relative_speed_mps);

        if (result.dynamic_pressure_pa <
            config.aerodynamics.minimum_dynamic_pressure_pa)
        {
            return result;
        }
        result.outside_validity =
            result.dynamic_pressure_pa >
                config.aerodynamics.maximum_dynamic_pressure_pa ||
            result.knudsen_number <= kRarefiedFlowWarningKnudsenThreshold;

        const Mat3d body_to_icrf =
            state.attitude_body_to_icrf.ToRotationMatrix();
        const Mat3d icrf_to_body = body_to_icrf.Transposed();
        // u_flow is the gas velocity direction seen by the spacecraft:
        // u_flow^B = R_BI [-v_rel^I/|v_rel|].
        const Vec3d incoming_flow_icrf = -relative_velocity_icrf.Normalized();
        const Vec3d incoming_flow_body = icrf_to_body * incoming_flow_icrf;

        const bool has_database =
            config.aerodynamics.coefficient_database.enabled;
        if (has_database && interpolator_ && interpolator_->IsReady())
        {
            AerodynamicCoefficientResult coefficients;
            const AerodynamicCoefficientQuery query{
                result.molecular_speed_ratio,
                result.knudsen_number,
                incoming_flow_body,
                &state.articulation_coordinates};
            if (interpolator_->Interpolate(query, coefficients))
            {
                result.used_coefficient_database = true;
                result.outside_validity = result.outside_validity ||
                    coefficients.outside_sampled_scalar_bounds;
                result.force_coefficients_body =
                    coefficients.force_coefficients_body;

                // Whole-spacecraft database force:
                // F_aero^B = q S_ref C_F^B(s,Kn,u_flow^B,eta).
                const Vec3d force_body = result.dynamic_pressure_pa *
                    config.aerodynamics.reference_area_m2 *
                    coefficients.force_coefficients_body;
                result.force_icrf_n = body_to_icrf * force_body;

                // Database moment is about the fixed body-frame point R:
                // M_R^B = q S_ref L_ref C_M,R^B.
                const Vec3d moment_about_reference_body =
                    result.dynamic_pressure_pa *
                    config.aerodynamics.reference_area_m2 *
                    config.aerodynamics.reference_length_m *
                    coefficients.moment_coefficients_body_about_reference;
                // Transport that moment to the instantaneous total CM:
                // M_CM^B = M_R^B + (r_R^B-r_CM^B) x F^B.
                result.torque_body_nm = moment_about_reference_body + Cross(
                    config.aerodynamics.coefficient_database.
                        moment_reference_point_body_m - center_of_mass_body_m,
                    force_body);
                const double moment_scale = result.dynamic_pressure_pa *
                    config.aerodynamics.reference_area_m2 *
                    config.aerodynamics.reference_length_m;
                result.moment_coefficients_body_about_cm =
                    result.torque_body_nm / moment_scale;
                return result;
            }

            // A database miss remains visible in telemetry even when the simple
            // drag fallback supplies a finite translational force.
            result.outside_validity = true;
        }

        if (!config.aerodynamics.constant_drag_fallback_enabled) return result;
        result.used_fallback_model = true;

        // Whole-spacecraft drag-only fallback:
        // F_D^I = q S_ref C_D u_flow^I
        //       = -(1/2) rho |v_rel|^2 S_ref C_D v_rel^I/|v_rel|.
        // It acts at the instantaneous total CM by definition, so torque_body_nm
        // and every component load intentionally remain zero.
        const double force_scale = result.dynamic_pressure_pa *
            config.aerodynamics.reference_area_m2;
        result.force_icrf_n = force_scale *
            config.aerodynamics.fallback_drag_coefficient * incoming_flow_icrf;
        result.force_coefficients_body =
            config.aerodynamics.fallback_drag_coefficient * incoming_flow_body;
        return result;
    }
}
