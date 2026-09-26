# Case 7 — Cassini reconstruction

Twenty continuous segments cover 15 October 1997 through 1 July 2004, including
both Venus encounters, Earth, Jupiter, Phoebe, and Saturn orbit insertion.
The sequence starts from one external state. Later segments inherit the full
preceding PHAROS state, with the final calibrated controller settings.

At insertion cutoff the same-epoch position and velocity residuals were
1.329 km and 0.843 m/s, and Saturn-centered osculating energy was negative.
The time-weighted position RMS over the sampled mission history was 491.112 km;
the largest recorded residual was 1429.068 km. Closest-approach altitude
differences use each trajectory's own minimum epoch and must be distinguished
from the same-epoch vector residuals.

- [Results and residual conventions](RESULTS.md)
- [Model and sources](METHODOLOGY.md)
- [Calibration record](TUNING.md)
- [Reproduction](REPRODUCE.md)

`segments/01`–`20` contain nominal scenarios and controller sources;
`expected` contains reference queries, and `analysis` contains comparisons.
The nine effective cruise corrections are added modeling inputs, not nine
documented historical maneuvers. Their calibrated commands are supplied.

The optional evidence bundle restores the histories, frozen DLLs, and additional
numerical diagnostics under `checks`. The continuous
half-step prefix ends after Segment 06, following Venus 2; its maximum sampled
position difference was 4411.816 km. It did not reach the Earth encounter.
The local insertion replay began from the same nominal incoming state and
had a maximum position difference of 0.0108191 m. It does not measure upstream
mission sensitivity. Both records support numerical-sensitivity comparisons
with different propagation intervals and initial-state conventions.
