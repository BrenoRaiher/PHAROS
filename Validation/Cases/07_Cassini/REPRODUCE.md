# Reproduce Case 7

Follow the [shared instructions](../../../Docs/GettingStarted/Reproduce.md), then work on a copy of the
repository after installing the optional evidence bundle. The complete multiyear propagation is
expensive; supplied histories can be analyzed without rerunning it.

From `Validation/Cases/07_Cassini`:

```powershell
python tools/continuous_chain.py --compile
python tools/reference.py --dense
python tools/audit.py
python tools/sync_visuals.py
python tools/encounters.py
```

Set `PHAROS_VCVARS` to the Visual Studio x64 setup script for controller builds.
Omit `--compile` to use the supplied DLLs. `tools/reference.py --build` rebuilds
the CSPICE extractor; its dependencies default to this repository, with
`PHAROS_SOURCE` available as an override. The nominal segment sequence and
controller configuration are already calibrated; no tuning is required.

Nominal segments inherit preceding states. The templates under `provenance`
are generator inputs, not standalone scenarios. Use `segments/NN/scenario`
for individual runs. Visual variants use fixed massless display geometry.

The following are optional, separate numerical diagnostics. Existing results
are supplied under `checks`; none is required to inspect the nominal results:

```powershell
python tools/dense_encounters.py
python tools/encounters.py
python tools/refine_chain.py --to 6
python tools/check_refinement.py --to 6
python tools/local_refinement.py --segment 20
```

The continuous refinement is a completed prefix through Venus 2, not a full
twenty-segment refinement. The insertion replay starts from the same nominal
incoming state and answers a local question. Calibration summaries remain in
`TUNING.md` and compact `tuning/**/decision.json` records.

Use the repository checker described in the shared instructions to verify the restored evidence.
