# Added Current-Core Verification Cases

Twenty-eight runner scenarios produce 210 passing checks. Every construction is run with fixed-step RK4 and adaptive Dormand–Prince 5(4). The final inputs are in the individual case folders, `specifications.json` contains the independently chosen parameters, and `metrics.json` records errors and tolerances. From the repository root, `python Validation/Scripts/additional_features.py --analyze-only` reproduces the analysis without rerunning or regenerating inputs.

## Geometric Center of Mass Translation — 16 Runs

The spacecraft consists of a 10 kg fixed hub and a tank initially containing 5 kg, with its component centroid at x=3 m relative to the hub origin. They remain rigidly connected, do not rotate, and have collinear thrust. Gravity and environmental loads are disabled. The system starts with geometric CM position and velocity zero. These choices isolate the changing-CM correction while keeping a separately derivable material-origin trajectory.

Let q(t) be the positive total discharge rate, Q(t)=integral from 0 to t of q(s) ds, M(t)=15−Q(t), and m_tank(t)=5−Q(t). The tank/hub centroid separation is 3 m. The geometric CM offset from the hub is

    c(t) = 3 m_tank(t) / M(t)
    c_dot(t) = −30 q(t) / M(t)^2
    c_ddot(t) = −30 q_dot(t) / M(t)^2 − 60 q(t)^2 / M(t)^3.

The rigid material origin has acceleration F(t)/M(t). Its initial velocity is 30 q(0)/225 so that geometric CM velocity initially vanishes. Consequently, when q-dot is supplied or analytically available,

    v_C(t) = 30 q(0)/225 + integral_0^t [F(s)/M(s)] ds − 30 q(t)/M(t)^2
    x_C(t) = [30 q(0)/225] t
             + integral_0^t [(t−s) F(s)/M(s)] ds
             + 3 m_tank(t)/M(t) − 1.

These formulas provide the reference independently of PHAROS's assembled state derivative. The smooth integrals are evaluated using 32-point Gauss–Legendre quadrature from NumPy. Checks compare every stored mass, CM position, CM velocity, and material-origin acceleration. Tolerances are respectively 5e-11 kg, 5e-9 m, 5e-9 m/s, and 5e-10 m/s². The recorded articulated-solve status and transverse/rotational response are also checked.

| Variant | Input and purpose |
|---|---|
| positive | q=0.2+0.1t kg/s; supplied q-dot=+0.1 kg/s² |
| negative | q=0.4−0.1t kg/s; supplied q-dot=−0.1 kg/s² |
| constant | q=0.2 kg/s; explicitly supplied q-dot=0 |
| omitted | q=0.2+0.1t, optional q-dot omitted on every callback |
| zero_start | Commanded q=0.1t, supplied q-dot=0.1 even at zero initial thrust |
| prescribed | The same zero-start smooth ramp through prescribed profiles |
| variable_isp | Prescribed T=g0 t and Isp=10+2t, so q=0.5−5/(10+2t), q-dot=10/(10+2t)² and Q=0.5t−2.5 ln(1+0.2t) |
| shared | Two nozzles with distinct mount components share the tank owner; their accepted discharge and derivatives sum to the same positive-ramp reference |

For the omitted variant, the adopted contract deliberately drops only the mass second-derivative correction. Its acceleration therefore differs from the complete reference above by +30 q-dot/M². The independent omission reference adds its time integral to velocity and its time-weighted integral to position. Passing this test means the implementation follows that explicit scope decision; it does not claim omission is physically exact for varying discharge.

The prescribed profiles return to zero beyond the two-second propagation interval to satisfy current profile input requirements. The propagated interval stays on the specified smooth segment. Controller source and current-contract DLL are retained in `controllers`.

## Coupled Joint Stops — 12 Runs

The hub has axial inertia 2 kg m². Each coaxial rotor has axial inertia 0.5 kg m², and each upper coordinate limit is 0.1 rad. Initial base rate is zero. Free motion between contacts has constant rates. At an internal perfectly inelastic stop, total axial angular momentum must be conserved, the stopped relative rate must vanish, and an upper stop cannot exert a pulling impulse to retain a contact whose unconstrained response points inward.

Total angular momentum and kinetic energy can be evaluated independently as

    H = 2 omega + 0.5 sum_j(omega + eta_dot_j)
    K = omega^2 + 0.25 sum_j(omega + eta_dot_j)^2.

Solving the scalar contact momentum balances gives the terminal references below. Coordinates are radians and rates are rad/s here; authored TGSCN rotational inputs are degrees, as required by the current file contract.

| Variant | Initial (coordinate, relative rate) per rotor | Duration (s) | Final Base Rate | Final Relative Rates | Final Coordinates |
|---|---|---:|---:|---|---|
| single | (0,1) | 0.2 | 0.2 | (0) | (0.1) |
| both_lock | (0,1), (0,1) | 0.2 | 1/3 | (0,0) | (0.1,0.1) |
| release | (0,1), (0.09,0.1) | 0.2 | 0.2 | (0,−0.1) | (0.1,0.09) |
| resting | (0,1), (0.1,0) | 0.2 | 0.2 | (0,−0.2) | (0.1,0.08) |
| sequential | (0,1), (0,0.5) | 0.4 | 0.26 | (−0.06,0) | (0.092,0.1) |
| near | (0,1), (0.08999999,0.1) | 0.2 | 0.2 | (0,−0.1) | (0.1,0.08999999) |

The near case's free-motion contact times differ by 1e-7 s. After the faster rotor stops, the second rotor must reverse and not be spuriously locked. The sequential case must release the first contact at the later impact. These directly probe the corrected active-contact behavior.

Acceptance checks the final base rate, each final coordinate/rate, upper-limit nonpenetration, conserved angular momentum, nonincreasing kinetic energy, and successful recorded articulated solves. Coordinate/rate tolerance is 5e-8, penetration tolerance is 5e-10 rad, momentum tolerance is 5e-9 N m s, and energy-increase tolerance is 5e-9 J. These are sampled-state checks, complemented by the native contact-reaction tests.
