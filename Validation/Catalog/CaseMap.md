# Assessment case map

Case identifiers organize the assessment inputs and results in this repository.
Cases 1–4 cover controlled verification; Cases 5–7 compare simplified mission
reconstructions with external trajectory references.

| Case or subject | Repository location |
|---|---|
| 1 — Physics Core and scenario integrity | [Backend checks](../Cases/01-03_CoreVerification/cases/01_backend_verification/) |
| 2 — Actuated multibody mechanics | [Mechanical checks](../Cases/01-03_CoreVerification/cases/02_actuated_multibody/) and [additional constructions](../Cases/01-03_CoreVerification/cases/additional_features/) |
| 3 — Environmental loads | [Environmental checks](../Cases/01-03_CoreVerification/cases/03_srp_rarefied_aero/) |
| 4 — Controlled Earth orbit | [Earth orbit](../Cases/04_EarthOrbit/) |
| 5 — JWST transfer | [JWST](../Cases/05_JWST/) |
| 6 — Apollo 8 reconstruction | [Apollo 8](../Cases/06_Apollo8/) |
| 7 — Cassini reconstruction | [Cassini](../Cases/07_Cassini/) |
| Verification coverage and native checks | [Coverage](FEATURE_COVERAGE_MATRIX.md), [native catalog](NATIVE_TEST_CATALOG.md) |
| Runner, TGSCN, and controllers | [Runner guide](../../Docs/GettingStarted/StandaloneRunner.md), [file format](../../Docs/Reference/TGSCN.md), [SDK](../../ControllerSDK/README.md) |
| Source/evidence versions and checksums | [Reproducibility](../../Docs/GettingStarted/Reproduce.md) |

The 111 native checks belong to Case 1. The output-integrity audit inspects
existing Cases 1–4 result files. The mission directories identify the models,
calibration inputs, external references, and residual definitions used in
Cases 5–7.

Use a release or commit identifier together with the case, segment, and input
path when citing or discussing a result. Recorded histories correspond to the
frozen validation runtime; rebuilding the application produces a new executable.
