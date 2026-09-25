# Cassini Reconstruction Results

The run comprises 2450.723853 days, 284,833 recorded rows and 127,422,641 numeric cells. Its checks pass 377/377; no recorded articulated solve fails. All 19 boundaries carry position, velocity, attitude, angular velocity, propellant mass, inertia and all three wheel momenta.

## Historical State Residuals

The full-history time-weighted position RMS is 491.111972 km. The largest recorded residual is 1429.067947 km in Segment 17. Finite-burn timing offsets and fitted endpoint corrections make error growth nonmonotonic. A small final residual therefore does not describe the accuracy of every intervening epoch.

| Segment | Interval | Endpoint Position (km) | Endpoint Velocity (m/s) | Largest Recorded Position (km) |
|---|---|---:|---:|---:|
| 01 | separation to pre tcm1 | 42.138815 | 0.020270 | 42.138815 |
| 02 | tcm1 to pre tcm2 | 0.051105 | 0.026832 | 78.942957 |
| 03 | tcm2 through venus1 to pre tcm5 | 90.968994 | 0.036694 | 495.286269 |
| 04 | tcm5 to pre tcm6 | 550.958304 | 0.034655 | 598.563794 |
| 05 | tcm6 to pre tcm7 | 307.983387 | 0.124868 | 550.958304 |
| 06 | tcm7 through venus2 to pre tcm9 | 1260.745327 | 0.279052 | 1365.524344 |
| 07 | tcm9 to pre tcm10 | 1401.314266 | 0.034837 | 1401.314266 |
| 08 | tcm10 to pre tcm11 | 3.451181 | 1.151223 | 1401.314266 |
| 09 | tcm11 to pre tcm12 | 0.982169 | 0.000227 | 3.674851 |
| 10 | tcm12 through earth to pre tcm13 | 30.326491 | 0.027284 | 30.326491 |
| 11 | tcm13 to pre tcm14 | 1.424573 | 0.063261 | 621.571688 |
| 12 | tcm14 through jupiter to pre tcm17 | 1.248561 | 0.044145 | 947.756557 |
| 13 | tcm17 to pre tcm18 | 84.878343 | 0.027218 | 1271.577029 |
| 14 | tcm18 to pre tcm19 | 40.325457 | 0.097179 | 1213.536962 |
| 15 | tcm19 to pre tcm19a | 2.069092 | 0.100525 | 768.220930 |
| 16 | tcm19a to pre tcm19b | 0.135723 | 0.103866 | 91.787670 |
| 17 | tcm19b to pre tcm20 | 49.942454 | 0.097429 | 1429.067947 |
| 18 | tcm20 through phoebe to pre tcm21 | 1.056439 | 0.094928 | 79.510093 |
| 19 | tcm21 to pre soi | 0.159681 | 0.168252 | 60.271919 |
| 20 | soi to post soi | 1.329073 | 0.842755 | 3.084693 |

Each endpoint is a comparison with target −82, observer Solar System Barycenter, frame J2000 and aberration correction NONE. These kernels describe reconstructed trajectory arcs. The same arcs also informed the retained controller calibration; these residuals are reconstruction-fit evidence, not independent orbit-prediction validation. Dense intermediate samples extend the assessment beyond the fitted endpoints.

## Encounters

| Encounter | Simulated Closest Altitude (km) | Reference Closest Altitude (km) | Range Difference (km) | Time Difference (s) |
|---|---:|---:|---:|---:|
| Venus 1 | 283.523229 | 283.751390 | -0.228161 | -42.054229 |
| Venus 2 | 601.944608 | 602.598503 | -0.653896 | -100.239737 |
| Earth | 1171.481148 | 1171.491994 | -0.010846 | 0.155052 |
| Jupiter | 9722718.461392 | 9722965.016715 | -246.555323 | -13.283413 |
| Phoebe | 2074.567592 | 2070.037351 | 4.530241 | 3.504735 |

The two closest approaches are evaluated at their own minimizing epochs. Range/altitude agreement must not be mistaken for the full same-epoch position residual. The JSON encounter records also give the latter. Altitudes use the same stated spherical radius on both sides; mission reports may use a different reference surface. Cubic Hermite interpolation uses recorded positions and velocities; the reference is sampled directly from SPICE at one-second spacing around each encounter. All displayed straight-line playback chords remain above the body reference surfaces.

## Saturn Orbit Insertion

| Quantity | PHAROS at Cutoff | Reconstructed State |
|---|---:|---:|
| Saturn-Centered Radius (km) | 81299.9466 | 81301.2695 |
| Saturn-Relative Speed (km/s) | 30.4641646 | 30.4636361 |
| Specific Orbital Energy (km²/s²) | -2.52614674 | -2.53465562 |
| Osculating Semimajor Axis (km) | 7507720.28 | 7482516.75 |
| Osculating Eccentricity | 0.989310649 | 0.989274422 |
| Osculating Periapsis Radius (km) | 80252.6574 | 80254.3164 |

The steering direction varies throughout the 5780.250 s burn. The retained scalar maneuver target is 626.7153 m/s; it is not the magnitude of a single integrated inertial delta-velocity vector during a turning burn.

Mass changes from 4544.640648 to 3693.071089 kg. Negative final Saturn-centered osculating energy confirms capture. These osculating elements summarize an instantaneous two-body orbit; the simulation itself includes the other configured gravitational and environmental forces.

## Numerical Sensitivity

| Segment | Maximum Sampled Position Difference (m) | Endpoint Position Difference (m) | Endpoint Velocity Difference (m/s) |
|---|---:|---:|---:|
| 01 | 0.113589 | 0.113589 | 5.09032772e-08 |
| 02 | 1.696560 | 1.696560 | 5.19563136e-07 |
| 03 | 22122.101294 | 22122.101294 | 0.00157427821 |
| 04 | 22223.704639 | 21257.093618 | 0.00175953239 |
| 05 | 21257.093618 | 12159.867443 | 0.00279253647 |
| 06 | 4411816.398780 | 4411816.398780 | 4.49467261 |

The completed refinement prefix extends through Segment 06. It starts at separation and passes its own full terminal state between segments. Maximum and initial integration steps are halved; output epochs and error tolerances are unchanged. Because requested output times are integration endpoints, a maximum-step reduction need not halve every accepted adaptive step. Gravity assists can amplify a small incoming state difference. The commands remain frozen and are not refitted for the sensitivity replay, so it measures the stability of this particular calibrated sequence as well as local integration sensitivity. These results do not establish absolute convergence or independent physical accuracy.

The independent local Saturn-insertion replay uses the identical complete nominal incoming state. Its maximum position difference is 0.0108190733 m; cutoff differences are 0.00649873958 m and 1.32302584e-06 m/s. Both local trajectories have successful recorded solves. This local result does not erase the upstream continuous-chain sensitivity.

## Controller and Scope Observations

The controller DLLs use the SDK's structural contract. Existing maneuver vectors received the bounded adjustments documented in TUNING.md. Historical maneuver times/durations, the finite TCM-5 ignition delay, the SOI steering approximation, Sun-pointing cruise attitude, simplified mass distribution, and the nine effective cruise corrections remain documented modeling choices.

Wheel capacity was reached at 38 stored output rows across the mission. Saturation is permitted; capacity and torque-limit checks remain satisfied. The early post-TCM-2 slew is a known limitation of the retained attitude controller.

The coincident dry-spacecraft and propellant centroids make the offset-mass translation corrections vanish in this particular mission model. Those features are exercised by the separately accepted backend verification cases; this reconstruction does not provide an independent offset-tank test.
