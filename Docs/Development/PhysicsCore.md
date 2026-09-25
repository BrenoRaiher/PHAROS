# TGSimCore Backend

`TGSimCore` is the standalone, sequential C++ physics backend. Unreal is not involved in propagation.

```text
SimulationRequest
  -> SimulationConfigBuilder (normalization only; no HUD dependency)
  -> SimulationEngine
  -> ScenarioFactory
  -> General6DofDynamics
       -> ComponentKinematicsModel
       -> MassPropertiesModel
       -> GravityModel
       -> SolarRadiationPressureModel
       -> AerodynamicsModel
       -> IController
       -> PropulsionModel
       -> FloatingBaseTreeDynamics
            -> SpatialAlgebra
  -> FixedStepRK4 or AdaptiveDormandPrince54
  -> StateRecorder
  -> SimulationResult.samples + SimulationResult.solution_array
```

## State

The propagated state contains the translational, attitude, and internal coordinates:

```text
[rI(3), vI(3), q_B_to_I(4), omegaB(3), mass(1),
 component_masses(M), eta(N), eta_dot(N), wheel_momenta(W)]
```

`eta` contains one generalized coordinate per articulation DOF: radians for rotations and meters for translations. Each coordinate has a matching rate state. `M` counts variable-mass components, `N` articulation DOFs, and `W` reaction wheels; each component mass and wheel momentum contributes one scalar. Quaternions use scalar-first order `[w, x, y, z]` and are normalized after accepted integration steps.

## Units and Frames

- SI units throughout.
- Translational state and total force use ICRF/J2000 axes.
- Base angular velocity, assembled inertia, centroid offset, and total torque use body axes.
- Each wheel stores a scalar signed momentum along its mount-component axis.
- Component and joint quantities use the local frames named by their fields.
- `attitude_body_to_icrf` rotates a body-frame vector into ICRF.
- Planet-fixed gravity coefficients are evaluated in each body's rotating frame and transformed back to ICRF.

## Implemented Models

- Sequential fixed-step fourth-order Runge-Kutta or adaptive
  Dormand-Prince 5(4) propagation.
- Multi-body point-mass Newtonian gravity evaluated at every component CM to
  produce gravity-gradient and articulation loads.
- Normalized spherical-harmonic gravity coefficients through configurable degree/order.
- Optional first post-Newtonian test-particle correction.
- Nested component trees with ordered rotational and translational articulation DOFs, including coordinate, rate, and acceleration limits.
- Featherstone floating-base Articulated-Body forward dynamics for fixed, revolute, and prismatic joints, including nested/compound joints.
- Known joint efforts, solved joint/base accelerations, component-resolved external wrenches, hard-stop reactions, and total momentum telemetry.
- Component-composed mass, center of mass, inertia, and inertia rate using current articulated poses and the parallel-axis theorem.
- Prescribed-profile and commanded thrusters, articulated mount geometry,
  specific impulse, propellant depletion, analytic prescribed discharge rates,
  optional commanded discharge-rate derivatives, and thrust torque.
- Geometric-center-of-mass translation with the component mass-second-
  derivative, component mass-rate velocity, and nozzle carrier-velocity
  corrections assembled from current component kinematics.
- Converter-generated triangular SRP proxies with three-point partial-shadow
  sampling, articulated component-local BVHs, and apparent-disk eclipses.
- General uploaded atmosphere profiles or Earth cubic Harris-Priester density,
  SPICE-derived co-rotation, molecular speed-ratio/Knudsen evaluation, an
  aggregate scattered coefficient database, and constant-C_D drag fallback.
- External controller interface and reaction-wheel momentum states.
- User-authored C++ controller callback with backend-built state/status input,
  safely sized command output, and an optional analytic actual-discharge-rate
  derivative per commanded thruster. No production control law is supplied.
- Every-step or fixed-interval output.
- Bounded output storage, cancellation polling, scheduled-event step boundaries,
  and playback-ready component transforms.

## Current Model Boundaries

- `StandaloneSpiceProvider` supplies barycentric center states, body-fixed
  orientation and angular velocity, GM, and reference radius to the runner.
  Both terminal and graphical-app simulations use this runner. Unreal uses
  `FTGSpiceEphemerisProvider` for its native request adapter and SPICE queries;
  the backend also accepts injected providers, including synthetic test data.
- An aerodynamic coefficient database always supplies one aggregate B-frame
  spacecraft wrench about a fixed B-frame reference center. It cannot infer
  load shares among components. The constant-drag fallback acts at the total CM
  and therefore supplies neither attitude torque nor articulated joint loads.
- Joint coordinates and rates are propagated states. Controller-supplied joint
  forces/torques and external loads determine base and joint accelerations
  through forward dynamics. Joint limits include acceleration constraints
  and coupled impact impulses. General component collisions, closed loops,
  flexible modes, slosh, detailed actuator dynamics, and joint friction are
  not implemented.
- Cubic Harris-Priester is Earth-only and uses one centered-average F10.7 value;
  the general profile is valid only over the user's uploaded altitude interval.
- The constant-drag fallback may run in any regime, but states with `Kn <= 10`
  are flagged because this moderate-fidelity backend targets low-density flight.
- The scheduler ends steps at prescribed profile/ignition events and declared
  controller discontinuities. Wheel-momentum and joint-limit crossings are
  predicted and, when necessary, located by bracketed reintegration before
  applying the limit response. There is no arbitrary user-defined event API.
- Missing commanded-thruster discharge derivatives use zero only for their
  mass-second-derivative CM contribution. Finite derivatives apply on smooth
  branches and do not represent the impulse at an ideal mass-flow-rate jump.
- Propellant remains a lumped component-mass model; general internal transport,
  moving fluid centroids, and slosh are outside the model.
- Propagation is sequential inside the runner. The graphical application
  launches it as a separate process and receives progress, logs, and result
  paths through process pipes.

See [MultibodyDynamics.md](../Reference/MultibodyDynamics.md) for joint definitions, Featherstone equations, telemetry, and verification cases.
