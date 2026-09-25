# PHAROS Simulation Engine

General sequential C++ simulation engine for PHAROS. The native module retains
the internal name `TGSimCore` for source and asset compatibility.

## Public Entry Point

```cpp
tgsim::SimulationRequest request;
// Fill initial state, vehicle, environment, model, and time settings.

const tgsim::SimulationResult result = tgsim::SimulationEngine().Run(request);
```

`result.samples` is the typed history. `result.column_names` and `result.solution_array` form the numeric result table that the runner serializes for analysis and Unreal playback.

## Rules

- No Unreal types or scene operations in this module.
- SI units internally.
- ICRF/J2000 translational propagation.
- SPICE ET (TDB seconds past J2000) as the propagated time coordinate.
- Body-frame rotational propagation.
- Physics is evaluated synchronously and sequentially.
- Unreal conversion, visual scale, splines, actors, and widgets belong in `Source/TG`.

`SimulationRequest` remains the solver's direct input boundary. A portable
`ScenarioDocument` and the current-schema `.tgscn` parser/compiler sit immediately
above it. `SimulationConfigBuilder` only normalizes the resulting request
(time, quaternion, component mass states, articulation states, and wheel
states); it does not read Unreal objects or HUD widgets.

Detailed architecture and limitations are in `Docs/Development/PhysicsCore.md` and `Docs/Reference/MultibodyDynamics.md` at the project root.
The Featherstone implementation and basic-joint input forms are in `Docs/Reference/MultibodyDynamics.md`.
The complete field-by-field contract for the Unreal HUD, including the
user-authored C++ controller interface, is in `Docs/Reference/ScenarioInputs.md`.
The portable scenario language and backend-only executable are documented in
`Docs/Reference/TGSCN.md` and `Tools/TGScenarioRunner/README.md`.

`tests/TGSimCoreValidationMain.cpp` runs the backend-only validation suite. It does not load or launch Unreal.

The solver currently exposes fixed classical RK4 and Boost.Odeint adaptive
Dormand-Prince 5(4). Scalar profiles use piecewise-linear interpolation, and
Eigen supplies the rank-revealing 6x6 articulated-base solve.
nanoflann supplies exact nearest-neighbor searches for the scattered aerodynamic
coefficient database.

The backend exposes a control callback for a completely user-authored C++
control system. The PHAROS simulation engine supplies current state/status data and a safely sized
command writer, but implements no production guidance or control law. Gravity
is evaluated at each component CM. The caller supplies an `IEphemerisProvider`;
the standalone runner uses `StandaloneSpiceProvider` for body ephemerides,
orientation, GM, and reference radius. The graphical application launches that
same runner. Output storage is
bounded, and observers may cancel a run.

## Aerodynamic Database Rows

Each `AerodynamicCoefficientSample` contains the independent coordinates

```text
[molecular_speed_ratio, Kn, gas_flow_direction_body_x/y/z, eta_0, ..., eta_n]
```

and the six outputs `[C_X, C_Y, C_Z, C_l, C_m, C_n]`. Flow direction must be a
unit vector in spacecraft body axes. Kn is stored as supplied and transformed to
`log10(Kn)` only inside interpolation. The database also declares the body-frame
reference point for its moment coefficients; every lookup transports that moment
to the instantaneous spacecraft CM before dynamics uses it. Invalid or
under-spanned databases are rejected during setup. Out-of-domain lookups either
select the nearest row or use whole-spacecraft constant-C_D drag at the CM.

Atmospheric density is supplied by a general altitude profile or the Earth-only
cubic Harris-Priester model. Uploaded thermodynamic rows provide temperature,
mean particle mass, and effective collision cross-section; the backend derives
mean free path, Kn, and molecular speed ratio. There is no wind model,
component-resolved aerodynamic coefficient database, or aerodynamic surface
mesh. The optional constant-C_D fallback supplies translation only.
