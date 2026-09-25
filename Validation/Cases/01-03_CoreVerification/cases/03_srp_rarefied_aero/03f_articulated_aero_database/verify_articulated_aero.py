from __future__ import annotations

import csv
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parent
SOLUTION = ROOT / "results" / "articulated_aero_database_solution.csv"
DATABASE = ROOT / "data" / "aerodynamics.csv"
REPORT = ROOT / "results" / "articulated_aero_database_metrics.json"


def numeric_check(metric: str, actual: float, expected: float, tolerance: float) -> dict:
    error = abs(actual - expected)
    return {
        "metric": metric,
        "actual": actual,
        "expected": expected,
        "absolute_error": error,
        "tolerance": tolerance,
        "pass": error <= tolerance,
    }


def load_database() -> list[list[float]]:
    with DATABASE.open("r", encoding="utf-8", newline="") as stream:
        return [[float(value) for value in row] for row in csv.reader(stream)]


def expected_coefficients(rows: list[list[float]], eta: list[float]) -> list[float]:
    minima = [min(row[5 + axis] for row in rows) for axis in range(3)]
    maxima = [max(row[5 + axis] for row in rows) for axis in range(3)]
    normalized_query = [
        (eta[axis] - minima[axis]) / (maxima[axis] - minima[axis])
        for axis in range(3)
    ]

    weighted = [0.0] * 6
    weight_sum = 0.0
    for row in rows:
        normalized_sample = [
            (row[5 + axis] - minima[axis]) / (maxima[axis] - minima[axis])
            for axis in range(3)
        ]
        squared_distance = sum(
            (normalized_query[axis] - normalized_sample[axis]) ** 2
            for axis in range(3)
        )
        weight = 1.0 / squared_distance
        for output in range(6):
            weighted[output] += weight * row[8 + output]
        weight_sum += weight
    return [item / weight_sum for item in weighted]


def main() -> None:
    with SOLUTION.open("r", encoding="utf-8", newline="") as stream:
        solution = list(csv.DictReader(stream))
    initial = solution[0]
    eta = [float(initial[f"articulation_coordinate_{index}_rad_or_m"]) for index in range(3)]
    expected_eta = [0.2, -0.3, 0.4]
    coefficients = expected_coefficients(load_database(), eta)
    output_columns = [
        "aerodynamic_force_coefficient_body_x",
        "aerodynamic_force_coefficient_body_y",
        "aerodynamic_force_coefficient_body_z",
        "aerodynamic_moment_coefficient_about_cm_body_x",
        "aerodynamic_moment_coefficient_about_cm_body_y",
        "aerodynamic_moment_coefficient_about_cm_body_z",
    ]

    checks = [
        numeric_check(
            "maximum initial articulation-coordinate error",
            max(abs(actual - expected) for actual, expected in zip(eta, expected_eta)),
            0.0,
            1.0e-14,
        ),
        numeric_check(
            "initial dynamic pressure",
            float(initial["aerodynamic_dynamic_pressure_pa"]),
            0.032,
            1.0e-9,
        ),
        numeric_check(
            "initial molecular speed ratio",
            float(initial["aerodynamic_molecular_speed_ratio"]),
            10.547607654346578,
            1.0e-8,
        ),
        numeric_check(
            "initial Knudsen number",
            float(initial["aerodynamic_knudsen_number"]),
            11.313708498984751,
            1.0e-8,
        ),
    ]
    checks.extend(
        numeric_check(column, float(initial[column]), expected, 1.0e-12)
        for column, expected in zip(output_columns, coefficients)
    )
    checks.extend([
        numeric_check(
            "database-used flag",
            float(initial["aerodynamic_database_used"]),
            1.0,
            0.0,
        ),
        numeric_check(
            "fallback-used flag",
            float(initial["aerodynamic_fallback_used"]),
            0.0,
            0.0,
        ),
        numeric_check(
            "outside-validity flag",
            float(initial["aerodynamics_outside_validity"]),
            0.0,
            0.0,
        ),
    ])

    document = {
        "case": "03f_articulated_aero_database",
        "oracle": "Independent eight-neighbor Shepard interpolation over three articulation coordinates",
        "database_rows": 8,
        "articulation_dofs": 3,
        "query_eta_rad": eta,
        "expected_coefficients": coefficients,
        "checks": checks,
        "summary": {
            "passed": sum(check["pass"] for check in checks),
            "failed": sum(not check["pass"] for check in checks),
        },
    }
    REPORT.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(document["summary"]))
    if document["summary"]["failed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
