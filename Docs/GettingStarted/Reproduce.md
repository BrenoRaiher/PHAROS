# Reproduce the assessment cases

The main repository contains inputs, controller sources, reference data, and
compact results. Ordinary development does not require the full recorded
histories. To analyze those histories or use the exact frozen executables,
obtain the matching **PHAROS Reproducibility Bundle** distributed alongside this
source snapshot. No public download URL is assumed by the tools.

## Install the optional evidence

Use Windows x64 and Python 3.11 or newer. Work on a copy: rerunning a case writes
results and analyses into that copy. From the repository root, with the bundle
beside the checkout:

```powershell
python Tools/Repository/install_evidence.py --bundle '../PHAROS Reproducibility Bundle' --check-only
python Tools/Repository/install_evidence.py --bundle '../PHAROS Reproducibility Bundle'
python Tools/Repository/check_repository.py --verify-evidence
python -m pip install -r Validation/Scripts/requirements.txt
```

The installer verifies the bundle against the repository's evidence index,
checks path containment, and refuses to overwrite a different existing file.
Matching files are left in place. The frozen runner, SDK, and native harnesses
do not require Unreal Engine to execute. Original histories and binaries retain
their original bytes. Installing evidence does not add it to version control.

## Kernel setup

Fetch the source repository's Git LFS objects. Kernels are external data and
are not embedded in either runner. Application and assessment scripts share
`Content/SPICEKernels`. The repository includes `ura111.bsp` through Git LFS.
The following optional command verifies its pinned checksum or restores it
from the provider if the file is missing:

```powershell
./Tools/Release/InstallExternalKernels.ps1 -KernelDirectory ./Content/SPICEKernels
```

The frozen runner requires the complete default kernel set, including Uranus,
even for cases that do not select that body. `PHAROS_KERNELS` can select an existing
complete kernel directory. Mission-specific reference kernels remain inside
the corresponding case. Record the identity of any substituted data or runtime.

## Cases 1–4

From the repository root:

```powershell
python Validation/Scripts/preflight.py
python Validation/Scripts/run_campaign.py --group all
python Validation/Cases/04_EarthOrbit/analyze_campaign.py
python Validation/Cases/01-03_CoreVerification/analysis/analyze_cases_01_03.py
python Validation/Scripts/additional_features.py --analyze-only
python Validation/Scripts/audit_campaign.py --records runs_all.json
python Validation/Scripts/make_plots.py
python Validation/Cases/01-03_CoreVerification/cases/01_backend_verification/visual_independence/verify.py
```

`--group earth` selects the two Earth physical scenarios; `--group features`
selects the backend constructions and timing diagnostics. These commands do
not run the mission reconstructions. Preflight checks 78 inputs: the 77
verification scenarios and one additional sub-picosecond stress probe.
The [coverage matrix](../../Validation/Catalog/FEATURE_COVERAGE_MATRIX.md)
defines the verification groups and their recorded outcomes. The
[backend findings](../../Validation/Catalog/BACKEND_FINDINGS.md) describe the
additional propagation and strict output-grid diagnostics.

Without `--records`, the audit reads the merged original run records, including
later output refinements. Use `--records runs_all.json` after a fresh complete
campaign. Analysis commands may read installed histories without propagation.

## Rebuilds and mission runs

Use the [build guide](Build.md) for the maintained source. Rebuilding frozen
native harnesses instead uses `Validation/Support/Provenance/current_source`
from the bundle. Set `UE_ENGINE_ROOT` to Unreal Engine 5.7 and `PHAROS_VCVARS`
to the Visual Studio x64 compiler setup script, then run:

```powershell
python Validation/Scripts/build_verification.py
python Validation/Scripts/build_controllers.py --vcvars '<path-to-vcvars64.bat>'
```

The two old-layout controller DLLs in Case 1 are intentional rejection fixtures.
Rebuilt tools have new identities and should not be described as the original
frozen binaries. The mission guides describe their own builds and analyses:

- [JWST](../../Validation/Cases/05_JWST/REPRODUCE.md)
- [Apollo 8](../../Validation/Cases/06_Apollo8/REPRODUCE.md)
- [Cassini](../../Validation/Cases/07_Cassini/REPRODUCE.md)

These are long simulations. Inspecting compact summaries or analyzing existing
histories does not require repeating propagation or calibration.
