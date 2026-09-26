# Reproduce Case 5

First follow the [shared runtime instructions](../../../Docs/GettingStarted/Reproduce.md). Work on a copy.
From `Validation/Cases/05_JWST`, the nominal propagation and analysis are:

```powershell
$env:TG_JWST_START_PHASE = 'A'
$env:TG_JWST_END_PHASE = 'H'
python analysis/run_continuous_chain.py
python analysis/sync_visual_variants.py
python analysis/analyze_continuous_chain.py
python analysis/inspect_reference.py
```

Only phase A reads the external initial state. Later phases start from the
preceding output. The optional evidence bundle supplies the final controller DLLs; no calibration run
is required. Partial phase reruns are supported by the two environment
variables, but an upstream change requires recomputing downstream phases.

The analyzer compares matching ephemeris times and excludes the external seed,
inherited fitting epochs, and declared phase-end epochs from its additional-
sample RMS. `provenance/inherited_fit_epochs.json` preserves the original
exclusion values. The exact sampling rule remains in the analyzer.

Optional numerical refinement repeats all eight phases with half the maximum
step: `python analysis/check_step_refinement.py`. It is a long, separate run;
the recorded comparison already exists. Its output remains under
`provenance/trials/05_retuned_step_refinement` for compatibility with the records.

To rebuild the supplied controllers and independent CSPICE tools, run
`python analysis/build_tools.py --vcvars <path-to-vcvars64.bat>` and
`python analysis/build_inventory.py`. Set `PHAROS_VCVARS` for the latter.
CSPICE is found in the repository source by default; `PHAROS_SOURCE` overrides
that location. `PHAROS_RUNNER` and `PHAROS_KERNELS` select deliberate runtime
alternatives. Record their identities when comparing a new run.

Calibration policy, sensitivity matrix, and selection summaries are retained
in `provenance/joint_retuning`. Superseded full tuning trajectories and local
report-publication helpers are not needed to reproduce the final case.
