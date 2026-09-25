# Case 5 — JWST transfer reconstruction

Eight continuously connected phases cover 25 December 2021, 13:00 UTC through
1 February 2022, 00:00 UTC. The nominal reconstruction used the supplied final
controllers and simplified spacecraft model. It began from one external state;
all subsequent phases inherited the preceding complete PHAROS state.

The final same-epoch geometric position and velocity residuals were 6.848 km
and 0.025029 m/s. The sampled position RMS was 15.614 km after the exclusions
recorded by the analyzer. This is an unweighted statistic over nonuniform
output samples, not a continuous-time or time-weighted RMS. The full-chain
half-step comparison changed the final position by 6.891 m.

- [Numerical results](analysis/continuous_jwst_metrics.json)
- [Model and sources](MODEL_AND_SOURCES.md)
- [Reference-quality audit](REFERENCE_QUALITY.md)
- [Calibration decision](provenance/calibration_decision.json)
- [Reproduction](REPRODUCE.md)

`phases` contains the final scenarios; `controllers` contains their C++ sources.
The optional evidence bundle restores the histories and frozen DLLs into those
folders. `truth` supplies the reference SPK and exact-epoch
queries. The inherited calibration epochs and final targeting decisions are
preserved without duplicating discarded trajectories. The reference anomaly
near 10 January is documented separately and was not a calibration target.
