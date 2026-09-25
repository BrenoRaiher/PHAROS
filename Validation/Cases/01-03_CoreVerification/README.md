# Cases 1–3 — Controlled verification

Case 1 contains the native Physics Core, scenario round-trip, controller ABI,
and visual-independence harnesses. Case 2 contains the actuated multibody
comparisons; Case 3 contains the environmental-load comparisons.
`cases/additional_features` contains the paired offset-tank and coupled-stop
constructions. `diagnostics` contains focused timing, finite-window, refinement,
and wheel-boundary probes.

The recorded native suite passed 111 checks and the controller-contract harness
passed 56. Mechanical and environmental comparisons are in
`analysis/cases_01_03_metrics.json`; the additional constructions are in
`cases/additional_features/metrics.json`. Output inspection is recorded in
`analysis/campaign_integrity.json` and is not a separate set of mission cases.

Use [the coverage map](../../Catalog/FEATURE_COVERAGE_MATRIX.md) for the verification groups and recorded outcomes,
[the reproduction guide](../../../Docs/GettingStarted/Reproduce.md) for execution, and
[the diagnostic findings](../../Catalog/BACKEND_FINDINGS.md) for the separate extreme-time
probes, including their inputs, expectations, and differences from the baseline
recording criteria.
