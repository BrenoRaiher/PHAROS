# Gravity Inputs for the Reconstruction

`earth_j2.csv` uses the DE440-family Earth GM, the SPICE Earth equatorial radius, and the conventional unnormalized Earth coefficient `J2 = 1.08262668e-3`, converted to the fully normalized coefficient required by PHAROS. The trajectory ephemeris is DE442.

`moon_boeing_r2.csv` reproduces the four nonzero terms listed in Table 3.2-3 of the Apollo 8 trajectory-reconstruction supplement: `J2`, `J3`, `C22`, and `C31`. The conventional unnormalized terms were converted to fully normalized coefficients. The model uses the DE440-family Moon GM and SPICE lunar reference radius. The supplement used Boeing R2 for lunar-orbit reconstruction and lower-order triaxial terms outside lunar orbit. This deliberately simplified scenario keeps the selected degree-3 field enabled throughout the mission.
