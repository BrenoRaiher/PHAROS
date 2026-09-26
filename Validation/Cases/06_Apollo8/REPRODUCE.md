# Reproduce Case 6

Follow the [shared instructions](../../../Docs/GettingStarted/Reproduce.md) and use a working copy. The
frozen runner and SDK are installed under `Validation/Support/Runtime`.
Use Python 3.11 or newer with NumPy.

From `Validation/Cases/06_Apollo8`:

```powershell
python tools/continuous_chain.py run-all
python tools/check_run.py
python tools/prepare_reference.py
python tools/sync_visual_variants.py
```

The first command compiles the supplied controller sources and propagates all
thirteen phases. Set `PHAROS_VCVARS` to your Visual Studio x64 setup script.
It reads `expected/burn_vectors.json`, `expected/controller_tuning.json`, and
the initial-state template. Later initial states come from preceding outputs.
The bundle’s DLLs can instead be used to run an individual authored TGSCN:

```powershell
../../Support/Runtime/PHAROSScenarioRunner.exe segments/06/scenario/apollo8_like_06.tgscn --kernel-dir ../../../Content/SPICEKernels --validate-only
../../Support/Runtime/PHAROSScenarioRunner.exe segments/06/scenario/apollo8_like_06.tgscn --kernel-dir ../../../Content/SPICEKernels --output segments/06/results
```

Reference conversion and the audits can be repeated using the supplied
histories without propagation. `tools/prepare_reference.py` uses the full TRW
catalog and the separately identified rounded maneuver positions. The final
comparison ends at the last available reference epoch; propagation itself
ends immediately before module separation.

`python tools/check_refinement.py` performs the optional full-chain half-step
run. Its existing results are in `diagnostics/refinement`. `tools/build_reference.py`
rebuilds the independent CSPICE executables using repository dependencies and
`PHAROS_VCVARS`; `PHAROS_SOURCE` can select another source tree.

Use the repository checker described in the shared instructions to verify the restored evidence.
