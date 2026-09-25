# HUD Input Specification

This document is the complete user-input contract represented by
`FTGSimulationScenario`. The HUD may organize these values into tabs or
step-by-step panels, but it must preserve the units, frames, ordering, and
conditional rules below.

The conversion boundary is now implemented. `TGScenarioDocumentAdapter` maps
between `FTGSimulationScenario` and the portable `ScenarioDocument`, while the
shared pure-C++ `ScenarioCompiler` creates `tgsim::SimulationRequest`. No
physics is implemented in either adapter. The same `ScenarioDocument` is saved
as `.tgscn` for HUD import/export and standalone runs; see
`Docs/Reference/TGSCN.md`. Exact Blueprint/native integration and Run Simulation
wiring are documented in `Docs/Development/ApplicationIntegration.md`.

## Global Conventions

| Quantity | Backend convention |
|---|---|
| Length | meter |
| Mass | kilogram |
| Time interval | second |
| Epoch | SPICE ET, numerically TDB seconds past J2000 |
| Angle | radian |
| Force | newton |
| Torque | newton-meter |
| Inertia | kilogram-meter squared |
| Translation frame | barycentric J2000/ICRF |
| Spacecraft attitude | quaternion rotating B components into ICRF |
| Quaternion order | `[w,x,y,z]` |
| Angular velocity and torque | spacecraft main-body frame B |
| Component geometry | each component's local axes |
| Matrix storage | row-major in the C++ structures and result table |

The HUD may display kilometers, degrees, UTC, or other convenient units. The
converter must transform them to the conventions above exactly once.

## 1. Scenario and Solver

| HUD input | Type | Backend field | Rule/default |
|---|---|---|---|
| Scenario name | text | `scenario_name` | Default `Untitled Scenario` |
| Simulation kind | option | `simulation_kind` | Currently only `Spacecraft6Dof` |
| Start UTC | UTC text/date-time | converted to `start_ephemeris_time_tdb_seconds` | Required |
| Final UTC or duration | UTC/date-time or seconds | converted to `final_ephemeris_time_tdb_seconds` | Must not precede start |
| Integrator | option | `integrator_kind` | `FixedStepRK4` or `AdaptiveDormandPrince54` |
| Maximum integrator step | positive scalar [s] | `maximum_integrator_step_seconds` | Step for RK4; upper bound for adaptive |
| Initial adaptive step | positive scalar [s] | `initial_integrator_step_seconds` | Initial Dopri5 guess |
| Absolute tolerance | positive scalar | `absolute_tolerance` | Adaptive integrator |
| Relative tolerance | positive scalar | `relative_tolerance` | Adaptive integrator |
| Output mode | option | `output_mode` | Every integrator step or fixed interval |
| Output interval | positive scalar [s] | `output_step_seconds` | Required in fixed-interval mode |
| Maximum integration attempts | positive integer | `maximum_integration_steps` | Runaway protection |
| Maximum stored samples | positive integer | `maximum_output_samples` | Memory protection |
| Maximum real backend runtime | positive scalar [s] | frontend execution policy; not copied to `SimulationRequest` | Hard child-process deadline; default 300 s |
| Variable-mass convention | option | `mass_flow_convention` | Normally `ThrustIncludesExhaustMomentum` |

The engine automatically shortens a step at thruster ignition/shutdown and at
every configured profile knot. Output times are also exact step boundaries.

## 2. Initial Spacecraft State

| HUD input | Type and unit | Frame |
|---|---|---|
| Initial total-CM position | 3-vector [m] | ICRF |
| Initial total-CM velocity | 3-vector [m/s] | ICRF |
| Initial attitude | quaternion or Euler-angle editor | B to ICRF |
| Initial angular velocity | 3-vector [rad/s] | B |

The following backend state arrays should normally be derived by the converter,
not entered as separate flattened HUD arrays:

| Derived state | Derivation |
|---|---|
| `mass_kg` | Sum of current component masses |
| `variable_component_masses_kg` | Each variable component's initial mass |
| `articulation_coordinates` | Every DOF's initial coordinate in component/DOF order |
| `articulation_rates` | Every DOF's initial rate in the same order |
| `internal_angular_momenta_nms` | Each wheel's initial stored momentum |
| `ephemeris_time_tdb_seconds` | Simulation start ET |

## 3. Component Tree, Kinematics, and Visual Playback

Every user-defined component is a physical rigid body with mass, local CM,
centroidal inertia, and one pose in the joint-connected tree. Component index
zero is the unique root. Every later component names an earlier parent, so the
array is also a parent-before-child traversal order.

Thrusters and reaction wheels are behavior records mounted on physical
components; they are not additional bodies and contribute no mass or inertia
by themselves. The intended spacecraft roles are assembled as follows:

- A structure component has no actuator behavior mounted on it.
- An actuator component is a normal physical component with one or more
  thruster or reaction-wheel records mounted on it.
- A tank is a normal variable-mass component. Its minimum mass is its dry-mass
  floor. Its centroidal inertia automatically follows the TG linear model
  \(I_C(t)=m(t)I_{C,0}/m_0\).

For a detailed model, create the engine or wheel assembly as a physical
component and mount its behavior there. Mounting an actuator record directly
on the bus is a valid lumped model only if that actuator hardware's mass and
inertia are already included in the bus component.

The backend internally expands compound joints into massless scalar ABA nodes.
Those nodes are an implementation detail and must never appear in the HUD
component tree.

### 3.1 Frames and Transform Notation

The kinematic equations use the following definitions:

| Symbol | Definition |
|---|---|
| `B` | Spacecraft body reference frame used by rotational dynamics |
| `P` | Local frame of a child component's physical parent |
| `C` | Local frame of the child component |
| `J_i` | Intermediate joint frame after DOFs `0` through `i-1` |
| \(R_{A C}\) | Active rotation matrix mapping vector components from `C` into `A`: \(v^A=R_{A C}v^C\) |
| \(s^A_{A C}\) | Position of origin `C` measured from origin `A`, expressed in `A` |
| \(r^P_{P a}\) | Parent-anchor position from parent origin, in `P` |
| \(r^C_{C a}\) | Child-anchor position from child origin, in `C` |
| \(R^0_{P C}\) | Child-to-parent active orientation when all joint coordinates are zero |

All component-local and B vectors use the backend's right-handed Cartesian
algebra. The names `x`, `y`, and `z` have no imposed spacecraft meaning; the
user decides which direction is forward, starboard, or upward when defining B
and each component frame.

### 3.2 Exact Main-Component Transform

The HUD asks the following only for component zero:

| HUD input | Type and unit | Exact meaning |
|---|---|---|
| Origin in B | 3-vector [m] | \(s^B_{B0}\), position of the main-component origin measured from the B origin and expressed in B |
| Component-to-B orientation | quaternion | \(R_{B0}\), active rotation mapping main-component vectors into B |

For any point with component-zero coordinates \(r^0\),

\[
r^B=s^B_{B0}+R_{B0}r^0.
\]

Normally both inputs are zero/identity, making the main-component frame
coincident with B, but the backend does not require coincidence.

### 3.3 Every Physical Component

| HUD input | Type and unit | Meaning |
|---|---|---|
| Component name | unique text | Used by HUD references and result columns |
| Initial mass | nonnegative scalar [kg] | Wet/current mass at start; an individual component may be massless |
| Minimum mass | nonnegative scalar [kg] | Dry mass or depletion floor; cannot exceed initial mass |
| Variable mass | checkbox | Assigns one mass state and automatically uses \(I_C(t)=m(t)I_{C,0}/m_0\) |
| Local CM | 3-vector [m] | Component origin to component CM, component axes |
| Centroidal inertia | symmetric 3x3 [kg m2] | About component CM, component axes |

The inertia editor asks for `Ixx,Iyy,Izz,Ixy,Ixz,Iyz` and mirrors the
off-diagonal entries. The backend rejects non-finite, nonsymmetric, or
physically impossible inertia tensors.

The HUD permits zero mass for individual components. It rejects a component
set unless total initial spacecraft mass is positive and the spacecraft's
minimum reachable mass remains positive. Fixed components contribute their
initial mass to that floor; variable-mass components contribute their minimum
mass.

### 3.4 Child Connection and Ordered DOF Inputs

Every non-root component asks for:

| HUD input | Type and unit | Exact meaning |
|---|---|---|
| Parent component | component name | Must resolve to an earlier array entry |
| Parent anchor | 3-vector [m] | \(r^P_{P a}\), parent origin to joint anchor in `P` |
| Child anchor | 3-vector [m] | \(r^C_{C a}\), child origin to matching anchor in `C` |
| Child-to-parent zero orientation | quaternion | \(R^0_{P C}\), active child-vector to parent-vector rotation at zero coordinates |
| Ordered DOF list | array | Applied exactly in displayed array order |

Every DOF asks for:

| HUD input | Rotation DOF | Translation DOF |
|---|---|---|
| Name | Display/debug label only | Display/debug label only |
| Axis | Unit vector in current intermediate frame `J_i` | Unit vector in current intermediate frame `J_i` |
| Initial coordinate | rad | m |
| Initial rate | rad/s | m/s |
| Optional minimum/maximum | rad | m |
| Maximum absolute rate | rad/s | m/s |
| Maximum absolute effort | N m | N |

DOF names and HUD GUIDs do not determine backend mechanics. Component order,
DOF array order, and the converter-assigned flattened indices are
authoritative. Renaming a DOF does not change the simulation.

### 3.5 Exact Ordered Transform Algorithm

Initialize the joint-chain transform from the final intermediate joint frame
into the parent frame:

\[
R_J=I_3,\qquad p_J^P=0.
\]

Then process DOFs in array order. Each DOF is appended by right multiplication.
Its stored axis \(a_i^{J_i}\) is expressed in the intermediate frame produced
by all preceding DOFs, and

\[
a_i^P=R_J a_i^{J_i}.
\]

For a rotational DOF,

\[
R_J\leftarrow R_J\operatorname{Rot}(a_i^{J_i},\eta_i).
\]

`Rot` is an active, right-handed Rodrigues rotation. Looking along the positive
axis toward the origin, a positive angle is counterclockwise. For a
translational DOF,

\[
p_J^P\leftarrow p_J^P+a_i^P\eta_i.
\]

Thus a positive translational coordinate moves along the positive stored axis
of its current intermediate frame. Rotation and translation DOFs may be mixed
freely and use the same array-order rule.

After every DOF has been applied, the zero orientation is rightmost:

\[
R_{P C}=R_JR^0_{P C},
\]

and child-anchor compensation uses the negative transformed child-anchor
vector:

\[
s^P_{P C}=r^P_{P a}+p_J^P-R_{P C}r^C_{C a}.
\]

The complete nested pose in B is

\[
R_{B C}=R_{B P}R_{P C},\qquad
s^B_{B C}=s^B_{B P}+R_{B P}s^P_{P C}.
\]

These equations guarantee anchor coincidence:

\[
s^P_{P C}+R_{P C}r^C_{C a}=r^P_{P a}+p_J^P.
\]

Implementation-equivalent pseudocode is:

```text
R_joint = Identity
p_joint_parent = Zero

for dof in DegreesOfFreedom, in array order:
    axis_current = Normalize(dof.Axis)
    axis_parent = R_joint * axis_current

    if dof is Rotation:
        R_joint = R_joint * RightHandRotation(axis_current, coordinate)
    else:
        p_joint_parent += axis_parent * coordinate

R_parent_child = R_joint * R_parent_child_at_zero
s_parent_child = parent_anchor + p_joint_parent
                 - R_parent_child * child_anchor

R_body_child = R_body_parent * R_parent_child
s_body_child = s_body_parent
               + R_body_parent * s_parent_child
```

An empty DOF array sets \(R_J=I\) and \(p_J=0\). It is therefore exactly a
fixed connection with orientation \(R^0_{P C}\), while the anchor equation
places the stored child anchor on the stored parent anchor.

### 3.6 Flattened Articulation State and Units

The converter/builder flattens joint states in component array order and then
DOF array order inside each component. A component with DOFs receives one
stable `articulation_state_offset`; local DOF `j` uses

```text
state_index = component.articulation_state_offset + j
```

Components without DOFs receive no offset. Initial coordinates and rates are
copied into these arrays in SI units and clamped to their configured limits.
If a caller explicitly supplies flattened arrays, those entries override the
per-DOF initial values; the HUD converter should normally derive the arrays
instead of asking the user for them separately.

| Motion | Coordinate | Rate | Acceleration | Effort |
|---|---|---|---|---|
| Rotation | rad | rad/s | rad/s2 | N m |
| Translation | m | m/s | m/s2 | N |

Recorded articulation coordinates are accepted generalized coordinates after
limit projection. Revolute coordinates are not wrapped to `[-pi,pi]` or any
other interval. Unlimited revolute joints may therefore pass through multiple
turns continuously.

### 3.7 Limit Semantics and Preview Policy

- Minimum and maximum coordinates are hard stops in the implemented rigid
  model. Initial/output states are clamped, outward rates at a stop are zeroed,
  and ABA is rerun with an active acceleration constraint when a solved
  acceleration would drive farther through the stop. This is not an impact,
  restitution, contact, or friction model.
- Maximum absolute rate is a hard speed cap. States are clamped to the cap and
  outward acceleration at an active speed limit is constrained.
- Maximum absolute effort saturates only the controller's commanded generalized
  force or torque. It is not a cap on external loads or stop reactions.
- If a HUD coordinate-bound checkbox is false, the converter writes negative
  infinity for a missing minimum and positive infinity for a missing maximum.
- Temporary preview values must use the same coordinate limits as the backend.
  Manual sliders and oscillation previews must clamp rather than display an
  impossible configuration.

### 3.8 Right-Handed Backend and Unreal Conversion

The backend and ICRF use right-handed vector algebra. Unreal uses a left-handed
world. The existing visualization convention reflects Y:

\[
C=\operatorname{diag}(1,-1,1),\qquad
p_{UE,cm}=100\,C\,p_{backend,m}.
\]

For a rotation whose source and destination frames are both converted with
this convention,

\[
R_{UE}=C R_{backend} C.
\]

The converter/actor should convert through this matrix equation and then build
an Unreal `FQuat`; it should not copy quaternion fields and guess sign changes.

Backend `Quatd` storage is scalar-first `[w,x,y,z]`; Unreal `FQuat` fields are
`[x,y,z,w]`. The backend uses the Hamilton product. If \(q_{A B}\) maps B into
A and \(q_{B C}\) maps C into B, then

\[
q_{A C}=q_{A B}\otimes q_{B C},
\]

so the right operand is applied first. Active vector rotation is

\[
[0,v^A]=q_{A B}\otimes[0,v^B]\otimes q_{A B}^{*}.
\]

The HUD should require finite, nonzero axes/quaternions and normalize them once
in the converter. The current HUD validation policy is:

- axis length at most `1e-12`: error;
- quaternion squared norm at most `1e-12`: error;
- absolute unit-length error above `1e-6`: warning and normalize before use.

The pure backend independently rejects DOF-axis norms at or below `1e-15` and
the initial attitude quaternion at or below `1e-15`. Component orientation
quaternions must be normalized before conversion to the rotation matrices that
backend validation receives.

### 3.9 Visual Geometry Is Not Physical Geometry

Primitive choice/dimensions, STL file and units, surface appearance, color,
textures, and visibility do not directly modify mass, CM, inertia, anchors,
joints, actuators, or aerodynamics. SRP is the one explicit opt-in reuse: its
converter may generate a separate physics proxy from the selected primitive/STL
source and store the resulting triangles in the simulation request.

STL coordinates are interpreted in the physical component's local frame after
applying only the selected STL length unit. The current HUD always preserves the
imported STL origin and orientation. It does not ask for recentering, visual
offset, visual orientation, or visual scale. The stored frontend fields are
fixed to `KeepImportedOrigin`, zero, identity, and one respectively. The user
must correct the source STL when a different origin, orientation, or scale is
needed.

The core backend never reads a render mesh or STL file. Regenerating an SRP
proxy is an explicit preprocessing operation after source geometry changes;
propagation receives only the saved proxy triangles. Aerodynamics uses either a
whole-spacecraft coefficient database or the geometry-free constant-drag
fallback and never consumes visual geometry.

### 3.10 Authoritative Result Playback

At each recorded output epoch, `TelemetrySample.component_poses` is the
authoritative component-pose array. It contains one \(s^B_{B C}\) and one
\(R_{B C}\) per configured component, already evaluated by the backend's
kinematics model. Playback should consume these poses when available rather
than independently rebuilding them from names or joint coordinates.

The propagated position is the total spacecraft CM, not the B origin. Place B
in ICRF using

\[
r^I_{O_B}=r^I_{CM}-R_{I B}r^B_{B,CM},
\]

where `state.position_icrf_m` is \(r^I_{CM}\),
`state.attitude_body_to_icrf` supplies \(R_{I B}\), and
`sample.center_of_mass_body_m` is \(r^B_{B,CM}\). Component `C` then has

\[
r^I_{O_C}=r^I_{O_B}+R_{I B}s^B_{B C},\qquad
R_{I C}=R_{I B}R_{B C}.
\]

The backend records exact requested output epochs; it does not generate
additional display frames between samples. Smooth display interpolation is a
HUD responsibility. Interpolate positions linearly (or with a velocity-aware
scheme) and orientations with shortest-arc quaternion SLERP. Raw articulation
coordinates may be interpolated as unwrapped scalars because the backend does
not wrap rotational coordinates. Never wrap them in the HUD before
interpolation.

The reusable actor should expose both preview and playback entry points:

- `ApplyJointCoordinates` evaluates the exact Section 3.5 chain for a temporary
  preview.
- `ApplySimulationPlaybackState` consumes the returned component poses and
  body/CM state.

Both should feed one lower-level `ApplyComponentPoses` path for Unreal frame,
unit, mesh, and actor-transform conversion. If an old result lacks component
poses, rebuilding them from raw articulation coordinates is a fallback, not
the preferred path.

### 3.11 Numerical Kinematic Examples

Both examples use backend right-handed coordinates, an identity parent pose in
B, and all dimensions in meters.

**Example A: one rotational DOF.** Let

```text
parent anchor = (1, 0, 0)
child anchor = (0.5, 0, 0)
zero orientation = Identity
DOF 0 = rotation about +Z by pi/2
```

Then

\[
R_{P C}=R_z(\pi/2)=
\begin{bmatrix}0&-1&0\\1&0&0\\0&0&1\end{bmatrix},
\qquad
s^P_{P C}=(1,-0.5,0).
\]

The transformed child anchor is
\((1,-0.5,0)+R_{PC}(0.5,0,0)=(1,0,0)\), exactly the parent anchor.

**Example B: mixed rotation then translation.** Let

```text
parent anchor = (1, 2, 0)
child anchor = (0, 1, 0)
zero orientation = rotation about +X by pi/2
DOF 0 = rotation about +Z by pi/2
DOF 1 = translation along +X of its current frame by 2
```

After DOF 0, the current +X axis is parent +Y, so DOF 1 contributes
\(p_J^P=(0,2,0)\). The final results are

\[
R_{P C}=R_z(\pi/2)R_x(\pi/2)=
\begin{bmatrix}0&0&1\\1&0&0\\0&1&0\end{bmatrix},
\qquad
s^P_{P C}=(1,4,-1).
\]

Because \(R_{PC}(0,1,0)=(0,0,1)\), the child anchor becomes
\((1,4,0)\), equal to the parent anchor plus the translated joint displacement.

## 4. Thrusters and Propellant

Each thruster has:

| HUD input | Type and unit | Meaning |
|---|---|---|
| Name | text | Result/UI identity |
| Mode | option | Prescribed profile or commanded |
| Mount component | component name | Where the external wrench enters the tree |
| Propellant component | required component name | Variable-mass component whose mass is depleted |
| Application point | 3-vector [m] | In mount-component axes |
| Direction | normalized 3-vector | Thrust direction in mount-component axes |
| Ignition time | UTC or elapsed seconds | Converted to absolute ET |
| Shutdown time | UTC, elapsed seconds, or never | Converted to absolute ET/infinity |

Every thruster must reference an existing variable-mass propellant component.
The thruster shuts down at that component's minimum mass. The configured thrust is assumed to include
exhaust momentum under the recommended mass-flow convention.

### 4.1 Prescribed-Profile Thruster

A prescribed thruster is independent of controller propulsion output. The HUD
asks for:

| HUD input | Type and unit | Meaning |
|---|---|---|
| Thrust source | constant or CSV | Constant \(T\), or \(T(\tau)\) with \(\tau=t-t_{\mathrm{ignition}}\) |
| Constant thrust | scalar [N] | Shown only when the thrust source is constant |
| Thrust profile | CSV rows `(time_s,thrust_n)` | Shown only when the thrust source is CSV |
| Specific-impulse source | constant or CSV | Constant \(I_{sp}\), or \(I_{sp}(\tau)\) |
| Constant specific impulse | positive scalar [s] | Shown only when the Isp source is constant |
| Specific-impulse profile | CSV rows `(time_s,isp_s)` | Shown only when the Isp source is CSV |

The CSV files have no header. The importer may also accept one combined
three-column CSV `(time_s,thrust_n,isp_s)` and populate both backend curves on
the same time grid. Thrust samples must be nonnegative, with the first and last
thrust values equal to zero. Isp samples must be positive.

At each dynamics evaluation the backend computes

```text
T = constant_thrust or thrust_profile(time_since_ignition)
Isp = constant_isp or isp_profile(time_since_ignition)
mass_rate = -T / (Isp * g0)
q_dot = (T_dot / Isp - T * Isp_dot / Isp^2) / g0
```

Curve derivatives are obtained analytically from the active linear segment;
constants and clamped extrapolation have zero rate. There is no
prescribed-thruster maximum-thrust field and no throttle input.

### 4.2 Commanded Thruster

The HUD asks only for a positive maximum thrust in addition to the common
thruster geometry and firing window:

| HUD input | Type and unit | Meaning |
|---|---|---|
| Maximum commanded thrust | positive scalar [N] | \(T_{\max}\) |

At every dynamics evaluation the user's C++ controller supplies throttle
\(u\) and instantaneous specific impulse \(I_{sp}\). The backend computes

```text
T = maximum_thrust * clamp(u, 0, 1)
mass_rate = -T / (Isp * g0)
```

Commanded thrusters have no thrust, throttle, or Isp CSV fields.
For a smooth controller law, each `PHAROSThrusterCommand` output may also supply the
total derivative of actual outward discharge `q = T/(Isp*g0)` in kg/s^2. It is
not another HUD scenario input.

## 5. Reaction Wheels

| HUD input | Type and unit | Meaning |
|---|---|---|
| Name | text | Wheel identity |
| Mount component | component name | Component receiving wheel reaction |
| Axis | normalized 3-vector | In mount-component axes |
| Initial momentum | scalar [N m s] | Signed along axis |
| Maximum absolute momentum | scalar [N m s] | Saturation limit |

The propagated wheel state is stored momentum, not rotor angle. Wheel momentum
rate is commanded in the Control section.

## 6. User C++ Control System

The packaged application uses dynamically loaded controller DLLs. A controller
is compiled independently from the packaged Unreal executable against the
POD-only API in `ControllerSDK/PHAROSControllerAPI.h`; it is not compiled
into the Unreal project and does not use Unreal headers, `UObject`, reflection,
UBT, or Live Coding. The application ships `ControllerTemplate.cpp`, the SDK
header, and the pinned self-contained LLVM-MinGW compiler.

The control law itself remains entirely user code. The HUD only manages its
source, build, registration, and selection.

### 6.1 Simulation Setup Inputs

These are the controller-selection fields exposed in the scenario HUD:

| HUD input | Type | Required | Meaning |
|---|---|---|---|
| Control mode | choice: `None` or `User C++ controller` | yes | `None` produces zero commanded-actuator outputs. `User C++ controller` enables the controller selector. |
| Controller | selector populated from successfully built/validated controllers | only in `User C++ controller` mode | Stores the stable controller ID, not a source or DLL path. |

The scenario HUD must not ask for controller source paths, header paths, DLL
paths, compiler paths, compiler flags, entry-point names, gains, targets, or
controller-specific parameters. The selected controller owns all control-law
constants, modes, estimators, filters, memory, targets, and actuator-selection
logic in its C++ source.

An imported TGSCN also preserves its standalone controller DLL path so that
export does not discard it. That path is not a HUD selection field: Review
requires importing and trusting the DLL in the Controller Library and selecting
its registered ID. A missing, unbuilt, untrusted, or incompatible selection
prevents a graphical run.

### 6.2 Controller Library and Build Inputs

Controller creation belongs in a separate Controller Library screen. It is a
content-management workflow, not part of `SimulationRequest`.

For a **New from template** operation, the HUD asks for exactly:

| HUD input | Type | Required | Meaning |
|---|---|---|---|
| Controller name | unique editable text | yes | Human-readable name. The application generates the opaque stable controller ID. |
| Trusted-code confirmation | confirmation checkbox/dialog | yes before build | Confirms that native controller code receives the application's operating-system permissions. |

For an **Import source** operation, the HUD asks for exactly:

| HUD input | Type | Required | Meaning |
|---|---|---|---|
| Controller name | unique editable text | yes | Human-readable name. |
| Source file | file picker restricted to one existing `.cpp` file | yes | The application copies the text into managed storage; the original path is not retained. Maximum UTF-8 source size is 2 MiB. |
| Trusted-code confirmation | confirmation checkbox/dialog | yes before build | Same native-code warning as above. |

For the optional advanced **Import prebuilt DLL** operation, the HUD asks for
exactly:

| HUD input | Type | Required | Meaning |
|---|---|---|---|
| Controller name | unique editable text | yes | Human-readable name. |
| Controller DLL | file picker restricted to one existing `.dll` file | yes | Copied into managed storage only after its required exports and structure contract are validated. |
| Trusted-code confirmation | confirmation checkbox/dialog | yes | Import is rejected until confirmed. |

For an existing source controller, the editable fields are only its controller
name, trusted status, and the C++ source text loaded from managed storage. A
prebuilt-DLL controller has a name and trusted status but no editable source or
Build button.

The Controller Library provides these commands and read-only results:

| HUD control/status | Behavior |
|---|---|
| New from template | Calls `CreateControllerFromTemplate`; the installed template remains unchanged. |
| Import source | Calls `ImportControllerSource` after the shared file dialog selects a `.cpp`. |
| Save source | Calls `SaveControllerSource`. Saving invalidates the previous selectable build until another build succeeds. |
| Build/Rebuild | Calls `BuildController`, which invokes the bundled compiler asynchronously. It never invokes UBT or rebuilds the game. |
| Build diagnostics | Shows compiler standard output, warnings, and errors with source line numbers. It is read-only. |
| Build status | Shows `Not built`, `Building`, `Ready`, `Failed`, or `Incompatible controller`. |
| Rename controller | Calls `RenameController`; this changes only the display name, never the stable ID or DLL. |
| Trust/untrust | Calls `SetControllerTrusted`. An untrusted controller cannot be built, selected, or attached to a run. |
| Delete controller | Calls `DeleteController` after confirmation. Deletion is rejected while building or loaded by a simulation. |
| Import prebuilt DLL | Calls `ImportPrebuiltControllerDll`; no source or compiler is involved. |

Source, registry, and DLL files are stored below Unreal's writable
`ProjectSavedDir()/Controllers`, never beside the packaged executable. A
successful rebuild produces a unique build directory so Windows never has
to overwrite a loaded DLL. Rebuild and deletion are rejected while one of that
controller's DLLs is loaded by a simulation.

Compiler choice, architecture, optimization level, include paths, linker flags,
output path, generated wrapper code, DLL filename, contract validation, and controller
registry maintenance are application responsibilities and are not HUD inputs.

### 6.3 Controller Source Contract

The template contains all DLL-export and lifecycle boilerplate. For the normal
one-file workflow, the user edits `PHAROSUserController::ComputeControl`:

```cpp
class PHAROSUserController final
{
public:
    double NextDiscontinuityElapsedTime(
        double current_elapsed_time_seconds) const
    {
        return std::numeric_limits<double>::infinity();
    }

    void ComputeControl(
        const PHAROSControlInput& input,
        PHAROSControlOutput& output)
    {
        // The user implements the complete control system here.
        // Optional analytic q_dot belongs in each thruster command.
    }
};
```

`PHAROSControlInput` and `PHAROSControlOutput` are POD/fixed-width SDK types with a
structurally validated C ABI. The DLL boundary must not expose `IController`, Unreal types,
C++ standard-library containers, references, exceptions, or ownership across
modules. During simulation, the runner's `StandaloneControllerAdapter`
translates between this ABI and the backend's `IController`, `ControlInput`,
and `ControlCommandWriter` objects.

The SDK wrapper exports and the application validates exactly:

```cpp
uint32_t PHAROS_DescribeControllerContract(
    PHAROSControllerContract* contract);
void* PHAROS_CreateController();
void PHAROS_DestroyController(void* controller);
uint32_t PHAROS_ComputeControl(
    void* controller,
    const PHAROSControlInput* input,
    PHAROSControlOutput* output);
```

The template additionally exports the optional
`PHAROS_NextControllerDiscontinuityElapsedTime` hook. A controller with a hard
elapsed-time cutoff returns the next cutoff through this method; a continuous
controller returns positive infinity. This is source behavior, not another HUD
field. Controllers must be built with the SDK distributed with PHAROS.

The user does not type these function names into the HUD. They are fixed by the
SDK/template. A source file that needs private classes, helper functions, or
controller state may define all of them inside the same `.cpp`. Support for
multi-file controllers or third-party libraries is outside the initial HUD
contract.

### 6.4 Data Supplied to the Controller

At every dynamics right-hand-side evaluation, including intermediate
Runge-Kutta stages, the SDK input provides:

| Supplied control input | Contents |
|---|---|
| Time | Absolute SPICE ET and elapsed simulation seconds |
| Spacecraft truth state | CM position/velocity, attitude, body angular velocity, component masses, eta, eta-dot, and wheel momenta |
| Configuration | Scenario/solver values and relevant gravity, SRP, atmosphere, and aerodynamics settings |
| Mass properties | Current total CM and inertia in B |
| Components | Run-stable array index, name, mass definition, current mass, origin in B, component-to-B rotation, local CM, and local inertia |
| Thrusters | Run-stable array index, name, mode, mounting/propellant indices, geometry, timing, maximum commanded thrust, firing-window status, current propellant mass, and availability |
| Joints | Flattened state index, name, child/local DOF identity, type, axis, coordinate, rate, limits, and active-limit flags |
| Reaction wheels | Run-stable array index, name, mount/axis, current/maximum momentum, and saturation flags |
| Celestial bodies | Run-stable array index, name, NAIF ID, gravity metadata, current ICRF position/velocity, and body-fixed-to-ICRF orientation |

Definitions such as component names, commanded-thruster maximum thrust,
thruster directions, joint axes and effort limits, wheel axes, and environment
settings are available in the read-only configuration view.

There is no separate current-throttle state because propulsion has no modeled
actuator lag: the throttle returned during an evaluation is the current command.
Joint coordinates/rates and wheel momentum are propagated states and are
supplied explicitly.

### 6.5 Commands Returned by the Controller

The application initializes all output commands to zero before calling user
code. The controller can return:

| Controller output | Type and unit | Applies to |
|---|---|---|
| Thruster throttle | scalar `[0,1]` | Each commanded thruster |
| Thruster specific impulse | positive scalar [s] when throttle is positive | Each commanded thruster |
| Joint effort | scalar [N m] for revolute DOFs or [N] for prismatic DOFs | Each controllable joint DOF |
| Wheel momentum rate | scalar [N m] | Each reaction wheel |
| Additional external body torque | three-vector in B [N m] | Spacecraft as a whole |

Each thruster command may provide one signed finite
`MassFlowDerivativeKilogramsPerSecondSquared` value. This is the total
derivative of actual outward discharge after command limiting, not a throttle
derivative and not an additional thrust command.
`MassFlowDerivativeProvided = false` means omitted. Omission contributes zero
only to that thruster's owner-component `m_ddot` term; ordinary thrust, actual
mass flow, inertia-rate terms,
and the two velocity-dependent CM corrections remain active.

Commands not written remain zero. The adapter rejects non-finite values,
invalid IDs/indices, and positive throttle without positive specific impulse.
TGSimCore then clamps throttle, applies joint-effort and wheel-saturation limits,
and routes valid commands into propulsion and articulated-body dynamics.
Prescribed-profile thrusters ignore controller propulsion commands.

An empty controller is valid and produces zero commanded-actuator outputs;
prescribed thrusters continue to follow their configured profiles.

### 6.6 Evaluation Rules

The controller computes commands but must not advance the simulation state.
Adaptive integration may reevaluate a trial state or call the controller at
non-monotonic trial-stage times. A stateful controller must therefore derive its
updates from supplied time/state and tolerate repeated evaluations; a pure
function of the supplied snapshot does this naturally.

The optional derivative fields follow the same rule. For a state-dependent
command, they must provide the total time derivative, including state
dependence. PHAROS does not finite-difference omitted values. A finite
derivative represents a smooth branch; it
does not encode the impulsive effect of an ideal instantaneous mass-flow-rate
jump.

TGSimCore owns input construction, zeroed and correctly sized output storage,
numerical and index safety, actuator limits, force/torque generation, and state
propagation. The user owns the control law. Because the compiled DLL is native
code, the application cannot sandbox it; only trusted source or DLLs should be
built and loaded.

### 6.7 Exact Widget Callback Map

The remaining Controls implementation is widget construction and wiring these
events; it must not reproduce controller-library logic in Blueprint:

| Widget event | Existing function/delegate to use |
|---|---|
| Controls screen constructed | Obtain `UTGControllerLibrarySubsystem`, bind `OnControllerLibraryChanged` and `OnControllerBuildFinished`, call `GetControllers`, and call `InspectControllerToolchain` |
| Control mode changed | `UTGControlEditingLibrary::SetControlMode` |
| Scenario controller selected | `UTGControlEditingLibrary::SelectReadyController` |
| Scenario controller cleared | `UTGControlEditingLibrary::ClearSelectedController` |
| Scenario/review validation | `UTGControlEditingLibrary::ValidateControlSelection` |
| New from template confirmed | `UTGControllerLibrarySubsystem::CreateControllerFromTemplate` |
| Import source clicked | `UTGFileDialogLibrary::OpenSingleFileDialog` with `cpp`, then `ImportControllerSource` |
| Import DLL clicked | `UTGFileDialogLibrary::OpenSingleFileDialog` with `dll`, then `ImportPrebuiltControllerDll` |
| Existing source editor opened | `LoadControllerSource` |
| Source Save clicked | `SaveControllerSource` |
| Build/Rebuild clicked | `BuildController`; disable repeated build until `OnControllerBuildFinished` |
| Name committed | `RenameController` |
| Trusted status changed | `SetControllerTrusted` |
| Delete confirmed | `DeleteController` |
| Library/build delegate received | Refresh rows from `GetControllers`; display `LastBuildDiagnostics` for the selected row |

The controller selector must show only records returned by
`UTGControlEditingLibrary::GetReadyControllers`. It displays `DisplayName` and
stores `ControllerId` in `FTGControlConfig`. `ManagedSourceFilePath`,
`ManagedDllFilePath`, compiler path, compiler version, status,
timestamps, and diagnostics are read-only presentation data rather than user
inputs.

## 7. Scalar Curves

The HUD exposes only strictly increasing `(time_seconds,value)` rows. It does
not expose interpolation, extrapolation, or default-value controls.

The backend uses piecewise-linear interpolation. Before the first row it returns
the first value; after the last row it returns the last value. The generic
backend curve type retains internal extrapolation policies for non-HUD use, but
HUD propulsion curves always use endpoint clamping.

Thruster curve time is time since that thruster's ignition. A thrust curve needs
at least two rows, all thrust values must be nonnegative, and its first and last
values must be zero. Every Isp profile value must be positive.

## 8. Gravity and Celestial Bodies

SPICE is mandatory and is attached automatically by the Unreal converter.
Failure to load the required kernels or resolve a configured body is a setup
error, not a user-selectable fallback mode.

The HUD presents the backend's supported SPICE body catalog. Display names and
SPICE targets are fixed by the tables below; the user does not type either one.
SPICE supplies ephemerides, point-mass GM, body-fixed frames, and physical
shape radii automatically. A system barycenter has no shape or body-fixed
frame and is therefore restricted to point-mass gravity. An uploaded harmonic
gravity model supplies a separate GM and mathematical reference radius in its
CSV file; SPICE shape radii are never substituted for that model radius.

The compact satellite-kernel interval is `2000-01-01` through `2050-01-01`
(end exclusive). The HUD applies this restriction only when a selected or
automatically activatable source actually depends on those compact moon
kernels. Earth's Moon, Earth, the Sun, and the Earth-Moon barycenter come from
DE442 and do not trigger this compact interval; their actual SPICE availability
is checked directly at the requested start and final epochs.

### 8.1 Global Gravity Input

| HUD input | Type | Meaning |
|---|---|---|
| Include 1PN | checkbox | First post-Newtonian test-particle correction |

TGSimCore always evaluates gravity at every component CM. The tiny differences
across the spacecraft produce gravity-gradient torque and generalized loads on
articulated joints. This is backend physics behavior, not a HUD choice.

### 8.2 Exact HUD Inputs

The user supplies these fields for **every** catalog source, whether it is an
independent body, a physical system member, or a system barycenter:

| HUD input | Type and unit | Meaning |
|---|---|---|
| Gravity enabled | checkbox | Explicit gravity selection |
| Automatic activation radius | nonnegative scalar [m] | An unchecked source activates when the spacecraft CM enters this distance from that source; zero disables automatic activation |

The user supplies this additional field for **each system barycenter only**:

| HUD input | Type and unit | Meaning |
|---|---|---|
| Barycenter resolution radius | nonnegative scalar [m] | An active barycenter is replaced by every physical member in its fixed catalog system when the spacecraft CM enters this distance from the barycenter; zero keeps the barycenter representation at all distances |

The user supplies these additional fields for a physical body only when
spherical-harmonic gravity is desired:

| HUD input | Type and unit | Meaning |
|---|---|---|
| Harmonic model CSV | file picker | No-header file containing model GM, model radius, and fully normalized coefficients |
| Maximum harmonic degree used | nonnegative integer | Zero selects SPICE-GM point-mass gravity; positive values truncate the uploaded file at this degree and cannot exceed its largest degree |

The HUD must not ask the user for the SPICE target, NAIF ID, gravity-system
name, or whether an entry is a barycenter/member/independent source. Those are
fixed catalog metadata attached automatically to each displayed row.

Every catalog row must remain present in `SimulationRequest`, including
unchecked rows. Unchecked does not mean omitted: the backend may need that row
later for automatic activation or for resolving an active barycenter into its
complete member set.

The SPICE GM and PCK shape radii are never editable HUD inputs. For point-mass
gravity the backend uses SPICE GM and no radius. For harmonic gravity the
backend uses the CSV model's GM and radius for its implicit `C00 = 1` monopole
and every higher-degree coefficient.

#### 8.2.1 Harmonic Model CSV Format

The file has no header:

```text
GM_m3ps2,R_m
n,m,Cbar_nm,Sbar_nm
n,m,Cbar_nm,Sbar_nm
...
```

The first nonempty row contains exactly two positive SI values:

1. Gravity model `GM` in `[m3/s2]`.
2. Gravity model reference radius `R` in `[m]`.

Every subsequent nonempty row contains exactly four values:

1. Integer degree `n`.
2. Integer order `m`.
3. Fully normalized coefficient `Cbar_nm`.
4. Fully normalized coefficient `Sbar_nm`.

The converter must reject headers, malformed/nonfinite values, duplicate
`(n,m)` rows, `n < 0`, `m < 0`, `m > n`, and explicit `(0,0)`. Missing
coefficient pairs are zero. The converter places the first-row constants into
`harmonic_model_gravitational_parameter_m3ps2` and
`harmonic_model_reference_radius_m`, and places the remaining rows into the
backend coefficient array.

Example:

```text
4.282837024529127e13,3396000.0
2,0,-8.750000000000000e-4,0.0
2,1,1.000000000000000e-10,-2.000000000000000e-10
2,2,-8.500000000000000e-5,4.900000000000000e-5
```

### 8.3 Barycenter Checkboxes

These are the far-field choices. `SOLAR SYSTEM BARYCENTER` is the ICRF/J2000
origin used by the provider and is not a selectable gravity source.

| HUD label | SPICE target | NAIF ID |
|---|---|---:|
| Mercury barycenter | `MERCURY BARYCENTER` | 1 |
| Venus barycenter | `VENUS BARYCENTER` | 2 |
| Earth-Moon barycenter | `EARTH BARYCENTER` | 3 |
| Mars barycenter | `MARS BARYCENTER` | 4 |
| Jupiter barycenter | `JUPITER BARYCENTER` | 5 |
| Saturn barycenter | `SATURN BARYCENTER` | 6 |
| Uranus barycenter | `URANUS BARYCENTER` | 7 |
| Neptune barycenter | `NEPTUNE BARYCENTER` | 8 |
| Pluto barycenter | `PLUTO BARYCENTER` | 9 |

### 8.4 Physical-Body Checkboxes

| System | HUD label | SPICE target | NAIF ID |
|---|---|---|---:|
| Solar | Sun | `SUN` | 10 |
| Mercury | Mercury | `MERCURY` | 199 |
| Venus | Venus | `VENUS` | 299 |
| Earth-Moon | Earth | `EARTH` | 399 |
| Earth-Moon | Moon | `MOON` | 301 |
| Mars | Mars | `MARS` | 499 |
| Mars | Phobos | `PHOBOS` | 401 |
| Mars | Deimos | `DEIMOS` | 402 |
| Jupiter | Jupiter | `JUPITER` | 599 |
| Jupiter | Io | `IO` | 501 |
| Jupiter | Europa | `EUROPA` | 502 |
| Jupiter | Ganymede | `GANYMEDE` | 503 |
| Jupiter | Callisto | `CALLISTO` | 504 |
| Saturn | Saturn | `SATURN` | 699 |
| Saturn | Mimas | `MIMAS` | 601 |
| Saturn | Enceladus | `ENCELADUS` | 602 |
| Saturn | Tethys | `TETHYS` | 603 |
| Saturn | Dione | `DIONE` | 604 |
| Saturn | Rhea | `RHEA` | 605 |
| Saturn | Titan | `TITAN` | 606 |
| Saturn | Iapetus | `IAPETUS` | 608 |
| Saturn | Phoebe | `PHOEBE` | 609 |
| Uranus | Uranus | `URANUS` | 799 |
| Uranus | Miranda | `MIRANDA` | 705 |
| Uranus | Ariel | `ARIEL` | 701 |
| Uranus | Umbriel | `UMBRIEL` | 702 |
| Uranus | Titania | `TITANIA` | 703 |
| Uranus | Oberon | `OBERON` | 704 |
| Neptune | Neptune | `NEPTUNE` | 899 |
| Neptune | Triton | `TRITON` | 801 |
| Pluto | Pluto | `PLUTO` | 999 |
| Pluto | Charon | `CHARON` | 901 |

### 8.5 Main Minor-Planet Checkboxes

The numeric target string is deliberate and avoids depending on aliases.

| HUD label | SPICE target | NAIF ID | Gravity use |
|---|---|---:|---|
| Ceres | `2000001` | 2000001 | Point mass; generic PCK radius/frame available |
| Pallas | `2000002` | 2000002 | Point mass only |
| Vesta | `2000004` | 2000004 | Point mass; generic PCK radius/frame available |

### 8.6 Exact Activation and Double-Counting Rules

At every dynamics evaluation, the backend determines the effective gravity
sources from the five rules below. Distance tests use the spacecraft CM.

1. If an independent body, system barycenter, and all relevant system members
   are unchecked and outside their automatic activation radii, they contribute
   no gravity.
2. A checked physical body is active at every distance. It suppresses its
   system barycenter even if a malformed request marks both as checked.
3. A checked or automatically activated barycenter is used as a point mass
   outside its resolution radius. Inside its resolution radius, the backend
   disables the barycenter and activates **every physical member** in that
   catalog system.
4. A system barycenter and any mass represented by it are never summed in the
   same evaluation. Explicit members have first priority, full-system
   resolution has second priority, and individually auto-activated members
   have third priority.
5. Any unchecked body or barycenter activates inside its own automatic
   activation radius. An automatically activated member suppresses its
   barycenter. An automatically activated barycenter may immediately resolve
   into all members if it is also inside its resolution radius.

The HUD checkboxes should mirror rule 4 for clarity:

- Checking a physical member visually unchecks its system barycenter.
- Checking a system barycenter visually unchecks all its physical members.

Runtime automatic activation does not modify the user's stored checkbox values;
it modifies only the backend's effective source set for that evaluation.
The backend independently normalizes explicit conflicts, validates unique NAIF
IDs and group definitions, and therefore remains responsible for preventing
double-counting even if the HUD or converter supplies inconsistent choices.

## 9. Solar Radiation Pressure

| HUD input | Type and unit | Meaning |
|---|---|---|
| Enable SRP | checkbox | Master switch |
| Compute celestial eclipses | checkbox, default on | Apparent-disk Sun/occulter overlap |
| Compute spacecraft component shadows | checkbox, default on | Three visibility rays per illuminated proxy triangle |
| Global fallback absorption | nonnegative weight, default 1 | Normalized with the other two weights before storage |
| Global fallback specular reflection | nonnegative weight, default 0 | Normalized with the other two weights before storage |
| Global fallback diffuse reflection | nonnegative weight, default 0 | Normalized with the other two weights before storage |

The current HUD does not ask for an occulter subset. When celestial eclipses
are enabled, every supported physical non-Sun catalog body is an occulter. The
frontend stores an empty `OccultingBodyNames` array, which the converter/backend
interprets as this complete set.

The three optical entries must be finite and nonnegative. The HUD divides them
by their sum and stores fractions in `[0,1]` that sum to one. Three zero weights
produce the safe absorbing default `(1,0,0)`. `Sun` is the fixed SPICE source
and `4.5391e-6 Pa` is the pressure at one AU; neither is an ordinary HUD input.
The frontend still stores both fixed values for deterministic conversion.

### 9.1 Every Component

| HUD input | Type | Meaning |
|---|---|---|
| Include component in SRP proxy | checkbox | If off, its geometry neither receives SRP nor casts spacecraft shadows |
| Proxy resolution | `Automatic` or custom target triangle count | Used only by the preprocessing converter |
| Use global fallback optical properties | checkbox | Inherits the three normalized global fractions |
| Apply one optical configuration to entire component | checkbox, default on | Master assignment for every generated proxy triangle |
| Component absorption | nonnegative weight | Shown when global fallback is not selected; normalized before storage |
| Component specular reflection | nonnegative weight | Shown when global fallback is not selected; normalized before storage |
| Component diffuse reflection | nonnegative weight | Shown when global fallback is not selected; normalized before storage |

Every displayed optical triplet uses the same finite, nonnegative weight entry
and normalization rule as the global triplet. A component with no explicit
optical triplet inherits the visible global fallback `(1,0,0)`; the HUD warns
that fallback properties are being used.

The existing component primitive or STL is the source geometry. The user does
not enter proxy vertices, triangle centers, normals, or areas. Primitive editing
uses logical surface regions:

- cuboid: six faces, emitted as twelve proxy triangles;
- cylinder: curved side, positive cap, and negative cap;
- sphere: whole-component optical assignment only.

For a custom STL, `Automatic` should normally target approximately 100--300
proxy triangles while preserving external shape and outward orientation. A
custom count is an advanced override. The converter runs only when the source
geometry, units, or requested proxy resolution changes, never during an ODE
evaluation.

### 9.2 Individual Overrides

Primitive components use logical-region overrides only: six cuboid faces or the
cylinder side and two caps. A sphere supports only whole-component assignment.
Primitive proxy triangles are never individually editable.

Stable triangle overrides are available only for a custom STL whose generated
proxy has at most **32 triangles**. Above 32, the HUD disables individual
triangle editing and uses the component-wide assignment. Changing/regenerating
the STL proxy invalidates triangle overrides unless the converter proves that
its stable indexing is unchanged; the HUD asks for confirmation before
discarding them.

Every individual override contains only:

| HUD input | Type |
|---|---|
| Custom-STL proxy triangle | read-only stable index plus preview selection |
| Absorption | nonnegative weight, normalized before storage |
| Specular reflection | nonnegative weight, normalized before storage |
| Diffuse reflection | nonnegative weight, normalized before storage |

Optical-property precedence is:

```text
individual triangle override
    > primitive logical-region override
    > component-wide optical configuration
    > global fallback optical configuration
```

The converter resolves this inheritance. Every triangle sent to `TGSimCore`
already contains its final optical fractions.

### 9.3 Converter Output and Validation

One backend `OpticalFacet` is exactly one triangle with three vertices in its
own component axes. Right-handed winding defines the outward normal:

\[
\mathbf n^C=
\frac{(\mathbf v_1^C-\mathbf v_0^C)\times
(\mathbf v_2^C-\mathbf v_0^C)}
{\left\|(\mathbf v_1^C-\mathbf v_0^C)\times
(\mathbf v_2^C-\mathbf v_0^C)\right\|},
\qquad
A=\frac12\left\|(\mathbf v_1^C-\mathbf v_0^C)\times
(\mathbf v_2^C-\mathbf v_0^C)\right\|.
\]

The converter must reject non-finite/degenerate triangles, repair or reject
inconsistent winding, and emit only external surfaces. Proxy triangles are
opaque and cast shadows from either side. SRP loading is one-sided according to
the outward winding. A physically two-sided thin sheet therefore needs
coincident triangles with opposite winding and the appropriate properties for
each side.

Show the generated per-component and total triangle counts before simulation.
The HUD emits a soft performance warning above **2,000 total SRP proxy
triangles**. This is not a backend rejection. A future measured preflight timing
may refine the warning without changing the physics contract.

### 9.4 Component Shadow Calculation

The same proxy triangles calculate SRP and cast shadows. The backend builds one
immutable BVH in each component's local axes once when the dynamics model is
constructed. Articulation changes ray transforms, not BVH geometry, so no tree
is rebuilt during propagation.

For triangle vertices \(\mathbf v_0,\mathbf v_1,\mathbf v_2\), the three fixed
area-weighted sample points are

\[
\mathbf p_0=\frac{4\mathbf v_0+\mathbf v_1+\mathbf v_2}{6},\quad
\mathbf p_1=\frac{\mathbf v_0+4\mathbf v_1+\mathbf v_2}{6},\quad
\mathbf p_2=\frac{\mathbf v_0+\mathbf v_1+4\mathbf v_2}{6}.
\]

A ray from each sample toward the Sun is tested against every articulated proxy
through the component BVHs. The source triangle is excluded and the ray origin
is moved a small distance toward the Sun to prevent numerical self-intersection.
The local visibility estimate is

\[
\nu_k=\frac{N_{\mathrm{unblocked}}}{3}.
\]

Rather than applying \(\nu_k\) at the triangle centroid, the backend assigns
area \(A/3\) to every unblocked sample and evaluates its own moment arm. This
captures the approximate center-of-pressure shift caused by partial shadowing.
The component-shadow rays use a point/parallel Sun; finite-disk penumbra remains
part of the separate celestial-eclipse factor.

### 9.5 Force, Moment, and Articulation

For every unblocked sample with area \(A_s\),

\[
\mathbf F_s^I=-P A_s c\left[
(\alpha+\rho_d)\mathbf e_\odot^I+
2\left(\rho_s c+\frac{\rho_d}{3}\right)\mathbf n^I
\right],
\qquad c=\mathbf n^I\cdot\mathbf e_\odot^I>0.
\]

Celestial and local visibility enter independently:

\[
P=P_{1\mathrm{AU}}
\left(\frac{\mathrm{AU}}{r_\odot}\right)^2
\nu_{\mathrm{celestial}},
\qquad A_s\in\{0,A/3\}.
\]

Each sample force acts at its articulated component-local sample position. The
backend therefore returns the total CM force/moment and the component-local
wrench required by floating-base ABA. Facet positions and orientations follow
the complete nested component tree automatically.

### 9.6 Current Unreal Storage Contract

`TGSimulationScenarioTypes.h` now represents this authoring model directly.
`FTGComponentSrpConfig` stores proxy choices and optical assignments;
`FTGSolarRadiationPressureConfig::OpticalFacets` is generated output and is
never authored manually. Each generated `FTGOpticalFacetConfig` stores three
component-frame vertices plus resolved optical fractions. Proxy generation and
simplification remain converter responsibilities rather than propagation-time
work.

## 10. Atmosphere

| HUD input | Type and unit | Meaning |
|---|---|---|
| Enable atmosphere | checkbox | Required for aerodynamic loads |
| Central body | physical celestial-body name | Atmosphere center, orientation, and angular velocity come from SPICE |
| Density model | option | `Uploaded profile` or `Cubic Harris-Priester (Earth only)` |

There is deliberately **no wind input**. The atmosphere translates with its
central body and rigidly co-rotates with the SPICE body-fixed frame:

\[
\mathbf v_{\mathrm{atm}}^I=\mathbf v_{\mathrm{body}}^I+
\boldsymbol\omega_{\mathrm{body}/I}^I\times\mathbf r_{\mathrm{rel}}^I.
\]

### 10.1 General Uploaded Profile

This is the default and works for any planet or moon. The HUD asks for one
no-header CSV path. Every row is:

```text
altitude_m,density_kgpm3,temperature_k,mean_particle_mass_kg,effective_collision_cross_section_m2
```

The file needs at least two rows, strictly increasing finite altitudes, and
positive values in all four property columns. The converter splits this file
into `density_profile` and `thermodynamic_profile`.
For this general model, altitude is spherical radius above the central body's
SPICE physical reference radius. Earth CHP instead uses WGS-84 ellipsoidal
height internally.

Interpolation is fixed, not a HUD option:

- density is linear in `ln(rho)` versus altitude;
- temperature, mean particle mass, and collision cross-section are linear;
- the backend does not extrapolate beyond the uploaded altitude interval;
- outside that interval, aerodynamic force is zero and telemetry marks the
  model outside validity.

### 10.2 Cubic Harris-Priester for Earth

This optional density model is Earth-only. The HUD additionally asks for:

| HUD input | Type and unit | Meaning |
|---|---|---|
| Centered 81-day average F10.7 | positive scalar [sfu] | Solar activity used by every cubic density station |
| CHP coefficient CSV | file path | Minimum/maximum density-envelope coefficients |
| Molecular profile CSV | file path | Temperature, mean particle mass, and collision cross-section versus altitude |

The preferred coefficient CSV is the publication-compatible no-header file:
exactly 50 rows and eight columns,

```text
cmax0,cmax1,cmax2,cmax3,cmin0,cmin1,cmin2,cmin3
```

in `g/km^3` with the appropriate inverse powers of sfu. The converter attaches
the published 50-station altitude sequence from 100 km through 1000 km and
multiplies every coefficient by `1e-12` before filling the backend SI arrays.
That sequence, in km, is:

```text
100,120,130,140,150,160,170,180,190,200,
210,220,230,240,250,260,270,280,290,300,
320,340,360,380,400,420,440,460,480,500,
520,540,560,580,600,620,640,660,680,700,
720,740,760,780,800,840,880,920,960,1000
```

The molecular-profile CSV has no header and rows

```text
altitude_m,temperature_k,mean_particle_mass_kg,effective_collision_cross_section_m2
```

with the same ordering and positivity rules as the general profile.

The backend evaluates

\[
\rho_{m/M}(h_i,F)=c_0+c_1F+c_2F^2+c_3F^3,
\qquad
\rho=\rho_m+(\rho_M-\rho_m)\cos^n(\psi/2),
\]

where \(n=2.001+4\sin^2 i\). It computes WGS-84 ellipsoidal altitude, applies
the published third-order scale-height smoothing, and obtains the Sun direction
and Earth orientation from SPICE. The 30-degree diurnal-bulge lag and smoothing
width are fixed model constants, not HUD fields. Source: Hatten and Russell,
["A Smooth and Robust Harris-Priester Atmospheric Density Model for Low Earth
Orbit Applications"](https://doi.org/10.1016/j.asr.2016.10.015).

### 10.3 Derived Molecular Quantities

The HUD never asks for Mach number, sound speed, or a reference mean free path.
At each solver evaluation the backend derives

\[
n_g=\frac{\rho}{\bar m},\qquad
\lambda=\frac{1}{\sqrt{2}\,n_g\sigma_{\mathrm{eff}}},\qquad
Kn=\frac{\lambda}{L_{\mathrm{ref}}},\qquad
s=\frac{\lVert\mathbf v_{\mathrm{rel}}\rVert}
{\sqrt{2k_BT/\bar m}}.
\]

## 11. Aerodynamics

| HUD input | Type and unit | Meaning |
|---|---|---|
| Enable aerodynamics | checkbox | Requires atmosphere |
| Reference area | positive scalar [m2] | Scales database coefficients and reported fallback coefficients |
| Reference length | positive scalar [m] | Moment coefficient and Knudsen number |
| Minimum dynamic pressure | scalar [Pa] | Below this, load is zero |
| Maximum valid dynamic pressure | scalar [Pa] | Above this, telemetry marks outside validity |
| Enable constant-drag fallback | checkbox | Supplies translation-only drag if the database is unavailable or cannot answer |
| Fallback drag coefficient | positive scalar | Constant whole-spacecraft \(C_D\); shown only when fallback is enabled |

Model priority is the aggregate coefficient database, then the constant-drag
fallback. There is no aerodynamic facet model or component-resolved coefficient
database.

### 11.1 Scattered Coefficient Database

| HUD input | Type | Meaning |
|---|---|---|
| Enable database | checkbox | Preferred aerodynamic model |
| Coefficient CSV | file path | Rows in the exact column order below; no header |
| Interpolation | inverse-distance or nearest row | Exact at supplied rows |
| Extrapolation | constant-drag fallback or nearest row | For speed-ratio/Kn/eta outside sampled bounds |
| Neighbor count | nonnegative integer | Zero selects an automatic count |
| Inverse-distance power | positive scalar | Normally 2 |
| Maximum normalized neighbor distance | positive scalar or disabled | Optional sparse-coverage guard |
| Moment reference center R | 3-vector [m], B axes | Fixed point about which every uploaded moment coefficient is defined |

Every uploaded row contains:

```text
s, Kn,
gas_flow_direction_B_x, gas_flow_direction_B_y, gas_flow_direction_B_z,
eta_0, ..., eta_(N-1),
C_X, C_Y, C_Z, C_l, C_m, C_n
```

Here `gas_flow_direction_B` is the direction in which gas travels relative to
the spacecraft:

\[
\mathbf u_{\mathrm{flow}}^B=R_{BI}
\frac{-\mathbf v_{\mathrm{rel}}^I}{\lVert\mathbf v_{\mathrm{rel}}^I\rVert}.
\]

It must be normalized. Every row contains the complete eta vector in the same
flattened component/DOF order as the vehicle. `s` is nonnegative and `Kn` is
positive. The backend rejects duplicates and data that do not independently
span every coordinate that varies. Interpolation is the existing normalized
scattered-data inverse-distance method or nearest-row mode; it is exact at an
uploaded row.

The six coefficients describe the **complete spacecraft**, not a component.
At runtime,

\[
\mathbf F^B=qS_{\mathrm{ref}}\mathbf C_F^B,
\qquad
\mathbf M_{CM}^B=qS_{\mathrm{ref}}L_{\mathrm{ref}}\mathbf C_{M,R}^B+
(\mathbf r_R^B-\mathbf r_{CM}^B)\times\mathbf F^B.
\]

Because an aggregate database does not identify how the surface load is shared
among articulated bodies, it supplies the correct total spacecraft wrench but
does not uniquely supply aerodynamic generalized joint loads.

### 11.2 Constant-Drag Fallback

This path requires no component geometry. The backend applies one force to the
instantaneous total spacecraft CM:

\[
\mathbf F_D^I
=-\frac12\rho\lVert\mathbf v_{\mathrm{rel}}^I\rVert^2
S_{\mathrm{ref}}C_D
\frac{\mathbf v_{\mathrm{rel}}^I}
{\lVert\mathbf v_{\mathrm{rel}}^I\rVert},
\qquad
\mathbf M_{CM}^B=\mathbf 0.
\]

It therefore contributes only to CM translation. It creates no aerodynamic
attitude torque and no generalized load on an articulated joint. Those effects
require the six-output coefficient database. The fallback may be the sole
aerodynamic model, or it may replace an out-of-range/sparsely covered database
query when `constant-drag fallback` is selected as the extrapolation policy.
The database and fallback are never summed.

### 11.3 Regime and Warning Rules

- `Kn > 10`: free-molecular; the constant-\(C_D\) fallback is still only a rough estimate.
- `0.1 < Kn <= 10`: transitional; the fallback may run, but telemetry warns and a database is preferred.
- `Kn <= 0.1`: outside the simulator's intended aerodynamic regime; telemetry
  warns even if a database or fallback returns a force.
- Above the user-selected maximum dynamic pressure, telemetry also warns that
  aerodynamics may be too important for this moderate-fidelity model.

### 11.4 Current Unreal Storage Contract

`TGSimulationScenarioTypes.h` now matches Sections 10 and 11. It stores the
uploaded-profile/CHP atmosphere choices, molecular speed ratio rather than
Mach, one aggregate aerodynamic moment reference center, and the constant-drag
fallback. There are no wind inputs, aerodynamic facets, component-resolved
coefficient rows, or aerodynamic load-component selection. Imported
aerodynamic rows may be cached, but the selected CSV path remains authoritative
for validation and scenario persistence.

## 12. Inputs That Should Not Be Exposed as Ordinary HUD Fields

| Backend object | Reason |
|---|---|
| `IEphemerisProvider` shared pointer | Created by the runner for propagation; native Unreal request construction has its own adapter |
| `IController` shared pointer | Created in the runner from the resolved controller DLL; the graphical application resolves its selected library ID before export |
| SPICE body name | Read-only identity supplied by the backend body catalog |
| NAIF ID, gravity-system name, and source role | Fixed catalog metadata used to prevent duplicate bodies and barycenter/member overlap |
| SPICE point-mass GM and shape radii | Queried from loaded SPICE kernels |
| Harmonic-model GM and reference radius | Parsed from the first row of the user-selected harmonic CSV |
| Body ephemeris and orientation | Queried from SPICE at the required epoch |
| Atmosphere-body angular velocity | Derived by SPICE from the body-frame state transform |
| Wind, Mach, sound speed, and reference mean free path | No wind model; molecular quantities are derived from uploaded gas properties |
| Component-CM gravity evaluation | Always enabled internally for gravity-gradient and joint loads |
| Component parent indices | HUD uses names; converter resolves and topologically orders them |
| Variable mass indices | Converter assigns unique contiguous indices |
| Articulation state offsets | `SimulationConfigBuilder` assigns them |
| SRP proxy triangles and final optical fractions | Converter derives vertices from each opted-in component's primitive/STL and resolves optical inheritance; backend derives normal and area from winding |
| Total initial mass | Derived from components |
| Flattened eta/wheel/mass arrays | Derived from per-object initial values |

## 13. Conversion and Run Responsibilities

The conversion pipeline is split between Unreal scenario preparation and
the shared compiler running in `PHAROSScenarioRunner`:

1. Read the Blueprint-facing scenario structure.
2. Convert user units, UTC, Euler angles, names, and uploaded files.
3. Topologically order the component tree and resolve every name reference.
4. Attach each celestial row's fixed NAIF ID, system name, and gravity-source
   role from the catalog; never infer these identities from editable labels.
   Include every catalog row, even when its checkbox is off.
5. Parse and validate each selected harmonic CSV, including its first-row
   model GM/radius and its fully normalized coefficient rows.
6. Parse the selected atmosphere and aerodynamic CSV files, convert units,
   split general atmosphere rows into density/thermodynamic arrays, and attach
   the fixed CHP altitude sequence when the publication-compatible file is used.
7. For each SRP-enabled component, generate or simplify the primitive/STL into
   its saved component-local proxy, preserve outward winding, resolve global,
   component, logical-region, and custom-STL triangle optical inheritance, and emit one backend
   `OpticalFacet` per proxy triangle. Show the triangle counts and warning before
   the run; do not regenerate proxies inside propagation.
8. Resolve the selected controller DLL and export the prepared `.tgscn`.
   The runner attaches its SPICE provider and run-local controller object.
9. Use the shared `ScenarioCompiler` in the runner to assign variable-mass
   indices and fill `tgsim::SimulationRequest` without propagating the state.
10. Keep Unreal-only visuals, import paths, colors, controller-library IDs, and
    proxy-generation cache data in `ScenarioDocument`, but omit them from
    `SimulationRequest`.
11. Save/load that complete portable document through `UTGScenarioFileLibrary`.

`UTGSimulationRunSubsystem` launches the runner as a hidden child process,
receives progress and log messages, forwards cancellation, and enforces the
wall-clock timeout. The runner calls `SimulationEngine::Run()` with a
cancellable observer and writes the results. Unreal loads the completed CSV
for playback; it does not receive the native `SimulationResult` across the
process boundary.

The converter should report all UI-level issues together. `SimulationEngine`
will still perform authoritative backend validation before propagation.
