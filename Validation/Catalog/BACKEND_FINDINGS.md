# Extreme-time diagnostics

These diagnostics examine propagation over a sub-picosecond interval and
recording when distinct elapsed times map to the same absolute ephemeris time.
They supplement the baseline comparisons in the
[coverage matrix](FEATURE_COVERAGE_MATRIX.md). The inputs, expected responses,
and recorded differences are given below for numerical development.

Frozen runner SHA-256: `bb682d5eace254cbe4d8a140cf8015c379e8f1d2d351558f04a2670ad1b4fac7`.

## 1. Adaptive Sub-Picosecond Interval Advances the Clock Without the Position

Classification: implementation defect documented by the sub-picosecond diagnostic.

Input: `Cases/01-03_CoreVerification/diagnostics/adaptive_subpicosecond_tail/adaptive_subpicosecond_tail.tgscn`. Duration is 5e-13 s, initial position is zero, and constant velocity is (1, 2, 3) m/s, with no forces. Exact final position is (5e-13, 1e-12, 1.5e-12) m. The frozen runner recorded the correct final elapsed time but position (0, 0, 0) m and reported success. Maximum component error is 1.5e-12 m against this dedicated probe's 1e-15 m tolerance. Two gates detect the same skipped physical interval.

Source: `Source/TGSimCore/src/Integrators/AdaptiveDormandPrince54.cpp:148` in the captured source corresponding to the frozen runner. The optional
evidence bundle installs this source under `Support/Provenance/current_source`. The loop condition is `while (time + 1.0e-12 < target_time)`. For this nonzero interval, the loop is skipped. The subsequent `UnpackState(values, target_time, ...)` assigns the target epoch to unchanged values.

If this out-of-scope stress behavior is ever revisited, a possible correction would integrate a representable nonzero remaining interval or report an inability to advance; it should not declare the target state reached solely by relabeling its time. This campaign does not change that behavior. The measured displacement is picometer scale, and no failure of the normal-duration physical acceptance cases was observed from it.

## 2. Terminal Output Coalescence Omits a Distinct Elapsed-Time Sample

Classification: source-documented output policy incompatible with a strict elapsed-output-grid expectation. It is distinct from a dynamics propagation defect.

Source: `Source/TGSimCore/src/Simulation/SimulationEngine.cpp:1716–1728`. When the next scheduled output and final epoch map to the same binary64 absolute ephemeris time, the code replaces the next scheduled output with the final elapsed time:

```cpp
if (next_output_time < final_elapsed_time &&
    start_time + next_output_time == start_time + final_elapsed_time)
    next_output_time = final_elapsed_time;
```

The nearby comment explicitly describes avoiding duplicate absolute-time output rows. However, the two elapsed epochs are distinct. The strict audit independently constructs each k·output_spacing before the endpoint, followed by the final sample, and therefore flags the following five cases:

| Scenario | Duration (s) | Output Spacing (s) | Actual Rows | Strict Expected Rows |
|---|---:|---:|---:|---:|
| `duration_offgrid_plus_1e12s` | 1.0000000000010001 | 0.10000000000000001 | 11 | 12 |
| `duration_offgrid_plus_1e14s` | 1.00000000000001 | 0.10000000000000001 | 11 | 12 |
| `duration_offgrid_plus_1e8s` | 1.0000000099999999 | 0.10000000000000001 | 11 | 12 |
| `duration_offgrid_plus_2e14s` | 1.00000000000002 | 0.10000000000000001 | 11 | 12 |
| `short_elapsed_thruster_window` | 9.9999999999999995e-07 | 9.9999999999999995e-08 | 10 | 11 |

The four approximately one-second cases omit the 1 s grid output. The short thruster case omits the 0.9 microsecond output. Every final elapsed time and the tested final physical states/impulses are correct. Each case fails a row-count gate and an epoch-list gate, giving ten failed gates for this single policy issue.

The baseline recording criteria permit this absolute-time coalescence. Retaining every requested elapsed epoch despite an absolute-ET collision would be a different output policy, not a requirement of this campaign. The present audit retains the strict expectation and reports the difference; the raw diagnostic expectation was not changed to rewrite measured results.

## Reproduction

From the validation folder, run either diagnostic directly, choosing a new output directory if preserving the record:

```powershell
.\Support\Runtime\PHAROSScenarioRunner.exe .\Cases\01-03_CoreVerification\diagnostics\adaptive_subpicosecond_tail\adaptive_subpicosecond_tail.tgscn --kernel-dir ..\Content\SPICEKernels --output .\scratch\adaptive_tail
.\Support\Runtime\PHAROSScenarioRunner.exe .\Cases\01-03_CoreVerification\diagnostics\short_elapsed_thruster_window\short_elapsed_thruster_window.tgscn --kernel-dir ..\Content\SPICEKernels --output .\scratch\short_window
```

The full retained check table and exact CSV paths/hashes are in `Cases/01-03_CoreVerification/analysis/campaign_integrity.json`. See the [reproduction guide](../../Docs/GettingStarted/Reproduce.md) for the complete audit commands.
