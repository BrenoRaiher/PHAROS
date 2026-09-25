from __future__ import annotations

import csv
import json
import math
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
G0 = 9.80665


def read_rows(relative_path: str) -> list[dict[str, str]]:
    path = ROOT / relative_path
    with path.open("r", encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def read_numeric_rows(relative_path: str) -> list[list[float]]:
    path = ROOT / relative_path
    with path.open("r", encoding="utf-8", newline="") as stream:
        return [[float(item) for item in row] for row in csv.reader(stream)]


def value(row: dict[str, str], name: str) -> float:
    return float(row[name])


def vector(
    row: dict[str, str],
    prefix: str,
    suffix: str,
) -> list[float]:
    return [value(row, f"{prefix}_{axis}{suffix}") for axis in "xyz"]


def norm(items) -> float:
    return math.sqrt(sum(item * item for item in items))


def articulated_aero_expected_coefficients(
    database_rows: list[list[float]],
    eta: list[float],
) -> list[float]:
    """Independent p=2 Shepard interpolation in the three eta coordinates."""
    minima = [min(row[5 + axis] for row in database_rows) for axis in range(3)]
    maxima = [max(row[5 + axis] for row in database_rows) for axis in range(3)]
    normalized_query = [
        (eta[axis] - minima[axis]) / (maxima[axis] - minima[axis])
        for axis in range(3)
    ]
    weighted = [0.0] * 6
    weight_sum = 0.0
    for row in database_rows:
        normalized_sample = [
            (row[5 + axis] - minima[axis]) /
            (maxima[axis] - minima[axis])
            for axis in range(3)
        ]
        squared_distance = sum(
            (normalized_query[axis] - normalized_sample[axis]) ** 2
            for axis in range(3)
        )
        weight = 1.0 / squared_distance
        weight_sum += weight
        for output in range(6):
            weighted[output] += weight * row[8 + output]
    return [item / weight_sum for item in weighted]


def maximum_vector_norm(
    rows: list[dict[str, str]],
    prefix: str,
    suffix: str,
) -> float:
    return max(norm(vector(row, prefix, suffix)) for row in rows)


def numeric_check(
    metric: str,
    actual: float,
    expected: float,
    tolerance: float,
    unit: str = "",
) -> dict:
    error = abs(actual - expected)
    return {
        "metric": metric,
        "unit": unit,
        "actual": actual,
        "expected": expected,
        "absolute_error": error,
        "tolerance": tolerance,
        "pass": error <= tolerance,
    }


def range_check(
    metric: str,
    actual_values,
    expected: float,
    tolerance: float,
    unit: str = "",
) -> dict:
    actual_values = list(actual_values)
    maximum_error = max(abs(item - expected) for item in actual_values)
    return {
        "metric": metric,
        "unit": unit,
        "actual_range": [min(actual_values), max(actual_values)],
        "expected": expected,
        "maximum_absolute_error": maximum_error,
        "tolerance": tolerance,
        "pass": maximum_error <= tolerance,
    }


def integrity(relative_path: str, nominal_rows: int) -> dict:
    rows = read_rows(relative_path)
    path = ROOT / relative_path
    times = [
        value(row, "ephemeris_time_tdb_seconds_past_j2000")
        for row in rows
    ]
    nonfinite = 0
    malformed = 0
    field_count = len(rows[0]) if rows else 0
    quaternion_error = 0.0
    solve_failures = 0
    for row in rows:
        if len(row) != field_count:
            malformed += 1
        for item in row.values():
            try:
                if not math.isfinite(float(item)):
                    nonfinite += 1
            except (TypeError, ValueError):
                nonfinite += 1
        quaternion = [
            value(row, f"quaternion_body_to_icrf_{axis}")
            for axis in "wxyz"
        ]
        quaternion_error = max(
            quaternion_error,
            abs(math.sqrt(sum(item * item for item in quaternion)) - 1.0),
        )
        solve_failures += (
            value(row, "multibody_solve_succeeded") != 1.0
        )
    terminal_state_differences = []
    if len(rows) >= 2:
        for column in rows[-1]:
            if column == "ephemeris_time_tdb_seconds_past_j2000":
                continue
            difference = abs(float(rows[-1][column]) - float(rows[-2][column]))
            if difference != 0.0:
                terminal_state_differences.append(difference)
    return {
        "solution_csv": relative_path,
        "rows_actual": len(rows),
        "rows_nominal": nominal_rows,
        "row_count_matches_nominal": len(rows) == nominal_rows,
        "columns": field_count,
        "malformed_rows": malformed,
        "nonfinite_numeric_cells": nonfinite,
        "strictly_increasing_absolute_et": all(
            later > earlier for earlier, later in zip(times, times[1:])
        ),
        "last_absolute_et_gap_s": (
            times[-1] - times[-2] if len(times) >= 2 else None
        ),
        "terminal_pair_different_state_cell_count":
            len(terminal_state_differences),
        "terminal_pair_maximum_absolute_state_cell_difference": (
            max(terminal_state_differences)
            if terminal_state_differences
            else 0.0
        ),
        "maximum_quaternion_norm_error": quaternion_error,
        "multibody_solve_failures": solve_failures,
        "bytes": path.stat().st_size,
    }


CASE_FILES = {
    "02a_revolute": (
        "cases/02_actuated_multibody/02a_revolute/results/"
        "revolute_reaction_solution.csv",
        11,
    ),
    "02b_prismatic": (
        "cases/02_actuated_multibody/02b_prismatic/results/"
        "prismatic_reaction_solution.csv",
        11,
    ),
    "02c_coordinate_stop": (
        "cases/02_actuated_multibody/02c_coordinate_stop/results/"
        "coordinate_stop_solution.csv",
        11,
    ),
    "02d_nested_tree": (
        "cases/02_actuated_multibody/02d_nested_tree/results/"
        "nested_tree_solution.csv",
        1001,
    ),
    "02e_reaction_wheel": (
        "cases/02_actuated_multibody/02e_reaction_wheel/results/"
        "reaction_wheel_solution.csv",
        301,
    ),
    "02f_commanded_thrust": (
        "cases/02_actuated_multibody/02f_commanded_thrust/results/"
        "commanded_thrust_solution.csv",
        151,
    ),
    "02g_direct_torque": (
        "cases/02_actuated_multibody/02g_direct_torque/results/"
        "direct_torque_solution.csv",
        201,
    ),
    "02h_prescribed_thrust": (
        "cases/02_actuated_multibody/02h_prescribed_thrust/results/"
        "prescribed_thrust_solution.csv",
        301,
    ),
    "03a_srp_one_au": (
        "cases/03_srp_rarefied_aero/03a_srp_one_au/results/"
        "srp_one_au_solution.csv",
        11,
    ),
    "03b_umbra_off": (
        "cases/03_srp_rarefied_aero/03b_earth_umbra_off/results/"
        "earth_umbra_off_solution.csv",
        11,
    ),
    "03b_umbra_on": (
        "cases/03_srp_rarefied_aero/03b_earth_umbra_on/results/"
        "earth_umbra_on_solution.csv",
        11,
    ),
    "03c_shadow_off": (
        "cases/03_srp_rarefied_aero/03c_component_shadow_off/results/"
        "component_shadow_off_solution.csv",
        11,
    ),
    "03c_shadow_on": (
        "cases/03_srp_rarefied_aero/03c_component_shadow_on/results/"
        "component_shadow_on_solution.csv",
        11,
    ),
    "03d_rarefied_fallback": (
        "cases/03_srp_rarefied_aero/03d_rarefied_fallback/results/"
        "rarefied_fallback_solution.csv",
        101,
    ),
    "03e_aero_database": (
        "cases/03_srp_rarefied_aero/03e_aero_database/results/"
        "aero_database_solution.csv",
        101,
    ),
    "03f_articulated_aero_database": (
        "cases/03_srp_rarefied_aero/03f_articulated_aero_database/results/"
        "articulated_aero_database_solution.csv",
        2,
    ),
}


def analyze_case_2() -> dict:
    rows = {
        name: read_rows(path)
        for name, (path, _) in CASE_FILES.items()
        if name.startswith("02")
    }

    revolute = rows["02a_revolute"]
    prismatic = rows["02b_prismatic"]
    stop = rows["02c_coordinate_stop"]
    nested = rows["02d_nested_tree"]
    wheel = rows["02e_reaction_wheel"]
    commanded = rows["02f_commanded_thrust"]
    direct = rows["02g_direct_torque"]
    prescribed = rows["02h_prescribed_thrust"]

    nested_initial_h = vector(
        nested[0],
        "angular_momentum_about_cm_icrf",
        "_kgm2ps",
    )
    nested_h_drift = max(
        norm([
            current - initial
            for current, initial in zip(
                vector(
                    row,
                    "angular_momentum_about_cm_icrf",
                    "_kgm2ps",
                ),
                nested_initial_h,
            )
        ])
        for row in nested
    )
    wheel_h = maximum_vector_norm(
        wheel,
        "angular_momentum_about_cm_icrf",
        "_kgm2ps",
    )

    firing = [
        norm(vector(row, "force_thrust_icrf", "_n"))
        for row in commanded
        if norm(vector(row, "force_thrust_icrf", "_n")) > 0.0
    ]
    commanded_propellant_loss = (
        value(commanded[0], "variable_component_mass_0_kg")
        - value(commanded[-1], "variable_component_mass_0_kg")
    )
    # Independent planar open-system reference for the co-located bus/tank.
    # I = I0 - b*t, b = 0.2*q; the nozzle is one metre from the centroid.
    # I*omega_dot + (q*ell^2-b)*omega = tau. Therefore, for omega(0)=0,
    # omega(t) = tau/a * [1-(I(t)/I0)^(a/b)], with a=q*ell^2-b.
    # This includes both the complete inertia rate and nozzle carrier flux.
    commanded_torque_z_nm = -40.0
    commanded_burn_duration_s = 10.0
    commanded_initial_inertia_z_kgm2 = 1002.0
    discharge = 40.0 / (250.0 * G0)
    inertia_depletion_coefficient = 0.2 * discharge
    commanded_inertia_rate_z_kgm2ps = -inertia_depletion_coefficient
    carrier_minus_depletion = discharge - inertia_depletion_coefficient
    commanded_final_inertia_z_kgm2 = (
        commanded_initial_inertia_z_kgm2
        - inertia_depletion_coefficient * commanded_burn_duration_s
    )
    commanded_expected_omega_z_radps = (
        -commanded_torque_z_nm / carrier_minus_depletion
        * math.expm1(carrier_minus_depletion / inertia_depletion_coefficient
            * math.log1p(-inertia_depletion_coefficient
                * commanded_burn_duration_s / commanded_initial_inertia_z_kgm2))
    )
    commanded_expected_h_z_kgm2ps = (
        commanded_final_inertia_z_kgm2 * commanded_expected_omega_z_radps
    )
    prescribed_propellant_loss = (
        value(prescribed[0], "variable_component_mass_0_kg")
        - value(prescribed[-1], "variable_component_mass_0_kg")
    )
    direct_angle = 2.0 * math.atan2(
        value(direct[-1], "quaternion_body_to_icrf_x"),
        value(direct[-1], "quaternion_body_to_icrf_w"),
    )

    return {
        "02a_revolute_internal_reaction": {
            "checks": [
                numeric_check(
                    "final joint coordinate",
                    value(revolute[-1], "articulation_coordinate_0_rad_or_m"),
                    0.005,
                    1.0e-10,
                    "rad",
                ),
                numeric_check(
                    "final joint rate",
                    value(revolute[-1], "articulation_rate_0_radps_or_mps"),
                    0.1,
                    1.0e-10,
                    "rad/s",
                ),
                numeric_check(
                    "final bus angular velocity z",
                    value(revolute[-1], "angular_velocity_body_z_radps"),
                    -0.02,
                    1.0e-10,
                    "rad/s",
                ),
                numeric_check(
                    "maximum total angular momentum",
                    maximum_vector_norm(
                        revolute,
                        "angular_momentum_about_cm_icrf",
                        "_kgm2ps",
                    ),
                    0.0,
                    1.0e-12,
                    "kg m^2/s",
                ),
            ],
        },
        "02b_prismatic_internal_reaction": {
            "checks": [
                numeric_check(
                    "final slider coordinate",
                    value(prismatic[-1], "articulation_coordinate_0_rad_or_m"),
                    0.01,
                    1.0e-10,
                    "m",
                ),
                numeric_check(
                    "final slider rate",
                    value(prismatic[-1], "articulation_rate_0_radps_or_mps"),
                    0.2,
                    1.0e-10,
                    "m/s",
                ),
                numeric_check(
                    "base-origin acceleration x",
                    value(prismatic[-1], "base_origin_acceleration_body_x_mps2"),
                    -0.4,
                    1.0e-12,
                    "m/s^2",
                ),
                numeric_check(
                    "maximum total linear momentum",
                    maximum_vector_norm(
                        prismatic,
                        "linear_momentum_icrf",
                        "_kgmps",
                    ),
                    0.0,
                    1.0e-12,
                    "kg m/s",
                ),
            ],
        },
        "02c_coordinate_stop": {
            "checks": [
                numeric_check(
                    "maximum coordinate error at upper stop",
                    max(
                        abs(
                            value(row, "articulation_coordinate_0_rad_or_m")
                            - math.pi
                        )
                        for row in stop
                    ),
                    0.0,
                    1.0e-12,
                    "rad",
                ),
                numeric_check(
                    "maximum joint-rate magnitude",
                    max(
                        abs(value(row, "articulation_rate_0_radps_or_mps"))
                        for row in stop
                    ),
                    0.0,
                    1.0e-12,
                    "rad/s",
                ),
                numeric_check(
                    "constraint reaction effort",
                    value(stop[-1], "joint_constraint_effort_0_nm_or_n"),
                    -0.4,
                    1.0e-12,
                    "N m",
                ),
            ],
        },
        "02d_nested_mixed_joint_tree": {
            "checks": [
                numeric_check(
                    "maximum linear-momentum norm",
                    maximum_vector_norm(
                        nested,
                        "linear_momentum_icrf",
                        "_kgmps",
                    ),
                    norm(vector(nested[0], "linear_momentum_icrf", "_kgmps")),
                    1.0e-10,
                    "kg m/s",
                ),
                numeric_check(
                    "maximum angular-momentum vector drift",
                    nested_h_drift,
                    0.0,
                    2.0e-7,
                    "kg m^2/s",
                ),
            ],
        },
        "02e_reaction_wheel_exchange_and_saturation": {
            "checks": [
                numeric_check(
                    "final wheel momentum",
                    value(wheel[-1], "reaction_wheel_momentum_0_nms"),
                    0.2,
                    1.0e-12,
                    "N m s",
                ),
                numeric_check(
                    "final bus angular velocity z",
                    value(wheel[-1], "angular_velocity_body_z_radps"),
                    -0.05,
                    1.0e-10,
                    "rad/s",
                ),
                numeric_check(
                    "maximum total angular-momentum magnitude",
                    wheel_h,
                    0.0,
                    1.0e-10,
                    "N m s",
                ),
            ],
        },
        "02f_commanded_variable_mass_thrust": {
            "checks": [
                range_check(
                    "thrust magnitude while firing",
                    firing,
                    40.0,
                    1.0e-10,
                    "N",
                ),
                numeric_check(
                    "propellant loss",
                    commanded_propellant_loss,
                    400.0 / (250.0 * G0),
                    1.0e-10,
                    "kg",
                ),
                numeric_check(
                    "final inertial angular momentum z with inertia rate and nozzle carrier flux",
                    value(
                        commanded[-1],
                        "angular_momentum_about_cm_icrf_z_kgm2ps",
                    ),
                    commanded_expected_h_z_kgm2ps,
                    1.0e-8,
                    "kg m^2/s",
                ),
            ],
            "implied_total_thrust_impulse_from_propellant_loss_Ns":
                commanded_propellant_loss * 250.0 * G0,
            "angular_momentum_reference": {
                "effective_thrust_convention": "Current fixed backend convention; relative exhaust momentum is included in thrust",
                "equation": "I(t)*omega_dot + (I_dot + q*ell^2)*omega = tau; H_dot = tau - q*ell^2*omega",
                "nozzle_radius_m": 1.0,
                "discharge_rate_kgps": 40.0 / (250.0 * G0),
                "initial_inertia_z_kgm2": commanded_initial_inertia_z_kgm2,
                "inertia_rate_z_kgm2ps": commanded_inertia_rate_z_kgm2ps,
                "burn_duration_s": commanded_burn_duration_s,
                "expected_final_angular_velocity_z_radps":
                    commanded_expected_omega_z_radps,
                "expected_final_angular_momentum_z_kgm2ps":
                    commanded_expected_h_z_kgm2ps,
            },
        },
        "02g_direct_external_torque": {
            "checks": [
                numeric_check(
                    "final angular velocity x",
                    value(direct[-1], "angular_velocity_body_x_radps"),
                    0.5,
                    1.0e-11,
                    "rad/s",
                ),
                numeric_check(
                    "final angular momentum x",
                    value(
                        direct[-1],
                        "angular_momentum_about_cm_icrf_x_kgm2ps",
                    ),
                    1.0,
                    1.0e-11,
                    "kg m^2/s",
                ),
                numeric_check(
                    "final rotation about x",
                    direct_angle,
                    0.75,
                    1.0e-10,
                    "rad",
                ),
            ],
        },
        "02h_prescribed_thrust_profile": {
            "checks": [
                numeric_check(
                    "sampled peak thrust",
                    max(
                        norm(vector(row, "force_thrust_icrf", "_n"))
                        for row in prescribed
                    ),
                    10.0,
                    1.0e-10,
                    "N",
                ),
                numeric_check(
                    "final tank mass",
                    value(prescribed[-1], "variable_component_mass_0_kg"),
                    1.0 - 10.0 / (200.0 * G0),
                    1.0e-10,
                    "kg",
                ),
                numeric_check(
                    "profile impulse inferred from propellant loss",
                    prescribed_propellant_loss * 200.0 * G0,
                    10.0,
                    1.0e-10,
                    "N s",
                ),
            ],
        },
    }


def analyze_case_3() -> dict:
    srp = read_rows(CASE_FILES["03a_srp_one_au"][0])
    umbra_off = read_rows(CASE_FILES["03b_umbra_off"][0])
    umbra_on = read_rows(CASE_FILES["03b_umbra_on"][0])
    shadow_off = read_rows(CASE_FILES["03c_shadow_off"][0])
    shadow_on = read_rows(CASE_FILES["03c_shadow_on"][0])
    fallback = read_rows(CASE_FILES["03d_rarefied_fallback"][0])
    database = read_rows(CASE_FILES["03e_aero_database"][0])
    articulated_database = read_rows(
        CASE_FILES["03f_articulated_aero_database"][0]
    )
    articulated_database_rows = read_numeric_rows(
        "cases/03_srp_rarefied_aero/03f_articulated_aero_database/"
        "data/aerodynamics.csv"
    )

    sun_distance = norm([
        value(srp[0], f"position_icrf_{axis}_m")
        - value(srp[0], f"body_sun_position_icrf_{axis}_m")
        for axis in "xyz"
    ])
    earth_distance = norm([
        value(umbra_on[0], f"position_icrf_{axis}_m")
        - value(umbra_on[0], f"body_earth_position_icrf_{axis}_m")
        for axis in "xyz"
    ])
    off_force = norm(vector(shadow_off[0], "force_srp_icrf", "_n"))
    on_force = norm(vector(shadow_on[0], "force_srp_icrf", "_n"))
    articulated_initial = articulated_database[0]
    articulated_eta = [
        value(
            articulated_initial,
            f"articulation_coordinate_{index}_rad_or_m",
        )
        for index in range(3)
    ]
    expected_articulated_eta = [0.2, -0.3, 0.4]
    expected_articulated_coefficients = articulated_aero_expected_coefficients(
        articulated_database_rows,
        articulated_eta,
    )
    articulated_coefficient_columns = [
        "aerodynamic_force_coefficient_body_x",
        "aerodynamic_force_coefficient_body_y",
        "aerodynamic_force_coefficient_body_z",
        "aerodynamic_moment_coefficient_about_cm_body_x",
        "aerodynamic_moment_coefficient_about_cm_body_y",
        "aerodynamic_moment_coefficient_about_cm_body_z",
    ]

    return {
        "03a_absorbing_plate_at_one_au": {
            "checks": [
                numeric_check(
                    "Sun-spacecraft distance",
                    sun_distance,
                    149_597_870_700.0,
                    1.0e-3,
                    "m",
                ),
                numeric_check(
                    "absorbing 2 m^2 plate SRP force",
                    norm(vector(srp[0], "force_srp_icrf", "_n")),
                    9.0782e-6,
                    1.0e-15,
                    "N",
                ),
                numeric_check(
                    "visible Sun fraction",
                    value(srp[0], "visible_sun_fraction"),
                    1.0,
                    1.0e-12,
                ),
            ],
        },
        "03b_earth_umbra_on_off": {
            "checks": [
                numeric_check(
                    "spacecraft-Earth center distance",
                    earth_distance,
                    42_164_000.0,
                    1.0e-3,
                    "m",
                ),
                numeric_check(
                    "maximum SRP force in full umbra",
                    maximum_vector_norm(
                        umbra_on,
                        "force_srp_icrf",
                        "_n",
                    ),
                    0.0,
                    1.0e-15,
                    "N",
                ),
                numeric_check(
                    "off-run visible Sun fraction",
                    value(umbra_off[0], "visible_sun_fraction"),
                    1.0,
                    1.0e-12,
                ),
                numeric_check(
                    "on-run visible Sun fraction",
                    value(umbra_on[0], "visible_sun_fraction"),
                    0.0,
                    1.0e-12,
                ),
            ],
        },
        "03c_component_shadow_on_off": {
            "checks": [
                numeric_check(
                    "unshadowed SRP force",
                    off_force,
                    9.0782e-6,
                    1.0e-15,
                    "N",
                ),
                numeric_check(
                    "shadowed SRP force",
                    on_force,
                    6.0521333333333334e-6,
                    1.0e-15,
                    "N",
                ),
                numeric_check(
                    "shadowed/unshadowed force ratio",
                    on_force / off_force,
                    2.0 / 3.0,
                    1.0e-12,
                ),
            ],
        },
        "03d_rarefied_constant_drag_fallback": {
            "checks": [
                numeric_check(
                    "initial dynamic pressure",
                    value(fallback[0], "aerodynamic_dynamic_pressure_pa"),
                    0.032,
                    1.0e-9,
                    "Pa",
                ),
                numeric_check(
                    "initial molecular speed ratio",
                    value(
                        fallback[0],
                        "aerodynamic_molecular_speed_ratio",
                    ),
                    10.547607649432795,
                    1.0e-8,
                ),
                numeric_check(
                    "initial drag-force magnitude",
                    norm(
                        vector(
                            fallback[0],
                            "force_aerodynamic_icrf",
                            "_n",
                        )
                    ),
                    0.1408,
                    1.0e-9,
                    "N",
                ),
                numeric_check(
                    "fallback-used flag",
                    value(fallback[0], "aerodynamic_fallback_used"),
                    1.0,
                    0.0,
                ),
            ],
        },
        "03e_aerodynamic_database_and_moment_transport": {
            "checks": [
                numeric_check(
                    "initial dynamic pressure",
                    value(database[0], "aerodynamic_dynamic_pressure_pa"),
                    0.032,
                    1.0e-9,
                    "Pa",
                ),
                numeric_check(
                    "initial aerodynamic force",
                    norm(
                        vector(
                            database[0],
                            "force_aerodynamic_icrf",
                            "_n",
                        )
                    ),
                    0.064,
                    1.0e-9,
                    "N",
                ),
                numeric_check(
                    "moment coefficient about CM z",
                    value(
                        database[0],
                        "aerodynamic_moment_coefficient_about_cm_body_z",
                    ),
                    5.0 / 6.0,
                    1.0e-12,
                ),
                numeric_check(
                    "initial aerodynamic torque z",
                    value(database[0], "torque_aerodynamic_body_z_nm"),
                    0.16,
                    1.0e-9,
                    "N m",
                ),
                numeric_check(
                    "database-used flag",
                    value(database[0], "aerodynamic_database_used"),
                    1.0,
                    0.0,
                ),
            ],
        },
        "03f_three_dof_aerodynamic_database_interpolation": {
            "checks": [
                numeric_check(
                    "maximum initial articulation-coordinate error",
                    max(
                        abs(actual - expected)
                        for actual, expected in zip(
                            articulated_eta,
                            expected_articulated_eta,
                        )
                    ),
                    0.0,
                    1.0e-14,
                    "rad",
                ),
                numeric_check(
                    "initial dynamic pressure",
                    value(
                        articulated_initial,
                        "aerodynamic_dynamic_pressure_pa",
                    ),
                    0.032,
                    1.0e-9,
                    "Pa",
                ),
                numeric_check(
                    "initial molecular speed ratio",
                    value(
                        articulated_initial,
                        "aerodynamic_molecular_speed_ratio",
                    ),
                    10.547607654346578,
                    1.0e-8,
                ),
                numeric_check(
                    "initial Knudsen number",
                    value(
                        articulated_initial,
                        "aerodynamic_knudsen_number",
                    ),
                    11.313708498984751,
                    1.0e-8,
                ),
                *[
                    numeric_check(
                        column,
                        value(articulated_initial, column),
                        expected,
                        1.0e-12,
                    )
                    for column, expected in zip(
                        articulated_coefficient_columns,
                        expected_articulated_coefficients,
                    )
                ],
                numeric_check(
                    "database-used flag",
                    value(articulated_initial, "aerodynamic_database_used"),
                    1.0,
                    0.0,
                ),
                numeric_check(
                    "fallback-used flag",
                    value(articulated_initial, "aerodynamic_fallback_used"),
                    0.0,
                    0.0,
                ),
                numeric_check(
                    "outside-validity flag",
                    value(articulated_initial, "aerodynamics_outside_validity"),
                    0.0,
                    0.0,
                ),
            ],
        },
    }


def test_metrics() -> dict:
    backend_log = (
        ROOT
        / "cases/01_backend_verification/backend_validation_log.txt"
    ).read_text(encoding="utf-8")
    roundtrip_log = (
        ROOT
        / "cases/01_backend_verification/scenario_roundtrip_log.txt"
    ).read_text(encoding="utf-8")
    passes = len(re.findall(r"^PASS \|", backend_log, re.MULTILINE))
    failures = len(re.findall(r"^FAIL \|", backend_log, re.MULTILINE))
    return {
        "backend_checks_passed": passes,
        "backend_checks_failed": failures,
        "round_trip_passed":
            "round-trip and compiler tests passed" in roundtrip_log,
    }


def log_scan() -> dict:
    logs = sorted(ROOT.glob("cases/**/*runner_log.txt"))
    patterns = {
        "kernel_variable_not_found": "SPICE(KERNELVARNOTFOUND)",
        "severe_error_block": "A traceback follows",
        "spice_error": "SPICE(",
    }
    matches = {
        name: [] for name in patterns
    }
    for path in logs:
        text = path.read_text(encoding="utf-8", errors="replace")
        for name, pattern in patterns.items():
            if pattern.lower() in text.lower():
                matches[name].append(str(path.relative_to(ROOT)))
    return {
        "runner_log_count": len(logs),
        "matches": matches,
    }


def main() -> None:
    integrity_results = {
        name: integrity(path, nominal)
        for name, (path, nominal) in CASE_FILES.items()
    }
    case_2 = analyze_case_2()
    case_3 = analyze_case_3()
    all_checks = [
        check
        for case in [*case_2.values(), *case_3.values()]
        for check in case["checks"]
    ]
    document = {
        "schema": "PHAROS validation Cases 1-3 quantitative metrics v3",
        "validation_root": str(ROOT),
        "integrity": integrity_results,
        "case_2": case_2,
        "case_3": case_3,
        "case_1": test_metrics(),
        "runner_logs": log_scan(),
        "summary": {
            "quantitative_checks_passed": sum(
                bool(check["pass"]) for check in all_checks
            ),
            "quantitative_checks_failed": sum(
                not bool(check["pass"]) for check in all_checks
            ),
            "all_nominal_row_counts": all(
                item["row_count_matches_nominal"]
                for item in integrity_results.values()
            ),
            "series_with_nonstrict_absolute_et": [
                name
                for name, item in integrity_results.items()
                if not item["strictly_increasing_absolute_et"]
            ],
            "total_nonfinite_numeric_cells": sum(
                item["nonfinite_numeric_cells"]
                for item in integrity_results.values()
            ),
            "total_multibody_solve_failures": sum(
                item["multibody_solve_failures"]
                for item in integrity_results.values()
            ),
            "maximum_quaternion_norm_error": max(
                item["maximum_quaternion_norm_error"]
                for item in integrity_results.values()
            ),
        },
        "baseline_metrics_path":
            "../../Support/Provenance/legacy_reference/05_to_07_Backend_Feature_Verification/analysis/cases_05_07_metrics.json",
    }
    output = ROOT / "analysis/cases_01_03_metrics.json"
    output.write_text(
        json.dumps(document, indent=2, allow_nan=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(output)


if __name__ == "__main__":
    main()
