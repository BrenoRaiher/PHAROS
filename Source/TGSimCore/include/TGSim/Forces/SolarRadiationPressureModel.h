// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/SimulationConfig.h"
#include "TGSim/Vehicle/ComponentKinematics.h"

#include <memory>

namespace tgsim
{
    /// Generic external load pair: force in ICRF and torque in spacecraft body axes.
    struct LoadEvaluation
    {
        Vec3d force_icrf_n;
        Vec3d torque_body_nm;
        // Indexed by component; used by multibody dynamics to recover exact joint loads.
        std::vector<ComponentLoad> component_loads;
    };

    /// SRP totals plus the fraction of the apparent solar disk visible from the
    /// spacecraft: 1 in full sunlight, 0 in umbra, and 0<nu<1 in penumbra.
    struct SolarRadiationEvaluation : LoadEvaluation
    {
        double visible_sun_fraction = 1.0;
    };

    /// Computes facet solar-radiation-pressure forces and their moments about the CM.
    class SolarRadiationPressureModel
    {
    public:
        /// Derives immutable triangle geometry and builds one local-space BVH per
        /// component. Mesh simplification must already have been done by the converter.
        explicit SolarRadiationPressureModel(
            const SolarRadiationSettings& settings);

        /// Returns total illuminated-facet SRP force [N] in ICRF and torque [N m] in body axes.
        /// component_poses supplies each proxy's current nested articulated pose.
        /// Three fixed barycentric samples estimate partial component shadowing.
        SolarRadiationEvaluation ComputeLoads(
            double ephemeris_time_tdb_seconds,
            const SpacecraftState& state,
            const SimulationConfig& config,
            const Vec3d& center_of_mass_body_m,
            const std::vector<ComponentPose>& component_poses) const;

    private:
        struct Impl;
        std::shared_ptr<const Impl> impl_;
    };
}
