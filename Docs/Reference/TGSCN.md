# PHAROS Scenario File Format

`*.tgscn` is the portable, editable PHAROS scenario format. It is the common
scenario representation used by the application and by PHAROS Scenario Runner.
The extension and `format = "TGSCN"` marker identify the format.

The file syntax is TOML 1.0. PHAROS defines the keys and their physical
meaning; TOML only supplies dependable parsing of tables, arrays, strings,
numbers, booleans, and comments. The implementation uses the single-header
[toml++ 3.4.0](https://github.com/marzer/tomlplusplus/tree/v3.4.0) parser.

## 1. Data Flow

```text
FTGSimulationScenario (Unreal)
       <-> ScenarioDocument <-> .tgscn
                   |
                   +-> ScenarioCompiler -> SimulationRequest -> PHAROS simulation engine
```

`ScenarioDocument` contains both physics inputs and Unreal presentation data.
`ScenarioCompiler` deliberately copies only physics inputs into
`SimulationRequest`. Visual meshes, material files, display colors, Unreal
controller IDs, the initial-state authoring-frame preference, and SRP-proxy
authoring/cache fields survive a save/load cycle but never enter the numerical
solver.

The canonical implementation is located in:

- `Source/TGSimCore/include/TGSim/Scenario/ScenarioDocument.h`
- `Source/TGSimCore/src/Scenario/ScenarioFile.cpp`
- `Source/TGSimCore/src/Scenario/ScenarioCompiler.cpp`
- `Source/TG/Private/Simulation/TGScenarioDocumentAdapter.cpp`

## 2. Global Rules

| Subject | Rule |
|---|---|
| File encoding | UTF-8 text |
| File marker | `format = "TGSCN"` |
| Comments | Start with `#` |
| Units | SI except rotational DOF angles and angular rates, which use degrees |
| Translation frame | Barycentric J2000/ICRF |
| Attitude | Rotates spacecraft-body components into ICRF |
| Quaternion order | `[w, x, y, z]` |
| Body quantities | Suffix `_body` means main-body frame B |
| Component quantities | Suffix `_component` means that component's local axes |
| Joint axes | Expressed in the joint frame |
| Array order | Preserved, except components are topologically sorted for the backend |

Use finite decimal or scientific-notation numbers. Parsing, semantic
compilation, and `SimulationEngine` request validation reject invalid masses,
directions, limits, profile rows, references, and incompatible feature
combinations before numerical integration begins.

## 3. Time

Authored absolute epochs are quoted ISO-8601 UTC strings, which CSPICE converts
to the backend time coordinate, for example:

```toml
start_utc = "2030-01-01T00:00:00Z"
```

They are strings rather than TOML date-time values so the text is preserved
exactly. At compilation, CSPICE converts UTC to ET, numerically TDB seconds past
J2000, which is the time coordinate propagated by the PHAROS simulation engine.

The simulation end is selected with either:

```toml
end_mode = "duration"
duration_seconds = 3600.0
```

or:

```toml
end_mode = "final_utc"
final_utc = "2030-01-01T01:00:00Z"
```

Thruster ignition and shutdown independently accept `"utc"` or `"elapsed"`.
Elapsed thruster times are seconds after `scenario.start_utc`. CSV profile time
is seconds since that thruster's ignition.

## 4. Paths

Every file key accepts either an absolute path or a relative path. Relative
paths are resolved from the directory containing the `.tgscn` file, never from
the process working directory. This applies to STL files, textures, controller
DLLs, gravity models, propulsion profiles, atmosphere files, and aerodynamic
databases.

Portable mission folders should prefer relative paths:

```toml
controller_dll_file = "controllers/OrbitController.dll"
stl_file = "geometry/bus.stl"
```

Absolute Windows paths should be TOML literal strings to avoid escaping each
backslash:

```toml
controller_dll_file = 'C:\Mission\Controllers\OrbitController.dll'
```

## 5. Top-Level Structure

```toml
format = "TGSCN"
generator = "optional authoring-tool name"

[scenario]
[initial_state]
[[components]]
[[thrusters]]
[[reaction_wheels]]
[control]
[gravity]
[[gravity.bodies]]
[srp]
[[srp.facets]]
[atmosphere]
[aerodynamics]
[aerodynamics.database]
[[aerodynamics.database.rows]]
```

Repeated sections use TOML arrays of tables (`[[...]]`). Empty actuator, body,
facet, and database-row arrays are valid when the corresponding feature does
not need them. `scenario`, `initial_state`, and at least one physical component
are required for a runnable simulation.

## 6. Scenario and Initial State

### `[scenario]`

| Key | Type | Values or meaning |
|---|---|---|
| `name` | string | Scenario label |
| `start_utc` | string | Required UTC epoch |
| `end_mode` | enum | `duration`, `final_utc` |
| `final_utc` | string | Used by `final_utc` mode |
| `duration_seconds` | number | Used by `duration` mode |
| `integrator` | enum | `fixed_step_rk4`, `adaptive_dormand_prince_54` |
| `maximum_integrator_step_seconds` | number | RK4 step or adaptive upper bound |
| `initial_integrator_step_seconds` | number | Initial adaptive step guess |
| `absolute_tolerance` | number | Adaptive absolute tolerance |
| `relative_tolerance` | number | Adaptive relative tolerance |
| `output_mode` | enum | `every_integrator_step`, `fixed_interval` |
| `output_step_seconds` | number | Used by fixed-interval output |
| `maximum_integration_steps` | integer | Runaway guard |
| `maximum_output_samples` | integer | Stored-history guard |
| `maximum_wall_clock_runtime_seconds` | number | Frontend/supervisor real-time deadline; preserved but not compiled into physics |
| `mass_flow_convention` | enum | `thrust_includes_exhaust_momentum`; the backend-only advanced alternative is `momentum_derivative` |

The current Unreal `FTGSimulationScenario` exposes only
`thrust_includes_exhaust_momentum`. A backend-only file may select
`momentum_derivative`, but Unreal import rejects that file rather than silently
changing its physics.

`maximum_wall_clock_runtime_seconds` is execution policy rather than a physical
input. Unreal uses it to supervise the isolated runner process. A direct CLI
caller remains responsible for imposing its own operating-system process
deadline when a hard timeout is required.

### `[initial_state]`

| Key | Shape | Unit/frame |
|---|---|---|
| `position_icrf_m` | 3-vector | m, ICRF |
| `velocity_icrf_mps` | 3-vector | m/s, ICRF |
| `attitude_body_to_icrf` | quaternion | `[w,x,y,z]`, B to ICRF |
| `angular_velocity_body_radps` | 3-vector | rad/s, B |
| `authoring_frame` | string | Optional celestial catalog key used to restore the Unreal input frame |

Component masses, joint states, and wheel momenta are assembled from the
component/actuator records; they are not duplicated in `[initial_state]`.
`authoring_frame` is presentation metadata only. Omitting it, or writing an
empty string, selects ICRF. The four physical state fields always remain in
their canonical frames shown above.

## 7. Components and Joints

Every `[[components]]` entry is one physical rigid body. Thrusters and reaction
wheels are behavior records mounted on these bodies and add no hidden mass or
inertia.

| Key | Type | Meaning |
|---|---|---|
| `id` | string | Stable Unreal identity, normally a GUID |
| `name` | string | Unique component name used by references |
| `initial_mass_kg` | nonnegative number | Initial physical mass; zero is valid for an individual component |
| `minimum_mass_kg` | nonnegative number | Dry/minimum mass; cannot exceed initial mass |
| `variable_mass` | boolean | Allocates a mass state and automatically uses \(I_C(m)=mI_{C,0}/m_0\) |
| `local_center_of_mass_m` | 3-vector | CM from component origin, component axes |
| `origin_body_m` | 3-vector | Root placement in B; retained for every component |
| `component_to_body` | quaternion | Root local axes to B |
| `parent_component` | string | Empty only for the unique root |
| `parent_anchor_m` | 3-vector | Joint anchor from parent origin, parent axes |
| `child_anchor_m` | 3-vector | Joint anchor from child origin, child axes |
| `child_to_parent_zero_orientation` | quaternion | Child axes to parent axes at zero joint coordinates |
| `inertia` | table | Symmetric centroidal inertia |
| `dofs` | table array | Ordered joint DOFs connecting this child to its parent |

Only `variable_mass` controls whether component inertia scales with mass.
Unsupported schema fields such as `scale_inertia_with_mass` are rejected.

The sum of all initial component masses must be greater than zero. The minimum
reachable spacecraft mass must also be greater than zero: fixed components
contribute `initial_mass_kg`, while variable-mass components contribute
`minimum_mass_kg`. These rules allow massless visual or structural components
without permitting a massless spacecraft at startup or after depletion.
| `visual` | table | Unreal-only appearance/import data |
| `srp` | table | Unreal-only SRP-proxy authoring/cache data |

`inertia` keys are `ixx_kgm2`, `iyy_kgm2`, `izz_kgm2`, `ixy_kgm2`,
`ixz_kgm2`, and `iyz_kgm2`.

Each entry in `dofs` has:

| Key | Type | Meaning |
|---|---|---|
| `id` | string | Stable Unreal identity |
| `name` | string | User-facing DOF name |
| `motion` | enum | `rotation` or `translation` |
| `axis` | 3-vector | Unit axis in joint coordinates |
| `initial_coordinate` | number | deg for rotation, m for translation |
| `initial_rate` | number | deg/s for rotation, m/s for translation |
| `minimum_coordinate` | optional number | deg for rotation, m for translation; omit for no lower bound |
| `maximum_coordinate` | optional number | deg for rotation, m for translation; omit for no upper bound |
| `maximum_absolute_rate` | optional number | deg/s for rotation, m/s for translation; omit for no finite rate limit |
| `maximum_absolute_effort` | optional number | N m for rotation, N for translation |

Version 2 uses degrees and degrees per second at the file boundary; TGSimCore
converts them to canonical radians when parsing. Files using any other format
version are rejected.

The file may list a child before its parent. The compiler verifies a unique,
acyclic tree and produces parent-before-child backend order. Aerodynamic eta
vectors use the resulting component order and each component's local DOF order.

### Unreal-only `visual`

The complete visual table is preserved for HUD import but ignored by
`SimulationRequest`:

```text
geometry_source = none | primitive | custom_stl
primitive_type = box | sphere | cylinder
box_dimensions_m, sphere_radius_m, cylinder_radius_m, cylinder_length_m
stl_file, stl_length_unit, stl_recenter_mode
visual_offset_m, visual_orientation, visual_scale
surface_appearance = solid_color | textured
display_color, base_color_tint                 # [r,g,b,a]
base_color_texture_file, normal_texture_file
roughness_texture_file, metallic_texture_file
visible
```

`stl_length_unit` is `millimeters`, `centimeters`, or `meters`.
`stl_recenter_mode` is `keep_imported_origin`, `center_on_bounds`, or
`place_base_at_origin`.

### Unreal-only component `srp`

This table preserves proxy-generation choices and cache state:

```text
included_in_proxy
proxy_resolution = automatic | custom
custom_target_triangle_count
use_global_fallback_optics
one_optical_configuration
optics = { absorption, specular_reflection, diffuse_reflection }
logical_region_overrides = [{ region, optics }, ...]
triangle_overrides = [{ triangle_index, optics }, ...]
generated_geometry_signature
generated_triangle_count
proxy_generation_required
last_proxy_generation_message
```

These are authoring inputs, not propagated facets. Only the finalized
`[[srp.facets]]` array enters TGSimCore.

## 8. Thrusters and Wheels

### `[[thrusters]]`

| Key | Meaning |
|---|---|
| `name` | Unique thruster label |
| `mode` | `prescribed_profile` or `commanded` |
| `mount_component` | Physical component receiving the force |
| `propellant_component` | Variable-mass component depleted by this thruster |
| `application_point_component_m` | Point on mount component, its local axes |
| `direction_component` | Unit thrust direction in mount-component axes |
| `ignition_time_mode` | `utc` or `elapsed` |
| `ignition_utc` / `ignition_elapsed_seconds` | Selected ignition value |
| `never_shuts_down` | Ignores shutdown fields when true |
| `shutdown_time_mode` | `utc` or `elapsed` |
| `shutdown_utc` / `shutdown_elapsed_seconds` | Selected shutdown value |
| `maximum_thrust_n` | Required only for commanded mode |
| `prescribed_thrust` | Scalar profile used only by prescribed mode |
| `prescribed_specific_impulse` | Scalar profile used only by prescribed mode |

A scalar profile is:

```toml
prescribed_thrust = {
    source = "constant",
    constant_value = 10.0,
    csv_file = ""
}
```

or `source = "csv"` with `csv_file`. Interpolation is always linear and values
outside the table clamp to the closest endpoint. A thrust CSV must begin and
end at zero. A commanded thruster receives throttle and instantaneous Isp from
the compiled controller DLL; it has no authored throttle or Isp curve. A
controller may additionally provide the optional analytic derivative of actual
outward discharge in the `PHAROSThrusterCommand` output. That derivative is evaluated
at runtime and is deliberately not a TGSCN field.

Every thruster must name a variable-mass propellant component. TGSimCore sums
the resulting mass-flow rates and never creates an invisible propellant source.

### `[[reaction_wheels]]`

| Key | Meaning |
|---|---|
| `name` | Unique wheel label |
| `mount_component` | Physical component containing the wheel |
| `axis_component` | Unit spin axis in mount-component axes |
| `initial_momentum_nms` | Initial stored angular momentum |
| `maximum_absolute_momentum_nms` | Positive saturation magnitude |

## 9. Controller

```toml
[control]
mode = "compiled_user_controller"
unreal_controller_id = "optional Unreal library ID"
controller_dll_file = "controllers/OrbitController.dll"
```

`mode` is `none` or `compiled_user_controller`. Unreal uses the opaque ID to
reselect a managed controller library entry. The standalone runner uses the DLL
path and validates the exports and structural contract defined by
`ControllerSDK/PHAROSControllerAPI.h`. Both values are preserved so one file
can be used in both environments.

Controller source code is not embedded in `.tgscn`, and the file cannot request
a build. A controller must already be compiled before a run.

## 10. Gravity

```toml
[gravity]
include_first_post_newtonian_correction = true

[[gravity.bodies]]
catalog_key = "Earth"
gravity_enabled = true
automatic_activation_radius_m = 0.0
barycenter_resolution_radius_m = 0.0
harmonic_model_csv_file = "gravity/earth_egm.csv"
maximum_harmonic_degree = 8
```

Bodies come from the fixed SPICE/catalog list; users do not author names, NAIF
IDs, reference frames, point-mass GM, or SPICE radii. The compiler resolves
those automatically. A harmonic CSV supplies its own gravity-model GM and
reference radius because they must match the normalized coefficients.

The backend applies explicit selection, automatic activation, and barycenter
resolution while ensuring that a system barycenter and resolved member bodies
are never counted simultaneously.

## 11. Solar Radiation Pressure

`[srp]` fields are:

```text
enabled
sun_body
pressure_at_one_au_pa
compute_eclipse
occulting_bodies                 # string array of catalog references
compute_component_shadows
global_fallback_optics           # optics table, authoring fallback
facets                           # finalized proxy triangles
```

Each `[[srp.facets]]` entry contains:

```text
name
component_id
component_name
stable_triangle_index
logical_region
vertex0_component_m
vertex1_component_m
vertex2_component_m
optics = { absorption, specular_reflection, diffuse_reflection }
```

The vertices use the owning component's axes. Winding defines the normal.
Optical fractions must be nonnegative and sum to one. `logical_region` accepts
`none`, the six signed Cartesian faces, `cylinder_side`,
`cylinder_positive_cap`, or `cylinder_negative_cap`.

The Unreal preprocessing workflow creates these proxy facets from primitive or
STL geometry before propagation. TGSimCore evaluates load and shadowing on this
same proxy mesh; it does not simplify render geometry at runtime.

## 12. Atmosphere and Aerodynamics

### `[atmosphere]`

```text
enabled
central_body
model = uploaded_profile | cubic_harris_priester_earth
general_profile_csv_file
centered_average_f107_sfu
chp_coefficient_csv_file
chp_molecular_profile_csv_file
```

The uploaded model is general for any catalog body. Cubic Harris-Priester is an
Earth-only moderate-fidelity option.

### `[aerodynamics]`

```text
enabled
reference_area_m2
reference_length_m
minimum_dynamic_pressure_pa
maximum_valid_dynamic_pressure_pa
constant_drag_fallback_enabled
fallback_drag_coefficient
```

### `[aerodynamics.database]`

```text
enabled
csv_file
interpolation = inverse_distance | nearest_row
extrapolation = constant_drag_fallback | nearest_row
neighbor_count
inverse_distance_power
maximum_normalized_neighbor_distance     # optional
moment_reference_center_body_m
rows                                     # inline alternative to csv_file
```

Each inline row contains `speed_ratio`, `knudsen_number`,
`gas_flow_direction_body`, `articulation_coordinates`,
`force_coefficients_body`, and `moment_coefficients_body`.

The flow vector points in the incoming gas-flow direction expressed in B and
is normalized by the compiler. The moment coefficients are initially about the
authored reference center and are transported to the instantaneous total CM.
There is no wind model and no component-resolved aerodynamic database.

## 13. Auxiliary CSV Files

CSV files have no headers, use commas, and contain only finite numbers.

| File | Columns |
|---|---|
| Thrust or Isp profile | `time_since_ignition_s, value` |
| Harmonic gravity first row | `model_GM_m3ps2, model_reference_radius_m` |
| Harmonic gravity later rows | `n, m, Cbar_nm, Sbar_nm` |
| General atmosphere | `altitude_m, density_kgpm3, temperature_K, mean_particle_mass_kg, collision_cross_section_m2` |
| CHP coefficients | `max_c0, max_c1, max_c2, max_c3, min_c0, min_c1, min_c2, min_c3` (exactly 50 rows) |
| CHP molecular profile | `altitude_m, temperature_K, mean_particle_mass_kg, collision_cross_section_m2` |
| Aerodynamic database | `speed_ratio, Kn, ux, uy, uz, eta_0, ..., eta_(N-1), Cx, Cy, Cz, Cl, Cm, Cn` |

Aerodynamic rows therefore contain `11 + N` columns for a spacecraft with `N`
joint DOFs. Profile times and atmosphere altitudes must be strictly increasing.
All scalar-profile interpolation is linear with endpoint clamping.
The preferred CHP coefficient file uses the published 50-station altitude
sequence implicitly and stores coefficients in `g/km^3`. Nine-column
rows with explicit altitude and SI coefficients are also accepted.

## 14. Unreal Import and Export

Blueprint calls are exposed by `UTGScenarioFileLibrary`:

```text
ExportScenarioToTgscn
ImportScenarioFromTgscn
```

Export maps `FTGSimulationScenario` to the portable document and preserves all
frontend fields. Import reconstructs `FTGSimulationScenario`; relative external
paths are expanded against the imported file's directory so existing HUD file
pickers can use them directly.

Importing a file never silently loads untrusted native code. The stored Unreal
controller ID is restored into the scenario, but the Review Panel must verify
that it identifies a ready managed controller. A standalone DLL path must still
go through the Controller Library's explicit trust/import workflow.

## 15. Parameter Sweeps

The PHAROS scenario format deliberately has no loop, expression, variable,
include, or sweep syntax.
A Python, PowerShell, batch, C, MATLAB, or other external program should create
or modify one `.tgscn` per case and invoke `PHAROSScenarioRunner` repeatedly. This
keeps each run fully reproducible and leaves experiment design to ordinary
automation tools.

See the [standalone runner guide](../../Tools/TGScenarioRunner/README.md) for
command-line use and the [assessment cases](../../Validation/README.md) for
complete scenario inputs.
