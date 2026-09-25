// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

// Contract implemented by the simulation user's own C++ control system.
// TGSimCore supplies current truth/status data and owns command construction,
// validation, limiting, and routing. No guidance or control law lives here.

#include "TGSim/Core/ModelTypes.h"
#include "TGSim/Core/SimulationConfig.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace tgsim
{
    struct ComponentControlState
    {
        std::size_t component_index = kInvalidIndex;
        double current_mass_kg = 0.0;
        Vec3d origin_body_m;
        Mat3d component_to_body = Mat3d::Identity();
    };

    struct ThrusterControlState
    {
        std::size_t thruster_index = kInvalidIndex;
        bool firing_window_open = false;
        bool has_propellant_component = false;
        bool propellant_available = true;
        double current_propellant_component_mass_kg = 0.0;
    };

    struct JointControlState
    {
        std::size_t articulation_state_index = kInvalidIndex;
        std::size_t child_component_index = kInvalidIndex;
        std::size_t local_dof_index = kInvalidIndex;
        double coordinate = 0.0;
        double rate = 0.0;
        bool at_lower_limit = false;
        bool at_upper_limit = false;
    };

    struct ReactionWheelControlState
    {
        std::size_t wheel_index = kInvalidIndex;
        double momentum_nms = 0.0;
        double maximum_momentum_nms = 0.0;
        bool saturated_negative = false;
        bool saturated_positive = false;
    };

    struct CelestialBodyControlState
    {
        std::string name;
        Vec3d position_icrf_m;
        Vec3d velocity_icrf_mps;
        Mat3d body_fixed_to_icrf = Mat3d::Identity();
    };

    /// Read-only snapshot passed to the user's control function at every ODE
    /// right-hand-side evaluation, including intermediate RK stages.
    struct ControlInput
    {
        double ephemeris_time_tdb_seconds = 0.0;
        double elapsed_time_seconds = 0.0;
        const SpacecraftState& spacecraft_state;
        const SimulationConfig& configuration;
        Vec3d center_of_mass_body_m;
        Mat3d inertia_body_kgm2 = Mat3d::Identity();
        const std::vector<ComponentControlState>& components;
        const std::vector<ThrusterControlState>& thrusters;
        const std::vector<JointControlState>& joints;
        const std::vector<ReactionWheelControlState>& reaction_wheels;
        const std::vector<CelestialBodyControlState>& celestial_bodies;
    };

    /// Backend-owned output supplied already sized and initialized to zero.
    /// Setters reject invalid indices/non-finite values. Physical actuator
    /// limits are applied afterward by the corresponding backend model.
    class ControlCommandWriter
    {
    public:
        ControlCommandWriter(
            std::size_t thruster_count,
            std::size_t joint_count,
            std::size_t reaction_wheel_count)
        {
            command_.thruster_throttles.assign(thruster_count, 0.0);
            command_.thruster_specific_impulses_seconds.assign(
                thruster_count, 0.0);
            command_.thruster_mass_flow_derivatives_kgps2.assign(
                thruster_count, std::nullopt);
            command_.joint_efforts.assign(joint_count, 0.0);
            command_.wheel_momentum_rates_nm.assign(
                reaction_wheel_count, 0.0);
        }

        /// Defines one commanded thruster's complete instantaneous performance.
        /// A positive throttle requires positive Isp because the backend must be
        /// able to evaluate m_dot = -T/(Isp*g0). Prescribed thrusters ignore this.
        bool SetThrusterCommand(
            std::size_t index,
            double throttle,
            double specific_impulse_seconds)
        {
            if (index >= command_.thruster_throttles.size() ||
                !std::isfinite(throttle) ||
                !std::isfinite(specific_impulse_seconds) ||
                specific_impulse_seconds < 0.0 ||
                (throttle > 0.0 && specific_impulse_seconds <= 0.0))
            {
                return false;
            }
            command_.thruster_throttles[index] =
                std::clamp(throttle, 0.0, 1.0);
            command_.thruster_specific_impulses_seconds[index] =
                specific_impulse_seconds;
            return true;
        }

        /// Supplies d/dt of the actual applied outward discharge
        /// q=T/(g0*Isp), in kg/s^2, on the current smooth command branch.
        /// For state-dependent commands this is the total derivative. The
        /// backend does not infer or numerically differentiate omitted values.
        bool SetThrusterMassFlowDerivative(
            std::size_t index,
            double mass_flow_derivative_kgps2)
        {
            if (index >=
                    command_.thruster_mass_flow_derivatives_kgps2.size() ||
                !std::isfinite(mass_flow_derivative_kgps2))
            {
                return false;
            }
            command_.thruster_mass_flow_derivatives_kgps2[index] =
                mass_flow_derivative_kgps2;
            return true;
        }

        bool SetJointEffort(std::size_t index, double effort_nm_or_n)
        {
            if (index >= command_.joint_efforts.size() ||
                !std::isfinite(effort_nm_or_n)) return false;
            command_.joint_efforts[index] = effort_nm_or_n;
            return true;
        }

        bool SetReactionWheelMomentumRate(
            std::size_t index,
            double momentum_rate_nm)
        {
            if (index >= command_.wheel_momentum_rates_nm.size() ||
                !std::isfinite(momentum_rate_nm)) return false;
            command_.wheel_momentum_rates_nm[index] = momentum_rate_nm;
            return true;
        }

        bool SetExternalTorqueBody(const Vec3d& torque_body_nm)
        {
            if (!std::isfinite(torque_body_nm.x) ||
                !std::isfinite(torque_body_nm.y) ||
                !std::isfinite(torque_body_nm.z)) return false;
            command_.external_torque_body_nm = torque_body_nm;
            return true;
        }

        const ControlCommand& Command() const { return command_; }

    private:
        ControlCommand command_;
    };

    class IController
    {
    public:
        virtual ~IController() = default;

        /// Returns the first known time at which the controller command is
        /// discontinuous after current_elapsed_time_seconds. Controllers with
        /// only continuous commands need not override this hook. The propagation
        /// engine lands on announced events from the left
        /// and starts the following step with the post-event command.
        virtual double NextDiscontinuityElapsedTime(
            double current_elapsed_time_seconds) const
        {
            (void)current_elapsed_time_seconds;
            return std::numeric_limits<double>::infinity();
        }

        /// This is the only function the control-system author implements.
        /// Commands left unwritten remain zero for this evaluation.
        virtual void ComputeControl(
            const ControlInput& input,
            ControlCommandWriter& output) const = 0;
    };
}
