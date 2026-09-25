# FTGSimulationScenario - Stored Variables and HUD Specification Mapping

Source: `Source/TG/Public/Simulation/TGSimulationScenarioTypes.h`

Portable counterpart: `tgsim::scenario::ScenarioDocument` in
`Source/TGSimCore/include/TGSim/Scenario/ScenarioDocument.h`. The exhaustive
two-way mapping is implemented by `TGScenarioDocumentAdapter`; its serialized
form is the current `.tgscn` language documented in `Docs/Reference/TGSCN.md`.
All **Additional** fields round-trip through `.tgscn` but are intentionally
discarded when `ScenarioCompiler` creates `SimulationRequest`.

Status meanings:

- **Exact** - matches [ScenarioInputs.md](../Reference/ScenarioInputs.md).
- **Additional** - frontend identity, visualization, or cache data; not a requested backend input.
- **Derived/output** - produced or resolved by the converter; not authored by the HUD.
- **Fixed** - stored, but not editable by the user.

## FTGSimulationScenario

| Type | Variable | Status |
|---|---|---|
| `FTGScenarioSolverConfig` | `ScenarioAndSolver` | Exact |
| `FTGInitialSpacecraftState` | `InitialState` | Exact |
| `TArray<FTGComponentConfig>` | `Components` | Exact |
| `TArray<FTGThrusterConfig>` | `Thrusters` | Exact |
| `TArray<FTGReactionWheelConfig>` | `ReactionWheels` | Exact |
| `FTGControlConfig` | `Control` | Exact |
| `FTGGravitySettings` | `GravitySettings` | Exact |
| `TArray<FTGCelestialBodyConfig>` | `CelestialBodies` | Exact |
| `FTGSolarRadiationPressureConfig` | `SolarRadiationPressure` | Exact; see fixed and derived child fields in the SRP section |
| `FTGAtmosphereConfig` | `Atmosphere` | Exact |
| `FTGAerodynamicsConfig` | `Aerodynamics` | Exact |

## FTGScenarioSolverConfig

| Type | Variable | Status |
|---|---|---|
| `FString` | `ScenarioName` | Exact |
| `ETGSimulationKind` | `SimulationKind` | Exact |
| `FDateTime` | `StartUtc` | Exact |
| `ETGSimulationEndMode` | `EndMode` | Additional helper for the requested final-UTC-or-duration choice |
| `FDateTime` | `FinalUtc` | Exact |
| `double` | `DurationSeconds` | Exact |
| `ETGIntegratorKind` | `IntegratorKind` | Exact |
| `double` | `MaximumIntegratorStepSeconds` | Exact |
| `double` | `InitialIntegratorStepSeconds` | Exact |
| `double` | `AbsoluteTolerance` | Exact |
| `double` | `RelativeTolerance` | Exact |
| `ETGOutputMode` | `OutputMode` | Exact |
| `double` | `OutputStepSeconds` | Exact |
| `int32` | `MaximumIntegrationSteps` | Exact |
| `int32` | `MaximumOutputSamples` | Exact |
| `double` | `MaximumWallClockRuntimeSeconds` | Additional execution policy; preserved in `.tgscn`, consumed by Unreal's run coordinator, and omitted from `SimulationRequest` |
| `ETGMassFlowConvention` | `MassFlowConvention` | Exact |

## FTGInitialSpacecraftState

| Type | Variable | Status |
|---|---|---|
| `FVector` | `PositionMeters` | Exact |
| `FVector` | `VelocityMetersPerSecond` | Exact |
| `FQuat` | `AttitudeBodyToIcrf` | Exact |
| `FVector` | `AngularVelocityBodyRadiansPerSecond` | Exact |
| `FName` | `AuthoringFrameCatalogKey` | Additional frontend authoring preference; preserved in `.tgscn` and omitted from `SimulationRequest` |

## FTGComponentConfig

| Type | Variable | Status |
|---|---|---|
| `FGuid` | `ComponentId` | Additional HUD identity |
| `FString` | `Name` | Exact |
| `double` | `InitialMassKilograms` | Exact |
| `double` | `MinimumMassKilograms` | Exact |
| `bool` | `bVariableMass` | Exact; also enables automatic linear inertia scaling |
| `FVector` | `LocalCenterOfMassMeters` | Exact |
| `FTGSymmetricInertia` | `CentroidalInertia` | Exact |
| `FVector` | `OriginInBodyMeters` | Exact; root component only in HUD |
| `FQuat` | `ComponentToBodyOrientation` | Exact; root component only in HUD |
| `FString` | `ParentComponentName` | Exact; non-root components only |
| `FVector` | `ParentAnchorMeters` | Exact; non-root components only |
| `FVector` | `ChildAnchorMeters` | Exact; non-root components only |
| `FQuat` | `ChildToParentZeroOrientation` | Exact; non-root components only |
| `TArray<FTGJointDofConfig>` | `DegreesOfFreedom` | Exact |
| `FTGComponentVisualConfig` | `Visual` | Exact frontend representation; see fixed visual-transform fields below |
| `FTGComponentSrpConfig` | `SolarRadiationPressure` | Exact frontend authoring representation |

## FTGSymmetricInertia

| Type | Variable | Status |
|---|---|---|
| `double` | `IxxKilogramMetersSquared` | Exact |
| `double` | `IyyKilogramMetersSquared` | Exact |
| `double` | `IzzKilogramMetersSquared` | Exact |
| `double` | `IxyKilogramMetersSquared` | Exact |
| `double` | `IxzKilogramMetersSquared` | Exact |
| `double` | `IyzKilogramMetersSquared` | Exact |

## FTGJointDofConfig

| Type | Variable | Status |
|---|---|---|
| `FGuid` | `DofId` | Additional HUD identity |
| `FString` | `Name` | Exact |
| `ETGJointMotionType` | `MotionType` | Exact |
| `FVector` | `Axis` | Exact |
| `double` | `InitialCoordinate` | Exact |
| `double` | `InitialRate` | Exact |
| `bool` | `bHasMinimumCoordinate` | Exact |
| `double` | `MinimumCoordinate` | Exact |
| `bool` | `bHasMaximumCoordinate` | Exact |
| `double` | `MaximumCoordinate` | Exact |
| `double` | `MaximumAbsoluteRate` | Exact |
| `double` | `MaximumAbsoluteEffort` | Exact |

## FTGComponentVisualConfig

| Type | Variable | Status |
|---|---|---|
| `ETGComponentGeometrySource` | `GeometrySource` | Exact |
| `ETGPrimitiveGeometryType` | `PrimitiveType` | Exact |
| `FVector` | `BoxDimensionsMeters` | Exact |
| `double` | `SphereRadiusMeters` | Exact |
| `double` | `CylinderRadiusMeters` | Exact |
| `double` | `CylinderLengthMeters` | Exact |
| `FString` | `StlFilePath` | Exact |
| `ETGStlLengthUnit` | `StlLengthUnit` | Exact |
| `ETGStlRecenterMode` | `StlRecenterMode` | Fixed to `KeepImportedOrigin`, as specified by the MD |
| `FVector` | `VisualOffsetMeters` | Fixed to zero, as specified by the MD |
| `FQuat` | `VisualOrientation` | Fixed to identity, as specified by the MD |
| `FVector` | `VisualScale` | Fixed to one, as specified by the MD |
| `ETGComponentSurfaceAppearanceMode` | `SurfaceAppearanceMode` | Additional visualization setting |
| `FLinearColor` | `DisplayColor` | Exact visualization setting |
| `FLinearColor` | `BaseColorTint` | Additional visualization setting |
| `FString` | `BaseColorTextureFilePath` | Additional visualization setting |
| `FString` | `NormalTextureFilePath` | Additional visualization setting |
| `FString` | `RoughnessTextureFilePath` | Additional visualization setting |
| `FString` | `MetallicTextureFilePath` | Additional visualization setting |
| `bool` | `bVisible` | Exact visualization setting |

## FTGComponentSrpConfig

| Type | Variable | Status |
|---|---|---|
| `bool` | `bIncludedInProxy` | Exact |
| `ETGSrpProxyResolutionMode` | `ProxyResolutionMode` | Exact |
| `int32` | `CustomTargetTriangleCount` | Exact |
| `bool` | `bUseGlobalFallbackOpticalProperties` | Exact |
| `bool` | `bApplyOneOpticalConfigurationToEntireComponent` | Exact |
| `FTGSrpOpticalProperties` | `ComponentOpticalProperties` | Exact; HUD weights are normalized to stored fractions |
| `TArray<FTGSrpLogicalRegionOverride>` | `LogicalRegionOverrides` | Exact for primitive logical faces; precedence is not fully documented in MD |
| `TArray<FTGSrpTriangleOverride>` | `TriangleOverrides` | Exact; stable triangles are used only for custom STL proxies |
| `FString` | `GeneratedGeometrySignature` | Derived/output cache |
| `int32` | `GeneratedTriangleCount` | Derived/output cache |
| `bool` | `bProxyGenerationRequired` | Derived/output cache state |
| `FString` | `LastProxyGenerationMessage` | Derived/output cache status |

## FTGSrpOpticalProperties

| Type | Variable | Status |
|---|---|---|
| `double` | `AbsorptionFraction` | Exact stored fraction produced from the HUD weight |
| `double` | `SpecularReflectionFraction` | Exact stored fraction produced from the HUD weight |
| `double` | `DiffuseReflectionFraction` | Exact stored fraction produced from the HUD weight |

## FTGSrpLogicalRegionOverride

| Type | Variable | Status |
|---|---|---|
| `ETGSrpLogicalRegion` | `Region` | Exact primitive-region selection |
| `FTGSrpOpticalProperties` | `OpticalProperties` | Exact stored fractions produced from HUD weights |

## FTGSrpTriangleOverride

| Type | Variable | Status |
|---|---|---|
| `int32` | `ProxyTriangleIndex` | Exact stable proxy index |
| `FTGSrpOpticalProperties` | `OpticalProperties` | Exact stored fractions produced from HUD weights |

## FTGThrusterConfig

| Type | Variable | Status |
|---|---|---|
| `FString` | `Name` | Exact |
| `ETGThrusterMode` | `Mode` | Exact |
| `FString` | `MountComponentName` | Exact |
| `FString` | `PropellantComponentName` | Exact |
| `FVector` | `ApplicationPointMeters` | Exact |
| `FVector` | `Direction` | Exact |
| `ETGThrusterTimeMode` | `IgnitionTimeMode` | Additional helper for the requested UTC-or-elapsed input |
| `FDateTime` | `IgnitionUtc` | Exact |
| `double` | `IgnitionElapsedSeconds` | Exact |
| `bool` | `bNeverShutsDown` | Exact |
| `ETGThrusterTimeMode` | `ShutdownTimeMode` | Additional helper for the requested UTC-or-elapsed input |
| `FDateTime` | `ShutdownUtc` | Exact |
| `double` | `ShutdownElapsedSeconds` | Exact |
| `FTGScalarProfileConfig` | `PrescribedThrust` | Exact |
| `FTGScalarProfileConfig` | `PrescribedSpecificImpulse` | Exact |
| `double` | `MaximumThrustNewtons` | Exact; commanded mode only |

## FTGScalarProfileConfig

| Type | Variable | Status |
|---|---|---|
| `ETGScalarProfileSource` | `Source` | Exact |
| `double` | `ConstantValue` | Exact |
| `FString` | `CsvFilePath` | Exact |

## FTGReactionWheelConfig

| Type | Variable | Status |
|---|---|---|
| `FString` | `Name` | Exact |
| `FString` | `MountComponentName` | Exact |
| `FVector` | `Axis` | Exact |
| `double` | `InitialMomentumNewtonMeterSeconds` | Exact |
| `double` | `MaximumAbsoluteMomentumNewtonMeterSeconds` | Exact |

## FTGControlConfig

| Type | Variable | Status |
|---|---|---|
| `ETGControlMode` | `Mode` | Exact |
| `FName` | `ControllerId` | Exact |
| `FString` | `StandaloneControllerDllFilePath` | Portable DLL reference preserved on import/export; not an ordinary HUD input |

Controller source, DLL bytes, build, trust, and registry data are managed outside `FTGSimulationScenario`. The scenario retains only the mode, library ID, and imported standalone DLL path. Review requires importing and trusting that DLL in the Controller Library before it can be selected for a graphical run.

## FTGGravitySettings

| Type | Variable | Status |
|---|---|---|
| `bool` | `bIncludeFirstPostNewtonianCorrection` | Exact |

## FTGCelestialBodyConfig

| Type | Variable | Status |
|---|---|---|
| `FName` | `CatalogKey` | Fixed catalog identity; not typed by the user |
| `bool` | `bGravityEnabled` | Exact |
| `double` | `AutomaticActivationRadiusMeters` | Exact |
| `double` | `BarycenterResolutionRadiusMeters` | Exact; barycenters only |
| `FString` | `HarmonicModelCsvFilePath` | Exact; physical bodies only |
| `int32` | `MaximumHarmonicDegreeUsed` | Exact; physical bodies only |

## FTGSolarRadiationPressureConfig

| Type | Variable | Status |
|---|---|---|
| `bool` | `bEnabled` | Exact |
| `FString` | `SunBodyName` | Fixed; MD says it is not an ordinary HUD input |
| `double` | `PressureAtOneAstronomicalUnitPascals` | Fixed; MD says it is not an ordinary HUD input |
| `bool` | `bComputeEclipse` | Exact |
| `TArray<FString>` | `OccultingBodyNames` | Fixed empty; empty means all physical non-Sun bodies, as specified by the MD |
| `bool` | `bComputeComponentShadows` | Exact |
| `FTGSrpOpticalProperties` | `GlobalFallbackOpticalProperties` | Exact stored fractions produced from HUD weights |
| `TArray<FTGOpticalFacetConfig>` | `OpticalFacets` | Derived converter output; not authored by HUD |

## FTGOpticalFacetConfig

| Type | Variable | Status |
|---|---|---|
| `FString` | `Name` | Derived/output diagnostic identity |
| `FGuid` | `ComponentId` | Derived/output component identity |
| `FString` | `ComponentName` | Derived/output diagnostic snapshot |
| `int32` | `StableTriangleIndex` | Derived/output stable index |
| `ETGSrpLogicalRegion` | `LogicalRegion` | Derived/output primitive region |
| `FVector` | `Vertex0Meters` | Derived converter output |
| `FVector` | `Vertex1Meters` | Derived converter output |
| `FVector` | `Vertex2Meters` | Derived converter output |
| `double` | `AbsorptionFraction` | Derived converter-resolved output |
| `double` | `SpecularReflectionFraction` | Derived converter-resolved output |
| `double` | `DiffuseReflectionFraction` | Derived converter-resolved output |

## FTGAtmosphereConfig

| Type | Variable | Status |
|---|---|---|
| `bool` | `bEnabled` | Exact |
| `FString` | `CentralBodyName` | Exact |
| `ETGAtmosphereModel` | `Model` | Exact |
| `FString` | `GeneralProfileCsvPath` | Exact |
| `double` | `CenteredAverageF107SolarFluxUnits` | Exact |
| `FString` | `ChpCoefficientCsvPath` | Exact |
| `FString` | `ChpMolecularProfileCsvPath` | Exact |

## FTGAerodynamicsConfig

| Type | Variable | Status |
|---|---|---|
| `bool` | `bEnabled` | Exact |
| `double` | `ReferenceAreaSquareMeters` | Exact |
| `double` | `ReferenceLengthMeters` | Exact |
| `double` | `MinimumDynamicPressurePascals` | Exact |
| `double` | `MaximumValidDynamicPressurePascals` | Exact |
| `bool` | `bEnableConstantDragFallback` | Exact |
| `double` | `FallbackDragCoefficient` | Exact |
| `FTGAerodynamicDatabaseConfig` | `Database` | Exact |

## FTGAerodynamicDatabaseConfig

| Type | Variable | Status |
|---|---|---|
| `bool` | `bEnabled` | Exact |
| `FString` | `CsvFilePath` | Exact |
| `ETGAerodynamicDatabaseInterpolation` | `Interpolation` | Exact |
| `ETGAerodynamicDatabaseExtrapolation` | `Extrapolation` | Exact |
| `int32` | `NeighborCount` | Exact |
| `double` | `InverseDistancePower` | Exact |
| `bool` | `bUseMaximumNormalizedNeighborDistance` | Exact optional-enable representation |
| `double` | `MaximumNormalizedNeighborDistance` | Exact |
| `FVector` | `MomentReferenceCenterBodyMeters` | Exact |
| `TArray<FTGAerodynamicDatabaseRow>` | `Rows` | Optional parsed cache allowed by MD |

## FTGAerodynamicDatabaseRow

| Type | Variable | Status |
|---|---|---|
| `double` | `SpeedRatio` | Exact |
| `double` | `KnudsenNumber` | Exact |
| `FVector` | `GasFlowDirectionBody` | Exact |
| `TArray<double>` | `ArticulationCoordinates` | Exact parsed eta vector |
| `FVector` | `BodyForceCoefficients` | Exact |
| `FVector` | `BodyMomentCoefficients` | Exact |

## Intentional Current HUD Representation

| Area | Current specification |
|---|---|
| SRP occulting bodies | The HUD always uses all physical non-Sun bodies and stores an empty occulter list |
| SRP optical entry | The HUD accepts nonnegative weights, normalizes them, and stores exact fractions |
| Primitive SRP overrides | Cuboids and cylinders use logical regions; spheres use whole-component optics |
| Custom-STL SRP overrides | Stable proxy-triangle editing is available only up to 32 generated triangles |
| STL recentering | Stored but fixed to `KeepImportedOrigin` |
| Visual offset/orientation/scale | Stored but fixed to zero/identity/one |

## Portable File Boundary

| Operation | Implemented path |
|---|---|
| HUD to `.tgscn` | `FTGSimulationScenario -> ScenarioDocument -> SaveScenarioFile` |
| `.tgscn` to HUD | `LoadScenarioFile -> ScenarioDocument -> FTGSimulationScenario` |
| HUD to backend | `FTGSimulationScenario -> ScenarioDocument -> CompileScenario -> SimulationRequest` |
| File to standalone backend | `.tgscn -> ScenarioDocument -> CompileScenario -> SimulationRequest` |

Relative paths are authored relative to the `.tgscn` directory. On Unreal
import they are expanded to absolute paths for the existing file-picker fields.
Absolute paths remain valid. UTC strings are converted to SPICE ET only while
compiling a run.
