"""Repeat the frozen A-H model with half the maximum integration step.

No calibration, new reference states, or force-model changes are made. The
separate trial is regenerated from the same single external initial state.
"""
from pathlib import Path
from dataclasses import replace
import csv
import json
import math
import os
import shutil
import run_continuous_chain as chain

ROOT = Path(__file__).resolve().parents[1]
TRIAL = ROOT / ('provenance/trials/05_retuned_step_refinement' if (ROOT/'provenance/joint_retuning/nominal_assembly.json').exists() else 'provenance/trials/03_step_refinement')

def rows(path):
    with path.open(newline='', encoding='utf-8') as stream:
        return list(csv.DictReader(stream))

def main():
    TRIAL.mkdir(parents=True, exist_ok=True)
    for directory in ('controllers', 'data'):
        shutil.copytree(ROOT/directory, TRIAL/directory, dirs_exist_ok=True)
    (TRIAL/'analysis').mkdir(exist_ok=True)
    # Keep the frozen runner, kernels and sole seed at their original locations.
    chain.KEY_STATES = ROOT/'truth/jwst_key_states_barycentric.csv'
    chain.ROOT = TRIAL
    chain.PHASES = [replace(p, maximum_step_s=p.maximum_step_s/2) for p in chain.PHASES]
    os.environ['TG_JWST_START_PHASE'] = 'A'
    os.environ['TG_JWST_END_PHASE'] = 'H'
    chain.main()
    nominal = rows(ROOT/'analysis/phase_manifest.csv')
    refined = rows(TRIAL/'analysis/phase_manifest.csv')
    comparisons = []
    all_finite = True
    all_solves = True
    max_epoch_difference = 0.0
    for first, second in zip(nominal, refined):
        assert first['phase'] == second['phase']
        original = rows(ROOT/first['solution'])
        alternate = rows(TRIAL/second['solution'])
        assert len(original) == len(alternate)
        for a, b in zip(original, alternate):
            dt = abs(float(a['ephemeris_time_tdb_seconds_past_j2000']) - float(b['ephemeris_time_tdb_seconds_past_j2000']))
            max_epoch_difference = max(dt, max_epoch_difference)
            assert dt <= 5e-7, 'Stored epoch mismatch exceeds comparison allowance'
            all_finite &= all(math.isfinite(float(v)) for v in b.values())
            all_solves &= float(b['multibody_solve_succeeded']) == 1.0
            d = {'phase':first['phase'], 'et':float(a['ephemeris_time_tdb_seconds_past_j2000']), 'epoch_difference_s':dt}
            for prefix, unit in [('position', 'm'), ('velocity', 'mps')]:
                d[prefix+'_difference_'+unit] = math.sqrt(sum((float(b[f'{prefix}_icrf_{axis}_{unit}'])-float(a[f'{prefix}_icrf_{axis}_{unit}']))**2 for axis in 'xyz'))
            comparisons.append(d)
    with (ROOT/'analysis/step_refinement_residuals.csv').open('w', newline='', encoding='utf-8') as stream:
        writer=csv.DictWriter(stream, fieldnames=list(comparisons[0])); writer.writeheader(); writer.writerows(comparisons)
    result = {
        'scope':'All eight phases; same sole external initial state and frozen controls/physics. Maximum step halved; output epochs and tolerances unchanged.',
        'nominal_tolerances':{'absolute':1e-5, 'relative':1e-12},
        'all_refined_values_finite':all_finite,
        'all_recorded_refined_articulated_solves_successful':all_solves,
        'sample_pairs':len(comparisons),
        'maximum_epoch_difference_s':max_epoch_difference,
        'maximum_position_difference_m':max(r['position_difference_m'] for r in comparisons),
        'maximum_velocity_difference_mps':max(r['velocity_difference_mps'] for r in comparisons),
        'final_position_difference_m':comparisons[-1]['position_difference_m'],
        'final_velocity_difference_mps':comparisons[-1]['velocity_difference_mps'],
        'interpretation':'A step-bound sensitivity check for this reconstruction, not a rigorous error bound or a general convergence proof.'
    }
    (ROOT/'analysis/step_refinement_metrics.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    (TRIAL/'README.md').write_text('# Integration-Step Refinement\n\n'+result['scope']+'\n\nRun from the mission root with `python analysis/check_step_refinement.py`. The comparison results are in `analysis/step_refinement_metrics.json` at the mission root. These outputs are not the nominal mission results.\n', encoding='utf-8')
    print(json.dumps(result, indent=2), flush=True)

if __name__ == '__main__':
    main()
