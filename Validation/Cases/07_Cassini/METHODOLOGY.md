# Cassini Reconstruction Method

The reconstruction contains 20 continuous segments, from launch-vehicle separation to the end of Saturn orbit insertion. Only the initial separation state comes from external trajectory data. Every subsequent initial state comes from the preceding PHAROS solution, including the three reaction-wheel momenta and the changing propellant inertia.

## Simulation Setup

The TGSCNs define the output cadence, force models and maneuver schedule. Relative and absolute integrator tolerances are 1e-14 and 1e-8. Each segment has its own controller source, compiled against the PHAROS SDK and checked through the structural contract descriptor. Saturn orbit insertion uses a dedicated pointing controller and a changing burn direction.

Commanded thrust is piecewise constant in throttle between declared boundaries. The optional mass-flow-rate derivative is omitted. The dry spacecraft and propellant inventory have coincident origins/centroids, so the offset-mass terms in center-of-mass translation vanish here.

## Simplified Spacecraft and Environment

- Initial mass: 5573.8 kg, comprising 2523 kg dry mass and 3050.8 kg propellant inventory.
- Initial total diagonal inertia: (11148, 27049, 27049) kg m². Dry inertia: (5046, 12249, 12249) kg m². The propellant contribution scales with remaining inventory mass.
- Fixed dry body and propellant component with coincident centers; three orthogonal reaction wheels, each limited to 36 N m s and commanded to at most 0.14 N m.
- Effective axial main-engine/RCS thrust, applied at the coincident origin. Effective specific impulse is retained separately for each historical maneuver and cruise correction.
- Solar radiation pressure from a 4 m high-gain-antenna proxy, absorbing optical coefficients, reference pressure 4.5391 μPa at 1 au, and the configured eclipse bodies. Cruise attitude follows the Sun except during maneuver preparation and execution. Detailed thermal recoil, outgassing, flight attitude history and the individual attitude-control translation impulses are not reconstructed.
- Sun/planet-system gravity, separate Earth and Moon, Earth EGM2008 harmonics through degree 18, and first post-Newtonian correction. The Jupiter encounter resolves Jupiter and the four Galilean moons within the retained resolution radius; the later approach resolves Saturn and its configured moons with Saturn J2/J4/J6. The accepted activation/resolution choices avoid using compact moon kernels outside their supported interval.
- Aerodynamics is disabled. The display meshes are fixed, massless children excluded from the optical proxy.

## Maneuvers and Calibration

Historical finite-burn durations and maneuver magnitudes constrain the reconstruction, while selected directions, small magnitude adjustments and the documented pointing delays represent missing execution information. The large TCM-5 begins 1500 s after its tabulated epoch to allow the inherited attitude to align. TCM-3, TCM-4, TCM-8, TCM-15 and TCM-16 remain cancelled; the TCM-22 contingency is not flown.

Nine small finite, propellant-consuming cruise corrections remain in Segments 11–19, one per segment. They represent the accumulated translational effects of omitted disturbances and spacecraft activity. They are not additional historical TCMs. Their magnitudes, directions, durations and pointing intervals are explicit in each ControllerConfig.h and controller_parameters.json.

Calibration used bounded adjustments to the prescribed maneuver vectors. TCM-2 received one three-parameter sensitivity update, targeting the retained simplified outgoing trajectory one day after Venus 1. Its commanded velocity increment changed by 0.0000460603 m/s. This local target is an auxiliary calibration datum; the nominal mission remains continuous and is compared directly with NAIF throughout. A navigation-feedback prototype did not adequately remove the measured step sensitivity and was rejected; the retained controller continues to use fixed maneuver commands.

Short auxiliary maneuver experiments compared candidate finite-burn delivery with baseline calculations from case development, using identical maneuver initial conditions. Each experiment uses one finite-difference sensitivity estimate and at most one candidate, bounded to the smaller of 0.05 m/s and 5% of the original maneuver magnitude. An update is retained only when it at least halves a measurable local velocity difference. The complete decisions, including rejected updates, are in `tuning/local_burn_delivery`. Those experiments do not inject their initial or final states into the nominal mission. All retained adjustments and any subsequent bounded encounter targeting are listed in `TUNING.md`.

For the long cruise, an offline gravity sensitivity estimate can adjust both an existing TCM and its existing effective cruise correction together, addressing the outgoing position and velocity in one bounded candidate. Its approximate sensitivity model is used only for the parameter estimate; the complete PHAROS equations verify the candidate by replaying the affected full segments, including the preceding pointing preparation. This does not add a maneuver, introduce online reference-state feedback, or simplify the propagated force model.

The nominal delta-v vectors in the controller-parameter records configure pointing and throttle. The realized inertial thrust-acceleration integrals also depend on the incoming attitude, the finite pointing response and the mass at the burn. Short delivery checks therefore preserve any existing preparation-to-burn pointing offset when converting a desired physical increment change into new controller parameters.

The endpoint states were already used to select controller parameters. Their residuals therefore measure the quality of a calibrated simplified reconstruction. The dense intermediate trajectory comparison, encounter geometry, numerical refinement and separate analytic/backend verification provide complementary evidence. The final error alone cannot establish global mission accuracy or independent prediction skill.

## Reference Data and Source Checks

Five final reconstructed NAIF spacecraft SPKs supply target −82. Each comparison uses SSB/J2000 with aberration correction NONE. The appropriate arc is selected for each segment; its exact load order is recorded beside each generated reference CSV. Physical-moon kernels match the runtime and DE442 is loaded last to keep common planetary translations consistent with PHAROS. Reference states are regenerated at the actual recorded ephemeris times, without coarse interpolation through maneuvers.

The transition between the launch/Venus-1 and later trajectory arcs has a previously documented 0.185 s coverage gap and about 32.2 km state discrepancy after transporting across that gap. Sampling avoids an unsupported instant; no artificial state reset or controller fit removes this reference-arc discontinuity. Reference-arc inconsistencies and omitted accelerations are distinct from numerical integration error.

The source review reconfirmed the reconstructed delta-v column in the [maneuver history, Table 5](sources/documents/PDS_Cassini_Maneuver_History.pdf), PDF page 2, and finite burn durations in the [MEA/RCS record](sources/documents/PDS_Cassini_MEA_RCS_Burns.pdf), PDF page 1. TCM-18 is 0.8968 m/s, TCM-21 is 3.6968 m/s, and SOI is 626.7153 m/s over 5780.250 s. The adjacent values 2.8857 and 1.7739 belong to a normalized prediction-error column measured in sigma units; they are neither maneuver magnitudes nor percentages.

[Wood, AAS 08-311](sources/documents/AAS_08-311_Deep_Space_Navigation_1989_1999.pdf), pages 14–15, describes 22 velocity-change events on the first Earth–Venus leg and additional estimated SRP, RTG-radiation and outgassing effects. It also describes 47 non-TCM velocity-change events on the Venus-1–Earth leg. This supports retaining an explicitly simplified disturbance reconstruction, rather than interpreting a few historical main maneuvers as a complete force history. These observations do not independently validate the numerical values or placement of the nine retained corrections.

The remaining official references, mission kernel identities, and their original roles are retained in [the source index](sources/SOURCES.md), [JPL's navigation performance assessment](sources/documents/DESCANSO17_Cassini_Navigation_Performance_Assessment.pdf), the [launch press kit](sources/documents/Cassini_Launch_Press_Kit.pdf), and the [Saturn gravity reference](sources/documents/NASA_NESC_RP_09_00605_Saturn_Data.pdf).
