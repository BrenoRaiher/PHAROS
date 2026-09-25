# Apollo 8 figures

Generated from saved PHAROS histories and the historical reference catalog by
[tools/plot_results.py](../tools/plot_results.py).

- `trajectory`: Earth-relative transfer and return, with a lunar-plane close view. All 19 distinct TRW positions and 12 rounded maneuver-table positions are included; coincident projections can overlap.
- `state_residuals`: position and velocity residuals for all 18 post-initialization TRW states, with the 12 rounded maneuver-position comparisons distinguished. Markers identify discrete records, not a continuous historical ephemeris.
- `mission_profiles`: mass, lunar altitude, and thrust from the unchanged propagation.
