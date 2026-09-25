// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/SimulationConfig.h"
#include "TGSim/Environment/IEphemerisProvider.h"

namespace tgsim
{
    /// Returns body center state from IEphemerisProvider, or constant-velocity fallback data.
    BodyState ResolveBodyState(
        const GravityBody& body,
        const GravitySettings& settings,
        double ephemeris_time_tdb_seconds);
    /// Returns the matrix mapping body-fixed vector components into ICRF at this epoch.
    Mat3d ResolveBodyFixedToIcrf(
        const GravityBody& body,
        const GravitySettings& settings,
        double ephemeris_time_tdb_seconds);
    /// Returns body-frame angular velocity relative to ICRF, expressed in ICRF.
    Vec3d ResolveBodyAngularVelocityIcrf(
        const GravityBody& body,
        const GravitySettings& settings,
        double ephemeris_time_tdb_seconds);
    /// Finds a configured gravity body by exact name, or returns nullptr.
    const GravityBody* FindGravityBody(const GravitySettings& settings, const std::string& name);

    struct AtmosphereState
    {
        double density_kgpm3 = 0.0;
        double temperature_k = 0.0;
        double mean_particle_mass_kg = 0.0;
        double effective_collision_cross_section_m2 = 0.0;
        // False means altitude was outside at least one required uploaded table.
        bool within_model_domain = false;
    };

    /// Evaluates all local gas properties. For cubic Harris-Priester, relative
    /// position, center-relative inertial velocity, and Sun direction are
    /// required for orbital inclination and the diurnal density bulge; all
    /// vectors are ICRF. This velocity is not the atmosphere-relative flow.
    AtmosphereState EvaluateAtmosphereState(
        const AtmosphereSettings& settings,
        double altitude_m,
        const Vec3d& relative_position_icrf_m,
        const Vec3d& central_body_relative_inertial_velocity_icrf_mps,
        const Mat3d& central_body_fixed_to_icrf,
        const Vec3d& sun_direction_from_central_body_icrf);

    /// Kinetic-theory mean free path:
    /// lambda = 1 / (sqrt(2) n sigma), n = rho / mean_particle_mass.
    double EvaluateMeanFreePath(const AtmosphereState& atmosphere_state);

    /// Molecular speed ratio used by free-molecular aerodynamics:
    /// s = |v_rel| / sqrt(2 k_B T / mean_particle_mass).
    double EvaluateMolecularSpeedRatio(
        const AtmosphereState& atmosphere_state,
        double relative_speed_mps);
}
