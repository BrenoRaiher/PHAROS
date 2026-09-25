# Case 4 — Controlled Earth orbit

Two physical scenarios compare Sun-pointing recovery using four reaction
wheels or twelve attitude thrusters, under the same five disturbances.
The visual variants preserve physical inputs and are not separate experiments.

| Configuration | Recorded rows | Final pointing error (deg) | Propellant consumed (kg) | Recoveries |
|---|---:|---:|---:|---:|
| Four wheels | 11,854 | 0.00083510 | 0.009775438 | 5 / 5 |
| Attitude thrusters | 11,854 | 0.26969220 | 0.051437176 | 5 / 5 |

Wheel-case consumption came from the common disturbance thruster. The wheels
provided recovery torque without consuming propellant. The other configuration
also consumed propellant through its attitude thrusters.

Controller sources, disturbance schedules, scenarios, geometry, and compact
results are included. The optional evidence bundle supplies the frozen DLLs
and raw histories. From the repository root, run
`python Validation/Scripts/run_campaign.py --group earth` for propagation and
`python Validation/Cases/04_EarthOrbit/analyze_campaign.py` for analysis.
See the [reproduction guide](../../../Docs/GettingStarted/Reproduce.md) for
runtime dependencies. The analyzer regenerates the metrics and time series.
