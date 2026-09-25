# PHAROS 1.0 Clean-Machine Test Matrix

Complete this matrix against the exact packaged candidate. Record machine
details, package hash, tester, date, and evidence for every run.

## Environment Coverage

| Case | Required environment | Status |
|---|---|---|
| OS-01 | Supported Windows 10 x64, standard user, fresh PHAROS AppData | Not run |
| OS-02 | Supported Windows 11 x64, standard user, fresh PHAROS AppData | Not run |
| DEP-01 | No Unreal Engine installation | Not run |
| DEP-02 | No Visual Studio or standalone compiler installation | Not run |
| DPI-01 | 100% display scaling | Not run |
| DPI-02 | 125% or 150% display scaling | Not run |
| DPI-03 | 200% display scaling | Not run |
| GPU-01 | Lowest intended supported graphics configuration | Not run |
| GPU-02 | Recommended graphics configuration | Not run |

## Functional Coverage

| Case | Required result | Status |
|---|---|---|
| APP-01 | Installer or portable launch succeeds and shows PHAROS metadata and icon | Not run |
| LEGAL-01 | First launch requires EULA acceptance | Not run |
| LEGAL-02 | Declining exits; accepting persists; legal links open packaged files | Not run |
| UI-01 | Main Menu, Scenario Library, configuration, and visualization HUD render correctly | Not run |
| SAVE-01 | Save, rename, reopen, delete, and restart preserve expected scenarios | Not run |
| SAVE-02 | Corrupt-library test preserves the original under `SaveGames\Recovery` | Not run |
| TGSCN-01 | TGSCN imports, exports, and reimports with the documented schema and units | Not run |
| RUN-01 | Representative RK4 and Dormand-Prince scenarios complete | Not run |
| RUN-02 | Saved result visualizes and continues from its final state | Not run |
| CTRL-01 | Bundled toolchain builds and runs a trusted template controller | Not run |
| CTRL-02 | Incompatible or untrusted controller is rejected clearly | Not run |
| DATA-01 | SPICE kernels and representative uploaded CSV/STL resources load | Not run |
| PRIV-01 | No CrashReportClient or PHAROS telemetry transmission is present | Not run |
| UNINSTALL-01 | Uninstallation removes program files without deleting user data unexpectedly | Not run |

## Performance Record

For each hardware tier, record startup time, idle memory, peak memory, package
size, representative propagation time, visualization frame rate, and disk use
for a long run. Use those measurements to publish minimum and recommended
requirements rather than estimates.
