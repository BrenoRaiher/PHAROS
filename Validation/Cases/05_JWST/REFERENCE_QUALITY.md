# Local Quality of the Retained JWST SPK

The residual curve changes sharply near 10 January 2022. Direct inspection shows that this behavior is present in the retained reference ephemeris. It is not a PHAROS phase handoff: the nominal trajectory remains within the same coast F.

The relevant segment is `JWST_DEFINITIVE_EPHEMERIS_2021359130000_`, target -170 relative to Earth (399), frame 1, SPK type 13. CSPICE provides the segment addresses; the documented type-13 layout identifies its interpolation nodes and an actual polynomial degree of **3**. Neighboring nodes 60 s apart differ in position by about **16.479 km** more than the displacement implied by their endpoint velocities. Their exact states are in `truth/reference_audit/type13_nodes.csv`.

Type 13 fits position and velocity data with Hermite polynomials and obtains interpolated velocity from the polynomial derivative. A sharp inconsistency between nearby tabulated positions and their velocities can therefore produce a large interpolated velocity excursion. This interpretation follows the [NAIF SPK specification](https://naif.jpl.nasa.gov/pub/naif/toolkit_docs/C/req/spk.html#Type%2013:%20Hermite%20Interpolation%20---%20Unequal%20Time%20Steps) and the inspected node values. The underlying cause of the inconsistent source nodes was not established.

A separate one-second query over the affected interval records a peak velocity departure of **420.9 m/s** from the pretransition reference velocity, and a largest one-second velocity change of **27.0 m/s**. These are diagnostics of local reference behavior, not measured spacecraft maneuvers. Across the surrounding hourly interval, the reference position differs from endpoint-velocity trapezoidal displacement by 16.416 km; the corresponding PHAROS discrepancy is only 5.155 m. A trapezoidal diagnostic alone is not an error bound; the dense query and raw nodes establish what happens between the hourly samples.

The main mission statistics use the original stored output cadence, including one-hour samples during coast F. They retain the position change but do **not** resolve the full brief reference velocity excursion. The report's sampled maximum and RMS must therefore not be quoted as continuous-time maxima or dense-cadence agreement. The final-state comparison is at 1 February and is separate from this local feature.

No reference node was edited, smoothed, shifted, or removed from the nominal comparison. The diagnostic samples were not used for controller fitting. The case retains the original SPK for provenance and documents why exact matching of this local feature would not be a sensible reconstruction objective. A complete survey of every reference interpolation interval is outside this rerun.

Reproduce with `analysis/SpkInventory.exe` and `analysis/inspect_reference.py`; retained results are in `analysis/reference_continuity_audit.json` and `truth/reference_audit/`.
