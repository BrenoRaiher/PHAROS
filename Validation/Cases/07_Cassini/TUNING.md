# Cassini Controller Calibration

Calibration kept the maneuver schedule, finite burn durations, nine effective cruise corrections and simplified force model fixed. The baseline trajectory in the comparisons below is an intermediate calculation used while developing the reconstruction. Adjustments changed existing maneuver parameters without adding propulsion events or state resets. Historical reference states informed the calibration, so agreement with those states is reconstruction-fit evidence.

## First Venus Outgoing Trajectory

A single three-component sensitivity update changed the TCM-2 velocity-increment command by 0.0000460603 m/s. At the local target about one day after Venus 1, the difference from the baseline trajectory fell from 5061.660 m to 1.776 m. The full mission is still compared directly with NAIF. The proposed state-feedback predictor was rejected after its half-step trial retained about 127.965 m of discrepancy. Its selection record remains under `tuning/venus1`; the rejected controller and exploratory trajectories are omitted from this export. It is not part of any retained controller.

## Local Finite-Burn Delivery

Each short auxiliary experiment starts from the original maneuver initial conditions solely to isolate the delivered velocity increment with the Physics Core. One three-column sensitivity estimate and one candidate are evaluated. The update is bounded to min(0.05 m/s, 5% of the existing maneuver magnitude); it is retained only if a measurable original velocity difference is reduced by at least half. The actual mission inherits its preceding full state, never these auxiliary states.

| Segment | Original Local Velocity Difference (m/s) | Candidate Difference (m/s) | Command Update (m/s) | Retained |
|---|---:|---:|---:|---|
| 04 | 0.000761486582 | 1.86972513e-11 | 0.000763409535 | Yes |
| 05 | 1.79625459e-08 | 1.01684599e-11 | 6.06517755e-07 | No |
| 06 | 1.74005329e-09 | 2.33653419e-11 | 1.16762172e-08 | No |
| 07 | 0.000152946811 | 5.48566312e-10 | 0.000699157746 | Yes |
| 08 | 2.81328473e-09 | 8.89428235e-11 | 3.23954813e-07 | No |
| 09 | 1.24413566e-07 | 7.94663677e-11 | 7.03108764e-07 | Yes |
| 10 | 2.22747275e-06 | 2.85129185e-09 | 5.98386679e-05 | Yes |
| 11 | 1.57776032e-07 | 1.30372187e-10 | 1.08533915e-05 | Yes |
| 12 | 1.15124077e-10 | 1.09894663e-11 | 5.65384736e-09 | No |
| 13 | 7.98285628e-11 | 4.65977372e-11 | 8.9553985e-07 | No |
| 14 | 2.31385534e-11 | 5.08422995e-12 | 1.11037687e-07 | No |
| 15 | 2.99995418e-11 | 1.69922845e-11 | 3.06225267e-08 | No |
| 16 | 1.04195708e-11 | 2.82392247e-12 | 2.33377506e-11 | No |
| 17 | 3.18712691e-11 | 1.86002229e-11 | 2.98952063e-08 | No |
| 18 | 6.50205591e-09 | 2.78706709e-12 | 4.27880103e-08 | No |
| 19 | 1.14622002e-10 | 9.79621742e-13 | 2.21373892e-09 | No |

## Continuous-Trajectory Targeting

Where propagation differences materially affect later encounters, a further single bounded update may target the existing simplified trajectory. Every trial uses the complete newly propagated incoming state and the existing finite-burn controller. Three perturbed trajectories estimate the local sensitivity; one candidate is retained only if it at least halves the target position difference. Subsequent mission segments then inherit the resulting state. No parameter search continues after that single candidate.

| Segment | Target | Before (km) | After (km) | Command Update (m/s) | Retained |
|---|---|---:|---:|---:|---|
| 06 | Baseline | 1404.33 | 383532 | 0.00606206632 | No |
| 10 | Baseline | 751.843 | 30.1019 | 0.0435930488 | Yes |
| 06, With Updated Segment 05 Preparation | Baseline | 1404.33 | 0.280688 | 0.000892397271 | Yes |

The first TCM-7 candidate changed the burn direction while retaining the old preceding pointing target. Its pointing gate prevented the burn, and the candidate was rejected. The corrected experiment propagates the preceding preparation slew for every sensitivity trial; its single update is then checked by replaying both complete mission segments. The preparation interval and burn epoch remain unchanged.

Baseline differences measure the response to calibration adjustments. Historical residuals are evaluated separately against the reconstructed SPKs. The very small remaining local target residuals are not evidence for mission accuracy at that precision.

## Numerical Sensitivity During Calibration

An earlier unchanged-tolerance replay, superseded by the final diagnostic records, exposed amplification of small differences across the gravity assists, including a 43.295 km continuous half-step discrepancy at the end of Segment 03. Relative and absolute integration tolerances were then tightened from 1e-12/1e-5 to 1e-14/1e-8. The continuous fixed-command sensitivity results are reported in RESULTS.md. Tightened tolerances and fitted endpoint agreement do not justify an unqualified claim of numerical convergence.

Compact decisions are retained; incomplete exploratory runs and the rejected navigation controller are omitted from this export. Only `segments/01`–`20` define the retained mission; `checks` contains its explicitly described diagnostic replays.

## Coupled Cruise-State Correction

Segment 11 uses one bounded adjustment to its existing main maneuver and existing effective cruise correction. Their command changes are 0.0259938189 m/s and 0.00292603669 m/s. The preceding pointing preparation is updated consistently. No maneuver is added.

An offline gravity sensitivity calculation supplies a single candidate for restoring both the outgoing position and velocity. The complete PHAROS replay changes the differences from the baseline from 422.274 km / 0.0123482623 m/s to 628.925 km / 0.0309748487 m/s. Retained: No.

The predictor represents the two burns at their centers and differentiates the configured central gravity along the already computed trajectory. It is used only to estimate this parameter update. The retained simulations still propagate the full configured forces, finite burns, attitude, mass and wheel states. It is not a feedback controller or a change to the physics core.

## Preserving the Existing Pointing Offset

TCM-13 starts with a small existing offset between the preceding preparation target and the burn target. The first cruise trial replaced that offset by exact alignment and therefore changed the delivered burn beyond the intended update. That trial was rejected.

The corrected mapping measures the nominal thrust-acceleration integrals in short PHAROS continuations. It applies the same small inertial rotation to both the preceding preparation target and the main-burn target, preserving their angular separation. Throttle scaling accounts for the measured delivery and the mass at each burn. The existing trim uses its own preparation target. This applies the same physical velocity-increment correction; it adds no further target-fitting iteration.

After the complete affected segments are replayed, the outgoing differences from the baseline are 0.00386316331 km and 5.35644221e-07 m/s. Retained: Yes. The separate short main-burn check and complete candidate record are in `tuning/cruise_delivery/11`.
