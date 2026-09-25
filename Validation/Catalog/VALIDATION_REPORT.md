# Assessment record index

The final verification inventory is summarized in [FEATURE_COVERAGE_MATRIX.md](FEATURE_COVERAGE_MATRIX.md).
Use the [native catalog](NATIVE_TEST_CATALOG.md) and per-case JSON/CSV analyses
for individual checks, reference values, and acceptance conditions. The
output-integrity audit reexamines existing Cases 1–4 results.

Mission comparisons are reported separately:

- [JWST](../Cases/05_JWST/README.md): final eight-phase transfer and exact-epoch SPICE comparisons.
- [Apollo 8](../Cases/06_Apollo8/VALIDATION_REPORT.md): thirteen phases ending before separation, with discrete historical marks.
- [Cassini](../Cases/07_Cassini/RESULTS.md): nominal twenty-phase reconstruction, encounter geometry, and separate numerical diagnostics.

These comparisons use simplified, calibrated spacecraft models. Numerical
execution checks, command-delivery comparisons, historical residuals, and
step-refinement differences answer different questions. Their definitions and
sampling are retained alongside each analysis rather than collapsed into one
accuracy score.
