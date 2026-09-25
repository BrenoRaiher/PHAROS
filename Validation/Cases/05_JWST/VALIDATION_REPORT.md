# JWST reconstruction results

The nominal eight-phase reconstruction used one external initial state and seven complete PHAROS state handoffs. All fifteen recorded physical/interface criteria passed. The comparison below concerns historical state agreement, separately from those execution criteria.

## Same-epoch reference comparison

| Phase | End position residual (km) | End velocity residual (m/s) | Maximum sampled position residual (km) |
|---|---:|---:|---:|
| A | 1.378 | 0.050933 | 1.378 |
| B | 2.043 | 0.034627 | 2.044 |
| C | 1.018 | 0.007065 | 2.043 |
| D | 1.019 | 0.037473 | 1.021 |
| E | 21.809 | 0.096606 | 21.809 |
| F | 8.907 | 0.107749 | 53.097 |
| G | 8.911 | 0.042659 | 8.912 |
| H | 6.848 | 0.025029 | 9.334 |

The additional-sample position RMS was 15.614 km and the sampled maximum was 53.097 km. The unweighted RMS uses 3165 unique exact-epoch output samples after excluding initialization and the inherited/declared fitting epochs; the output grid is nonuniform. These samples belong to the same calibrated trajectory. The final residual was 6.848 km.

The local reconstructed-reference anomaly near 10 January is documented in [REFERENCE_QUALITY.md](REFERENCE_QUALITY.md). Hourly coast outputs do not resolve every dense-query excursion. The reference was not edited and the feature was not a fitting target.

## Maneuver comparisons

| Maneuver | Published magnitude (m/s) | Selected command (m/s) | Mass-derived delivery (m/s) |
|---|---:|---:|---:|
| MCC1A | 20.033000 | 20.105369 | 20.105369 |
| MCC1B | 2.773000 | 2.777373 | 2.777373 |
| MCC2 | 1.484000 | 1.331974 | 1.331974 |

The delivery criterion compares the mass-derived rocket-equation increment with the selected command, not with the published magnitude. Scalar thrust delivery is also distinct from the total inertial velocity change during a finite burn. Pointing histories describe simulated command following, not comparison with flight attitude telemetry.

## Calibration and numerical sensitivity

The final calibration policy, fitted marks, parameter updates, and stopping decision are in [calibration_decision.json](provenance/calibration_decision.json) and its accompanying policy/selection summaries. Superseded full tuning histories are omitted from the collaborator export. No state resets were inserted after initialization.

The recorded full-chain half-step comparison changed the final position by 6.891 m, substantially less than the reported historical residuals. See `analysis/step_refinement_metrics.json`. This is one fixed-command numerical comparison, not a bound on all model errors.

[Model and sources](MODEL_AND_SOURCES.md) · [Reproduction](REPRODUCE.md) · [Complete numerical metrics](analysis/continuous_jwst_metrics.json)
