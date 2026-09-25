# Licenses and Attribution

First-party PHAROS source and documentation are licensed under the
[MIT License](../../LICENSE). Third-party software, data, and assets retain
their respective terms.

| Document | Purpose |
|---|---|
| [EULA.txt](EULA.txt) | Terms for the packaged application, including Unreal Engine disclaimers |
| [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt) | Dependency versions, attribution, and applicable licenses |
| [ASSET_PROVENANCE.md](ASSET_PROVENANCE.md) | Visual-asset and astronomical-table origins, modifications, and source records |
| [Licenses/](Licenses/) | Full license texts and permission notices |
| [Sources/](Sources/README.md) | Corresponding third-party source and source locations |

The application build stages the EULA, third-party notices, MIT License,
license texts, and corresponding source beside the runtime. Package
preparation also copies these materials to the package root. The controller
compiler is installed separately for development and included in application
packages; its notices accompany it.

See the [release checklist](../Release/RELEASE_CHECKLIST.md) for packaging and
publication steps. Update attribution and corresponding source when changing
dependencies or assets.
