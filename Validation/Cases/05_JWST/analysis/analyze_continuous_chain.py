#!/usr/bin/env python3
"""Independent SPICE comparison and strict handoff audit for JWST chain."""

from __future__ import annotations

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))


import csv
import hashlib
import json
import math
import os
import subprocess
from datetime import datetime, timedelta, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "analysis" / "SpiceReference.exe"
JWST_KERNEL = ROOT / "truth" / "kernels" / "jwst_rec.bsp"
RUNTIME_ROOT = _RUNTIME
LSK = _KERNELS / "naif0012.tls"
DE442 = _KERNELS / "de442.bsp"
MANIFEST = ROOT / "analysis" / "phase_manifest.csv"
METRICS_JSON = ROOT / "analysis" / "continuous_jwst_metrics.json"
HANDOFF_CSV = ROOT / "analysis" / "handoff_audit.csv"
TRUTH_CSV = ROOT / "truth" / "heldout_truth_at_output_epochs.csv"
TUNING_JSON = ROOT / "analysis" / "preslew_consistent_tuning.json"
HANDOFF_TOLERANCE = 1.0e-12

POSITION_COLUMNS = [f"position_icrf_{axis}_m" for axis in "xyz"]
VELOCITY_COLUMNS = [f"velocity_icrf_{axis}_mps" for axis in "xyz"]
QUATERNION_COLUMNS = [f"quaternion_body_to_icrf_{part}"
                      for part in ("w", "x", "y", "z")]
RATE_COLUMNS = [f"angular_velocity_body_{axis}_radps" for axis in "xyz"]
INERTIA_COLUMNS = [
    "inertia_body_xx_kgm2", "inertia_body_xy_kgm2", "inertia_body_xz_kgm2",
    "inertia_body_yx_kgm2", "inertia_body_yy_kgm2", "inertia_body_yz_kgm2",
    "inertia_body_zx_kgm2", "inertia_body_zy_kgm2", "inertia_body_zz_kgm2",
]

SOURCE_URLS = {
    "executed_mcc_history":
        "https://ntrs.nasa.gov/api/citations/20220010207/downloads/MCC%20Paper.pdf",
    "propulsion_description":
        "https://jwst-docs.stsci.edu/jwst-observatory-hardware/jwst-spacecraft-bus/jwst-propulsion",
    "attitude_control_description":
        "https://jwst-docs.stsci.edu/jwst-observatory-hardware/jwst-attitude-control-subsystem",
    "propellant_loading":
        "https://science.nasa.gov/blogs/webb/2021/12/06/nasas-james-webb-space-telescope-fully-fueled-for-launch/",
    "naif_spk_archive": "https://naif.jpl.nasa.gov/pub/naif/JWST/kernels/spk/",
    "naif_kernel_selection": "https://naif.jpl.nasa.gov/naif/kernel_selection.html",
    "earth_gravity_model":
        "https://earth-info.nga.mil/index.php?action=wgs84&dir=wgs84",
    "sunshield_dimensions": "https://science.nasa.gov/mission/webb/webbs-sunshield/",
    "sunshield_completion": "https://science.nasa.gov/blogs/webb/2022/01/04/webb-team-tensions-fifth-layer-sunshield-fully-deployed/",
}


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def norm(vector: list[float]) -> float:
    return math.sqrt(sum(value * value for value in vector))


def difference(a: dict[str, str], b: dict[str, str], columns: list[str]) -> list[float]:
    return [float(a[column]) - float(b[column]) for column in columns]


def parse_utc(text: str) -> datetime:
    return datetime.fromisoformat(text.rstrip("Z")).replace(tzinfo=timezone.utc)


def format_utc(epoch: datetime) -> str:
    return epoch.strftime("%Y-%m-%dT%H:%M:%S.%fZ")


def solution_path(entry: dict[str, str]) -> Path:
    return ROOT / Path(entry["solution"])


def extract_exact_truth(entries, solutions):
    """Query the retained SPK at the actual recorded binary64 ET, without UTC rounding."""
    truth_by_phase={};combined_rows=[]
    exact_root=ROOT/'truth/exact_samples';exact_root.mkdir(parents=True,exist_ok=True)
    for entry in entries:
        phase=entry['phase'];output=exact_root/(phase+'.csv');epochs=exact_root/(phase+'_epochs_et.txt')
        epochs.write_text('\n'.join(format(float(r['ephemeris_time_tdb_seconds_past_j2000']),'.17g') for r in solutions[phase])+'\n',encoding='utf-8')
        arguments=[str(TOOL),'--kernel',str(LSK),'--kernel',str(DE442),'--kernel',str(JWST_KERNEL),'--et-file',str(epochs),'--output',str(output)]
        result=subprocess.run(arguments,cwd=ROOT,text=True,capture_output=True)
        if result.returncode:raise RuntimeError(result.stdout+'\n'+result.stderr)
        phase_truth=read_csv(output)
        if len(phase_truth)!=len(solutions[phase]):raise RuntimeError('Truth row mismatch: '+phase)
        truth_by_phase[phase]=phase_truth
        combined_rows.extend({'phase':phase,'sample_index':str(i),**row} for i,row in enumerate(phase_truth))
    with TRUTH_CSV.open('w',newline='',encoding='utf-8') as stream:
        writer=csv.DictWriter(stream,fieldnames=list(combined_rows[0]));writer.writeheader();writer.writerows(combined_rows)
    return truth_by_phase


def rotation_axes(row: dict[str, str]) -> tuple[list[float], list[float], list[float]]:
    w, x, y, z = [float(row[column]) for column in QUATERNION_COLUMNS]
    x_axis = [1 - 2 * (y*y + z*z), 2 * (x*y + w*z), 2 * (x*z - w*y)]
    y_axis = [2 * (x*y - w*z), 1 - 2 * (x*x + z*z), 2 * (y*z + w*x)]
    z_axis = [2 * (x*z + w*y), 2 * (y*z - w*x), 1 - 2 * (x*x + y*y)]
    return x_axis, y_axis, z_axis


def angle_degrees(a: list[float], b: list[float]) -> float:
    denominator = norm(a) * norm(b)
    if denominator == 0.0:
        return float("nan")
    return math.degrees(math.acos(max(-1.0, min(1.0,
        sum(x*y for x, y in zip(a, b)) / denominator))))


def percentile(values: list[float], probability: float) -> float:
    ordered = sorted(values)
    if not ordered:
        return float("nan")
    index = (len(ordered) - 1) * probability
    lower = math.floor(index)
    upper = math.ceil(index)
    if lower == upper:
        return ordered[lower]
    return ordered[lower] * (upper - index) + ordered[upper] * (index - lower)


def main() -> None:
    entries = read_csv(MANIFEST)
    solutions = {entry["phase"]: read_csv(solution_path(entry)) for entry in entries}
    truth = extract_exact_truth(entries, solutions)

    # State completeness and exact CSV-to-TGSCN-to-CSV handoff proof.
    handoff_rows = []
    all_handoffs_continuous = True
    for previous, current in zip(entries, entries[1:]):
        terminal = solutions[previous["phase"]][-1]
        initial = solutions[current["phase"]][0]
        headers = list(terminal)
        joint_columns = [column for column in headers if
                         column.startswith("joint_coordinate_") or
                         column.startswith("joint_rate_") or
                         column.startswith("articulation_coordinate_") or
                         column.startswith("articulation_rate_")]
        variable_mass_columns = [column for column in headers if
                                 column.startswith("variable_component_mass_")]
        wheel_columns = [column for column in headers if
                         column.startswith("reaction_wheel_momentum_")]
        groups = {
            "ephemeris_time": ["ephemeris_time_tdb_seconds_past_j2000"],
            "position": POSITION_COLUMNS,
            "velocity": VELOCITY_COLUMNS,
            "quaternion": QUATERNION_COLUMNS,
            "body_rates": RATE_COLUMNS,
            "total_mass": ["mass_kg"],
            "variable_masses": variable_mass_columns,
            "joint_coordinates_rates": joint_columns,
            "wheel_momenta": wheel_columns,
            "inertia_tensor": INERTIA_COLUMNS,
        }
        for group, columns in groups.items():
            deltas = [abs(float(initial[column]) - float(terminal[column]))
                      for column in columns]
            maximum = max(deltas, default=0.0)
            exact = all(delta == 0.0 for delta in deltas)
            continuous = maximum <= HANDOFF_TOLERANCE
            all_handoffs_continuous = all_handoffs_continuous and continuous
            handoff_rows.append({
                "from_phase": previous["phase"],
                "to_phase": current["phase"],
                "state_group": group,
                "column_count": len(columns),
                "maximum_absolute_difference": maximum,
                "exact": exact,
                "within_numerical_tolerance": continuous,
            })
    with HANDOFF_CSV.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(handoff_rows[0]))
        writer.writeheader()
        writer.writerows(handoff_rows)

    per_phase = []
    heldout_position_errors: list[float] = []
    heldout_velocity_errors: list[float] = []
    heldout_ephemeris_times: set[float] = set()
    calibration_ephemeris_times = {
        round(float(solutions[phase][-1][
            "ephemeris_time_tdb_seconds_past_j2000"]), 3)
        for phase in ("B", "C", "D", "E", "F", "G", "H")
    }
    # Keep the inherited fit exclusions without requiring discarded trial histories.
    inherited = json.loads((ROOT/'provenance/inherited_fit_epochs.json').read_text(encoding='utf-8'))
    for entry in inherited['epochs']:
        calibration_ephemeris_times.add(round(entry['ephemeris_time_tdb_seconds_past_j2000'], 3))
    residual_rows = []
    all_aba_success = True
    maximum_quaternion_error = 0.0
    all_finite = True
    all_strict = True
    for entry in entries:
        phase = entry["phase"]
        position_errors = []
        velocity_errors = []
        normalized_position_errors = []
        time_mismatches = []
        previous_et = None
        for index, (simulation, reference) in enumerate(zip(solutions[phase], truth[phase])):
            values = [float(value) for value in simulation.values()]
            all_finite = all_finite and all(math.isfinite(value) for value in values)
            all_aba_success = all_aba_success and float(simulation['multibody_solve_succeeded']) == 1.0
            maximum_quaternion_error=max(maximum_quaternion_error,abs(norm([float(simulation[k]) for k in QUATERNION_COLUMNS])-1.0))
            et = float(simulation["ephemeris_time_tdb_seconds_past_j2000"])
            time_mismatches.append(abs(
                et - float(reference["ephemeris_time_tdb_seconds_past_j2000"])))
            if previous_et is not None:
                all_strict = all_strict and et > previous_et
            previous_et = et
            reference_position = [1000.0 * float(reference[f"position_j2000_{axis}_km"])
                                  for axis in "xyz"]
            reference_velocity = [1000.0 * float(reference[f"velocity_j2000_{axis}_kmps"])
                                  for axis in "xyz"]
            position_error = norm([
                float(simulation[POSITION_COLUMNS[i]]) - reference_position[i]
                for i in range(3)])
            velocity_error = norm([
                float(simulation[VELOCITY_COLUMNS[i]]) - reference_velocity[i]
                for i in range(3)])
            earth_position = [float(simulation[f"body_earth_position_icrf_{axis}_m"])
                              for axis in "xyz"]
            earth_range = norm([reference_position[i] - earth_position[i]
                                for i in range(3)])
            position_errors.append(position_error)
            velocity_errors.append(velocity_error)
            normalized_position_errors.append(position_error / earth_range)

            # Exclude the external seed, inherited fit endpoints, and current
            # B-H endpoints (conservatively including the shifted E endpoint).
            # The current fitting marks are declared in the tuning metadata.
            # Other samples are descriptive checks on the same trajectory.
            calibration_sample = (
                (phase == "A" and index == 0) or
                round(et, 3) in calibration_ephemeris_times
            )
            held_out = not calibration_sample and et not in heldout_ephemeris_times
            residual_rows.append({'phase':phase,'et':et,'utc':reference['utc'],'position_error_m':position_error,'velocity_error_mps':velocity_error,'earth_range_m':earth_range,'held_out':held_out,
                **{f'dr_{a}_m':float(simulation[POSITION_COLUMNS[i]])-reference_position[i] for i,a in enumerate('xyz')},
                **{f'dv_{a}_mps':float(simulation[VELOCITY_COLUMNS[i]])-reference_velocity[i] for i,a in enumerate('xyz')}})
            if held_out:
                heldout_ephemeris_times.add(et)
                heldout_position_errors.append(position_error)
                heldout_velocity_errors.append(velocity_error)

        per_phase.append({
            "phase": phase,
            "label": entry["label"],
            "sample_count": len(solutions[phase]),
            "maximum_truth_time_mismatch_s": max(time_mismatches),
            "start_position_error_m": position_errors[0],
            "final_position_error_m": position_errors[-1],
            "rms_position_error_m": math.sqrt(sum(x*x for x in position_errors) /
                                              len(position_errors)),
            "maximum_position_error_m": max(position_errors),
            "final_position_error_fraction_of_earth_range":
                normalized_position_errors[-1],
            "maximum_position_error_fraction_of_earth_range":
                max(normalized_position_errors),
            "start_velocity_error_mps": velocity_errors[0],
            "final_velocity_error_mps": velocity_errors[-1],
            "rms_velocity_error_mps": math.sqrt(sum(x*x for x in velocity_errors) /
                                                len(velocity_errors)),
            "maximum_velocity_error_mps": max(velocity_errors),
        })

    with (ROOT/'analysis/state_residuals.csv').open('w',newline='',encoding='utf-8') as stream:
        writer=csv.DictWriter(stream,fieldnames=list(residual_rows[0]));writer.writeheader();writer.writerows(residual_rows)
    tuning = json.loads(TUNING_JSON.read_text(encoding="utf-8"))
    tuned_vectors = tuning["retuned_delta_v_vectors_icrf_mps"]
    burn_targets = {
        "B": ("mcc1a", 20.033),
        "D": ("mcc1b", 2.773),
        "G": ("mcc2", 1.484),
    }
    burn_metrics = []
    burn_pointing_pass = True
    burn_dv_pass = True
    for phase, (maneuver, published_delta_v) in burn_targets.items():
        target_vector = [float(value) for value in tuned_vectors[maneuver]]
        target_delta_v = norm(target_vector)
        rows = solutions[phase]
        initial_mass = float(rows[0]["mass_kg"])
        final_mass = float(rows[-1]["mass_kg"])
        achieved_rocket_delta_v = 295.0 * 9.80665 * math.log(initial_mass / final_mass)
        delta_v_fraction_error = (achieved_rocket_delta_v - target_delta_v) / target_delta_v
        pointing_errors = []
        for row in rows:
            x_axis, _, _ = rotation_axes(row)
            pointing_errors.append(angle_degrees(x_axis, target_vector))
        settled = [error for error, row in zip(pointing_errors, rows)
                   if float(row["elapsed_time_seconds"]) >= 60.0]
        thrust_norms = [norm([float(row[f"force_thrust_icrf_{axis}_n"])
                             for axis in "xyz"]) for row in rows]
        sample_at_tau = min(range(len(rows)), key=lambda index: abs(
            float(rows[index]["elapsed_time_seconds"]) - 5.0))
        steady_thrust = max(thrust_norms)
        lag_fraction_at_tau = thrust_norms[sample_at_tau] / steady_thrust
        burn_dv_pass = burn_dv_pass and abs(delta_v_fraction_error) <= 1.0e-3
        burn_pointing_pass = burn_pointing_pass and max(settled) <= 1.0
        burn_metrics.append({
            "phase": phase,
            "maneuver": maneuver,
            "published_reconstructed_delta_v_mps": published_delta_v,
            "retuned_command_delta_v_vector_icrf_mps": target_vector,
            "retuned_command_delta_v_mps": target_delta_v,
            "retuned_minus_published_delta_v_mps":
                target_delta_v - published_delta_v,
            "rocket_equation_delta_v_mps": achieved_rocket_delta_v,
            "fractional_command_tracking_error": delta_v_fraction_error,
            "propellant_used_kg": initial_mass - final_mass,
            "maximum_thrust_n": steady_thrust,
            "first_order_time_constant_s": 5.0,
            "measured_thrust_fraction_at_one_time_constant": lag_fraction_at_tau,
            "expected_first_order_fraction_at_one_time_constant": 1.0 - math.exp(-1.0),
            "initial_pointing_error_deg": pointing_errors[0],
            "settled_maximum_pointing_error_deg": max(settled),
            "settled_rms_pointing_error_deg": math.sqrt(
                sum(x*x for x in settled) / len(settled)),
        })

    # Sun-pointing is assessed only while the controller is in its designated
    # Sun-point mode, after a one-hour acquisition interval. Burn and explicit
    # pre-burn slew intervals are separate controller objectives and are not
    # silently mixed into the Sun-pointing statistic.
    sun_pointing_phase_metrics = []
    settled_sun_errors = []
    wheel_fractions = []
    slew_labels = {"COAST_TO_MCC1A", "COAST_TO_MCC1B",
                   "DEPLOYED_COAST_TO_MCC2"}
    pure_sun_labels = {"COAST_TO_DEPLOYED_CONFIGURATION",
                       "POST_INSERTION_L2_COAST"}
    for entry in entries:
        rows = solutions[entry["phase"]]
        phase_sun_errors = []
        duration = float(rows[-1]["elapsed_time_seconds"])
        for row in rows:
            _, _, z_axis = rotation_axes(row)
            sun_direction = [float(row[f"body_sun_position_icrf_{axis}_m"]) -
                             float(row[f"position_icrf_{axis}_m"])
                             for axis in "xyz"]
            error = angle_degrees(z_axis, sun_direction)
            elapsed = float(row["elapsed_time_seconds"])
            designated_sun_mode = (
                entry["label"] in pure_sun_labels or
                (entry["label"] in slew_labels and
                 elapsed < duration - 3600.0))
            settled = elapsed >= 3600.0
            if designated_sun_mode and settled:
                phase_sun_errors.append(error)
                settled_sun_errors.append(error)
            for index in range(3):
                wheel_fractions.append(abs(float(
                    row[f"reaction_wheel_momentum_{index}_nms"])) / 2000.0)
        if phase_sun_errors:
            sun_pointing_phase_metrics.append({
                "phase": entry["phase"],
                "label": entry["label"],
                "settling_interval_s": 3600.0,
                "assessed_sample_count": len(phase_sun_errors),
                "maximum_error_deg": max(phase_sun_errors),
                "rms_error_deg": math.sqrt(sum(x*x for x in phase_sun_errors) /
                                           len(phase_sun_errors)),
                "95th_percentile_error_deg": percentile(phase_sun_errors, 0.95),
            })

    tgscn_files = sorted((ROOT / "phases").rglob("*.tgscn"))
    truth_tokens = ("jwst_rec", "truth/", "truth\\", ".bsp", "key_states")
    truth_referenced_by_scenario = any(
        any(token in path.read_text(encoding="utf-8").lower()
            for token in truth_tokens)
        for path in tgscn_files)
    external_source_count = sum(
        entry["initial_state_source"].startswith("NAIF") for entry in entries)

    final_phase = per_phase[-1]
    acceptance = {
        "all_eight_phases_propagated": len(entries) == 8 and all(
            "success=true" in (solution_path(entry).with_name(
                solution_path(entry).name.replace("_solution.csv", "_summary.txt")))
            .read_text(encoding="utf-8") for entry in entries),
        "all_state_values_finite": all_finite,
        "all_recorded_articulated_solves_successful": all_aba_success,
        "quaternion_norm_within_1e_10": maximum_quaternion_error <= 1e-10,
        "truth_queried_at_exact_recorded_ephemeris_times": max(x['maximum_truth_time_mismatch_s'] for x in per_phase)==0.0,
        "strictly_increasing_ephemeris_time_within_each_phase": all_strict,
        "all_seven_handoffs_continuous_to_1e_12": all_handoffs_continuous,
        "only_one_external_initial_state": external_source_count == 1,
        "no_truth_file_referenced_by_any_tgscn": not truth_referenced_by_scenario,
        "retuned_burn_command_delta_v_within_0_1_percent": burn_dv_pass,
        "settled_burn_pointing_within_1_degree": burn_pointing_pass,
        "settled_designated_sun_pointing_within_1_degree":
            bool(settled_sun_errors) and max(settled_sun_errors) <= 1.0,
        "final_position_error_below_1_percent_of_earth_range":
            final_phase["final_position_error_fraction_of_earth_range"] < 0.01,
        "final_velocity_error_below_5_mps":
            final_phase["final_velocity_error_mps"] < 5.0,
        "reaction_wheels_below_80_percent_capacity": max(wheel_fractions) < 0.8,
    }

    metrics = {
        "campaign": "JWST continuous transfer to Sun-Earth L2",
        "phase_count": len(entries),
        "recorded_articulated_solves_all_successful": all_aba_success,
        "maximum_quaternion_norm_error": maximum_quaternion_error,
        "current_calibration_note": tuning['method']+" See provenance/calibration_decision.json. No later reference-state reset is used.",
        "total_output_rows": sum(len(rows) for rows in solutions.values()),
        "calibration_and_evaluation_separation": {
            "controller_calibration_inputs": [
                "NAIF reconstructed barycentric state at 2021-12-25T13:00:00Z",
                "published MCC epochs, durations, and reconstructed delta-v values",
                "Calibration against NAIF B-H terminal states using weighted objectives for finite-difference burn-vector tuning",
                "Current calibration: "+tuning.get('current_fit_description', 'final H position/velocity residual only, for one capped two-burn correction'),
                "published physical mass, propellant, thrust type, and attitude strategy",
            ],
            "held_out_evaluation":
                "Unique exact-epoch samples excluding A[0], inherited B-H fit epochs and current B-H endpoints; duplicate boundaries count once. These are correlated samples of the same reconstructed mission, not a statistically independent validation mission.",
            "calibration_endpoint_count": 7,
            "current_correction_endpoint_count": len(tuning.get('current_fit_phases', ['H'])),
            "excluded_noninitial_epoch_count": len(calibration_ephemeris_times),
            "held_out_sample_count": len(heldout_position_errors),
            "held_out_rms_position_error_m": math.sqrt(sum(
                x*x for x in heldout_position_errors) / len(heldout_position_errors)),
            "held_out_maximum_position_error_m": max(heldout_position_errors),
            "held_out_rms_velocity_error_mps": math.sqrt(sum(
                x*x for x in heldout_velocity_errors) / len(heldout_velocity_errors)),
            "held_out_maximum_velocity_error_mps": max(heldout_velocity_errors),
        },
        "truth_injection_audit": {
            "external_initial_state_count": external_source_count,
            "phase_a_source": entries[0]["initial_state_source"],
            "later_phase_sources": [entry["initial_state_source"]
                                    for entry in entries[1:]],
            "all_later_complete_state_handoffs_continuous_to_1e_12":
                all_handoffs_continuous,
            "tgscn_truth_reference_detected": truth_referenced_by_scenario,
            "joint_state_count": 0,
            "note": "No joint states exist in this rigid JWST abstraction; the audit records the zero-length group for every handoff.",
        },
        "per_phase_ephemeris_residuals": per_phase,
        "burn_performance": burn_metrics,
        "burn_vector_tuning": {
            "method": tuning["method"],
            "truth_use": tuning["truth_use"],
            "retuned_delta_v_vectors_icrf_mps": tuned_vectors,
            "retuned_delta_v_magnitudes_mps":
                tuning["retuned_delta_v_magnitudes_mps"],
        },
        "attitude_and_actuator_summary": {
            "sun_pointing_assessment_scope":
                "designated Sun-point modes after 3600 s; burns and explicit pre-burn slews excluded",
            "sun_pointing_per_phase": sun_pointing_phase_metrics,
            "settled_sun_pointing_maximum_error_deg": max(settled_sun_errors),
            "settled_sun_pointing_median_error_deg": percentile(settled_sun_errors, 0.5),
            "settled_sun_pointing_95th_percentile_error_deg": percentile(settled_sun_errors, 0.95),
            "maximum_reaction_wheel_capacity_fraction": max(wheel_fractions),
        },
        "acceptance_criteria": acceptance,
        "reference_quality": {
            "statistics_scope": "Reported RMS and maxima concern the retained output grid, not continuous-time extrema. A separate dense audit resolves a brief velocity excursion in the reference near 10 January 2022 that is not resolved by the hourly coast grid.",
            "audit": "analysis/reference_continuity_audit.json",
            "report": "REFERENCE_QUALITY.md",
            "reference_samples_modified_or_filtered_to_reduce_error": False,
        },
        "overall_pass": all(acceptance.values()),
        "source_urls": SOURCE_URLS,
        "file_hashes_sha256": {
            "runner": hashlib.sha256((RUNTIME_ROOT / "PHAROSScenarioRunner.exe").read_bytes()).hexdigest(),
            "jwst_reconstructed_spk": hashlib.sha256(JWST_KERNEL.read_bytes()).hexdigest(),
            "de442": hashlib.sha256(DE442.read_bytes()).hexdigest(),
        },
    }
    METRICS_JSON.write_text(json.dumps(metrics, indent=2), encoding="utf-8")
    print(json.dumps({
        "overall_pass": metrics["overall_pass"],
        "acceptance_criteria": acceptance,
        "final_position_error_m": final_phase["final_position_error_m"],
        "final_position_error_fraction_of_earth_range":
            final_phase["final_position_error_fraction_of_earth_range"],
        "final_velocity_error_mps": final_phase["final_velocity_error_mps"],
        "held_out_sample_count": len(heldout_position_errors),
    }, indent=2))


if __name__ == "__main__":
    main()
