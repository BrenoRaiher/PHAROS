# SPICE Kernel Set

This directory contains the compact kernel set loaded by `FSpiceBridge`.
Together, the files support the major planets, selected principal moons, and
the three main minor planets exposed by `Docs/Reference/ScenarioInputs.md`.

The common interval in which every moon supplied by the compact satellite
kernels is available is `2000-01-01` through `2050-01-01` (end exclusive).
Earth's Moon is supplied by `de442.bsp` and is therefore checked against that
kernel's wider coverage instead of this compact interval.

| File | Purpose | Official source |
|---|---|---|
| `naif0012.tls` | Leap seconds and UTC/ET conversion | https://naif.jpl.nasa.gov/pub/naif/generic_kernels/lsk/naif0012.tls |
| `pck00011.tpc` | Generic body orientation and radii | https://naif.jpl.nasa.gov/pub/naif/generic_kernels/pck/pck00011.tpc |
| `gm_de440.tpc` | System/body gravitational parameters | https://naif.jpl.nasa.gov/pub/naif/generic_kernels/pck/gm_de440.tpc |
| `de442.bsp` | Sun, planets, barycenters, Earth, and Moon | https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de442.bsp |
| `mar099s.bsp` | Mars, Phobos, and Deimos | https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/satellites/a_old_versions/mar099s.bsp |
| `jup230-short.bsp` | Jupiter and the four Galilean moons | https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/satellites/a_old_versions/jup230-short.bsp |
| `jup348.bsp` | Physical Jupiter coverage from 1799 through 2199; complements the compact Galilean-moon kernel | https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/satellites/jup348.bsp |
| `sat252s.bsp` | Saturn and selected principal moons | https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/satellites/a_old_versions/sat252s.bsp |
| `ura111.bsp` | Uranus and its five classical moons | https://ssd.jpl.nasa.gov/ftp/eph/satellites/bsp/ura111.bsp |
| `nep076.bsp` | Neptune and Triton | https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/satellites/a_old_versions/nep076.bsp |
| `plu058.bsp` | Pluto and Charon | https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/satellites/a_old_versions/plu058.bsp |
| `codes_300ast_20100725.tf` | Names/IDs for the asteroid SPK | https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/asteroids/codes_300ast_20100725.tf |
| `codes_300ast_20100725.bsp` | Ephemerides for 300 asteroids, of which the HUD exposes Ceres, Pallas, and Vesta | https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/asteroids/codes_300ast_20100725.bsp |

System barycenters intentionally have no physical radius or body-fixed frame.
They are valid point-mass gravity sources but cannot be used for harmonics,
atmospheres, or eclipse disks.

## Source Repository Distribution

The repository includes the kernel set through Git LFS, including the original
JPL SSD `ura111.bsp`. Run `git lfs pull` after cloning. Kernel filenames,
embedded attribution, and data bytes are retained.

`Tools/Release/ExternalKernels.json` records the SSD source URL, byte count,
and SHA-256. To verify the local file, or restore it from the provider if absent:

```powershell
& .\Tools\Release\InstallExternalKernels.ps1
```

The script leaves a matching file untouched and refuses to overwrite a
different file. It does not substitute NAIF's different same-named kernel.
The manifest is kept outside `Content` to avoid Unreal DataTable auto-import.

Provider notices remain applicable; the first-party MIT License does not
relicense these data. See [NAIF's kernel rules](https://naif.jpl.nasa.gov/naif/rules.html)
and [SSD's republication guidance](https://ssd.jpl.nasa.gov/faq.html).
No separate explicit SSD redistribution grant is recorded in this repository.
