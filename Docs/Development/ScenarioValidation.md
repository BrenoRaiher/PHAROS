# FTGSimulationScenario Review Panel Validation Contract

This file defines the final HUD Review Panel for an authored
`FTGSimulationScenario`. It is intentionally stricter and more specific than
TGSimCore's first-error validation because the Review Panel must identify every
problem the user can fix before conversion or propagation.

Contract hierarchy:

1. `Docs/Reference/ScenarioInputs.md` defines what the current HUD asks and the
   meaning of every authored value.
2. `Source/TG/Public/Simulation/TGSimulationScenarioTypes.h` defines how those
   values are stored in Unreal.
3. `Docs/Development/ScenarioMapping.md` audits that storage against the
   specification; it does not override the specification.
4. TGSimCore validation in `SimulationEngine.cpp` remains the authoritative
   final backend safety gate.

## 1. Backend Compatibility Decision

The intentional HUD representation choices recorded in
[ScenarioMapping.md](ScenarioMapping.md) do not require changes to TGSimCore. The
HUD-to-backend converter can produce the existing canonical backend
representation:

| Current HUD choice | Converter action |
|---|---|
| Optical values are entered as nonnegative weights | Store or convert them to finite fractions in `[0,1]` that sum to one before creating backend facets |
| Every physical non-Sun body is an SRP occulter | Clear `OccultingBodyNames`; an empty backend list already means all supported non-Sun bodies |
| Primitive SRP editing uses logical regions | Resolve region properties onto the generated primitive triangles |
| Custom STL SRP editing uses stable triangle indices | Resolve stable-index overrides onto the generated STL proxy triangles |
| STL origin is always preserved | Interpret STL coordinates using `KeepImportedOrigin` |
| Visual offset/orientation/scale are fixed | Ignore them physically and require zero/identity/one in the stored HUD scenario |
| Additional GUID, appearance, texture, and cache fields | Keep them in Unreal; do not expose them as TGSimCore physics inputs |

The converter must not reinterpret physics. It performs unit, frame, identity,
file, indexing, and representation conversion only.

## 2. Acceptance Rule

The Review Panel must produce an array of issues. Each issue needs:

```text
Severity: Error or Warning
Code: stable machine-readable identifier
Path: exact FTGSimulationScenario field or array entry
Message: complete user-facing string
Section: HUD panel to reopen when the issue is selected
```

The displayed string is always:

```text
[ERROR] <Path>: <Message>
[WARNING] <Path>: <Message>
```

`{name}`, `{index}`, `{value}`, and similar braces in this document are runtime
substitutions. They must not appear literally in the HUD.

Acceptance is permitted only when `ErrorCount == 0`. Warnings never block
acceptance, although the panel should ask the user to acknowledge them.
Validation must aggregate all independent issues. Do not stop after the first
error, except inside one malformed file when continuing cannot identify column
semantics reliably.

## 3. Two Validation Phases

### 3.1 Authoring Review

This is the Review Panel requested here. It validates every user-authored
field, referenced file, controller selection, and cross-panel dependency.

Generated SRP triangles are not required yet. Use
`ValidateSolarRadiationPressureAuthoring`, not the final-proxy validator.
Missing or stale generated proxies are converter work and must not prevent the
HUD from accepting an otherwise valid authored scenario.

### 3.2 Converter Run Readiness

After scenario preparation has parsed files, generated SRP
proxies, attached SPICE, and created a canonical `SimulationRequest`, it must
perform the additional checks in section 16. These are setup errors, not new
HUD inputs.

## 4. Validation Order and Existing Helpers

Run validation against an immutable snapshot so the draft cannot change while
files or SPICE are being inspected.

1. Audit enum values, identities, names, and array structure.
2. Call `UTGComponentTreeValidationLibrary::ValidateScenarioComponentTree`.
3. Validate every thruster with `UTGActuatorEditingLibrary::ValidateThruster`.
4. Validate every wheel with `UTGActuatorEditingLibrary::ValidateReactionWheel`.
5. Call `UTGControlEditingLibrary::ValidateControlSelection`.
6. Inspect gravity rows and harmonic files with
   `UTGHarmonicCsvLibrary::InspectHarmonicModelCsv`.
7. Call `UTGSolarRadiationPressureEditingLibrary::ValidateSolarRadiationPressureAuthoring`.
8. Call `UTGEnvironmentEditingLibrary::ValidateAtmosphereScenario` and
   `ValidateAerodynamicsScenario`.
9. Run the cross-panel checks in section 15.
10. Perform the SPICE preflight in section 15.6.

Do not silently normalize malformed user data during review. Unit axes and
nonzero quaternions may be normalized by the converter only after emitting the
warnings specified below. Fixed SRP fields and the gravity catalog may be
canonicalized on the accepted copy, but any discarded stored value must first
produce the stated warning.

## 5. General Data Rules

These rules apply wherever the corresponding type appears.

| Code | Severity | Condition | Message |
|---|---|---|---|
| GEN-001 | Error | A scalar or vector component that is required for the active mode is NaN or infinite | `Value must be finite.` |
| GEN-002 | Error | A required text value is empty after trimming | `Value cannot be empty.` |
| GEN-003 | Error | A required GUID is invalid | `Persistent identifier is invalid.` |
| GEN-004 | Error | A persistent GUID is duplicated in its required uniqueness scope | `Persistent identifier is duplicated.` |
| GEN-005 | Error | A serialized enum contains an unsupported underlying value | `Stored option is unsupported or corrupted; select a valid option again.` |
| GEN-006 | Warning | A user-facing name has leading or trailing whitespace | `Leading or trailing whitespace will be removed.` |
| GEN-007 | Warning | A nonzero axis differs from unit length by more than `1e-6` | `Vector norm is {norm}; it will be normalized before simulation.` |
| GEN-008 | Error | An axis norm is at most `1e-12` | `Vector must be finite and nonzero.` |
| GEN-009 | Error | A quaternion is nonfinite or has squared norm at most `1e-12` | `Quaternion must be finite and nonzero.` |
| GEN-010 | Warning | A quaternion norm differs from one by more than `1e-6` | `Quaternion norm is {norm}; it will be normalized before simulation.` |

Inactive conditional fields do not produce numeric errors unless this document
explicitly marks them as fixed/canonical fields. The converter must ignore or
clear inactive values rather than send contradictory data to TGSimCore.

## 6. Scenario and Solver

Paths below are relative to `ScenarioAndSolver`.

| Code | Path | Severity | Condition | Message |
|---|---|---|---|---|
| SOL-001 | `ScenarioName` | Error | Empty after trimming | `Scenario name cannot be empty.` |
| SOL-002 | `SimulationKind` | Error | Not `Spacecraft6Dof` | `Only Spacecraft 6-DOF simulation is currently supported.` |
| SOL-003 | `StartUtc` | Error | Equal to `FDateTime::MinValue()` or UTC-to-ET conversion fails | `Enter a valid start UTC that SPICE can convert.` |
| SOL-004 | `EndMode` | Error | `Unspecified` or invalid | `Select Final UTC or Duration as the simulation end mode.` |
| SOL-005 | `FinalUtc` | Error | Final-UTC mode and value is invalid | `Enter a valid final UTC that SPICE can convert.` |
| SOL-006 | `FinalUtc` | Error | Converted final ET precedes start ET | `Final UTC must not precede start UTC.` |
| SOL-007 | `DurationSeconds` | Error | Duration mode and value is nonfinite or negative | `Duration must be finite and nonnegative.` |
| SOL-008 | `DurationSeconds` | Warning | Effective duration is zero | `Simulation duration is zero; only the initial state will be recorded.` |
| SOL-009 | `IntegratorKind` | Error | `Unspecified` or unsupported | `Select Fixed-Step RK4 or Adaptive Dormand-Prince 5(4).` |
| SOL-010 | `MaximumIntegratorStepSeconds` | Error | Nonfinite or not positive | `Maximum integrator step must be a positive finite number of seconds.` |
| SOL-011 | `InitialIntegratorStepSeconds` | Error | Adaptive mode and nonfinite or not positive | `Initial adaptive step must be a positive finite number of seconds.` |
| SOL-012 | `InitialIntegratorStepSeconds` | Warning | Adaptive mode and greater than maximum step | `Initial adaptive step exceeds the maximum integrator step and will be capped to that maximum.` |
| SOL-013 | `AbsoluteTolerance` | Error | Adaptive mode and nonfinite or not positive | `Absolute tolerance must be positive and finite.` |
| SOL-014 | `RelativeTolerance` | Error | Adaptive mode and nonfinite or not positive | `Relative tolerance must be positive and finite.` |
| SOL-015 | `OutputMode` | Error | `Unspecified` or unsupported | `Select Every Integrator Step or Fixed Interval output.` |
| SOL-016 | `OutputStepSeconds` | Error | Fixed-interval mode and nonfinite or not positive | `Output interval must be a positive finite number of seconds.` |
| SOL-017 | `MaximumIntegrationSteps` | Error | Not positive | `Maximum integration attempts must be greater than zero.` |
| SOL-018 | `MaximumOutputSamples` | Error | Not positive | `Maximum stored samples must be greater than zero.` |
| SOL-019 | `MassFlowConvention` | Error | Not `ThrustIncludesExhaustMomentum` | `Only Thrust Includes Exhaust Momentum is currently supported.` |
| SOL-020 | `MaximumIntegrationSteps` | Error | Positive duration and less than `ceil(duration / maximum_step)` | `Maximum integration attempts is below the theoretical minimum required to reach the final time.` |
| SOL-021 | `MaximumOutputSamples` | Error | Fixed-interval output and `1 + ceil(duration / output_step)` exceeds the limit for positive duration | `Maximum stored samples is smaller than the number required by the selected duration and output interval.` |
| SOL-022 | `OutputStepSeconds` | Warning | Fixed interval exceeds a positive duration | `Output interval exceeds the simulation duration; only boundary samples may be stored.` |
| SOL-023 | `MaximumWallClockRuntimeSeconds` | Error | Not positive and finite | `Maximum backend runtime must be a positive finite number of real-time seconds.` |
| SOL-023 | `MaximumIntegrationSteps` | Error | Fixed RK4 and a dry-run boundary count exceeds the limit | `Maximum integration attempts is too small after accounting for output times and scheduled thruster/profile events; at least {required} attempts are required.` |

For fixed RK4, the initial adaptive step and tolerance fields are inactive and
must not produce authoring-review errors. If the backend request retains those
members, the converter supplies canonical values rather than requiring dormant
HUD inputs.

The fixed-RK4 dry run in `SOL-023` must reproduce TGSimCore's time-boundary
logic without evaluating dynamics: repeatedly advance by the minimum of the
maximum step, remaining duration, next fixed-output time, and next prescribed
thruster ignition, shutdown, thrust-sample, or Isp-sample time. This yields the
exact fixed-step attempt count for the authored schedule. Adaptive rejected
steps cannot be predicted during review, so only the theoretical lower bound
can be checked before propagation.

## 7. Initial State

| Code | Path | Severity | Condition | Message |
|---|---|---|---|---|
| STA-001 | `InitialState.PositionMeters` | Error | Any component nonfinite | `Initial ICRF position must contain three finite values in meters.` |
| STA-002 | `InitialState.VelocityMetersPerSecond` | Error | Any component nonfinite | `Initial ICRF velocity must contain three finite values in meters per second.` |
| STA-003 | `InitialState.AttitudeBodyToIcrf` | Error | Nonfinite or zero quaternion | `Initial B-to-ICRF attitude quaternion must be finite and nonzero.` |
| STA-004 | `InitialState.AttitudeBodyToIcrf` | Warning | Unit-length error exceeds `1e-6` | `Initial attitude quaternion norm is {norm}; it will be normalized before simulation.` |
| STA-005 | `InitialState.AngularVelocityBodyRadiansPerSecond` | Error | Any component nonfinite | `Initial body angular velocity must contain three finite values in radians per second.` |

Total mass, variable-component masses, flattened joint states, wheel momenta,
and start ET are derived. They are not separate Review Panel inputs.

## 8. Components, Joints, and Visual Geometry

The Review Panel must include every issue returned by
`ValidateScenarioComponentTree`; its `Path` and `Message` are already suitable
for direct display. The following checks are mandatory and summarize that
contract.

### 8.1 Component Array and Mass Properties

| Code | Path | Severity | Condition | Message |
|---|---|---|---|---|
| CMP-001 | `Components` | Error | Empty | `The spacecraft must contain at least one physical component.` |
| CMP-002 | `Components[{i}].ComponentId` | Error | Invalid | `Component identifier is invalid.` |
| CMP-003 | `Components[{i}].ComponentId` | Error | Duplicated | `Component identifier is duplicated.` |
| CMP-004 | `Components[{i}].Name` | Error | Empty after trimming | `Component name cannot be empty.` |
| CMP-005 | `Components[{i}].Name` | Error | Case-insensitive duplicate after trimming | `Component name '{name}' is duplicated.` |
| CMP-006 | `Components[{i}].InitialMassKilograms` | Error | Nonfinite or `< 0` | `Initial mass must be finite and nonnegative.` |
| CMP-007 | `Components[{i}].MinimumMassKilograms` | Error | Nonfinite or `< 0` | `Minimum mass must be finite and nonnegative.` |
| CMP-008 | `Components[{i}].MinimumMassKilograms` | Error | Greater than initial mass | `Minimum mass cannot exceed initial mass.` |
| CMP-009 | `Components.TotalInitialMassKilograms` | Error | Aggregate is nonfinite or `<= 0` | `Total initial spacecraft mass must be finite and greater than zero.` |
| CMP-010 | `Components[{i}].MinimumMassKilograms` | Warning | Fixed mass and differs from initial mass | `Minimum mass differs from initial mass although Variable Mass is disabled.` |
| CMP-011 | `Components[{i}].MinimumMassKilograms` | Warning | Variable mass and equals initial mass | `This variable-mass component has no consumable mass above its minimum.` |
| CMP-012 | `Components.MinimumReachableMassKilograms` | Error | Aggregate depletion floor is nonfinite or `<= 0` | `Minimum reachable spacecraft mass must be finite and greater than zero.` |
| CMP-013 | `Components[{i}].LocalCenterOfMassMeters` | Error | Nonfinite | `Local center of mass must contain three finite component-frame values.` |
| CMP-014 | `Components[{i}].CentroidalInertia` | Error | Any coefficient nonfinite | `Every inertia-tensor coefficient must be finite.` |
| CMP-015 | `Components[{i}].CentroidalInertia` | Error | A principal moment is not strictly positive | `All principal moments of inertia must be strictly positive.` |
| CMP-016 | `Components[{i}].CentroidalInertia` | Error | Principal moments violate the rigid-body triangle inequality | `The principal moments violate the rigid-body triangle inequality: Imax must not exceed the sum of the other two principal moments.` |

After canonical conversion, assemble the initial total inertia. If its
determinant magnitude is at most `1e-18`, emit:

```text
[ERROR] Components: Initial assembled spacecraft inertia is singular; revise component masses, centers of mass, inertias, or geometry offsets.
```

### 8.2 Tree and Joint Checks

| Code | Path | Severity | Condition | Message |
|---|---|---|---|---|
| TREE-001 | `Components[0].ParentComponentName` | Error | Nonempty | `The main component cannot have a parent.` |
| TREE-002 | `Components[0].DegreesOfFreedom` | Error | Nonempty | `The main component cannot have parent-joint degrees of freedom.` |
| TREE-003 | `Components[0].OriginInBodyMeters` | Error | Nonfinite | `Main-component origin must contain three finite B-frame values.` |
| TREE-004 | `Components[0].ComponentToBodyOrientation` | Error | Nonfinite or zero | `Main-component-to-B orientation must be finite and nonzero.` |
| TREE-005 | `Components[0].ComponentToBodyOrientation` | Warning | Not unit within `1e-6` | `Main-component orientation norm is {norm}; it will be normalized before simulation.` |
| TREE-006 | `Components[{i}].ParentComponentName` | Error | Child parent is empty | `Every child component requires a parent component.` |
| TREE-007 | `Components[{i}].ParentComponentName` | Error | Name does not resolve | `Parent component '{parent}' does not exist.` |
| TREE-008 | `Components[{i}].ParentComponentName` | Error | Parent index is not less than child index | `A child must reference a parent appearing earlier in the component array.` |
| TREE-009 | `Components[{i}].ParentAnchorMeters` | Error | Nonfinite | `Parent anchor must contain three finite parent-frame values.` |
| TREE-010 | `Components[{i}].ChildAnchorMeters` | Error | Nonfinite | `Child anchor must contain three finite child-frame values.` |
| TREE-011 | `Components[{i}].ChildToParentZeroOrientation` | Error | Nonfinite or zero | `Child-to-parent zero orientation must be finite and nonzero.` |
| TREE-012 | `Components[{i}].ChildToParentZeroOrientation` | Warning | Not unit within `1e-6` | `Child-to-parent zero-orientation norm is {norm}; it will be normalized before simulation.` |
| DOF-001 | `Components[{i}].DegreesOfFreedom[{j}].DofId` | Error | Invalid | `Joint DOF identifier is invalid.` |
| DOF-002 | same | Error | Duplicated globally | `Joint DOF identifier is duplicated.` |
| DOF-003 | `.Name` | Error | Empty | `Joint DOF name cannot be empty.` |
| DOF-004 | `.Name` | Error | Duplicated inside the same component, case-insensitive | `Joint DOF name '{name}' is duplicated inside this component.` |
| DOF-005 | `.MotionType` | Error | Unsupported enum | `Joint motion type is unsupported.` |
| DOF-006 | `.Axis` | Error | Nonfinite or norm `<= 1e-12` | `Joint axis must be finite and nonzero.` |
| DOF-007 | `.Axis` | Warning | Unit-length error exceeds `1e-6` | `Joint axis length is {norm}; it will be normalized before simulation.` |
| DOF-008 | `.InitialCoordinate` | Error | Nonfinite | `Initial coordinate must be finite.` |
| DOF-009 | `.InitialRate` | Error | Nonfinite | `Initial rate must be finite.` |
| DOF-010 | `.MaximumAbsoluteRate` | Error | Nonfinite or negative | `Maximum absolute rate must be finite and nonnegative.` |
| DOF-011 | `.InitialRate` | Error | Absolute initial rate exceeds maximum | `Initial rate exceeds the maximum absolute rate.` |
| DOF-012 | `.MaximumAbsoluteEffort` | Error | Nonfinite or negative | `Maximum absolute effort must be finite and nonnegative.` |
| DOF-013 | `.MinimumCoordinate` | Error | Enabled and nonfinite | `Minimum coordinate must be finite when its limit is enabled.` |
| DOF-014 | `.MaximumCoordinate` | Error | Enabled and nonfinite | `Maximum coordinate must be finite when its limit is enabled.` |
| DOF-015 | `.CoordinateLimits` | Error | Enabled minimum exceeds enabled maximum | `Minimum coordinate cannot exceed maximum coordinate.` |
| DOF-016 | `.InitialCoordinate` | Error | Outside an enabled coordinate bound | `Initial coordinate lies outside the enabled coordinate limits.` |

An empty DOF array on a child is valid and represents a fixed connection.

### 8.3 Visual Geometry and Appearance

| Code | Path | Severity | Condition | Message |
|---|---|---|---|---|
| VIS-001 | `.Visual.GeometrySource` | Error | `NoGeometry` or invalid | `Choose Standard Primitive or Custom STL for this physical component.` |
| VIS-002 | `.Visual.BoxDimensionsMeters` | Error | Selected box and any dimension is nonfinite or not positive | `All box dimensions must be finite and greater than zero.` |
| VIS-003 | `.Visual.SphereRadiusMeters` | Error | Selected sphere and radius is nonfinite or not positive | `Sphere radius must be finite and greater than zero.` |
| VIS-004 | `.Visual.CylinderRadiusMeters` | Error | Selected cylinder and radius is nonfinite or not positive | `Cylinder radius must be finite and greater than zero.` |
| VIS-005 | `.Visual.CylinderLengthMeters` | Error | Selected cylinder and length is nonfinite or not positive | `Cylinder length must be finite and greater than zero.` |
| VIS-006 | `.Visual.StlFilePath` | Error | Custom STL and empty | `Custom STL geometry requires a selected STL file.` |
| VIS-007 | `.Visual.StlFilePath` | Error | Extension is not `.stl` | `Custom geometry file must use the .stl extension.` |
| VIS-008 | `.Visual.StlFilePath` | Error | Missing or unreadable | `Selected STL file does not exist or cannot be read.` |
| VIS-009 | `.Visual.StlFilePath` | Error | STL parser finds no complete finite triangle | `Selected STL contains no valid finite triangles.` |
| VIS-010 | `.Visual.StlLengthUnit` | Error | Unsupported enum | `Select millimeters, centimeters, or meters for STL coordinates.` |
| VIS-011 | `.Visual.StlRecenterMode` | Error | Not `KeepImportedOrigin` | `STL recentering must remain Keep Imported Origin. Adjust the source STL instead.` |
| VIS-012 | `.Visual.VisualOffsetMeters` | Error | Not exactly zero | `Visual offset must remain zero. Adjust the source geometry instead.` |
| VIS-013 | `.Visual.VisualOrientation` | Error | Not exactly identity | `Visual orientation must remain identity. Adjust the source geometry instead.` |
| VIS-014 | `.Visual.VisualScale` | Error | Not exactly one | `Visual scale must remain one. Adjust the source geometry or choose the correct STL length unit.` |
| VIS-015 | `.Visual.DisplayColor` | Error | Solid-color mode and any channel is nonfinite | `Display color must be finite.` |
| VIS-016 | `.Visual.BaseColorTint` | Error | Textured mode and any channel is nonfinite | `Base-color tint must be finite.` |
| VIS-017 | `.Visual.SurfaceAppearanceMode` | Error | Unsupported enum | `Surface appearance mode is invalid.` |
| VIS-018 | `.Visual.BaseColorTextureFilePath` | Error | Textured mode and empty | `Base color texture is required in Textured mode.` |
| VIS-019 | any nonempty texture path | Error | Textured mode and extension is not PNG/JPG/JPEG | `{label} texture must be a PNG, JPG, or JPEG file.` |
| VIS-020 | any nonempty texture path | Error | Textured mode and file is missing, unreadable, or undecodable | `{label} texture file does not exist or cannot be decoded.` |
| VIS-021 | `.Visual.PrimitiveType` | Error | Primitive geometry selected and enum is unsupported | `Select Box, Sphere, or Cylinder as the primitive geometry type.` |
| VIS-022 | `Components` | Warning | Every component has `.Visual.bVisible == false` | `All spacecraft components are hidden; result playback will show no spacecraft geometry.` |

`bVisible`, colors, and texture maps are visualization-only. They never modify
mass properties or the backend dynamics.

## 9. Thrusters and Scalar Profiles

Call `ValidateThruster` for every row and prefix its returned message with
`Thrusters[{i}] ('{name}')`. Also enforce unique thruster names over the whole
array.

| Code | Path | Severity | Condition | Message |
|---|---|---|---|---|
| THR-001 | `.Name` | Error | Empty | `Thruster name cannot be empty.` |
| THR-002 | `.Name` | Error | Case-insensitive duplicate | `A thruster named '{name}' already exists.` |
| THR-003 | `.Mode` | Error | Unsupported enum | `Select Prescribed Profile or Commanded mode.` |
| THR-004 | `.MountComponentName` | Error | Does not resolve | `Select an existing mount component.` |
| THR-005 | `.PropellantComponentName` | Error | Does not resolve | `Select an existing propellant component.` |
| THR-006 | `.PropellantComponentName` | Error | Resolved component is not variable mass | `Propellant component '{name}' must be marked Variable Mass in the Component Tree.` |
| THR-007 | `.ApplicationPointMeters` | Error | Nonfinite | `Thruster application point must contain three finite mount-component values.` |
| THR-008 | `.Direction` | Error | Nonfinite or norm `<= 1e-12` | `Thruster direction must be a finite nonzero vector.` |
| THR-009 | `.Direction` | Warning | Unit-length error exceeds `1e-6` | `Thruster direction norm is {norm}; it will be normalized before storage.` |
| THR-010 | `.IgnitionTimeMode` | Error | Unsupported enum | `Select Absolute UTC or Elapsed Simulation Time for ignition.` |
| THR-011 | `.IgnitionElapsedSeconds` | Error | Elapsed mode and nonfinite or negative | `Ignition elapsed time must be finite and nonnegative.` |
| THR-012 | `.IgnitionUtc` | Error | Absolute mode and invalid or not convertible by SPICE | `Enter a valid ignition UTC time.` |
| THR-013 | `.ShutdownTimeMode` | Error | `bNeverShutsDown` is false and enum is unsupported | `Select Absolute UTC or Elapsed Simulation Time for shutdown.` |
| THR-014 | `.ShutdownElapsedSeconds` | Error | `bNeverShutsDown` is false, elapsed mode, and value is nonfinite or negative | `Shutdown elapsed time must be finite and nonnegative.` |
| THR-015 | `.ShutdownUtc` | Error | `bNeverShutsDown` is false, absolute mode, and value is invalid or not convertible by SPICE | `Enter a valid shutdown UTC time.` |
| THR-016 | firing window | Error | Finite shutdown is not strictly after ignition | `Shutdown time must be after ignition time.` |
| THR-017 | firing window | Warning | Window has no overlap with the simulation interval | `Thruster firing window does not overlap the simulation interval; this thruster will not fire.` |
| THR-018 | `.PropellantComponentName` | Warning | Propellant initial mass equals minimum mass | `Propellant component '{name}' starts at its depletion floor; this thruster cannot fire.` |
| THR-019 | `.MaximumThrustNewtons` | Error | Commanded mode and nonfinite or not positive | `Maximum commanded thrust must be finite and greater than zero.` |
| THR-020 | commanded mode | Warning | Control mode is `None` | `Commanded thruster receives zero throttle because no user controller is selected.` |

For prescribed mode, clear/ignore `MaximumThrustNewtons` during conversion. For
commanded mode, clear/ignore both prescribed profiles during conversion. Stale
inactive values are not errors.

### 9.1 Constant Profile Checks

| Code | Path | Severity | Condition | Message |
|---|---|---|---|---|
| PRO-001 | `.Source` | Error | Unsupported enum | `Select Constant or CSV Profile.` |
| PRO-002 | thrust `.ConstantValue` | Error | Nonfinite or negative | `Constant thrust must be finite and nonnegative.` |
| PRO-003 | Isp `.ConstantValue` | Error | Nonfinite or not positive | `Constant specific impulse must be finite and greater than zero.` |

### 9.2 CSV Profile Checks

Blank lines may be ignored. Headers are not accepted.

| Code | Severity | Condition | Message |
|---|---|---|---|
| CSV-PRO-001 | Error | Empty path | `Select a CSV profile file.` |
| CSV-PRO-002 | Error | File missing | `CSV profile file does not exist: {path}` |
| CSV-PRO-003 | Error | File unreadable | `Could not read CSV profile file: {path}` |
| CSV-PRO-004 | Error | Nonempty row has other than two columns | `CSV line {line} must contain exactly two comma-separated values.` |
| CSV-PRO-005 | Error | Time or value is nonnumeric/nonfinite | `CSV line {line} contains an invalid numeric value.` |
| CSV-PRO-006 | Error | Time is negative | `CSV line {line} has a negative time. Profile times are measured from ignition.` |
| CSV-PRO-007 | Error | Time is not strictly greater than preceding sample | `CSV line {line} must have a time greater than the previous sample time.` |
| CSV-PRO-008 | Error | No samples | `CSV profile file contains no samples.` |
| CSV-PRO-009 | Error | Thrust value is negative | `CSV line {line} has negative thrust. Thrust samples must be nonnegative.` |
| CSV-PRO-010 | Error | Thrust profile has fewer than two samples | `A thrust CSV profile needs at least two samples so both endpoints can be zero.` |
| CSV-PRO-011 | Error | First or last thrust magnitude exceeds `1e-12` | `The first and last thrust samples must both be zero.` |
| CSV-PRO-012 | Error | Isp value is not positive | `CSV line {line} has non-positive specific impulse.` |
| CSV-PRO-013 | Error | Selected path does not have a `.csv` extension | `Selected profile must be a .csv file.` |

## 10. Reaction Wheels

Call `ValidateReactionWheel` for every row and enforce unique wheel names.

| Code | Path | Severity | Condition | Message |
|---|---|---|---|---|
| WHL-001 | `.Name` | Error | Empty | `Reaction-wheel name cannot be empty.` |
| WHL-002 | `.Name` | Error | Case-insensitive duplicate | `A reaction wheel named '{name}' already exists.` |
| WHL-003 | `.MountComponentName` | Error | Does not resolve | `Select an existing mount component.` |
| WHL-004 | `.Axis` | Error | Nonfinite or norm `<= 1e-12` | `Reaction-wheel axis must be a finite nonzero vector.` |
| WHL-005 | `.Axis` | Warning | Unit-length error exceeds `1e-6` | `Reaction-wheel axis norm is {norm}; it will be normalized before storage.` |
| WHL-006 | `.InitialMomentumNewtonMeterSeconds` | Error | Nonfinite | `Initial wheel momentum must be finite.` |
| WHL-007 | `.MaximumAbsoluteMomentumNewtonMeterSeconds` | Error | Nonfinite or negative | `Maximum absolute wheel momentum must be finite and nonnegative.` |
| WHL-008 | initial momentum | Error | Absolute initial value exceeds maximum by more than `1e-12` | `The absolute initial wheel momentum cannot exceed the maximum absolute wheel momentum.` |
| WHL-009 | wheel row | Warning | Control mode is `None` and maximum momentum is positive | `Reaction-wheel momentum will remain uncommanded because no user controller is selected.` |

## 11. Controller Selection

Call `UTGControlEditingLibrary::ValidateControlSelection` and display its exact
error. Required outcomes are:

| Code | Path | Severity | Condition | Message |
|---|---|---|---|---|
| CTL-001 | `Control.Mode` | Error | Unsupported enum | `The selected control mode is unsupported.` |
| CTL-002 | `Control.ControllerId` | Error | Compiled mode and `None` | `Select a ready user controller.` |
| CTL-003 | controller library | Error | Subsystem unavailable | `The controller library is unavailable.` |
| CTL-004 | `Control.ControllerId` | Error | ID not registered | `The selected controller is not registered.` |
| CTL-005 | selected controller | Error | Not trusted | `The selected controller has not been trusted.` |
| CTL-006 | selected controller | Error | Build status is not Ready | `The selected controller is not in the Ready state.` |
| CTL-007 | selected controller | Error | Managed DLL missing | `The selected controller DLL is missing.` |
| CTL-008 | selected controller | Error | DLL probe/export/API check fails | Use the exact diagnostic returned by `FTGDynamicControllerAdapter::ProbeDll`. |
| CTL-009 | `Control.ControllerId` | Warning | Mode is `None` but a stale ID is stored | `Stored controller selection is ignored because control mode is None.` |

The accepted canonical scenario clears `ControllerId` in `None` mode.

## 12. Gravity and Celestial Catalog

### 12.1 Catalog Structure

Compare the original array against `GetCelestialCatalog()` before calling
`NormalizeCelestialBodyConfigs`.

| Code | Path | Severity | Condition | Message |
|---|---|---|---|---|
| GRV-001 | `CelestialBodies` | Error | A catalog key is `None` or unknown | `Celestial-body row {index} has an unknown catalog key '{key}'.` |
| GRV-002 | `CelestialBodies` | Error | Duplicate key | `Celestial-body catalog key '{key}' appears more than once.` |
| GRV-003 | `CelestialBodies` | Error | A fixed catalog key is missing | `Required celestial-body catalog row '{key}' is missing.` |
| GRV-004 | `CelestialBodies` | Error | Array order differs from fixed catalog order | `Celestial-body rows are not in the fixed catalog order; reopen the Gravity panel to rebuild them.` |
| GRV-005 | `.AutomaticActivationRadiusMeters` | Error | Nonfinite or negative | `Automatic activation radius must be finite and nonnegative.` |
| GRV-006 | `.BarycenterResolutionRadiusMeters` | Error | Barycenter row and nonfinite or negative | `Barycenter resolution radius must be finite and nonnegative.` |
| GRV-007 | `.BarycenterResolutionRadiusMeters` | Error | Non-barycenter row and nonzero | `Only a system barycenter may have a resolution radius.` |
| GRV-008 | harmonic fields | Error | Source does not support harmonics but path or degree is set | `Catalog source '{name}' supports point-mass gravity only; remove its harmonic configuration.` |
| GRV-009 | `.MaximumHarmonicDegreeUsed` | Error | Negative | `Maximum harmonic degree must be nonnegative.` |
| GRV-010 | one system | Error | Barycenter and one or more members are explicitly enabled | `Gravity system '{system}' selects both its barycenter and physical members; select only one representation.` |
| GRV-011 | `CelestialBodies` | Warning | No source can become active | `No gravitational source is enabled and every automatic activation radius is zero; gravity will be absent.` |
| GRV-012 | `GravitySettings.bIncludeFirstPostNewtonianCorrection` | Warning | 1PN enabled but no source can become active | `The 1PN option has no effect because no gravitational source can become active.` |

"Can become active" means explicitly enabled or has a positive automatic
activation radius. A resolvable active barycenter also makes all fixed system
members potentially required.

### 12.2 Harmonic CSV

When `MaximumHarmonicDegreeUsed > 0`, the path is required and
`InspectHarmonicModelCsv` must succeed. Prefix each parser message with the
body path, for example:

```text
[ERROR] CelestialBodies[12] ('Earth').HarmonicModelCsvFilePath: Line 7: order m cannot exceed degree n.
```

Mandatory parser checks and exact messages are:

| Code | Condition | Message |
|---|---|---|
| HGM-001 | Empty path | `Select a harmonic-model CSV when maximum harmonic degree is positive.` |
| HGM-002 | Missing/unreadable | `The harmonic-model file could not be opened or read.` |
| HGM-003 | No nonempty rows | `The file contains no nonempty rows.` |
| HGM-004 | First row is not exactly two columns | `Line {line}: the first nonempty row must contain exactly GM and reference radius.` |
| HGM-005 | GM is nonnumeric, nonfinite, or not positive | `Line {line}: model GM must be a positive finite value in m^3/s^2.` |
| HGM-006 | Radius is nonnumeric, nonfinite, or not positive | `Line {line}: model reference radius must be a positive finite value in meters.` |
| HGM-007 | Coefficient row is not four columns | `Line {line}: coefficient rows must contain exactly n,m,Cbar_nm,Sbar_nm.` |
| HGM-008 | Degree is not a strict integer | `Line {line}: degree n must be an integer.` |
| HGM-009 | Order is not a strict integer | `Line {line}: order m must be an integer.` |
| HGM-010 | Cbar nonnumeric/nonfinite | `Line {line}: Cbar_nm must be a finite number.` |
| HGM-011 | Sbar nonnumeric/nonfinite | `Line {line}: Sbar_nm must be a finite number.` |
| HGM-012 | Degree negative | `Line {line}: degree n cannot be negative.` |
| HGM-013 | Order negative | `Line {line}: order m cannot be negative.` |
| HGM-014 | `m > n` | `Line {line}: order m cannot exceed degree n.` |
| HGM-015 | Explicit `(0,0)` | `Line {line}: an explicit coefficient (0,0) is not permitted.` |
| HGM-016 | Duplicate pair | `Line {line}: duplicate coefficient pair ({n},{m}).` |
| HGM-017 | Requested degree exceeds file maximum | `Requested maximum degree {requested} exceeds file maximum degree {available}.` |
| HGM-018 | No coefficient survives truncation at requested positive degree | `No harmonic coefficient row exists at or below requested degree {requested}; select point-mass degree zero or a supported degree.` |

A nonempty path with degree zero is allowed and retained for later reuse. It is
not parsed for run readiness until the user selects a positive degree.

For harmonic gravity, the CSV's first-row GM and reference radius are the
gravity model's authoritative values. A difference from SPICE GM or physical
shape radii is not an error and must not be silently overwritten. SPICE radii
remain separate physical-shape data for atmosphere altitude and eclipse disks.

## 13. Solar Radiation Pressure

Use `ValidateSolarRadiationPressureAuthoring` for Review Panel acceptance.
Optical weights must already have been normalized by the SRP input widget; the
stored values are final fractions and must pass `IsValidOpticalProperties`.

### 13.1 Fixed and Global Fields

| Code | Path | Severity | Condition | Message |
|---|---|---|---|---|
| SRP-001 | `SolarRadiationPressure.SunBodyName` | Warning | SRP enabled and not `Sun` | `Stored Sun source is ignored; the converter always uses Sun.` |
| SRP-002 | `.PressureAtOneAstronomicalUnitPascals` | Warning | SRP enabled and value differs from `4.5391e-6` or is nonfinite | `Stored solar pressure is ignored; the converter uses 4.5391e-6 Pa at one AU.` |
| SRP-004 | `.GlobalFallbackOpticalProperties` | Error | SRP enabled, at least one included component uses the fallback, and any fraction is nonfinite/outside `[0,1]` or the sum differs from one by more than `1e-6` | `Global fallback optical properties: {reason}` |
| SRP-005 | component list | Error | SRP enabled and no component is included | `SRP is enabled, but no component is included in the SRP proxy.` |
| SRP-006 | `.bComputeEclipse` | Warning | SRP enabled and false | `Celestial-body eclipse attenuation is disabled; every SRP facet will be treated as fully sunlit unless component shadowing blocks it.` |
| SRP-007 | `.bComputeComponentShadows` | Warning | SRP enabled and false | `Mutual component shadowing is disabled; overlapping components may cause SRP to be overestimated.` |

Canonical conversion sets Sun, pressure, and the empty occulter list exactly as
shown above.

### 13.2 Component SRP Authoring

Apply these checks only when SRP is enabled and `bIncludedInProxy` is true.
Override-array errors apply only when
`bApplyOneOpticalConfigurationToEntireComponent` is false; otherwise the
stored arrays are inactive and only `SRP-C13` is shown.

| Code | Path | Severity | Condition | Message |
|---|---|---|---|---|
| SRP-C01 | component geometry | Error | No valid source geometry | `Component '{name}' has no source geometry for its SRP proxy.` |
| SRP-C02 | `.ProxyResolutionMode` | Error | Unsupported enum | `Component '{name}' has an unsupported SRP proxy-resolution mode.` |
| SRP-C03 | `.CustomTargetTriangleCount` | Error | Custom mode and count `<= 0` | `Component '{name}' custom target triangle count must be positive.` |
| SRP-C04 | `.ComponentOpticalProperties` | Error | Global fallback disabled and stored fractions invalid | `Component '{name}' optical properties: {reason}` |
| SRP-C05 | `.LogicalRegionOverrides` | Error | Region unsupported by selected geometry | `Component '{name}' has an override for a logical region its geometry does not support.` |
| SRP-C06 | same | Error | Duplicate region | `Component '{name}' has duplicate logical-region overrides.` |
| SRP-C07 | region properties | Error | Invalid fractions | `Component '{name}' logical-region override: {reason}` |
| SRP-C08 | `.TriangleOverrides` | Error | Primitive box or cylinder has triangle overrides | `Component '{name}' uses primitive geometry; use logical-region overrides instead of triangle overrides.` |
| SRP-C09 | same | Error | Sphere has any triangle override | `Component '{name}' is a sphere and cannot use triangle overrides.` |
| SRP-C10 | same | Error | Duplicate stable index | `Component '{name}' has duplicate triangle override index {index}.` |
| SRP-C11 | same | Error | Index is negative | `Component '{name}' triangle override index {index} cannot be negative.` |
| SRP-C12 | triangle properties | Error | Invalid fractions | `Component '{name}' triangle override: {reason}` |
| SRP-C13 | overrides | Warning | `bApplyOneOpticalConfigurationToEntireComponent` is true and override arrays are nonempty | `Component '{name}' applies one optical configuration; stored region and triangle overrides are currently ignored.` |
| SRP-C14 | base optics | Warning | Uses global fallback | `Component '{name}' uses the global fallback optical properties.` |
| SRP-C15 | `.TriangleOverrides` | Error | Proxy is current and an index is at least `GeneratedTriangleCount` | `Component '{name}' triangle override index {index} is outside the generated proxy.` |

Use the exact optical-property reasons returned by
`IsValidOpticalProperties`:

```text
all fractions must be finite.
every fraction must lie in [0, 1].
fractions must sum to 1 (current sum {sum}).
```

Generated signatures, triangle counts, `bProxyGenerationRequired`, status
messages, and `OpticalFacets` are derived cache/output values. During authoring
review, show their status as information only. Do not reject an otherwise valid
scenario merely because preprocessing has not run.

## 14. Atmosphere and Aerodynamics

### 14.1 Atmosphere

When atmosphere is disabled, its model-specific values are inactive.

| Code | Path | Severity | Condition | Message |
|---|---|---|---|---|
| ATM-001 | `.CentralBodyName` | Error | Enabled and does not resolve to a physical non-barycenter catalog body | `Atmosphere central body must resolve to a physical celestial-body catalog entry.` |
| ATM-002 | `.Model` | Error | Unsupported enum | `Select Uploaded Profile or Cubic Harris-Priester.` |
| ATM-003 | CHP central body | Error | Not Earth | `Cubic Harris-Priester is Earth-only; select Earth as the atmosphere central body.` |
| ATM-004 | `.CenteredAverageF107SolarFluxUnits` | Error | CHP and nonfinite or not positive | `Centered 81-day average F10.7 must be a positive finite value.` |
| ATM-005 | `.GeneralProfileCsvPath` | Error | Uploaded-profile mode and CSV validation fails | Use the exact uploaded-profile CSV error below. |
| ATM-006 | `.ChpCoefficientCsvPath` | Error | CHP mode and coefficient CSV validation fails | Use the exact CHP coefficient CSV error below. |
| ATM-007 | `.ChpMolecularProfileCsvPath` | Error | CHP mode and molecular CSV validation fails | Use the exact CHP molecular-profile CSV error below. |

All atmosphere CSV paths must be nonempty, use `.csv`, exist, and be readable.
Common file messages are:

```text
Select a CSV file.
The selected file must have a .csv extension.
CSV file does not exist: {path}
The CSV file could not be read.
CSV row {row} has {actual} columns; expected {expected}.
CSV row {row}, column {column} is not a finite number.
```

General uploaded profile checks:

Every row has exactly five columns, in this order:
`altitude_m,density_kgpm3,temperature_k,mean_particle_mass_kg,effective_collision_cross_section_m2`.

| Code | Condition | Message |
|---|---|---|
| ATM-G01 | Fewer than two nonempty rows | `The uploaded atmosphere profile needs at least two nonempty rows.` |
| ATM-G02 | Altitude not strictly increasing | `Atmosphere profile altitudes must be strictly increasing; row {row} is not above the preceding altitude.` |
| ATM-G03 | Density, temperature, particle mass, or cross-section is not positive | `Atmosphere profile row {row} contains a nonpositive gas-property value in column {column}.` |

CHP coefficient checks:

Every row has exactly eight finite cubic-envelope coefficients. The selected
CHP implementation fixes the coefficient interpretation; no header row or
extra identifying column is accepted.

| Code | Condition | Message |
|---|---|---|
| ATM-C01 | Row count is not exactly 50 | `The Cubic Harris-Priester coefficient CSV must contain exactly 50 nonempty rows; found {count}.` |
| ATM-C02 | A row is not eight finite values | Use the common row/column message above. |
| ATM-C03 | At selected F10.7, a row does not satisfy `0 < rho_min <= rho_max` | `CHP coefficient row {row} does not evaluate to 0 < rho_min <= rho_max at the selected F10.7.` |
| ATM-C04 | Evaluated min or max envelope fails to decrease strictly with altitude | `CHP density envelopes must decrease strictly with altitude; row {row} violates the ordering.` |

CHP molecular-profile checks:

Every row has exactly four columns, in this order:
`altitude_m,temperature_k,mean_particle_mass_kg,effective_collision_cross_section_m2`.

| Code | Condition | Message |
|---|---|---|
| ATM-M01 | Fewer than two rows | `The CHP molecular profile needs at least two nonempty rows.` |
| ATM-M02 | Altitude not strictly increasing | `CHP molecular-profile altitudes must be strictly increasing; row {row} is not above the preceding altitude.` |
| ATM-M03 | Temperature, particle mass, or cross-section is not positive | `CHP molecular-profile row {row} contains a nonpositive property value in column {column}.` |

### 14.2 Aerodynamics

When aerodynamics is disabled, all aerodynamic fields are inactive.

| Code | Path | Severity | Condition | Message |
|---|---|---|---|---|
| AER-001 | `Aerodynamics.bEnabled` | Error | Enabled while atmosphere is disabled | `Aerodynamics requires Atmosphere to be enabled.` |
| AER-002 | `.ReferenceAreaSquareMeters` | Error | Nonfinite or not positive | `Aerodynamic reference area must be a positive finite value.` |
| AER-003 | `.ReferenceLengthMeters` | Error | Nonfinite or not positive | `Aerodynamic reference length must be a positive finite value.` |
| AER-004 | `.MinimumDynamicPressurePascals` | Error | Nonfinite or negative | `Minimum dynamic pressure must be a nonnegative finite value.` |
| AER-005 | `.MaximumValidDynamicPressurePascals` | Error | Nonfinite or negative | `Maximum valid dynamic pressure must be a nonnegative finite value.` |
| AER-006 | dynamic-pressure limits | Error | Maximum is below minimum | `Maximum valid dynamic pressure must be greater than or equal to minimum dynamic pressure.` |
| AER-007 | model selection | Error | Database and fallback both disabled | `Enable the aerodynamic coefficient database, the constant-drag fallback, or both.` |
| AER-008 | `.FallbackDragCoefficient` | Error | Fallback enabled and value nonfinite or not positive | `Fallback drag coefficient must be a positive finite value.` |
| AER-009 | database extrapolation | Error | Constant-drag extrapolation selected while fallback disabled | `Database extrapolation is set to Constant-Drag Fallback, but the constant-drag fallback is disabled.` |
| AER-010 | fallback | Warning | Enabled | `Constant-drag fallback is translation-only: it produces no aerodynamic attitude torque or articulated-joint load.` |
| AER-011 | aggregate database | Warning | Enabled and spacecraft has one or more articulation DOFs | `The aggregate aerodynamic database supplies total force and moment but no uniquely resolved aerodynamic generalized joint loads for {count} articulation DOF(s).` |

### 14.3 Aerodynamic Database CSV

Let `N` be the flattened articulation DOF count. Every nonempty row must have
exactly `N + 11` finite columns.

| Code | Condition | Message |
|---|---|---|
| ADB-001 | Empty database path | `Select an aerodynamic coefficient CSV file.` |
| ADB-002 | No nonempty rows | `The aerodynamic coefficient database contains no data rows.` |
| ADB-003 | Wrong column count | `CSV row {row} has {actual} columns; expected {N+11}.` |
| ADB-004 | Molecular speed ratio `< 0` | `Aerodynamic database row {row} has a negative molecular speed ratio s.` |
| ADB-005 | Knudsen number `<= 0` | `Aerodynamic database row {row} must have Kn > 0.` |
| ADB-006 | Flow direction nonfinite or norm `<= 1e-12` | `Aerodynamic database row {row} has a zero or invalid gas-flow direction.` |
| ADB-007 | Flow norm differs from one by more than `1e-6` | `Aerodynamic database row {row} gas-flow direction is not normalized; norm = {norm}.` |
| ADB-008 | Any eta or coefficient value nonfinite | `Aerodynamic database row {row}, column {column} is not a finite number.` |
| ADB-009 | Duplicate independent-variable point within backend tolerance | `Aerodynamic database row {row} duplicates an earlier independent-variable point.` |
| ADB-010 | Varying domain has fewer than intrinsic dimension plus one independent rows | `Aerodynamic database needs at least {minimum} independent rows for its varying coordinates.` |
| ADB-011 | Rows do not independently span every varying coordinate | `Aerodynamic database rows do not independently span all varying speed-ratio, Kn, direction, and eta coordinates.` |
| ADB-012 | Inverse-distance interpolation and `.NeighborCount < 0` | `Aerodynamic database neighbor count must be nonnegative.` |
| ADB-013 | Inverse-distance interpolation and neighbor count exceeds row count | `Aerodynamic neighbor count {neighbors} exceeds the {rows} available rows; it will be clamped.` |
| ADB-014 | Inverse-distance interpolation and power is nonfinite or not positive | `Inverse-distance power must be a positive finite value.` |
| ADB-015 | Enabled maximum normalized neighbor distance nonfinite or not positive | `Maximum normalized neighbor distance must be a positive finite value.` |
| ADB-016 | Moment reference center nonfinite | `Aerodynamic moment reference center must contain finite B-frame values.` |
| ADB-017 | Interpolation enum is unsupported | `Select Inverse Distance or Nearest Row database interpolation.` |
| ADB-018 | Extrapolation enum is unsupported | `Select Constant-Drag Fallback or Nearest Row database extrapolation.` |

When the maximum-neighbor-distance option is disabled, the converter writes
positive infinity to the backend field. When neighbor count is zero, the
backend chooses its automatic count.

`Database.Rows` is only a parsed cache. The selected CSV is authoritative at
review and conversion time. Reparse it and replace the cache; never accept
different cached rows in preference to the file.

## 15. Cross-Panel and Preflight Checks

### 15.1 References and Canonical Names

All component-name references are resolved case-insensitively during review,
then replaced by the exact canonical component name in the accepted copy.
Ambiguous resolution is impossible only after component uniqueness passes.

### 15.2 Variable-Mass Ownership

Every thruster must reference a variable-mass component. Multiple thrusters may
share one propellant component; their mass-flow rates are summed. A
variable-mass component referenced by no thruster is valid but should warn:

```text
[WARNING] Components[{i}].bVariableMass: Variable-mass component '{name}' is not assigned to any thruster; its mass will remain constant.
```

### 15.3 Controller/Actuator Consistency

Commanded thrusters receive zero throttle without a controller, and wheels
receive zero momentum-rate command. These are warnings, not errors. Prescribed
thrusters remain active without a controller.

### 15.4 Atmosphere/Aerodynamics Consistency

Aerodynamics requires atmosphere. The atmosphere central body need not have
its gravity enabled, but it must remain in the fixed catalog so SPICE can
provide its state, radius, frame, and angular velocity.

At the initial epoch, compute initial altitude when possible:

| Severity | Condition | Message |
|---|---|---|
| Error | Spacecraft initial point lies at or below the central body's physical surface | `Initial spacecraft position is inside or on the atmosphere central body's SPICE physical radius.` |
| Warning | Uploaded atmosphere profile does not cover initial altitude | `Initial altitude {altitude} m is outside the uploaded atmosphere interval [{min}, {max}] m; atmosphere and aerodynamic loads initially evaluate to zero.` |
| Warning | CHP molecular profile does not cover initial altitude | `Initial altitude {altitude} m is outside the CHP molecular-profile interval [{min}, {max}] m; aerodynamic molecular properties are initially unavailable.` |
| Warning | Initial dynamic pressure is below the configured aerodynamic minimum | `Initial dynamic pressure {q} Pa is below the configured minimum {minimum} Pa; aerodynamic loads initially evaluate to zero.` |
| Warning | Initial dynamic pressure exceeds the configured validity maximum | `Initial dynamic pressure {q} Pa exceeds the configured aerodynamic validity limit {maximum} Pa; initial aerodynamic results are outside the accepted model range.` |

Compute this initial dynamic pressure with the selected atmosphere model and
the atmosphere-relative velocity used by TGSimCore, including central-body
rotation and no wind. Skip the two dynamic-pressure warnings when aerodynamics
is disabled or the atmosphere cannot be evaluated at the initial altitude.

### 15.5 Gravity Double-Counting

Explicit member/barycenter conflicts are errors in the HUD even though
TGSimCore contains a final safety normalization. Automatic activation is not a
stored checkbox conflict and must not be reported as one.

### 15.6 SPICE Preflight

The Review Panel must call `FSpiceBridge::LoadKernels` once, convert start/final
UTC to ET, and inspect the fixed catalog identities required by the scenario.

| Code | Severity | Condition | Message |
|---|---|---|---|
| SPC-001 | Error | Kernel load fails | `SPICE kernels could not be loaded: {diagnostic}` |
| SPC-002 | Error | UTC-to-ET conversion fails | `SPICE could not convert {field} UTC to ephemeris time: {diagnostic}` |
| SPC-003 | Error | `RequiresCompactMoonCatalogInterval` is true and interval is outside `[2000-01-01, 2050-01-01)` | `The selected moon/barycenter configuration requires a simulation interval from 2000-01-01 inclusive to 2050-01-01 exclusive.` |
| SPC-004 | Error | Required body state unavailable at start or final ET | `SPICE cannot resolve body '{name}' across the requested simulation interval.` |
| SPC-005 | Error | GM metadata unavailable or GM not positive | `SPICE cannot resolve a positive gravitational parameter for '{name}'.` |
| SPC-006 | Error | Harmonics enabled and body-fixed frame unavailable/invalid at start | `SPICE cannot resolve the body-fixed frame required by harmonic gravity for '{name}'.` |
| SPC-007 | Error | Atmosphere body radius is unavailable/not positive | `SPICE cannot resolve a positive physical radius for atmosphere body '{name}'.` |
| SPC-008 | Error | Atmosphere body frame or angular velocity unavailable at start | `SPICE cannot resolve the body-fixed frame and angular velocity required for atmosphere body '{name}'.` |
| SPC-009 | Error | SRP enabled and Sun state/radius unavailable | `SPICE cannot resolve the Sun state and physical radius required by SRP.` |

Do not require a body-fixed frame for a point-mass-only source. Do not require a
shape radius for a pure point-mass gravity source.

### 15.7 Initial Singularities

Using start-epoch SPICE states:

| Severity | Condition | Message |
|---|---|---|
| Error | Initial spacecraft position coincides numerically with an active point-mass source | `Initial spacecraft position coincides with gravity source '{name}', where point-mass gravity is singular.` |
| Warning | Initial spacecraft position lies inside a physical body's SPICE radius | `Initial spacecraft position lies inside physical body '{name}'; the selected spacecraft dynamics model is not intended for interior-body motion.` |

## 16. Converter Run-Readiness Checks

These checks occur after authoring acceptance and do not belong to editable HUD
fields.

### 16.1 SRP Proxy Completion

Call `ValidateSolarRadiationPressureScenario` with final-proxy validation.
Use these exact failures:

| Code | Condition | Message |
|---|---|---|
| SRP-F01 | Included component still requires generation | `Final converter output is pending for component '{name}'.` |
| SRP-F02 | Included component generated no triangles | `Final converter output for component '{name}' contains no SRP proxy triangles.` |
| SRP-F03 | SRP enabled and aggregate facet array is empty | `SRP is enabled, but final converter output contains no optical facets.` |
| SRP-F04 | Facet component identity does not resolve | `Generated facet '{facet}' references no component.` |
| SRP-F05 | Any facet vertex is nonfinite | `Generated facet '{facet}' has a non-finite vertex.` |
| SRP-F06 | Triangle area is zero within converter tolerance | `Generated facet '{facet}' is degenerate.` |
| SRP-F07 | Resolved facet fractions are invalid | `Generated facet '{facet}' optical properties: {reason}` |
| SRP-F08 | Stable index is duplicated inside a component | `Component '{name}' has duplicate stable triangle index {index}.` |
| SRP-F09 | Stored geometry signature differs from current source/resolution signature | `Component '{name}' SRP proxy is stale because its geometry or resolution changed.` |
| SRP-F10 | Facet count differs from `GeneratedTriangleCount` | `Component '{name}' reports {reported} generated triangles but contains {actual} finalized facets.` |
| SRP-F11 | Stable indices are not exactly contiguous `0..count-1` | `Component '{name}' SRP stable triangle indices are incomplete or out of sequence.` |
| SRP-F12 | A facet ID resolves to a different included component | `Generated facet '{facet}' is assigned to the wrong component identity.` |
| SRP-F13 | Proxy orientation check detects inconsistent or inward winding | `Component '{name}' SRP proxy has inconsistent or inward-facing triangle winding.` |
| SRP-F14 | Proxy generation emits a detected duplicate/internal surface | `Component '{name}' SRP proxy contains a duplicate or internal surface.` |
| SRP-F15 | Resolved facet optics differ from override precedence | `Generated facet '{facet}' does not match triangle, logical-region, component, and global optical precedence.` |
| SRP-F16 | More than 32 generated triangles and triangle overrides remain without confirmed discard | `Component '{name}' has more than 32 generated triangles; confirm discarding its triangle overrides before conversion.` |

Warn, but do not reject, when total generated SRP triangles exceed 2,000:

```text
The SRP proxy contains {count} triangles, above the soft performance threshold of 2,000.
```

If a component has more than 32 generated triangles, triangle overrides are
disabled. Existing incompatible overrides must be explicitly discarded before
conversion, not silently applied.

### 16.2 Canonical Backend Request

Before passing the request to `SimulationEngine::Run`, apply these checks to
the converted request:

| Code | Condition | Message |
|---|---|---|
| CNV-001 | Start/final ET is nonfinite or disagrees with the accepted UTC interval | `Converter produced an invalid SPICE ET/TDB simulation interval.` |
| CNV-002 | Component array is not parent-before-child | `Converter produced a component tree that is not in parent-before-child order.` |
| CNV-003 | A converted quaternion is nonfinite, nonunit, or fails a HUD-to-backend round trip | `Converter produced an invalid backend quaternion for {field}.` |
| CNV-004 | A converted vector/rotation fails the documented Unreal-to-backend frame round trip | `Converter frame conversion failed for {field}; check the Unreal Y-axis reflection.` |
| CNV-005 | Variable-mass indices are missing, duplicated, or not contiguous | `Converter produced an invalid variable-mass component index mapping.` |
| CNV-006 | Eta or eta-dot length/order differs from component/DOF order | `Converter produced an articulation-state array inconsistent with the component tree.` |
| CNV-007 | Wheel momentum length/order differs from reaction-wheel order | `Converter produced a reaction-wheel momentum array inconsistent with the wheel list.` |
| CNV-008 | An actuator mount or propellant component name does not resolve | `Converter could not resolve component reference '{name}' for {actuator}.` |
| CNV-009 | A thruster does not map to a variable-mass component | `Converter linked thruster '{name}' to a non-variable-mass propellant component.` |
| CNV-010 | Prescribed curve is not linear and endpoint-clamped after parsing | `Converter produced a noncanonical prescribed profile for thruster '{name}'.` |
| CNV-011 | Commanded thruster contains a prescribed curve or lacks positive maximum thrust | `Converter produced contradictory commanded-thruster data for '{name}'.` |
| CNV-012 | Fixed celestial catalog metadata is missing or differs from the catalog | `Converter produced invalid immutable metadata for celestial source '{name}'.` |
| CNV-013 | SPICE provider is absent | `Converter did not attach the required SPICE ephemeris provider.` |
| CNV-014 | Harmonic row degree exceeds the selected maximum | `Converter failed to truncate harmonic model '{name}' to degree {degree}.` |
| CNV-015 | An active atmosphere CSV was not parsed into canonical rows | `Converter failed to produce canonical atmosphere data from '{path}'.` |
| CNV-016 | An active aerodynamic CSV was not parsed into canonical rows | `Converter failed to produce canonical aerodynamic data from '{path}'.` |
| CNV-017 | A disabled/inactive option retains contradictory backend data | `Converter did not clear inactive backend data for {field}.` |
| CNV-018 | SRP is enabled but finalized triangle/fraction data is absent or invalid | `Converter did not produce a valid finalized SRP facet array.` |
| CNV-019 | Disabled joint coordinate bounds were not converted to negative/positive infinity | `Converter produced an invalid disabled coordinate bound for joint DOF '{name}'.` |

TGSimCore still performs authoritative validation when the run starts. Any
backend rejection should be shown verbatim and treated as a converter/setup
defect if the Review Panel had accepted the same snapshot.

## 17. Review Panel Presentation

The panel should show:

```text
Scenario: <trimmed name>
Status: Ready / Errors / Ready with warnings
Errors: <count>
Warnings: <count>
```

Group issues in this order:

1. Scenario and Solver
2. Initial State
3. Components and Joints
4. Actuators
5. Controller
6. Gravity
7. Solar Radiation Pressure
8. Atmosphere
9. Aerodynamics
10. Cross-System and SPICE

Selecting an issue should reopen the owning configuration panel and, where
possible, select the referenced component, DOF, actuator, body, or file field.
Never truncate the stored issue string. A compact list may ellipsize visually,
but the complete message must remain available in a details area or tooltip.

The Accept/Continue button is disabled while validation is running or while
any error exists. It is enabled with warnings after acknowledgement. Validation
must rerun whenever the draft revision changes; results from an older snapshot
must never authorize a newer draft.
