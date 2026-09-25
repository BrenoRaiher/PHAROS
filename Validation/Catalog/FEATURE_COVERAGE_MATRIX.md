# Verification coverage

This matrix summarizes the recorded verification results for Cases 1–4.
The [case map](CaseMap.md) identifies each assessment. Individual native checks
are listed in [NATIVE_TEST_CATALOG.md](NATIVE_TEST_CATALOG.md); per-case analyses
record the observed errors, tolerances, and outcomes. Locations in the table
are relative to `Validation/`.

| Evidence group | Reported outcome | Location |
|---|---|---|
| Native Physics Core checks | 111 / 111 | `Cases/01-03_CoreVerification/cases/01_backend_verification` |
| Scenario acceptance | 77 / 77 | Backend and Earth inputs; `Support/Provenance/preflight.json` also records the separate stress probe |
| Scenario round trip | Passed | Case 1 native scenario harness |
| Controller contract | 56 / 56 | Case 1 `controller_contract` |
| Visual independence | 3 / 3 visual-only comparisons | Case 1 `visual_independence`; changed-thrust control included separately |
| Actuated multibody mechanics | 25 / 25 | Case 2 and its independent analyses |
| Offset-tank mass redistribution | 96 / 96 | Eight constructions, each with two integrators |
| Coupled joint stops | 114 / 114 | Six constructions, each with two integrators |
| Output integrity and timing | 809 / 809 baseline conditions | Audit of the existing Cases 1–4 files |
| Environmental loads | 32 / 32 | Case 3 and articulated aerodynamic query |
| Integrated attitude recovery | 10 / 10 recoveries | `Cases/04_EarthOrbit` |

The scenario-acceptance count excludes the additional sub-picosecond probe;
the [preflight record](../Support/Provenance/preflight.json) contains all 78 inputs.
The [output audit](../Cases/01-03_CoreVerification/analysis/campaign_integrity.json)
contains 829 conditions. The 809 baseline conditions exclude the ten checks
of `adaptive_subpicosecond_tail` and the ten strict-grid conditions for the
five epoch-coalescence cases listed in [BACKEND_FINDINGS.md](BACKEND_FINDINGS.md).
Those additional diagnostics retain their original outcomes in the audit.
Reinspection of a result file does not constitute a new simulation case.

The two Earth configurations used the same disturbance sequence. Wheel-case
propellant consumption came from the disturbance thruster.

Cases 5–7 supply separate mission comparisons: final calibrated JWST, Apollo 8,
and Cassini reconstructions are included. Their mission-specific residuals,
sampling conventions, and numerical sensitivity are documented alongside the
recorded histories. They are not pass/fail demonstrations of flight accuracy.
