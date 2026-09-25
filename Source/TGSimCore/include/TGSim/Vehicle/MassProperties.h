// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Vehicle/ComponentKinematics.h"

namespace tgsim
{
    /// Spacecraft-wide mass properties expressed in body axes about the instantaneous CM.
    struct MassProperties
    {
        double mass_kg = 0.0;
        Vec3d center_of_mass_body_m;
        /// Body-resolved derivative of the geometric total-spacecraft CM caused
        /// by articulation and changing component mass weights.
        Vec3d center_of_mass_rate_body_mps;
        /// beta = sum(m_dot_i (r_i-r_CM))/M in body axes. This is the part of
        /// geometric-CM motion caused solely by redistribution of lumped masses.
        Vec3d center_of_mass_mass_redistribution_rate_body_mps;
        Mat3d inertia_body_kgm2 = Mat3d::Zero();
        Mat3d inertia_rate_body_kgm2ps = Mat3d::Zero();
        std::vector<ComponentPose> component_poses;
    };

    /// Assembles total mass, center of mass, inertia, and inertia rate from components.
    class MassPropertiesModel
    {
    public:
        /// Evaluates spacecraft mass properties at state. Optional component mass rates
        /// and articulation rates are used in the analytical inertia and CM
        /// derivatives; no state is modified.
        MassProperties Compute(
            const VehicleSettings& vehicle,
            const SpacecraftState& state,
            const std::vector<double>& component_mass_rates_kgps = {},
            const std::vector<double>& articulation_rates = {}) const;

        /// Returns a component's current mass from its variable state, or its fixed initial mass.
        static double ComponentMass(const ComponentDefinition& component, const SpacecraftState& state);

    private:
        /// Computes m, r_CM, and I_CM without estimating the inertia derivative.
        MassProperties ComputeWithoutRate(
            const VehicleSettings& vehicle,
            const SpacecraftState& state,
            const std::vector<double>& articulation_rates = {}) const;
    };
}
