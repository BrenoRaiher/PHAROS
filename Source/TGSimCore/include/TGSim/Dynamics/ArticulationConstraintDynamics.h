// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

// Coupled impulsive corrections for ideal articulation-coordinate and speed limits.

#include "TGSim/Core/SimulationConfig.h"

#include <cstddef>
#include <string>
#include <vector>

namespace tgsim
{
    struct ArticulationVelocityConstraint
    {
        std::size_t articulation_state_index = kInvalidIndex;
        /// +1 for a lower boundary and -1 for an upper boundary. This orients
        /// the constraint impulse toward the admissible side of the limit.
        double admissible_direction = 1.0;
        double target_rate = 0.0;
    };

    struct ArticulationImpulseEvaluation
    {
        bool solved = false;
        std::string message;
        /// Nonnegative unilateral impulse magnitudes, indexed like the input constraints.
        std::vector<double> generalized_impulses;
        /// True only for contacts retained by the complementarity solve.
        std::vector<bool> active_constraints;
        /// Post-impact admissible normal velocities. Active entries are zero;
        /// released entries are positive and move away from their boundary.
        std::vector<double> post_constraint_velocities;
    };

    /// Applies instantaneous unilateral internal constraint impulses at fixed configuration.
    /// A complementarity solve retains only contacts that can be supported by a
    /// nonnegative mechanical-stop impulse. The free base and every articulation rate
    /// respond through the complete articulated inertia, while inertial total-CM
    /// velocity remains unchanged.
    class ArticulationConstraintDynamics
    {
    public:
        ArticulationImpulseEvaluation ApplyIdealVelocityConstraints(
            const SimulationConfig& config,
            SpacecraftState& state,
            const std::vector<ArticulationVelocityConstraint>& constraints) const;
    };
}
