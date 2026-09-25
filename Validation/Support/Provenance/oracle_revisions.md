# Reference-formula revisions

During development, the Case 2 commanded-thrust angular-momentum reference was corrected to include I_dot*omega. Its tolerance was preserved while its equation was replaced by the analytical solution of the adopted open-system balance. This correction followed the reference model rather than fitting the simulation output.

For the original co-located bus and tank, I(t)=I0-b*t, b=0.2*q, q=40/(250*g0), nozzle radius ell=1 m, and tau=-40 N m. The nozzle carrier angular-momentum flux is q*ell^2*omega. Thus I*omega_dot+(q*ell^2-b)*omega=tau. With a=q*ell^2-b and omega(0)=0, omega(t)=tau/a*(1-(I(t)/I0)^(a/b)). Remaining-body angular momentum is I(t)*omega(t). The analyzer evaluates this formula using log1p/expm1 to avoid cancellation. The same reference is used for the three integration-step convergence variants.

The analytical and Earth-recovery tolerances were otherwise unchanged. Any subsequent correction must be documented here with its independent rationale.


## Timing and Profile Audit References

The ordinary force-free position probes use the timekeeping audit's 1e-9 m absolute tolerance, enlarged to eight binary64 ULPs at the predicted position scale for very long runs. A blanket roundoff-only threshold was not adopted for these integration-accuracy checks. The dedicated sub-picosecond displacement probe retains its separate 1e-15 m tolerance and still fails; the ordinary tolerance does not mask it.

The strict fixed-interval output grid is constructed independently as k*spacing strictly before the endpoint plus the endpoint itself. No coalescence allowance is added to make the five affected probes pass. Those differences remain recorded findings.

The short elapsed CSV thrust reference integrates its clipped piecewise-linear profile analytically in time since ignition. Its expected impulse is 0.0999995 N s. Mass-derived impulse must agree within 1e-9 N s and propellant loss within 1e-12 kg.
