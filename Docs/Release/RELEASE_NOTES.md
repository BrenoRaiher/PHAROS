# PHAROS 1.0 Release Notes

## Highlights

- Scenario configuration for rigid and articulated spacecraft.
- Fixed-step RK4 and adaptive Dormand-Prince propagation.
- SPICE-backed celestial states, gravity, atmosphere, aerodynamics, solar
  radiation pressure, thrusters, reaction wheels, and variable mass.
- User-authored C++ controllers through the PHAROS Controller API.
- TGSCN import and export, scenario-library persistence, and continuation from
  completed runs.
- Interactive three-dimensional playback, telemetry, vectors, constellations,
  trajectory display, and plots.

## Compatibility Summary

| Contract | PHAROS 1.0 |
|---|---|
| Saved scenario library | Format 1 |
| TGSCN import and export | [Documented TGSCN contract](../Reference/TGSCN.md) |
| Saved result records | Format 1 |
| Controller DLL API | SDK exports and public-structure sizes |

PHAROS preserves rolling scenario-library backups and quarantines unreadable
or unsupported libraries before creating a replacement. See
[Compatibility](COMPATIBILITY.md) for the full policy.

## Important Notices

PHAROS is not certified for flight, mission operations, navigation, guidance,
control, or safety-critical decisions. Independently verify all inputs,
models, assumptions, and outputs.

User C++ controllers execute as native code with the permissions of PHAROS.
Compile or load only source and DLLs that you understand and trust.

Review [Known Limitations](KNOWN_LIMITATIONS.md), [Privacy and Local Data](PRIVACY.md),
the packaged EULA, and the third-party notices before use.
