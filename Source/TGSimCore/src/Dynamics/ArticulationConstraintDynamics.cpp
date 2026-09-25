// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Dynamics/ArticulationConstraintDynamics.h"

#include "TGSim/Dynamics/FloatingBaseTreeDynamics.h"
#include "TGSim/Vehicle/MassProperties.h"

#include <Eigen/LU>

#include <algorithm>
#include <cmath>
#include <limits>

namespace tgsim
{
    ArticulationImpulseEvaluation
    ArticulationConstraintDynamics::ApplyIdealVelocityConstraints(
        const SimulationConfig& config,
        SpacecraftState& state,
        const std::vector<ArticulationVelocityConstraint>& constraints) const
    {
        ArticulationImpulseEvaluation result;
        if (constraints.empty())
        {
            result.solved = true;
            return result;
        }

        const std::size_t articulation_count = state.articulation_rates.size();
        for (std::size_t row = 0; row < constraints.size(); ++row)
        {
            const ArticulationVelocityConstraint& constraint = constraints[row];
            if (constraint.articulation_state_index >= articulation_count ||
                std::abs(std::abs(constraint.admissible_direction) - 1.0) > 1.0e-12 ||
                !std::isfinite(constraint.target_rate))
            {
                result.message = "An articulation impulse references an invalid velocity constraint.";
                return result;
            }
            for (std::size_t preceding = 0; preceding < row; ++preceding)
            {
                if (constraints[preceding].articulation_state_index ==
                    constraint.articulation_state_index)
                {
                    result.message = "An articulation impulse contains duplicate velocity constraints.";
                    return result;
                }
            }
        }

        // At an ideal impulse, configuration and mass are continuous. Evaluate H^-1 J^T
        // one constraint column at a time by applying a unit generalized impulse to the
        // zero-velocity tree. With all velocity biases zero, ABA acceleration response is
        // exactly the generalized velocity jump per unit impulse.
        SpacecraftState zero_velocity_state = state;
        zero_velocity_state.velocity_icrf_mps = Vec3d::Zero();
        zero_velocity_state.angular_velocity_body_radps = Vec3d::Zero();
        zero_velocity_state.articulation_rates.assign(articulation_count, 0.0);
        zero_velocity_state.internal_angular_momenta_nms.assign(
            state.internal_angular_momenta_nms.size(), 0.0);

        const MassProperties mass_properties = MassPropertiesModel().Compute(
            config.vehicle, zero_velocity_state);
        FloatingBaseTreeInput input;
        input.center_of_mass_body_m = mass_properties.center_of_mass_body_m;
        input.component_poses = mass_properties.component_poses;
        input.articulation_rates.assign(articulation_count, 0.0);
        input.applied_joint_efforts.assign(articulation_count, 0.0);
        input.variable_component_mass_rates_kgps.assign(
            zero_velocity_state.variable_component_masses_kg.size(), 0.0);

        const std::size_t constraint_count = constraints.size();
        Eigen::MatrixXd inverse_constraint_inertia(
            static_cast<Eigen::Index>(constraint_count),
            static_cast<Eigen::Index>(constraint_count));
        std::vector<SpatialMotion> base_responses(constraint_count);
        std::vector<std::vector<double>> articulation_responses(constraint_count);
        for (std::size_t column = 0; column < constraint_count; ++column)
        {
            std::fill(
                input.applied_joint_efforts.begin(),
                input.applied_joint_efforts.end(),
                0.0);
            input.applied_joint_efforts[
                constraints[column].articulation_state_index] =
                    constraints[column].admissible_direction;
            const FloatingBaseTreeEvaluation response =
                FloatingBaseTreeDynamics().SolveForwardDynamics(
                    zero_velocity_state,
                    config,
                    input,
                    false);
            if (!response.solved ||
                response.joint_accelerations.size() < articulation_count)
            {
                result.message = "The articulated inertia could not resolve a joint-limit impulse.";
                return result;
            }
            base_responses[column] = response.base_acceleration_body;
            articulation_responses[column] = response.joint_accelerations;
            for (std::size_t row = 0; row < constraint_count; ++row)
            {
                inverse_constraint_inertia(
                    static_cast<Eigen::Index>(row),
                    static_cast<Eigen::Index>(column)) =
                    constraints[row].admissible_direction *
                    response.joint_accelerations[
                        constraints[row].articulation_state_index];
            }
        }

        // For each candidate contact, normal velocity is
        //   gamma = s * (qdot - qdot_target),
        // where s points toward the admissible side. The unilateral impact law is
        //   lambda >= 0, gamma_plus >= 0, lambda*gamma_plus = 0,
        // with gamma_plus = gamma_minus + K*lambda.
        inverse_constraint_inertia = 0.5 * (
            inverse_constraint_inertia + inverse_constraint_inertia.transpose());
        Eigen::VectorXd required_normal_velocity_change(
            static_cast<Eigen::Index>(constraint_count));
        for (std::size_t row = 0; row < constraint_count; ++row)
        {
            required_normal_velocity_change(static_cast<Eigen::Index>(row)) =
                constraints[row].admissible_direction *
                (constraints[row].target_rate -
                    state.articulation_rates[
                        constraints[row].articulation_state_index]);
        }

        // Active-set solution of the strictly convex complementarity problem.
        // Starting with no contact impulses naturally admits resting contacts that
        // move away when another stop is struck.
        Eigen::VectorXd impulses = Eigen::VectorXd::Zero(
            static_cast<Eigen::Index>(constraint_count));
        std::vector<bool> active(constraint_count, false);
        const double matrix_scale = std::max({
            1.0,
            inverse_constraint_inertia.cwiseAbs().maxCoeff(),
            required_normal_velocity_change.cwiseAbs().maxCoeff()});
        const double complementarity_tolerance = 1.0e-11 * matrix_scale;
        const std::size_t maximum_iterations =
            std::max<std::size_t>(64, 8 * constraint_count * constraint_count + 8);
        std::size_t iterations = 0;

        while (true)
        {
            const Eigen::VectorXd gradient =
                required_normal_velocity_change -
                inverse_constraint_inertia * impulses;
            std::size_t entering = kInvalidIndex;
            double largest_violation = complementarity_tolerance;
            for (std::size_t index = 0; index < constraint_count; ++index)
            {
                if (!active[index] &&
                    gradient(static_cast<Eigen::Index>(index)) > largest_violation)
                {
                    entering = index;
                    largest_violation =
                        gradient(static_cast<Eigen::Index>(index));
                }
            }
            if (entering == kInvalidIndex) break;
            active[entering] = true;

            while (true)
            {
                if (++iterations > maximum_iterations)
                {
                    result.message =
                        "The joint-limit complementarity solve did not converge.";
                    return result;
                }

                std::vector<std::size_t> active_indices;
                for (std::size_t index = 0; index < constraint_count; ++index)
                    if (active[index]) active_indices.push_back(index);

                Eigen::MatrixXd active_matrix(
                    static_cast<Eigen::Index>(active_indices.size()),
                    static_cast<Eigen::Index>(active_indices.size()));
                Eigen::VectorXd active_rhs(
                    static_cast<Eigen::Index>(active_indices.size()));
                for (std::size_t row = 0; row < active_indices.size(); ++row)
                {
                    active_rhs(static_cast<Eigen::Index>(row)) =
                        required_normal_velocity_change(
                            static_cast<Eigen::Index>(active_indices[row]));
                    for (std::size_t column = 0;
                        column < active_indices.size(); ++column)
                    {
                        active_matrix(
                            static_cast<Eigen::Index>(row),
                            static_cast<Eigen::Index>(column)) =
                            inverse_constraint_inertia(
                                static_cast<Eigen::Index>(active_indices[row]),
                                static_cast<Eigen::Index>(active_indices[column]));
                    }
                }

                Eigen::FullPivLU<Eigen::MatrixXd> decomposition(active_matrix);
                decomposition.setThreshold(1.0e-12);
                if (decomposition.rank() <
                    static_cast<Eigen::Index>(active_indices.size()))
                {
                    result.message =
                        "The active joint-limit impulse constraints are dynamically singular.";
                    return result;
                }
                const Eigen::VectorXd active_solution =
                    decomposition.solve(active_rhs);
                if (!active_solution.allFinite())
                {
                    result.message =
                        "The joint-limit impulse solve produced a non-finite result.";
                    return result;
                }

                Eigen::VectorXd candidate = Eigen::VectorXd::Zero(
                    static_cast<Eigen::Index>(constraint_count));
                bool all_positive = true;
                for (std::size_t index = 0;
                    index < active_indices.size(); ++index)
                {
                    const double value =
                        active_solution(static_cast<Eigen::Index>(index));
                    candidate(static_cast<Eigen::Index>(active_indices[index])) = value;
                    if (value <= 0.0) all_positive = false;
                }
                if (all_positive)
                {
                    impulses = std::move(candidate);
                    break;
                }

                double step_fraction = 1.0;
                for (const std::size_t index : active_indices)
                {
                    const double current =
                        impulses(static_cast<Eigen::Index>(index));
                    const double proposed =
                        candidate(static_cast<Eigen::Index>(index));
                    if (proposed <= 0.0 && current > proposed)
                    {
                        step_fraction = std::min(
                            step_fraction,
                            current / (current - proposed));
                    }
                }
                impulses += step_fraction * (candidate - impulses);
                for (const std::size_t index : active_indices)
                {
                    if (impulses(static_cast<Eigen::Index>(index)) <=
                        complementarity_tolerance)
                    {
                        impulses(static_cast<Eigen::Index>(index)) = 0.0;
                        active[index] = false;
                    }
                }
            }
        }

        const Eigen::VectorXd post_constraint_velocities =
            inverse_constraint_inertia * impulses -
            required_normal_velocity_change;
        if (!impulses.allFinite() || !post_constraint_velocities.allFinite() ||
            impulses.minCoeff() < -complementarity_tolerance ||
            post_constraint_velocities.minCoeff() < -complementarity_tolerance)
        {
            result.message =
                "The joint-limit impulse solve violated unilateral contact conditions.";
            return result;
        }

        Vec3d angular_velocity_change;
        std::vector<double> articulation_rate_changes(articulation_count, 0.0);
        result.generalized_impulses.resize(constraint_count, 0.0);
        result.active_constraints = active;
        result.post_constraint_velocities.resize(constraint_count, 0.0);
        for (std::size_t column = 0; column < constraint_count; ++column)
        {
            const double impulse = impulses(static_cast<Eigen::Index>(column));
            result.generalized_impulses[column] = impulse;
            result.post_constraint_velocities[column] =
                post_constraint_velocities(static_cast<Eigen::Index>(column));
            angular_velocity_change += impulse * base_responses[column].angular;
            for (std::size_t index = 0; index < articulation_count; ++index)
            {
                articulation_rate_changes[index] +=
                    impulse * articulation_responses[column][index];
            }
        }

        state.angular_velocity_body_radps += angular_velocity_change;
        for (std::size_t index = 0; index < articulation_count; ++index)
            state.articulation_rates[index] += articulation_rate_changes[index];
        for (std::size_t index = 0; index < constraints.size(); ++index)
        {
            if (!active[index]) continue;
            // Remove only linear-solve roundoff from retained contacts. Released
            // contacts keep the coupled separating velocity produced by the impact.
            const ArticulationVelocityConstraint& constraint = constraints[index];
            state.articulation_rates[constraint.articulation_state_index] =
                constraint.target_rate;
        }

        result.solved = true;
        return result;
    }
}
