# Apollo 8 reconstruction results

The nominal reconstruction propagated thirteen continuous CSM phases from GET 04:45:54 to GET 146:28:48, immediately before Command–Service Module separation. It used one external initial state and six finite maneuvers. All later phases inherited the preceding PHAROS state. The final state has no historical comparison at that epoch.

## Historical comparisons

The trajectory supplement provides 19 distinct TRW postflight position-and-velocity states: initialization and 18 later comparisons, including ten lunar revolutions. Repeated TRW rows at the same epoch are counted once. The last available state is GET 131:24:42. These are discrete records, not a continuous spacecraft ephemeris.

NBY B1969.0 components were rotated into J2000 and translated using the DE442 origin state. Saved PHAROS positions and velocities were interpolated with cubic Hermite interpolation to matching epochs. Coast outputs were spaced by 60 s; burn outputs by 0.1 s.

| Checkpoint | GET (h:mm:ss) | Position residual (km) | Velocity residual (m/s) |
|---|---:|---:|---:|
| After MC1 | 11:55:36 | 5.686 | 0.1086 |
| Outbound Transfer | 30:14:54 | 9.335 | 0.2298 |
| Before MC2 | 39:56:48 | 15.423 | 0.2313 |
| After MC2 | 61:06:36 | 27.669 | 0.9964 |
| Lunar Revolution 1 | 69:33:00 | 91.604 | 42.0160 |
| Lunar Revolution 2 | 71:40:24 | 81.209 | 36.4454 |
| Lunar Revolution 3 | 73:49:24 | 73.966 | 23.2073 |
| Lunar Revolution 4 | 75:48:06 | 73.887 | 22.7729 |
| Lunar Revolution 5 | 77:46:24 | 72.416 | 24.5318 |
| Lunar Revolution 6 | 79:47:30 | 74.191 | 18.7640 |
| Lunar Revolution 7 | 81:44:18 | 72.630 | 27.6771 |
| Lunar Revolution 8 | 83:42:18 | 73.572 | 33.9153 |
| Lunar Revolution 9 | 85:40:54 | 74.406 | 39.2540 |
| Lunar Revolution 10 | 87:39:24 | 77.647 | 45.6085 |
| After TEI | 89:47:36 | 46.238 | 7.6628 |
| After MC3 | 104:06:24 | 1.201 | 0.5174 |
| Return (129 h) | 129:25:24 | 45.634 | 0.4837 |
| Return (131 h) | 131:24:42 | 48.915 | 0.4567 |

Lunar position residuals ranged from 72.416 to 91.604 km, with velocity residuals from 18.764 to 45.608 m/s. The post-MC3 position residual was 1.201 km, but the final available return mark differed by 48.915 km. The small calibrated return residual therefore did not characterize the entire return coast.

Twelve additional positions were reconstructed from rounded Mission Report Table 5-II maneuver records. They are plotted separately. Their lunar heading-derived velocity vectors are not treated as precision references; direct TRW vectors supply the lunar velocity comparisons. The corrected third-MCC ignition altitude is documented in `expected/SOURCE_CORRECTIONS.md`.

## Calibration and reference provenance

The maneuver commands were selected against navigation positions, lunar apsidal altitudes, and the post-MC3 position. The source epochs for lunar revolutions 1 and 10 informed the lunar-burn calibration, while the rounded LOI-ignition position informed MC2 and the post-MC3 position informed TEI. Calibration summaries remain under `analysis` and `provenance`; superseded trial histories are omitted from the collaborator export. No new fitting or propagation was performed when expanding the reference catalog.

The frozen `expected/direct_states.json` preserves the actual calibration inputs. Its two lunar positions contain transcription errors, and the key `lunar_rev9` identifies revolution 10. The corrected complete catalog is `expected/historical_states.json`. Coordinate corrections, source pages, transcription precision, and unchanged simulation/input hashes are documented in `provenance/historical_reference_expansion.json`. The final comparisons use the corrected source vectors.

## Finite maneuver delivery

| Phase | Ignition GET (s) | Duration (s) | Commanded increment (m/s) | Integrated thrust acceleration (m/s) | Maximum pointing error (deg) |
|---|---:|---:|---:|---:|---:|
| first_midcourse | 39599.2 | 2.4 | 7.5590 | 7.5594 | 0.1028 |
| second_midcourse | 219595.9 | 11.8 | 0.7364 | 0.7364 | 0.0158 |
| lunar_orbit_insertion | 248900.4 | 246.9 | 905.8000 | 905.8778 | 0.1951 |
| lunar_orbit_circularization | 264906.6 | 9.6 | 41.1300 | 41.1307 | 0.3531 |
| transearth_injection | 321556.6 | 203.7 | 1068.8233 | 1069.2514 | 0.0016 |
| third_midcourse | 374400.0 | 15.0 | 1.4783 | 1.4783 | 0.0000 |

The integral accumulated thrust magnitude divided by instantaneous mass with a left-rule quadrature at the 0.1 s output interval. TEI includes its preceding RCS ullage. This scalar delivery comparison is distinct from the inertial velocity change and from historical-state agreement.

## Lunar geometry and final propagated state

| State | Periapsis altitude (nmi) | Apoapsis altitude (nmi) | Period (min) |
|---|---:|---:|---:|
| After insertion | 51.591 | 178.572 | 128.836 |
| After circularization | 51.485 | 70.809 | 119.067 |
| Before TEI acquisition | 50.509 | 71.902 | 119.077 |

These are osculating two-body altitudes above the adopted 1737.4 km lunar radius. The reported post-insertion and post-circularization values were 60.0–168.5 nmi and 59.7–60.7 nmi. The reconstructed circularized orbit remained less nearly circular.

At the pre-separation endpoint, Earth-relative altitude was 4419.9 km, speed 8.543 km/s, and flight-path angle -39.54 degrees. The final mass of 14409.7 kg followed the prescribed consumable schedule.

## Numerical and execution evidence

All 171/171 recorded execution conditions and 26 scenario preflights passed. Halving maximum integration steps over the complete chain changed position by at most 33.583 m. These findings concern execution and numerical sensitivity, separately from historical residuals.

## Sources and reproduction

- [Reference catalog](expected/historical_states.json)
- [Complete comparison records](analysis/historical_comparisons.json)
- [Execution audit](analysis/final_audit.json)
- [Reference corrections](expected/SOURCE_CORRECTIONS.md)
- [Reproduction instructions](REPRODUCE.md)
- [Trajectory supplement](sources/Apollo_8_Trajectory_Reconstruction_Supplement_1.pdf)
- [Mission Report](sources/Apollo_8_Mission_Report_19700033031.pdf)
