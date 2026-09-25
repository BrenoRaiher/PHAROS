// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGSimulationPlaybackActor.generated.h"

class ATGSpacecraftVisualActor;
class ATGSpacecraftOrbitCameraActor;
class ADirectionalLight;
class APawn;
class ULineBatchComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UGameViewportClient;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTGVisualizationHudWidget;
class SWidget;

/** Maps one CSV body key, such as "earth", to its visual actor class. */
USTRUCT(BlueprintType)
struct TG_API FTGCelestialPlaybackBinding
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Body")
    FString BodyColumnKey;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Body")
    TSoftClassPtr<AActor> ActorClass;

    /** Zero keeps the body visible at every distance from the reference origin. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Body", meta = (ClampMin = "0.0"))
    double MaximumVisibleDistanceMeters = 0.0;
};

/** Physical quantity represented by one selectable spacecraft-rooted arrow. */
UENUM(BlueprintType)
enum class ETGVisualizationArrowQuantity : uint8
{
    BodyDirection,
    Force,
    Torque,
    LinearMomentum,
    AngularMomentum,
    AngularVelocity,
    LinearVelocity
};

/** One user-selectable direction indicator rooted at the spacecraft origin. */
USTRUCT(BlueprintType)
struct TG_API FTGVisualizationArrowInfo
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    FName ArrowId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    FString DisplayName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    FLinearColor Color = FLinearColor::White;

    /** Exposure-safe color used by the rendered arrowhead and body reticle. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    FLinearColor DisplayColor = FLinearColor::White;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    bool bVisible = false;

    /** Determines frame handling, magnitude units, and reticle behavior. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    ETGVisualizationArrowQuantity Quantity =
        ETGVisualizationArrowQuantity::BodyDirection;

    /** Current magnitude in the SI unit named by MagnitudeUnit. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    double CurrentMagnitude = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    FString MagnitudeUnit;

    /**
     * Magnification relative to a linear display where the largest current
     * force receives the configured maximum arrow length.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    double RelativeVisualAmplification = 1.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    bool bVisuallyAmplified = false;

    /** Current body direction in Unreal world axes. Body arrows only. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    FVector CurrentBodyDirectionUnreal = FVector::ZeroVector;

    /** Current target center in the spacecraft-centered Unreal world. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    FVector CurrentBodyWorldLocation = FVector::ZeroVector;

    /** Physical apparent angular radius before the HUD's minimum size. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    double CurrentBodyAngularRadiusRadians = 0.0;

    /** True when the selected body has a valid direction at this frame. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    bool bBodyTargetAvailable = false;
};

/** How one live telemetry value should be presented by the HUD. */
UENUM(BlueprintType)
enum class ETGVisualizationTelemetryType : uint8
{
    Scalar,
    Vector3,
    Vector4,
    Matrix3,
    Text,
    Status
};

/** User-selectable color for the trajectory drawn in the 3D viewport. */
UENUM(BlueprintType)
enum class ETGTrajectoryColorMode : uint8
{
    White,
    Red
};

/** Reference translation used to draw the trajectory in the 3D viewport. */
UENUM(BlueprintType)
enum class ETGTrajectoryFrameMode : uint8
{
    Hidden,
    Icrf,
    ClosestBody
};

/**
 * Structured live telemetry sampled at the current playback time.
 *
 * Keeping the numeric value separate from its label and unit lets the HUD
 * format scalars compactly and render vector components in dedicated X/Y/Z
 * cells instead of parsing comma-separated display strings.
 */
USTRUCT(BlueprintType)
struct TG_API FTGVisualizationTelemetryItem
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
    FName ItemId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
    FString Group;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
    FString DisplayName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
    FString Unit;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
    ETGVisualizationTelemetryType Type =
        ETGVisualizationTelemetryType::Scalar;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
    double ScalarValue = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
    FVector VectorValue = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
    FVector4 Vector4Value = FVector4(0.0, 0.0, 0.0, 0.0);

    /** Row-major values for a 3 x 3 telemetry matrix. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
    TArray<double> MatrixValues;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
    FString TextValue;

    /** Optional compact explanatory text displayed beside the item label. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
    FString Note;
};

/** Absolute ICRF history consumed by the native solar-system overview. */
struct TG_API FTGSolarSystemOverviewPath
{
    FString Key;
    FString DisplayName;
    FString SpiceTargetName;
    FLinearColor Color = FLinearColor::White;
    double ReferenceRadiusMeters = 0.0;
    bool bSpacecraft = false;
    TArray<double> EphemerisTimes;
    TArray<FVector> PositionsIcrfMeters;
};

/** Body-centered trajectory data consumed by the compact local-orbit graph. */
struct TG_API FTGClosestBodyOverviewData
{
    FString BodyKey;
    FString BodyDisplayName;
    FLinearColor BodyColor = FLinearColor::White;
    double BodyReferenceRadiusMeters = 0.0;
    FVector CurrentSpacecraftPositionRelativeMeters = FVector::ZeroVector;
    TArray<FVector> SpacecraftPathRelativeMeters;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FTGPlaybackTimeChanged,
    double, ElapsedSimulationSeconds,
    double, EphemerisTimeTdbSeconds,
    double, NormalizedTime);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
    FTGPlaybackFinished);

/**
 * Loads a TGSimCore solution CSV and turns it into an interpolated Unreal view.
 *
 * The actor owns presentation only. It never recomputes dynamics: spacecraft,
 * component and celestial poses all come from named backend result columns.
 * A Level Blueprint only needs to create or reference this actor and connect
 * its Play/Pause/Seek functions to the time HUD.
 */
UCLASS(BlueprintType, Blueprintable)
class TG_API ATGSimulationPlaybackActor : public AActor
{
    GENERATED_BODY()

public:
    ATGSimulationPlaybackActor();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

    /** Uses the current scenario draft already owned by the HUD. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback")
    bool InitializeFromCurrentScenario(
        const FString& ResultCsvFilePath,
        FText& OutErrorText);

    /** Loads the exact .tgscn/CSV pair handed off by the run subsystem. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback")
    bool InitializeFromLatestCompletedRun(FText& OutErrorText);

    /** Imports a portable scenario, then loads its matching result CSV. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback")
    bool InitializeFromFiles(
        const FString& ScenarioFilePath,
        const FString& ResultCsvFilePath,
        FText& OutErrorText);

    /** Initializes directly from an already available frontend scenario. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback")
    bool InitializeFromScenario(
        const FTGSimulationScenario& Scenario,
        const FString& ResultCsvFilePath,
        FText& OutErrorText);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback")
    void Play();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback")
    void Pause();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback")
    void TogglePlayPause();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback")
    void SetPlaybackRate(double SimulationSecondsPerRealSecond);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback")
    bool SeekToElapsedSeconds(
        double RequestedElapsedSeconds,
        FText& OutErrorText);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback")
    bool SeekToNormalizedTime(
        double RequestedNormalizedTime,
        FText& OutErrorText);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback")
    bool StepBySimulationSeconds(
        double DeltaSimulationSeconds,
        FText& OutErrorText);

    /** Reapplies the current frame after scale/reference/body mappings change. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback")
    bool RefreshCurrentFrame(FText& OutErrorText);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback")
    void ClearPlayback();

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback")
    bool IsResultLoaded() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback")
    bool IsPlaying() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback")
    double GetStartEphemerisTimeTdbSeconds() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback")
    double GetEndEphemerisTimeTdbSeconds() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback")
    double GetDurationSeconds() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback")
    double GetElapsedSeconds() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback")
    double GetNormalizedTime() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback")
    double GetCurrentEphemerisTimeTdbSeconds() const;

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback")
    bool GetCurrentUtc(FString& OutUtc, FText& OutErrorText) const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback")
    TArray<FString> GetLoadedCelestialBodyKeys() const;

    /** Returns all body-direction and force arrows available to the HUD. */
    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback|Overlays")
    TArray<FTGVisualizationArrowInfo> GetVisualizationArrows() const;

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback|Overlays")
    bool SetVisualizationArrowVisible(FName ArrowId, bool bVisible);

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback|Overlays")
    bool IsVisualizationArrowVisible(FName ArrowId) const;

    bool GetVisualizationArrowInfo(
        FName ArrowId,
        FTGVisualizationArrowInfo& OutInfo) const;

    /** Samples typed telemetry for the current interpolated playback frame. */
    void GetCurrentTelemetryItems(
        TArray<FTGVisualizationTelemetryItem>& OutItems) const;

    /** Legacy text view retained for existing Blueprint callers. */
    void GetCurrentTelemetryLines(TArray<FString>& OutLines) const;

    /** Exact numeric headers available in the loaded result CSV. */
    void GetResultNumericColumnNames(
        TArray<FString>& OutColumnNames) const;

    /**
     * Returns raw CSV-unit samples for three user-selected columns over an
     * independent normalized time interval. An empty Z name requests 2D data.
     */
    bool BuildResultNumericPlotSamples(
        const FString& XColumnName,
        const FString& YColumnName,
        const FString& ZColumnName,
        double StartNormalized,
        double EndNormalized,
        TArray<FVector>& OutSamples,
        FVector& OutMinimum,
        FVector& OutMaximum) const;

    /** Interpolates the selected raw CSV-unit values at the playback time. */
    bool EvaluateResultNumericPlotPoint(
        const FString& XColumnName,
        const FString& YColumnName,
        const FString& ZColumnName,
        FVector& OutPoint) const;

    /** Data remains absolute ICRF; only the overview widget projects it. */
    const TArray<FTGSolarSystemOverviewPath>&
        GetSolarSystemOverviewPaths() const;

    /** Interpolates one cached ICRF overview path at the current ET. */
    bool EvaluateOverviewPathAtCurrentTime(
        const FTGSolarSystemOverviewPath& Path,
        FVector& OutPositionIcrfMeters) const;

    /**
     * Builds the currently selected trajectory interval relative to the
     * closest physical body. The returned positions remain in ICRF-oriented
     * axes; only the body's translation is removed.
     */
    bool BuildClosestBodyOverviewData(
        FTGClosestBodyOverviewData& OutData) const;

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback|HUD")
    void ToggleVisualizationMenu();

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback|HUD")
    bool IsVisualizationMenuOpen() const;

    /** Prevents the orbit camera from consuming pointer input over HUD tools. */
    void SetVisualizationUiCapturesCameraInput(bool bCapturesInput);
    bool IsVisualizationUiCapturingCameraInput() const;

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback|Options")
    void SetBodiesEmissive(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback|Options")
    bool AreBodiesEmissive() const;

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback|Options")
    void SetSpacecraftEmissive(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback|Options")
    bool IsSpacecraftEmissive() const;

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback|Options")
    void SetVisualizationCameraFovDegrees(double FieldOfViewDegrees);

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback|Options")
    double GetVisualizationCameraFovDegrees() const;

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback|Options")
    void SetVisualizationOrbitSensitivity(
        double SensitivityDegreesPerPixel);

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback|Options")
    double GetVisualizationOrbitSensitivity() const;

    /** Selects the absolute CSV interval drawn by the local trajectory. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback|Options")
    void SetLocalTrajectoryDisplayRangeNormalized(
        double StartNormalized,
        double EndNormalized);

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback|Options")
    double GetLocalTrajectoryDisplayStartNormalized() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback|Options")
    double GetLocalTrajectoryDisplayEndNormalized() const;

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback|Options")
    void SetLocalTrajectoryColorMode(ETGTrajectoryColorMode Mode);

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback|Options")
    ETGTrajectoryColorMode GetLocalTrajectoryColorMode() const;

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Simulation Playback|Options")
    void SetLocalTrajectoryFrameMode(ETGTrajectoryFrameMode Mode);

    UFUNCTION(BlueprintPure, Category = "PHAROS|Simulation Playback|Options")
    ETGTrajectoryFrameMode GetLocalTrajectoryFrameMode() const;

    UPROPERTY(BlueprintAssignable, Category = "PHAROS|Simulation Playback")
    FTGPlaybackTimeChanged OnPlaybackTimeChanged;

    UPROPERTY(BlueprintAssignable, Category = "PHAROS|Simulation Playback")
    FTGPlaybackFinished OnPlaybackFinished;

    /** Optional automatic startup for a playback actor placed in a level. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Startup")
    bool bInitializeOnBeginPlay = false;

    /** Preferred packaged-app startup; harmless when no completed run exists. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Startup")
    bool bInitializeFromLatestCompletedRunOnBeginPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Startup", meta = (FilePathFilter = "tgscn"))
    FFilePath StartupScenarioFile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Startup", meta = (FilePathFilter = "csv"))
    FFilePath StartupResultCsvFile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Time")
    bool bAutoPlay = true;

    /** Simulated seconds advanced per real second; playback starts at real time. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Time", meta = (ClampMin = "0.0"))
    double PlaybackRateSimulationSecondsPerRealSecond = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Time")
    bool bLoop = true;

    /** Unreal uses centimeters, so real scale is exactly 100 UE units per meter. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Scale", meta = (ClampMin = "0.000000000001"))
    double WorldUnitsPerMeter = 100.0;

    /** Keep at 1 for real scale. Values above 1 visually exaggerate the spacecraft. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Scale", meta = (ClampMin = "0.0"))
    double SpacecraftVisualMagnification = 1.0;

    /** Keeps body radii on the same uniform scale as their relative positions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Scale")
    bool bScaleCelestialBodiesFromResultRadii = true;

    /** Keep at 1 for real scale. Values above 1 visually exaggerate celestial bodies. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Scale", meta = (ClampMin = "0.0"))
    double CelestialBodyVisualMagnification = 1.0;

    /** Bodies below this projected diameter are omitted for the whole run. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Celestial Visibility", meta = (ClampMin = "0.0"))
    double MinimumCelestialApparentDiameterPixels = 0.75;

    /** Conservative viewport used before a runtime camera exists. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Celestial Visibility", meta = (ClampMin = "1.0"))
    double CelestialVisibilityReferenceViewportHeightPixels = 1080.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Celestial Visibility", meta = (ClampMin = "1.0", ClampMax = "170.0"))
    double CelestialVisibilityReferenceVerticalFovDegrees = 60.0;

    /**
     * Optional movable directional light used as sunlight. During playback its
     * rays are pointed from the simulated Sun toward the spacecraft origin.
     */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Lighting")
    TObjectPtr<ADirectionalLight> SunDirectionalLight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Lighting")
    bool bOrientSunDirectionalLight = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Actors")
    TSubclassOf<ATGSpacecraftVisualActor> SpacecraftActorClass;

    /** Only bodies with a binding are spawned; the defaults cover existing assets. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Actors")
    TArray<FTGCelestialPlaybackBinding> CelestialBodyBindings;

    /** Unbound physical bodies use an engine sphere instead of disappearing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Actors")
    bool bUseGenericCelestialFallback = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Camera")
    bool bCreateSpacecraftOrbitCamera = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Camera", meta = (ClampMin = "1.0"))
    double InitialCameraDistanceCentimeters = 600.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Trajectory")
    bool bShowLocalTrajectory = true;

    /** Half the number of time intervals sampled across the selected CSV range. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Trajectory", meta = (ClampMin = "4", ClampMax = "256"))
    int32 LocalTrajectorySampleCountPerSide = 32;

    /** Additional Hermite samples drawn inside every covered CSV interval. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Trajectory", meta = (ClampMin = "1", ClampMax = "16"))
    int32 LocalTrajectorySubdivisionsPerCsvInterval = 4;

    /** Hard cap on trajectory points submitted to the renderer. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Trajectory", meta = (ClampMin = "16", ClampMax = "16384"))
    int32 LocalTrajectoryMaximumSampleCount = 4096;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Trajectory")
    FLinearColor LocalTrajectoryColor = FLinearColor::Red;

    /** Constant trajectory width in viewport pixels, independent of zoom. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Trajectory", meta = (ClampMin = "0.1"))
    double TrajectoryLineThickness = 0.5;

    /** Fallback length when no orbit camera exists; otherwise length is 7/12 of camera distance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Overlays", meta = (ClampMin = "1.0"))
    double VectorArrowLengthCentimeters = 350.0;

    /** Small forces never render shorter than this fraction of max length. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Overlays", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    double MinimumForceArrowLengthFraction = 0.75;

    /** Values below 1 compress force-length differences; 0.25 is a fourth root. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|Overlays", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    double ForceArrowCompressionExponent = 0.25;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|HUD")
    bool bCreateNativeVisualizationHud = true;

    /** Designer-authored HUD whose parent supplies the native behavior. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHAROS|Simulation Playback|HUD")
    TSoftClassPtr<UTGVisualizationHudWidget> VisualizationHudClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHAROS|Simulation Playback|Actors")
    TObjectPtr<ATGSpacecraftVisualActor> SpacecraftActor;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHAROS|Simulation Playback")
    TObjectPtr<USceneComponent> PlaybackRoot;

private:
    struct FColumnVector
    {
        int32 X = INDEX_NONE;
        int32 Y = INDEX_NONE;
        int32 Z = INDEX_NONE;

        bool IsComplete() const;
    };

    struct FColumnRotation
    {
        int32 Values[9] = {
            INDEX_NONE, INDEX_NONE, INDEX_NONE,
            INDEX_NONE, INDEX_NONE, INDEX_NONE,
            INDEX_NONE, INDEX_NONE, INDEX_NONE};

        bool IsComplete() const;
        bool IsEmpty() const;
    };

    struct FComponentTrack
    {
        FString Key;
        FColumnVector Origin;
        FColumnRotation Rotation;
    };

    struct FForceTrack
    {
        FString Key;
        FString DisplayName;
        FColumnVector ForceIcrf;
    };

    struct FPhysicalVectorTrack
    {
        FString Key;
        FName ArrowId;
        FString DisplayName;
        FString Unit;
        FColumnVector Components;
        ETGVisualizationArrowQuantity Quantity =
            ETGVisualizationArrowQuantity::Force;
        /** True when recorded components must be rotated by body-to-ICRF. */
        bool bComponentsInBodyFrame = false;
    };

    struct FCelestialTrack
    {
        FString Key;
        FColumnVector Position;
        FColumnVector Velocity;
        int32 ReferenceRadius = INDEX_NONE;
        FColumnRotation Rotation;
    };

    UPROPERTY(Transient)
    FTGSimulationScenario ScenarioSnapshot;

    UPROPERTY(Transient)
    TMap<FString, TObjectPtr<AActor>> SpawnedCelestialActors;

    UPROPERTY(Transient)
    TObjectPtr<ATGSpacecraftOrbitCameraActor> OrbitCameraActor;

    /**
     * Unreal may automatically possess a visible spherical DefaultPawn even
     * though playback uses its own camera actor. Keep it only long enough to
     * restore its prior state if the visualization presentation is removed.
     */
    UPROPERTY(Transient)
    TObjectPtr<APawn> SuppressedDefaultPawn;

    bool bSuppressedDefaultPawnWasHidden = false;
    bool bSuppressedDefaultPawnHadCollision = false;

    UPROPERTY(Transient)
    TObjectPtr<UTGVisualizationHudWidget> VisualizationHudWidget;

    /** High-resolution labels rendered below the main visualization HUD. */
    TSharedPtr<SWidget> BodyReticleLabelOverlay;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> GenericCelestialSphereMesh;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> GenericCelestialMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> VectorArrowCylinderMesh;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> VectorArrowConeMesh;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> VectorArrowMaterial;

    /**
     * Runtime-only copy of the configured Sun light. It affects lighting
     * channel 1 exclusively, so only the spacecraft sees it. Its intensity is
     * the user-configured Sun intensity multiplied by the apparent visible
     * fraction of the solar disk at the current interpolated playback frame.
     */
    UPROPERTY(Transient)
    TObjectPtr<ADirectionalLight> SpacecraftSunDirectionalLight;

    /** Camera-facing visibility fills controlled by the Options menu. */
    UPROPERTY(Transient)
    TObjectPtr<ADirectionalLight> BodiesEmissiveDirectionalLight;

    UPROPERTY(Transient)
    TObjectPtr<ADirectionalLight> SpacecraftEmissiveDirectionalLight;

    struct FBodyEmissiveMaterialOverride
    {
        TWeakObjectPtr<UMeshComponent> MeshComponent;
        TWeakObjectPtr<UMaterialInterface> OriginalMaterial;
        int32 MaterialIndex = INDEX_NONE;
    };

    TArray<FBodyEmissiveMaterialOverride>
        BodyEmissiveMaterialOverrides;

    /** Keeps active material instances reachable by Unreal's garbage collector. */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>>
        BodyEmissiveDynamicMaterials;

    TArray<FString> ColumnNames;
    /** Number of columns authored by the backend result CSV. */
    int32 ResultCsvColumnCount = 0;
    TArray<TArray<double>> NumericRows;
    TMap<FString, int32> ColumnIndices;
    TArray<FComponentTrack> ComponentTracks;
    TArray<FCelestialTrack> CelestialTracks;
    TArray<FForceTrack> ForceTracks;
    TArray<FPhysicalVectorTrack> PhysicalVectorTracks;
    TArray<FTGVisualizationArrowInfo> VisualizationArrowInfos;
    TArray<FTGSolarSystemOverviewPath> SolarSystemOverviewPaths;
    /** Physical catalog bodies available to the VECTORS panel by key. */
    TMap<FString, FString> VectorBodySpiceTargets;
    TMap<FString, double> VectorBodyReferenceRadiiMeters;

    FColumnVector SpacecraftPosition;
    FColumnVector SpacecraftVelocity;
    FColumnVector SpacecraftAngularVelocityBody;
    FColumnVector SpacecraftCenterOfMassBody;
    int32 SpacecraftQuaternionW = INDEX_NONE;
    int32 SpacecraftQuaternionX = INDEX_NONE;
    int32 SpacecraftQuaternionY = INDEX_NONE;
    int32 SpacecraftQuaternionZ = INDEX_NONE;
    int32 VisibleSunFractionColumn = INDEX_NONE;
    int32 MassColumn = INDEX_NONE;
    int32 MassRateColumn = INDEX_NONE;
    int32 TimeColumn = INDEX_NONE;

    double CurrentElapsedSeconds = 0.0;
    bool bResultLoaded = false;
    bool bPlaybackRunning = false;
    bool bVisualizationUiCapturesCameraInput = false;
    bool bBodiesEmissive = false;
    bool bSpacecraftEmissive = false;
    bool bHasTranslucentSpacecraftSurface = false;
    double VisualizationCameraFovDegrees = 55.0;
    double VisualizationOrbitSensitivityDegreesPerPixel = 1.0;
    double LocalTrajectoryDisplayStartNormalized = 0.0;
    double LocalTrajectoryDisplayEndNormalized = 1.0;
    ETGTrajectoryColorMode LocalTrajectoryColorMode =
        ETGTrajectoryColorMode::Red;
    ETGTrajectoryFrameMode LocalTrajectoryFrameMode =
        ETGTrajectoryFrameMode::Icrf;

    TWeakObjectPtr<UGameViewportClient>
        VisualizationWarmupViewport;
    TSharedPtr<SWidget> VisualizationWarmupOverlay;
    TArray<TWeakObjectPtr<UMeshComponent>>
        VisualizationWarmupForcedMeshes;
    int32 VisualizationWarmupFramesRemaining = 0;
    double VisualizationWarmupEarliestRemovalSeconds = 0.0;

    UPROPERTY(VisibleAnywhere, Category = "PHAROS|Simulation Playback|Trajectory")
    TObjectPtr<ULineBatchComponent> TrajectoryLineBatch;

    UPROPERTY(VisibleAnywhere, Category = "PHAROS|Simulation Playback|Overlays")
    TObjectPtr<ULineBatchComponent> BodyReticleLineBatch;

    struct FVisualizationArrowMesh
    {
        TWeakObjectPtr<UStaticMeshComponent> Shaft;
        TWeakObjectPtr<UStaticMeshComponent> Head;
    };

    /** Runtime cylinder-and-cone geometry, created only for selected arrows. */
    TMap<FName, FVisualizationArrowMesh> VisualizationArrowMeshes;

    bool LoadResultCsv(
        const FString& ResultCsvFilePath,
        FText& OutErrorText);

    /**
     * Adds in-memory SPICE tracks for visible catalog bodies omitted from the
     * backend CSV because they were not required by the force configuration.
     * These columns are playback-only and are never written to the result.
     */
    void AppendVisibleCatalogCelestialTracks();

    /**
     * Completes initial texture streaming while the map loading screen still
     * owns the viewport, preventing newly spawned visuals from flashing with
     * placeholder materials on their first presented frame.
     */
    void WarmInitialVisualizationResources();
    void ShowVisualizationWarmupOverlay();
    void ScheduleVisualizationReveal();
    void UpdateVisualizationWarmupOverlay();
    void HideVisualizationWarmupOverlay();
    void ReleaseVisualizationWarmupTextureResidency();

    bool BuildTrackSchema(FText& OutErrorText);
    bool BuildVisualActors(FText& OutErrorText);
    void SpawnCelestialActors();
    void DestroySpawnedActors();
    void BuildOverviewPaths();
    void BuildVisualizationArrows();
    void ClearVisualizationArrows();
    FVisualizationArrowMesh* FindOrCreateVisualizationArrowMesh(
        FName ArrowId,
        const FLinearColor& Color);
    void BuildPresentation();
    void DestroyPresentation();
    void CreateVisualizationOptionLights();
    void UpdateVisualizationOptionLights();
    void UpdateBodyEmissiveMaterials();
    void RestoreBodyEmissiveMaterials();
    void UpdateVisualizationOverlays(
        int32 LowerIndex,
        int32 UpperIndex,
        double Alpha,
        const FVector& SpacecraftPositionIcrf);
    void UpdateLocalTrajectory();
    bool ApplyCurrentFrame(FText& OutErrorText);

    bool ShouldEverSpawnCelestialTrack(
        const FCelestialTrack& Track,
        const FTGCelestialPlaybackBinding* Binding) const;

    bool IsCelestialTrackVisibleAtFrame(
        const FCelestialTrack& Track,
        const FTGCelestialPlaybackBinding* Binding,
        const FVector& SpacecraftPositionIcrf,
        int32 LowerIndex,
        int32 UpperIndex,
        double Alpha) const;

    AActor* SpawnCelestialActor(
        const FCelestialTrack& Track,
        const FTGCelestialPlaybackBinding* Binding);

    const FCelestialTrack* FindClosestPhysicalBodyTrack(
        const FVector& SpacecraftPositionIcrf,
        int32 LowerIndex,
        int32 UpperIndex,
        double Alpha) const;

    /**
     * Samples the spacecraft path in ICRF-oriented axes. When a reference
     * body is supplied, its simultaneous translation is removed; null
     * preserves the absolute inertial path.
     */
    bool BuildSpacecraftTrajectorySamples(
        const FCelestialTrack* TranslationReferenceBody,
        double StartTime,
        double EndTime,
        int32 MaximumSampleCount,
        bool bIncludeCurrentTime,
        TArray<FVector>& OutPositionsIcrfMeters,
        TArray<double>* OutSampleTimes = nullptr) const;

    bool FindInterpolationRows(
        double EphemerisTime,
        int32& OutLowerIndex,
        int32& OutUpperIndex,
        double& OutAlpha) const;

    FVector InterpolateVector(
        const FColumnVector& Columns,
        int32 LowerIndex,
        int32 UpperIndex,
        double Alpha) const;

    FQuat InterpolateQuaternion(
        int32 WColumn,
        int32 XColumn,
        int32 YColumn,
        int32 ZColumn,
        int32 LowerIndex,
        int32 UpperIndex,
        double Alpha) const;

    FQuat InterpolateRotationMatrix(
        const FColumnRotation& Columns,
        int32 LowerIndex,
        int32 UpperIndex,
        double Alpha) const;

    double InterpolateColumn(
        int32 Column,
        int32 LowerIndex,
        int32 UpperIndex,
        double Alpha,
        double Fallback = 0.0) const;

    bool EvaluateVectorAtEphemerisTime(
        const FColumnVector& Columns,
        double EphemerisTime,
        FVector& OutValue) const;

    /**
     * Reconstructs a smooth position between CSV rows from the recorded
     * endpoint positions and velocities using cubic Hermite interpolation.
     */
    bool EvaluateHermitePositionAtEphemerisTime(
        const FColumnVector& PositionColumns,
        const FColumnVector& VelocityColumns,
        double EphemerisTime,
        FVector& OutPosition) const;

    /**
     * Re-evaluates celestial apparent-disk overlap from interpolated result
     * positions. This avoids stretching a seconds-long penumbra across a
     * coarse output interval such as the mock scenario's 180-second spacing.
     */
    bool TryComputeVisualSunFraction(
        const FCelestialTrack& SunTrack,
        const FVector& SpacecraftPositionIcrf,
        int32 LowerIndex,
        int32 UpperIndex,
        double Alpha,
        double& OutVisibleSunFraction) const;

    const FTGCelestialPlaybackBinding* FindBodyBinding(
        const FString& Key) const;

    void ApplyCelestialBodyRadius(
        AActor& BodyActor,
        const FCelestialTrack& Track,
        int32 LowerIndex,
        int32 UpperIndex,
        double Alpha,
        const FVector& TargetCenterWorld) const;

    static FVector ConvertIcrfVectorToUnreal(const FVector& VectorIcrf);
    static FLinearColor ResolveBodyColor(const FString& Key);
    static FLinearColor ResolveForceColor(const FString& Key);
    static FLinearColor ResolveVectorColor(
        ETGVisualizationArrowQuantity Quantity,
        const FString& Key);

    static FString NormalizeColumnKey(const FString& Value);
    static FString ResolveInputPath(const FString& Value);
    static bool ParseCsvLine(const FString& Line, TArray<FString>& OutFields);
    static FQuat QuaternionFromMatrixValues(
        const TArray<double>& Row,
        const FColumnRotation& Columns);
};
