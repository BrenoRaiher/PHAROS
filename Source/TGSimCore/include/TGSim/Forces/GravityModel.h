// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/SimulationConfig.h"
#include "TGSim/Environment/IEphemerisProvider.h"
#include "TGSim/Vehicle/ComponentKinematics.h"

namespace tgsim
{
    /// Gravity-only result. Acceleration is mass-independent; force equals spacecraft mass times acceleration.
    struct GravityEvaluation
    {
        Vec3d acceleration_icrf_mps2;
        Vec3d force_icrf_n;
        Vec3d torque_about_cm_body_nm;
        std::vector<ComponentLoad> component_loads;
    };

    /// Evaluates configured Newtonian, harmonic, and optional 1PN gravity contributions.
    class GravityModel
    {
    public:
        /// Returns total gravity acceleration [m/s^2] and force [N], both in ICRF axes.
        GravityEvaluation ComputeGravity(
            double ephemeris_time_tdb_seconds,
            const SpacecraftState& state,
            const SimulationConfig& config,
            const Vec3d& center_of_mass_body_m,
            const std::vector<ComponentPose>& component_poses) const;

    private:
        /// Returns one body's Newtonian point-mass or spherical-harmonic acceleration in ICRF.
        Vec3d EvaluateNewtonianBody(
            const SpacecraftState& state,
            const GravityBody& body,
            const BodyState& body_state,
            const Mat3d& body_fixed_to_icrf) const;
        /// Returns the combined first post-Newtonian acceleration correction in ICRF.
        Vec3d EvaluateFirstPostNewtonian(
            const SpacecraftState& state,
            const GravitySettings& settings,
            const std::vector<bool>& active_bodies,
            const std::vector<BodyState>& body_states) const;
    };
}
