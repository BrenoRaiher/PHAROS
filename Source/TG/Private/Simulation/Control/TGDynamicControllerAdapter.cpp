// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/Control/TGDynamicControllerAdapter.h"

#include "HAL/PlatformProcess.h"
#include "Misc/ScopeLock.h"
#include "Misc/Paths.h"

#include <algorithm>
#include <cmath>
#include <limits>

DEFINE_LOG_CATEGORY_STATIC(
    LogTGController,
    Log,
    All);

namespace
{
    FCriticalSection LoadedControllerDllMutex;
    TMap<FString, int32> LoadedControllerDllReferenceCounts;

    TGStringView ToStringView(const std::string& Value)
    {
        return {
            Value.empty() ? nullptr : Value.data(),
            static_cast<uint64>(Value.size())};
    }

    TGVec3 ToApi(const tgsim::Vec3d& Value)
    {
        return {Value.x, Value.y, Value.z};
    }

    TGQuat ToApi(const tgsim::Quatd& Value)
    {
        return {Value.w, Value.x, Value.y, Value.z};
    }

    TGMat3 ToApi(const tgsim::Mat3d& Value)
    {
        TGMat3 Result{};

        for (int Row = 0; Row < 3; ++Row)
        {
            for (int Column = 0; Column < 3; ++Column)
            {
                Result.M[Row * 3 + Column] =
                    Value.m[Row][Column];
            }
        }

        return Result;
    }

    tgsim::Vec3d FromApi(const TGVec3& Value)
    {
        return {Value.X, Value.Y, Value.Z};
    }

    uint64 ToApiIndex(const std::size_t Index)
    {
        return Index == tgsim::kInvalidIndex
            ? TG_CONTROLLER_INVALID_INDEX
            : static_cast<uint64>(Index);
    }

    TGBool ToApiBool(const bool bValue)
    {
        return bValue ? 1u : 0u;
    }

    uint32 ToApiIntegratorKind(
        const tgsim::IntegratorKind Kind)
    {
        return Kind ==
                tgsim::IntegratorKind::AdaptiveDormandPrince54
            ? TG_INTEGRATOR_ADAPTIVE_DORMAND_PRINCE_54
            : TG_INTEGRATOR_FIXED_STEP_RK4;
    }

    uint32 ToApiOutputMode(const tgsim::OutputMode Mode)
    {
        return Mode == tgsim::OutputMode::FixedInterval
            ? TG_OUTPUT_FIXED_INTERVAL
            : TG_OUTPUT_EVERY_INTEGRATOR_STEP;
    }

    uint32 ToApiJointMotion(
        const tgsim::ArticulationMotion Motion)
    {
        return Motion == tgsim::ArticulationMotion::Translation
            ? TG_JOINT_TRANSLATION
            : TG_JOINT_ROTATION;
    }

    uint32 ToApiThrusterMode(const tgsim::ThrusterMode Mode)
    {
        return Mode == tgsim::ThrusterMode::Commanded
            ? TG_THRUSTER_COMMANDED
            : TG_THRUSTER_PRESCRIBED_PROFILE;
    }

    uint32 ToApiGravityRole(
        const tgsim::GravitySourceRole Role)
    {
        switch (Role)
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

    uint32 ToApiAtmosphereModel(
        const tgsim::AtmosphereModelKind Model)
    {
        return Model ==
                tgsim::AtmosphereModelKind::CubicHarrisPriesterEarth
            ? TG_ATMOSPHERE_CUBIC_HARRIS_PRIESTER_EARTH
            : TG_ATMOSPHERE_TABULATED_PROFILE;
    }

    template <typename FunctionType>
    FunctionType ResolveExport(
        void* DllHandle,
        const ANSICHAR* ExportName)
    {
        return reinterpret_cast<FunctionType>(
            FPlatformProcess::GetDllExport(
                DllHandle,
                ANSI_TO_TCHAR(ExportName)));
    }

    FString NormalizeExistingDllPath(
        const FString& DllPath)
    {
        FString Result =
            FPaths::ConvertRelativePathToFull(DllPath);
        FPaths::NormalizeFilename(Result);
        return Result;
    }

    void RegisterLoadedDll(const FString& DllPath)
    {
        FScopeLock Lock(&LoadedControllerDllMutex);
        int32& ReferenceCount =
            LoadedControllerDllReferenceCounts.FindOrAdd(DllPath);
        ++ReferenceCount;
    }

    void UnregisterLoadedDll(const FString& DllPath)
    {
        FScopeLock Lock(&LoadedControllerDllMutex);

        int32* ReferenceCount =
            LoadedControllerDllReferenceCounts.Find(DllPath);

        if (ReferenceCount == nullptr)
        {
            return;
        }

        --(*ReferenceCount);

        if (*ReferenceCount <= 0)
        {
            LoadedControllerDllReferenceCounts.Remove(DllPath);
        }
    }

    template <typename ElementType>
    const ElementType* DataOrNull(
        const std::vector<ElementType>& Values)
    {
        return Values.empty() ? nullptr : Values.data();
    }

    template <typename ElementType>
    ElementType* MutableDataOrNull(
        std::vector<ElementType>& Values)
    {
        return Values.empty() ? nullptr : Values.data();
    }

    bool ValidateControllerContract(
        const TGDescribeControllerContractFunction DescribeContract,
        FString& OutError)
    {
        TGControllerContract Contract{};
        Contract.StructSize = sizeof(TGControllerContract);

        if (DescribeContract == nullptr ||
            DescribeContract(&Contract) != TG_CONTROLLER_RESULT_OK ||
            Contract.StructSize != sizeof(TGControllerContract) ||
            Contract.ControlInputSize != sizeof(TGControlInput) ||
            Contract.ControlOutputSize != sizeof(TGControlOutput) ||
            Contract.ThrusterCommandSize != sizeof(TGThrusterCommand) ||
            Contract.SimulationConfigurationSize !=
                sizeof(TGSimulationConfiguration) ||
            Contract.SpacecraftStateViewSize !=
                sizeof(TGSpacecraftStateView) ||
            Contract.ComponentStateViewSize != sizeof(TGComponentStateView) ||
            Contract.JointStateViewSize != sizeof(TGJointStateView) ||
            Contract.ThrusterStateViewSize != sizeof(TGThrusterStateView) ||
            Contract.ReactionWheelStateViewSize !=
                sizeof(TGReactionWheelStateView) ||
            Contract.CelestialBodyStateViewSize !=
                sizeof(TGCelestialBodyStateView))
        {
            OutError = TEXT(
                "The controller does not match the PHAROS Controller API. "
                "Rebuild it with the Controller SDK distributed with this application.");
            return false;
        }

        return true;
    }
}

FTGDynamicControllerAdapter::~FTGDynamicControllerAdapter()
{
    Unload();
}

bool FTGDynamicControllerAdapter::ProbeDll(
    const FString& DllPath,
    FString& OutError)
{
    OutError.Reset();

    const FString NormalizedPath =
        NormalizeExistingDllPath(DllPath);

    if (!FPaths::FileExists(NormalizedPath))
    {
        OutError = TEXT("The controller DLL does not exist.");
        return false;
    }

    void* Handle =
        FPlatformProcess::GetDllHandle(*NormalizedPath);

    if (Handle == nullptr)
    {
        OutError =
            TEXT("Windows could not load the controller DLL.");
        return false;
    }

    const TGDescribeControllerContractFunction DescribeContract =
        ResolveExport<TGDescribeControllerContractFunction>(
            Handle,
            TG_DESCRIBE_CONTROLLER_CONTRACT_EXPORT);

    const TGCreateControllerFunction CreateController =
        ResolveExport<TGCreateControllerFunction>(
            Handle,
            TG_CREATE_CONTROLLER_EXPORT);

    const TGDestroyControllerFunction DestroyController =
        ResolveExport<TGDestroyControllerFunction>(
            Handle,
            TG_DESTROY_CONTROLLER_EXPORT);

    const TGComputeControlFunction ComputeController =
        ResolveExport<TGComputeControlFunction>(
            Handle,
            TG_COMPUTE_CONTROL_EXPORT);

    if (DescribeContract == nullptr ||
        CreateController == nullptr ||
        DestroyController == nullptr ||
        ComputeController == nullptr)
    {
        OutError =
            TEXT("The DLL is missing one or more required PHAROS exports.");
        FPlatformProcess::FreeDllHandle(Handle);
        return false;
    }

    if (!ValidateControllerContract(DescribeContract, OutError))
    {
        FPlatformProcess::FreeDllHandle(Handle);
        return false;
    }

    void* ProbeInstance = CreateController();

    if (ProbeInstance == nullptr)
    {
        OutError =
            TEXT("The controller DLL could not create an instance.");
        FPlatformProcess::FreeDllHandle(Handle);
        return false;
    }

    DestroyController(ProbeInstance);
    FPlatformProcess::FreeDllHandle(Handle);
    return true;
}

std::shared_ptr<FTGDynamicControllerAdapter>
FTGDynamicControllerAdapter::Create(
    const FString& DllPath,
    FString& OutError)
{
    std::shared_ptr<FTGDynamicControllerAdapter> Result(
        new FTGDynamicControllerAdapter());

    if (!Result->Load(DllPath, OutError))
    {
        return nullptr;
    }

    return Result;
}

bool FTGDynamicControllerAdapter::IsAnyDllLoadedBelow(
    const FString& DirectoryPath)
{
    FString NormalizedDirectory =
        FPaths::ConvertRelativePathToFull(DirectoryPath);
    FPaths::NormalizeDirectoryName(NormalizedDirectory);

    const FString Prefix = NormalizedDirectory + TEXT("/");
    FScopeLock Lock(&LoadedControllerDllMutex);

    for (const TPair<FString, int32>& Entry :
         LoadedControllerDllReferenceCounts)
    {
        if (Entry.Value > 0 &&
            (Entry.Key.Equals(
                 NormalizedDirectory,
                 ESearchCase::IgnoreCase) ||
             Entry.Key.StartsWith(
                 Prefix,
                 ESearchCase::IgnoreCase)))
        {
            return true;
        }
    }

    return false;
}

bool FTGDynamicControllerAdapter::Load(
    const FString& DllPath,
    FString& OutError)
{
    OutError.Reset();
    LoadedDllPath = NormalizeExistingDllPath(DllPath);

    if (!FPaths::FileExists(LoadedDllPath))
    {
        OutError = TEXT("The selected controller DLL does not exist.");
        return false;
    }

    DllHandle =
        FPlatformProcess::GetDllHandle(*LoadedDllPath);

    if (DllHandle == nullptr)
    {
        OutError =
            TEXT("Windows could not load the selected controller DLL.");
        return false;
    }

    const TGDescribeControllerContractFunction DescribeContract =
        ResolveExport<TGDescribeControllerContractFunction>(
            DllHandle,
            TG_DESCRIBE_CONTROLLER_CONTRACT_EXPORT);

    const TGCreateControllerFunction CreateController =
        ResolveExport<TGCreateControllerFunction>(
            DllHandle,
            TG_CREATE_CONTROLLER_EXPORT);

    DestroyController =
        ResolveExport<TGDestroyControllerFunction>(
            DllHandle,
            TG_DESTROY_CONTROLLER_EXPORT);

    ComputeController =
        ResolveExport<TGComputeControlFunction>(
            DllHandle,
            TG_COMPUTE_CONTROL_EXPORT);
    NextDiscontinuity =
        ResolveExport<TGNextControllerDiscontinuityFunction>(
            DllHandle,
            TG_NEXT_CONTROLLER_DISCONTINUITY_EXPORT);

    if (DescribeContract == nullptr ||
        CreateController == nullptr ||
        DestroyController == nullptr ||
        ComputeController == nullptr)
    {
        OutError =
            TEXT("The selected DLL is missing required PHAROS exports.");
        Unload();
        return false;
    }

    if (!ValidateControllerContract(DescribeContract, OutError))
    {
        Unload();
        return false;
    }

    ControllerInstance = CreateController();

    if (ControllerInstance == nullptr)
    {
        OutError =
            TEXT("The selected DLL could not create a controller instance.");
        Unload();
        return false;
    }

    RegisterLoadedDll(LoadedDllPath);
    bRegisteredAsLoaded = true;

    return true;
}

void FTGDynamicControllerAdapter::Unload()
{
    if (ControllerInstance != nullptr &&
        DestroyController != nullptr)
    {
        DestroyController(ControllerInstance);
    }

    ControllerInstance = nullptr;
    DestroyController = nullptr;
    ComputeController = nullptr;
    NextDiscontinuity = nullptr;

    if (DllHandle != nullptr)
    {
        FPlatformProcess::FreeDllHandle(DllHandle);
        DllHandle = nullptr;
    }

    if (bRegisteredAsLoaded)
    {
        UnregisterLoadedDll(LoadedDllPath);
        bRegisteredAsLoaded = false;
    }
}

double FTGDynamicControllerAdapter::NextDiscontinuityElapsedTime(
    const double CurrentElapsedTimeSeconds) const
{
    if (ControllerInstance == nullptr || NextDiscontinuity == nullptr)
        return std::numeric_limits<double>::infinity();

    const double Result = NextDiscontinuity(
        ControllerInstance,
        CurrentElapsedTimeSeconds);
    return std::isfinite(Result) && Result > CurrentElapsedTimeSeconds
        ? Result
        : std::numeric_limits<double>::infinity();
}

void FTGDynamicControllerAdapter::ComputeControl(
    const tgsim::ControlInput& Input,
    tgsim::ControlCommandWriter& Output) const
{
    if (ControllerInstance == nullptr ||
        ComputeController == nullptr)
    {
        return;
    }

    const tgsim::SimulationConfig& Config =
        Input.configuration;

    TGSimulationConfiguration ApiConfig{};
    ApiConfig.ScenarioName = ToStringView(Config.scenario_name);
    ApiConfig.IntegratorKind =
        ToApiIntegratorKind(Config.solver.integrator_kind);
    ApiConfig.OutputMode =
        ToApiOutputMode(Config.solver.output_mode);
    ApiConfig.StartEphemerisTimeTdbSeconds =
        Config.solver.start_ephemeris_time_tdb_seconds;
    ApiConfig.FinalEphemerisTimeTdbSeconds =
        Config.solver.final_ephemeris_time_tdb_seconds;
    ApiConfig.MaximumIntegratorStepSeconds =
        Config.solver.maximum_integrator_step_seconds;
    ApiConfig.InitialIntegratorStepSeconds =
        Config.solver.initial_integrator_step_seconds;
    ApiConfig.AbsoluteTolerance =
        Config.solver.absolute_tolerance;
    ApiConfig.RelativeTolerance =
        Config.solver.relative_tolerance;
    ApiConfig.OutputStepSeconds =
        Config.solver.output_step_seconds;
    ApiConfig.MaximumIntegrationSteps =
        static_cast<uint64>(Config.solver.maximum_integration_steps);
    ApiConfig.MaximumOutputSamples =
        static_cast<uint64>(Config.solver.maximum_output_samples);
    ApiConfig.IncludeFirstPostNewtonianCorrection =
        ToApiBool(
            Config.gravity.
                include_first_post_newtonian_correction);
    ApiConfig.SolarRadiationPressureEnabled =
        ToApiBool(Config.solar_radiation.enabled);
    ApiConfig.ComputeEclipseShadow =
        ToApiBool(
            Config.solar_radiation.compute_eclipse_shadow);
    ApiConfig.AtmosphereEnabled =
        ToApiBool(Config.atmosphere.enabled);
    ApiConfig.AerodynamicsEnabled =
        ToApiBool(Config.aerodynamics.enabled);
    ApiConfig.AerodynamicCoefficientDatabaseEnabled =
        ToApiBool(Config.aerodynamics.coefficient_database.enabled);
    ApiConfig.AerodynamicConstantDragFallbackEnabled =
        ToApiBool(Config.aerodynamics.constant_drag_fallback_enabled);
    ApiConfig.ComputeComponentSrpShadows =
        ToApiBool(Config.solar_radiation.compute_component_shadows);
    ApiConfig.SunBodyName =
        ToStringView(Config.solar_radiation.sun_body_name);
    ApiConfig.SolarPressureAtOneAstronomicalUnitPascals =
        Config.solar_radiation.pressure_at_one_au_pa;
    ApiConfig.AtmosphereCentralBodyName =
        ToStringView(Config.atmosphere.central_body_name);
    ApiConfig.AtmosphereModel =
        ToApiAtmosphereModel(Config.atmosphere.model_kind);
    ApiConfig.AtmosphereDensityProfileSampleCount =
        static_cast<uint64>(Config.atmosphere.density_profile.size());
    ApiConfig.AtmosphereThermodynamicProfileSampleCount =
        static_cast<uint64>(Config.atmosphere.thermodynamic_profile.size());
    ApiConfig.AtmosphereCubicHarrisPriesterSampleCount =
        static_cast<uint64>(Config.atmosphere.cubic_harris_priester.
            density_samples.size());
    ApiConfig.AtmosphereCenteredAverageF107SolarFluxUnits =
        Config.atmosphere.cubic_harris_priester.
            centered_average_f107_sfu;
    ApiConfig.AerodynamicReferenceAreaSquareMeters =
        Config.aerodynamics.reference_area_m2;
    ApiConfig.AerodynamicReferenceLengthMeters =
        Config.aerodynamics.reference_length_m;
    ApiConfig.AerodynamicMomentReferencePointBodyMeters =
        ToApi(Config.aerodynamics.coefficient_database.
            moment_reference_point_body_m);
    ApiConfig.AerodynamicFallbackDragCoefficient =
        Config.aerodynamics.fallback_drag_coefficient;
    ApiConfig.AerodynamicMinimumDynamicPressurePascals =
        Config.aerodynamics.minimum_dynamic_pressure_pa;
    ApiConfig.AerodynamicMaximumDynamicPressurePascals =
        Config.aerodynamics.maximum_dynamic_pressure_pa;

    ComponentViews.clear();
    ComponentViews.reserve(Config.vehicle.components.size());

    for (std::size_t Index = 0;
         Index < Config.vehicle.components.size();
         ++Index)
    {
        const tgsim::ComponentDefinition& Definition =
            Config.vehicle.components[Index];

        const tgsim::ComponentControlState* Current =
            Index < Input.components.size()
                ? &Input.components[Index]
                : nullptr;

        TGComponentStateView View{};
        View.ComponentIndex = static_cast<uint64>(Index);
        View.ParentComponentIndex =
            ToApiIndex(Definition.parent_component_index);
        View.VariableMassStateIndex =
            ToApiIndex(Definition.variable_mass_state_index);
        View.Name = ToStringView(Definition.name);
        View.CurrentMassKilograms =
            Current != nullptr
                ? Current->current_mass_kg
                : Definition.initial_mass_kg;
        View.InitialMassKilograms =
            Definition.initial_mass_kg;
        View.MinimumMassKilograms =
            Definition.minimum_mass_kg;
        View.HasVariableMass = ToApiBool(
            Definition.variable_mass_state_index !=
                tgsim::kInvalidIndex);
        View.CurrentOriginBodyMeters =
            Current != nullptr
                ? ToApi(Current->origin_body_m)
                : ToApi(Definition.origin_body_m);
        View.CurrentComponentToBody =
            Current != nullptr
                ? ToApi(Current->component_to_body)
                : ToApi(Definition.component_to_body);
        View.LocalCenterOfMassMeters =
            ToApi(Definition.center_of_mass_component_m);
        View.LocalCentroidalInertiaKilogramMetersSquared =
            ToApi(Definition.inertia_centroid_component_kgm2);

        ComponentViews.push_back(View);
    }

    ThrusterViews.clear();
    ThrusterViews.reserve(Config.vehicle.thrusters.size());

    for (std::size_t Index = 0;
         Index < Config.vehicle.thrusters.size();
         ++Index)
    {
        const tgsim::ThrusterDefinition& Definition =
            Config.vehicle.thrusters[Index];

        const tgsim::ThrusterControlState* Current =
            Index < Input.thrusters.size()
                ? &Input.thrusters[Index]
                : nullptr;

        TGThrusterStateView View{};
        View.ThrusterIndex = static_cast<uint64>(Index);
        View.Name = ToStringView(Definition.name);
        View.Mode = ToApiThrusterMode(Definition.mode);
        View.MountComponentIndex =
            ToApiIndex(Definition.component_index);
        View.PropellantComponentIndex =
            ToApiIndex(
                Definition.propellant_component_index);
        View.ApplicationPointComponentMeters =
            ToApi(Definition.application_point_component_m);
        View.DirectionComponent =
            ToApi(Definition.direction_component);
        View.IgnitionEphemerisTimeTdbSeconds =
            Definition.ignition_ephemeris_time_tdb_seconds;
        View.ShutdownEphemerisTimeTdbSeconds =
            Definition.shutdown_ephemeris_time_tdb_seconds;
        View.MaximumThrustNewtons =
            Definition.maximum_thrust_n;

        if (Current != nullptr)
        {
            View.CurrentPropellantComponentMassKilograms =
                Current->current_propellant_component_mass_kg;
            View.FiringWindowOpen =
                ToApiBool(Current->firing_window_open);
            View.HasPropellantComponent =
                ToApiBool(Current->has_propellant_component);
            View.PropellantAvailable =
                ToApiBool(Current->propellant_available);
        }

        ThrusterViews.push_back(View);
    }

    JointViews.clear();
    JointViews.reserve(Input.joints.size());

    for (const tgsim::JointControlState& Current : Input.joints)
    {
        TGJointStateView View{};
        View.ArticulationStateIndex =
            ToApiIndex(Current.articulation_state_index);
        View.ChildComponentIndex =
            ToApiIndex(Current.child_component_index);
        View.LocalDegreeOfFreedomIndex =
            ToApiIndex(Current.local_dof_index);
        View.Coordinate = Current.coordinate;
        View.Rate = Current.rate;
        View.AtLowerLimit =
            ToApiBool(Current.at_lower_limit);
        View.AtUpperLimit =
            ToApiBool(Current.at_upper_limit);

        if (Current.child_component_index <
                Config.vehicle.components.size())
        {
            const tgsim::ComponentDefinition& Component =
                Config.vehicle.components[
                    Current.child_component_index];

            if (Current.local_dof_index <
                Component.articulation_to_parent.dofs.size())
            {
                const tgsim::ArticulationDof& Dof =
                    Component.articulation_to_parent.dofs[
                        Current.local_dof_index];

                View.Name = ToStringView(Dof.name);
                View.MotionType =
                    ToApiJointMotion(Dof.motion);
                View.AxisJoint = ToApi(Dof.axis_joint);
                View.MinimumCoordinate =
                    Dof.limits.minimum_coordinate;
                View.MaximumCoordinate =
                    Dof.limits.maximum_coordinate;
                View.MaximumAbsoluteRate =
                    Dof.limits.maximum_absolute_rate;
                View.MaximumAbsoluteEffort =
                    Dof.limits.maximum_absolute_effort;
            }
        }

        JointViews.push_back(View);
    }

    ReactionWheelViews.clear();
    ReactionWheelViews.reserve(
        Config.vehicle.reaction_wheels.size());

    for (std::size_t Index = 0;
         Index < Config.vehicle.reaction_wheels.size();
         ++Index)
    {
        const tgsim::ReactionWheelDefinition& Definition =
            Config.vehicle.reaction_wheels[Index];

        const tgsim::ReactionWheelControlState* Current =
            Index < Input.reaction_wheels.size()
                ? &Input.reaction_wheels[Index]
                : nullptr;

        TGReactionWheelStateView View{};
        View.ReactionWheelIndex = static_cast<uint64>(Index);
        View.Name = ToStringView(Definition.name);
        View.MountComponentIndex =
            ToApiIndex(Definition.component_index);
        View.AxisComponent = ToApi(Definition.axis_component);
        View.MaximumAbsoluteMomentumNewtonMeterSeconds =
            Definition.maximum_momentum_nms;

        if (Current != nullptr)
        {
            View.CurrentMomentumNewtonMeterSeconds =
                Current->momentum_nms;
            View.SaturatedNegative =
                ToApiBool(Current->saturated_negative);
            View.SaturatedPositive =
                ToApiBool(Current->saturated_positive);
        }

        ReactionWheelViews.push_back(View);
    }

    CelestialBodyViews.clear();
    CelestialBodyViews.reserve(Config.gravity.bodies.size());

    for (std::size_t Index = 0;
         Index < Config.gravity.bodies.size();
         ++Index)
    {
        const tgsim::GravityBody& Definition =
            Config.gravity.bodies[Index];

        const tgsim::CelestialBodyControlState* Current =
            Index < Input.celestial_bodies.size()
                ? &Input.celestial_bodies[Index]
                : nullptr;

        TGCelestialBodyStateView View{};
        View.CelestialBodyIndex = static_cast<uint64>(Index);
        View.Name = ToStringView(Definition.name);
        View.NaifId = Definition.naif_id;
        View.GravitySourceRole =
            ToApiGravityRole(Definition.gravity_source_role);
        View.GravityExplicitlyEnabled =
            ToApiBool(Definition.gravity_enabled);
        View.AutomaticActivationRadiusMeters =
            Definition.automatic_gravity_activation_radius_m;
        View.BarycenterResolutionRadiusMeters =
            Definition.barycenter_resolution_radius_m;
        View.GravitationalParameterMetersCubedPerSecondSquared =
            Definition.gravitational_parameter_m3ps2;
        View.ReferenceRadiusMeters =
            Definition.reference_radius_m;

        if (Current != nullptr)
        {
            View.PositionIcrfMeters =
                ToApi(Current->position_icrf_m);
            View.VelocityIcrfMetersPerSecond =
                ToApi(Current->velocity_icrf_mps);
            View.BodyFixedToIcrf =
                ToApi(Current->body_fixed_to_icrf);
        }

        CelestialBodyViews.push_back(View);
    }

    const tgsim::SpacecraftState& State =
        Input.spacecraft_state;

    TGSpacecraftStateView ApiState{};
    ApiState.EphemerisTimeTdbSeconds =
        State.ephemeris_time_tdb_seconds;
    ApiState.PositionIcrfMeters =
        ToApi(State.position_icrf_m);
    ApiState.VelocityIcrfMetersPerSecond =
        ToApi(State.velocity_icrf_mps);
    ApiState.AttitudeBodyToIcrf =
        ToApi(State.attitude_body_to_icrf);
    ApiState.AngularVelocityBodyRadiansPerSecond =
        ToApi(State.angular_velocity_body_radps);
    ApiState.TotalMassKilograms = State.mass_kg;
    ApiState.VariableComponentMassesKilograms =
        DataOrNull(State.variable_component_masses_kg);
    ApiState.VariableComponentMassCount =
        static_cast<uint64>(
            State.variable_component_masses_kg.size());
    ApiState.ArticulationCoordinates =
        DataOrNull(State.articulation_coordinates);
    ApiState.ArticulationCoordinateCount =
        static_cast<uint64>(
            State.articulation_coordinates.size());
    ApiState.ArticulationRates =
        DataOrNull(State.articulation_rates);
    ApiState.ArticulationRateCount =
        static_cast<uint64>(State.articulation_rates.size());
    ApiState.ReactionWheelMomentaNewtonMeterSeconds =
        DataOrNull(State.internal_angular_momenta_nms);
    ApiState.ReactionWheelMomentumCount =
        static_cast<uint64>(
            State.internal_angular_momenta_nms.size());

    TGControlInput ApiInput{};
    ApiInput.StructSize = sizeof(TGControlInput);
    ApiInput.EphemerisTimeTdbSeconds =
        Input.ephemeris_time_tdb_seconds;
    ApiInput.ElapsedSimulationTimeSeconds =
        Input.elapsed_time_seconds;
    ApiInput.Configuration = ApiConfig;
    ApiInput.SpacecraftState = ApiState;
    ApiInput.CenterOfMassBodyMeters =
        ToApi(Input.center_of_mass_body_m);
    ApiInput.InertiaBodyKilogramMetersSquared =
        ToApi(Input.inertia_body_kgm2);
    ApiInput.Components = DataOrNull(ComponentViews);
    ApiInput.ComponentCount =
        static_cast<uint64>(ComponentViews.size());
    ApiInput.Thrusters = DataOrNull(ThrusterViews);
    ApiInput.ThrusterCount =
        static_cast<uint64>(ThrusterViews.size());
    ApiInput.Joints = DataOrNull(JointViews);
    ApiInput.JointCount =
        static_cast<uint64>(JointViews.size());
    ApiInput.ReactionWheels =
        DataOrNull(ReactionWheelViews);
    ApiInput.ReactionWheelCount =
        static_cast<uint64>(ReactionWheelViews.size());
    ApiInput.CelestialBodies =
        DataOrNull(CelestialBodyViews);
    ApiInput.CelestialBodyCount =
        static_cast<uint64>(CelestialBodyViews.size());

    ThrusterCommands.assign(
        Config.vehicle.thrusters.size(),
        TGThrusterCommand{});
    JointEfforts.assign(JointViews.size(), 0.0);
    ReactionWheelMomentumRates.assign(
        Config.vehicle.reaction_wheels.size(),
        0.0);

    TGControlOutput ApiOutput{};
    ApiOutput.StructSize = sizeof(TGControlOutput);
    ApiOutput.ThrusterCommands =
        MutableDataOrNull(ThrusterCommands);
    ApiOutput.ThrusterCommandCount =
        static_cast<uint64>(ThrusterCommands.size());
    ApiOutput.JointEffortsNewtonMetersOrNewtons =
        MutableDataOrNull(JointEfforts);
    ApiOutput.JointEffortCount =
        static_cast<uint64>(JointEfforts.size());
    ApiOutput.ReactionWheelMomentumRatesNewtonMeters =
        MutableDataOrNull(ReactionWheelMomentumRates);
    ApiOutput.ReactionWheelMomentumRateCount =
        static_cast<uint64>(ReactionWheelMomentumRates.size());

    const uint32 Result = ComputeController(
        ControllerInstance,
        &ApiInput,
        &ApiOutput);

    if (Result != TG_CONTROLLER_RESULT_OK)
    {
        if (!bRuntimeFailureLogged)
        {
            UE_LOG(
                LogTGController,
                Error,
                TEXT(
                    "Controller '%s' returned runtime error code %u; "
                    "commands for this evaluation remain zero."),
                *LoadedDllPath,
                Result);

            bRuntimeFailureLogged = true;
        }

        return;
    }

    for (std::size_t Index = 0;
         Index < ThrusterCommands.size();
         ++Index)
    {
        const TGThrusterCommand& Command = ThrusterCommands[Index];
        Output.SetThrusterCommand(
            Index,
            Command.Throttle,
            Command.SpecificImpulseSeconds);
        if (Command.MassFlowDerivativeProvided == 0u) continue;
        if (Command.MassFlowDerivativeProvided != 1u ||
            !std::isfinite(
                Command.MassFlowDerivativeKilogramsPerSecondSquared))
        {
            if (!bDerivativeFailureLogged)
            {
                UE_LOG(
                    LogTGController,
                    Warning,
                    TEXT(
                        "Controller '%s' supplied an invalid mass-flow "
                        "derivative entry; that entry was omitted."),
                    *LoadedDllPath);
                bDerivativeFailureLogged = true;
            }
            continue;
        }
        Output.SetThrusterMassFlowDerivative(
            Index,
            Command.MassFlowDerivativeKilogramsPerSecondSquared);
    }

    for (std::size_t Index = 0;
         Index < JointEfforts.size();
         ++Index)
    {
        Output.SetJointEffort(Index, JointEfforts[Index]);
    }

    for (std::size_t Index = 0;
         Index < ReactionWheelMomentumRates.size();
         ++Index)
    {
        Output.SetReactionWheelMomentumRate(
            Index,
            ReactionWheelMomentumRates[Index]);
    }

    Output.SetExternalTorqueBody(
        FromApi(
            ApiOutput.
                AdditionalExternalTorqueBodyNewtonMeters));
}
