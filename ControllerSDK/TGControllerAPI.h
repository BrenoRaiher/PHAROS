// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#ifndef TG_CONTROLLER_API_H
#define TG_CONTROLLER_API_H

/*
 * Internal C ABI definitions used by PHAROSControllerAPI.h. New controller
 * source should include PHAROSControllerAPI.h. Keep this header independent
 * of Unreal, TGSimCore, and the C++ standard library.
 */

#include <stddef.h>
#include <stdint.h>

#define TG_CONTROLLER_INVALID_INDEX UINT64_MAX

#if defined(_WIN32)
#define TG_CONTROLLER_EXPORT extern "C" __declspec(dllexport)
#define TG_CONTROLLER_CALL __cdecl
#else
#define TG_CONTROLLER_EXPORT extern "C" __attribute__((visibility("default")))
#define TG_CONTROLLER_CALL
#endif

typedef uint8_t TGBool;

enum TGControllerResult
{
    TG_CONTROLLER_RESULT_OK = 0,
    TG_CONTROLLER_RESULT_INVALID_ARGUMENT = 1,
    TG_CONTROLLER_RESULT_CREATION_FAILED = 2,
    TG_CONTROLLER_RESULT_USER_EXCEPTION = 3
};

enum TGIntegratorKind
{
    TG_INTEGRATOR_FIXED_STEP_RK4 = 0,
    TG_INTEGRATOR_ADAPTIVE_DORMAND_PRINCE_54 = 1
};

enum TGOutputMode
{
    TG_OUTPUT_EVERY_INTEGRATOR_STEP = 0,
    TG_OUTPUT_FIXED_INTERVAL = 1
};

enum TGJointMotionType
{
    TG_JOINT_ROTATION = 0,
    TG_JOINT_TRANSLATION = 1
};

enum TGThrusterMode
{
    TG_THRUSTER_PRESCRIBED_PROFILE = 0,
    TG_THRUSTER_COMMANDED = 1
};

enum TGGravitySourceRole
{
    TG_GRAVITY_SOURCE_INDEPENDENT = 0,
    TG_GRAVITY_SOURCE_SYSTEM_BARYCENTER = 1,
    TG_GRAVITY_SOURCE_SYSTEM_MEMBER = 2
};

enum TGAtmosphereModel
{
    TG_ATMOSPHERE_TABULATED_PROFILE = 0,
    TG_ATMOSPHERE_CUBIC_HARRIS_PRIESTER_EARTH = 1
};

typedef struct TGStringView
{
    const char* Data;
    uint64_t Length;
} TGStringView;

typedef struct TGVec3
{
    double X;
    double Y;
    double Z;
} TGVec3;

typedef struct TGQuat
{
    double W;
    double X;
    double Y;
    double Z;
} TGQuat;

/* Row-major: M[row * 3 + column]. */
typedef struct TGMat3
{
    double M[9];
} TGMat3;

typedef struct TGSimulationConfiguration
{
    TGStringView ScenarioName;
    uint32_t IntegratorKind;
    uint32_t OutputMode;
    double StartEphemerisTimeTdbSeconds;
    double FinalEphemerisTimeTdbSeconds;
    double MaximumIntegratorStepSeconds;
    double InitialIntegratorStepSeconds;
    double AbsoluteTolerance;
    double RelativeTolerance;
    double OutputStepSeconds;
    uint64_t MaximumIntegrationSteps;
    uint64_t MaximumOutputSamples;

    TGBool IncludeFirstPostNewtonianCorrection;
    TGBool SolarRadiationPressureEnabled;
    TGBool ComputeEclipseShadow;
    TGBool AtmosphereEnabled;
    TGBool AerodynamicsEnabled;
    TGBool AerodynamicCoefficientDatabaseEnabled;
    TGBool AerodynamicConstantDragFallbackEnabled;
    TGBool ComputeComponentSrpShadows;

    TGStringView SunBodyName;
    double SolarPressureAtOneAstronomicalUnitPascals;

    TGStringView AtmosphereCentralBodyName;
    uint32_t AtmosphereModel;
    uint64_t AtmosphereDensityProfileSampleCount;
    uint64_t AtmosphereThermodynamicProfileSampleCount;
    uint64_t AtmosphereCubicHarrisPriesterSampleCount;
    double AtmosphereCenteredAverageF107SolarFluxUnits;

    double AerodynamicReferenceAreaSquareMeters;
    double AerodynamicReferenceLengthMeters;
    TGVec3 AerodynamicMomentReferencePointBodyMeters;
    double AerodynamicFallbackDragCoefficient;
    double AerodynamicMinimumDynamicPressurePascals;
    double AerodynamicMaximumDynamicPressurePascals;
} TGSimulationConfiguration;

typedef struct TGSpacecraftStateView
{
    double EphemerisTimeTdbSeconds;
    TGVec3 PositionIcrfMeters;
    TGVec3 VelocityIcrfMetersPerSecond;
    TGQuat AttitudeBodyToIcrf;
    TGVec3 AngularVelocityBodyRadiansPerSecond;
    double TotalMassKilograms;

    const double* VariableComponentMassesKilograms;
    uint64_t VariableComponentMassCount;
    const double* ArticulationCoordinates;
    uint64_t ArticulationCoordinateCount;
    const double* ArticulationRates;
    uint64_t ArticulationRateCount;
    const double* ReactionWheelMomentaNewtonMeterSeconds;
    uint64_t ReactionWheelMomentumCount;
} TGSpacecraftStateView;

typedef struct TGComponentStateView
{
    uint64_t ComponentIndex;
    uint64_t ParentComponentIndex;
    uint64_t VariableMassStateIndex;
    TGStringView Name;

    double CurrentMassKilograms;
    double InitialMassKilograms;
    double MinimumMassKilograms;
    TGBool HasVariableMass;
    TGBool ReservedFlags[7];

    TGVec3 CurrentOriginBodyMeters;
    TGMat3 CurrentComponentToBody;
    TGVec3 LocalCenterOfMassMeters;
    TGMat3 LocalCentroidalInertiaKilogramMetersSquared;
} TGComponentStateView;

typedef struct TGJointStateView
{
    uint64_t ArticulationStateIndex;
    uint64_t ChildComponentIndex;
    uint64_t LocalDegreeOfFreedomIndex;
    TGStringView Name;
    uint32_t MotionType;
    uint32_t Reserved;
    TGVec3 AxisJoint;

    double Coordinate;
    double Rate;
    double MinimumCoordinate;
    double MaximumCoordinate;
    double MaximumAbsoluteRate;
    double MaximumAbsoluteEffort;
    TGBool AtLowerLimit;
    TGBool AtUpperLimit;
    TGBool ReservedFlags[6];
} TGJointStateView;

typedef struct TGThrusterStateView
{
    uint64_t ThrusterIndex;
    TGStringView Name;
    uint32_t Mode;
    uint32_t Reserved;
    uint64_t MountComponentIndex;
    uint64_t PropellantComponentIndex;
    TGVec3 ApplicationPointComponentMeters;
    TGVec3 DirectionComponent;
    double IgnitionEphemerisTimeTdbSeconds;
    double ShutdownEphemerisTimeTdbSeconds;
    double MaximumThrustNewtons;
    double CurrentPropellantComponentMassKilograms;
    TGBool FiringWindowOpen;
    TGBool HasPropellantComponent;
    TGBool PropellantAvailable;
    TGBool ReservedFlags[5];
} TGThrusterStateView;

typedef struct TGReactionWheelStateView
{
    uint64_t ReactionWheelIndex;
    TGStringView Name;
    uint64_t MountComponentIndex;
    TGVec3 AxisComponent;
    double CurrentMomentumNewtonMeterSeconds;
    double MaximumAbsoluteMomentumNewtonMeterSeconds;
    TGBool SaturatedNegative;
    TGBool SaturatedPositive;
    TGBool ReservedFlags[6];
} TGReactionWheelStateView;

typedef struct TGCelestialBodyStateView
{
    uint64_t CelestialBodyIndex;
    TGStringView Name;
    int32_t NaifId;
    uint32_t GravitySourceRole;
    TGBool GravityExplicitlyEnabled;
    TGBool ReservedFlags[7];
    double AutomaticActivationRadiusMeters;
    double BarycenterResolutionRadiusMeters;
    double GravitationalParameterMetersCubedPerSecondSquared;
    double ReferenceRadiusMeters;
    TGVec3 PositionIcrfMeters;
    TGVec3 VelocityIcrfMetersPerSecond;
    TGMat3 BodyFixedToIcrf;
} TGCelestialBodyStateView;

/*
 * All pointers are read-only and valid only for one host evaluation. A
 * controller must copy anything it wishes to retain beyond that evaluation.
 */
typedef struct TGControlInput
{
    uint32_t StructSize;
    double EphemerisTimeTdbSeconds;
    double ElapsedSimulationTimeSeconds;
    TGSimulationConfiguration Configuration;
    TGSpacecraftStateView SpacecraftState;
    TGVec3 CenterOfMassBodyMeters;
    TGMat3 InertiaBodyKilogramMetersSquared;

    const TGComponentStateView* Components;
    uint64_t ComponentCount;
    const TGThrusterStateView* Thrusters;
    uint64_t ThrusterCount;
    const TGJointStateView* Joints;
    uint64_t JointCount;
    const TGReactionWheelStateView* ReactionWheels;
    uint64_t ReactionWheelCount;
    const TGCelestialBodyStateView* CelestialBodies;
    uint64_t CelestialBodyCount;
} TGControlInput;

typedef struct TGThrusterCommand
{
    double Throttle;
    double SpecificImpulseSeconds;
    /* Optional total derivative of actual outward q=T/(g0*Isp), in kg/s^2. */
    TGBool MassFlowDerivativeProvided;
    uint8_t Reserved[7];
    double MassFlowDerivativeKilogramsPerSecondSquared;
} TGThrusterCommand;

/*
 * The host owns and zero-initializes every output array. The DLL may change
 * values but must not replace pointers or alter counts.
 */
typedef struct TGControlOutput
{
    uint32_t StructSize;
    TGThrusterCommand* ThrusterCommands;
    uint64_t ThrusterCommandCount;
    double* JointEffortsNewtonMetersOrNewtons;
    uint64_t JointEffortCount;
    double* ReactionWheelMomentumRatesNewtonMeters;
    uint64_t ReactionWheelMomentumRateCount;
    TGVec3 AdditionalExternalTorqueBodyNewtonMeters;
} TGControlOutput;

/*
 * Structural description of the PHAROS Controller API. The host validates
 * these sizes before it creates or executes a controller, preventing a DLL
 * built from a different contract from being loaded.
 */
typedef struct TGControllerContract
{
    uint32_t StructSize;
    uint32_t ControlInputSize;
    uint32_t ControlOutputSize;
    uint32_t ThrusterCommandSize;
    uint32_t SimulationConfigurationSize;
    uint32_t SpacecraftStateViewSize;
    uint32_t ComponentStateViewSize;
    uint32_t JointStateViewSize;
    uint32_t ThrusterStateViewSize;
    uint32_t ReactionWheelStateViewSize;
    uint32_t CelestialBodyStateViewSize;
} TGControllerContract;

typedef uint32_t (TG_CONTROLLER_CALL* TGDescribeControllerContractFunction)(
    TGControllerContract* Contract);
typedef void* (TG_CONTROLLER_CALL* TGCreateControllerFunction)(void);
typedef void (TG_CONTROLLER_CALL* TGDestroyControllerFunction)(void* Controller);
typedef uint32_t (TG_CONTROLLER_CALL* TGComputeControlFunction)(
    void* Controller,
    const TGControlInput* Input,
    TGControlOutput* Output);
/* Optional export. Return the next hard command-switch time in elapsed
 * simulation seconds, or positive infinity when no later switch is known. */
typedef double (TG_CONTROLLER_CALL*
    TGNextControllerDiscontinuityFunction)(
        void* Controller,
        double CurrentElapsedSimulationTimeSeconds);

#define TG_DESCRIBE_CONTROLLER_CONTRACT_EXPORT "TG_DescribeControllerContract"
#define TG_CREATE_CONTROLLER_EXPORT "TG_CreateController"
#define TG_DESTROY_CONTROLLER_EXPORT "TG_DestroyController"
#define TG_COMPUTE_CONTROL_EXPORT "TG_ComputeControl"
#define TG_NEXT_CONTROLLER_DISCONTINUITY_EXPORT \
    "TG_NextControllerDiscontinuityElapsedTime"

#endif /* TG_CONTROLLER_API_H */
