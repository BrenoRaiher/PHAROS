// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Validation/BackendValidationSuite.h"

// Small analytic and conservation cases for the rigid and articulated backend.

#include "TGSim/Control/IController.h"
#include "TGSim/Environment/EnvironmentModels.h"
#include "TGSim/Dynamics/FloatingBaseTreeDynamics.h"
#include "TGSim/Dynamics/General6DofDynamics.h"
#include "TGSim/Forces/AerodynamicCoefficientDatabase.h"
#include "TGSim/Forces/GravityModel.h"
#include "TGSim/Forces/PropulsionModel.h"
#include "TGSim/Integrators/AdaptiveDormandPrince54.h"
#include "TGSim/Scenario/CelestialCatalog.h"
#include "TGSim/Simulation/SimulationConfigBuilder.h"
#include "TGSim/Simulation/SimulationEngine.h"
#include "TGSim/Vehicle/MassProperties.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tgsim
{
    namespace
    {
        // Test-only controller that supplies a known generalized force/torque vector.
        // Analytic joint-reaction cases use it to make tau an exact, state-independent input.
        class ConstantJointEffortController final : public IController
        {
        public:
            explicit ConstantJointEffortController(std::vector<double> efforts)
                : efforts_(std::move(efforts))
            {
            }

            void ComputeControl(
                const ControlInput&,
                ControlCommandWriter& output) const override
            {
                for (std::size_t index = 0; index < efforts_.size(); ++index)
                    output.SetJointEffort(index, efforts_[index]);
            }

        private:
            std::vector<double> efforts_;
        };

        // Test-only conservative controller used by the long nested-tree propagation.
        // Internal spring efforts exchange energy/momentum between bodies without applying
        // an external spacecraft wrench.
        class JointSpringController final : public IController
        {
        public:
            explicit JointSpringController(std::vector<double> stiffnesses)
                : stiffnesses_(std::move(stiffnesses))
            {
            }

            void ComputeControl(
                const ControlInput& input,
                ControlCommandWriter& output) const override
            {
                for (std::size_t index = 0;
                    index < input.spacecraft_state.articulation_coordinates.size();
                    ++index)
                {
                    const double stiffness = index < stiffnesses_.size()
                        ? stiffnesses_[index]
                        : 0.0;
                    // Basic internal spring: tau_or_force = -k*eta.
                    output.SetJointEffort(
                        index,
                        -stiffness *
                            input.spacecraft_state.articulation_coordinates[index]);
                }
            }

        private:
            std::vector<double> stiffnesses_;
        };

        class ConstantBodyTorqueController final : public IController
        {
        public:
            explicit ConstantBodyTorqueController(const Vec3d& torque_body_nm)
                : torque_body_nm_(torque_body_nm)
            {
            }

            void ComputeControl(
                const ControlInput&,
                ControlCommandWriter& output) const override
            {
                output.SetExternalTorqueBody(torque_body_nm_);
            }

        private:
            Vec3d torque_body_nm_;
        };

        class TimedBodyTorqueController final : public IController
        {
        public:
            TimedBodyTorqueController(
                const Vec3d& torque_body_nm,
                double cutoff_elapsed_time_seconds)
                : torque_body_nm_(torque_body_nm),
                  cutoff_elapsed_time_seconds_(cutoff_elapsed_time_seconds)
            {
            }

            double NextDiscontinuityElapsedTime(
                double current_elapsed_time_seconds) const override
            {
                return current_elapsed_time_seconds <
                        cutoff_elapsed_time_seconds_
                    ? cutoff_elapsed_time_seconds_
                    : std::numeric_limits<double>::infinity();
            }

            void ComputeControl(
                const ControlInput& input,
                ControlCommandWriter& output) const override
            {
                if (input.elapsed_time_seconds <
                    cutoff_elapsed_time_seconds_)
                {
                    output.SetExternalTorqueBody(torque_body_nm_);
                }
            }

        private:
            Vec3d torque_body_nm_;
            double cutoff_elapsed_time_seconds_ = 0.0;
        };

        class TimedThrusterCommandController final : public IController
        {
        public:
            TimedThrusterCommandController(
                std::size_t thruster_index,
                double start_elapsed_time_seconds,
                double cutoff_elapsed_time_seconds,
                double throttle,
                double specific_impulse_seconds)
                : thruster_index_(thruster_index),
                  start_elapsed_time_seconds_(start_elapsed_time_seconds),
                  cutoff_elapsed_time_seconds_(cutoff_elapsed_time_seconds),
                  throttle_(throttle),
                  specific_impulse_seconds_(specific_impulse_seconds)
            {
            }

            double NextDiscontinuityElapsedTime(
                double current_elapsed_time_seconds) const override
            {
                if (current_elapsed_time_seconds < start_elapsed_time_seconds_)
                    return start_elapsed_time_seconds_;
                if (current_elapsed_time_seconds < cutoff_elapsed_time_seconds_)
                    return cutoff_elapsed_time_seconds_;
                return std::numeric_limits<double>::infinity();
            }

            void ComputeControl(
                const ControlInput& input,
                ControlCommandWriter& output) const override
            {
                if (input.elapsed_time_seconds >= start_elapsed_time_seconds_ &&
                    input.elapsed_time_seconds < cutoff_elapsed_time_seconds_)
                {
                    output.SetThrusterCommand(
                        thruster_index_, throttle_, specific_impulse_seconds_);
                }
            }

        private:
            std::size_t thruster_index_ = 0;
            double start_elapsed_time_seconds_ = 0.0;
            double cutoff_elapsed_time_seconds_ = 0.0;
            double throttle_ = 0.0;
            double specific_impulse_seconds_ = 0.0;
        };

        class ConstantWheelRateController final : public IController
        {
        public:
            explicit ConstantWheelRateController(double momentum_rate_nm)
                : momentum_rate_nm_(momentum_rate_nm)
            {
            }

            void ComputeControl(
                const ControlInput&,
                ControlCommandWriter& output) const override
            {
                output.SetReactionWheelMomentumRate(0, momentum_rate_nm_);
            }

        private:
            double momentum_rate_nm_ = 0.0;
        };

        class ConstantThrusterCommandController final : public IController
        {
        public:
            ConstantThrusterCommandController(
                std::size_t thruster_index,
                double throttle,
                double specific_impulse_seconds)
                : thruster_index_(thruster_index),
                  throttle_(throttle),
                  specific_impulse_seconds_(specific_impulse_seconds)
            {
            }

            void ComputeControl(
                const ControlInput&,
                ControlCommandWriter& output) const override
            {
                output.SetThrusterCommand(
                    thruster_index_,
                    throttle_,
                    specific_impulse_seconds_);
            }

        private:
            std::size_t thruster_index_ = 0;
            double throttle_ = 0.0;
            double specific_impulse_seconds_ = 0.0;
        };

        class ThrusterCommandWithMassFlowDerivativeController final
            : public IController
        {
        public:
            ThrusterCommandWithMassFlowDerivativeController(
                std::size_t thruster_index,
                double throttle,
                double specific_impulse_seconds,
                std::optional<double> mass_flow_derivative_kgps2)
                : thruster_index_(thruster_index),
                  throttle_(throttle),
                  specific_impulse_seconds_(specific_impulse_seconds),
                  mass_flow_derivative_kgps2_(
                      mass_flow_derivative_kgps2)
            {
            }

            void ComputeControl(
                const ControlInput&,
                ControlCommandWriter& output) const override
            {
                output.SetThrusterCommand(
                    thruster_index_,
                    throttle_,
                    specific_impulse_seconds_);
                if (mass_flow_derivative_kgps2_.has_value())
                {
                    output.SetThrusterMassFlowDerivative(
                        thruster_index_,
                        *mass_flow_derivative_kgps2_);
                }
            }

        private:
            std::size_t thruster_index_ = 0;
            double throttle_ = 0.0;
            double specific_impulse_seconds_ = 0.0;
            std::optional<double> mass_flow_derivative_kgps2_;
        };

        class FixedGravityMetadataProvider final : public IEphemerisProvider
        {
        public:
            bool TryGetBodyState(
                const std::string&,
                double,
                BodyState&) const override
            {
                return false;
            }

            bool TryGetBodyGravityMetadata(
                const std::string& body_name,
                BodyGravityMetadata& metadata) const override
            {
                if (body_name != "Metadata body") return false;
                metadata.gravitational_parameter_m3ps2 = 1234.5;
                metadata.reference_radius_m = 6789.0;
                return true;
            }
        };

        class UniformTranslationDynamics final : public IDynamicsModel
        {
        public:
            DynamicsEvaluation ComputeDynamics(
                double,
                const SpacecraftState& state,
                const SimulationConfig&) const override
            {
                DynamicsEvaluation result;
                result.derivative.position_rate_mps = state.velocity_icrf_mps;
                return result;
            }
        };

        class ImmediateCancellationObserver final : public ISimulationObserver
        {
        public:
            void OnProgress(double, const std::string&) override {}
            void OnLog(const std::string&) override {}
            bool IsCancellationRequested() const override { return true; }
        };

        ComponentDefinition MakeComponent(
            const std::string& name,
            double mass_kg,
            const Vec3d& center_of_mass_m,
            const Vec3d& centroidal_inertia_diagonal_kgm2)
        {
            // Build the minimum physically valid rigid component shared by the scenarios.
            // The supplied inertia is centroidal and diagonal in the component's own axes.
            ComponentDefinition component;
            component.name = name;
            component.initial_mass_kg = mass_kg;
            component.minimum_mass_kg = mass_kg;
            component.center_of_mass_component_m = center_of_mass_m;
            component.inertia_centroid_component_kgm2 = Mat3d::Diagonal(centroidal_inertia_diagonal_kgm2);
            return component;
        }

        ArticulationDof MakeDof(
            const std::string& name,
            ArticulationMotion motion,
            const Vec3d& axis)
        {
            // Build one scalar revolute/prismatic test DOF with broad finite limits. The
            // validation scenarios override state/effort as needed but share this indexing.
            ArticulationDof dof;
            dof.name = name;
            dof.motion = motion;
            dof.axis_joint = axis;
            dof.limits.minimum_coordinate = motion == ArticulationMotion::Rotation ? -kPi : -2.0;
            dof.limits.maximum_coordinate = motion == ArticulationMotion::Rotation ? kPi : 2.0;
            dof.limits.maximum_absolute_rate = 10.0;
            dof.limits.maximum_absolute_effort = 20.0;
            return dof;
        }

        ValidationCheck Check(
            const std::string& name,
            double error,
            double tolerance,
            const std::string& message)
        {
            // Convert a measured nonnegative error and allowed tolerance into the common
            // report format. Non-finite results always fail, even with a large tolerance.
            ValidationCheck check;
            check.name = name;
            check.error = error;
            check.tolerance = tolerance;
            check.passed = std::isfinite(error) && error <= tolerance;
            check.message = message;
            return check;
        }

        SimulationRequest MakeRigidRequest()
        {
            // Base request for instantaneous single-rigid-body tests. With one component
            // and no joints, floating-base ABA must reduce exactly to Euler's equation.
            SimulationRequest request;
            request.scenario_name = "Rigid body validation";
            request.vehicle.components.push_back(MakeComponent(
                "Main body", 10.0, Vec3d::Zero(), {2.0, 3.0, 4.0}));
            request.initial_state.attitude_body_to_icrf = Quatd::Identity();
            request.initial_state.angular_velocity_body_radps = {0.2, -0.1, 0.3};
            return request;
        }

        SimulationRequest MakeNestedFreeFlightRequest()
        {
            // Build a force-free three-body tree containing both supported joint types:
            // free bus -> revolute arm -> prismatic payload. The spring controller excites
            // internal motion so the full propagation can test momentum conservation.
            SimulationRequest request;
            request.scenario_name = "Nested revolute-prismatic free flight";
            request.initial_state.position_icrf_m = {20.0, -10.0, 5.0};
            request.initial_state.velocity_icrf_mps = {2.0, -1.0, 0.5};
            request.initial_state.angular_velocity_body_radps = {0.05, -0.02, 0.03};
            request.initial_state.articulation_coordinates = {0.25, 0.15};
            request.initial_state.articulation_rates = {0.0, 0.0};

            request.vehicle.components.push_back(MakeComponent(
                "Main body", 8.0, Vec3d::Zero(), {3.0, 4.0, 5.0}));

            ComponentDefinition arm = MakeComponent(
                "Rotating arm", 2.0, {0.45, 0.0, 0.0}, {0.08, 0.35, 0.35});
            arm.parent_component_index = 0;
            arm.articulation_to_parent.parent_anchor_component_m = {0.8, 0.0, 0.0};
            arm.articulation_to_parent.dofs.push_back(MakeDof(
                "arm_hinge", ArticulationMotion::Rotation, Vec3d::UnitZ()));
            request.vehicle.components.push_back(arm);

            ComponentDefinition slider = MakeComponent(
                "Sliding payload", 1.0, {0.15, 0.0, 0.0}, {0.04, 0.05, 0.06});
            slider.parent_component_index = 1;
            slider.articulation_to_parent.parent_anchor_component_m = {0.9, 0.0, 0.0};
            slider.articulation_to_parent.dofs.push_back(MakeDof(
                "payload_slider", ArticulationMotion::Translation, Vec3d::UnitX()));
            request.vehicle.components.push_back(slider);

            request.control.controller = std::make_shared<JointSpringController>(
                std::vector<double>{1.5, 4.0});
            request.final_ephemeris_time_tdb_seconds = 10.0;
            request.maximum_integrator_step_seconds = 0.01;
            request.output_mode = OutputMode::FixedInterval;
            request.output_step_seconds = 0.1;
            return request;
        }

        double RelativeDifference(const Vec3d& final_value, const Vec3d& initial_value)
        {
            // Scale conservation error by the initial magnitude, but use one as a floor so
            // a nearly zero conserved vector is still measured by an absolute norm.
            return (final_value - initial_value).Norm() / std::max(1.0, initial_value.Norm());
        }

        double MaximumAbsoluteDifference(const Mat3d& a, const Mat3d& b)
        {
            double error = 0.0;
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 3; ++column)
                    error = std::max(error, std::abs(a.m[row][column] - b.m[row][column]));
            return error;
        }
    }

    ValidationReport RunBackendValidationSuite()
    {
        // Execute deterministic backend-only checks. Early blocks compare one RHS evaluation
        // with closed-form accelerations; later blocks run SimulationEngine to exercise RK4,
        // recording, repeated ABA solves, and conservation over finite time.
        ValidationReport report;
        report.scenario_name = "TGSimCore multibody and orbit validation";

        {
            // Case 1: one free rigid body under a known external moment. This isolates the
            // root solve and verifies that ABA reduces to Euler rotation with no joints.
            SimulationRequest request = MakeRigidRequest();
            const Vec3d applied_torque_body_nm{1.0, 2.0, -0.5};
            request.control.controller =
                std::make_shared<ConstantBodyTorqueController>(
                    applied_torque_body_nm);
            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            const DynamicsEvaluation evaluation =
                General6DofDynamics(config).ComputeDynamics(0.0, config.initial_state, config);
            const Vec3d omega = config.initial_state.angular_velocity_body_radps;
            const Mat3d inertia = request.vehicle.components[0].inertia_centroid_component_kgm2;
            // Euler equation: alpha = I^-1 [M - omega x (I omega)].
            const Vec3d expected = inertia.Inverse() * (
                applied_torque_body_nm - Cross(omega, inertia * omega));
            report.checks.push_back(Check(
                "Rigid body matches Euler equation",
                (evaluation.derivative.angular_acceleration_body_radps2 - expected).Norm(),
                1.0e-12,
                "Single-component Featherstone result must reduce to Euler rotation."));
        }

        {
            // Case 2: coaxial bus and rotor with one revolute DOF and no external moment.
            // A known internal torque has a closed-form bus counter-rotation and relative qdd.
            SimulationRequest request;
            request.scenario_name = "Analytic revolute reaction";
            request.vehicle.components.push_back(MakeComponent(
                "Main body", 5.0, Vec3d::Zero(), {1.0, 1.0, 2.0}));
            ComponentDefinition rotor = MakeComponent(
                "Hinged rotor", 1.0, Vec3d::Zero(), {0.3, 0.3, 0.5});
            rotor.parent_component_index = 0;
            rotor.articulation_to_parent.dofs.push_back(MakeDof(
                "rotor_hinge", ArticulationMotion::Rotation, Vec3d::UnitZ()));
            request.vehicle.components.push_back(rotor);
            request.control.controller = std::make_shared<ConstantJointEffortController>(
                std::vector<double>{0.4});

            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            const DynamicsEvaluation evaluation =
                General6DofDynamics(config).ComputeDynamics(0.0, config.initial_state, config);
            // Known hinge torque tau=0.4 N m:
            // alpha_base=-tau/I_base=-0.2; qdd=tau(1/I_rotor+1/I_base)=1.
            const double expected_base_alpha = -0.4 / 2.0;
            report.checks.push_back(Check(
                "Revolute joint produces base counter-rotation",
                std::abs(evaluation.derivative.angular_acceleration_body_radps2.z - expected_base_alpha),
                1.0e-12,
                "An internal hinge torque must produce equal system counter-reaction."));
            const double solved_acceleration = evaluation.derivative.articulation_accelerations.empty()
                ? 0.0
                : evaluation.derivative.articulation_accelerations[0];
            report.checks.push_back(Check(
                "Revolute joint acceleration follows applied torque",
                std::abs(solved_acceleration - 1.0),
                1.0e-12,
                "Forward dynamics must solve qddot rather than accept it as input."));
        }

        {
            // Case 3: collinear bus and sliding point payload. This is the translational
            // analogue of Case 2 and isolates a prismatic S=[0;axis] implementation.
            SimulationRequest request;
            request.scenario_name = "Analytic prismatic reaction";
            request.vehicle.components.push_back(MakeComponent(
                "Main body", 4.0, Vec3d::Zero(), {1.0, 1.0, 1.0}));
            ComponentDefinition payload = MakeComponent(
                "Sliding payload", 1.0, Vec3d::Zero(), {0.1, 0.1, 0.1});
            payload.parent_component_index = 0;
            payload.articulation_to_parent.dofs.push_back(MakeDof(
                "payload_slider", ArticulationMotion::Translation, Vec3d::UnitX()));
            request.vehicle.components.push_back(payload);
            request.control.controller = std::make_shared<ConstantJointEffortController>(
                std::vector<double>{1.6});

            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            const DynamicsEvaluation evaluation =
                General6DofDynamics(config).ComputeDynamics(0.0, config.initial_state, config);
            // Known slider force F=1.6 N:
            // a_base=-F/m_base=-0.4; qdd=F(1/m_payload+1/m_base)=2.
            const double expected_base_acceleration = -1.6 / 4.0;
            report.checks.push_back(Check(
                "Prismatic joint produces base translation reaction",
                std::abs(evaluation.base_origin_acceleration_body_mps2.x - expected_base_acceleration),
                1.0e-12,
                "An internal slider force must leave total-CM acceleration at zero."));
            const double solved_acceleration = evaluation.derivative.articulation_accelerations.empty()
                ? 0.0
                : evaluation.derivative.articulation_accelerations[0];
            report.checks.push_back(Check(
                "Prismatic joint acceleration follows applied force",
                std::abs(solved_acceleration - 2.0),
                1.0e-12,
                "Forward dynamics must solve slider acceleration from the known force."));
        }

        {
            // Case 4: apply a known moment through a thruster attached to the child body.
            // This verifies that ComponentLoad reaches the child ABA node instead of being
            // collapsed prematurely into a root-only torque.
            SimulationRequest request;
            request.scenario_name = "Child-mounted external load";
            ComponentDefinition main_body = MakeComponent(
                "Main body", 5.0, Vec3d::Zero(), {1.0, 1.0, 2.0});
            main_body.minimum_mass_kg = 4.0;
            main_body.variable_mass_state_index = 0;
            request.vehicle.components.push_back(main_body);
            ComponentDefinition rotor = MakeComponent(
                "Hinged rotor", 1.0, Vec3d::Zero(), {0.1, 0.1, 0.5});
            rotor.parent_component_index = 0;
            rotor.articulation_to_parent.dofs.push_back(MakeDof(
                "rotor_hinge", ArticulationMotion::Rotation, Vec3d::UnitZ()));
            request.vehicle.components.push_back(rotor);

            ThrusterDefinition thruster;
            thruster.name = "Rotor test thruster";
            thruster.component_index = 1;
            thruster.propellant_component_index = 0;
            thruster.application_point_component_m = {1.0, 0.0, 0.0};
            thruster.direction_component = Vec3d::UnitY();
            thruster.mode = ThrusterMode::Commanded;
            thruster.maximum_thrust_n = 1.0;
            request.vehicle.thrusters.push_back(thruster);
            request.control.controller =
                std::make_shared<ConstantThrusterCommandController>(
                    0, 1.0, 100.0);

            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            const DynamicsEvaluation evaluation =
                General6DofDynamics(config).ComputeDynamics(0.0, config.initial_state, config);
            // The passive hinge transmits no torque. A 1 N m child load therefore
            // accelerates only the child inertially; the main body remains unaccelerated.
            const double expected_base_alpha = 0.0;
            report.checks.push_back(Check(
                "Child-mounted thrust drives the complete free base",
                std::abs(evaluation.derivative.angular_acceleration_body_radps2.z - expected_base_alpha),
                1.0e-12,
                "A component-resolved wrench must produce the same total external moment."));
            const double joint_acceleration = evaluation.derivative.articulation_accelerations.empty()
                ? 0.0
                : evaluation.derivative.articulation_accelerations[0];
            report.checks.push_back(Check(
                "Child-mounted thrust accelerates a passive hinge",
                std::abs(joint_acceleration - 2.0),
                1.0e-12,
                "qddot=1 N m / 0.5 kg m^2 when the bus remains inertially fixed."));
        }

        {
            // Case 5: evaluate the same variable-mass articulated state before and after a
            // constant 30 km/s-class Galilean boost. Under the standard rocket convention,
            // internal angular/joint accelerations must be invariant to that translation.
            SimulationRequest request;
            request.scenario_name = "Standard rocket convention is Galilean invariant";
            request.mass_flow_convention = MassFlowConvention::ThrustIncludesExhaustMomentum;
            request.initial_state.velocity_icrf_mps = {30000.0, -5000.0, 2000.0};
            request.initial_state.variable_component_masses_kg = {2.0};
            request.vehicle.components.push_back(MakeComponent(
                "Main body", 10.0, Vec3d::Zero(), {2.0, 3.0, 4.0}));

            ComponentDefinition tank = MakeComponent(
                "Offset variable tank", 2.0, Vec3d::Zero(), {0.2, 0.2, 0.2});
            tank.minimum_mass_kg = 1.0;
            tank.variable_mass_state_index = 0;
            tank.parent_component_index = 0;
            tank.articulation_to_parent.parent_anchor_component_m = {0.0, 0.6, 0.0};
            request.vehicle.components.push_back(tank);

            ComponentDefinition slider = MakeComponent(
                "Sliding appendage", 1.0, {0.25, 0.0, 0.0}, {0.02, 0.08, 0.08});
            slider.parent_component_index = 0;
            slider.articulation_to_parent.parent_anchor_component_m = {0.5, 0.0, 0.0};
            slider.articulation_to_parent.dofs.push_back(MakeDof(
                "appendage_slider", ArticulationMotion::Translation, Vec3d::UnitX()));
            request.vehicle.components.push_back(slider);
            request.control.controller = std::make_shared<ConstantJointEffortController>(
                std::vector<double>{0.3});

            ThrusterDefinition thruster;
            thruster.name = "Mass-depleting test thruster";
            thruster.component_index = 0;
            thruster.propellant_component_index = 1;
            thruster.direction_component = Vec3d::UnitX();
            thruster.constant_thrust_n = 1.0;
            thruster.constant_specific_impulse_seconds = 100.0;
            request.vehicle.thrusters.push_back(thruster);

            // Both evaluations use identical geometry, rates, loads, and config; only the
            // common ICRF translational velocity differs.
            const SimulationConfig boosted_config = SimulationConfigBuilder().Build(request);
            const DynamicsEvaluation boosted = General6DofDynamics(boosted_config).ComputeDynamics(
                0.0, boosted_config.initial_state, boosted_config);
            SpacecraftState unboosted_state = boosted_config.initial_state;
            unboosted_state.velocity_icrf_mps = Vec3d::Zero();
            const DynamicsEvaluation unboosted = General6DofDynamics(boosted_config).ComputeDynamics(
                0.0, unboosted_state, boosted_config);

            report.checks.push_back(Check(
                "Mass depletion does not make attitude depend on inertial translation",
                (boosted.derivative.angular_acceleration_body_radps2 -
                    unboosted.derivative.angular_acceleration_body_radps2).Norm(),
                1.0e-12,
                "With thrust including exhaust momentum, a uniform velocity boost cannot change angular acceleration."));
            const double boosted_joint_acceleration = boosted.derivative.articulation_accelerations.empty()
                ? 0.0
                : boosted.derivative.articulation_accelerations[0];
            const double unboosted_joint_acceleration = unboosted.derivative.articulation_accelerations.empty()
                ? 0.0
                : unboosted.derivative.articulation_accelerations[0];
            report.checks.push_back(Check(
                "Mass depletion does not make articulation depend on inertial translation",
                std::abs(boosted_joint_acceleration - unboosted_joint_acceleration),
                1.0e-12,
                "With thrust including exhaust momentum, a uniform velocity boost cannot change qddot."));
        }

        {
            // A rotating body whose reference origin is offset from its centroid isolates
            // the conversion from Featherstone spatial acceleration to ordinary acceleration.
            SimulationRequest request;
            request.scenario_name = "Spatial acceleration conversion";
            request.initial_state.angular_velocity_body_radps = {0.0, 0.0, 2.0};
            request.vehicle.components.push_back(MakeComponent(
                "Offset rigid body", 2.0, {1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}));
            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            const MassProperties mass_properties = MassPropertiesModel().Compute(
                config.vehicle, config.initial_state);
            FloatingBaseTreeInput input;
            input.center_of_mass_body_m = mass_properties.center_of_mass_body_m;
            input.component_poses = mass_properties.component_poses;
            const FloatingBaseTreeEvaluation evaluation =
                FloatingBaseTreeDynamics().SolveForwardDynamics(
                    config.initial_state, config, input);

            report.checks.push_back(Check(
                "Offset rotating body has zero spatial linear acceleration",
                evaluation.solved
                    ? evaluation.base_acceleration_body.linear.Norm()
                    : std::numeric_limits<double>::infinity(),
                1.0e-12,
                "The free spherical body has zero ABA spatial acceleration in its instantaneous centroid-rest frame."));
            report.checks.push_back(Check(
                "Offset rotating origin reports ordinary centripetal acceleration",
                evaluation.solved
                    ? (evaluation.base_origin_acceleration_body_mps2 -
                        Vec3d{4.0, 0.0, 0.0}).Norm()
                    : std::numeric_limits<double>::infinity(),
                1.0e-12,
                "The reference origin must accelerate at +4 m/s^2 while the centroid remains inertially fixed."));
            report.checks.push_back(Check(
                "Offset rotating body centroid remains inertially fixed",
                evaluation.solved
                    ? evaluation.mass_weighted_component_acceleration_icrf_mps2.Norm()
                    : std::numeric_limits<double>::infinity(),
                1.0e-12,
                "Ordinary centroid reconstruction must include omega cross reference-origin velocity."));
        }

        {
            // The momentum-derivative request selection is accepted by the interface,
            // but the existing propulsion input is effective thrust and must remain Galilean invariant.
            SimulationRequest request;
            request.scenario_name = "Momentum-derivative selection uses effective thrust";
            request.mass_flow_convention = MassFlowConvention::MomentumDerivative;
            request.initial_state.variable_component_masses_kg = {10.0};
            ComponentDefinition tank = MakeComponent(
                "Variable-mass main body", 10.0, Vec3d::Zero(), {1.0, 1.0, 1.0});
            tank.minimum_mass_kg = 1.0;
            tank.variable_mass_state_index = 0;
            request.vehicle.components.push_back(tank);
            ThrusterDefinition thruster;
            thruster.name = "Effective-thrust test";
            thruster.component_index = 0;
            thruster.propellant_component_index = 0;
            thruster.direction_component = Vec3d::UnitX();
            thruster.constant_thrust_n = 10.0;
            thruster.constant_specific_impulse_seconds = 100.0;
            request.vehicle.thrusters.push_back(thruster);

            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            SpacecraftState boosted_state = config.initial_state;
            boosted_state.velocity_icrf_mps = {30000.0, -5000.0, 2000.0};
            SpacecraftState unboosted_state = boosted_state;
            unboosted_state.velocity_icrf_mps = Vec3d::Zero();
            const General6DofDynamics dynamics(config);
            const DynamicsEvaluation boosted = dynamics.ComputeDynamics(
                0.0, boosted_state, config);
            const DynamicsEvaluation unboosted = dynamics.ComputeDynamics(
                0.0, unboosted_state, config);
            report.checks.push_back(Check(
                "Mass-flow selection is Galilean invariant",
                (boosted.derivative.velocity_rate_mps2 -
                    unboosted.derivative.velocity_rate_mps2).Norm(),
                1.0e-12,
                "Standard effective thrust cannot acquire an acceleration term from a uniform velocity boost."));
        }

        {
            // A rotating variable-mass body with a radial nozzle has an exact axial
            // cancellation when J_dot=-q*a^2 and the carrier flux is q*a^2*omega.
            SimulationRequest request;
            request.scenario_name = "Rotating nozzle carrier-flux cancellation";
            request.initial_state.angular_velocity_body_radps = {0.0, 0.0, 0.4};
            request.initial_state.variable_component_masses_kg = {10.0};
            ComponentDefinition body = MakeComponent(
                "Variable rotating body", 10.0, Vec3d::Zero(), {20.0, 20.0, 40.0});
            body.minimum_mass_kg = 1.0;
            body.variable_mass_state_index = 0;
            request.vehicle.components.push_back(body);

            ThrusterDefinition thruster;
            thruster.name = "Radial discharge";
            thruster.component_index = 0;
            thruster.propellant_component_index = 0;
            thruster.application_point_component_m = {2.0, 0.0, 0.0};
            thruster.direction_component = Vec3d::UnitX();
            thruster.constant_thrust_n = kStandardGravityMps2;
            thruster.constant_specific_impulse_seconds = 1.0;
            request.vehicle.thrusters.push_back(thruster);

            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            const DynamicsEvaluation evaluation =
                General6DofDynamics(config).ComputeDynamics(
                    0.0, config.initial_state, config);
            report.checks.push_back(Check(
                "Rotating discharge balances inertia rate with carrier flux",
                evaluation.multibody_solve_succeeded
                    ? std::abs(
                        evaluation.derivative.
                            angular_acceleration_body_radps2.z)
                    : std::numeric_limits<double>::infinity(),
                1.0e-12,
                "For J_dot=-q*a^2, the matching q*a^2*omega carrier flux must leave axial angular velocity unchanged."));

            // Change only J_z/m so the two terms no longer cancel. With q=1 kg/s,
            // a=2 m, omega=0.4 rad/s, and J_z=20 kg m^2, alpha_z=-0.04 rad/s^2.
            request.vehicle.components[0].inertia_centroid_component_kgm2 =
                Mat3d::Diagonal({10.0, 10.0, 20.0});
            const SimulationConfig unequal_config =
                SimulationConfigBuilder().Build(request);
            const DynamicsEvaluation unequal =
                General6DofDynamics(unequal_config).ComputeDynamics(
                    0.0, unequal_config.initial_state, unequal_config);
            report.checks.push_back(Check(
                "Rotating discharge retains a noncancelling carrier residual",
                unequal.multibody_solve_succeeded
                    ? std::abs(
                        unequal.derivative.
                            angular_acceleration_body_radps2.z + 0.04)
                    : std::numeric_limits<double>::infinity(),
                1.0e-12,
                "The solver must retain the difference between local inertia loss and nozzle carrier flux."));
        }

        {
            // Assemble accepted prescribed/commanded discharge and the corresponding
            // carrier flux independently from component poses and point velocities.
            SimulationRequest request;
            request.scenario_name = "Articulated multi-nozzle carrier flux";
            request.initial_state.angular_velocity_body_radps = {0.0, 0.0, 0.25};
            request.initial_state.articulation_coordinates = {0.35};
            request.initial_state.articulation_rates = {0.6};
            request.initial_state.variable_component_masses_kg = {2.0};
            request.vehicle.components.push_back(MakeComponent(
                "Main body", 8.0, Vec3d::Zero(), {3.0, 4.0, 5.0}));

            ComponentDefinition tank = MakeComponent(
                "Propellant owner", 2.0, {0.1, 0.0, 0.0}, {0.2, 0.2, 0.2});
            tank.minimum_mass_kg = 1.0;
            tank.variable_mass_state_index = 0;
            tank.parent_component_index = 0;
            tank.articulation_to_parent.parent_anchor_component_m = {0.0, -0.7, 0.0};
            request.vehicle.components.push_back(tank);

            ComponentDefinition mount = MakeComponent(
                "Articulated mount", 1.0, {0.2, 0.0, 0.0}, {0.1, 0.2, 0.2});
            mount.parent_component_index = 0;
            mount.articulation_to_parent.parent_anchor_component_m = {0.8, 0.0, 0.0};
            mount.articulation_to_parent.dofs.push_back(MakeDof(
                "mount_hinge", ArticulationMotion::Rotation, Vec3d::UnitZ()));
            request.vehicle.components.push_back(mount);

            ThrusterDefinition prescribed;
            prescribed.name = "Articulated prescribed nozzle";
            prescribed.component_index = 2;
            prescribed.propellant_component_index = 1;
            prescribed.application_point_component_m = {0.4, 0.1, 0.0};
            prescribed.direction_component = Vec3d::UnitX();
            prescribed.constant_thrust_n = 2.0 * kStandardGravityMps2;
            prescribed.constant_specific_impulse_seconds = 2.0;
            request.vehicle.thrusters.push_back(prescribed);

            ThrusterDefinition commanded;
            commanded.name = "Commanded main-body nozzle";
            commanded.mode = ThrusterMode::Commanded;
            commanded.component_index = 0;
            commanded.propellant_component_index = 1;
            commanded.application_point_component_m = {-0.3, 0.2, 0.0};
            commanded.direction_component = Vec3d::UnitX();
            commanded.maximum_thrust_n = 4.0 * kStandardGravityMps2;
            request.vehicle.thrusters.push_back(commanded);

            ThrusterDefinition inactive = prescribed;
            inactive.name = "Inactive future nozzle";
            inactive.ignition_elapsed_time_seconds = 10.0;
            request.vehicle.thrusters.push_back(inactive);

            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            const MassProperties initial_mass_properties =
                MassPropertiesModel().Compute(
                    config.vehicle,
                    config.initial_state,
                    {},
                    config.initial_state.articulation_rates);
            ControlCommand command;
            command.thruster_throttles = {0.0, 0.5, 0.0};
            command.thruster_specific_impulses_seconds = {0.0, 2.0, 0.0};
            const PropulsionEvaluation propulsion = PropulsionModel().ComputeLoads(
                0.0,
                config.initial_state,
                config,
                command,
                initial_mass_properties.center_of_mass_body_m,
                initial_mass_properties.component_poses);
            const double discharge_error =
                propulsion.thruster_mass_flows_kgps.size() == 3
                    ? std::max({
                        std::abs(propulsion.thruster_mass_flows_kgps[0] - 1.0),
                        std::abs(propulsion.thruster_mass_flows_kgps[1] - 1.0),
                        std::abs(propulsion.thruster_mass_flows_kgps[2])})
                    : std::numeric_limits<double>::infinity();
            report.checks.push_back(Check(
                "Propulsion exposes accepted discharge in thruster order",
                discharge_error,
                1.0e-12,
                "Prescribed and commanded nozzles must expose their accepted q while inactive nozzles remain zero."));

            const MassProperties mass_properties = MassPropertiesModel().Compute(
                config.vehicle,
                config.initial_state,
                propulsion.component_mass_rates_kgps,
                config.initial_state.articulation_rates);
            FloatingBaseTreeInput input;
            input.component_external_loads = propulsion.component_loads;
            input.center_of_mass_body_m = mass_properties.center_of_mass_body_m;
            input.center_of_mass_mass_redistribution_rate_body_mps =
                mass_properties.center_of_mass_mass_redistribution_rate_body_mps;
            input.component_poses = mass_properties.component_poses;
            input.articulation_rates = config.initial_state.articulation_rates;
            input.variable_component_mass_rates_kgps =
                propulsion.component_mass_rates_kgps;
            input.thruster_mass_flows_kgps =
                propulsion.thruster_mass_flows_kgps;
            const FloatingBaseTreeEvaluation tree =
                FloatingBaseTreeDynamics().SolveForwardDynamics(
                    config.initial_state, config, input);

            Vec3d centroid_velocity_sum_body;
            for (std::size_t component_index = 0;
                component_index < config.vehicle.components.size();
                ++component_index)
            {
                const ComponentDefinition& component =
                    config.vehicle.components[component_index];
                const ComponentPose& pose =
                    mass_properties.component_poses[component_index];
                const Vec3d component_angular_velocity_body =
                    config.initial_state.angular_velocity_body_radps +
                    pose.angular_velocity_relative_body_radps;
                const Vec3d component_origin_velocity_body =
                    Cross(
                        config.initial_state.angular_velocity_body_radps,
                        pose.origin_body_m) +
                    pose.origin_rate_body_mps;
                const Vec3d component_center_velocity_body =
                    component_origin_velocity_body +
                    Cross(
                        component_angular_velocity_body,
                        pose.component_to_body *
                            component.center_of_mass_component_m);
                centroid_velocity_sum_body +=
                    MassPropertiesModel::ComponentMass(
                        component, config.initial_state) *
                    component_center_velocity_body;
            }
            const Vec3d solver_observer_velocity_body =
                -centroid_velocity_sum_body / mass_properties.mass_kg;
            Vec3d expected_linear_flux_body;
            Vec3d expected_angular_flux_about_cm_body;
            for (std::size_t thruster_index = 0;
                thruster_index < config.vehicle.thrusters.size();
                ++thruster_index)
            {
                const double mass_flow =
                    propulsion.thruster_mass_flows_kgps[thruster_index];
                if (mass_flow <= 0.0) continue;
                const ThrusterDefinition& thruster =
                    config.vehicle.thrusters[thruster_index];
                const ComponentPose& pose =
                    mass_properties.component_poses[thruster.component_index];
                const Vec3d component_angular_velocity_body =
                    config.initial_state.angular_velocity_body_radps +
                    pose.angular_velocity_relative_body_radps;
                const Vec3d nozzle_position_body =
                    ComponentKinematicsModel::PointToBody(
                        pose, thruster.application_point_component_m);
                const Vec3d nozzle_velocity_body =
                    solver_observer_velocity_body +
                    Cross(
                        config.initial_state.angular_velocity_body_radps,
                        pose.origin_body_m) +
                    pose.origin_rate_body_mps +
                    Cross(
                        component_angular_velocity_body,
                        pose.component_to_body *
                            thruster.application_point_component_m);
                expected_linear_flux_body += mass_flow * nozzle_velocity_body;
                expected_angular_flux_about_cm_body += mass_flow * Cross(
                    nozzle_position_body - mass_properties.center_of_mass_body_m,
                    nozzle_velocity_body);
            }
            report.checks.push_back(Check(
                "Articulated nozzle linear carrier flux matches point kinematics",
                tree.solved
                    ? (tree.nozzle_carrier_linear_momentum_flux_solver_body_n -
                        expected_linear_flux_body).Norm()
                    : std::numeric_limits<double>::infinity(),
                2.0e-12,
                "Carrier linear flux must use accepted q and the mount point velocity in the solver observer."));
            report.checks.push_back(Check(
                "Articulated nozzle angular carrier flux matches point kinematics",
                tree.solved
                    ? (tree.
                        nozzle_carrier_angular_momentum_flux_about_cm_solver_body_nm -
                        expected_angular_flux_about_cm_body).Norm()
                    : std::numeric_limits<double>::infinity(),
                2.0e-12,
                "Local mount-node fluxes must rotate and shift to the same moment about the spacecraft CM."));
        }

        {
            // A flow-biased rotor at its upper stop exercises the identical carrier
            // correction in forward dynamics and inverse stop-reaction recovery.
            SimulationRequest request;
            request.scenario_name = "Sustained stop reaction with nozzle carrier flux";
            request.initial_state.angular_velocity_body_radps = {0.0, 0.0, 0.4};
            request.initial_state.articulation_coordinates = {kPi};
            request.initial_state.articulation_rates = {0.0};
            request.initial_state.variable_component_masses_kg = {10.0};
            request.vehicle.components.push_back(MakeComponent(
                "Main body", 5.0, Vec3d::Zero(), {5.0, 5.0, 10.0}));

            ComponentDefinition rotor = MakeComponent(
                "Variable rotor", 10.0, Vec3d::Zero(), {30.0, 30.0, 60.0});
            rotor.minimum_mass_kg = 1.0;
            rotor.variable_mass_state_index = 0;
            rotor.parent_component_index = 0;
            rotor.articulation_to_parent.dofs.push_back(MakeDof(
                "stopped_rotor", ArticulationMotion::Rotation, Vec3d::UnitZ()));
            request.vehicle.components.push_back(rotor);

            ThrusterDefinition thruster;
            thruster.name = "Rotor radial discharge";
            thruster.component_index = 1;
            thruster.propellant_component_index = 1;
            thruster.application_point_component_m = {2.0, 0.0, 0.0};
            thruster.direction_component = Vec3d::UnitX();
            thruster.constant_thrust_n = kStandardGravityMps2;
            thruster.constant_specific_impulse_seconds = 1.0;
            request.vehicle.thrusters.push_back(thruster);

            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            const DynamicsEvaluation evaluation =
                General6DofDynamics(config).ComputeDynamics(
                    0.0, config.initial_state, config);
            const double joint_acceleration =
                evaluation.derivative.articulation_accelerations.empty()
                    ? std::numeric_limits<double>::infinity()
                    : evaluation.derivative.articulation_accelerations[0];
            const double reaction = evaluation.joint_constraint_efforts.empty()
                ? std::numeric_limits<double>::infinity()
                : evaluation.joint_constraint_efforts[0];
            report.checks.push_back(Check(
                "Flow-biased upper stop prevents outward acceleration",
                evaluation.multibody_solve_succeeded
                    ? std::abs(joint_acceleration)
                    : std::numeric_limits<double>::infinity(),
                1.0e-12,
                "The upper stop must hold a rotor whose unconstrained depletion bias drives it outward."));
            report.checks.push_back(Check(
                "Flow-biased stop reaction includes nozzle carrier flux",
                std::abs(reaction + 4.0 / 35.0),
                2.0e-12,
                "Forward dynamics and inverse reaction recovery must use the same I_dot*v plus carrier-flux bias."));
        }

        {
            // Propulsion-mode contract: prescribed histories ignore controller
            // propulsion output, while commanded thrusters use both controller
            // throttle and controller Isp.
            SimulationRequest prescribed_request;
            prescribed_request.scenario_name =
                "Prescribed propulsion ignores controller commands";
            ComponentDefinition prescribed_tank = MakeComponent(
                "Variable-mass main body", 10.0, Vec3d::Zero(),
                {1.0, 1.0, 1.0});
            prescribed_tank.minimum_mass_kg = 9.0;
            prescribed_tank.variable_mass_state_index = 0;
            prescribed_request.vehicle.components.push_back(
                prescribed_tank);
            ThrusterDefinition prescribed;
            prescribed.name = "Prescribed test thruster";
            prescribed.propellant_component_index = 0;
            prescribed.thrust_profile_n.samples = {
                {0.0, 0.0}, {1.0, 10.0}, {2.0, 0.0}};
            prescribed.specific_impulse_profile_seconds.samples = {
                {0.0, 200.0}, {2.0, 200.0}};
            prescribed_request.vehicle.thrusters.push_back(prescribed);
            prescribed_request.control.controller =
                std::make_shared<ConstantThrusterCommandController>(
                    0, 0.1, 1.0);
            const SimulationConfig prescribed_config =
                SimulationConfigBuilder().Build(prescribed_request);
            const DynamicsEvaluation prescribed_evaluation =
                General6DofDynamics(prescribed_config).ComputeDynamics(
                    1.0,
                    prescribed_config.initial_state,
                    prescribed_config);
            const double prescribed_expected_mass_rate =
                -10.0 / (200.0 * kStandardGravityMps2);
            report.checks.push_back(Check(
                "Prescribed thrust is independent of controller throttle",
                std::abs(
                    prescribed_evaluation.applied_force_torque.
                        thrust_force_icrf_n.x -
                    10.0),
                1.0e-12,
                "The tabulated 10 N value must not be scaled or overridden by the controller."));
            report.checks.push_back(Check(
                "Prescribed mass flow uses prescribed Isp",
                std::abs(
                    prescribed_evaluation.applied_force_torque.mass_rate_kgps -
                    prescribed_expected_mass_rate),
                1.0e-12,
                "m_dot=-T/(Isp*g0) must use the profile's 200 s Isp."));
            const double prescribed_component_mass_rate =
                prescribed_evaluation.derivative.
                    variable_component_mass_rates_kgps.empty()
                    ? 0.0
                    : prescribed_evaluation.derivative.
                        variable_component_mass_rates_kgps[0];
            report.checks.push_back(Check(
                "Thruster mass loss is owned by its propellant component",
                std::abs(
                    prescribed_component_mass_rate -
                    prescribed_expected_mass_rate),
                1.0e-12,
                "Total spacecraft mass rate and tank mass rate must represent the same propellant loss."));

            SimulationRequest commanded_request;
            commanded_request.scenario_name =
                "Commanded propulsion uses controller throttle and Isp";
            ComponentDefinition commanded_tank = MakeComponent(
                "Variable-mass main body", 10.0, Vec3d::Zero(),
                {1.0, 1.0, 1.0});
            commanded_tank.minimum_mass_kg = 9.0;
            commanded_tank.variable_mass_state_index = 0;
            commanded_request.vehicle.components.push_back(
                commanded_tank);
            ThrusterDefinition commanded;
            commanded.name = "Commanded test thruster";
            commanded.mode = ThrusterMode::Commanded;
            commanded.propellant_component_index = 0;
            commanded.maximum_thrust_n = 20.0;
            commanded_request.vehicle.thrusters.push_back(commanded);
            commanded_request.control.controller =
                std::make_shared<ConstantThrusterCommandController>(
                    0, 0.25, 250.0);
            const SimulationConfig commanded_config =
                SimulationConfigBuilder().Build(commanded_request);
            const DynamicsEvaluation commanded_evaluation =
                General6DofDynamics(commanded_config).ComputeDynamics(
                    0.0,
                    commanded_config.initial_state,
                    commanded_config);
            const double commanded_expected_mass_rate =
                -5.0 / (250.0 * kStandardGravityMps2);
            report.checks.push_back(Check(
                "Commanded thrust equals maximum thrust times throttle",
                std::abs(
                    commanded_evaluation.applied_force_torque.
                        thrust_force_icrf_n.x -
                    5.0),
                1.0e-12,
                "T=20 N times the controller's clamped 0.25 throttle."));
            report.checks.push_back(Check(
                "Commanded mass flow uses controller Isp",
                std::abs(
                    commanded_evaluation.applied_force_torque.mass_rate_kgps -
                    commanded_expected_mass_rate),
                1.0e-12,
                "m_dot=-T/(Isp*g0) must use the controller's 250 s Isp."));

            SimulationRequest missing_tank_request = commanded_request;
            missing_tank_request.vehicle.thrusters[0].
                propellant_component_index = kInvalidIndex;
            const SimulationResult missing_tank_result =
                SimulationEngine().Run(missing_tank_request);
            report.checks.push_back(Check(
                "Thruster without a propellant component is rejected",
                !missing_tank_result.success &&
                    missing_tank_result.message.find(
                        "variable-mass propellant component") !=
                        std::string::npos
                    ? 0.0
                    : 1.0,
                0.0,
                "Validation must prevent thrust or spacecraft mass loss without an owning tank state."));

            SimulationRequest fixed_tank_request = commanded_request;
            fixed_tank_request.vehicle.components[0].
                variable_mass_state_index = kInvalidIndex;
            const SimulationResult fixed_tank_result =
                SimulationEngine().Run(fixed_tank_request);
            report.checks.push_back(Check(
                "Thruster linked to a fixed-mass component is rejected",
                !fixed_tank_result.success &&
                    fixed_tank_result.message.find(
                        "variable-mass propellant component") !=
                        std::string::npos
                    ? 0.0
                    : 1.0,
                0.0,
                "A propellant source must own a propagated component-mass state."));
        }

        {
            // Independent translating two-component example. The root is a
            // fixed hub, the offset child owns propellant, and the nozzle is
            // fixed at the root origin so no rotation is excited.
            constexpr double hub_mass = 10.0;
            constexpr double propellant_mass = 5.0;
            constexpr double offset = 3.0;
            constexpr double discharge = 1.0;
            constexpr double discharge_rate = 0.2;
            constexpr double total_mass = hub_mass + propellant_mass;

            const auto make_request = [=]
            {
                SimulationRequest request;
                request.scenario_name =
                    "Geometric CM translation with changing discharge";
                request.initial_state.variable_component_masses_kg = {
                    propellant_mass};
                request.vehicle.components.push_back(MakeComponent(
                    "Fixed hub", hub_mass, Vec3d::Zero(),
                    {2.0, 2.0, 2.0}));
                ComponentDefinition tank = MakeComponent(
                    "Offset propellant", propellant_mass,
                    Vec3d::Zero(), {1.0, 1.0, 1.0});
                tank.minimum_mass_kg = 1.0;
                tank.variable_mass_state_index = 0;
                tank.parent_component_index = 0;
                tank.articulation_to_parent.parent_anchor_component_m = {
                    offset, 0.0, 0.0};
                request.vehicle.components.push_back(tank);

                ThrusterDefinition thruster;
                thruster.name = "Root nozzle drawing from offset tank";
                thruster.component_index = 0;
                thruster.propellant_component_index = 1;
                thruster.direction_component = Vec3d::UnitX();
                thruster.constant_specific_impulse_seconds = 1.0;
                request.vehicle.thrusters.push_back(thruster);
                return request;
            };

            SimulationRequest prescribed_request = make_request();
            ThrusterDefinition& prescribed_thruster =
                prescribed_request.vehicle.thrusters[0];
            prescribed_thruster.thrust_profile_n.samples = {
                {0.0, kStandardGravityMps2 *
                    (discharge - discharge_rate)},
                {2.0, kStandardGravityMps2 *
                    (discharge + discharge_rate)}};
            prescribed_request.control.controller =
                std::make_shared<
                    ThrusterCommandWithMassFlowDerivativeController>(
                        0, 0.0, 1.0, 999.0);
            const SimulationConfig prescribed_config =
                SimulationConfigBuilder().Build(prescribed_request);
            const DynamicsEvaluation prescribed_evaluation =
                General6DofDynamics(prescribed_config).ComputeDynamics(
                    1.0,
                    prescribed_config.initial_state,
                    prescribed_config);
            const double expected_cm_second_derivative =
                -discharge_rate * hub_mass * offset /
                    (total_mass * total_mass) -
                2.0 * discharge * discharge * hub_mass * offset /
                    (total_mass * total_mass * total_mass);
            const double expected_acceleration =
                kStandardGravityMps2 * discharge / total_mass +
                expected_cm_second_derivative;
            report.checks.push_back(Check(
                "Prescribed q-dot completes geometric-CM acceleration",
                std::abs(
                    prescribed_evaluation.derivative.
                        velocity_rate_mps2.x - expected_acceleration),
                2.0e-12,
                "The independent two-component result is F/M+c_ddot, including m_ddot and both velocity corrections."));

            SimulationRequest zero_ramp_request = make_request();
            zero_ramp_request.vehicle.thrusters[0].
                thrust_profile_n.samples = {
                    {0.0, 0.0}, {1.0, kStandardGravityMps2}};
            const SimulationConfig zero_ramp_config =
                SimulationConfigBuilder().Build(zero_ramp_request);
            const MassProperties zero_ramp_properties =
                MassPropertiesModel().Compute(
                    zero_ramp_config.vehicle,
                    zero_ramp_config.initial_state);
            const PropulsionEvaluation zero_ramp_evaluation =
                PropulsionModel().ComputeLoads(
                    0.0,
                    zero_ramp_config.initial_state,
                    zero_ramp_config,
                    ControlCommand{},
                    zero_ramp_properties.center_of_mass_body_m,
                    zero_ramp_properties.component_poses);
            const double zero_ramp_error = std::max(
                zero_ramp_evaluation.thruster_mass_flows_kgps.empty()
                    ? 1.0
                    : std::abs(
                        zero_ramp_evaluation.
                            thruster_mass_flows_kgps[0]),
                zero_ramp_evaluation.
                        thruster_mass_flow_derivatives_kgps2.empty()
                    ? 1.0
                    : std::abs(
                        zero_ramp_evaluation.
                            thruster_mass_flow_derivatives_kgps2[0] - 1.0));
            report.checks.push_back(Check(
                "Zero-thrust ramp endpoint retains analytic q-dot",
                zero_ramp_error,
                1.0e-12,
                "At a smooth ramp start q may be zero while its right derivative remains finite."));

            SimulationRequest isp_slope_request = make_request();
            isp_slope_request.vehicle.thrusters[0].constant_thrust_n = 10.0;
            isp_slope_request.vehicle.thrusters[0].
                specific_impulse_profile_seconds.samples = {
                    {0.0, 100.0}, {2.0, 200.0}};
            const SimulationConfig isp_slope_config =
                SimulationConfigBuilder().Build(isp_slope_request);
            const MassProperties isp_slope_properties =
                MassPropertiesModel().Compute(
                    isp_slope_config.vehicle,
                    isp_slope_config.initial_state);
            const PropulsionEvaluation isp_slope_evaluation =
                PropulsionModel().ComputeLoads(
                    1.0,
                    isp_slope_config.initial_state,
                    isp_slope_config,
                    ControlCommand{},
                    isp_slope_properties.center_of_mass_body_m,
                    isp_slope_properties.component_poses);
            const double expected_isp_slope_qdot =
                -10.0 * 50.0 /
                (150.0 * 150.0 * kStandardGravityMps2);
            report.checks.push_back(Check(
                "Prescribed Isp slope contributes analytically to q-dot",
                std::abs(
                    isp_slope_evaluation.
                        thruster_mass_flow_derivatives_kgps2[0] -
                    expected_isp_slope_qdot),
                1.0e-12,
                "The quotient-rule derivative must include an independently varying specific-impulse profile."));

            SpacecraftState exhausted_state = zero_ramp_config.initial_state;
            exhausted_state.variable_component_masses_kg[0] = 1.0;
            const PropulsionEvaluation exhausted_evaluation =
                PropulsionModel().ComputeLoads(
                    0.0,
                    exhausted_state,
                    zero_ramp_config,
                    ControlCommand{},
                    zero_ramp_properties.center_of_mass_body_m,
                    zero_ramp_properties.component_poses);
            SimulationConfig closed_window_config = zero_ramp_config;
            closed_window_config.vehicle.thrusters[0].
                shutdown_elapsed_time_seconds = 0.0;
            const PropulsionEvaluation closed_window_evaluation =
                PropulsionModel().ComputeLoads(
                    0.0,
                    zero_ramp_config.initial_state,
                    closed_window_config,
                    ControlCommand{},
                    zero_ramp_properties.center_of_mass_body_m,
                    zero_ramp_properties.component_poses);
            report.checks.push_back(Check(
                "Inactive and exhausted thrusters suppress stale q-dot",
                std::max(
                    std::abs(exhausted_evaluation.
                        thruster_mass_flow_derivatives_kgps2[0]),
                    std::abs(closed_window_evaluation.
                        thruster_mass_flow_derivatives_kgps2[0])),
                0.0,
                "Firing-window and propellant-availability hard suppressions must zero both q and q-dot."));

            SimulationRequest routed_request = make_request();
            routed_request.vehicle.thrusters[0].mode =
                ThrusterMode::Commanded;
            routed_request.vehicle.thrusters[0].maximum_thrust_n =
                kStandardGravityMps2;
            ThrusterDefinition second_thruster =
                routed_request.vehicle.thrusters[0];
            second_thruster.name = "Second nozzle on propellant owner";
            second_thruster.component_index = 1;
            routed_request.vehicle.thrusters.push_back(second_thruster);
            const SimulationConfig routed_config =
                SimulationConfigBuilder().Build(routed_request);
            ControlCommandWriter routed_writer(2, 0, 0);
            routed_writer.SetThrusterCommand(0, 1.0, 1.0);
            routed_writer.SetThrusterCommand(1, 1.0, 1.0);
            routed_writer.SetThrusterMassFlowDerivative(0, 0.3);
            routed_writer.SetThrusterMassFlowDerivative(1, -0.1);
            const MassProperties routed_properties =
                MassPropertiesModel().Compute(
                    routed_config.vehicle,
                    routed_config.initial_state);
            const PropulsionEvaluation routed_evaluation =
                PropulsionModel().ComputeLoads(
                    0.0,
                    routed_config.initial_state,
                    routed_config,
                    routed_writer.Command(),
                    routed_properties.center_of_mass_body_m,
                    routed_properties.component_poses);
            const double routed_derivative_error = std::max({
                routed_evaluation.
                        thruster_mass_flow_derivatives_kgps2.size() == 2
                    ? std::abs(
                        routed_evaluation.
                            thruster_mass_flow_derivatives_kgps2[0] - 0.3)
                    : 1.0,
                routed_evaluation.
                        thruster_mass_flow_derivatives_kgps2.size() == 2
                    ? std::abs(
                        routed_evaluation.
                            thruster_mass_flow_derivatives_kgps2[1] + 0.1)
                    : 1.0,
                routed_evaluation.
                        component_mass_second_derivatives_kgps2.empty()
                    ? 1.0
                    : std::abs(
                        routed_evaluation.
                            component_mass_second_derivatives_kgps2[0] +
                        0.2)});
            report.checks.push_back(Check(
                "Mixed commanded q-dot values retain thruster and owner indexing",
                routed_derivative_error,
                1.0e-12,
                "Signed derivatives from nozzles on different mounts accumulate independently into their shared propellant owner."));

            ControlCommandWriter reset_writer(2, 0, 0);
            reset_writer.SetThrusterCommand(0, 1.0, 1.0);
            reset_writer.SetThrusterCommand(1, 1.0, 1.0);
            const PropulsionEvaluation reset_evaluation =
                PropulsionModel().ComputeLoads(
                    0.0,
                    routed_config.initial_state,
                    routed_config,
                    reset_writer.Command(),
                    routed_properties.center_of_mass_body_m,
                    routed_properties.component_poses);
            const double reset_error = std::max(
                std::abs(reset_evaluation.
                    thruster_mass_flow_derivatives_kgps2[0]),
                std::abs(reset_evaluation.
                    thruster_mass_flow_derivatives_kgps2[1]));
            report.checks.push_back(Check(
                "Commanded q-dot omission resets every RHS evaluation",
                reset_error,
                0.0,
                "A new backend-owned command writer must not inherit derivatives from an earlier stage."));

            SimulationRequest commanded_request = make_request();
            ThrusterDefinition& commanded_thruster =
                commanded_request.vehicle.thrusters[0];
            commanded_thruster.mode = ThrusterMode::Commanded;
            commanded_thruster.maximum_thrust_n =
                2.0 * kStandardGravityMps2;
            commanded_request.control.controller =
                std::make_shared<
                    ThrusterCommandWithMassFlowDerivativeController>(
                        0, 0.5, 1.0, discharge_rate);
            const SimulationConfig commanded_config =
                SimulationConfigBuilder().Build(commanded_request);
            const DynamicsEvaluation supplied_evaluation =
                General6DofDynamics(commanded_config).ComputeDynamics(
                    0.0,
                    commanded_config.initial_state,
                    commanded_config);
            report.checks.push_back(Check(
                "Commanded supplied q-dot reaches geometric-CM RHS",
                std::abs(
                    supplied_evaluation.derivative.velocity_rate_mps2.x -
                    expected_acceleration),
                2.0e-12,
                "A supplied derivative must reach translation, not stop in the controller adapter or propulsion buffer."));

            commanded_request.control.controller =
                std::make_shared<
                    ThrusterCommandWithMassFlowDerivativeController>(
                        0, 0.5, 1.0, std::nullopt);
            const SimulationConfig omitted_config =
                SimulationConfigBuilder().Build(commanded_request);
            const DynamicsEvaluation omitted_evaluation =
                General6DofDynamics(omitted_config).ComputeDynamics(
                    0.0,
                    omitted_config.initial_state,
                    omitted_config);
            const double expected_omitted_acceleration =
                kStandardGravityMps2 * discharge / total_mass -
                2.0 * discharge * discharge * hub_mass * offset /
                    (total_mass * total_mass * total_mass);
            report.checks.push_back(Check(
                "Omitted q-dot drops only the m-ddot correction",
                std::abs(
                    omitted_evaluation.derivative.velocity_rate_mps2.x -
                    expected_omitted_acceleration),
                2.0e-12,
                "Actual q, m_dot, thrust, and both velocity terms remain active when the optional derivative is omitted."));
            report.checks.push_back(Check(
                "Supplied and omitted q-dot differ by the analytic term",
                std::abs(
                    (supplied_evaluation.derivative.velocity_rate_mps2.x -
                        omitted_evaluation.derivative.velocity_rate_mps2.x) +
                    discharge_rate * hub_mass * offset /
                        (total_mass * total_mass)),
                2.0e-12,
                "The omission policy is per evaluation and affects only sum(m_ddot*rho)/M."));

            commanded_request.control.controller =
                std::make_shared<
                    ThrusterCommandWithMassFlowDerivativeController>(
                        0, 0.5, 1.0, -discharge_rate);
            const SimulationConfig negative_rate_config =
                SimulationConfigBuilder().Build(commanded_request);
            const DynamicsEvaluation negative_rate_evaluation =
                General6DofDynamics(negative_rate_config).ComputeDynamics(
                    0.0,
                    negative_rate_config.initial_state,
                    negative_rate_config);
            report.checks.push_back(Check(
                "Negative commanded q-dot reverses the m-ddot correction",
                std::abs(
                    negative_rate_evaluation.derivative.
                        velocity_rate_mps2.x -
                    (expected_omitted_acceleration +
                        discharge_rate * hub_mass * offset /
                            (total_mass * total_mass))),
                2.0e-12,
                "Signed actual-discharge derivatives must be accepted without changing the positive discharge itself."));

            commanded_request.control.controller =
                std::make_shared<
                    ThrusterCommandWithMassFlowDerivativeController>(
                        0, 0.5, 1.0, 0.0);
            const SimulationConfig supplied_zero_config =
                SimulationConfigBuilder().Build(commanded_request);
            const DynamicsEvaluation supplied_zero_evaluation =
                General6DofDynamics(supplied_zero_config).ComputeDynamics(
                    0.0,
                    supplied_zero_config.initial_state,
                    supplied_zero_config);
            report.checks.push_back(Check(
                "Supplied zero q-dot matches omission numerically",
                std::abs(
                    supplied_zero_evaluation.derivative.
                        velocity_rate_mps2.x -
                    omitted_evaluation.derivative.velocity_rate_mps2.x),
                0.0,
                "Provided zero and omitted metadata remain distinct while contributing the same numerical m-ddot value."));

            SimulationRequest kinematic_request;
            kinematic_request.scenario_name =
                "Rotating articulated geometric-CM correction";
            kinematic_request.initial_state.variable_component_masses_kg = {
                2.0};
            kinematic_request.initial_state.articulation_coordinates = {0.4};
            kinematic_request.initial_state.articulation_rates = {0.3};
            kinematic_request.initial_state.angular_velocity_body_radps = {
                0.1, -0.2, 0.3};
            constexpr double attitude_angle = 0.7;
            kinematic_request.initial_state.attitude_body_to_icrf = {
                std::cos(0.5 * attitude_angle),
                0.0,
                0.0,
                std::sin(0.5 * attitude_angle)};
            kinematic_request.vehicle.components.push_back(MakeComponent(
                "Kinematic hub", 8.0, Vec3d::Zero(),
                {2.0, 2.0, 2.0}));
            ComponentDefinition moving_tank = MakeComponent(
                "Moving tank", 2.0, Vec3d::Zero(),
                {1.0, 1.0, 1.0});
            moving_tank.minimum_mass_kg = 1.0;
            moving_tank.variable_mass_state_index = 0;
            moving_tank.parent_component_index = 0;
            moving_tank.articulation_to_parent.
                parent_anchor_component_m = {1.0, 0.0, 0.0};
            moving_tank.articulation_to_parent.dofs.push_back(MakeDof(
                "Tank translation",
                ArticulationMotion::Translation,
                Vec3d::UnitY()));
            kinematic_request.vehicle.components.push_back(moving_tank);
            ThrusterDefinition moving_nozzle;
            moving_nozzle.name = "Offset root nozzle";
            moving_nozzle.mode = ThrusterMode::Commanded;
            moving_nozzle.component_index = 0;
            moving_nozzle.propellant_component_index = 1;
            moving_nozzle.application_point_component_m = {0.5, 0.0, 0.0};
            moving_nozzle.direction_component = Vec3d::UnitX();
            moving_nozzle.maximum_thrust_n = kStandardGravityMps2;
            kinematic_request.vehicle.thrusters.push_back(moving_nozzle);
            kinematic_request.control.controller =
                std::make_shared<
                    ThrusterCommandWithMassFlowDerivativeController>(
                        0, 0.5, 1.0, 0.1);
            const SimulationConfig kinematic_config =
                SimulationConfigBuilder().Build(kinematic_request);
            const DynamicsEvaluation kinematic_evaluation =
                General6DofDynamics(kinematic_config).ComputeDynamics(
                    0.0,
                    kinematic_config.initial_state,
                    kinematic_config);

            // Independent hand assembly for this one prismatic geometry.
            const Vec3d center_rate_body{-0.04, 0.044, 0.0};
            const Vec3d tank_rho_body{0.8, 0.32, 0.0};
            const Vec3d nozzle_rho_body{0.3, -0.08, 0.0};
            const Vec3d omega_body{0.1, -0.2, 0.3};
            const Vec3d tank_relative_velocity_body =
                Cross(omega_body, tank_rho_body) +
                Vec3d{0.0, 0.3, 0.0} - center_rate_body;
            const Vec3d nozzle_relative_velocity_body =
                Cross(omega_body, nozzle_rho_body) - center_rate_body;
            const Vec3d expected_kinematic_correction_body =
                (-0.1 * tank_rho_body -
                    0.5 * tank_relative_velocity_body -
                    0.5 * nozzle_relative_velocity_body) / 10.0;
            const Vec3d expected_kinematic_acceleration =
                kinematic_config.initial_state.attitude_body_to_icrf.Rotate(
                    Vec3d{0.5 * kStandardGravityMps2 / 10.0, 0.0, 0.0} +
                    expected_kinematic_correction_body);
            SpacecraftState boosted_kinematic_state =
                kinematic_config.initial_state;
            boosted_kinematic_state.velocity_icrf_mps = {
                1.0e8, -2.0e8, 3.0e8};
            const DynamicsEvaluation boosted_kinematic_evaluation =
                General6DofDynamics(kinematic_config).ComputeDynamics(
                    0.0,
                    boosted_kinematic_state,
                    kinematic_config);
            report.checks.push_back(Check(
                "Rotating articulated CM correction matches point kinematics",
                std::max(
                    (kinematic_evaluation.derivative.velocity_rate_mps2 -
                        expected_kinematic_acceleration).Norm(),
                    (boosted_kinematic_evaluation.derivative.
                        velocity_rate_mps2 -
                        kinematic_evaluation.derivative.
                            velocity_rate_mps2).Norm()),
                3.0e-12,
                "Nontrivial attitude, base rotation, joint motion, offset centroids, and a large inertial boost must preserve the independently assembled CM acceleration."));

            // Smooth prescribed propagation has an independent velocity solution:
            // v_B=g0*Isp*ln(M0/M), v_CM=v_B+c_dot.
            SimulationRequest propagation_request = make_request();
            propagation_request.final_ephemeris_time_tdb_seconds = 1.0;
            propagation_request.requested_duration_seconds = 1.0;
            propagation_request.maximum_integrator_step_seconds = 0.01;
            propagation_request.initial_integrator_step_seconds = 0.01;
            propagation_request.absolute_tolerance = 1.0e-11;
            propagation_request.relative_tolerance = 1.0e-11;
            propagation_request.vehicle.thrusters[0].
                thrust_profile_n.samples = {
                    {0.0, 0.0},
                    {1.0, kStandardGravityMps2 * discharge},
                    {3.0, kStandardGravityMps2 *
                        (discharge + 2.0 * discharge_rate)},
                    {4.0, 0.0}};
            propagation_request.vehicle.thrusters[0].
                ignition_elapsed_time_seconds = -1.0;
            propagation_request.initial_state.velocity_icrf_mps.x =
                -discharge * hub_mass * offset /
                    (total_mass * total_mass);
            const double final_mass = total_mass - discharge -
                0.5 * discharge_rate;
            const double final_discharge = discharge + discharge_rate;
            const double expected_final_velocity =
                kStandardGravityMps2 * std::log(total_mass / final_mass) -
                final_discharge * hub_mass * offset /
                    (final_mass * final_mass);

            double propagation_error = 0.0;
            std::string propagation_message;
            for (const IntegratorKind integrator : {
                IntegratorKind::FixedStepRK4,
                IntegratorKind::AdaptiveDormandPrince54})
            {
                propagation_request.integrator_kind = integrator;
                const SimulationResult propagation_result =
                    SimulationEngine().Run(propagation_request);
                if (!propagation_result.success)
                    propagation_message = propagation_result.message;
                propagation_error = std::max(
                    propagation_error,
                    propagation_result.success &&
                            !propagation_result.samples.empty()
                        ? std::abs(
                            propagation_result.samples.back().state.
                                velocity_icrf_mps.x -
                            expected_final_velocity)
                        : 1.0);
            }
            report.checks.push_back(Check(
                "Both integrators converge to geometric-CM rocket solution",
                propagation_error,
                2.0e-8,
                propagation_message.empty()
                    ? "RK4 and adaptive Dormand-Prince must follow the independent smooth-burn velocity solution."
                    : propagation_message));
        }

        {
            // Case 6: place a revolute joint exactly at its upper coordinate stop and command
            // positive torque. The active-set rerun must prescribe qdd=0, recover the stop
            // reaction, and leave the free spacecraft unaffected by those internal torques.
            SimulationRequest request;
            request.scenario_name = "Revolute upper stop";
            request.initial_state.articulation_coordinates = {kPi};
            request.initial_state.articulation_rates = {0.0};
            request.vehicle.components.push_back(MakeComponent(
                "Main body", 5.0, Vec3d::Zero(), {1.0, 1.0, 2.0}));
            ComponentDefinition rotor = MakeComponent(
                "Stopped rotor", 1.0, Vec3d::Zero(), {0.1, 0.1, 0.5});
            rotor.parent_component_index = 0;
            rotor.articulation_to_parent.dofs.push_back(MakeDof(
                "limited_hinge", ArticulationMotion::Rotation, Vec3d::UnitZ()));
            request.vehicle.components.push_back(rotor);
            request.control.controller = std::make_shared<ConstantJointEffortController>(
                std::vector<double>{0.4});

            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            const DynamicsEvaluation evaluation =
                General6DofDynamics(config).ComputeDynamics(0.0, config.initial_state, config);
            const double joint_acceleration = evaluation.derivative.articulation_accelerations.empty()
                ? 1.0
                : evaluation.derivative.articulation_accelerations[0];
            const double stop_reaction = evaluation.joint_constraint_efforts.empty()
                ? 0.0
                : evaluation.joint_constraint_efforts[0];
            report.checks.push_back(Check(
                "Coordinate stop prevents outward joint acceleration",
                std::abs(joint_acceleration),
                1.0e-12,
                "The upper stop must constrain qddot to zero."));
            report.checks.push_back(Check(
                "Coordinate stop supplies the reaction effort",
                std::abs(stop_reaction + 0.4),
                1.0e-12,
                "The stop reaction cancels the known actuator torque at the hard limit."));
            report.checks.push_back(Check(
                "Internal stop load does not accelerate the free base",
                evaluation.derivative.angular_acceleration_body_radps2.Norm(),
                1.0e-12,
                "Actuator and stop torques are internal to the spacecraft."));
        }

        {
            // Case 7: propagate the nested revolute-prismatic tree for ten seconds with only
            // internal spring efforts. This exercises every RK4 stage and checks structural
            // solvability, output schema, inertial momentum conservation, and uniform CM motion.
            const SimulationRequest request = MakeNestedFreeFlightRequest();
            const SimulationResult result = SimulationEngine().Run(request);
            const SimulationConfig direct_config = SimulationConfigBuilder().Build(request);
            const IntegrationAdvance direct_advance = AdaptiveDormandPrince54().Advance(
                UniformTranslationDynamics{}, direct_config, direct_config.initial_state, 2.0);
            report.checks.push_back(Check(
                "Adaptive Dormand-Prince accepts a constant derivative",
                direct_advance.success ? 0.0 : 1.0,
                0.0,
                direct_advance.success ? "Controlled Dopri5 accepted the test ODE." :
                    direct_advance.message));
            const bool has_samples = result.success && result.samples.size() >= 2;
            const double solve_error = has_samples && std::all_of(
                result.samples.begin(), result.samples.end(),
                [](const TelemetrySample& sample) { return sample.multibody_solve_succeeded; })
                ? 0.0
                : 1.0;
            report.checks.push_back(Check(
                "Nested mixed-joint tree remains solvable",
                solve_error,
                0.0,
                "Every RK4 output state must have a nonsingular floating-base solve."));
            // StateRecorder must keep its numeric AVS-style table rectangular and row-aligned
            // with the typed sample history as joint/momentum fields are appended.
            const bool aligned_output = has_samples && result.solution_array.size() == result.samples.size() &&
                std::all_of(
                    result.solution_array.begin(), result.solution_array.end(),
                    [&result](const std::vector<double>& row)
                    {
                        return row.size() == result.column_names.size();
                    });
            report.checks.push_back(Check(
                "Multibody solution rows match their column schema",
                aligned_output ? 0.0 : 1.0,
                0.0,
                "Every returned AVS-style row must include all momentum and joint-effort columns."));
            if (has_samples)
            {
                const TelemetrySample& first = result.samples.front();
                const TelemetrySample& last = result.samples.back();
                report.checks.push_back(Check(
                    "Nested tree conserves inertial angular momentum",
                    RelativeDifference(
                        last.total_angular_momentum_about_cm_icrf_kgm2ps,
                        first.total_angular_momentum_about_cm_icrf_kgm2ps),
                    2.0e-7,
                    "No external moment is present; joint motion may only redistribute momentum."));
                report.checks.push_back(Check(
                    "Nested tree conserves inertial linear momentum",
                    RelativeDifference(
                        last.total_linear_momentum_icrf_kgmps,
                        first.total_linear_momentum_icrf_kgmps),
                    1.0e-12,
                    "Internal revolute/prismatic reactions cannot change total linear momentum."));
                const Vec3d expected_position = request.initial_state.position_icrf_m +
                    request.final_ephemeris_time_tdb_seconds *
                    request.initial_state.velocity_icrf_mps;
                report.checks.push_back(Check(
                    "Nested tree center of mass follows uniform translation",
                    (last.state.position_icrf_m - expected_position).Norm(),
                    1.0e-10,
                    "With no external force, r_CM(t)=r_CM(0)+v_CM(0)t."));
            }
        }

        {
            // Case 8: propagate one analytic circular orbit for one Keplerian period using
            // point-mass Earth gravity. This tests the complete translation/RK4/recording path
            // independently of articulation and compares radius, energy, and orbit closure.
            constexpr double earth_mu_m3ps2 = 3.986004418e14;
            constexpr double earth_radius_m = 6378137.0;
            const double orbit_radius_m = earth_radius_m + 400000.0;
            // Circular conditions: v_c=sqrt(mu/r), T=2*pi*sqrt(r^3/mu).
            const double circular_speed_mps = std::sqrt(earth_mu_m3ps2 / orbit_radius_m);
            const double orbit_period_seconds = 2.0 * kPi *
                std::sqrt(orbit_radius_m * orbit_radius_m * orbit_radius_m / earth_mu_m3ps2);

            SimulationRequest request;
            request.scenario_name = "Circular 400 km Earth orbit";
            request.initial_state.position_icrf_m = {orbit_radius_m, 0.0, 0.0};
            request.initial_state.velocity_icrf_mps = {0.0, circular_speed_mps, 0.0};
            request.vehicle.components.push_back(MakeComponent(
                "Satellite bus", 100.0, Vec3d::Zero(), {40.0, 50.0, 60.0}));
            GravityBody earth;
            earth.name = "Earth";
            earth.gravitational_parameter_m3ps2 = earth_mu_m3ps2;
            earth.reference_radius_m = earth_radius_m;
            request.gravity.bodies.push_back(earth);
            request.final_ephemeris_time_tdb_seconds = orbit_period_seconds;
            request.maximum_integrator_step_seconds = 10.0;
            request.output_mode = OutputMode::FixedInterval;
            request.output_step_seconds = 60.0;

            const SimulationResult result = SimulationEngine().Run(request);
            const bool has_samples = result.success && result.samples.size() >= 2;
            const double run_error = has_samples ? 0.0 : 1.0;
            report.checks.push_back(Check(
                "Circular Earth orbit completes",
                run_error,
                0.0,
                "General6DofDynamics must propagate one point-mass Earth orbit."));
            if (has_samples)
            {
                const SpacecraftState& final_state = result.samples.back().state;
                const double final_radius_error = std::abs(final_state.position_icrf_m.Norm() - orbit_radius_m);
                report.checks.push_back(Check(
                    "Circular orbit radius remains bounded",
                    final_radius_error,
                    1.0,
                    "RK4 at 10 s should return to the circular radius within one meter."));
                // Specific mechanical energy: epsilon=|v|^2/2-mu/|r|.
                const double initial_energy = 0.5 * circular_speed_mps * circular_speed_mps -
                    earth_mu_m3ps2 / orbit_radius_m;
                const double final_energy = 0.5 * final_state.velocity_icrf_mps.NormSquared() -
                    earth_mu_m3ps2 / final_state.position_icrf_m.Norm();
                report.checks.push_back(Check(
                    "Circular orbit conserves specific energy",
                    std::abs((final_energy - initial_energy) / initial_energy),
                    1.0e-9,
                    "Point-mass gravity is conservative; remaining error is RK4 truncation."));
                report.checks.push_back(Check(
                    "Circular orbit closes after one period",
                    (final_state.position_icrf_m - request.initial_state.position_icrf_m).Norm(),
                    20.0,
                    "The numerical state should return near its starting position after one analytic period."));
            }
        }

        {
            // Case 9: scalar curves use linear interpolation and retain an
            // internal, explicit extrapolation policy.
            ScalarCurve curve;
            curve.samples = {{0.0, 0.0}, {1.0, 2.0}, {2.0, 4.0}, {3.0, 6.0}};
            curve.extrapolation = ScalarExtrapolationMethod::ExtendEndpointSlope;
            const double interpolation_error = std::abs(curve.ValueAt(1.5) - 3.0);
            const double extrapolation_error = std::abs(curve.ValueAt(-0.5) + 1.0);
            ScalarCurve endpoint_clamped_curve;
            endpoint_clamped_curve.samples = curve.samples;
            const double endpoint_clamp_error = std::max(
                std::abs(endpoint_clamped_curve.ValueAt(-0.5)),
                std::abs(endpoint_clamped_curve.ValueAt(3.5) - 6.0));
            report.checks.push_back(Check(
                "Scalar curve linearly interpolates samples",
                interpolation_error,
                1.0e-12,
                "The public profile contract uses piecewise-linear interpolation."));
            report.checks.push_back(Check(
                "Scalar curve extends the selected endpoint slope",
                extrapolation_error,
                1.0e-12,
                "Extrapolation is a caller-selected policy rather than implicit clamping."));
            report.checks.push_back(Check(
                "Scalar curve clamps endpoints by default",
                endpoint_clamp_error,
                1.0e-12,
                "HUD-imported curves rely on the backend's default closest-sample behavior."));

            ScalarCurve changing_slope_curve;
            changing_slope_curve.samples = {
                {0.0, 0.0}, {1.0, 2.0}, {3.0, 8.0}};
            changing_slope_curve.extrapolation =
                ScalarExtrapolationMethod::ExtendEndpointSlope;
            const double analytic_rate_error = std::max({
                std::abs(changing_slope_curve.RateAt(-1.0) - 2.0),
                std::abs(changing_slope_curve.RateAt(0.0) - 2.0),
                std::abs(changing_slope_curve.RateAt(1.0) - 3.0),
                std::abs(changing_slope_curve.RateAt(3.0) - 3.0),
                std::abs(changing_slope_curve.RateAt(4.0) - 3.0)});
            report.checks.push_back(Check(
                "Scalar curve exposes analytic one-sided rates",
                analytic_rate_error,
                1.0e-12,
                "RateAt must match piecewise-linear interpolation, choose the right segment at knots, and extend endpoint slopes when requested."));

            changing_slope_curve.extrapolation =
                ScalarExtrapolationMethod::ClampToEndpoint;
            ScalarCurve malformed_curve;
            malformed_curve.samples = {
                {0.0, 1.0}, {0.0, 2.0}};
            const double zero_rate_error = std::max({
                std::abs(changing_slope_curve.RateAt(-1.0)),
                std::abs(changing_slope_curve.RateAt(3.0)),
                std::abs(changing_slope_curve.RateAt(4.0)),
                std::abs(malformed_curve.RateAt(0.0)),
                std::abs(ScalarCurve{}.RateAt(0.0))});
            report.checks.push_back(Check(
                "Scalar curve rates honor clamp and malformed safeguards",
                zero_rate_error,
                1.0e-12,
                "Clamped/default extrapolation and invalid or underspecified curves have deterministic zero rates."));

            ControlCommandWriter command_writer(2, 0, 0);
            const bool initially_omitted =
                command_writer.Command().
                    thruster_mass_flow_derivatives_kgps2.size() == 2 &&
                !command_writer.Command().
                    thruster_mass_flow_derivatives_kgps2[0].has_value() &&
                !command_writer.Command().
                    thruster_mass_flow_derivatives_kgps2[1].has_value();
            const bool setter_contract =
                command_writer.SetThrusterMassFlowDerivative(0, 0.0) &&
                command_writer.SetThrusterMassFlowDerivative(1, -0.25) &&
                !command_writer.SetThrusterMassFlowDerivative(
                    2, 1.0) &&
                !command_writer.SetThrusterMassFlowDerivative(
                    0, std::numeric_limits<double>::infinity()) &&
                command_writer.Command().
                    thruster_mass_flow_derivatives_kgps2[0].value() == 0.0 &&
                command_writer.Command().
                    thruster_mass_flow_derivatives_kgps2[1].value() == -0.25;
            report.checks.push_back(Check(
                "Optional q-dot writer distinguishes omission and supplied zero",
                initially_omitted && setter_contract ? 0.0 : 1.0,
                0.0,
                "Every writer starts omitted, accepts finite signed values, and rejects invalid indices and nonfinite derivatives."));
        }

        {
            // Case 10: uploaded atmosphere interpolation and Earth CHP bulge.
            AtmosphereSettings atmosphere;
            atmosphere.enabled = true;
            atmosphere.model_kind = AtmosphereModelKind::TabulatedProfile;
            atmosphere.density_profile = {
                {0.0, 1.0},
                {1000.0, 0.01}};
            atmosphere.thermodynamic_profile = {
                {0.0, 200.0, 4.0e-26, 1.0e-19},
                {1000.0, 300.0, 6.0e-26, 3.0e-19}};
            const AtmosphereState midpoint = EvaluateAtmosphereState(
                atmosphere,
                500.0,
                Vec3d::UnitX(),
                Vec3d::UnitY(),
                Mat3d::Identity(),
                Vec3d::UnitX());
            const double interpolation_error =
                std::abs(midpoint.density_kgpm3 - 0.1) +
                std::abs(midpoint.temperature_k - 250.0) +
                std::abs(midpoint.mean_particle_mass_kg - 5.0e-26) / 1.0e-26 +
                std::abs(
                    midpoint.effective_collision_cross_section_m2 - 2.0e-19) /
                    1.0e-19;
            report.checks.push_back(Check(
                "Uploaded atmosphere interpolates density and molecular properties",
                interpolation_error,
                1.0e-12,
                "Density is log-linear; T, mean particle mass, and collision cross-section are linear."));

            AtmosphereSettings harris_priester;
            harris_priester.enabled = true;
            harris_priester.model_kind =
                AtmosphereModelKind::CubicHarrisPriesterEarth;
            harris_priester.thermodynamic_profile = {
                {-100.0, 500.0, 5.0e-26, 2.0e-19},
                {1000.0, 500.0, 5.0e-26, 2.0e-19}};
            CubicHarrisPriesterDensitySample lower;
            lower.altitude_m = 0.0;
            lower.maximum_density_coefficients_kgpm3[0] = 8.0e-9;
            lower.minimum_density_coefficients_kgpm3[0] = 4.0e-9;
            CubicHarrisPriesterDensitySample upper;
            upper.altitude_m = 1000.0;
            upper.maximum_density_coefficients_kgpm3[0] = 2.0e-9;
            upper.minimum_density_coefficients_kgpm3[0] = 1.0e-9;
            harris_priester.cubic_harris_priester.density_samples = {
                lower, upper};
            const Vec3d sun_direction = RotationAroundAxis(
                Vec3d::UnitZ(), -kPi / 6.0) * Vec3d::UnitX();
            const AtmosphereState bulge_apex = EvaluateAtmosphereState(
                harris_priester,
                0.0,
                {6378137.0, 0.0, 0.0},
                {0.0, 7500.0, 0.0},
                Mat3d::Identity(),
                sun_direction);
            report.checks.push_back(Check(
                "Cubic Harris-Priester reaches rho_max at the diurnal bulge apex",
                std::abs(bulge_apex.density_kgpm3 - 8.0e-9),
                1.0e-20,
                "At a coefficient station with psi=0, cos^n(psi/2)=1 and rho=rho_max."));
        }

        {
            // Case 11: analytical TG-1 Eq. (92) against a centered directional difference.
            SimulationRequest request = MakeNestedFreeFlightRequest();
            ComponentDefinition& moving_mass = request.vehicle.components[2];
            moving_mass.minimum_mass_kg = 0.5;
            moving_mass.variable_mass_state_index = 0;
            request.initial_state.variable_component_masses_kg = {1.0};
            request.initial_state.articulation_rates = {0.31, -0.12};
            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            const std::vector<double> mass_rates = {-0.02};
            const MassProperties analytical = MassPropertiesModel().Compute(
                config.vehicle,
                config.initial_state,
                mass_rates,
                config.initial_state.articulation_rates);

            constexpr double epsilon_seconds = 1.0e-6;
            SpacecraftState plus = config.initial_state;
            SpacecraftState minus = config.initial_state;
            plus.variable_component_masses_kg[0] += mass_rates[0] * epsilon_seconds;
            minus.variable_component_masses_kg[0] -= mass_rates[0] * epsilon_seconds;
            for (std::size_t index = 0; index < plus.articulation_coordinates.size(); ++index)
            {
                plus.articulation_coordinates[index] +=
                    plus.articulation_rates[index] * epsilon_seconds;
                minus.articulation_coordinates[index] -=
                    minus.articulation_rates[index] * epsilon_seconds;
            }
            const Mat3d numerical_rate =
                (MassPropertiesModel().Compute(config.vehicle, plus).inertia_body_kgm2 -
                    MassPropertiesModel().Compute(config.vehicle, minus).inertia_body_kgm2) *
                (0.5 / epsilon_seconds);
            report.checks.push_back(Check(
                "Analytical inertia rate matches a centered directional derivative",
                MaximumAbsoluteDifference(
                    analytical.inertia_rate_body_kgm2ps, numerical_rate),
                2.0e-8,
                "TG-1 Eq. (92) must include articulation and scaled variable-mass terms."));
        }

        {
            // Case 12: complete solar occultation from apparent-disk geometry.
            SimulationRequest request = MakeRigidRequest();
            GravityBody sun;
            sun.name = "Sun";
            sun.gravity_enabled = false;
            sun.reference_radius_m = 100.0;
            sun.position_icrf_at_epoch_m = {1000.0, 0.0, 0.0};
            GravityBody occulter;
            occulter.name = "Occulter";
            occulter.gravity_enabled = false;
            occulter.reference_radius_m = 60.0;
            occulter.position_icrf_at_epoch_m = {500.0, 0.0, 0.0};
            request.gravity.bodies = {sun, occulter};
            request.solar_radiation.enabled = true;
            OpticalFacet facet;
            facet.component_index = 0;
            facet.vertices_component_m = {
                Vec3d{0.0, -0.5, -0.5},
                Vec3d{0.0, 0.5, -0.5},
                Vec3d{0.0, 0.0, 0.5}};
            request.solar_radiation.facets.push_back(facet);
            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            const DynamicsEvaluation evaluation = General6DofDynamics(config).ComputeDynamics(
                0.0, config.initial_state, config);
            report.checks.push_back(Check(
                "Apparent-disk eclipse reaches full umbra",
                std::abs(evaluation.applied_force_torque.visible_sun_fraction),
                1.0e-12,
                "A larger aligned foreground disk must hide the complete solar disk."));

            // Eclipse visibility is also exported for visualization when the
            // scenario does not request SRP force or provide optical facets.
            request.solar_radiation.enabled = false;
            request.solar_radiation.facets.clear();
            const SimulationConfig telemetry_only_config =
                SimulationConfigBuilder().Build(request);
            const DynamicsEvaluation telemetry_only_evaluation =
                General6DofDynamics(telemetry_only_config).ComputeDynamics(
                    0.0,
                    telemetry_only_config.initial_state,
                    telemetry_only_config);
            report.checks.push_back(Check(
                "Eclipse telemetry remains available with SRP disabled",
                std::abs(
                    telemetry_only_evaluation.applied_force_torque.
                        visible_sun_fraction),
                1.0e-12,
                "Visualization must receive celestial visibility without requiring SRP facets."));
        }

        {
            // Component-shadow quadrature: one small back-facing triangle on a
            // fixed child blocks exactly one of a larger receiver's three samples.
            SimulationRequest request = MakeRigidRequest();
            request.initial_state.angular_velocity_body_radps = Vec3d::Zero();

            ComponentDefinition occluder_component = MakeComponent(
                "Fixed occluder", 1.0, Vec3d::Zero(), {0.1, 0.1, 0.1});
            occluder_component.parent_component_index = 0;
            occluder_component.articulation_to_parent.
                parent_anchor_component_m = {0.5, 0.0, 0.0};
            request.vehicle.components.push_back(occluder_component);

            GravityBody sun;
            sun.name = "Sun";
            sun.gravity_enabled = false;
            sun.reference_radius_m = 1.0;
            sun.position_icrf_at_epoch_m =
                {kAstronomicalUnitM, 0.0, 0.0};
            request.gravity.bodies = {sun};

            request.solar_radiation.enabled = true;
            request.solar_radiation.compute_eclipse_shadow = false;
            request.solar_radiation.compute_component_shadows = true;

            OpticalFacet receiver;
            receiver.name = "Receiver";
            receiver.component_index = 0;
            // Winding gives outward normal +X and area 2 m^2.
            receiver.vertices_component_m = {
                Vec3d{0.0, -1.0, -1.0},
                Vec3d{0.0, 1.0, -1.0},
                Vec3d{0.0, 0.0, 1.0}};
            request.solar_radiation.facets.push_back(receiver);

            OpticalFacet occluder;
            occluder.name = "One-sample occluder";
            occluder.component_index = 1;
            // Winding gives -X, so this triangle casts an opaque shadow but
            // contributes no direct SRP load for a Sun in +X.
            occluder.vertices_component_m = {
                Vec3d{0.0, -0.6, -0.7666666666666667},
                Vec3d{0.0, -0.5, -0.4666666666666667},
                Vec3d{0.0, -0.4, -0.7666666666666667}};
            request.solar_radiation.facets.push_back(occluder);

            const SimulationConfig shadowed_config =
                SimulationConfigBuilder().Build(request);
            const DynamicsEvaluation shadowed =
                General6DofDynamics(shadowed_config).ComputeDynamics(
                    0.0,
                    shadowed_config.initial_state,
                    shadowed_config);

            request.solar_radiation.compute_component_shadows = false;
            const SimulationConfig unshadowed_config =
                SimulationConfigBuilder().Build(request);
            const DynamicsEvaluation unshadowed =
                General6DofDynamics(unshadowed_config).ComputeDynamics(
                    0.0,
                    unshadowed_config.initial_state,
                    unshadowed_config);

            const double shadow_ratio =
                shadowed.applied_force_torque.
                    solar_radiation_force_icrf_n.x /
                unshadowed.applied_force_torque.
                    solar_radiation_force_icrf_n.x;
            report.checks.push_back(Check(
                "Component shadow blocks one of three SRP samples",
                std::abs(shadow_ratio - 2.0 / 3.0),
                1.0e-12,
                "Three-point triangle visibility must retain exactly two unblocked area shares."));
        }

        {
            // Provider-owned gravity metadata must replace caller placeholders.
            SimulationRequest request = MakeRigidRequest();
            GravityBody body;
            body.name = "Metadata body";
            body.gravitational_parameter_m3ps2 = 1.0;
            body.reference_radius_m = 2.0;
            request.gravity.bodies.push_back(body);
            request.gravity.ephemeris_provider =
                std::make_shared<FixedGravityMetadataProvider>();
            const SimulationConfig config =
                SimulationConfigBuilder().Build(request);
            const double metadata_error =
                std::abs(
                    config.gravity.bodies[0].
                        gravitational_parameter_m3ps2 - 1234.5) +
                std::abs(config.gravity.bodies[0].reference_radius_m - 6789.0);
            report.checks.push_back(Check(
                "Gravity metadata comes from the ephemeris provider",
                metadata_error,
                1.0e-12,
                "HUD/request placeholders must not override SPICE GM or radius."));
        }

        {
            // A harmonic solution must use the GM and mathematical reference
            // radius published with its coefficients, not the SPICE shape data.
            GravityBody harmonic_body;
            harmonic_body.name = "Harmonic model body";
            harmonic_body.gravity_enabled = true;
            harmonic_body.gravitational_parameter_m3ps2 = 100.0;
            harmonic_body.reference_radius_m = 9.0;
            harmonic_body.harmonic_model_gravitational_parameter_m3ps2 = 200.0;
            harmonic_body.harmonic_model_reference_radius_m = 2.0;
            harmonic_body.maximum_harmonic_degree = 2;
            harmonic_body.harmonics.push_back({2, 0, 1.0e-3, 0.0});

            const auto evaluate_harmonic_x = [](const GravityBody& body)
            {
                SimulationConfig config;
                config.gravity.bodies = {body};
                SpacecraftState state;
                state.position_icrf_m = {10.0, 0.0, 0.0};
                state.mass_kg = 1.0;
                return GravityModel().ComputeGravity(
                    0.0, state, config, Vec3d::Zero(), {}).
                    acceleration_icrf_mps2.x;
            };

            GravityBody monopole_only = harmonic_body;
            monopole_only.harmonics[0].normalized_c = 0.0;
            const double model_gm_error = std::abs(
                evaluate_harmonic_x(monopole_only) - (-200.0 / 100.0));
            report.checks.push_back(Check(
                "Harmonic monopole uses uploaded model GM",
                model_gm_error,
                1.0e-12,
                "SPICE GM must not replace the coefficient model's GM."));

            const double point_mass_acceleration = -200.0 / 100.0;
            const double perturbation_at_r2 =
                evaluate_harmonic_x(harmonic_body) - point_mass_acceleration;
            GravityBody doubled_radius = harmonic_body;
            doubled_radius.harmonic_model_reference_radius_m = 4.0;
            const double perturbation_at_r4 =
                evaluate_harmonic_x(doubled_radius) - point_mass_acceleration;
            report.checks.push_back(Check(
                "Degree-two harmonic scales with uploaded reference radius",
                std::abs(perturbation_at_r4 - 4.0 * perturbation_at_r2),
                1.0e-12,
                "At fixed coefficients, the degree-two perturbation scales as R squared."));

            SimulationRequest missing_constants = MakeRigidRequest();
            GravityBody invalid_harmonic_body = harmonic_body;
            invalid_harmonic_body.harmonic_model_gravitational_parameter_m3ps2 = 0.0;
            invalid_harmonic_body.harmonic_model_reference_radius_m = 0.0;
            missing_constants.gravity.bodies.push_back(invalid_harmonic_body);
            const SimulationResult invalid_result =
                SimulationEngine().Run(missing_constants);
            report.checks.push_back(Check(
                "Harmonic model rejects missing CSV GM and radius",
                invalid_result.success ? 1.0 : 0.0,
                0.0,
                "A positive maximum degree requires both constants from the CSV first row."));
        }

        {
            // Grouped gravity selection must never add a system barycenter and
            // any mass represented by that barycenter in the same evaluation.
            GravityBody barycenter;
            barycenter.name = "Test barycenter";
            barycenter.naif_id = 100;
            barycenter.gravity_system_name = "Test system";
            barycenter.gravity_source_role = GravitySourceRole::SystemBarycenter;
            barycenter.gravity_enabled = false;
            barycenter.barycenter_resolution_radius_m = 50.0;
            barycenter.gravitational_parameter_m3ps2 = 100.0;

            GravityBody planet;
            planet.name = "Test planet";
            planet.naif_id = 101;
            planet.gravity_system_name = "Test system";
            planet.gravity_source_role = GravitySourceRole::SystemMember;
            planet.gravity_enabled = false;
            planet.gravitational_parameter_m3ps2 = 10.0;
            planet.position_icrf_at_epoch_m = {10.0, 0.0, 0.0};

            GravityBody moon;
            moon.name = "Test moon";
            moon.naif_id = 102;
            moon.gravity_system_name = "Test system";
            moon.gravity_source_role = GravitySourceRole::SystemMember;
            moon.gravity_enabled = false;
            moon.gravitational_parameter_m3ps2 = 5.0;
            moon.position_icrf_at_epoch_m = {-10.0, 0.0, 0.0};

            const auto evaluate_x = [](
                const std::vector<GravityBody>& bodies,
                double spacecraft_x_m)
            {
                SimulationConfig config;
                config.gravity.bodies = bodies;
                SpacecraftState state;
                state.position_icrf_m = {spacecraft_x_m, 0.0, 0.0};
                state.mass_kg = 1.0;
                return GravityModel().ComputeGravity(
                    0.0, state, config, Vec3d::Zero(), {}).
                    acceleration_icrf_mps2.x;
            };
            const auto point_mass_x = [](
                double mu_m3ps2,
                double source_x_m,
                double spacecraft_x_m)
            {
                const double displacement = spacecraft_x_m - source_x_m;
                return -mu_m3ps2 * displacement /
                    std::pow(std::abs(displacement), 3.0);
            };

            report.checks.push_back(Check(
                "Unchecked gravity system remains inactive",
                std::abs(evaluate_x({barycenter, planet, moon}, 100.0)),
                1.0e-14,
                "No checkbox or activation radius means no gravity contribution."));

            GravityBody selected_barycenter = barycenter;
            selected_barycenter.gravity_enabled = true;
            const double far_barycenter_error = std::abs(
                evaluate_x({selected_barycenter, planet, moon}, 100.0) -
                point_mass_x(100.0, 0.0, 100.0));
            report.checks.push_back(Check(
                "Far field uses only the selected system barycenter",
                far_barycenter_error,
                1.0e-14,
                "The member masses must not also be added in the far field."));

            GravityBody selected_planet = planet;
            selected_planet.gravity_enabled = true;
            const double explicit_member_error = std::abs(
                evaluate_x(
                    {selected_barycenter, selected_planet, moon}, 100.0) -
                point_mass_x(10.0, 10.0, 100.0));
            report.checks.push_back(Check(
                "Explicit member suppresses its selected barycenter",
                explicit_member_error,
                1.0e-14,
                "Member selection has priority even for a malformed double-checked request."));

            const double resolved_members_expected =
                point_mass_x(10.0, 10.0, 20.0) +
                point_mass_x(5.0, -10.0, 20.0);
            const double resolution_error = std::abs(
                evaluate_x({selected_barycenter, planet, moon}, 20.0) -
                resolved_members_expected);
            report.checks.push_back(Check(
                "Near field replaces barycenter with every system member",
                resolution_error,
                1.0e-14,
                "Inside the resolution radius, only the resolved member set is summed."));

            GravityBody automatic_planet = planet;
            automatic_planet.automatic_gravity_activation_radius_m = 100.0;
            const double automatic_member_error = std::abs(
                evaluate_x(
                    {selected_barycenter, automatic_planet, moon}, 100.0) -
                point_mass_x(10.0, 10.0, 100.0));
            report.checks.push_back(Check(
                "Automatically activated member suppresses barycenter",
                automatic_member_error,
                1.0e-14,
                "Automatic activation must not double-count the represented system mass."));

            GravityBody automatic_barycenter = barycenter;
            automatic_barycenter.automatic_gravity_activation_radius_m = 150.0;
            const double automatic_barycenter_error = std::abs(
                evaluate_x({automatic_barycenter, planet, moon}, 100.0) -
                point_mass_x(100.0, 0.0, 100.0));
            report.checks.push_back(Check(
                "Unchecked barycenter activates inside its own radius",
                automatic_barycenter_error,
                1.0e-14,
                "Automatic activation and member-resolution radii have distinct meanings."));

            SimulationRequest double_checked_request;
            double_checked_request.gravity.bodies =
                {selected_barycenter, selected_planet, moon};
            const SimulationConfig normalized =
                SimulationConfigBuilder().Build(double_checked_request);
            report.checks.push_back(Check(
                "Configuration normalization clears a double-checked barycenter",
                normalized.gravity.bodies[0].gravity_enabled ? 1.0 : 0.0,
                0.0,
                "The backend safety net mirrors the HUD's mutually exclusive checkboxes."));
        }

        {
            // Case 13: adaptive propagation plus recorded fallback ephemerides.
            SimulationRequest request = MakeRigidRequest();
            request.integrator_kind = IntegratorKind::AdaptiveDormandPrince54;
            request.initial_state.position_icrf_m = {1.0, 2.0, 3.0};
            request.initial_state.velocity_icrf_mps = {4.0, -2.0, 1.0};
            request.initial_state.angular_velocity_body_radps = Vec3d::Zero();
            request.final_ephemeris_time_tdb_seconds = 10.0;
            request.maximum_integrator_step_seconds = 2.0;
            request.initial_integrator_step_seconds = 0.1;
            request.maximum_integration_steps = 1000;
            request.output_mode = OutputMode::FixedInterval;
            request.output_step_seconds = 2.0;
            GravityBody catalog_body;
            catalog_body.name = "Catalog body";
            catalog_body.gravity_enabled = false;
            catalog_body.epoch_ephemeris_time_tdb_seconds = 0.0;
            catalog_body.position_icrf_at_epoch_m = {10.0, 20.0, 30.0};
            catalog_body.velocity_icrf_mps = {1.0, 0.0, -1.0};
            request.gravity.bodies.push_back(catalog_body);

            const SimulationResult result = SimulationEngine().Run(request);
            const bool has_result = result.success && !result.samples.empty();
            const Vec3d expected_spacecraft_position =
                request.initial_state.position_icrf_m +
                request.initial_state.velocity_icrf_mps * 10.0;
            const double propagation_error = has_result
                ? (result.samples.back().state.position_icrf_m -
                    expected_spacecraft_position).Norm()
                : 1.0;
            report.checks.push_back(Check(
                "Adaptive Dormand-Prince propagates uniform translation",
                propagation_error,
                1.0e-10,
                result.success
                    ? "Boost.Odeint reached the requested output epoch with controlled substeps."
                    : result.message));

            const bool has_body_state = has_result &&
                result.samples.back().celestial_bodies.size() == 1;
            const Vec3d expected_body_position = {20.0, 20.0, 20.0};
            const double ephemeris_error = has_body_state
                ? (result.samples.back().celestial_bodies[0].position_icrf_m -
                    expected_body_position).Norm()
                : 1.0;
            report.checks.push_back(Check(
                "Simulation result records configured body ephemerides",
                ephemeris_error,
                1.0e-12,
                "Unreal playback must receive each configured body's ICRF state."));
        }

        {
            // Case 14: scattered speed-ratio/log10(Kn) interpolation and coverage checks.
            AerodynamicCoefficientDatabase database;
            database.enabled = true;
            database.nearest_neighbor_count = 4;
            for (double speed_ratio : {0.0, 2.0})
            {
                for (double log_knudsen : {-1.0, 1.0})
                {
                    AerodynamicCoefficientSample sample;
                    sample.molecular_speed_ratio = speed_ratio;
                    sample.knudsen_number = std::pow(10.0, log_knudsen);
                    sample.incoming_flow_direction_body = Vec3d::UnitX();
                    const double value = speed_ratio + log_knudsen;
                    sample.force_coefficients_body = {value, 0.0, 0.0};
                    sample.moment_coefficients_body_about_reference =
                        {0.0, 0.0, 2.0 * value};
                    database.samples.push_back(sample);
                }
            }
            const std::string validation_error =
                ValidateAerodynamicCoefficientDatabase(database, 0);
            AerodynamicCoefficientInterpolator interpolator(database);
            AerodynamicCoefficientResult coefficients;
            const std::vector<double> no_articulations;
            const bool interpolated = validation_error.empty() &&
                interpolator.Interpolate(
                    {1.0, 1.0, Vec3d::UnitX(), &no_articulations},
                    coefficients);
            const double interpolation_error = interpolated
                ? std::abs(coefficients.force_coefficients_body.x - 1.0) +
                    std::abs(
                        coefficients.moment_coefficients_body_about_reference.z - 2.0)
                : 1.0;
            report.checks.push_back(Check(
                "Scattered aerodynamic database interpolates six-output rows",
                interpolation_error,
                1.0e-12,
                "Symmetric inverse-distance weights must reproduce the known center value."));

            AerodynamicCoefficientDatabase insufficient;
            insufficient.enabled = true;
            for (int index = 0; index < 2; ++index)
            {
                AerodynamicCoefficientSample sample;
                sample.molecular_speed_ratio = static_cast<double>(index);
                sample.knudsen_number = index == 0 ? 0.1 : 1.0;
                sample.incoming_flow_direction_body = index == 0
                    ? Vec3d::UnitX()
                    : Vec3d::UnitY();
                sample.articulation_coordinates = {static_cast<double>(index)};
                insufficient.samples.push_back(sample);
            }
            report.checks.push_back(Check(
                "Aerodynamic database rejects insufficient dimensional coverage",
                ValidateAerodynamicCoefficientDatabase(insufficient, 1).empty()
                    ? 1.0
                    : 0.0,
                0.0,
                "Two rows cannot span varying speed ratio, Kn, direction, and eta coordinates."));
        }

        {
            // Case 15: database loads, reference-point transport, speed ratio,
            // and the whole-spacecraft constant-drag fallback.
            SimulationRequest request = MakeRigidRequest();
            GravityBody earth;
            earth.name = "Earth";
            earth.gravity_enabled = false;
            earth.reference_radius_m = 6371.0e3;
            request.gravity.bodies = {earth};
            request.atmosphere.enabled = true;
            request.atmosphere.model_kind =
                AtmosphereModelKind::TabulatedProfile;
            request.atmosphere.central_body_name = "Earth";
            constexpr double test_speed_mps = 1000.0;
            constexpr double test_temperature_k = 300.0;
            constexpr double boltzmann_constant_jpk = 1.380649e-23;
            const double mean_particle_mass_kg =
                2.0 * boltzmann_constant_jpk * test_temperature_k /
                (test_speed_mps * test_speed_mps);
            constexpr double target_mean_free_path_m = 60.0;
            const double collision_cross_section_m2 =
                mean_particle_mass_kg /
                (std::sqrt(2.0) * target_mean_free_path_m);
            request.atmosphere.density_profile = {
                {-100.0, 1.0}, {100.0, 1.0}};
            request.atmosphere.thermodynamic_profile = {
                {-100.0, test_temperature_k, mean_particle_mass_kg,
                    collision_cross_section_m2},
                {100.0, test_temperature_k, mean_particle_mass_kg,
                    collision_cross_section_m2}};

            request.aerodynamics.enabled = true;
            request.aerodynamics.reference_area_m2 = 2.0;
            request.aerodynamics.reference_length_m = 3.0;
            request.aerodynamics.maximum_dynamic_pressure_pa = 1.0e9;
            request.aerodynamics.coefficient_database.enabled = true;
            request.aerodynamics.coefficient_database.extrapolation =
                AerodynamicDatabaseExtrapolationMethod::UseConstantDragFallback;
            request.aerodynamics.coefficient_database.moment_reference_point_body_m =
                {1.0, 0.0, 0.0};
            request.aerodynamics.constant_drag_fallback_enabled = true;
            request.aerodynamics.fallback_drag_coefficient = 2.2;
            AerodynamicCoefficientSample sample;
            sample.molecular_speed_ratio = 1.0;
            sample.knudsen_number = 20.0;
            sample.incoming_flow_direction_body = -Vec3d::UnitY();
            sample.force_coefficients_body = {0.0, 1.0, 0.0};
            sample.moment_coefficients_body_about_reference = {0.0, 0.0, 0.5};
            request.aerodynamics.coefficient_database.samples.push_back(sample);

            request.initial_state.position_icrf_m = {earth.reference_radius_m, 0.0, 0.0};
            request.initial_state.velocity_icrf_mps = {0.0, test_speed_mps, 0.0};
            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            const General6DofDynamics dynamics(config);
            const DynamicsEvaluation database_evaluation = dynamics.ComputeDynamics(
                0.0, config.initial_state, config);
            const double dynamic_pressure =
                0.5 * test_speed_mps * test_speed_mps;
            const Vec3d expected_force = {0.0, 2.0 * dynamic_pressure, 0.0};
            // M_R,z = q S L C_m = 3q; (r_R-r_CM) x F contributes 2q.
            const Vec3d expected_moment = {0.0, 0.0, 5.0 * dynamic_pressure};
            const ForceTorqueSample& database_loads =
                database_evaluation.applied_force_torque;
            const double database_load_error =
                (database_loads.aerodynamic_force_icrf_n - expected_force).Norm() +
                (database_loads.aerodynamic_torque_body_nm - expected_moment).Norm() +
                std::abs(
                    database_loads.aerodynamic_molecular_speed_ratio - 1.0) +
                (database_loads.aerodynamic_database_used ? 0.0 : 1.0);
            report.checks.push_back(Check(
                "Aerodynamic moment is transported from reference point to CM",
                database_load_error,
                1.0e-9,
                "Database force/moment and r cross F transport must match the analytic load."));

            SpacecraftState outside_state = config.initial_state;
            outside_state.velocity_icrf_mps = {0.0, 2.0 * test_speed_mps, 0.0};
            const DynamicsEvaluation fallback_evaluation = dynamics.ComputeDynamics(
                0.0, outside_state, config);
            const ForceTorqueSample& fallback_loads =
                fallback_evaluation.applied_force_torque;
            const double fallback_selection_error =
                fallback_loads.aerodynamic_fallback_used &&
                !fallback_loads.aerodynamic_database_used
                    ? 0.0
                    : 1.0;
            report.checks.push_back(Check(
                "Out-of-domain aerodynamic query uses constant drag",
                fallback_selection_error,
                0.0,
                "UseConstantDragFallback must select drag rather than database extrapolation."));

            const double fallback_dynamic_pressure =
                0.5 * std::pow(2.0 * test_speed_mps, 2.0);
            const double expected_fallback_force_y =
                -fallback_dynamic_pressure *
                request.aerodynamics.reference_area_m2 *
                request.aerodynamics.fallback_drag_coefficient;
            const double fallback_load_error =
                std::abs(
                    fallback_loads.aerodynamic_force_icrf_n.y -
                    expected_fallback_force_y) +
                std::abs(fallback_loads.aerodynamic_force_icrf_n.x) +
                std::abs(fallback_loads.aerodynamic_force_icrf_n.z) +
                fallback_loads.aerodynamic_torque_body_nm.Norm();
            report.checks.push_back(Check(
                "Constant-drag fallback matches drag-only CM load",
                fallback_load_error,
                1.0e-9,
                "F_D=-q S_ref C_D v_rel/|v_rel| and M_CM=0."));
        }

        {
            // Case 16: a user controller writes into the backend-owned command object.
            SimulationRequest request = MakeRigidRequest();
            request.initial_state.angular_velocity_body_radps = Vec3d::Zero();
            request.control.controller =
                std::make_shared<ConstantBodyTorqueController>(
                    Vec3d{2.0, 0.0, 0.0});
            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            const DynamicsEvaluation evaluation =
                General6DofDynamics(config).ComputeDynamics(
                    0.0, config.initial_state, config);
            report.checks.push_back(Check(
                "User controller output reaches forward dynamics",
                std::abs(evaluation.derivative.
                    angular_acceleration_body_radps2.x - 1.0),
                1.0e-12,
                "A 2 N m body-X torque acting on Ixx=2 kg m^2 must produce 1 rad/s^2."));
        }

        {
            // Case 17: unequal fixed components in a strongly curved point-mass field
            // produce the direct sum of component force moments about total CM.
            SimulationRequest request;
            request.scenario_name = "Distributed gravity-gradient validation";
            request.initial_state.position_icrf_m = {10.0, 0.0, 0.0};
            request.vehicle.components.push_back(MakeComponent(
                "Upper mass", 1.0, {0.0, 1.0, 0.0}, {0.2, 0.2, 0.2}));
            ComponentDefinition lower = MakeComponent(
                "Lower mass", 2.0, {0.0, -0.5, 0.0}, {0.2, 0.2, 0.2});
            lower.parent_component_index = 0;
            request.vehicle.components.push_back(lower);
            GravityBody body;
            body.name = "Test body";
            body.gravitational_parameter_m3ps2 = 1000.0;
            request.gravity.bodies.push_back(body);

            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            const DynamicsEvaluation evaluation =
                General6DofDynamics(config).ComputeDynamics(
                    0.0, config.initial_state, config);
            Vec3d expected_torque;
            for (const auto& mass_and_offset : {
                std::pair<double, Vec3d>{1.0, {0.0, 1.0, 0.0}},
                std::pair<double, Vec3d>{2.0, {0.0, -0.5, 0.0}}})
            {
                const Vec3d position = request.initial_state.position_icrf_m +
                    mass_and_offset.second;
                const Vec3d force = -body.gravitational_parameter_m3ps2 *
                    mass_and_offset.first * position /
                    std::pow(position.Norm(), 3.0);
                expected_torque += Cross(mass_and_offset.second, force);
            }
            report.checks.push_back(Check(
                "Distributed gravity produces component gravity-gradient torque",
                (evaluation.applied_force_torque.gravity_torque_body_nm -
                    expected_torque).Norm(),
                1.0e-12,
                "Each component gravity force must act at that component's own CM."));
            report.checks.push_back(Check(
                "Dynamics exposes every component pose for playback",
                evaluation.component_origins_body_m.size() == 2 &&
                    evaluation.component_to_body_rotations.size() == 2
                    ? 0.0 : 1.0,
                0.0,
                "Unreal playback needs one body-relative transform per configured component."));
        }

        {
            // Case 18: output storage is bounded independently of the integration cap.
            SimulationRequest request = MakeRigidRequest();
            request.final_ephemeris_time_tdb_seconds = 2.0;
            request.maximum_integrator_step_seconds = 1.0;
            request.maximum_output_samples = 2;
            const SimulationResult result = SimulationEngine().Run(request);
            report.checks.push_back(Check(
                "Maximum output-sample count stops an oversized result",
                !result.success && result.samples.size() == 2 ? 0.0 : 1.0,
                0.0,
                "The backend must fail cleanly before unbounded telemetry exhausts memory."));

            ImmediateCancellationObserver observer;
            const SimulationResult cancelled =
                SimulationEngine().Run(MakeRigidRequest(), &observer);
            report.checks.push_back(Check(
                "Simulation cancellation is observable before propagation",
                !cancelled.success &&
                    cancelled.message.find("cancelled") != std::string::npos
                    ? 0.0 : 1.0,
                0.0,
                "The future Unreal Cancel button needs a backend cancellation boundary."));
        }

        {
            // Case 19: a short authored duration at a large absolute SPICE epoch must
            // remain exactly 0.1 s instead of being reconstructed from two ET values.
            SimulationRequest request = MakeRigidRequest();
            request.start_ephemeris_time_tdb_seconds = 946731646.683092;
            request.requested_duration_seconds = 0.1;
            request.final_ephemeris_time_tdb_seconds =
                request.start_ephemeris_time_tdb_seconds +
                request.requested_duration_seconds;
            request.maximum_integrator_step_seconds = 0.001;
            request.output_mode = OutputMode::FixedInterval;
            request.output_step_seconds = 0.01;
            const SimulationResult result = SimulationEngine().Run(request);

            double time_grid_error = result.success &&
                    result.samples.size() == 11
                ? 0.0
                : 1.0;
            for (std::size_t index = 0;
                index < result.samples.size(); ++index)
            {
                time_grid_error = std::max(
                    time_grid_error,
                    std::abs(
                        result.samples[index].state.elapsed_time_seconds -
                        0.01 * static_cast<double>(index)));
                if (index > 0 &&
                    result.samples[index].state.elapsed_time_seconds <=
                        result.samples[index - 1].state.elapsed_time_seconds)
                {
                    time_grid_error = 1.0;
                }
            }
            const auto elapsed_column = std::find(
                result.column_names.begin(),
                result.column_names.end(),
                "elapsed_time_seconds");
            if (elapsed_column == result.column_names.end())
                time_grid_error = 1.0;
            report.checks.push_back(Check(
                "Large absolute epoch preserves the elapsed output grid",
                time_grid_error,
                1.0e-14,
                "Elapsed time must drive integration/output scheduling without a duplicate terminal sample."));

            // A UTC-derived final time can lie only a few nanoseconds beyond
            // the fixed output grid. At a large ET, both elapsed instants may
            // map to the same representable absolute timestamp.
            SimulationRequest terminal_tail_request = MakeRigidRequest();
            terminal_tail_request.start_ephemeris_time_tdb_seconds =
                -978846602.21625125;
            terminal_tail_request.requested_duration_seconds =
                0.10000004768371582;
            terminal_tail_request.final_ephemeris_time_tdb_seconds =
                terminal_tail_request.start_ephemeris_time_tdb_seconds +
                terminal_tail_request.requested_duration_seconds;
            terminal_tail_request.maximum_integrator_step_seconds = 0.05;
            terminal_tail_request.output_mode = OutputMode::FixedInterval;
            terminal_tail_request.output_step_seconds = 0.1;
            const SimulationResult terminal_tail_result =
                SimulationEngine().Run(terminal_tail_request);

            double terminal_tail_error =
                terminal_tail_result.success &&
                terminal_tail_result.samples.size() == 2
                    ? std::abs(
                        terminal_tail_result.samples.back().state.
                            elapsed_time_seconds -
                        terminal_tail_request.requested_duration_seconds)
                    : 1.0;
            for (std::size_t index = 1;
                index < terminal_tail_result.samples.size();
                ++index)
            {
                if (terminal_tail_result.samples[index].state.
                        ephemeris_time_tdb_seconds <=
                    terminal_tail_result.samples[index - 1].state.
                        ephemeris_time_tdb_seconds)
                {
                    terminal_tail_error = 1.0;
                }
            }
            report.checks.push_back(Check(
                "Sub-ULP terminal output tail is coalesced",
                terminal_tail_error,
                1.0e-14,
                "The final state must replace an indistinguishable fixed-grid timestamp instead of duplicating ET."));
        }

        {
            // Case 20: a strict controller cutoff at t=1 s lies inside the nominal
            // 0.6 s RK step. Its announced discontinuity must yield exactly 1 N m s.
            SimulationRequest request = MakeRigidRequest();
            request.initial_state.angular_velocity_body_radps = Vec3d::Zero();
            request.start_ephemeris_time_tdb_seconds = 946731646.683092;
            request.final_ephemeris_time_tdb_seconds =
                request.start_ephemeris_time_tdb_seconds + 2.0;
            request.maximum_integrator_step_seconds = 0.6;
            request.control.controller =
                std::make_shared<TimedBodyTorqueController>(
                    Vec3d{1.0, 0.0, 0.0}, 1.0);
            const SimulationResult result = SimulationEngine().Run(request);
            const double expected_angular_velocity = 0.5;
            const double error = result.success && !result.samples.empty()
                ? std::abs(result.samples.back().state.
                    angular_velocity_body_radps.x -
                    expected_angular_velocity)
                : 1.0;
            report.checks.push_back(Check(
                "Controller cutoff is integrated as an exact event",
                error,
                1.0e-11,
                "A 1 N m torque on Ixx=2 kg m^2 for exactly one second gives omega_x=0.5 rad/s."));
        }

        {
            // Case 21: wheel saturation is an event, not a post-step projection.
            // Internal wheel/body torques must therefore conserve total momentum.
            SimulationRequest request = MakeRigidRequest();
            request.initial_state.angular_velocity_body_radps = Vec3d::Zero();
            ReactionWheelDefinition wheel;
            wheel.name = "Validation wheel";
            wheel.axis_component = Vec3d::UnitX();
            wheel.maximum_momentum_nms = 0.2;
            request.vehicle.reaction_wheels.push_back(wheel);
            request.control.controller =
                std::make_shared<ConstantWheelRateController>(0.1);
            request.final_ephemeris_time_tdb_seconds = 3.0;
            request.maximum_integrator_step_seconds = 0.7;
            const SimulationResult result = SimulationEngine().Run(request);
            double error = 1.0;
            if (result.success && !result.samples.empty())
            {
                const TelemetrySample& final_sample = result.samples.back();
                error = std::abs(final_sample.state.
                    internal_angular_momenta_nms[0] - 0.2) +
                    final_sample.total_angular_momentum_about_cm_icrf_kgm2ps.
                        Norm();
            }
            report.checks.push_back(Check(
                "Reaction-wheel saturation conserves angular momentum",
                error,
                1.0e-10,
                "The wheel must stop at 0.2 N m s without a clamp-induced spacecraft impulse."));
        }

        {
            // Case 22: prescribed-profile knots and shutdown are native events. A
            // triangular 0-10-0 N curve over two seconds has exactly 10 N s impulse.
            SimulationRequest request;
            request.scenario_name = "Prescribed-profile event validation";
            ComponentDefinition tank = MakeComponent(
                "Variable-mass body", 10.0, Vec3d::Zero(),
                {1.0, 1.0, 1.0});
            tank.minimum_mass_kg = 9.0;
            tank.variable_mass_state_index = 0;
            request.vehicle.components.push_back(tank);
            request.start_ephemeris_time_tdb_seconds = 946731646.683092;
            request.final_ephemeris_time_tdb_seconds =
                request.start_ephemeris_time_tdb_seconds + 2.0;
            request.maximum_integrator_step_seconds = 0.7;
            ThrusterDefinition thruster;
            thruster.name = "Triangular profile";
            thruster.propellant_component_index = 0;
            thruster.ignition_ephemeris_time_tdb_seconds =
                request.start_ephemeris_time_tdb_seconds;
            thruster.shutdown_ephemeris_time_tdb_seconds =
                request.final_ephemeris_time_tdb_seconds;
            thruster.thrust_profile_n.samples = {
                {0.0, 0.0}, {1.0, 10.0}, {2.0, 0.0}};
            thruster.constant_specific_impulse_seconds = 200.0;
            request.vehicle.thrusters.push_back(thruster);
            const SimulationResult result = SimulationEngine().Run(request);
            const double expected_final_mass =
                10.0 - 10.0 / (200.0 * kStandardGravityMps2);
            const double error = result.success && !result.samples.empty()
                ? std::abs(result.samples.back().state.mass_kg -
                    expected_final_mass)
                : 1.0;
            report.checks.push_back(Check(
                "Prescribed thrust profile integrates exact impulse and mass flow",
                error,
                1.0e-11,
                "The 10 N s triangular profile must consume exactly impulse/(Isp*g0)."));
        }

        {
            // Case 23: a pure point-mass barycenter is valid without dummy member
            // rows, but near-field resolution still requires physical members.
            SimulationRequest request = MakeRigidRequest();
            GravityBody barycenter;
            barycenter.name = "Test barycenter";
            barycenter.naif_id = 5;
            barycenter.gravity_system_name = "Test system";
            barycenter.gravity_source_role =
                GravitySourceRole::SystemBarycenter;
            barycenter.gravity_enabled = true;
            barycenter.gravitational_parameter_m3ps2 = 1.0e10;
            request.gravity.bodies.push_back(barycenter);
            const std::string point_mass_error =
                SimulationEngine().ValidateRequest(request);
            request.gravity.bodies[0].barycenter_resolution_radius_m = 1.0e9;
            const std::string unresolved_error =
                SimulationEngine().ValidateRequest(request);
            report.checks.push_back(Check(
                "Barycenter-only schema accepts point-mass systems",
                point_mass_error.empty() && !unresolved_error.empty()
                    ? 0.0 : 1.0,
                0.0,
                "Dummy physical members are unnecessary unless barycenter resolution is enabled."));

            const scenario::CelestialCatalogEntry* phoebe =
                scenario::FindCelestialCatalogEntry("Phoebe");
            report.checks.push_back(Check(
                "Phoebe is present in the gravity catalog",
                phoebe != nullptr && phoebe->naif_id == 609
                    ? 0.0 : 1.0,
                0.0,
                "Phoebe must compile as Saturn-system member NAIF 609."));
        }

        {
            // Case 24: both boundaries of a half-open commanded-thrust interval are
            // controller events. A step endpoint just below t=11 must be snapped to
            // the event so the next RK4 k1 uses the zero post-cutoff command.
            double maximum_mass_error = 0.0;
            bool all_runs_succeeded = true;
            for (const double step_seconds : {0.2, 0.03, 0.01})
            {
                SimulationRequest request;
                request.scenario_name =
                    "Commanded-thruster discontinuity validation";
                ComponentDefinition tank = MakeComponent(
                    "Variable-mass body", 110.0, Vec3d::Zero(),
                    {10.0, 10.0, 10.0});
                tank.minimum_mass_kg = 100.0;
                tank.variable_mass_state_index = 0;
                request.vehicle.components.push_back(tank);
                request.initial_state.attitude_body_to_icrf = Quatd::Identity();
                request.initial_state.angular_velocity_body_radps = Vec3d::Zero();
                request.start_ephemeris_time_tdb_seconds = 946731646.683092;
                request.requested_duration_seconds = 15.0;
                request.final_ephemeris_time_tdb_seconds =
                    request.start_ephemeris_time_tdb_seconds + 15.0;
                request.maximum_integrator_step_seconds = step_seconds;
                request.output_mode = OutputMode::FixedInterval;
                request.output_step_seconds = 1.0;

                ThrusterDefinition thruster;
                thruster.name = "Timed commanded thruster";
                thruster.mode = ThrusterMode::Commanded;
                thruster.component_index = 0;
                thruster.propellant_component_index = 0;
                thruster.direction_component = Vec3d::UnitX();
                thruster.ignition_ephemeris_time_tdb_seconds =
                    request.start_ephemeris_time_tdb_seconds;
                thruster.maximum_thrust_n = 100.0;
                request.vehicle.thrusters.push_back(thruster);
                request.control.controller =
                    std::make_shared<TimedThrusterCommandController>(
                        0, 1.0, 11.0, 0.4, 250.0);

                const SimulationResult result = SimulationEngine().Run(request);
                all_runs_succeeded = all_runs_succeeded && result.success &&
                    !result.samples.empty();
                if (result.success && !result.samples.empty())
                {
                    // Integral T dt = 40 N * (11-1) s = 400 N s.
                    const double expected_mass =
                        110.0 - 400.0 /
                            (250.0 * kStandardGravityMps2);
                    maximum_mass_error = std::max(
                        maximum_mass_error,
                        std::abs(result.samples.back().state.mass_kg -
                            expected_mass));
                }
            }
            report.checks.push_back(Check(
                "Commanded-thruster cutoff has exact impulse and mass flow",
                all_runs_succeeded ? maximum_mass_error : 1.0,
                1.0e-10,
                "A controller-announced half-open 40 N interval lasting 10 s must consume exactly 400/(Isp*g0) kg at every tested RK4 step."));
        }

        {
            // Case 25: elapsed-authored sub-microsecond firing boundaries must not
            // make a round trip through the much coarser absolute ET representation.
            SimulationRequest request;
            request.scenario_name =
                "Native elapsed thruster-boundary validation";
            ComponentDefinition tank = MakeComponent(
                "Variable-mass body", 10.0, Vec3d::Zero(),
                {1.0, 1.0, 1.0});
            tank.minimum_mass_kg = 1.0;
            tank.variable_mass_state_index = 0;
            request.vehicle.components.push_back(tank);
            request.initial_state.attitude_body_to_icrf = Quatd::Identity();
            request.initial_state.angular_velocity_body_radps = Vec3d::Zero();
            request.start_ephemeris_time_tdb_seconds = 946731646.683092;
            request.requested_duration_seconds = 1.0e-6;
            request.final_ephemeris_time_tdb_seconds =
                request.start_ephemeris_time_tdb_seconds +
                request.requested_duration_seconds;
            request.maximum_integrator_step_seconds = 3.0e-7;

            ThrusterDefinition thruster;
            thruster.name = "Sub-microsecond prescribed thruster";
            thruster.component_index = 0;
            thruster.propellant_component_index = 0;
            thruster.direction_component = Vec3d::UnitX();
            thruster.ignition_elapsed_time_seconds = 1.0e-7;
            thruster.shutdown_elapsed_time_seconds = 2.0e-7;
            // The compatibility ET values may collide with nearby instants; the
            // elapsed fields above remain the authoritative firing interval.
            thruster.ignition_ephemeris_time_tdb_seconds =
                request.start_ephemeris_time_tdb_seconds +
                thruster.ignition_elapsed_time_seconds;
            thruster.shutdown_ephemeris_time_tdb_seconds =
                request.start_ephemeris_time_tdb_seconds +
                thruster.shutdown_elapsed_time_seconds;
            thruster.constant_thrust_n = 1.0e6;
            thruster.constant_specific_impulse_seconds = 200.0;
            request.vehicle.thrusters.push_back(thruster);

            const SimulationResult result = SimulationEngine().Run(request);
            const double expected_final_mass = 10.0 -
                (1.0e6 * (2.0e-7 - 1.0e-7)) /
                    (200.0 * kStandardGravityMps2);
            const double error = result.success && !result.samples.empty()
                ? std::abs(result.samples.back().state.mass_kg -
                    expected_final_mass)
                : 1.0;
            report.checks.push_back(Check(
                "Elapsed thruster boundaries preserve sub-ET-ULP impulse",
                error,
                1.0e-11,
                "A native 0.1 microsecond firing window at a 2030 epoch must retain exactly 0.1 N s impulse."));
        }

        {
            // Case 26: a final interval smaller than the old scale-dependent
            // tolerance must still advance the physical state before recording time.
            SimulationRequest request = MakeRigidRequest();
            request.initial_state.position_icrf_m = Vec3d::Zero();
            request.initial_state.velocity_icrf_mps = {3.75, -1.25, 0.5};
            request.initial_state.angular_velocity_body_radps = Vec3d::Zero();
            request.requested_duration_seconds =
                1000000000.0 + 1.0013580322265625e-5;
            request.final_ephemeris_time_tdb_seconds =
                request.start_ephemeris_time_tdb_seconds +
                request.requested_duration_seconds;
            request.maximum_integrator_step_seconds = 1000000000.0;
            request.output_mode = OutputMode::EveryIntegratorStep;

            const SimulationResult result = SimulationEngine().Run(request);
            double error = 1.0;
            if (result.success && result.samples.size() == 3)
            {
                const SpacecraftState& final_state =
                    result.samples.back().state;
                const Vec3d expected_position =
                    request.initial_state.velocity_icrf_mps *
                    request.requested_duration_seconds;
                error = (final_state.position_icrf_m -
                    expected_position).Norm();
                if (final_state.elapsed_time_seconds !=
                    request.requested_duration_seconds)
                {
                    error = 1.0;
                }
            }
            report.checks.push_back(Check(
                "Terminal tail advances state and elapsed clock together",
                error,
                1.0e-6,
                "The representable tail after 1e9 s must contribute velocity times tail to final position."));
        }

        {
            // Case 27: a fixed output boundary immediately before wheel saturation
            // must be recorded before the later saturation boundary is processed.
            SimulationRequest request = MakeRigidRequest();
            request.initial_state.angular_velocity_body_radps = Vec3d::Zero();
            ReactionWheelDefinition wheel;
            wheel.name = "Fixed-grid validation wheel";
            wheel.axis_component = Vec3d::UnitZ();
            wheel.maximum_momentum_nms = 0.2;
            request.vehicle.reaction_wheels.push_back(wheel);
            request.control.controller =
                std::make_shared<ConstantWheelRateController>(0.01);
            request.requested_duration_seconds = 30.0;
            request.final_ephemeris_time_tdb_seconds = 30.0;
            request.maximum_integrator_step_seconds = 0.01;
            request.output_mode = OutputMode::FixedInterval;
            request.output_step_seconds = 0.1;
            request.maximum_integration_steps = 5000000;
            request.maximum_output_samples = 1000000;

            const SimulationResult result = SimulationEngine().Run(request);
            double error = result.success && result.samples.size() == 301
                ? 0.0
                : 1.0;
            bool found_saturation_output = false;
            for (std::size_t index = 0;
                index < result.samples.size(); ++index)
            {
                const double elapsed =
                    result.samples[index].state.elapsed_time_seconds;
                error = std::max(
                    error,
                    std::abs(elapsed - 0.1 * static_cast<double>(index)));
                if (elapsed == 20.0)
                    found_saturation_output = true;
                if (index > 0 && elapsed <= result.samples[index - 1].
                    state.elapsed_time_seconds)
                {
                    error = 1.0;
                }
            }
            if (!found_saturation_output || result.samples.empty())
            {
                error = 1.0;
            }
            else
            {
                const TelemetrySample& final_sample = result.samples.back();
                error = std::max(error, std::abs(
                    final_sample.state.internal_angular_momenta_nms[0] -
                    0.2));
                error = std::max(
                    error,
                    final_sample.
                        total_angular_momentum_about_cm_icrf_kgm2ps.Norm());
                error = std::max(
                    error,
                    std::abs(final_sample.state.elapsed_time_seconds - 30.0));
            }
            report.checks.push_back(Check(
                "Fixed output grid survives reaction-wheel saturation",
                error,
                1.0e-10,
                "The 0.1 s grid must contain 301 unique rows including t=20 s while the wheel saturates at 0.2 N m s without changing total angular momentum."));
        }

        {
            // Cases 28-30: every representably future elapsed event must be
            // scheduled even when it lies below the former scale-based tolerance.
            const auto make_request = [](
                double ignition_elapsed_time,
                double shutdown_elapsed_time,
                double duration_seconds)
            {
                SimulationRequest request;
                request.scenario_name =
                    "Large-elapsed strict event-order validation";
                ComponentDefinition tank = MakeComponent(
                    "Variable-mass body", 110.0, Vec3d::Zero(),
                    {10.0, 10.0, 10.0});
                tank.minimum_mass_kg = 100.0;
                tank.variable_mass_state_index = 0;
                request.vehicle.components.push_back(tank);
                request.initial_state.attitude_body_to_icrf =
                    Quatd::Identity();
                request.initial_state.angular_velocity_body_radps =
                    Vec3d::Zero();
                request.start_ephemeris_time_tdb_seconds =
                    946731646.683092;
                request.requested_duration_seconds = duration_seconds;
                request.final_ephemeris_time_tdb_seconds =
                    request.start_ephemeris_time_tdb_seconds +
                    duration_seconds;
                request.maximum_integrator_step_seconds = 1000000000.0;
                request.output_mode = OutputMode::FixedInterval;
                request.output_step_seconds = 1000000000.0;

                ThrusterDefinition thruster;
                thruster.name = "Large-elapsed boundary thruster";
                thruster.component_index = 0;
                thruster.propellant_component_index = 0;
                thruster.direction_component = Vec3d::UnitX();
                thruster.ignition_elapsed_time_seconds =
                    ignition_elapsed_time;
                thruster.shutdown_elapsed_time_seconds =
                    shutdown_elapsed_time;
                thruster.ignition_ephemeris_time_tdb_seconds =
                    request.start_ephemeris_time_tdb_seconds +
                    ignition_elapsed_time;
                thruster.shutdown_ephemeris_time_tdb_seconds =
                    request.start_ephemeris_time_tdb_seconds +
                    shutdown_elapsed_time;
                // T/(Isp*g0) = 1 kg/s exactly.
                thruster.constant_thrust_n = 9806.65;
                thruster.constant_specific_impulse_seconds = 1000.0;
                request.vehicle.thrusters.push_back(thruster);
                return request;
            };

            const double near_ignition = 1000000000.00001;
            const double near_shutdown = 1000000000.10001;
            const SimulationResult near_result = SimulationEngine().Run(
                make_request(
                    near_ignition,
                    near_shutdown,
                    1000000000.20001));
            const double near_error =
                near_result.success && !near_result.samples.empty()
                ? std::abs(
                    (110.0 - near_result.samples.back().state.mass_kg) -
                    (near_shutdown - near_ignition))
                : 1.0;
            report.checks.push_back(Check(
                "Representable near-future event remains schedulable at 1e9 s",
                near_error,
                1.0e-12,
                "Ignition 1.001358e-5 s after t=1e9 s must retain the complete representable 0.10000002384185791 kg burn."));

            const double control_ignition = 1000000000.00002;
            const double control_shutdown = 1000000000.10002;
            const SimulationResult control_result = SimulationEngine().Run(
                make_request(
                    control_ignition,
                    control_shutdown,
                    1000000000.20002));
            const double control_error =
                control_result.success && !control_result.samples.empty()
                ? std::abs(
                    (110.0 - control_result.samples.back().state.mass_kg) -
                    (control_shutdown - control_ignition))
                : 1.0;
            report.checks.push_back(Check(
                "Large-elapsed event above former tolerance remains exact",
                control_error,
                1.0e-12,
                "The above-tolerance control must retain its complete representable firing interval."));

            const SimulationResult before_event_result =
                SimulationEngine().Run(make_request(
                    near_ignition,
                    near_shutdown,
                    999999999.99999));
            const double before_event_error =
                before_event_result.success &&
                    !before_event_result.samples.empty()
                ? std::abs(
                    before_event_result.samples.back().state.mass_kg -
                    110.0)
                : 1.0;
            report.checks.push_back(Check(
                "Final time before future event does not fire thruster",
                before_event_error,
                0.0,
                "A boundary after the requested final elapsed time must not affect the propagated state."));
        }

        {
            // Individual fixed components may be massless, but the complete
            // spacecraft must start and remain above zero aggregate mass.
            SimulationRequest supported = MakeRigidRequest();
            ComponentDefinition massless = MakeComponent(
                "Massless fixed geometry", 0.0, Vec3d::Zero(),
                {0.0, 0.0, 0.0});
            massless.parent_component_index = 0;
            supported.vehicle.components.push_back(massless);
            const SimulationResult supported_result =
                SimulationEngine().Run(supported);
            report.checks.push_back(Check(
                "Massless fixed component propagates",
                supported_result.success ? 0.0 : 1.0,
                0.0,
                "A zero-mass component is valid when aggregate spacecraft mass and inertia remain valid."));

            SimulationRequest all_massless = MakeRigidRequest();
            all_massless.vehicle.components[0].initial_mass_kg = 0.0;
            all_massless.vehicle.components[0].minimum_mass_kg = 0.0;
            const std::string all_massless_error =
                SimulationEngine().ValidateRequest(all_massless);
            report.checks.push_back(Check(
                "All-massless spacecraft is rejected",
                all_massless_error.empty() ? 1.0 : 0.0,
                0.0,
                "Total initial spacecraft mass must remain positive."));

            SimulationRequest zero_floor = MakeRigidRequest();
            zero_floor.vehicle.components[0].minimum_mass_kg = 0.0;
            zero_floor.vehicle.components[0].variable_mass_state_index = 0;
            const std::string zero_floor_error =
                SimulationEngine().ValidateRequest(zero_floor);
            report.checks.push_back(Check(
                "Zero reachable aggregate mass is rejected",
                zero_floor_error.find("Minimum reachable spacecraft mass") !=
                    std::string::npos ? 0.0 : 1.0,
                0.0,
                "The configured depletion floors must keep aggregate spacecraft mass positive."));
        }

        {
            // Asymmetric depletion moves the geometric CM even when every
            // component centroid is instantaneously stationary.
            SimulationRequest request;
            ComponentDefinition left = MakeComponent(
                "Left mass", 1.0, {-1.0, 0.0, 0.0}, {0.1, 0.1, 0.1});
            ComponentDefinition right = MakeComponent(
                "Right mass", 1.0, {1.0, 0.0, 0.0}, {0.1, 0.1, 0.1});
            right.parent_component_index = 0;
            right.minimum_mass_kg = 0.5;
            right.variable_mass_state_index = 0;
            request.vehicle.components = {left, right};
            request.initial_state.variable_component_masses_kg = {1.0};
            request.initial_state.attitude_body_to_icrf = Quatd::Identity();
            const SimulationConfig config = SimulationConfigBuilder().Build(request);
            const MassProperties properties = MassPropertiesModel().Compute(
                config.vehicle, config.initial_state, {-0.1}, {});
            FloatingBaseTreeInput input;
            input.center_of_mass_body_m = properties.center_of_mass_body_m;
            input.center_of_mass_mass_redistribution_rate_body_mps =
                properties.center_of_mass_mass_redistribution_rate_body_mps;
            input.component_poses = properties.component_poses;
            const FloatingBaseTreeEvaluation momentum =
                FloatingBaseTreeDynamics().SolveForwardDynamics(
                    config.initial_state, config, input);
            report.checks.push_back(Check(
                "Asymmetric depletion exposes geometric-CM redistribution rate",
                std::max(
                    (properties.center_of_mass_mass_redistribution_rate_body_mps -
                        Vec3d{-0.05, 0.0, 0.0}).Norm(),
                    (momentum.total_linear_momentum_icrf_kgmps -
                        Vec3d{0.1, 0.0, 0.0}).Norm()),
                1.0e-12,
                "Two equal masses at x=+-1 m with right-side depletion of 0.1 kg/s must give beta_x=-0.05 m/s and P_x=0.1 kg m/s when geometric-CM velocity is zero."));
        }

        {
            // A perfectly inelastic internal stop sets the impacting relative
            // rate to zero while the free base receives the coupled impulse.
            SimulationRequest request;
            request.scenario_name = "Coupled inelastic joint-stop validation";
            request.vehicle.components.push_back(MakeComponent(
                "Main body", 5.0, Vec3d::Zero(), {1.0, 1.0, 2.0}));
            ComponentDefinition rotor = MakeComponent(
                "Hinged rotor", 1.0, Vec3d::Zero(), {0.3, 0.3, 0.5});
            rotor.parent_component_index = 0;
            ArticulationDof hinge = MakeDof(
                "limited_hinge", ArticulationMotion::Rotation, Vec3d::UnitZ());
            hinge.limits.minimum_coordinate = -1.0;
            hinge.limits.maximum_coordinate = 0.1;
            hinge.limits.maximum_absolute_rate = 10.0;
            rotor.articulation_to_parent.dofs.push_back(hinge);
            request.vehicle.components.push_back(rotor);
            request.initial_state.attitude_body_to_icrf = Quatd::Identity();
            request.initial_state.angular_velocity_body_radps = Vec3d::Zero();
            request.initial_state.articulation_coordinates = {0.0};
            request.initial_state.articulation_rates = {1.0};
            request.requested_duration_seconds = 0.2;
            request.final_ephemeris_time_tdb_seconds = 0.2;
            request.maximum_integrator_step_seconds = 0.2;
            request.output_mode = OutputMode::EveryIntegratorStep;
            request.maximum_output_samples = 20;

            const SimulationResult result = SimulationEngine().Run(request);
            double error = 1.0;
            if (result.success && result.samples.size() >= 2)
            {
                const TelemetrySample& initial = result.samples.front();
                const TelemetrySample& final = result.samples.back();
                error = std::max({
                    std::abs(final.state.articulation_coordinates[0] - 0.1),
                    std::abs(final.state.articulation_rates[0]),
                    std::abs(final.state.angular_velocity_body_radps.z - 0.2),
                    RelativeDifference(
                        final.total_angular_momentum_about_cm_icrf_kgm2ps,
                        initial.total_angular_momentum_about_cm_icrf_kgm2ps)});
            }
            report.checks.push_back(Check(
                "Inelastic joint stop conserves coupled angular momentum",
                error,
                2.0e-9,
                "A rotor hitting an internal hard stop must lock at the limit, transfer its relative momentum to the free base, and preserve total angular momentum."));
        }

        {
            // The maximum joint rate is an ideal coupled velocity constraint.
            // Sustained outward actuator effort is balanced by its reaction.
            SimulationRequest request;
            request.scenario_name = "Coupled ideal joint-rate validation";
            request.vehicle.components.push_back(MakeComponent(
                "Main body", 5.0, Vec3d::Zero(), {1.0, 1.0, 2.0}));
            ComponentDefinition rotor = MakeComponent(
                "Rate-limited rotor", 1.0, Vec3d::Zero(), {0.3, 0.3, 0.5});
            rotor.parent_component_index = 0;
            ArticulationDof hinge = MakeDof(
                "rate_limited_hinge",
                ArticulationMotion::Rotation,
                Vec3d::UnitZ());
            hinge.limits.minimum_coordinate = -10.0;
            hinge.limits.maximum_coordinate = 10.0;
            hinge.limits.maximum_absolute_rate = 0.2;
            rotor.articulation_to_parent.dofs.push_back(hinge);
            request.vehicle.components.push_back(rotor);
            request.control.controller =
                std::make_shared<ConstantJointEffortController>(
                    std::vector<double>{0.4});
            request.initial_state.attitude_body_to_icrf = Quatd::Identity();
            request.initial_state.angular_velocity_body_radps = Vec3d::Zero();
            request.initial_state.articulation_coordinates = {0.0};
            request.initial_state.articulation_rates = {0.0};
            request.requested_duration_seconds = 1.0;
            request.final_ephemeris_time_tdb_seconds = 1.0;
            request.maximum_integrator_step_seconds = 0.1;
            request.output_mode = OutputMode::EveryIntegratorStep;

            const SimulationResult result = SimulationEngine().Run(request);
            double error = 1.0;
            if (result.success && result.samples.size() >= 2)
            {
                const TelemetrySample& initial = result.samples.front();
                const TelemetrySample& final = result.samples.back();
                double maximum_rate = 0.0;
                for (const TelemetrySample& sample : result.samples)
                {
                    maximum_rate = std::max(
                        maximum_rate,
                        std::abs(sample.state.articulation_rates[0]));
                }
                error = std::max({
                    std::abs(maximum_rate - 0.2),
                    std::max(0.0, maximum_rate - 0.2),
                    RelativeDifference(
                        final.total_angular_momentum_about_cm_icrf_kgm2ps,
                        initial.total_angular_momentum_about_cm_icrf_kgm2ps)});
            }
            report.checks.push_back(Check(
                "Ideal joint-rate limit applies a coupled reaction",
                error,
                2.0e-9,
                "An internal actuator driven against its ideal speed cap must remain at the cap without changing total spacecraft angular momentum."));
        }

        {
            const auto make_two_rotor_request = [](
                double second_initial_coordinate,
                double second_initial_rate,
                double duration_seconds)
            {
                SimulationRequest request;
                request.scenario_name =
                    "Unilateral multi-contact joint-stop validation";
                request.vehicle.components.push_back(MakeComponent(
                    "Main body", 5.0, Vec3d::Zero(), {1.0, 1.0, 2.0}));

                for (const std::string& name : {
                    std::string{"First rotor"}, std::string{"Second rotor"}})
                {
                    ComponentDefinition rotor = MakeComponent(
                        name, 1.0, Vec3d::Zero(), {0.3, 0.3, 0.5});
                    rotor.parent_component_index = 0;
                    ArticulationDof hinge = MakeDof(
                        name + " hinge",
                        ArticulationMotion::Rotation,
                        Vec3d::UnitZ());
                    hinge.limits.minimum_coordinate = -1.0;
                    hinge.limits.maximum_coordinate = 0.1;
                    hinge.limits.maximum_absolute_rate = 10.0;
                    rotor.articulation_to_parent.dofs.push_back(hinge);
                    request.vehicle.components.push_back(rotor);
                }

                request.initial_state.attitude_body_to_icrf = Quatd::Identity();
                request.initial_state.angular_velocity_body_radps = Vec3d::Zero();
                request.initial_state.articulation_coordinates = {
                    0.0, second_initial_coordinate};
                request.initial_state.articulation_rates = {
                    1.0, second_initial_rate};
                request.requested_duration_seconds = duration_seconds;
                request.final_ephemeris_time_tdb_seconds = duration_seconds;
                request.maximum_integrator_step_seconds = 0.2;
                request.output_mode = OutputMode::EveryIntegratorStep;
                request.maximum_output_samples = 100;
                return request;
            };
            const SimulationResult simultaneous = SimulationEngine().Run(
                make_two_rotor_request(0.09, 0.1, 0.2));
            double simultaneous_error = 1.0;
            if (simultaneous.success && simultaneous.samples.size() >= 2)
            {
                const TelemetrySample& initial = simultaneous.samples.front();
                const TelemetrySample& final = simultaneous.samples.back();
                simultaneous_error = std::max({
                    std::abs(final.state.articulation_coordinates[0] - 0.1),
                    std::abs(final.state.articulation_rates[0]),
                    std::abs(final.state.articulation_coordinates[1] - 0.09),
                    std::abs(final.state.articulation_rates[1] + 0.1),
                    std::abs(final.state.angular_velocity_body_radps.z - 0.2),
                    RelativeDifference(
                        final.total_angular_momentum_about_cm_icrf_kgm2ps,
                        initial.total_angular_momentum_about_cm_icrf_kgm2ps)});
            }
            report.checks.push_back(Check(
                "Simultaneous stops release inadmissible contact",
                simultaneous_error,
                2.0e-8,
                "When stopping the faster coaxial rotor reverses the slower rotor, the second upper stop must exert no pulling impulse and must release."));

            const SimulationResult resting_contact = SimulationEngine().Run(
                make_two_rotor_request(0.1, 0.0, 0.2));
            double resting_contact_error = 1.0;
            if (resting_contact.success && resting_contact.samples.size() >= 2)
            {
                const TelemetrySample& initial = resting_contact.samples.front();
                const TelemetrySample& final = resting_contact.samples.back();
                resting_contact_error = std::max({
                    std::abs(final.state.articulation_rates[0]),
                    std::abs(final.state.articulation_coordinates[1] - 0.08),
                    std::abs(final.state.articulation_rates[1] + 0.2),
                    RelativeDifference(
                        final.total_angular_momentum_about_cm_icrf_kgm2ps,
                        initial.total_angular_momentum_about_cm_icrf_kgm2ps)});
            }
            report.checks.push_back(Check(
                "Impact releases an existing resting stop",
                resting_contact_error,
                2.0e-8,
                "A rotor already resting at its upper stop must participate in another joint's impact and separate when the coupled response points inward."));

            const SimulationResult sequential = SimulationEngine().Run(
                make_two_rotor_request(0.0, 0.5, 0.4));
            double sequential_error = 1.0;
            if (sequential.success && sequential.samples.size() >= 3)
            {
                const TelemetrySample& initial = sequential.samples.front();
                const TelemetrySample& final = sequential.samples.back();
                sequential_error = std::max({
                    std::abs(final.state.articulation_coordinates[0] - 0.092),
                    std::abs(final.state.articulation_rates[0] + 0.06),
                    std::abs(final.state.articulation_coordinates[1] - 0.1),
                    std::abs(final.state.articulation_rates[1]),
                    std::abs(final.state.angular_velocity_body_radps.z - 0.26),
                    RelativeDifference(
                        final.total_angular_momentum_about_cm_icrf_kgm2ps,
                        initial.total_angular_momentum_about_cm_icrf_kgm2ps)});
            }
            report.checks.push_back(Check(
                "Sequential impacts reevaluate existing contacts",
                sequential_error,
                5.0e-8,
                "At the later rotor impact, the first stopped rotor must be released if retaining both contacts would require a tensile stop impulse."));
        }

        return report;
    }
}
