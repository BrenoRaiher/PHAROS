// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Simulation/StateRecorder.h"

// Stores each requested output instant in typed and AVS-style numeric forms.

#include "TGSim/Environment/EnvironmentModels.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace tgsim
{
    namespace
    {
        void AppendVector(std::vector<double>& row, const Vec3d& value)
        {
            // Append x, y, z as three consecutive numeric result columns.
            row.push_back(value.x); row.push_back(value.y); row.push_back(value.z);
        }

        void AppendMatrix(std::vector<double>& row, const Mat3d& value)
        {
            // Append a 3x3 matrix in row-major order as nine result columns.
            for (int matrix_row = 0; matrix_row < 3; ++matrix_row)
                for (int column = 0; column < 3; ++column)
                    row.push_back(value.m[matrix_row][column]);
        }

        std::string ColumnSafeName(const std::string& name)
        {
            std::string result = name;
            for (char& character : result)
            {
                const unsigned char value = static_cast<unsigned char>(character);
                character = std::isalnum(value)
                    ? static_cast<char>(std::tolower(value))
                    : '_';
            }
            return result;
        }
    }

    StateRecorder::StateRecorder(const SimulationConfig& config)
        : gravity_(config.gravity),
          thruster_count_(config.vehicle.thrusters.size())
    {
        // Define the stable base-column schema before any samples are recorded.
        column_names_ = {
            "ephemeris_time_tdb_seconds_past_j2000",
            "elapsed_time_seconds",
            "position_icrf_x_m", "position_icrf_y_m", "position_icrf_z_m",
            "velocity_icrf_x_mps", "velocity_icrf_y_mps", "velocity_icrf_z_mps",
            "quaternion_body_to_icrf_w", "quaternion_body_to_icrf_x", "quaternion_body_to_icrf_y", "quaternion_body_to_icrf_z",
            "angular_velocity_body_x_radps", "angular_velocity_body_y_radps", "angular_velocity_body_z_radps",
            "mass_kg",
            "center_of_mass_body_x_m", "center_of_mass_body_y_m", "center_of_mass_body_z_m",
            "inertia_body_xx_kgm2", "inertia_body_xy_kgm2", "inertia_body_xz_kgm2",
            "inertia_body_yx_kgm2", "inertia_body_yy_kgm2", "inertia_body_yz_kgm2",
            "inertia_body_zx_kgm2", "inertia_body_zy_kgm2", "inertia_body_zz_kgm2",
            "force_total_icrf_x_n", "force_total_icrf_y_n", "force_total_icrf_z_n",
            "torque_total_body_x_nm", "torque_total_body_y_nm", "torque_total_body_z_nm",
            "force_gravity_icrf_x_n", "force_gravity_icrf_y_n", "force_gravity_icrf_z_n",
            "force_thrust_icrf_x_n", "force_thrust_icrf_y_n", "force_thrust_icrf_z_n",
            "force_srp_icrf_x_n", "force_srp_icrf_y_n", "force_srp_icrf_z_n",
            "force_aerodynamic_icrf_x_n", "force_aerodynamic_icrf_y_n", "force_aerodynamic_icrf_z_n",
            "torque_gravity_body_x_nm", "torque_gravity_body_y_nm", "torque_gravity_body_z_nm",
            "torque_thrust_body_x_nm", "torque_thrust_body_y_nm", "torque_thrust_body_z_nm",
            "torque_srp_body_x_nm", "torque_srp_body_y_nm", "torque_srp_body_z_nm",
            "torque_aerodynamic_body_x_nm", "torque_aerodynamic_body_y_nm", "torque_aerodynamic_body_z_nm",
            "torque_control_body_x_nm", "torque_control_body_y_nm", "torque_control_body_z_nm",
            "mass_rate_kgps", "visible_sun_fraction", "aerodynamics_outside_validity",
            "aerodynamic_dynamic_pressure_pa",
            "aerodynamic_molecular_speed_ratio",
            "aerodynamic_knudsen_number",
            "aerodynamic_force_coefficient_body_x", "aerodynamic_force_coefficient_body_y",
            "aerodynamic_force_coefficient_body_z",
            "aerodynamic_moment_coefficient_about_cm_body_x",
            "aerodynamic_moment_coefficient_about_cm_body_y",
            "aerodynamic_moment_coefficient_about_cm_body_z",
            "aerodynamic_database_used", "aerodynamic_fallback_used",
            "base_origin_acceleration_body_x_mps2", "base_origin_acceleration_body_y_mps2",
            "base_origin_acceleration_body_z_mps2",
            "linear_momentum_icrf_x_kgmps", "linear_momentum_icrf_y_kgmps", "linear_momentum_icrf_z_kgmps",
            "angular_momentum_about_cm_icrf_x_kgm2ps", "angular_momentum_about_cm_icrf_y_kgm2ps",
            "angular_momentum_about_cm_icrf_z_kgm2ps", "multibody_solve_succeeded"
        };

        for (std::size_t index = 0; index < thruster_count_; ++index)
            column_names_.push_back("thruster_thrust_" + std::to_string(index) + "_n");
        for (std::size_t index = 0; index < config.initial_state.variable_component_masses_kg.size(); ++index)
            column_names_.push_back("variable_component_mass_" + std::to_string(index) + "_kg");
        for (std::size_t index = 0; index < config.initial_state.articulation_coordinates.size(); ++index)
            column_names_.push_back("articulation_coordinate_" + std::to_string(index) + "_rad_or_m");
        for (std::size_t index = 0; index < config.initial_state.articulation_rates.size(); ++index)
            column_names_.push_back("articulation_rate_" + std::to_string(index) + "_radps_or_mps");
        for (std::size_t index = 0; index < config.initial_state.internal_angular_momenta_nms.size(); ++index)
            column_names_.push_back("reaction_wheel_momentum_" + std::to_string(index) + "_nms");
        for (std::size_t index = 0; index < config.initial_state.articulation_coordinates.size(); ++index)
        {
            column_names_.push_back("applied_joint_effort_" + std::to_string(index) + "_nm_or_n");
            column_names_.push_back("joint_constraint_effort_" + std::to_string(index) + "_nm_or_n");
        }
        for (const ComponentDefinition& component : config.vehicle.components)
        {
            const std::string prefix = "component_" +
                ColumnSafeName(component.name) + "_";
            column_names_.push_back(prefix + "origin_body_x_m");
            column_names_.push_back(prefix + "origin_body_y_m");
            column_names_.push_back(prefix + "origin_body_z_m");
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 3; ++column)
                    column_names_.push_back(prefix + "rotation_component_to_body_" +
                        std::to_string(row) + std::to_string(column));
        }
        for (const GravityBody& body : gravity_.bodies)
        {
            const std::string prefix = "body_" + ColumnSafeName(body.name) + "_";
            column_names_.push_back(prefix + "position_icrf_x_m");
            column_names_.push_back(prefix + "position_icrf_y_m");
            column_names_.push_back(prefix + "position_icrf_z_m");
            column_names_.push_back(prefix + "velocity_icrf_x_mps");
            column_names_.push_back(prefix + "velocity_icrf_y_mps");
            column_names_.push_back(prefix + "velocity_icrf_z_mps");
            column_names_.push_back(prefix + "reference_radius_m");
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 3; ++column)
                    column_names_.push_back(prefix + "rotation_body_fixed_to_icrf_" +
                        std::to_string(row) + std::to_string(column));
        }
    }

    void StateRecorder::Record(const SpacecraftState& state, const DynamicsEvaluation& evaluation)
    {
        // Preserve a typed sample for C++ consumers and a flat row for serialization/Unreal.
        TelemetrySample sample;
        sample.state = state;
        sample.applied_force_torque = evaluation.applied_force_torque;
        sample.center_of_mass_body_m = evaluation.center_of_mass_body_m;
        sample.inertia_body_kgm2 = evaluation.inertia_body_kgm2;
        sample.base_origin_acceleration_body_mps2 = evaluation.base_origin_acceleration_body_mps2;
        sample.total_linear_momentum_icrf_kgmps = evaluation.total_linear_momentum_icrf_kgmps;
        sample.total_angular_momentum_about_cm_icrf_kgm2ps =
            evaluation.total_angular_momentum_about_cm_icrf_kgm2ps;
        sample.applied_joint_efforts = evaluation.applied_joint_efforts;
        sample.thruster_thrusts_n = evaluation.thruster_thrusts_n;
        sample.joint_constraint_efforts = evaluation.joint_constraint_efforts;
        const std::size_t component_pose_count = std::min(
            evaluation.component_origins_body_m.size(),
            evaluation.component_to_body_rotations.size());
        for (std::size_t index = 0; index < component_pose_count; ++index)
        {
            sample.component_poses.push_back({
                evaluation.component_origins_body_m[index],
                evaluation.component_to_body_rotations[index]});
        }
        for (const GravityBody& body : gravity_.bodies)
        {
            const BodyState body_state = ResolveBodyState(
                body, gravity_, state.ephemeris_time_tdb_seconds);
            const Mat3d body_fixed_to_icrf = ResolveBodyFixedToIcrf(
                body, gravity_, state.ephemeris_time_tdb_seconds);
            sample.celestial_bodies.push_back({
                body.name,
                body_state.position_icrf_m,
                body_state.velocity_icrf_mps,
                body.reference_radius_m,
                body_fixed_to_icrf});
        }
        sample.multibody_solve_succeeded = evaluation.multibody_solve_succeeded;
        samples_.push_back(sample);

        const ForceTorqueSample& loads = evaluation.applied_force_torque;
        std::vector<double> row;
        row.reserve(column_names_.size());
        row.push_back(state.ephemeris_time_tdb_seconds);
        row.push_back(state.elapsed_time_seconds);
        AppendVector(row, state.position_icrf_m);
        AppendVector(row, state.velocity_icrf_mps);
        row.push_back(state.attitude_body_to_icrf.w);
        row.push_back(state.attitude_body_to_icrf.x);
        row.push_back(state.attitude_body_to_icrf.y);
        row.push_back(state.attitude_body_to_icrf.z);
        AppendVector(row, state.angular_velocity_body_radps);
        row.push_back(state.mass_kg);
        AppendVector(row, evaluation.center_of_mass_body_m);
        AppendMatrix(row, evaluation.inertia_body_kgm2);
        AppendVector(row, loads.force_icrf_n);
        AppendVector(row, loads.torque_body_nm);
        AppendVector(row, loads.gravity_force_icrf_n);
        AppendVector(row, loads.thrust_force_icrf_n);
        AppendVector(row, loads.solar_radiation_force_icrf_n);
        AppendVector(row, loads.aerodynamic_force_icrf_n);
        AppendVector(row, loads.gravity_torque_body_nm);
        AppendVector(row, loads.thrust_torque_body_nm);
        AppendVector(row, loads.solar_radiation_torque_body_nm);
        AppendVector(row, loads.aerodynamic_torque_body_nm);
        AppendVector(row, loads.control_torque_body_nm);
        row.push_back(loads.mass_rate_kgps);
        row.push_back(loads.visible_sun_fraction);
        row.push_back(loads.aerodynamics_outside_validity ? 1.0 : 0.0);
        row.push_back(loads.aerodynamic_dynamic_pressure_pa);
        row.push_back(loads.aerodynamic_molecular_speed_ratio);
        row.push_back(loads.aerodynamic_knudsen_number);
        AppendVector(row, loads.aerodynamic_force_coefficients_body);
        AppendVector(row, loads.aerodynamic_moment_coefficients_body_about_cm);
        row.push_back(loads.aerodynamic_database_used ? 1.0 : 0.0);
        row.push_back(loads.aerodynamic_fallback_used ? 1.0 : 0.0);
        AppendVector(row, evaluation.base_origin_acceleration_body_mps2);
        AppendVector(row, evaluation.total_linear_momentum_icrf_kgmps);
        AppendVector(row, evaluation.total_angular_momentum_about_cm_icrf_kgm2ps);
        row.push_back(evaluation.multibody_solve_succeeded ? 1.0 : 0.0);
        for (std::size_t index = 0; index < thruster_count_; ++index)
        {
            row.push_back(index < evaluation.thruster_thrusts_n.size()
                ? evaluation.thruster_thrusts_n[index]
                : 0.0);
        }
        row.insert(row.end(), state.variable_component_masses_kg.begin(), state.variable_component_masses_kg.end());
        row.insert(row.end(), state.articulation_coordinates.begin(), state.articulation_coordinates.end());
        row.insert(row.end(), state.articulation_rates.begin(), state.articulation_rates.end());
        row.insert(row.end(), state.internal_angular_momenta_nms.begin(), state.internal_angular_momenta_nms.end());
        for (std::size_t index = 0; index < evaluation.applied_joint_efforts.size(); ++index)
        {
            row.push_back(evaluation.applied_joint_efforts[index]);
            row.push_back(index < evaluation.joint_constraint_efforts.size()
                ? evaluation.joint_constraint_efforts[index]
                : 0.0);
        }
        for (const ComponentPoseTelemetry& pose : sample.component_poses)
        {
            AppendVector(row, pose.origin_body_m);
            AppendMatrix(row, pose.component_to_body);
        }
        for (const CelestialBodyTelemetry& body : sample.celestial_bodies)
        {
            AppendVector(row, body.position_icrf_m);
            AppendVector(row, body.velocity_icrf_mps);
            row.push_back(body.reference_radius_m);
            AppendMatrix(row, body.body_fixed_to_icrf);
        }
        solution_array_.push_back(std::move(row));
    }

    SimulationResult StateRecorder::BuildResult(bool success, const std::string& message) const
    {
        // Package all accumulated data without performing post-processing or frame conversion.
        SimulationResult result;
        result.success = success;
        result.message = message;
        result.samples = samples_;
        result.column_names = column_names_;
        result.solution_array = solution_array_;
        return result;
    }
}
