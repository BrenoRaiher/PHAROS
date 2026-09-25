# Fresh Native Backend Test Catalog

Captured source; 111 checks passed, zero failed. These source-level tests use the newly compiled validation executable.

| Check | Result | Error | Tolerance |
|---|---|---:|---:|
| Rigid body matches Euler equation | PASS | 0 | 1e-12 |
| Revolute joint produces base counter-rotation | PASS | 0 | 1e-12 |
| Revolute joint acceleration follows applied torque | PASS | 0 | 1e-12 |
| Prismatic joint produces base translation reaction | PASS | 0 | 1e-12 |
| Prismatic joint acceleration follows applied force | PASS | 0 | 1e-12 |
| Child-mounted thrust drives the complete free base | PASS | 0 | 1e-12 |
| Child-mounted thrust accelerates a passive hinge | PASS | 0 | 1e-12 |
| Mass depletion does not make attitude depend on inertial translation | PASS | 0 | 1e-12 |
| Mass depletion does not make articulation depend on inertial translation | PASS | 0 | 1e-12 |
| Offset rotating body has zero spatial linear acceleration | PASS | 0 | 1e-12 |
| Offset rotating origin reports ordinary centripetal acceleration | PASS | 0 | 1e-12 |
| Offset rotating body centroid remains inertially fixed | PASS | 0 | 1e-12 |
| `MomentumDerivative` mass-flow selection is Galilean invariant | PASS | 0 | 1e-12 |
| Rotating discharge balances inertia rate with carrier flux | PASS | 0 | 1e-12 |
| Rotating discharge retains a noncancelling carrier residual | PASS | 0 | 1e-12 |
| Propulsion exposes accepted discharge in thruster order | PASS | 0 | 1e-12 |
| Articulated nozzle linear carrier flux matches point kinematics | PASS | 5.55112e-17 | 2e-12 |
| Articulated nozzle angular carrier flux matches point kinematics | PASS | 0 | 2e-12 |
| Flow-biased upper stop prevents outward acceleration | PASS | 0 | 1e-12 |
| Flow-biased stop reaction includes nozzle carrier flux | PASS | 4.16334e-17 | 2e-12 |
| Prescribed thrust is independent of controller throttle | PASS | 0 | 1e-12 |
| Prescribed mass flow uses prescribed Isp | PASS | 0 | 1e-12 |
| Thruster mass loss is owned by its propellant component | PASS | 0 | 1e-12 |
| Commanded thrust equals maximum thrust times throttle | PASS | 0 | 1e-12 |
| Commanded mass flow uses controller Isp | PASS | 0 | 1e-12 |
| Thruster without a propellant component is rejected | PASS | 0 | 0 |
| Thruster linked to a fixed-mass component is rejected | PASS | 0 | 0 |
| Prescribed q-dot completes geometric-CM acceleration | PASS | 0 | 2e-12 |
| Zero-thrust ramp endpoint retains analytic q-dot | PASS | 0 | 1e-12 |
| Prescribed Isp slope contributes analytically to q-dot | PASS | 0 | 1e-12 |
| Inactive and exhausted thrusters suppress stale q-dot | PASS | 0 | 0 |
| Mixed commanded q-dot values retain thruster and owner indexing | PASS | 2.77556e-17 | 1e-12 |
| Commanded q-dot omission resets every RHS evaluation | PASS | 0 | 0 |
| Commanded supplied q-dot reaches geometric-CM RHS | PASS | 0 | 2e-12 |
| Omitted q-dot drops only the m-ddot correction | PASS | 0 | 2e-12 |
| Supplied and omitted q-dot differ by the analytic term | PASS | 5.20417e-17 | 2e-12 |
| Negative commanded q-dot reverses the m-ddot correction | PASS | 1.11022e-16 | 2e-12 |
| Supplied zero q-dot matches omission numerically | PASS | 0 | 0 |
| Rotating articulated CM correction matches point kinematics | PASS | 1.11022e-16 | 3e-12 |
| Both integrators converge to geometric-CM rocket solution | PASS | 2.28706e-14 | 2e-08 |
| Coordinate stop prevents outward joint acceleration | PASS | 0 | 1e-12 |
| Coordinate stop supplies the reaction effort | PASS | 0 | 1e-12 |
| Internal stop load does not accelerate the free base | PASS | 0 | 1e-12 |
| Adaptive Dormand-Prince accepts a constant derivative | PASS | 0 | 0 |
| Nested mixed-joint tree remains solvable | PASS | 0 | 0 |
| Multibody solution rows match their column schema | PASS | 0 | 0 |
| Nested tree conserves inertial angular momentum | PASS | 6.41361e-11 | 2e-07 |
| Nested tree conserves inertial linear momentum | PASS | 0 | 1e-12 |
| Nested tree center of mass follows uniform translation | PASS | 1.74202e-12 | 1e-10 |
| Circular Earth orbit completes | PASS | 0 | 0 |
| Circular orbit radius remains bounded | PASS | 0.000219231 | 1 |
| Circular orbit conserves specific energy | PASS | 3.23301e-11 | 1e-09 |
| Circular orbit closes after one period | PASS | 0.0170271 | 20 |
| Scalar curve linearly interpolates samples | PASS | 0 | 1e-12 |
| Scalar curve extends the selected endpoint slope | PASS | 0 | 1e-12 |
| Scalar curve clamps endpoints by default | PASS | 0 | 1e-12 |
| Scalar curve exposes analytic one-sided rates | PASS | 0 | 1e-12 |
| Scalar curve rates honor clamp and malformed safeguards | PASS | 0 | 1e-12 |
| Optional q-dot writer distinguishes omission and supplied zero | PASS | 0 | 0 |
| Uploaded atmosphere interpolates density and molecular properties | PASS | 1.38778e-17 | 1e-12 |
| Cubic Harris-Priester reaches rho_max at the diurnal bulge apex | PASS | 0 | 1e-20 |
| Analytical inertia rate matches a centered directional derivative | PASS | 5.77941e-10 | 2e-08 |
| Apparent-disk eclipse reaches full umbra | PASS | 0 | 1e-12 |
| Eclipse telemetry remains available with SRP disabled | PASS | 0 | 1e-12 |
| Component shadow blocks one of three SRP samples | PASS | 0 | 1e-12 |
| Gravity metadata comes from the ephemeris provider | PASS | 0 | 1e-12 |
| Harmonic monopole uses uploaded model GM | PASS | 0 | 1e-12 |
| Degree-two harmonic scales with uploaded reference radius | PASS | 2.22045e-16 | 1e-12 |
| Harmonic model rejects missing CSV GM and radius | PASS | 0 | 0 |
| Unchecked gravity system remains inactive | PASS | 0 | 1e-14 |
| Far field uses only the selected system barycenter | PASS | 0 | 1e-14 |
| Explicit member suppresses its selected barycenter | PASS | 0 | 1e-14 |
| Near field replaces barycenter with every system member | PASS | 0 | 1e-14 |
| Automatically activated member suppresses barycenter | PASS | 0 | 1e-14 |
| Unchecked barycenter activates inside its own radius | PASS | 0 | 1e-14 |
| Configuration normalization clears a double-checked barycenter | PASS | 0 | 0 |
| Adaptive Dormand-Prince propagates uniform translation | PASS | 8.14029e-15 | 1e-10 |
| Simulation result records configured body ephemerides | PASS | 0 | 1e-12 |
| Scattered aerodynamic database interpolates six-output rows | PASS | 0 | 1e-12 |
| Aerodynamic database rejects insufficient dimensional coverage | PASS | 0 | 0 |
| Aerodynamic moment is transported from reference point to CM | PASS | 0 | 1e-09 |
| Out-of-domain aerodynamic query uses constant drag | PASS | 0 | 0 |
| Constant-drag fallback matches drag-only CM load | PASS | 0 | 1e-09 |
| User controller output reaches forward dynamics | PASS | 0 | 1e-12 |
| Distributed gravity produces component gravity-gradient torque | PASS | 3.55271e-15 | 1e-12 |
| Dynamics exposes every component pose for playback | PASS | 0 | 0 |
| Maximum output-sample count stops an oversized result | PASS | 0 | 0 |
| Simulation cancellation is observable before propagation | PASS | 0 | 0 |
| Large absolute epoch preserves the elapsed output grid | PASS | 0 | 1e-14 |
| Sub-ULP terminal output tail is coalesced | PASS | 0 | 1e-14 |
| Controller cutoff is integrated as an exact event | PASS | 0 | 1e-11 |
| Reaction-wheel saturation conserves angular momentum | PASS | 2.77556e-17 | 1e-10 |
| Prescribed thrust profile integrates exact impulse and mass flow | PASS | 1.77636e-15 | 1e-11 |
| Barycenter-only schema accepts point-mass systems | PASS | 0 | 0 |
| Phoebe is present in the gravity catalog | PASS | 0 | 0 |
| Commanded-thruster cutoff has exact impulse and mass flow | PASS | 3.12639e-13 | 1e-10 |
| Elapsed thruster boundaries preserve sub-ET-ULP impulse | PASS | 0 | 1e-11 |
| Terminal tail advances state and elapsed clock together | PASS | 0 | 1e-06 |
| Fixed output grid survives reaction-wheel saturation | PASS | 0 | 1e-10 |
| Representable near-future event remains schedulable at 1e9 s | PASS | 0 | 1e-12 |
| Large-elapsed event above former tolerance remains exact | PASS | 0 | 1e-12 |
| Final time before future event does not fire thruster | PASS | 0 | 0 |
| Massless fixed component propagates | PASS | 0 | 0 |
| All-massless spacecraft is rejected | PASS | 0 | 0 |
| Zero reachable aggregate mass is rejected | PASS | 0 | 0 |
| Asymmetric depletion exposes geometric-CM redistribution rate | PASS | 0 | 1e-12 |
| Inelastic joint stop conserves coupled angular momentum | PASS | 0 | 2e-09 |
| Ideal joint-rate limit applies a coupled reaction | PASS | 6.38045e-13 | 2e-09 |
| Simultaneous stops release inadmissible contact | PASS | 1.38778e-17 | 2e-08 |
| Impact releases an existing resting stop | PASS | 0 | 2e-08 |
| Sequential impacts reevaluate existing contacts | PASS | 1.38778e-17 | 5e-08 |
