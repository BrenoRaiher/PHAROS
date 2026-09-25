# Case 6 — Apollo 8 reconstruction

The thirteen-phase CSM propagation runs from GET 04:45:54 to GET 146:28:48,
immediately before Command–Service Module separation. It uses one external
initial state; later phases inherit the preceding PHAROS state. No separation
or post-separation extension is part of this case.

The reference catalog contains 19 distinct TRW position-and-velocity epochs
(one initialization and 18 comparisons), plus 12 positions reconstructed from
rounded maneuver records. The last historical state is GET 131:24:42. Lunar
position residuals ranged from 72.4 to 91.6 km; the last return mark differed
by 48.9 km. The maximum full-chain half-step difference was 33.583 m. The
pre-separation endpoint is a PHAROS result without a historical mark at that epoch.

- [Results](VALIDATION_REPORT.md)
- [Reference catalog](expected/README.md)
- [Source and transcription corrections](expected/SOURCE_CORRECTIONS.md)
- [Reproduction](REPRODUCE.md)

`segments/01`–`13` contain scenarios and controller sources. The optional evidence
bundle restores frozen DLLs and recorded histories into those folders.
`expected/direct_states.json` preserves the calibration subset, including the
two original transcription errors; `expected/historical_states.json` contains
the corrected comparison catalog. The controllers were not recalibrated after
those corrections. Compact calibration records remain in `analysis` and
`provenance`; discarded trial histories are omitted.
