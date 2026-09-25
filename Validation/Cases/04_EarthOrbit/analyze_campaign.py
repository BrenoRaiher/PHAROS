from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parent
CASES = {
    "four_wheel": ROOT / "results/04a_four_wheel_recovery/04a_four_wheel_recovery_solution.csv",
    "attitude_thruster": ROOT / "results/04b_thruster_recovery/04b_thruster_recovery_solution.csv",
}
POINTING_LIMIT_DEG = 0.5
RATE_LIMIT_RADPS = 0.002
SUSTAINED_SECONDS = 20.0
SIGNIFICANT_PERTURBATION_DEG = 3.0


def read_rows(path: Path) -> list[dict[str, float]]:
    with path.open("r", encoding="utf-8", newline="") as stream:
        return [
            {name: float(value) for name, value in row.items()}
            for row in csv.DictReader(stream)
        ]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def vector(row: dict[str, float], pattern: str) -> tuple[float, float, float]:
    return tuple(row[pattern.format(axis=axis)] for axis in "xyz")


def subtract(a, b):
    return tuple(x - y for x, y in zip(a, b))


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def norm(a):
    return math.sqrt(dot(a, a))


def rotate(q, value):
    w, x, y, z = q
    qv = (x, y, z)
    inner = tuple(c + w * v for c, v in zip(cross(qv, value), value))
    correction = cross(qv, inner)
    return tuple(v + 2.0 * c for v, c in zip(value, correction))


def angle_degrees(a, b):
    denominator = norm(a) * norm(b)
    cosine = max(-1.0, min(1.0, dot(a, b) / denominator))
    return math.degrees(math.acos(cosine))


def row_metrics(row: dict[str, float]) -> dict[str, float]:
    q = tuple(
        row[f"quaternion_body_to_icrf_{axis}"]
        for axis in ("w", "x", "y", "z")
    )
    body_x = rotate(q, (1.0, 0.0, 0.0))
    position = vector(row, "position_icrf_{axis}_m")
    sun = vector(row, "body_sun_position_icrf_{axis}_m")
    earth = vector(row, "body_earth_position_icrf_{axis}_m")
    omega = vector(row, "angular_velocity_body_{axis}_radps")
    thrust_torque = vector(row, "torque_thrust_body_{axis}_nm")
    return {
        "pointing_error_deg": angle_degrees(body_x, subtract(sun, position)),
        "angular_rate_radps": norm(omega),
        "quaternion_norm_error": abs(norm(q) - 1.0),
        "altitude_m": norm(subtract(position, earth)) - row["body_earth_reference_radius_m"],
        "thrust_torque_nm": norm(thrust_torque),
    }


def nearest_row(rows, target):
    return min(rows, key=lambda row: abs(row["elapsed_time_seconds"] - target))


def first_sustained_recovery(rows, start, end, derived):
    candidates = [index for index, row in enumerate(rows) if start <= row["elapsed_time_seconds"] <= end]
    for index in candidates:
        finish = rows[index]["elapsed_time_seconds"] + SUSTAINED_SECONDS
        window = [
            j
            for j in range(index, len(rows))
            if rows[j]["elapsed_time_seconds"] <= finish + 1.0e-9
        ]
        if not window or rows[window[-1]]["elapsed_time_seconds"] < finish - 0.26:
            continue
        if all(
            derived[j]["pointing_error_deg"] < POINTING_LIMIT_DEG
            and derived[j]["angular_rate_radps"] < RATE_LIMIT_RADPS
            for j in window
        ):
            return rows[index]["elapsed_time_seconds"]
    return None


schedule = json.loads((ROOT / "disturbance_schedule.json").read_text(encoding="utf-8"))
all_case_metrics = {}
processed_rows = []
for case_name, path in CASES.items():
    rows = read_rows(path)
    if not rows:
        raise RuntimeError(f"No telemetry in {path}")
    derived = [row_metrics(row) for row in rows]
    nonfinite = sum(
        1 for row in rows for value in row.values() if not math.isfinite(value)
    )
    events = []
    for event_index, event in enumerate(schedule["events"]):
        start = event["start_s"]
        pulse_end = start + event["duration_s"]
        recovery_start = pulse_end + schedule["recovery_holdoff_after_pulse_s"]
        next_limit = (
            schedule["events"][event_index + 1]["start_s"]
            - schedule["gimbal_lead_s"]
            if event_index + 1 < len(schedule["events"])
            else rows[-1]["elapsed_time_seconds"]
        )
        interval_indices = [
            index
            for index, row in enumerate(rows)
            if start <= row["elapsed_time_seconds"] <= next_limit
        ]
        pulse_indices = [
            index
            for index, row in enumerate(rows)
            if start <= row["elapsed_time_seconds"] <= pulse_end + 0.25
        ]
        ignition = nearest_row(rows, start)
        recovered_at = first_sustained_recovery(
            rows, recovery_start, next_limit, derived
        )
        peak_index = max(
            interval_indices,
            key=lambda index: derived[index]["pointing_error_deg"],
        )
        events.append(
            {
                "event": event_index + 1,
                **event,
                "ignition_sample_time_s": ignition["elapsed_time_seconds"],
                "ignition_yaw_error_deg": math.degrees(
                    ignition["articulation_coordinate_0_rad_or_m"] - event["yaw_rad"]
                ),
                "ignition_pitch_error_deg": math.degrees(
                    ignition["articulation_coordinate_1_rad_or_m"] - event["pitch_rad"]
                ),
                "maximum_recorded_disturbance_torque_nm": max(
                    derived[index]["thrust_torque_nm"] for index in pulse_indices
                ),
                "peak_pointing_error_deg": derived[peak_index]["pointing_error_deg"],
                "peak_pointing_error_time_s": rows[peak_index]["elapsed_time_seconds"],
                "peak_angular_rate_radps": max(
                    derived[index]["angular_rate_radps"] for index in interval_indices
                ),
                "recovery_enabled_time_s": recovery_start,
                "sustained_recovery_time_s": recovered_at,
                "recovery_duration_s": (
                    recovered_at - recovery_start if recovered_at is not None else None
                ),
                "recovered_before_next_event": recovered_at is not None,
                "perturbation_exceeded_3_deg":
                    derived[peak_index]["pointing_error_deg"] > SIGNIFICANT_PERTURBATION_DEG,
            }
        )
    wheel_columns = sorted(
        name for name in rows[0] if name.startswith("reaction_wheel_momentum_")
    )
    maximum_wheel_momentum = (
        max(abs(row[name]) for row in rows for name in wheel_columns)
        if wheel_columns
        else None
    )
    initial_position = subtract(
        vector(rows[0], "position_icrf_{axis}_m"),
        vector(rows[0], "body_earth_position_icrf_{axis}_m"),
    )
    final_position = subtract(
        vector(rows[-1], "position_icrf_{axis}_m"),
        vector(rows[-1], "body_earth_position_icrf_{axis}_m"),
    )
    initial_velocity = subtract(
        vector(rows[0], "velocity_icrf_{axis}_mps"),
        vector(rows[0], "body_earth_velocity_icrf_{axis}_mps"),
    )
    final_velocity = subtract(
        vector(rows[-1], "velocity_icrf_{axis}_mps"),
        vector(rows[-1], "body_earth_velocity_icrf_{axis}_mps"),
    )
    summary_path = path.with_name(path.name.replace("_solution.csv", "_summary.txt"))
    summary_success = "success=true" in summary_path.read_text(encoding="utf-8")
    case_metrics = {
        "solution_csv": str(path.relative_to(ROOT)).replace("\\", "/"),
        "solution_sha256": sha256(path),
        "sample_count": len(rows),
        "summary_success": summary_success,
        "initial_pointing_error_deg": derived[0]["pointing_error_deg"],
        "terminal_pointing_error_deg": derived[-1]["pointing_error_deg"],
        "maximum_quaternion_norm_error": max(item["quaternion_norm_error"] for item in derived),
        "maximum_angular_rate_radps": max(item["angular_rate_radps"] for item in derived),
        "initial_mass_kg": rows[0]["mass_kg"],
        "terminal_mass_kg": rows[-1]["mass_kg"],
        "propellant_consumed_kg": rows[0]["mass_kg"] - rows[-1]["mass_kg"],
        "maximum_absolute_wheel_momentum_nms": maximum_wheel_momentum,
        "one_orbit_relative_position_closure_m": norm(subtract(final_position, initial_position)),
        "one_orbit_relative_velocity_closure_mps": norm(subtract(final_velocity, initial_velocity)),
        "initial_altitude_m": derived[0]["altitude_m"],
        "minimum_altitude_m": min(item["altitude_m"] for item in derived),
        "maximum_altitude_m": max(item["altitude_m"] for item in derived),
        "nonfinite_numeric_cells": nonfinite,
        "events": events,
    }
    acceptance = {
        "runner_summary_success": summary_success,
        "five_disturbance_events": len(events) == 5,
        "all_perturbations_exceed_3_deg": all(
            event["perturbation_exceeded_3_deg"] for event in events
        ),
        "all_recoveries_before_next_event": all(
            event["recovered_before_next_event"] for event in events
        ),
        "terminal_pointing_below_0p5_deg":
            derived[-1]["pointing_error_deg"] < POINTING_LIMIT_DEG,
        "quaternion_normalized":
            case_metrics["maximum_quaternion_norm_error"] < 1.0e-10,
        "gimbal_at_commanded_angles_at_ignition": all(
            abs(event["ignition_yaw_error_deg"]) < 0.1
            and abs(event["ignition_pitch_error_deg"]) < 0.1
            for event in events
        ),
        "all_values_finite": nonfinite == 0,
    }
    if case_name == "four_wheel":
        acceptance["four_wheels_present"] = len(wheel_columns) == 4
        acceptance["wheels_below_momentum_capacity"] = maximum_wheel_momentum < 50.0
    else:
        acceptance["physical_thruster_propellant_consumption"] = (
            case_metrics["propellant_consumed_kg"] > 0.0
        )
    case_metrics["acceptance"] = acceptance
    case_metrics["all_acceptance_checks_pass"] = all(acceptance.values())
    all_case_metrics[case_name] = case_metrics
    for row, item in zip(rows, derived):
        processed_rows.append(
            {
                "case": case_name,
                "elapsed_time_seconds": row["elapsed_time_seconds"],
                "sun_pointing_error_deg": item["pointing_error_deg"],
                "angular_rate_radps": item["angular_rate_radps"],
                "thrust_torque_nm": item["thrust_torque_nm"],
                "yaw_coordinate_rad": row["articulation_coordinate_0_rad_or_m"],
                "pitch_coordinate_rad": row["articulation_coordinate_1_rad_or_m"],
                "mass_kg": row["mass_kg"],
                "wheel_0_nms": row.get("reaction_wheel_momentum_0_nms", 0.0),
                "wheel_1_nms": row.get("reaction_wheel_momentum_1_nms", 0.0),
                "wheel_2_nms": row.get("reaction_wheel_momentum_2_nms", 0.0),
                "wheel_3_nms": row.get("reaction_wheel_momentum_3_nms", 0.0),
            }
        )

metrics = {
    "schema": "TGSimCore Earth-orbit attitude-recovery campaign v2",
    "purpose": "Demonstrate propagated attitude dynamics and repeated Sun-pointing recovery using either four redundant reaction wheels or physical attitude thrusters.",
    "disturbance_schedule": schedule,
    "acceptance_thresholds": {
        "pointing_error_deg": POINTING_LIMIT_DEG,
        "angular_rate_radps": RATE_LIMIT_RADPS,
        "sustained_recovery_seconds": SUSTAINED_SECONDS,
        "significant_perturbation_deg": SIGNIFICANT_PERTURBATION_DEG,
    },
    "cases": all_case_metrics,
    "all_cases_pass": all(
        case["all_acceptance_checks_pass"] for case in all_case_metrics.values()
    ),
}
(ROOT / "earth_attitude_recovery_metrics.json").write_text(
    json.dumps(metrics, indent=2, allow_nan=False) + "\n", encoding="utf-8"
)
with (ROOT / "earth_attitude_recovery_timeseries.csv").open(
    "w", encoding="utf-8", newline=""
) as stream:
    writer = csv.DictWriter(stream, fieldnames=list(processed_rows[0]))
    writer.writeheader()
    writer.writerows(processed_rows)
print(json.dumps({
    name: {
        "pass": case["all_acceptance_checks_pass"],
        "terminal_error_deg": case["terminal_pointing_error_deg"],
        "recovery_durations_s": [event["recovery_duration_s"] for event in case["events"]],
        "peak_errors_deg": [event["peak_pointing_error_deg"] for event in case["events"]],
        "propellant_consumed_kg": case["propellant_consumed_kg"],
        "max_wheel_momentum_nms": case["maximum_absolute_wheel_momentum_nms"],
    }
    for name, case in all_case_metrics.items()
}, indent=2))
