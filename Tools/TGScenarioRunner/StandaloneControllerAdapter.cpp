// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "StandaloneControllerAdapter.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

#include <cmath>
#include <iostream>
#include <limits>
#include <utility>

namespace
{
    TGStringView ToStringView(const std::string& value)
    {
        return {
            value.empty() ? nullptr : value.data(),
            static_cast<uint64_t>(value.size())};
    }

    TGVec3 ToApi(const tgsim::Vec3d& value)
    {
        return {value.x, value.y, value.z};
    }

    TGQuat ToApi(const tgsim::Quatd& value)
    {
        return {value.w, value.x, value.y, value.z};
    }

    TGMat3 ToApi(const tgsim::Mat3d& value)
    {
        TGMat3 result{};
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 3; ++column)
            {
                result.M[row * 3 + column] = value.m[row][column];
            }
        }
        return result;
    }

    tgsim::Vec3d FromApi(const TGVec3& value)
    {
        return {value.X, value.Y, value.Z};
    }

    uint64_t ToApiIndex(const std::size_t index)
    {
        return index == tgsim::kInvalidIndex
            ? TG_CONTROLLER_INVALID_INDEX
            : static_cast<uint64_t>(index);
    }

    TGBool ToApiBool(const bool value)
    {
        return value ? 1u : 0u;
    }

    uint32_t ToApiIntegratorKind(const tgsim::IntegratorKind kind)
    {
        return kind == tgsim::IntegratorKind::AdaptiveDormandPrince54
            ? TG_INTEGRATOR_ADAPTIVE_DORMAND_PRINCE_54
            : TG_INTEGRATOR_FIXED_STEP_RK4;
    }

    uint32_t ToApiOutputMode(const tgsim::OutputMode mode)
    {
        return mode == tgsim::OutputMode::FixedInterval
            ? TG_OUTPUT_FIXED_INTERVAL
            : TG_OUTPUT_EVERY_INTEGRATOR_STEP;
    }

    uint32_t ToApiJointMotion(const tgsim::ArticulationMotion motion)
    {
        return motion == tgsim::ArticulationMotion::Translation
            ? TG_JOINT_TRANSLATION
            : TG_JOINT_ROTATION;
    }

    uint32_t ToApiThrusterMode(const tgsim::ThrusterMode mode)
    {
        return mode == tgsim::ThrusterMode::Commanded
            ? TG_THRUSTER_COMMANDED
            : TG_THRUSTER_PRESCRIBED_PROFILE;
    }

    uint32_t ToApiGravityRole(const tgsim::GravitySourceRole role)
    {
        switch (role)
        {
            case tgsim::GravitySourceRole::SystemBarycenter:
                return TG_GRAVITY_SOURCE_SYSTEM_BARYCENTER;
            case tgsim::GravitySourceRole::SystemMember:
                return TG_GRAVITY_SOURCE_SYSTEM_MEMBER;
            case tgsim::GravitySourceRole::Independent:
            default:
                return TG_GRAVITY_SOURCE_INDEPENDENT;
        }
    }

    uint32_t ToApiAtmosphereModel(const tgsim::AtmosphereModelKind model)
    {
        return model == tgsim::AtmosphereModelKind::CubicHarrisPriesterEarth
            ? TG_ATMOSPHERE_CUBIC_HARRIS_PRIESTER_EARTH
            : TG_ATMOSPHERE_TABULATED_PROFILE;
    }

    template <typename Element>
    const Element* DataOrNull(const std::vector<Element>& values)
    {
        return values.empty() ? nullptr : values.data();
    }

    template <typename Element>
    Element* MutableDataOrNull(std::vector<Element>& values)
    {
        return values.empty() ? nullptr : values.data();
    }

#if defined(_WIN32)
    template <typename Function>
    Function ResolveExport(void* handle, const char* name)
    {
        return reinterpret_cast<Function>(
            GetProcAddress(static_cast<HMODULE>(handle), name));
    }

    bool ValidateControllerContract(
        const TGDescribeControllerContractFunction describe_contract,
        std::string& error)
    {
        TGControllerContract contract{};
        contract.StructSize = sizeof(TGControllerContract);

        if (describe_contract == nullptr ||
            describe_contract(&contract) != TG_CONTROLLER_RESULT_OK ||
            contract.StructSize != sizeof(TGControllerContract) ||
            contract.ControlInputSize != sizeof(TGControlInput) ||
            contract.ControlOutputSize != sizeof(TGControlOutput) ||
            contract.ThrusterCommandSize != sizeof(TGThrusterCommand) ||
            contract.SimulationConfigurationSize !=
                sizeof(TGSimulationConfiguration) ||
            contract.SpacecraftStateViewSize !=
                sizeof(TGSpacecraftStateView) ||
            contract.ComponentStateViewSize != sizeof(TGComponentStateView) ||
            contract.JointStateViewSize != sizeof(TGJointStateView) ||
            contract.ThrusterStateViewSize != sizeof(TGThrusterStateView) ||
            contract.ReactionWheelStateViewSize !=
                sizeof(TGReactionWheelStateView) ||
            contract.CelestialBodyStateViewSize !=
                sizeof(TGCelestialBodyStateView))
        {
            error =
                "The controller does not match the PHAROS Controller API. "
                "Rebuild it with the Controller SDK distributed with this application.";
            return false;
        }

        return true;
    }
#endif
}

StandaloneControllerAdapter::~StandaloneControllerAdapter()
{
    Unload();
}

std::shared_ptr<StandaloneControllerAdapter>
StandaloneControllerAdapter::Create(
    const std::filesystem::path& dll_path,
    std::string& error)
{
    auto result = std::shared_ptr<StandaloneControllerAdapter>(
        new StandaloneControllerAdapter());
    if (!result->Load(dll_path, error)) return nullptr;
    return result;
}

bool StandaloneControllerAdapter::Load(
    const std::filesystem::path& dll_path,
    std::string& error)
{
    error.clear();

#if !defined(_WIN32)
    (void)dll_path;
    error = "Compiled controller DLL loading is supported only on Windows.";
    return false;
#else
    std::error_code path_error;
    loaded_dll_path_ = std::filesystem::absolute(dll_path, path_error);
    if (path_error) loaded_dll_path_ = dll_path;
    loaded_dll_path_ = loaded_dll_path_.lexically_normal();

    if (!std::filesystem::is_regular_file(loaded_dll_path_))
    {
        error = "The controller DLL does not exist: " +
            loaded_dll_path_.string();
        return false;
    }

    dll_handle_ = static_cast<void*>(LoadLibraryW(loaded_dll_path_.c_str()));
    if (dll_handle_ == nullptr)
    {
        error = "Windows could not load the controller DLL (error " +
            std::to_string(GetLastError()) + "): " +
            loaded_dll_path_.string();
        return false;
    }

    const TGDescribeControllerContractFunction describe_contract =
        ResolveExport<TGDescribeControllerContractFunction>(
            dll_handle_, TG_DESCRIBE_CONTROLLER_CONTRACT_EXPORT);
    const TGCreateControllerFunction create_controller =
        ResolveExport<TGCreateControllerFunction>(
            dll_handle_, TG_CREATE_CONTROLLER_EXPORT);
    destroy_controller_ = ResolveExport<TGDestroyControllerFunction>(
        dll_handle_, TG_DESTROY_CONTROLLER_EXPORT);
    compute_controller_ = ResolveExport<TGComputeControlFunction>(
        dll_handle_, TG_COMPUTE_CONTROL_EXPORT);
    next_discontinuity_ =
        ResolveExport<TGNextControllerDiscontinuityFunction>(
            dll_handle_, TG_NEXT_CONTROLLER_DISCONTINUITY_EXPORT);

    if (describe_contract == nullptr || create_controller == nullptr ||
        destroy_controller_ == nullptr || compute_controller_ == nullptr)
    {
        error = "The controller DLL is missing one or more required PHAROS exports.";
        Unload();
        return false;
    }

    if (!ValidateControllerContract(describe_contract, error))
    {
        Unload();
        return false;
    }

    controller_instance_ = create_controller();
    if (controller_instance_ == nullptr)
    {
        error = "The controller DLL could not create a controller instance.";
        Unload();
        return false;
    }
    return true;
#endif
}

void StandaloneControllerAdapter::Unload()
{
    if (controller_instance_ != nullptr && destroy_controller_ != nullptr)
    {
        destroy_controller_(controller_instance_);
    }
    controller_instance_ = nullptr;
    destroy_controller_ = nullptr;
    compute_controller_ = nullptr;
    next_discontinuity_ = nullptr;

#if defined(_WIN32)
    if (dll_handle_ != nullptr)
    {
        FreeLibrary(static_cast<HMODULE>(dll_handle_));
    }
#endif
    dll_handle_ = nullptr;
}

double StandaloneControllerAdapter::NextDiscontinuityElapsedTime(
    double current_elapsed_time_seconds) const
{
    if (controller_instance_ == nullptr || next_discontinuity_ == nullptr)
        return std::numeric_limits<double>::infinity();

    const double result = next_discontinuity_(
        controller_instance_, current_elapsed_time_seconds);
    return std::isfinite(result) && result > current_elapsed_time_seconds
        ? result
        : std::numeric_limits<double>::infinity();
}

void StandaloneControllerAdapter::ComputeControl(
    const tgsim::ControlInput& input,
    tgsim::ControlCommandWriter& output) const
{
    if (controller_instance_ == nullptr || compute_controller_ == nullptr)
        return;

    const tgsim::SimulationConfig& config = input.configuration;
    TGSimulationConfiguration api_config{};
    api_config.ScenarioName = ToStringView(config.scenario_name);
    api_config.IntegratorKind =
        ToApiIntegratorKind(config.solver.integrator_kind);
    api_config.OutputMode = ToApiOutputMode(config.solver.output_mode);
    api_config.StartEphemerisTimeTdbSeconds =
        config.solver.start_ephemeris_time_tdb_seconds;
    api_config.FinalEphemerisTimeTdbSeconds =
        config.solver.final_ephemeris_time_tdb_seconds;
    api_config.MaximumIntegratorStepSeconds =
        config.solver.maximum_integrator_step_seconds;
    api_config.InitialIntegratorStepSeconds =
        config.solver.initial_integrator_step_seconds;
    api_config.AbsoluteTolerance = config.solver.absolute_tolerance;
    api_config.RelativeTolerance = config.solver.relative_tolerance;
    api_config.OutputStepSeconds = config.solver.output_step_seconds;
    api_config.MaximumIntegrationSteps =
        static_cast<uint64_t>(config.solver.maximum_integration_steps);
    api_config.MaximumOutputSamples =
        static_cast<uint64_t>(config.solver.maximum_output_samples);
    api_config.IncludeFirstPostNewtonianCorrection = ToApiBool(
        config.gravity.include_first_post_newtonian_correction);
    api_config.SolarRadiationPressureEnabled =
        ToApiBool(config.solar_radiation.enabled);
    api_config.ComputeEclipseShadow =
        ToApiBool(config.solar_radiation.compute_eclipse_shadow);
    api_config.AtmosphereEnabled = ToApiBool(config.atmosphere.enabled);
    api_config.AerodynamicsEnabled = ToApiBool(config.aerodynamics.enabled);
    api_config.AerodynamicCoefficientDatabaseEnabled =
        ToApiBool(config.aerodynamics.coefficient_database.enabled);
    api_config.AerodynamicConstantDragFallbackEnabled =
        ToApiBool(config.aerodynamics.constant_drag_fallback_enabled);
    api_config.ComputeComponentSrpShadows =
        ToApiBool(config.solar_radiation.compute_component_shadows);
    api_config.SunBodyName =
        ToStringView(config.solar_radiation.sun_body_name);
    api_config.SolarPressureAtOneAstronomicalUnitPascals =
        config.solar_radiation.pressure_at_one_au_pa;
    api_config.AtmosphereCentralBodyName =
        ToStringView(config.atmosphere.central_body_name);
    api_config.AtmosphereModel =
        ToApiAtmosphereModel(config.atmosphere.model_kind);
    api_config.AtmosphereDensityProfileSampleCount =
        static_cast<uint64_t>(config.atmosphere.density_profile.size());
    api_config.AtmosphereThermodynamicProfileSampleCount =
        static_cast<uint64_t>(config.atmosphere.thermodynamic_profile.size());
    api_config.AtmosphereCubicHarrisPriesterSampleCount =
        static_cast<uint64_t>(
            config.atmosphere.cubic_harris_priester.density_samples.size());
    api_config.AtmosphereCenteredAverageF107SolarFluxUnits =
        config.atmosphere.cubic_harris_priester.centered_average_f107_sfu;
    api_config.AerodynamicReferenceAreaSquareMeters =
        config.aerodynamics.reference_area_m2;
    api_config.AerodynamicReferenceLengthMeters =
        config.aerodynamics.reference_length_m;
    api_config.AerodynamicMomentReferencePointBodyMeters = ToApi(
        config.aerodynamics.coefficient_database.
            moment_reference_point_body_m);
    api_config.AerodynamicFallbackDragCoefficient =
        config.aerodynamics.fallback_drag_coefficient;
    api_config.AerodynamicMinimumDynamicPressurePascals =
        config.aerodynamics.minimum_dynamic_pressure_pa;
    api_config.AerodynamicMaximumDynamicPressurePascals =
        config.aerodynamics.maximum_dynamic_pressure_pa;

    component_views_.clear();
    component_views_.reserve(config.vehicle.components.size());
    for (std::size_t index = 0;
         index < config.vehicle.components.size(); ++index)
    {
        const tgsim::ComponentDefinition& definition =
            config.vehicle.components[index];
        const tgsim::ComponentControlState* current =
            index < input.components.size() ? &input.components[index] : nullptr;

        TGComponentStateView view{};
        view.ComponentIndex = static_cast<uint64_t>(index);
        view.ParentComponentIndex =
            ToApiIndex(definition.parent_component_index);
        view.VariableMassStateIndex =
            ToApiIndex(definition.variable_mass_state_index);
        view.Name = ToStringView(definition.name);
        view.CurrentMassKilograms = current != nullptr
            ? current->current_mass_kg : definition.initial_mass_kg;
        view.InitialMassKilograms = definition.initial_mass_kg;
        view.MinimumMassKilograms = definition.minimum_mass_kg;
        view.HasVariableMass = ToApiBool(
            definition.variable_mass_state_index != tgsim::kInvalidIndex);
        view.CurrentOriginBodyMeters = current != nullptr
            ? ToApi(current->origin_body_m) : ToApi(definition.origin_body_m);
        view.CurrentComponentToBody = current != nullptr
            ? ToApi(current->component_to_body)
            : ToApi(definition.component_to_body);
        view.LocalCenterOfMassMeters =
            ToApi(definition.center_of_mass_component_m);
        view.LocalCentroidalInertiaKilogramMetersSquared =
            ToApi(definition.inertia_centroid_component_kgm2);
        component_views_.push_back(view);
    }

    thruster_views_.clear();
    thruster_views_.reserve(config.vehicle.thrusters.size());
    for (std::size_t index = 0;
         index < config.vehicle.thrusters.size(); ++index)
    {
        const tgsim::ThrusterDefinition& definition =
            config.vehicle.thrusters[index];
        const tgsim::ThrusterControlState* current =
            index < input.thrusters.size() ? &input.thrusters[index] : nullptr;

        TGThrusterStateView view{};
        view.ThrusterIndex = static_cast<uint64_t>(index);
        view.Name = ToStringView(definition.name);
        view.Mode = ToApiThrusterMode(definition.mode);
        view.MountComponentIndex = ToApiIndex(definition.component_index);
        view.PropellantComponentIndex =
            ToApiIndex(definition.propellant_component_index);
        view.ApplicationPointComponentMeters =
            ToApi(definition.application_point_component_m);
        view.DirectionComponent = ToApi(definition.direction_component);
        view.IgnitionEphemerisTimeTdbSeconds =
            definition.ignition_ephemeris_time_tdb_seconds;
        view.ShutdownEphemerisTimeTdbSeconds =
            definition.shutdown_ephemeris_time_tdb_seconds;
        view.MaximumThrustNewtons = definition.maximum_thrust_n;
        if (current != nullptr)
        {
            view.CurrentPropellantComponentMassKilograms =
                current->current_propellant_component_mass_kg;
            view.FiringWindowOpen = ToApiBool(current->firing_window_open);
            view.HasPropellantComponent =
                ToApiBool(current->has_propellant_component);
            view.PropellantAvailable = ToApiBool(current->propellant_available);
        }
        thruster_views_.push_back(view);
    }

    joint_views_.clear();
    joint_views_.reserve(input.joints.size());
    for (const tgsim::JointControlState& current : input.joints)
    {
        TGJointStateView view{};
        view.ArticulationStateIndex =
            ToApiIndex(current.articulation_state_index);
        view.ChildComponentIndex = ToApiIndex(current.child_component_index);
        view.LocalDegreeOfFreedomIndex = ToApiIndex(current.local_dof_index);
        view.Coordinate = current.coordinate;
        view.Rate = current.rate;
        view.AtLowerLimit = ToApiBool(current.at_lower_limit);
        view.AtUpperLimit = ToApiBool(current.at_upper_limit);

        if (current.child_component_index < config.vehicle.components.size())
        {
            const tgsim::ComponentDefinition& component =
                config.vehicle.components[current.child_component_index];
            if (current.local_dof_index <
                component.articulation_to_parent.dofs.size())
            {
                const tgsim::ArticulationDof& dof =
                    component.articulation_to_parent.dofs[
                        current.local_dof_index];
                view.Name = ToStringView(dof.name);
                view.MotionType = ToApiJointMotion(dof.motion);
                view.AxisJoint = ToApi(dof.axis_joint);
                view.MinimumCoordinate = dof.limits.minimum_coordinate;
                view.MaximumCoordinate = dof.limits.maximum_coordinate;
                view.MaximumAbsoluteRate = dof.limits.maximum_absolute_rate;
                view.MaximumAbsoluteEffort = dof.limits.maximum_absolute_effort;
            }
        }
        joint_views_.push_back(view);
    }

    reaction_wheel_views_.clear();
    reaction_wheel_views_.reserve(config.vehicle.reaction_wheels.size());
    for (std::size_t index = 0;
         index < config.vehicle.reaction_wheels.size(); ++index)
    {
        const tgsim::ReactionWheelDefinition& definition =
            config.vehicle.reaction_wheels[index];
        const tgsim::ReactionWheelControlState* current =
            index < input.reaction_wheels.size()
                ? &input.reaction_wheels[index] : nullptr;

        TGReactionWheelStateView view{};
        view.ReactionWheelIndex = static_cast<uint64_t>(index);
        view.Name = ToStringView(definition.name);
        view.MountComponentIndex = ToApiIndex(definition.component_index);
        view.AxisComponent = ToApi(definition.axis_component);
        view.MaximumAbsoluteMomentumNewtonMeterSeconds =
            definition.maximum_momentum_nms;
        if (current != nullptr)
        {
            view.CurrentMomentumNewtonMeterSeconds = current->momentum_nms;
            view.SaturatedNegative = ToApiBool(current->saturated_negative);
            view.SaturatedPositive = ToApiBool(current->saturated_positive);
        }
        reaction_wheel_views_.push_back(view);
    }

    celestial_body_views_.clear();
    celestial_body_views_.reserve(config.gravity.bodies.size());
    for (std::size_t index = 0;
         index < config.gravity.bodies.size(); ++index)
    {
        const tgsim::GravityBody& definition = config.gravity.bodies[index];
        const tgsim::CelestialBodyControlState* current =
            index < input.celestial_bodies.size()
                ? &input.celestial_bodies[index] : nullptr;

        TGCelestialBodyStateView view{};
        view.CelestialBodyIndex = static_cast<uint64_t>(index);
        view.Name = ToStringView(definition.name);
        view.NaifId = definition.naif_id;
        view.GravitySourceRole = ToApiGravityRole(
            definition.gravity_source_role);
        view.GravityExplicitlyEnabled =
            ToApiBool(definition.gravity_enabled);
        view.AutomaticActivationRadiusMeters =
            definition.automatic_gravity_activation_radius_m;
        view.BarycenterResolutionRadiusMeters =
            definition.barycenter_resolution_radius_m;
        view.GravitationalParameterMetersCubedPerSecondSquared =
            definition.gravitational_parameter_m3ps2;
        view.ReferenceRadiusMeters = definition.reference_radius_m;
        if (current != nullptr)
        {
            view.PositionIcrfMeters = ToApi(current->position_icrf_m);
            view.VelocityIcrfMetersPerSecond =
                ToApi(current->velocity_icrf_mps);
            view.BodyFixedToIcrf = ToApi(current->body_fixed_to_icrf);
        }
        celestial_body_views_.push_back(view);
    }

    const tgsim::SpacecraftState& state = input.spacecraft_state;
    TGSpacecraftStateView api_state{};
    api_state.EphemerisTimeTdbSeconds = state.ephemeris_time_tdb_seconds;
    api_state.PositionIcrfMeters = ToApi(state.position_icrf_m);
    api_state.VelocityIcrfMetersPerSecond = ToApi(state.velocity_icrf_mps);
    api_state.AttitudeBodyToIcrf = ToApi(state.attitude_body_to_icrf);
    api_state.AngularVelocityBodyRadiansPerSecond =
        ToApi(state.angular_velocity_body_radps);
    api_state.TotalMassKilograms = state.mass_kg;
    api_state.VariableComponentMassesKilograms =
        DataOrNull(state.variable_component_masses_kg);
    api_state.VariableComponentMassCount =
        static_cast<uint64_t>(state.variable_component_masses_kg.size());
    api_state.ArticulationCoordinates =
        DataOrNull(state.articulation_coordinates);
    api_state.ArticulationCoordinateCount =
        static_cast<uint64_t>(state.articulation_coordinates.size());
    api_state.ArticulationRates = DataOrNull(state.articulation_rates);
    api_state.ArticulationRateCount =
        static_cast<uint64_t>(state.articulation_rates.size());
    api_state.ReactionWheelMomentaNewtonMeterSeconds =
        DataOrNull(state.internal_angular_momenta_nms);
    api_state.ReactionWheelMomentumCount =
        static_cast<uint64_t>(state.internal_angular_momenta_nms.size());

    TGControlInput api_input{};
    api_input.StructSize = sizeof(TGControlInput);
    api_input.EphemerisTimeTdbSeconds = input.ephemeris_time_tdb_seconds;
    api_input.ElapsedSimulationTimeSeconds = input.elapsed_time_seconds;
    api_input.Configuration = api_config;
    api_input.SpacecraftState = api_state;
    api_input.CenterOfMassBodyMeters = ToApi(input.center_of_mass_body_m);
    api_input.InertiaBodyKilogramMetersSquared =
        ToApi(input.inertia_body_kgm2);
    api_input.Components = DataOrNull(component_views_);
    api_input.ComponentCount = static_cast<uint64_t>(component_views_.size());
    api_input.Thrusters = DataOrNull(thruster_views_);
    api_input.ThrusterCount = static_cast<uint64_t>(thruster_views_.size());
    api_input.Joints = DataOrNull(joint_views_);
    api_input.JointCount = static_cast<uint64_t>(joint_views_.size());
    api_input.ReactionWheels = DataOrNull(reaction_wheel_views_);
    api_input.ReactionWheelCount =
        static_cast<uint64_t>(reaction_wheel_views_.size());
    api_input.CelestialBodies = DataOrNull(celestial_body_views_);
    api_input.CelestialBodyCount =
        static_cast<uint64_t>(celestial_body_views_.size());

    // The host owns zero-initialized output storage; the DLL only writes values.
    thruster_commands_.assign(
        config.vehicle.thrusters.size(), TGThrusterCommand{});
    joint_efforts_.assign(joint_views_.size(), 0.0);
    reaction_wheel_momentum_rates_.assign(
        config.vehicle.reaction_wheels.size(), 0.0);

    TGControlOutput api_output{};
    api_output.StructSize = sizeof(TGControlOutput);
    api_output.ThrusterCommands = MutableDataOrNull(thruster_commands_);
    api_output.ThrusterCommandCount =
        static_cast<uint64_t>(thruster_commands_.size());
    api_output.JointEffortsNewtonMetersOrNewtons =
        MutableDataOrNull(joint_efforts_);
    api_output.JointEffortCount =
        static_cast<uint64_t>(joint_efforts_.size());
    api_output.ReactionWheelMomentumRatesNewtonMeters =
        MutableDataOrNull(reaction_wheel_momentum_rates_);
    api_output.ReactionWheelMomentumRateCount =
        static_cast<uint64_t>(reaction_wheel_momentum_rates_.size());

    const uint32_t result = compute_controller_(
        controller_instance_, &api_input, &api_output);
    if (result != TG_CONTROLLER_RESULT_OK)
    {
        if (!runtime_failure_reported_)
        {
            std::cerr << "Controller '" << loaded_dll_path_.string()
                      << "' returned runtime error code " << result
                      << "; commands remain zero for that evaluation.\n";
            runtime_failure_reported_ = true;
        }
        return;
    }

    for (std::size_t index = 0; index < thruster_commands_.size(); ++index)
    {
        const TGThrusterCommand& command = thruster_commands_[index];
        output.SetThrusterCommand(
            index,
            command.Throttle,
            command.SpecificImpulseSeconds);
        if (command.MassFlowDerivativeProvided == 0u) continue;
        if (command.MassFlowDerivativeProvided != 1u ||
            !std::isfinite(
                command.MassFlowDerivativeKilogramsPerSecondSquared))
        {
            if (!derivative_failure_reported_)
            {
                std::cerr << "Controller '"
                          << loaded_dll_path_.string()
                          << "' supplied an invalid mass-flow derivative "
                             "entry; that entry was omitted.\n";
                derivative_failure_reported_ = true;
            }
            continue;
        }
        output.SetThrusterMassFlowDerivative(
            index,
            command.MassFlowDerivativeKilogramsPerSecondSquared);
    }
    for (std::size_t index = 0; index < joint_efforts_.size(); ++index)
    {
        output.SetJointEffort(index, joint_efforts_[index]);
    }
    for (std::size_t index = 0;
         index < reaction_wheel_momentum_rates_.size(); ++index)
    {
        output.SetReactionWheelMomentumRate(
            index, reaction_wheel_momentum_rates_[index]);
    }
    output.SetExternalTorqueBody(
        FromApi(api_output.AdditionalExternalTorqueBodyNewtonMeters));
}
