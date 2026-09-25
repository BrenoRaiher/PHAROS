# PHAROS 1.0 Known Limitations

- PHAROS 1.0 is released for 64-bit Windows and its interface is English-only.
- PHAROS is a general simulation tool, not certified flight or mission
  software. Results require independent engineering validation.
- Celestial-state and body-fixed-frame availability depends on the selected
  bodies, requested epoch, and coverage of the bundled SPICE kernels. Review
  rejects a scenario when the required coverage cannot be provided.
- Imported STL files, textures, atmosphere tables, aerodynamic databases,
  harmonic-gravity files, thrust profiles, and other external resources are
  referenced by path. Moving those files or transferring a saved library to a
  different computer can require selecting the resources again.
- Custom controllers are native Windows DLLs. They are not sandboxed and must
  match the PHAROS Controller API. Controllers must be built with the SDK
  shipped with the current PHAROS application.
- For commanded propulsion, an omitted analytic derivative of actual
  propellant discharge contributes zero to the geometric-center-of-mass
  second-mass-derivative term. Thrust, mass depletion, inertia-rate effects,
  and velocity-dependent center-of-mass corrections remain active.
- Finite discharge derivatives describe smooth command branches. They do not
  model the impulsive center-of-mass velocity reset associated with an ideal
  instantaneous jump in mass-flow rate; use physically smooth ramps when that
  effect matters.
- Propellant is represented by lumped variable-mass components. General
  internal transport, moving fluid centroids, and slosh are not modeled.
- Result CSV size grows with duration, output cadence, spacecraft complexity,
  and enabled telemetry. Long, densely sampled runs can require substantial
  memory and disk space.
- Trajectory visualization interpolates recorded result samples. A coarse
  output cadence can hide short-duration behavior between samples.
- PHAROS 1.0 does not automatically transmit crash reports or product
  telemetry. Diagnostic logs remain local unless the user chooses to share
  them.
