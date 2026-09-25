# Verification and mission comparisons

Start with the case relevant to your change. Inputs, controller sources,
external references, and compact analysis results are included here.

| Case | Location | Assessment |
|---|---|---|
| 1 | [Core and interfaces](Cases/01-03_CoreVerification/cases/01_backend_verification/) | 111 native checks, scenarios, and controller contracts |
| 2 | [Mechanics](Cases/01-03_CoreVerification/cases/02_actuated_multibody/) and [additional constructions](Cases/01-03_CoreVerification/cases/additional_features/) | Articulation, variable mass, reactions, and events |
| 3 | [Environment](Cases/01-03_CoreVerification/cases/03_srp_rarefied_aero/) | Radiation, shadowing, atmosphere, and aerodynamics |
| 4 | [Earth orbit](Cases/04_EarthOrbit/) | Wheel and thruster Sun-pointing recovery |
| 5 | [JWST](Cases/05_JWST/) | Eight-phase transfer reconstruction |
| 6 | [Apollo 8](Cases/06_Apollo8/) | Thirteen-phase reconstruction ending before module separation |
| 7 | [Cassini](Cases/07_Cassini/) | Twenty-phase gravity-assist and Saturn-insertion reconstruction |

## Where to look

- [Catalog](Catalog/README.md): case identifiers, coverage, check definitions, and compact summaries.
- [Scripts](Scripts/README.md): shared verification, integrity checks, and plotting.
- [Reproduction guide](../Docs/GettingStarted/Reproduce.md): dependencies and commands.

Cases 1–4 provide controlled verification; Cases 5–7 compare simplified,
calibrated mission models with external references. Output-integrity checks
inspect existing histories and are not additional mission cases.

## Optional recorded evidence

The **PHAROS Reproducibility Bundle** contains full histories, frozen runners and
controller DLLs, diagnostic outputs, generated plots, and captured source. It
can be restored into these case directories for analysis or reproduction.
The [evidence index](Catalog/evidence_manifest.csv) records every optional file
and its SHA-256. Installed evidence is excluded from Git.

`Support/Provenance` records the original campaign identities and run selection.
`Support/Runtime` is populated by the bundle. The maintained application source
is in the repository's `Source` folder; the frozen assessment source and
executables have their own identities. Both use `Content/SPICEKernels`.
