# Cassini Controller Calibration

The supplied `segments/01`–`20` contain the calibrated mission inputs. Calibration
kept the maneuver schedule, finite burn durations, nine effective cruise
corrections, and simplified force model fixed. It adjusted existing maneuver
parameters without adding propulsion events or resetting the propagated state.
Historical states informed the fit; their residuals therefore assess a calibrated
reconstruction.

## Targets and Parameter Selection

Calibration used terminal state marks and stored trajectory targets. Records
marked `calibration_reference` identify a numerical target used during fitting,
not a historical ephemeris. Historical residuals are calculated separately against
NAIF states at the same epochs. Target states enter only the offline parameter
selection, never the controller's runtime feedback or the propagated state.

Short finite-burn comparisons used identical initial conditions to isolate
command delivery. A finite-difference sensitivity estimate supplied at most one
candidate per comparison. Its velocity-increment change was limited to the
smaller of 0.05 m/s and 5% of the maneuver magnitude, and was accepted only when
it at least halved a measurable local velocity difference. The decisions are
recorded under `tuning/local_burn_delivery`.

Encounter targeting used the complete incoming PHAROS state. Three perturbed
trajectories estimated the sensitivity of terminal position to the maneuver
vector; a candidate was accepted only when it at least halved the target
position difference. The preceding pointing preparation was propagated with
the maneuver. These decisions are recorded under `tuning/trajectory_targeting`
and `tuning/venus2`.

For the long cruise, a bounded adjustment could change an existing maneuver and
its existing effective cruise correction together. An offline gravity
sensitivity estimate proposed the parameter change; a full PHAROS replay
evaluated it. The mapping preserved the angular separation between preparation
and burn pointing targets, and accounted for mass and finite thrust delivery.
Selection records are under `tuning/cruise_delivery` and `tuning/linear_cruise`.

## Using the Calibrated Case

The selected values are explicit in each segment's `ControllerConfig.h` and
`controller_parameters.json`. Reproduction uses these files directly and does
not require recalibration. `checks` contains separate numerical diagnostics;
their results are described in [RESULTS.md](RESULTS.md). Calibration-target
agreement and fixed-command numerical sensitivity answer different questions.
