# Multibody Dynamics

## Scope

The propagated state uses these conventions:

```text
position_icrf_m, velocity_icrf_mps = instantaneous total-CM state
attitude_body_to_icrf, angular_velocity_body_radps = main-body frame B
eta, eta_dot = ordered joint coordinates and rates
```

Joint coordinates and rates belong to the propagated state. The controller supplies known actuator effort: torque for a revolute DOF and force for a prismatic DOF. Passive joints receive zero effort. Featherstone's floating-base Articulated-Body Algorithm then solves the six free-base accelerations and every `eta_ddot`.

## Basic Joint Inputs

Each child `ComponentDefinition` references an earlier parent. Its `articulation_to_parent` contains anchors, the zero-pose rotation, and an ordered DOF list.

| User joint | Representation |
|---|---|
| Fixed | Leave `dofs` empty. |
| Revolute | Add one `ArticulationDof` with `motion=Rotation`, a local axis, and angular limits. |
| Prismatic | Add one `ArticulationDof` with `motion=Translation`, a local axis, and linear limits. |
| Gimbal/compound | Add multiple rotation/translation DOFs in transform order. |

Every axis is expressed in the joint frame after the preceding DOFs. Rotation coordinates use radians; translation coordinates use meters. Limits apply to coordinate, absolute rate, and actuator effort.

## Internal Tree

`FloatingBaseTreeDynamics.cpp` expands each compound connection into one internal node per DOF. Massless virtual nodes carry joint transforms; the child component's spatial inertia is attached after the final child-anchor transform. This preserves the public component model while giving Featherstone one motion subspace `S_i` per internal joint node.

For a revolute joint:

```text
S_i = [axis_i; 0]
```

For a prismatic joint:

```text
S_i = [0; axis_i]
```

The six entries are ordered `[angular; linear]`.

## Articulated-Body Algorithm

The first outward pass computes spatial velocities and velocity-product acceleration:

```text
vJ_i    = S_i qdot_i
v_i     = X_i v_parent + vJ_i
c_i     = v_i x vJ_i
IA_i    = I_i
pA_i    = v_i x* (I_i v_i) + Idot_i v_i - fExternal_i
```

The inward pass eliminates the unknown acceleration of each free one-DOF joint:

```text
U_i  = IA_i S_i
d_i  = S_i^T U_i
u_i  = jointEffort_i - S_i^T pA_i
Ia_i = IA_i - U_i U_i^T/d_i
pa_i = pA_i + Ia_i c_i + U_i u_i/d_i

IA_parent += X_i^T Ia_i X_i
pA_parent += X_i^T pa_i
```

After all joint accelerations have been eliminated, the free-base acceleration is one 6x6 solve:

```text
a_base = -(IA_base)^-1 pA_base
```

The final outward pass recovers every joint acceleration:

```text
a_i     = X_i a_parent + c_i
qddot_i = (u_i - U_i^T a_i)/d_i
a_i     = a_i + S_i qddot_i
```

At an active coordinate/rate stop, that DOF is rerun with constrained acceleration and the corresponding stop reaction is reported separately from the applied effort. The integration scheduler locates approaching limits, and `ArticulationConstraintDynamics` solves the coupled velocity jump. Its unilateral impulse conditions allow a contact to release instead of imposing a pulling impulse.

## Loads and Variable Mass

`General6DofDynamics.cpp` evaluates gravity, propulsion, SRP, aerodynamics, mass flow, and the controller. Thruster and facet wrenches retain their actual component of application for the inward pass. Body-level and gravity loads are converted to an equivalent wrench about the main-body origin.

The translational state is the geometric total-spacecraft CM. With effective
thrust and outward nozzle discharge `q_p`, its smooth-interval equation is:

```text
rCM_dot = vCM
u_P^B = omega_B x (r_P-r_CM)^B + r_dot_P^B - r_dot_CM^B
vCM_dot = gravity_acceleration + non_gravity_force / M
           + R_IB [sum(m_ddot_i (r_Gi-r_CM)^B)
                   + sum(m_dot_i u_Gi^B) - sum(q_p u_Np^B)] / M
```

Prescribed profile derivatives are analytic on each linear segment. A
commanded thruster may provide its analytic actual-discharge derivative in its
`PHAROSThrusterCommand` output; omission sets only that thruster's `m_ddot` contribution
to zero.

The floating-base solve computes base and joint accelerations together, including the relative motion of articulated components and their momentum exchange.

## Output

The recorded state and load history includes:

- main-body-origin linear acceleration;
- total inertial linear momentum;
- total inertial angular momentum about the instantaneous CM;
- solved joint acceleration in the propagated derivative;
- applied actuator effort and hard-stop reaction for every articulation coordinate;
- a flag indicating whether the 6x6 free-base solve succeeded.

## Validation

The [native validation suite](../../Source/TGSimCore/src/Validation/BackendValidationSuite.cpp) includes checks that:

1. one component reduces exactly to Euler's rigid-body equation;
2. analytic revolute acceleration and base counter-rotation from a known torque;
3. analytic prismatic acceleration and base reaction from a known force;
4. a passive hinged child responds correctly to its own external wrench;
5. coordinate stops constrain acceleration and return the correct reaction effort;
6. a nested revolute/prismatic tree driven by internal spring efforts conserves inertial momentum;
7. a circular 400 km Earth orbit remains bounded, conserves specific energy, and closes after one period.

The suite is sequential and uses no Unreal runtime or parallel computation.
