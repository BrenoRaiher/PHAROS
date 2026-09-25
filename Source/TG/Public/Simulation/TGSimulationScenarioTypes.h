// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Simulation/ComponentTree/TGComponentVisualTypes.h"
#include "TGSimulationScenarioTypes.generated.h"

// ============================================================================
// Scenario and solver
// ============================================================================

UENUM(BlueprintType)
enum class ETGSimulationKind : uint8
{
    Spacecraft6Dof UMETA(DisplayName = "Spacecraft 6-DOF")
};

UENUM(BlueprintType)
enum class ETGSimulationEndMode : uint8
{
    Unspecified UMETA(Hidden),
    FinalUtc UMETA(DisplayName = "Final UTC"),
    Duration UMETA(DisplayName = "Duration")
};

UENUM(BlueprintType)
enum class ETGIntegratorKind : uint8
{
    Unspecified UMETA(Hidden),
    FixedStepRK4 UMETA(DisplayName = "Fixed-Step RK4"),
    AdaptiveDormandPrince54
    UMETA(DisplayName = "Adaptive Dormand-Prince 5(4)")
};

UENUM(BlueprintType)
enum class ETGOutputMode : uint8
{
    Unspecified UMETA(Hidden),
    EveryIntegratorStep UMETA(DisplayName = "Every Integrator Step"),
    FixedInterval UMETA(DisplayName = "Fixed Interval")
};

UENUM(BlueprintType)
enum class ETGMassFlowConvention : uint8
{
    ThrustIncludesExhaustMomentum
    UMETA(DisplayName = "Thrust Includes Exhaust Momentum")
};

USTRUCT(BlueprintType)
struct TG_API FTGScenarioSolverConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
    FString ScenarioName = TEXT("Untitled Scenario");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
    ETGSimulationKind SimulationKind =
        ETGSimulationKind::Spacecraft6Dof;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
    FDateTime StartUtc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
    ETGSimulationEndMode EndMode =
        ETGSimulationEndMode::Duration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
    FDateTime FinalUtc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
    double DurationSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver")
    ETGIntegratorKind IntegratorKind =
        ETGIntegratorKind::FixedStepRK4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver")
    double MaximumIntegratorStepSeconds = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver")
    double InitialIntegratorStepSeconds = 0.1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver")
    double AbsoluteTolerance = 1.0e-9;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver")
    double RelativeTolerance = 1.0e-9;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
    ETGOutputMode OutputMode =
        ETGOutputMode::EveryIntegratorStep;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
    double OutputStepSeconds = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety Limits")
    int32 MaximumIntegrationSteps = 10000000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety Limits")
    int32 MaximumOutputSamples = 1000000;

    /**
     * Maximum real elapsed time allowed for one packaged-backend process.
     * This is frontend execution policy only; it is preserved in .tgscn files
     * but deliberately omitted when ScenarioDocument becomes SimulationRequest.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execution Policy")
    double MaximumWallClockRuntimeSeconds = 300.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable Mass")
    ETGMassFlowConvention MassFlowConvention =
        ETGMassFlowConvention::ThrustIncludesExhaustMomentum;
};

// ============================================================================
// Initial spacecraft state
// ============================================================================

USTRUCT(BlueprintType)
struct TG_API FTGInitialSpacecraftState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Translation")
    FVector PositionMeters = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Translation")
    FVector VelocityMetersPerSecond = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attitude")
    FQuat AttitudeBodyToIcrf = FQuat::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attitude")
    FVector AngularVelocityBodyRadiansPerSecond =
        FVector::ZeroVector;

    /**
     * Frontend authoring preference only. The physical state above remains
     * canonical ICRF/body data regardless of this selected input frame.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Authoring")
    FName AuthoringFrameCatalogKey = NAME_None;
};

// ============================================================================
// Solar-radiation-pressure component authoring
// ============================================================================

/**
 * User-authored optical fractions. Every valid triplet is finite, lies in
 * [0, 1], and sums to one.
 */
USTRUCT(BlueprintType)
struct TG_API FTGSrpOpticalProperties
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optical Properties")
    double AbsorptionFraction = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optical Properties")
    double SpecularReflectionFraction = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optical Properties")
    double DiffuseReflectionFraction = 0.0;
};

UENUM(BlueprintType)
enum class ETGSrpProxyResolutionMode : uint8
{
    Automatic UMETA(DisplayName = "Automatic"),
    CustomTargetTriangleCount
        UMETA(DisplayName = "Custom Target Triangle Count")
};

/** Stable logical surfaces for primitive-region optical overrides. */
UENUM(BlueprintType)
enum class ETGSrpLogicalRegion : uint8
{
    None UMETA(Hidden),
    BoxPositiveX UMETA(DisplayName = "Box +X"),
    BoxNegativeX UMETA(DisplayName = "Box -X"),
    BoxPositiveY UMETA(DisplayName = "Box +Y"),
    BoxNegativeY UMETA(DisplayName = "Box -Y"),
    BoxPositiveZ UMETA(DisplayName = "Box +Z"),
    BoxNegativeZ UMETA(DisplayName = "Box -Z"),
    CylinderSide UMETA(DisplayName = "Cylinder Side"),
    CylinderPositiveCap UMETA(DisplayName = "Cylinder +Z Cap"),
    CylinderNegativeCap UMETA(DisplayName = "Cylinder -Z Cap")
};

USTRUCT(BlueprintType)
struct TG_API FTGSrpLogicalRegionOverride
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
    ETGSrpLogicalRegion Region = ETGSrpLogicalRegion::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
    FTGSrpOpticalProperties OpticalProperties;
};

USTRUCT(BlueprintType)
struct TG_API FTGSrpTriangleOverride
{
    GENERATED_BODY()

    /** Stable index in the currently generated component proxy. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
    int32 ProxyTriangleIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
    FTGSrpOpticalProperties OpticalProperties;
};

/**
 * SRP authoring state owned by one physical component.
 *
 * The visual primitive or STL remains the sole geometry source. This struct
 * stores only proxy-generation choices and optical assignment, never manual
 * vertices, normals, centers, or areas.
 */
USTRUCT(BlueprintType)
struct TG_API FTGComponentSrpConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Proxy")
    bool bIncludedInProxy = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Proxy")
    ETGSrpProxyResolutionMode ProxyResolutionMode =
        ETGSrpProxyResolutionMode::Automatic;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Proxy")
    int32 CustomTargetTriangleCount = 200;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optical Properties")
    bool bUseGlobalFallbackOpticalProperties = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optical Properties")
    bool bApplyOneOpticalConfigurationToEntireComponent = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optical Properties")
    FTGSrpOpticalProperties ComponentOpticalProperties;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overrides")
    TArray<FTGSrpLogicalRegionOverride> LogicalRegionOverrides;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overrides")
    TArray<FTGSrpTriangleOverride> TriangleOverrides;

    /**
     * Converter-supplied fingerprint of the visual geometry, units, and
     * requested resolution used for the cached triangles.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    FString GeneratedGeometrySignature;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    int32 GeneratedTriangleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    bool bProxyGenerationRequired = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    FString LastProxyGenerationMessage;
};

// ============================================================================
// Component tree and articulated joints
// ============================================================================

UENUM(BlueprintType)
enum class ETGJointMotionType : uint8
{
    Rotation UMETA(DisplayName = "Rotation"),
    Translation UMETA(DisplayName = "Translation")
};

USTRUCT(BlueprintType)
struct TG_API FTGSymmetricInertia
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inertia")
    double IxxKilogramMetersSquared = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inertia")
    double IyyKilogramMetersSquared = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inertia")
    double IzzKilogramMetersSquared = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inertia")
    double IxyKilogramMetersSquared = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inertia")
    double IxzKilogramMetersSquared = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inertia")
    double IyzKilogramMetersSquared = 0.0;
};

USTRUCT(BlueprintType)
struct TG_API FTGJointDofConfig
{
    GENERATED_BODY()

    /**
     * Stable editor identity.
     *
     * Existing saves may initially contain an invalid GUID. The component-tree
     * normalization library will assign missing identifiers when loading.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
    FGuid DofId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
    ETGJointMotionType MotionType =
        ETGJointMotionType::Rotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
    FVector Axis = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initial State")
    double InitialCoordinate = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initial State")
    double InitialRate = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    bool bHasMinimumCoordinate = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    double MinimumCoordinate = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    bool bHasMaximumCoordinate = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    double MaximumCoordinate = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    double MaximumAbsoluteRate = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    double MaximumAbsoluteEffort = 0.0;
};

USTRUCT(BlueprintType)
struct TG_API FTGComponentConfig
{
    GENERATED_BODY()

    /**
     * Stable editor identity used for selection, tree rows and visual actors.
     *
     * ParentComponentName remains the backend-facing hierarchy reference.
     * The editing library will safely update it whenever a component is
     * renamed.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
    FGuid ComponentId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FString Name;

    /** Individual components may be massless; aggregate spacecraft mass may not. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mass",
        meta = (ClampMin = "0.0"))
    double InitialMassKilograms = 0.0;

    /** Depletion floor. Fixed components use InitialMassKilograms instead. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mass",
        meta = (ClampMin = "0.0"))
    double MinimumMassKilograms = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
    bool bVariableMass = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass Properties")
    FVector LocalCenterOfMassMeters = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass Properties")
    FTGSymmetricInertia CentroidalInertia;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Component")
    FVector OriginInBodyMeters = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Component")
    FQuat ComponentToBodyOrientation = FQuat::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connection")
    FString ParentComponentName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connection")
    FVector ParentAnchorMeters = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connection")
    FVector ChildAnchorMeters = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connection")
    FQuat ChildToParentZeroOrientation = FQuat::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connection")
    TArray<FTGJointDofConfig> DegreesOfFreedom;

    /**
     * Visualization-only data used by both the configuration preview and
     * simulation-result visualization actor.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
    FTGComponentVisualConfig Visual;

    /** Frontend SRP proxy and optical authoring state. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment|SRP")
    FTGComponentSrpConfig SolarRadiationPressure;
};

// ============================================================================
// Reusable scalar curves
// ============================================================================

UENUM(BlueprintType)
enum class ETGScalarCurveInterpolation : uint8
{
    Linear UMETA(DisplayName = "Linear"),
    MonotoneCubicPchip UMETA(DisplayName = "Monotone Cubic PCHIP"),
    ModifiedAkima UMETA(DisplayName = "Modified Akima")
};

UENUM(BlueprintType)
enum class ETGScalarCurveExtrapolation : uint8
{
    ClampEndpoint UMETA(DisplayName = "Clamp Endpoint"),
    ExtendEndpointSlope UMETA(DisplayName = "Extend Endpoint Slope"),
    UseDefaultValue UMETA(DisplayName = "Use Default Value")
};

USTRUCT(BlueprintType)
struct TG_API FTGScalarCurveSample
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve")
    double TimeSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve")
    double Value = 0.0;
};

// ============================================================================
// HUD scalar profiles
// ============================================================================

UENUM(BlueprintType)
enum class ETGScalarProfileSource : uint8
{
    Constant UMETA(DisplayName = "Constant"),
    CsvProfile UMETA(DisplayName = "CSV Profile")
};

/*
 * User-facing scalar profile selection.
 *
 * ConstantValue is used only when Source is Constant.
 * CsvFilePath is used only when Source is CsvProfile.
 *
 * The converter will later parse accepted CSV files into the backend's
 * piecewise-linear scalar-curve representation.
 */
USTRUCT(BlueprintType)
struct TG_API FTGScalarProfileConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    ETGScalarProfileSource Source =
        ETGScalarProfileSource::Constant;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    double ConstantValue = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    FString CsvFilePath;
};

USTRUCT(BlueprintType)
struct TG_API FTGScalarCurve
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve")
    double DefaultValue = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve")
    TArray<FTGScalarCurveSample> Samples;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve")
    ETGScalarCurveInterpolation Interpolation =
        ETGScalarCurveInterpolation::Linear;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve")
    ETGScalarCurveExtrapolation Extrapolation =
        ETGScalarCurveExtrapolation::ClampEndpoint;
};

// ============================================================================
// Thrusters and propellant
// ============================================================================

UENUM(BlueprintType)
enum class ETGThrusterMode : uint8
{
    PrescribedProfile UMETA(DisplayName = "Prescribed Profile"),
    Commanded UMETA(DisplayName = "Commanded")
};

UENUM(BlueprintType)
enum class ETGThrusterTimeMode : uint8
{
    AbsoluteUtc UMETA(DisplayName = "Absolute UTC"),
    ElapsedSimulationTime UMETA(DisplayName = "Elapsed Simulation Time")
};

USTRUCT(BlueprintType)
struct TG_API FTGThrusterConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    ETGThrusterMode Mode =
        ETGThrusterMode::PrescribedProfile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounting")
    FString MountComponentName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Propellant")
    FString PropellantComponentName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounting")
    FVector ApplicationPointMeters = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounting")
    FVector Direction = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    ETGThrusterTimeMode IgnitionTimeMode =
        ETGThrusterTimeMode::ElapsedSimulationTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    FDateTime IgnitionUtc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    double IgnitionElapsedSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    bool bNeverShutsDown = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    ETGThrusterTimeMode ShutdownTimeMode =
        ETGThrusterTimeMode::ElapsedSimulationTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    FDateTime ShutdownUtc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    double ShutdownElapsedSeconds = 0.0;

    /*
     * Prescribed-profile mode only.
     *
     * Thrust is either a nonnegative constant or a no-header two-column
     * CSV containing:
     *
     *     time_since_ignition_seconds, thrust_newtons
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Prescribed Profile")
    FTGScalarProfileConfig PrescribedThrust;

    /*
     * Prescribed-profile mode only.
     *
     * Specific impulse is either a positive constant or a no-header
     * two-column CSV containing:
     *
     *     time_since_ignition_seconds, specific_impulse_seconds
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Prescribed Profile")
    FTGScalarProfileConfig PrescribedSpecificImpulse;

    /*
     * Commanded mode only.
     *
     * Actual thrust is:
     *
     *     MaximumThrustNewtons * clamp(controller_throttle, 0, 1)
     *
     * The controller also supplies instantaneous specific impulse.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Commanded",
        meta = (
            EditCondition =
                "Mode == ETGThrusterMode::Commanded",
            EditConditionHides))
    double MaximumThrustNewtons = 0.0;
};

// ============================================================================
// Reaction wheels
// ============================================================================

USTRUCT(BlueprintType)
struct TG_API FTGReactionWheelConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounting")
    FString MountComponentName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounting")
    FVector Axis = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Momentum")
    double InitialMomentumNewtonMeterSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Momentum")
    double MaximumAbsoluteMomentumNewtonMeterSeconds = 0.0;
};

// ============================================================================
// Compiled user C++ controller selection
// ============================================================================

UENUM(BlueprintType)
enum class ETGControlMode : uint8
{
    None UMETA(DisplayName = "None"),
    CompiledUserController UMETA(DisplayName = "Compiled C++ Controller")
};

USTRUCT(BlueprintType)
struct TG_API FTGControlConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
    ETGControlMode Mode = ETGControlMode::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
    FName ControllerId = NAME_None;

    /*
     * Resolved DLL referenced by an imported standalone TGSCN file. Unreal
     * never executes this path directly: the user must first import and trust
     * the DLL through the managed Controller Library, which supplies
     * ControllerId. Keeping the path here prevents the import bridge from
     * silently discarding authored controller information.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
    FString StandaloneControllerDllFilePath;
};

// ============================================================================
// Gravity and celestial bodies
// ============================================================================

/*
 * Global gravity options that are genuinely editable by the user.
 *
 * SPICE attachment and gravity evaluation at every component center of mass
 * are mandatory backend behavior and are therefore not represented here.
 */
USTRUCT(BlueprintType)
struct TG_API FTGGravitySettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity")
    bool bIncludeFirstPostNewtonianCorrection = false;
};

/*
 * Editable scenario values for exactly one entry in the fixed celestial
 * catalog.
 *
 * CatalogKey is the only stored identity. Display name, SPICE target,
 * NAIF ID, system membership and source role come from the authoritative
 * TGCelestialCatalogLibrary.
 *
 * Every fixed catalog source must have one of these rows, including sources
 * whose explicit gravity checkbox is off.
 */
USTRUCT(BlueprintType)
struct TG_API FTGCelestialBodyConfig
{
    GENERATED_BODY()

    /*
     * Stable internal key from FTGCelestialCatalogEntry.
     *
     * Examples:
     *   Sun
     *   EarthMoonBarycenter
     *   Earth
     *   Moon
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FName CatalogKey = NAME_None;

    /*
     * Explicit user selection.
     *
     * An unchecked source can still become active through its automatic
     * activation radius or through barycenter resolution.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity")
    bool bGravityEnabled = false;

    /*
     * An unchecked source automatically activates when the spacecraft
     * center of mass enters this distance from it.
     *
     * Zero disables automatic activation.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity")
    double AutomaticActivationRadiusMeters = 0.0;

    /*
     * Used only by system barycenters.
     *
     * An active barycenter is replaced by every physical member of its fixed
     * catalog system when the spacecraft enters this distance.
     *
     * Zero keeps the barycenter representation at every distance.
     * Non-barycenter rows must retain zero.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barycenter")
    double BarycenterResolutionRadiusMeters = 0.0;

    /*
     * Optional no-header spherical-harmonic gravity-model CSV.
     *
     * The converter later parses:
     *   Row 1: model GM [m^3/s^2], model reference radius [m]
     *   Remaining rows: degree, order, fully normalized Cbar, Sbar
     *
     * The path may remain stored while MaximumHarmonicDegreeUsed is zero,
     * allowing temporary point-mass operation without discarding the file.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics")
    FString HarmonicModelCsvFilePath;

    /*
     * Zero selects SPICE-GM point-mass gravity.
     *
     * A positive value uses and truncates the selected harmonic CSV at this
     * degree. It must not exceed the largest degree available in the file.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics")
    int32 MaximumHarmonicDegreeUsed = 0;
};

// ============================================================================
// Solar radiation pressure
// ============================================================================

USTRUCT(BlueprintType)
struct TG_API FTGOpticalFacetConfig
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    FString Name;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    FGuid ComponentId;

    /** Snapshot for diagnostics only; ComponentId is the persistent identity. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    FString ComponentName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    int32 StableTriangleIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    ETGSrpLogicalRegion LogicalRegion = ETGSrpLogicalRegion::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    FVector Vertex0Meters = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    FVector Vertex1Meters = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    FVector Vertex2Meters = FVector::ZeroVector;

    /** Final converter-resolved fractions sent to TGSimCore. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    double AbsorptionFraction = 1.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    double SpecularReflectionFraction = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    double DiffuseReflectionFraction = 0.0;
};

USTRUCT(BlueprintType)
struct TG_API FTGSolarRadiationPressureConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SRP")
    bool bEnabled = false;

    /** Fixed backend source; not an ordinary HUD input. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fixed Backend Values")
    FString SunBodyName = TEXT("Sun");

    /** Fixed backend pressure; not an ordinary HUD input. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fixed Backend Values")
    double PressureAtOneAstronomicalUnitPascals = 4.5391e-6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse")
    bool bComputeEclipse = true;

    /** Explicit subset; empty means every supported physical non-Sun body. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse")
    TArray<FString> OccultingBodyNames;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows")
    bool bComputeComponentShadows = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optical Properties")
    FTGSrpOpticalProperties GlobalFallbackOpticalProperties;

    /** Converter output. The HUD never authors this array directly. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Proxy")
    TArray<FTGOpticalFacetConfig> OpticalFacets;
};

// ============================================================================
// Atmosphere
// ============================================================================

UENUM(BlueprintType)
enum class ETGAtmosphereModel : uint8
{
    UploadedProfile
        UMETA(DisplayName = "Uploaded Profile"),

    CubicHarrisPriesterEarth
        UMETA(DisplayName = "Cubic Harris-Priester (Earth only)")
};

USTRUCT(BlueprintType)
struct TG_API FTGAtmosphereConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
    FString CentralBodyName = TEXT("Earth");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
    ETGAtmosphereModel Model =
        ETGAtmosphereModel::UploadedProfile;

    // General uploaded atmosphere profile:
    // altitude_m,density_kgpm3,temperature_k,
    // mean_particle_mass_kg,effective_collision_cross_section_m2
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Uploaded Profile")
    FString GeneralProfileCsvPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubic Harris-Priester")
    double CenteredAverageF107SolarFluxUnits = 150.0;

    // Exactly 50 no-header rows with 8 cubic envelope coefficients.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubic Harris-Priester")
    FString ChpCoefficientCsvPath;

    // altitude_m,temperature_k,mean_particle_mass_kg,
    // effective_collision_cross_section_m2
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubic Harris-Priester")
    FString ChpMolecularProfileCsvPath;
};

// ============================================================================
// Aerodynamics
// ============================================================================

UENUM(BlueprintType)
enum class ETGAerodynamicDatabaseInterpolation : uint8
{
    InverseDistance UMETA(DisplayName = "Inverse Distance"),
    NearestRow UMETA(DisplayName = "Nearest Row")
};

UENUM(BlueprintType)
enum class ETGAerodynamicDatabaseExtrapolation : uint8
{
    ConstantDragFallback
        UMETA(DisplayName = "Constant-Drag Fallback"),

    NearestRow
        UMETA(DisplayName = "Nearest Row")
};

USTRUCT(BlueprintType)
struct TG_API FTGAerodynamicDatabaseRow
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Independent Variables")
    double SpeedRatio = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Independent Variables")
    double KnudsenNumber = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Independent Variables")
    FVector GasFlowDirectionBody = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Independent Variables")
    TArray<double> ArticulationCoordinates;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coefficients")
    FVector BodyForceCoefficients = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coefficients")
    FVector BodyMomentCoefficients = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct TG_API FTGAerodynamicDatabaseConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
    bool bEnabled = false;

    // Selected source path is intentionally retained for scenario persistence
    // and final converter validation.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
    FString CsvFilePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
    ETGAerodynamicDatabaseInterpolation Interpolation =
        ETGAerodynamicDatabaseInterpolation::InverseDistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
    ETGAerodynamicDatabaseExtrapolation Extrapolation =
        ETGAerodynamicDatabaseExtrapolation::ConstantDragFallback;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
    int32 NeighborCount = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
    double InverseDistancePower = 2.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
    bool bUseMaximumNormalizedNeighborDistance = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
    double MaximumNormalizedNeighborDistance = 0.0;

    // Fixed point R, expressed in spacecraft B axes, about which the uploaded
    // aggregate moment coefficients are defined.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
    FVector MomentReferenceCenterBodyMeters = FVector::ZeroVector;

    // Optional parsed cache. The HUD/converter may instead parse CsvFilePath.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
    TArray<FTGAerodynamicDatabaseRow> Rows;
};

USTRUCT(BlueprintType)
struct TG_API FTGAerodynamicsConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reference")
    double ReferenceAreaSquareMeters = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reference")
    double ReferenceLengthMeters = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validity")
    double MinimumDynamicPressurePascals = 1.0e-12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validity")
    double MaximumValidDynamicPressurePascals = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fallback")
    bool bEnableConstantDragFallback = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fallback")
    double FallbackDragCoefficient = 2.2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
    FTGAerodynamicDatabaseConfig Database;
};

// ============================================================================
// Complete Blueprint-facing scenario
// ============================================================================

USTRUCT(BlueprintType)
struct TG_API FTGSimulationScenario
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
    FTGScenarioSolverConfig ScenarioAndSolver;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initial State")
    FTGInitialSpacecraftState InitialState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    TArray<FTGComponentConfig> Components;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actuators")
    TArray<FTGThrusterConfig> Thrusters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actuators")
    TArray<FTGReactionWheelConfig> ReactionWheels;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
    FTGControlConfig Control;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity")
    FTGGravitySettings GravitySettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity")
    TArray<FTGCelestialBodyConfig> CelestialBodies;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
    FTGSolarRadiationPressureConfig SolarRadiationPressure;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
    FTGAtmosphereConfig Atmosphere;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    FTGAerodynamicsConfig Aerodynamics;

};
