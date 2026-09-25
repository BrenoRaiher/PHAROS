# Historical-source corrections

## Third midcourse correction ignition altitude

Apollo 8 Mission Report Table 5-II prints `165,561.5 nmi` at ignition
(GET 103:59:54) and `167,552.0 nmi` at cutoff 14 seconds later, while also
printing a space-fixed speed of about `4,299 ft/s` and a flight-path angle of
about `-80.6 deg`.

Those values cannot coexist: they imply an altitude increase of `1,990.5 nmi`
in 14 seconds while the spacecraft is descending almost vertically. The
kinematically consistent ignition altitude is `167,561.5 nmi`, which produces
the expected decrease of `9.5 nmi` to cutoff. The validation therefore treats
the leading `5` in the printed ignition altitude as a typographical error and
uses `167,561.5 nmi`.

The correction is confined to the historical truth dataset. It does not alter
any propagated state or any artifact before transearth injection.

## Terminology

All package terminology follows the Apollo 8 Mission Report, especially
Table 3-I and Table 5-II: **first midcourse correction**, **second midcourse
correction**, **lunar orbit insertion**, **lunar orbit circularization**,
**transearth injection**, and **third midcourse correction**.
Planned-opportunity numbering found in supplemental trajectory records is
intentionally not used in package names or human-facing text.


## Actual Firing Schedule

The detailed Mission Report Table 6.9-II governs actual firing windows. Table 5-II positions retain their own trajectory epochs. The first-correction ignition is corrected to GET 39599.2 s and circularization to a 9.6 s window. The third correction remains at GET 374400 s for 15 s. See `../provenance/timing_corrections.json`.

## Complete Appendix-A catalog

The final comparison uses the TRW postflight vectors on A-3 through A-23, deduplicating the repeated TRW states on A-4/A-5 and A-6/A-7. This gives 19 epochs, including initialization, with both position and velocity. 

### Transcription corrections in the calibration records

Two lunar positions had been entered incorrectly in our original calibration subset; these were transcription errors in the validation data, not inconsistencies in the NASA source. The corrected positions were read from the trajectory-supplement scans:

| Source page | Lunar revolution | GET | Difference between original and corrected position |
| --- | --- | --- | --- |
| A-10 | 1 | 69:33:00 | 22.997 m |
| A-19 | 10 | 87:39:24 | 11.196 km |

The discrepancies were mainly in the z coordinates. A-19 also had smaller x/y errors and was originally mislabeled as revolution 9. The exact original and corrected coordinates are recorded in [`historical_reference_expansion.json`](../provenance/historical_reference_expansion.json).

No recalibration or propagation rerun was performed after these corrections. [`direct_states.json`](direct_states.json) preserves the original calibration subset; [`historical_states.json`](historical_states.json) contains the corrected references used to compare against the same maneuver commands and saved trajectories.
