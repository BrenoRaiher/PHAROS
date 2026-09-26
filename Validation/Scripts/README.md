# Shared assessment scripts

Run these from the repository root as described in the
[reproduction guide](../../Docs/GettingStarted/Reproduce.md).

| Script | Purpose |
|---|---|
| `preflight.py` | Check authored scenarios without propagation |
| `run_campaign.py` | Run Cases 1–4 with the frozen runner |
| `additional_features.py` | Build/run or analyze offset-tank mass redistribution and coupled joint-stop constructions |
| `audit_campaign.py` | Inspect existing output integrity, timing, and physical tails |
| `build_verification.py` | Rebuild native harnesses from the captured assessment source |
| `build_controllers.py` | Rebuild Case 1–4 controller fixtures |

Mission-specific scripts stay inside each case. Their local paths are relative
to that case; the shared runtime is in `Validation/Support/Runtime` and kernels
are in `Content/SPICEKernels`. Optional evidence must be installed before reading
the supplied histories or using frozen executables.
