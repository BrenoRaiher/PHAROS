# PHAROS Standalone Runner, Scenario, and Controller Guide

This document is the complete workflow for running the PHAROS simulation engine without Unreal
Engine. It covers:

1. Preparing and invoking `PHAROSScenarioRunner.exe`.
2. Writing a complete `.tgscn` scenario.
3. Writing, compiling, and attaching a user C++ controller DLL.
4. Commanding every actuator channel currently exposed by the PHAROS simulation engine.

This guide describes the PHAROS TGSCN and Controller API contracts. All
physical values use SI units unless a field explicitly says otherwise.

## 1. What Runs Without Unreal

The standalone data flow is:

```text
mission.tgscn
    -> PHAROS Scenario Runner
    -> PHAROS scenario parser
    -> ScenarioCompiler
    -> SimulationRequest
    -> TGSimCore
    -> mission_solution.csv
    -> mission_summary.txt
```

`PHAROSScenarioRunner.exe` uses the same parser, SPICE provider, controller ABI,
dynamics, integrators, and result recorder used by the packaged application.
It does not launch Unreal and does not need an Unreal level, widget, Blueprint,
or `.uasset` at runtime.

When a controller is enabled, it is a DLL loaded into the runner process. The
runner does not compile controller source and does not create another
executable.

## 2. Standalone Directory Layout

### 2.1 Running from the PHAROS project directory

After building the runner (section 3), its executable is:

```text
Tools/TGScenarioRunner/bin/PHAROSScenarioRunner.exe
```

From the PHAROS project root, the runner automatically finds the kernels in:

```text
Content/SPICEKernels
```

For a new source checkout, run `git lfs pull` to obtain the complete kernel
set, including `ura111.bsp`. No kernel-directory copy is needed for this workflow.
`Tools/Release/InstallExternalKernels.ps1` verifies the pinned Uranus kernel
or restores a missing copy directly from JPL.

### 2.2 Making a portable runner directory

A runner directory independent of the Unreal project should have this shape:

```text
PHAROSStandalone/
|-- PHAROSScenarioRunner.exe
|-- LICENSE.txt
|-- THIRD_PARTY_NOTICES.txt
|-- Licenses/
|-- Sources/
|-- SPICEKernels/
|   |-- naif0012.tls
|   |-- pck00011.tpc
|   |-- gm_de440.tpc
|   |-- codes_300ast_20100725.tf
|   |-- de442.bsp
|   |-- mar099s.bsp
|   |-- jup230-short.bsp
|   |-- jup348.bsp
|   |-- sat252s.bsp
|   |-- ura111.bsp
|   |-- nep076.bsp
|   |-- plu058.bsp
|   `-- codes_300ast_20100725.bsp
`-- Missions/
    `-- ExampleMission/
        |-- mission.tgscn
        |-- controllers/
        |   |-- MissionController.cpp
        |   `-- MissionController.dll
        |-- data/
        `-- results/
```

Copy the kernel files from the project's `Content/SPICEKernels` directory.
All thirteen files are required by the current standalone SPICE provider, even
if a particular scenario enables only one gravity body.

A published runner must retain the applicable provider notices and source
materials from `Docs/Legal`. The repository includes the original SSD
`ura111.bsp`; its recorded provenance and distribution scope are described in
[the kernel README](../../Content/SPICEKernels/README.md).

If the kernels are stored elsewhere, pass their directory explicitly with
`--kernel-dir`.

## 3. Building or Rebuilding the Runner

The source repository excludes built executables. Build the runner after a
fresh checkout, and rebuild after changes to TGSimCore, the scenario
parser/compiler, the SPICE adapter, the controller adapter, or the runner.

From the PHAROS project root:

```bat
Tools\TGScenarioRunner\BuildStandaloneRunner.bat
```

The script writes and stages:

```text
Tools/TGScenarioRunner/bin/PHAROSScenarioRunner.exe
Binaries/Win64/PHAROSScenarioRunner.exe
```

The build script currently expects Visual Studio's x64 C++ toolchain and the
header-only Eigen, Boost, and nanoflann copies bundled with Unreal Engine 5.7.
Those build dependencies are not needed merely to run an already built
executable.

## 4. Running a Scenario

### 4.1 Basic PowerShell command

From the PHAROS project root, replace the input path below with your saved scenario:

```powershell
& ".\Tools\TGScenarioRunner\bin\PHAROSScenarioRunner.exe" `
  ".\Missions\ExampleMission\mission.tgscn"
```

Without `--output`, the files are written beside the input `.tgscn`:

```text
mission_solution.csv
mission_summary.txt
```

To select another output directory:

```powershell
& ".\Tools\TGScenarioRunner\bin\PHAROSScenarioRunner.exe" `
  ".\Missions\ExampleMission\mission.tgscn" `
  --output ".\Missions\ExampleMission\results"
```

The output filenames use the input filename stem:

```text
mission_solution.csv
mission_summary.txt
```

Every solution CSV begins with both absolute
`ephemeris_time_tdb_seconds_past_j2000` and independent
`elapsed_time_seconds` columns. Use elapsed time for mission-relative grids and
short intervals at large absolute epochs.

### 4.2 Validate before propagation

Use `--validate-only` after editing a scenario or rebuilding a controller:

```powershell
& ".\Tools\TGScenarioRunner\bin\PHAROSScenarioRunner.exe" `
  ".\Missions\ExampleMission\mission.tgscn" `
  --validate-only
```

Validation loads SPICE, parses the PHAROS scenario, loads and validates the controller DLL
when one is selected, resolves all references, constructs `SimulationRequest`,
and runs backend request validation. It does not propagate the state or create
a solution CSV.

### 4.3 Explicit SPICE directory

```powershell
& ".\PHAROSScenarioRunner.exe" `
  ".\Missions\ExampleMission\mission.tgscn" `
  --kernel-dir ".\SPICEKernels" `
  --output ".\Missions\ExampleMission\results"
```

### 4.4 Available command-line options

```text
PHAROSScenarioRunner <scenario.tgscn> [options]

--output <directory>      Output folder; defaults to the scenario folder
--kernel-dir <directory>  Folder containing the thirteen required SPICE kernels
--validate-only           Parse, compile, and validate without propagation
```

`maximum_wall_clock_runtime_seconds` in `[scenario]` is preserved for Unreal's
process supervisor. A direct CLI invocation does not enforce it. A batch or
other parent process must impose its own deadline if a hard wall-clock timeout
is required.

### 4.5 Exit codes

| Code | Meaning |
|---:|---|
| `0` | Validation or propagation succeeded |
| `2` | Invalid command-line arguments |
| `3` | The `.tgscn` could not be parsed |
| `4` | The controller DLL could not be loaded or validated |
| `5` | SPICE kernels could not be loaded |
| `6` | The scenario could not compile into a valid `SimulationRequest` |
| `7` | The output directory could not be created |
| `8` | Result files could not be written |
| `9` | Propagation returned a failed `SimulationResult` |

### 4.6 Running a directory of sweep cases

The PHAROS scenario format deliberately has no loop, variable, expression, or sweep syntax. Generate
one `.tgscn` per case, then invoke the same runner for each file. For example:

```powershell
$runner = Resolve-Path ".\Tools\TGScenarioRunner\bin\PHAROSScenarioRunner.exe"
$cases = Get-ChildItem ".\SweepCases" -Filter "*.tgscn"

foreach ($case in $cases)
{
    $caseOutput = Join-Path ".\SweepResults" $case.BaseName
    & $runner $case.FullName --output $caseOutput

    if ($LASTEXITCODE -ne 0)
    {
        throw "Case '$($case.Name)' failed with exit code $LASTEXITCODE."
    }
}
```

Python, MATLAB, C++, PowerShell, or another ordinary automation tool may create
the case files. Each generated file should remain available with its result so
the run is reproducible.

## 5. PHAROS Scenario Fundamentals

PHAROS scenario files use TOML 1.0 syntax. The `TGSCN` marker identifies the
file type.

```toml
format = "TGSCN"
generator = "My mission generator"
```

### 5.1 Basic TOML syntax

```toml
# Comment
name = "Text"
enabled = true
value = 1.25e-3
vector = [1.0, 0.0, 0.0]

[single_table]
field = 1

[[repeated_table]]
field = 1

[[repeated_table]]
field = 2
```

Unknown keys are reported by the PHAROS scenario parser. Use finite decimal or scientific
notation values.

### 5.2 Units and frames

| Quantity | Convention |
|---|---|
| Distance | meters |
| Velocity | meters per second |
| Mass | kilograms |
| Force | newtons |
| Torque | newton-meters |
| Inertia | kilogram-meter squared |
| Angle | radians |
| Angular rate | radians per second |
| Absolute translational frame | barycentric J2000/ICRF |
| Spacecraft attitude | body frame B to ICRF |
| Quaternion order | `[w, x, y, z]` |
| Component geometry | owning component's local axes |
| Joint axis | joint frame after all preceding local DOFs |

The initial position and velocity are not relative to the closest planet. They
are barycentric ICRF values at `start_utc`.

### 5.3 Time

Absolute authored epochs are quoted UTC strings:

```toml
start_utc = "2030-01-01T00:00:00Z"
```

CSPICE converts them to ET, numerically TDB seconds past J2000, before the run.
Choose one simulation end mode:

```toml
end_mode = "duration"
duration_seconds = 3600.0
```

or:

```toml
end_mode = "final_utc"
final_utc = "2030-01-01T01:00:00Z"
```

### 5.4 Referenced files

Every path may be absolute or relative. A relative path is resolved from the
directory containing the `.tgscn`, not from the current terminal directory.

```toml
controller_dll_file = "controllers/MissionController.dll"
general_profile_csv_file = "data/atmosphere.csv"
```

For absolute Windows paths, TOML literal strings avoid backslash escaping:

```toml
controller_dll_file = 'C:\Mission\controllers\MissionController.dll'
```

## 6. Complete Controlled Multibody Example

The following file demonstrates a physical bus, a variable-mass propellant
tank, an articulated panel, a commanded thruster, a reaction wheel, a user
controller, and Sun/Earth gravity. Replace the initial ICRF state and physical
properties with values appropriate to the mission.

```toml
format = "TGSCN"
generator = "Standalone guide example"

[scenario]
name = "Controlled articulated Earth orbiter"
start_utc = "2030-01-01T00:00:00Z"
end_mode = "duration"
duration_seconds = 600.0
integrator = "adaptive_dormand_prince_54"
maximum_integrator_step_seconds = 2.0
initial_integrator_step_seconds = 0.25
absolute_tolerance = 1.0e-8
relative_tolerance = 1.0e-10
output_mode = "fixed_interval"
output_step_seconds = 1.0
maximum_integration_steps = 1000000
maximum_output_samples = 100000
mass_flow_convention = "thrust_includes_exhaust_momentum"

[initial_state]
position_icrf_m = [-25921247154.130047, 132886458150.29922, 57609192903.231651]
velocity_icrf_mps = [-29820.331997819561, -260.99450308405903, 3746.4747979015547]
attitude_body_to_icrf = [1.0, 0.0, 0.0, 0.0]
angular_velocity_body_radps = [0.0, 0.0, 0.001]

[[components]]
id = "11111111-1111-1111-1111-111111111111"
name = "Bus"
initial_mass_kg = 80.0
minimum_mass_kg = 80.0
variable_mass = false
local_center_of_mass_m = [0.0, 0.0, 0.0]
origin_body_m = [0.0, 0.0, 0.0]
component_to_body = [1.0, 0.0, 0.0, 0.0]
parent_component = ""
parent_anchor_m = [0.0, 0.0, 0.0]
child_anchor_m = [0.0, 0.0, 0.0]
child_to_parent_zero_orientation = [1.0, 0.0, 0.0, 0.0]
dofs = []
inertia = { ixx_kgm2 = 12.0, iyy_kgm2 = 14.0, izz_kgm2 = 16.0, ixy_kgm2 = 0.0, ixz_kgm2 = 0.0, iyz_kgm2 = 0.0 }

[[components]]
id = "22222222-2222-2222-2222-222222222222"
name = "Propellant Tank"
initial_mass_kg = 20.0
minimum_mass_kg = 5.0
variable_mass = true
local_center_of_mass_m = [0.0, 0.0, 0.0]
origin_body_m = [-0.25, 0.0, 0.0]
component_to_body = [1.0, 0.0, 0.0, 0.0]
parent_component = "Bus"
parent_anchor_m = [-0.25, 0.0, 0.0]
child_anchor_m = [0.0, 0.0, 0.0]
child_to_parent_zero_orientation = [1.0, 0.0, 0.0, 0.0]
dofs = []
inertia = { ixx_kgm2 = 0.80, iyy_kgm2 = 0.80, izz_kgm2 = 0.80, ixy_kgm2 = 0.0, ixz_kgm2 = 0.0, iyz_kgm2 = 0.0 }

[[components]]
id = "33333333-3333-3333-3333-333333333333"
name = "Solar Panel"
initial_mass_kg = 8.0
minimum_mass_kg = 8.0
variable_mass = false
local_center_of_mass_m = [0.60, 0.0, 0.0]
origin_body_m = [0.70, 0.0, 0.0]
component_to_body = [1.0, 0.0, 0.0, 0.0]
parent_component = "Bus"
parent_anchor_m = [0.70, 0.0, 0.0]
child_anchor_m = [0.0, 0.0, 0.0]
child_to_parent_zero_orientation = [1.0, 0.0, 0.0, 0.0]
dofs = [
  { id = "44444444-4444-4444-4444-444444444444", name = "Panel Hinge", motion = "rotation", axis = [0.0, 1.0, 0.0], initial_coordinate = 0.0, initial_rate = 0.0, minimum_coordinate = -90.0, maximum_coordinate = 90.0, maximum_absolute_rate = 12.0, maximum_absolute_effort = 0.50 }
]
inertia = { ixx_kgm2 = 0.20, iyy_kgm2 = 1.20, izz_kgm2 = 1.30, ixy_kgm2 = 0.0, ixz_kgm2 = 0.0, iyz_kgm2 = 0.0 }

[[thrusters]]
name = "Main Thruster"
mode = "commanded"
mount_component = "Bus"
propellant_component = "Propellant Tank"
application_point_component_m = [-0.40, 0.0, 0.0]
direction_component = [1.0, 0.0, 0.0]
ignition_time_mode = "elapsed"
ignition_elapsed_seconds = 0.0
never_shuts_down = true
shutdown_time_mode = "elapsed"
shutdown_elapsed_seconds = 0.0
maximum_thrust_n = 2.0

[[reaction_wheels]]
name = "Z Wheel"
mount_component = "Bus"
axis_component = [0.0, 0.0, 1.0]
initial_momentum_nms = 0.0
maximum_absolute_momentum_nms = 0.20

[control]
mode = "compiled_user_controller"
unreal_controller_id = ""
controller_dll_file = "controllers/MissionController.dll"

[gravity]
include_first_post_newtonian_correction = true

[[gravity.bodies]]
catalog_key = "Sun"
gravity_enabled = true
automatic_activation_radius_m = 0.0
barycenter_resolution_radius_m = 0.0
harmonic_model_csv_file = ""
maximum_harmonic_degree = 0

[[gravity.bodies]]
catalog_key = "EarthMoonBarycenter"
gravity_enabled = false
automatic_activation_radius_m = 0.0
barycenter_resolution_radius_m = 0.0
harmonic_model_csv_file = ""
maximum_harmonic_degree = 0

[[gravity.bodies]]
catalog_key = "Earth"
gravity_enabled = true
automatic_activation_radius_m = 0.0
barycenter_resolution_radius_m = 0.0
harmonic_model_csv_file = ""
maximum_harmonic_degree = 0

[srp]
enabled = false

[atmosphere]
enabled = false

[aerodynamics]
enabled = false
```

## 7. Complete PHAROS Scenario Physics Reference

### 7.1 `[scenario]`

| Key | Meaning |
|---|---|
| `name` | Human-readable scenario name |
| `start_utc` | Required UTC start epoch |
| `end_mode` | `duration` or `final_utc` |
| `duration_seconds` | Used when `end_mode = "duration"` |
| `final_utc` | Used when `end_mode = "final_utc"` |
| `integrator` | `fixed_step_rk4` or `adaptive_dormand_prince_54` |
| `maximum_integrator_step_seconds` | RK4 step or adaptive maximum step |
| `initial_integrator_step_seconds` | Initial adaptive step guess |
| `absolute_tolerance` | Adaptive absolute tolerance |
| `relative_tolerance` | Adaptive relative tolerance |
| `output_mode` | `every_integrator_step` or `fixed_interval` |
| `output_step_seconds` | Used for fixed-interval output |
| `maximum_integration_steps` | Numerical runaway guard |
| `maximum_output_samples` | Stored-history guard |
| `maximum_wall_clock_runtime_seconds` | Supervisor policy; direct runner does not enforce it |
| `mass_flow_convention` | Normally `thrust_includes_exhaust_momentum` |

The backend-only alternative `momentum_derivative` exists, but Unreal import
does not accept it. Use `thrust_includes_exhaust_momentum` for portable files.

### 7.2 `[initial_state]`

| Key | Type and frame |
|---|---|
| `position_icrf_m` | ICRF 3-vector [m] |
| `velocity_icrf_mps` | ICRF 3-vector [m/s] |
| `attitude_body_to_icrf` | Unit quaternion `[w,x,y,z]`, B to ICRF |
| `angular_velocity_body_radps` | 3-vector expressed in B [rad/s] |

Mass states, articulation states, and reaction-wheel momenta are initialized by
their component, DOF, and wheel records rather than repeated here.

### 7.3 `[[components]]`

Every entry is a physical rigid body. Exactly one component must have an empty
`parent_component`; that component is the root. Component names must be unique.

| Key | Meaning |
|---|---|
| `id` | Stable unique string, normally a GUID |
| `name` | Unique component name used by references |
| `initial_mass_kg` | Nonnegative initial physical mass; zero is valid for one component |
| `minimum_mass_kg` | Nonnegative dry/minimum physical mass; cannot exceed initial mass |
| `variable_mass` | Allocates a propagated mass state |
| `local_center_of_mass_m` | CM from component origin in component axes |
| `origin_body_m` | Authored component origin in B |
| `component_to_body` | Quaternion rotating component axes into B |
| `parent_component` | Parent name; empty only for the root |
| `parent_anchor_m` | Joint anchor from parent origin in parent axes |
| `child_anchor_m` | Joint anchor from child origin in child axes |
| `child_to_parent_zero_orientation` | Child axes to parent axes at zero coordinates |
| `dofs` | Ordered local array of scalar joint DOFs |
| `inertia` | Initial centroidal inertia in component axes |

Total initial spacecraft mass must be greater than zero. Its minimum reachable
mass must also remain greater than zero, using initial mass for fixed components
and minimum mass for variable-mass components.

For `variable_mass = true`, TGSimCore automatically scales the component's
centroidal inertia linearly with current mass:

```text
I_C(m) = (m / initial_mass) I_C(initial_mass)
```

The six `inertia` keys are:

```text
ixx_kgm2, iyy_kgm2, izz_kgm2, ixy_kgm2, ixz_kgm2, iyz_kgm2
```

Thrusters and reaction-wheel records add no hidden mass or inertia. Represent
their physical hardware in one or more component records.

### 7.4 Component `dofs`

Each child component may have zero or more ordered scalar DOFs:

| Key | Meaning |
|---|---|
| `id` | Stable unique string |
| `name` | User-facing DOF name |
| `motion` | `rotation` or `translation` |
| `axis` | Unit axis in the current joint frame |
| `initial_coordinate` | deg for rotation, m for translation |
| `initial_rate` | deg/s for rotation, m/s for translation |
| `minimum_coordinate` | Optional lower coordinate limit in deg or m |
| `maximum_coordinate` | Optional upper coordinate limit in deg or m |
| `maximum_absolute_rate` | Optional rate limit in deg/s or m/s |
| `maximum_absolute_effort` | N m for rotation or N for translation |

TGSCN uses degrees for rotational DOF values.

For multiple DOFs on one connection, order matters. Each DOF axis is expressed
after all preceding DOF transforms have been applied. TGSimCore topologically
sorts components parent-before-child, then flattens joint states using component
order and each component's local DOF order.

### 7.5 `[[thrusters]]`

Every thruster must reference both a physical mount component and a
variable-mass propellant component.

| Key | Meaning |
|---|---|
| `name` | Unique thruster name |
| `mode` | `prescribed_profile` or `commanded` |
| `mount_component` | Component receiving force and moment |
| `propellant_component` | Variable-mass component depleted by the thruster |
| `application_point_component_m` | Application point in mount-component axes |
| `direction_component` | Unit thrust direction in mount-component axes |
| `ignition_time_mode` | `utc` or `elapsed` |
| `ignition_utc` | Used for UTC ignition |
| `ignition_elapsed_seconds` | Used for elapsed ignition |
| `never_shuts_down` | Ignores shutdown time when true |
| `shutdown_time_mode` | `utc` or `elapsed` |
| `shutdown_utc` | Used for UTC shutdown |
| `shutdown_elapsed_seconds` | Used for elapsed shutdown |
| `maximum_thrust_n` | Required for commanded mode |

A prescribed thruster defines thrust and specific impulse directly:

```toml
[[thrusters]]
name = "Solid Motor"
mode = "prescribed_profile"
mount_component = "Bus"
propellant_component = "Propellant Tank"
application_point_component_m = [-0.4, 0.0, 0.0]
direction_component = [1.0, 0.0, 0.0]
ignition_time_mode = "elapsed"
ignition_elapsed_seconds = 10.0
never_shuts_down = false
shutdown_time_mode = "elapsed"
shutdown_elapsed_seconds = 70.0
prescribed_thrust = { source = "csv", constant_value = 0.0, csv_file = "data/thrust.csv" }
prescribed_specific_impulse = { source = "constant", constant_value = 280.0, csv_file = "" }
```

Scalar-profile sources are `constant` and `csv`. CSV interpolation is linear,
and evaluation outside the table clamps to the nearest endpoint. A thrust CSV
must start and end with zero thrust. CSV time is time since that thruster's
ignition.

A commanded thruster instead uses:

```text
thrust = maximum_thrust_n * clamp(controller_throttle, 0, 1)
mass_rate = -thrust / (controller_Isp * standard_gravity)
```

It has no throttle or Isp profile. Its controller must provide both values at
every evaluation where positive throttle is requested. On a smooth command
branch, the thruster command may also provide the total derivative of actual
outward discharge `q = thrust/(controller_Isp*standard_gravity)` in kg/s^2.
This is a controller output, not a TGSCN field.

### 7.6 `[[reaction_wheels]]`

```toml
[[reaction_wheels]]
name = "Z Wheel"
mount_component = "Bus"
axis_component = [0.0, 0.0, 1.0]
initial_momentum_nms = 0.0
maximum_absolute_momentum_nms = 0.20
```

| Key | Meaning |
|---|---|
| `mount_component` | Physical body receiving the wheel reaction |
| `axis_component` | Unit positive momentum axis in mount-component axes |
| `initial_momentum_nms` | Initial signed stored momentum [N m s] |
| `maximum_absolute_momentum_nms` | Positive saturation magnitude [N m s] |

The propagated wheel state is stored momentum, not wheel angle.

### 7.7 `[control]`

No user controller:

```toml
[control]
mode = "none"
```

Compiled controller:

```toml
[control]
mode = "compiled_user_controller"
unreal_controller_id = ""
controller_dll_file = "controllers/MissionController.dll"
```

The standalone runner uses only `controller_dll_file`. The optional
`unreal_controller_id` allows the same PHAROS scenario file to remember an entry from the
packaged application's managed Controller Library.

### 7.8 `[gravity]` and `[[gravity.bodies]]`

```toml
[gravity]
include_first_post_newtonian_correction = true

[[gravity.bodies]]
catalog_key = "Earth"
gravity_enabled = true
automatic_activation_radius_m = 0.0
barycenter_resolution_radius_m = 0.0
harmonic_model_csv_file = ""
maximum_harmonic_degree = 0
```

| Key | Meaning |
|---|---|
| `catalog_key` | Entry from the fixed PHAROS/SPICE catalog |
| `gravity_enabled` | Explicitly include this source at all distances |
| `automatic_activation_radius_m` | Include an otherwise disabled body inside this range; zero disables |
| `barycenter_resolution_radius_m` | Replace a selected system barycenter with its member bodies inside this range |
| `harmonic_model_csv_file` | Optional body-specific harmonic model |
| `maximum_harmonic_degree` | Zero selects point mass; otherwise truncates the supplied model |

TGSimCore prevents a system barycenter and its resolved member bodies from
contributing simultaneously. When any grouped physical member is listed, its
system barycenter record must also be listed exactly once, even if
`gravity_enabled = false` for that barycenter. This gives the compiler the
group relationship needed for activation and no-double-counting logic.

Supported catalog keys are:

```text
Sun
MercuryBarycenter, Mercury
VenusBarycenter, Venus
EarthMoonBarycenter, Earth, Moon
MarsBarycenter, Mars, Phobos, Deimos
JupiterBarycenter, Jupiter, Io, Europa, Ganymede, Callisto
SaturnBarycenter, Saturn, Mimas, Enceladus, Tethys, Dione, Rhea, Titan, Iapetus, Phoebe
UranusBarycenter, Uranus, Miranda, Ariel, Umbriel, Titania, Oberon
NeptuneBarycenter, Neptune, Triton
PlutoBarycenter, Pluto, Charon
Ceres, Pallas, Vesta
```

SPICE supplies body states, point-mass GM values, reference radii, and
body-fixed frames. A harmonic model CSV supplies its own GM and reference
radius because those values must match its normalized coefficients:

```text
model_GM_m3ps2, model_reference_radius_m
n, m, Cbar_nm, Sbar_nm
n, m, Cbar_nm, Sbar_nm
...
```

The file has no header.

### 7.9 `[srp]` and `[[srp.facets]]`

```toml
[srp]
enabled = true
sun_body = "Sun"
pressure_at_one_au_pa = 4.5391e-6
compute_eclipse = true
occulting_bodies = ["Earth", "Moon"]
compute_component_shadows = true
global_fallback_optics = { absorption = 0.35, specular_reflection = 0.25, diffuse_reflection = 0.40 }

[[srp.facets]]
name = "Panel triangle 0"
component_id = "33333333-3333-3333-3333-333333333333"
component_name = "Solar Panel"
stable_triangle_index = 0
logical_region = "positive_z"
vertex0_component_m = [-0.5, -0.5, 0.0]
vertex1_component_m = [0.5, -0.5, 0.0]
vertex2_component_m = [0.5, 0.5, 0.0]
optics = { absorption = 0.20, specular_reflection = 0.50, diffuse_reflection = 0.30 }
```

Facet vertices use the owning component's axes. Vertex winding defines the
outward normal. Optical fractions must be nonnegative and sum to one.
`logical_region` may be `none`, one of the six signed Cartesian faces,
`cylinder_side`, `cylinder_positive_cap`, or `cylinder_negative_cap`.

The standalone runner does not create or simplify an SRP proxy. The PHAROS scenario file must
already contain the finalized triangles. The same proxy mesh is used for loads
and optional component shadowing.

### 7.10 `[atmosphere]`

General uploaded profile:

```toml
[atmosphere]
enabled = true
central_body = "Pluto"
model = "uploaded_profile"
general_profile_csv_file = "data/pluto_atmosphere.csv"
```

Earth Cubic Harris-Priester:

```toml
[atmosphere]
enabled = true
central_body = "Earth"
model = "cubic_harris_priester_earth"
centered_average_f107_sfu = 150.0
chp_coefficient_csv_file = "data/chp_coefficients.csv"
chp_molecular_profile_csv_file = "data/chp_molecular.csv"
```

The general-profile columns are:

```text
altitude_m, density_kgpm3, temperature_K,
mean_particle_mass_kg, collision_cross_section_m2
```

### 7.11 `[aerodynamics]` and `[aerodynamics.database]`

Constant-drag fallback:

```toml
[aerodynamics]
enabled = true
reference_area_m2 = 1.5
reference_length_m = 1.2
minimum_dynamic_pressure_pa = 1.0e-12
maximum_valid_dynamic_pressure_pa = 1.0
constant_drag_fallback_enabled = true
fallback_drag_coefficient = 2.2

[aerodynamics.database]
enabled = false
```

Scattered coefficient database:

```toml
[aerodynamics.database]
enabled = true
csv_file = "data/aerodynamics.csv"
interpolation = "inverse_distance"
extrapolation = "constant_drag_fallback"
neighbor_count = 8
inverse_distance_power = 2.0
maximum_normalized_neighbor_distance = 3.0
moment_reference_center_body_m = [0.0, 0.0, 0.0]
```

`interpolation` is `inverse_distance` or `nearest_row`. `extrapolation` is
`constant_drag_fallback` or `nearest_row`.

For `N` articulated scalar DOFs, each no-header database row has `11 + N`
columns:

```text
speed_ratio, Kn, ux, uy, uz, eta_0, ..., eta_(N-1),
Cx, Cy, Cz, Cl, Cm, Cn
```

`[ux,uy,uz]` is the normalized incoming gas-flow direction in B. Force
coefficients are in B. Moment coefficients are initially about
`moment_reference_center_body_m` and are transported to the instantaneous
spacecraft CM.

### 7.12 Auxiliary CSV rules

Auxiliary input CSV files have no headers and contain only finite numeric
values.

| File | Columns |
|---|---|
| Thrust or Isp profile | `time_since_ignition_s, value` |
| Harmonic first row | `model_GM_m3ps2, model_reference_radius_m` |
| Harmonic later rows | `n, m, Cbar_nm, Sbar_nm` |
| General atmosphere | `altitude_m, density_kgpm3, temperature_K, mean_particle_mass_kg, collision_cross_section_m2` |
| CHP coefficients | `max_c0, max_c1, max_c2, max_c3, min_c0, min_c1, min_c2, min_c3` (exactly 50 rows) |
| CHP molecular profile | `altitude_m, temperature_K, mean_particle_mass_kg, collision_cross_section_m2` |
| Aerodynamic database | `speed_ratio, Kn, ux, uy, uz, eta..., Cx, Cy, Cz, Cl, Cm, Cn` |

Profile times and atmosphere altitudes must be strictly increasing.
The preferred CHP coefficient file uses the published 50-station altitude
sequence implicitly and stores coefficients in `g/km^3`. Nine-column
rows with an explicit altitude and SI coefficients are also accepted by the
standalone compiler.

### 7.13 Unreal-only records

PHAROS scenario files may also preserve component `visual` tables and SRP-proxy authoring/cache
tables. The standalone parser preserves them, but `ScenarioCompiler` does not
copy them into `SimulationRequest`. They have no numerical effect. A
backend-only user may omit them.

## 8. Writing a Controller DLL

### 8.1 Start from the supplied template

Copy:

```text
ControllerSDK/ControllerTemplate.cpp
```

to the mission directory, for example:

```text
Missions/ExampleMission/controllers/MissionController.cpp
```

Do not edit `PHAROSControllerAPI.h`. The normal one-file workflow requires the user
to edit only:

```cpp
class PHAROSUserController final
{
public:
    double NextDiscontinuityElapsedTime(
        double CurrentElapsedSimulationTimeSeconds) const
    {
        return std::numeric_limits<double>::infinity();
    }

    void ComputeControl(
        const PHAROSControlInput& Input,
        PHAROSControlOutput& Output)
    {
        // User control law.
    }
};
```

Keep every exported wrapper function from the template unchanged.

### 8.2 Compile with the supplied standalone controller compiler

From the PHAROS project root, the following PowerShell command reproduces the
compiler settings used by the packaged Controller Library:

```powershell
& ".\Build\ControllerToolchain\Win64\bin\x86_64-w64-mingw32-clang++.exe" `
  -std=c++17 `
  -O2 `
  -DNDEBUG `
  -DTG_CONTROLLER_BUILD=1 `
  -shared `
  -static `
  -static-libgcc `
  -static-libstdc++ `
  "-Wl,--no-undefined" `
  "-I.\ControllerSDK" `
  ".\Missions\ExampleMission\controllers\MissionController.cpp" `
  -o ".\Missions\ExampleMission\controllers\MissionController.dll"
```

For a portable SDK directory, preserve the complete
`Build/ControllerToolchain/Win64` tree and adjust the compiler and include
paths. Do not copy only `clang++.exe`; the compiler uses the rest of its
toolchain directory.

After compiling, validate the scenario:

```powershell
& ".\Tools\TGScenarioRunner\bin\PHAROSScenarioRunner.exe" `
  ".\Missions\ExampleMission\mission.tgscn" `
  --validate-only
```

The runner checks these required DLL exports and their structural contract:

```cpp
uint32_t PHAROS_DescribeControllerContract(
    PHAROSControllerContract* Contract);
void* PHAROS_CreateController();
void PHAROS_DestroyController(void* Controller);
uint32_t PHAROS_ComputeControl(
    void* Controller,
    const PHAROSControlInput* Input,
    PHAROSControlOutput* Output);
```

The current template also supplies this optional export:

```cpp
double PHAROS_NextControllerDiscontinuityElapsedTime(
    void* Controller,
    double CurrentElapsedSimulationTimeSeconds);
```

Use it when `ComputeControl` contains a hard elapsed-time switch. For example,
if a command is active while `Input.ElapsedSimulationTimeSeconds < 10.0`, return
`10.0` while the current time is below 10 seconds and positive infinity
afterward. This makes the propagator land on the switch from the left before it
starts the next step with the new command. Smooth controllers may leave the
template implementation unchanged.

Controllers must be built with the SDK distributed with PHAROS. Native
controller code runs with the operating-system permissions of the runner and
must be treated as trusted code.

## 9. Controller Evaluation Contract

TGSimCore calls the controller at every dynamics right-hand-side evaluation,
including intermediate Runge-Kutta stages. Adaptive integration may evaluate a
trial state more than once or evaluate rejected stages. Stage times are not a
guaranteed monotonic sequence.

Therefore:

- Compute commands from the supplied time and state whenever possible.
- Do not advance a private controller integrator merely because
  `ComputeControl` was called.
- Tolerate repeated calls at the same time.
- Treat every input pointer as read-only and valid only for that call.
- Do not retain pointers into `PHAROSControlInput`.
- Output arrays are owned, correctly sized, and zero-initialized by PHAROS.
- Do not replace output pointers or change output counts.
- Commands left untouched remain zero for that evaluation.
- Announce every known hard time switch through
  `NextDiscontinuityElapsedTime`; do not use it for smooth command changes.
- Compute mass-flow derivatives from the same current-stage input and command;
  do not advance private controller state while producing them.

`PHAROSStringView` is a pointer plus length and is not guaranteed to be
null-terminated. `PHAROSMat3` is row-major:

```text
element(row,column) = M[row * 3 + column]
```

## 10. Information Supplied to the Controller

### 10.1 Top-level input

| Field | Meaning |
|---|---|
| `Input.EphemerisTimeTdbSeconds` | Current absolute SPICE ET |
| `Input.ElapsedSimulationTimeSeconds` | Seconds since scenario start |
| `Input.Configuration` | Read-only run configuration |
| `Input.SpacecraftState` | Current propagated truth state |
| `Input.CenterOfMassBodyMeters` | Current total CM in B |
| `Input.InertiaBodyKilogramMetersSquared` | Current total inertia about CM in B |
| `Input.Components`, `ComponentCount` | Current physical-component views |
| `Input.Thrusters`, `ThrusterCount` | Thruster definitions and availability |
| `Input.Joints`, `JointCount` | Flattened scalar DOF definitions and states |
| `Input.ReactionWheels`, `ReactionWheelCount` | Wheel definitions and states |
| `Input.CelestialBodies`, `CelestialBodyCount` | Current SPICE body states and metadata |

### 10.2 `Input.SpacecraftState`

```text
EphemerisTimeTdbSeconds
PositionIcrfMeters
VelocityIcrfMetersPerSecond
AttitudeBodyToIcrf
AngularVelocityBodyRadiansPerSecond
TotalMassKilograms
VariableComponentMassesKilograms[]
ArticulationCoordinates[]
ArticulationRates[]
ReactionWheelMomentaNewtonMeterSeconds[]
```

### 10.3 Each component view

```text
ComponentIndex
ParentComponentIndex
VariableMassStateIndex
Name
CurrentMassKilograms
InitialMassKilograms
MinimumMassKilograms
HasVariableMass
CurrentOriginBodyMeters
CurrentComponentToBody
LocalCenterOfMassMeters
LocalCentroidalInertiaKilogramMetersSquared
```

`PHAROS_CONTROLLER_INVALID_INDEX` represents no parent or no variable-mass state.

### 10.4 Each thruster view

```text
ThrusterIndex
Name
Mode
MountComponentIndex
PropellantComponentIndex
ApplicationPointComponentMeters
DirectionComponent
IgnitionEphemerisTimeTdbSeconds
ShutdownEphemerisTimeTdbSeconds
MaximumThrustNewtons
CurrentPropellantComponentMassKilograms
FiringWindowOpen
HasPropellantComponent
PropellantAvailable
```

### 10.5 Each joint view

```text
ArticulationStateIndex
ChildComponentIndex
LocalDegreeOfFreedomIndex
Name
MotionType
AxisJoint
Coordinate
Rate
MinimumCoordinate
MaximumCoordinate
MaximumAbsoluteRate
MaximumAbsoluteEffort
AtLowerLimit
AtUpperLimit
```

`MotionType` is `PHAROS_JOINT_ROTATION` or `PHAROS_JOINT_TRANSLATION`.

### 10.6 Each reaction-wheel view

```text
ReactionWheelIndex
Name
MountComponentIndex
AxisComponent
CurrentMomentumNewtonMeterSeconds
MaximumAbsoluteMomentumNewtonMeterSeconds
SaturatedNegative
SaturatedPositive
```

### 10.7 Each celestial-body view

```text
CelestialBodyIndex
Name
NaifId
GravitySourceRole
GravityExplicitlyEnabled
AutomaticActivationRadiusMeters
BarycenterResolutionRadiusMeters
GravitationalParameterMetersCubedPerSecondSquared
ReferenceRadiusMeters
PositionIcrfMeters
VelocityIcrfMetersPerSecond
BodyFixedToIcrf
```

The configuration view additionally exposes solver settings and relevant
gravity, SRP, atmosphere, and aerodynamic settings. The exact ABI definition is
`ControllerSDK/PHAROSControllerAPI.h`.

## 11. Everything the Controller Can Command

There are four command channels.

### 11.1 Commanded thrusters

```cpp
Output.ThrusterCommands[index].Throttle = 0.40;
Output.ThrusterCommands[index].SpecificImpulseSeconds = 300.0;
```

- The output index matches the corresponding `Input.Thrusters[index]`.
- Only `PHAROS_THRUSTER_COMMANDED` thrusters use these values.
- Prescribed-profile thrusters ignore controller commands.
- Throttle is clamped to `[0,1]`.
- Positive throttle requires finite, positive Isp in seconds.
- The backend multiplies throttle by `MaximumThrustNewtons`.
- The backend checks the firing window and available propellant.

For a smooth analytic command, write the derivative in the same command:

```cpp
Output.ThrusterCommands[index].MassFlowDerivativeProvided = 1;
Output.ThrusterCommands[index]
    .MassFlowDerivativeKilogramsPerSecondSquared = qDot;
```

`qDot` is the signed total derivative of actual outward discharge in kg/s^2,
after command limiting. It is not a throttle derivative or an additional
thrust command. Leave `MassFlowDerivativeProvided` false when unavailable.
PHAROS then uses zero only for that thruster's owner-component `m_ddot`
contribution while retaining
thrust, mass depletion, inertia-rate effects, and both velocity-dependent CM
terms. For state-dependent commands, include the state dependence in the total
derivative.

### 11.2 Articulated DOFs

```cpp
Output.JointEffortsNewtonMetersOrNewtons[index] = effort;
```

- The output index matches `Input.Joints[index]`.
- A rotational DOF receives torque [N m].
- A translational DOF receives force [N].
- Positive effort acts along the DOF's positive `AxisJoint` convention.
- The backend clamps effort to `MaximumAbsoluteEffort`.
- Joint coordinates, rates, limits, and active-limit flags are available in
  `Input.Joints[index]`.
- The articulated-body solver computes the resulting joint acceleration and
  coupled free-base acceleration. The controller must not prescribe
  acceleration directly.

### 11.3 Reaction wheels

```cpp
Output.ReactionWheelMomentumRatesNewtonMeters[index] = momentumRate;
```

- The output index matches `Input.ReactionWheels[index]`.
- The command is wheel stored-momentum derivative [N m].
- Positive command increases wheel momentum along `AxisComponent`.
- The spacecraft receives the corresponding reaction through multibody
  dynamics.
- At positive saturation, commands that further increase positive momentum are
  suppressed; commands back toward zero remain allowed. The negative limit is
  handled symmetrically.

### 11.4 Direct additional external body torque

```cpp
Output.AdditionalExternalTorqueBodyNewtonMeters = {Tx, Ty, Tz};
```

This is a direct spacecraft-level external torque expressed in B [N m]. It is
not attached to a modeled thruster, wheel, or physical component. Use it only
when an intentional externally supplied torque is desired. For physically
modeled actuation, prefer thruster, joint, and reaction-wheel commands.

The controller cannot directly overwrite position, velocity, attitude,
angular velocity, mass, joint coordinate, joint rate, or wheel momentum. It
supplies actuator inputs; TGSimCore propagates the state.

## 12. Complete Controller Example

This controller matches the names in the scenario from Section 6. It uses a
short main-engine burn, a panel-angle PD controller, and a wheel-based body-rate
damper. It also shows, but leaves disabled, the direct external-torque channel.

```cpp
#include "PHAROSControllerAPI.h"

#include <cmath>
#include <cstring>
#include <new>

namespace
{
    double Clamp(
        const double value,
        const double minimum,
        const double maximum)
    {
        return value < minimum
            ? minimum
            : (value > maximum ? maximum : value);
    }

    bool Equals(const PHAROSStringView view, const char* text)
    {
        const size_t length = std::strlen(text);
        return view.Length == static_cast<uint64_t>(length) &&
            (length == 0 ||
             (view.Data != nullptr &&
              std::memcmp(view.Data, text, length) == 0));
    }

    uint64_t FindThruster(
        const PHAROSControlInput& input,
        const char* name)
    {
        for (uint64_t index = 0; index < input.ThrusterCount; ++index)
        {
            if (Equals(input.Thrusters[index].Name, name)) return index;
        }
        return PHAROS_CONTROLLER_INVALID_INDEX;
    }

    uint64_t FindJoint(
        const PHAROSControlInput& input,
        const char* name)
    {
        for (uint64_t index = 0; index < input.JointCount; ++index)
        {
            if (Equals(input.Joints[index].Name, name)) return index;
        }
        return PHAROS_CONTROLLER_INVALID_INDEX;
    }

    uint64_t FindWheel(
        const PHAROSControlInput& input,
        const char* name)
    {
        for (uint64_t index = 0; index < input.ReactionWheelCount; ++index)
        {
            if (Equals(input.ReactionWheels[index].Name, name)) return index;
        }
        return PHAROS_CONTROLLER_INVALID_INDEX;
    }
}

class PHAROSUserController final
{
public:
    void ComputeControl(
        const PHAROSControlInput& input,
        PHAROSControlOutput& output)
    {
        const uint64_t thrusterIndex = FindThruster(input, "Main Thruster");
        if (thrusterIndex != PHAROS_CONTROLLER_INVALID_INDEX &&
            thrusterIndex < output.ThrusterCommandCount)
        {
            const PHAROSThrusterStateView& thruster =
                input.Thrusters[thrusterIndex];

            const bool burnRequested =
                input.ElapsedSimulationTimeSeconds < 20.0;

            if (burnRequested &&
                thruster.Mode == PHAROS_THRUSTER_COMMANDED &&
                thruster.FiringWindowOpen != 0 &&
                thruster.PropellantAvailable != 0)
            {
                output.ThrusterCommands[thrusterIndex].Throttle = 0.40;
                output.ThrusterCommands[thrusterIndex]
                    .SpecificImpulseSeconds = 300.0;
            }
        }

        const uint64_t jointIndex = FindJoint(input, "Panel Hinge");
        if (jointIndex != PHAROS_CONTROLLER_INVALID_INDEX &&
            jointIndex < output.JointEffortCount)
        {
            const PHAROSJointStateView& joint = input.Joints[jointIndex];
            const double targetAngle = 0.40 * std::sin(
                0.02 * input.ElapsedSimulationTimeSeconds);
            const double targetRate = 0.008 * std::cos(
                0.02 * input.ElapsedSimulationTimeSeconds);

            const double proportional =
                0.30 * (targetAngle - joint.Coordinate);
            const double derivative =
                0.10 * (targetRate - joint.Rate);

            output.JointEffortsNewtonMetersOrNewtons[jointIndex] =
                Clamp(
                    proportional + derivative,
                    -joint.MaximumAbsoluteEffort,
                    joint.MaximumAbsoluteEffort);
        }

        const uint64_t wheelIndex = FindWheel(input, "Z Wheel");
        if (wheelIndex != PHAROS_CONTROLLER_INVALID_INDEX &&
            wheelIndex < output.ReactionWheelMomentumRateCount)
        {
            const PHAROSReactionWheelStateView& wheel =
                input.ReactionWheels[wheelIndex];
            const double bodyRateZ = input.SpacecraftState
                .AngularVelocityBodyRadiansPerSecond.Z;

            // Positive wheel h_dot creates the opposite spacecraft reaction.
            double momentumRate = Clamp(
                0.02 * bodyRateZ,
                -2.0e-4,
                2.0e-4);

            if ((wheel.SaturatedPositive != 0 && momentumRate > 0.0) ||
                (wheel.SaturatedNegative != 0 && momentumRate < 0.0))
            {
                momentumRate = 0.0;
            }

            output.ReactionWheelMomentumRatesNewtonMeters[wheelIndex] =
                momentumRate;
        }

        // Optional direct external torque in body axes. It is intentionally
        // disabled because this example uses physical actuators.
        output.AdditionalExternalTorqueBodyNewtonMeters =
            {0.0, 0.0, 0.0};
    }
};

PHAROS_CONTROLLER_EXPORT uint32_t PHAROS_CONTROLLER_CALL
PHAROS_DescribeControllerContract(PHAROSControllerContract* contract)
{
    if (contract == nullptr ||
        contract->StructSize != sizeof(PHAROSControllerContract))
    {
        return PHAROS_CONTROLLER_RESULT_INVALID_ARGUMENT;
    }

    *contract = PHAROSControllerContract{};
    contract->StructSize = sizeof(PHAROSControllerContract);
    contract->ControlInputSize = sizeof(PHAROSControlInput);
    contract->ControlOutputSize = sizeof(PHAROSControlOutput);
    contract->ThrusterCommandSize = sizeof(PHAROSThrusterCommand);
    contract->SimulationConfigurationSize =
        sizeof(PHAROSSimulationConfiguration);
    contract->SpacecraftStateViewSize = sizeof(PHAROSSpacecraftStateView);
    contract->ComponentStateViewSize = sizeof(PHAROSComponentStateView);
    contract->JointStateViewSize = sizeof(PHAROSJointStateView);
    contract->ThrusterStateViewSize = sizeof(PHAROSThrusterStateView);
    contract->ReactionWheelStateViewSize =
        sizeof(PHAROSReactionWheelStateView);
    contract->CelestialBodyStateViewSize =
        sizeof(PHAROSCelestialBodyStateView);
    return PHAROS_CONTROLLER_RESULT_OK;
}

PHAROS_CONTROLLER_EXPORT void* PHAROS_CONTROLLER_CALL
PHAROS_CreateController(void)
{
    return new (std::nothrow) PHAROSUserController();
}

PHAROS_CONTROLLER_EXPORT void PHAROS_CONTROLLER_CALL
PHAROS_DestroyController(void* controller)
{
    delete static_cast<PHAROSUserController*>(controller);
}

PHAROS_CONTROLLER_EXPORT uint32_t PHAROS_CONTROLLER_CALL
PHAROS_ComputeControl(
    void* controller,
    const PHAROSControlInput* input,
    PHAROSControlOutput* output)
{
    if (controller == nullptr || input == nullptr || output == nullptr)
    {
        return PHAROS_CONTROLLER_RESULT_INVALID_ARGUMENT;
    }

    if (input->StructSize != sizeof(PHAROSControlInput) ||
        output->StructSize != sizeof(PHAROSControlOutput))
    {
        return PHAROS_CONTROLLER_RESULT_INVALID_ARGUMENT;
    }

    try
    {
        static_cast<PHAROSUserController*>(controller)->ComputeControl(
            *input,
            *output);
    }
    catch (...)
    {
        return PHAROS_CONTROLLER_RESULT_USER_EXCEPTION;
    }

    return PHAROS_CONTROLLER_RESULT_OK;
}
```

Names are searched rather than assuming a fixed index. This is useful because
the scenario compiler topologically sorts component trees. Within one compiled
run, all reported indices remain stable.

## 13. Controller Limits and Failure Behavior

- Every output starts at zero on every evaluation.
- Non-finite commands are rejected by the host adapter.
- Thruster throttle is clamped to `[0,1]`.
- Positive throttle with zero or negative Isp is rejected.
- Joint effort is clamped to the DOF's maximum absolute effort.
- Wheel commands that push farther into saturation are suppressed.
- Prescribed thrusters ignore controller output.
- An invalid derivative field is discarded without suppressing otherwise valid
  commands. Missing derivatives are never finite-differenced.
- Finite derivatives apply on smooth command branches and do not represent the
  impulse associated with an ideal instantaneous jump in mass-flow rate.
- A controller callback returning a nonzero result leaves commands at zero for
  that evaluation and reports a runtime message.
- A missing DLL, missing export, incompatible controller contract, or failed controller
  creation stops the run with exit code `4`.
- An uncontrolled commanded thruster receives zero throttle.
- An uncontrolled joint receives zero applied effort but still responds to
  coupled multibody and external dynamics.
- An uncontrolled wheel keeps its current stored momentum except for numerical
  limit enforcement.

## 14. End-to-End Checklist

1. Put `PHAROSScenarioRunner.exe` and the complete `SPICEKernels` folder in the
   standalone runtime directory, or run from the PHAROS project root.
2. Create the mission directory and `.tgscn`.
3. Keep all paths relative to the `.tgscn` when portability matters.
4. Define a valid physical component tree with exactly one root.
5. Make every thruster reference a variable-mass propellant component.
6. For a controller, copy `ControllerTemplate.cpp`, edit only the user class,
   and compile it into a DLL against the current `PHAROSControllerAPI.h`.
7. Set `[control].mode = "compiled_user_controller"` and point
   `controller_dll_file` to that DLL.
8. Run `--validate-only` and resolve every reported error.
9. Run propagation with a unique output directory.
10. Check the process exit code and `*_summary.txt` before consuming
    `*_solution.csv`.
11. Preserve the exact `.tgscn`, controller DLL/source, auxiliary CSV files,
    kernel version, runner version, and result directory for reproducibility.

## 15. Implementation References

The source-of-truth implementation files are:

```text
Tools/TGScenarioRunner/TGScenarioRunnerMain.cpp
Tools/TGScenarioRunner/StandaloneSpiceProvider.cpp
Tools/TGScenarioRunner/StandaloneControllerAdapter.cpp
Source/TGSimCore/src/Scenario/ScenarioFile.cpp
Source/TGSimCore/src/Scenario/ScenarioCompiler.cpp
ControllerSDK/PHAROSControllerAPI.h
ControllerSDK/ControllerTemplate.cpp
```
