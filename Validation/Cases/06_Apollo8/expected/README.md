# Reference and controller inputs

`historical_states.json` is the reference catalog for the final comparisons: 19 distinct TRW postflight position-and-velocity states from the trajectory supplement, including the initial state and all ten lunar revolutions. `historical_state_sources.json` records their B1969.0 components, units, GET, labels, and source pages. Two repeated TRW states in Appendix A are counted once. `historical_body_queries` records independent CSPICE origin translations. The last available state is GET 131:24:42, before the unchanged pre-separation endpoint GET 146:28:48.

`analysis/historical_comparisons.json` (in the parent directory) contains 18 comparisons after initialization and 12 additional positions derived from rounded Mission Report Table 5-II fields. The latter remain distinct from the direct TRW vectors. Lunar heading-derived Cartesian velocities are not used as precision references; the TRW catalog supplies lunar velocity vectors directly. The corrected third-MCC altitude is documented in `SOURCE_CORRECTIONS.md`.

`direct_states.json`, `direct_state_sources.json`, and `body_queries` preserve the exact inputs previously used for calibration. Two lunar position transcriptions in that frozen subset were incorrect, and its `lunar_rev9` label actually identifies revolution 10. They are retained only to reproduce the archived calibration calculations. Final comparisons use the corrected catalog, with the changes recorded in `../provenance/historical_reference_expansion.json`; no commands or histories were changed.

`burn_vectors.json` and `controller_tuning.json` specify the adopted commands. Other `nby_direct_*` and individual truth-state files preserve conversion provenance. `initial_state_provenance.csv` records the unchanged initial seed. Orientation kernels under `kernels` support independent conversion of the rounded maneuver table.

Run `python tools/prepare_reference.py` from the case directory to regenerate the expanded conversions and comparisons against saved histories. This does not propagate a new trajectory.
