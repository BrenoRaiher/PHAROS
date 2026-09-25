# Model, Assumptions, and Sources

## Physical Reconstruction

The initial mass is 6161.4 kg: a 5860.4 kg fixed bus and one 301 kg variable propellant component. Both local centroids and origins coincide. The bus inertia is 30000 kg m² on each principal axis; the initial tank inertia is 500 kg m² on each axis and scales linearly with remaining mass. These inertia values and spatial distribution are engineering assumptions, not recovered flight mass properties. The launch mass comes from [Gardner et al., The James Webb Space Telescope Mission](https://ntrs.nasa.gov/api/citations/20250006005/downloads/36_Gardner_2023_PASP_135_068001.pdf); the combined initial propellant follows NASA's [fueling report](https://science.nasa.gov/blogs/webb/2021/12/06/nasas-james-webb-space-telescope-fully-fueled-for-launch/).

One equivalent centerline SCAT thruster supplies at most 32 N with constant Isp of 295 s and a modeled 5 s first-order rise. Three orthogonal ideal reaction wheels use quaternion PD control with gains 0.12 and 120, a 0.2 N m per-axis command limit, and an assumed 2000 N m s capacity. The simplified actuator architecture, constant Isp, lag, gains, and limits are reconstruction choices. The actual spacecraft uses multiple SCAT and MRE thrusters; SCAT location and role differ before and after deployment, and MRE thrusters support attitude and momentum management. See [STScI propulsion documentation](https://jwst-docs.stsci.edu/jwst-observatory-hardware/jwst-spacecraft-bus/jwst-propulsion) and [attitude-control documentation](https://jwst-docs.stsci.edu/jwst-observatory-hardware/jwst-attitude-control-subsystem).

The model follows the Sun with body +Z during coast, then spends the last hour before each burn acquiring a fixed ICRF burn direction with body +X. No reconstructed attitude kernel is used. The model does not reproduce full attitude constraints, actual slew scheduling, six-wheel geometry, momentum dumps, or minor operational thrusting. The full propellant load is assigned at the first post-launch seed despite possible earlier expenditure. The centered, rigid layout also makes several centroid-redistribution transport terms vanish; the separate backend feature tests exercise those terms in nonzero configurations.

Gravity includes the Sun, Earth, Moon and the listed planetary barycenters, Earth degree-2 J2, and the backend's enabled first post-Newtonian correction. Earth-Moon and planetary bodies are selected to avoid simultaneously counting a body and its barycenter. SRP uses the facet model with Earth/Moon eclipse handling; atmosphere and aerodynamics are disabled. The coefficient file comes from the reduced Earth-gravity case and is not a full EGM2008 field; see [NGA WGS 84 resources](https://earth-info.nga.mil/index.php?action=wgs84&dir=wgs84).

## SRP Proxy and Deployment Time

NASA gives overall sunshield dimensions of 21.197 × 14.162 m. The retained model uses a rounded **21.2 × 14.2 m rectangle**, represented by two triangles, for an area of **301.04 m²**. It is a bounding-footprint approximation, not the actual projected area of the kite-shaped multilayer shield. Optical fractions remain 0.35 absorption, 0.25 specular reflection, and 0.40 diffuse reflection; these are fixed effective assumptions. The stowed proxy remains 20 m². See [NASA sunshield dimensions and description](https://science.nasa.gov/mission/webb/webbs-sunshield/).

NASA reported completion of fifth-layer tensioning on 4 January 2022 at 11:59 EST (16:59 UTC). The simplified model switches its SRP area at **17:00 UTC**, rounded to the hour. Actual deployment was progressive. Keeping the small area until completion and then changing it instantaneously is a deliberate abstraction; it does not model the intervening mechanical configuration or time-varying optical area. See [NASA completion report](https://science.nasa.gov/blogs/webb/2022/01/04/webb-team-tensions-fifth-layer-sunshield-fully-deployed/).

## Maneuver Inputs and Reference Ephemeris

The planned burn intervals and reference magnitudes were taken from [Burney et al., Mid-Course Correction Maneuver Planning and Execution for JWST](https://ntrs.nasa.gov/api/citations/20220010207/downloads/MCC%20Paper.pdf). The nominal intervals are 00:50:00–01:54:54.728 UTC on 26 December, 00:20:00–00:29:27.240 UTC on 28 December, and 19:00:00–19:04:56.648 UTC on 24 January. Reference magnitudes are 20.033, 2.773, and 1.484 m/s. These source-derived numbers are distinct from the modestly calibrated command magnitudes in the report.

The original reconstructed SPK is retained from the [NAIF JWST archive](https://naif.jpl.nasa.gov/pub/naif/JWST/kernels/spk/). Its embedded comments identify concatenated definitive trajectories supplied by Goddard FDF and a merge created on 18 August 2026. The complete comment area and CSPICE segment inventory are in `truth/reference_audit/`. Its SHA-256, the DE442 hash, and the exact geometric queries are recorded. This is an orbit-reconstruction reference with a documented local interpolation artifact, not an uncertainty-free record of the spacecraft's true state. See [Reference Quality](REFERENCE_QUALITY.md) and [NAIF kernel-selection guidance](https://naif.jpl.nasa.gov/naif/kernel_selection.html).

## Presentation Variants

The optional `*_with_visual_appearance.tgscn` files preserve every numerical parameter and inherited state of their corresponding canonical file. They hide the two physical primitives and add eighteen fixed, massless, zero-inertia display children, excluded from the SRP proxy. All sixteen inputs pass the runner's preflight; numerical results come from the eight canonical inputs. Display meshes show the fully deployed assembly even in A-E because no stowed/deploying model was supplied. Thus the early-phase appearance is illustrative, while physical SRP chronology follows the canonical scenario.

## Calibration Record

The bounded joint calibration changed only MCC-1b and MCC-2 vector components; all physical assumptions above remain fixed. Its selected values and stopping evidence are in [Retuning Report](provenance/calibration_decision.json).
