# Apollo 8 Execution Findings

No new backend failure was detected in this campaign. All 17,082 retained samples contain finite numeric data and report successful articulated solves. State continuity, mass and inertia consistency, time ordering, actuator limits, powered pointing, and recorded celestial-body clearance pass the audit.

The maximum full-chain step-refinement position difference is 33.583 m. Historical reconstruction discrepancies are much larger and are documented in the validation report. They are not, on their own, evidence of a backend defect.

This case uses two co-located rigid components with no articulated joints. It does not re-verify the general offset-tank or joint-impact features; those remain covered by the already accepted backend verification campaign. No physics-core, runner, SDK, UE, or HUD code was changed.
