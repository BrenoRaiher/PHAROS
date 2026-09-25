# Official source index

## Trajectory and attitude truth

- `truth_kernels/cassini/000331R_SK_LP0_V1P32.bsp` - JPL/NAIF Cassini body -82 SPK.
- `000331R_SK_LP0_V1P32.bsp.lbl` - PDS label identifying the product as the Cassini Navigation Team's final reconstructed “Launch to Venus 1” trajectory.
- `truth_kernels/cassini/97288_98002rc.bc`, `98002_98091rc.bc`, and `98091_98185rc.bc` - reconstructed Cassini attitude kernels covering the validation interval.
- `truth_kernels/cassini/cas00172.tsc` and `cas_v43.tf` - spacecraft-clock and frame definitions used with the CKs.
- NAIF archive: https://naif.jpl.nasa.gov/pub/naif/CASSINI/kernels/

## Mission, maneuver, and navigation documentation

- `AAS_08-311_Deep_Space_Navigation_1989_1999.pdf` - Lincoln J. Wood, “The Evolution of Deep Space Navigation: 1989-1999,” AAS 08-311. Pages 14-15 document 22 launch-to-Venus-1 discrete velocity events, continuously estimated SRP/RTG/outgassing accelerations, short-duration nongravitational accelerations, and the difficulty separating SRP from RTG radiation. Official URL: https://descanso.jpl.nasa.gov/evolution/AAS_08-311.pdf
- `DESCANSO17_Cassini_Navigation_Performance_Assessment.pdf` - JPL DESCANSO Design and Performance Summary Series, Article 17. Sections 1.2 and 2.1-2.2 document navigation force models, launch performance, TCMs, the 284 km Venus-1 flyby, and notable small-force events. Official URL: https://descanso.jpl.nasa.gov/DPSummary/DESCANSO17_Cassini_RevA.pdf
- `PDS_Cassini_Maneuver_History.pdf` - Cassini Final Mission Report supplementary Table A-5. It gives TCM-1 reconstructed delta-v 2.7770 m/s, TCM-2 reconstructed delta-v 0.1788 m/s, and TCM-3/TCM-4 cancellation. Official data are distributed by NASA PDS.
- `PDS_Cassini_MEA_RCS_Burns.pdf` - Cassini maneuver-engine/RCS supporting record distributed by NASA PDS.
- `The_Cassini_Era_1996_1997_SP-4227.pdf` - NASA SP-4227. It records the 2.7 m/s, 34.6 s TCM-1 and the early HGA-to-Sun cruise configuration. Official URL: https://www.nasa.gov/wp-content/uploads/2023/04/sp-4227.pdf
- `Cassini_Launch_Press_Kit.pdf` - JPL launch press kit. It supplies launch configuration, approximate mass, 4 m antenna diameter, engine/thruster architecture, and dimensions. Official URL: https://www.jpl.nasa.gov/news/press_kits/cassini.pdf
- ESA launch-vehicle record - confirms successful Centaur separation at mission elapsed time 42 minutes 40 seconds: https://www.esa.int/Science_Exploration/Space_Science/Cassini-Huygens/The_launcher
- NASA TCM-2 significant-event report - confirms the 25 February 1998 RCS maneuver and approximately 0.18 m/s delta-v: https://science.nasa.gov/missions/cassini/significant-event-report-for-week-ending-2271998/

## Gravity model

- `NGA.STND.0036_1.0.0_WGS84.pdf` - official NGA WGS 84 standard and EGM2008 context. Official portal: https://earth-info.nga.mil/?action=wgs84&dir=wgs84
- `EGM2008_120.gfc` - degree-120 coefficient distribution used only for the documented sensitivity branch. The retained degree-18 CSV is extracted from official EGM2008 coefficients and uses the DE442 Earth GM so its central term matches the runner ephemeris.

## Kernel-order convention

For spacecraft truth extraction, the Cassini SPK is loaded before DE442. DE442 is loaded last so the planetary states and reference-frame translation match TGScenarioRunner. The official SPK label's customized planetary ephemeris is retained in the archive but is intentionally superseded for this comparison.
