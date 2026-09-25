#!/usr/bin/env python3
"""Generate, validate, and propagate the continuous JWST mission chain.

Only phase A reads an external trajectory state. Every later initial state is
copied from the immediately preceding TGSimCore solution CSV terminal row.
"""

from __future__ import annotations

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))


import csv
import math
import os
import hashlib
import json
import time
import subprocess
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RUNNER = Path(os.environ.get('PHAROS_RUNNER', _RUNTIME/'PHAROSScenarioRunner.exe'))
KERNELS = Path(os.environ.get('PHAROS_KERNELS', _KERNELS))
KEY_STATES = ROOT / "truth" / "jwst_key_states_barycentric.csv"


@dataclass(frozen=True)
class Phase:
    identifier: str
    label: str
    start_utc: str
    final_utc: str
    controller_dll: str
    deployed: bool
    maximum_step_s: float
    output_step_s: float
    purpose: str


PHASES = [
    Phase("A", "COAST_TO_MCC1A", "2021-12-25T13:00:00Z",
          "2021-12-26T00:50:00Z", "JWSTCoastAndSlewController.dll",
          False, 30.0, 300.0,
          "stowed coast, Sun pointing, then velocity-vector acquisition"),
    Phase("B", "MCC1A_BURN", "2021-12-26T00:50:00Z",
          "2021-12-26T01:54:54.728Z", "JWSTVelocityBurnController.dll",
          False, 5.0, 5.0,
          "20.033 m/s reconstructed MCC-1a finite burn"),
    Phase("C", "COAST_TO_MCC1B", "2021-12-26T01:54:54.728Z",
          "2021-12-28T00:20:00Z", "JWSTCoastAndSlewController.dll",
          False, 120.0, 900.0,
          "stowed coast, Sun pointing, then velocity-vector acquisition"),
    Phase("D", "MCC1B_BURN", "2021-12-28T00:20:00Z",
          "2021-12-28T00:29:27.240Z", "JWSTVelocityBurnController.dll",
          False, 2.0, 1.0,
          "2.773 m/s reconstructed MCC-1b finite burn"),
    Phase("E", "COAST_TO_DEPLOYED_CONFIGURATION",
          "2021-12-28T00:29:27.240Z", "2022-01-04T17:00:00Z",
          "JWSTSunPointController.dll", False, 120.0, 1800.0,
          "Sun-pointing transfer while the simplified model remains stowed"),
    Phase("F", "DEPLOYED_COAST_TO_MCC2", "2022-01-04T17:00:00Z",
          "2022-01-24T19:00:00Z", "JWSTCoastAndSlewController.dll",
          True, 120.0, 3600.0,
          "deployed SRP coast, Sun pointing, then MCC-2 attitude acquisition"),
    Phase("G", "MCC2_BURN", "2022-01-24T19:00:00Z",
          "2022-01-24T19:04:56.648Z", "JWSTMcc2BurnController.dll",
          True, 1.0, 1.0,
          "1.484 m/s reconstructed L2-insertion finite burn"),
    Phase("H", "POST_INSERTION_L2_COAST", "2022-01-24T19:04:56.648Z",
          "2022-02-01T00:00:00Z", "JWSTSunPointController.dll",
          True, 120.0, 1800.0,
          "Sun-pointing early L2-orbit coast"),
]


GRAVITY_BODIES = [
    ("Sun", True),
    ("MercuryBarycenter", True), ("Mercury", False),
    ("VenusBarycenter", True), ("Venus", False),
    ("EarthMoonBarycenter", False), ("Earth", True), ("Moon", True),
    ("MarsBarycenter", True), ("Mars", False),
    ("JupiterBarycenter", True), ("Jupiter", False),
    ("SaturnBarycenter", True), ("Saturn", False),
    ("UranusBarycenter", True), ("Uranus", False),
    ("NeptuneBarycenter", True), ("Neptune", False),
    ("PlutoBarycenter", True), ("Pluto", False),
]


def number(value: float) -> str:
    if not math.isfinite(value):
        raise ValueError(f"non-finite TGSCN value: {value}")
    return format(value, ".17g")


def vector(values: list[float]) -> str:
    return "[" + ", ".join(number(value) for value in values) + "]"


def first_external_state() -> dict[str, object]:
    with KEY_STATES.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    row = next(item for item in rows if item["utc"].startswith(
        "2021-12-25T13:00:00"))
    return {
        "position": [1000.0 * float(row[f"position_j2000_{axis}_km"])
                     for axis in "xyz"],
        "velocity": [1000.0 * float(row[f"velocity_j2000_{axis}_kmps"])
                     for axis in "xyz"],
        # No public reconstructed attitude kernel is used in this validation.
        # Identity is an explicit engineering assumption; the controller slews
        # from it and all later phases inherit the resulting attitude.
        "quaternion": [1.0, 0.0, 0.0, 0.0],
        "omega": [0.0, 0.0, 0.0],
        "propellant_mass": 301.0,
        "wheel_momenta": [0.0, 0.0, 0.0],
    }


def terminal_state(solution_csv: Path) -> dict[str, object]:
    with solution_csv.open(newline="", encoding="utf-8") as stream:
        row = list(csv.DictReader(stream))[-1]
    return {
        "position": [float(row[f"position_icrf_{axis}_m"]) for axis in "xyz"],
        "velocity": [float(row[f"velocity_icrf_{axis}_mps"]) for axis in "xyz"],
        "quaternion": [float(row[f"quaternion_body_to_icrf_{part}"])
                       for part in ("w", "x", "y", "z")],
        "omega": [float(row[f"angular_velocity_body_{axis}_radps"])
                  for axis in "xyz"],
        "propellant_mass": float(row["variable_component_mass_0_kg"]),
        "wheel_momenta": [float(row[f"reaction_wheel_momentum_{index}_nms"])
                          for index in range(3)],
    }


def gravity_text() -> str:
    records = []
    for name, enabled in GRAVITY_BODIES:
        harmonic_file = "../../data/earth_j2.csv" if name == "Earth" else ""
        degree = 2 if name == "Earth" else 0
        records.append(f"""[[gravity.bodies]]
catalog_key = \"{name}\"
gravity_enabled = {str(enabled).lower()}
automatic_activation_radius_m = 0.0
barycenter_resolution_radius_m = 0.0
harmonic_model_csv_file = \"{harmonic_file}\"
maximum_harmonic_degree = {degree}
""")
    return "\n".join(records)


def facets_text(deployed: bool) -> str:
    # Rounded NASA overall dimensions; retain the explicit rectangular proxy.
    # This is a bounding-footprint abstraction, not the true kite outline.
    half_x = 10.6 if deployed else 2.5
    half_y = 7.1 if deployed else 2.0
    vertices = [
        ((-half_x, -half_y, 0.0), (half_x, -half_y, 0.0),
         (half_x, half_y, 0.0)),
        ((-half_x, -half_y, 0.0), (half_x, half_y, 0.0),
         (-half_x, half_y, 0.0)),
    ]
    blocks = []
    for index, triangle in enumerate(vertices):
        blocks.append(f"""[[srp.facets]]
name = \"Sunshield triangle {index}\"
component_id = \"31000000-0000-0000-0000-000000000001\"
component_name = \"Bus\"
stable_triangle_index = {index}
logical_region = \"positive_z\"
vertex0_component_m = {vector(list(triangle[0]))}
vertex1_component_m = {vector(list(triangle[1]))}
vertex2_component_m = {vector(list(triangle[2]))}
optics = {{ absorption = 0.35, specular_reflection = 0.25, diffuse_reflection = 0.40 }}
""")
    return "\n".join(blocks)


def scenario_text(phase: Phase, state: dict[str, object]) -> str:
    propellant = float(state["propellant_mass"])
    propellant_inertia = max(1.0e-6, 500.0 * propellant / 301.0)
    wheel = list(state["wheel_momenta"])
    return f"""format = \"TGSCN\"
generator = \"Continuous JWST mission validation\"

[scenario]
name = \"JWST CONTINUOUS {phase.label}\"
start_utc = \"{phase.start_utc}\"
end_mode = \"final_utc\"
final_utc = \"{phase.final_utc}\"
integrator = \"adaptive_dormand_prince_54\"
maximum_integrator_step_seconds = {number(phase.maximum_step_s)}
initial_integrator_step_seconds = {number(min(phase.maximum_step_s, 1.0))}
absolute_tolerance = 1.0e-5
relative_tolerance = 1.0e-12
output_mode = \"fixed_interval\"
output_step_seconds = {number(phase.output_step_s)}
maximum_integration_steps = 5000000
maximum_output_samples = 1000000

[initial_state]
position_icrf_m = {vector(list(state["position"]))}
velocity_icrf_mps = {vector(list(state["velocity"]))}
attitude_body_to_icrf = {vector(list(state["quaternion"]))}
angular_velocity_body_radps = {vector(list(state["omega"]))}

[[components]]
id = \"31000000-0000-0000-0000-000000000001\"
name = \"Bus\"
initial_mass_kg = 5860.4
minimum_mass_kg = 5860.4
variable_mass = false
local_center_of_mass_m = [0.0, 0.0, 0.0]
origin_body_m = [0.0, 0.0, 0.0]
component_to_body = [1.0, 0.0, 0.0, 0.0]
parent_component = \"\"
parent_anchor_m = [0.0, 0.0, 0.0]
child_anchor_m = [0.0, 0.0, 0.0]
child_to_parent_zero_orientation = [1.0, 0.0, 0.0, 0.0]
dofs = []
inertia = {{ ixx_kgm2 = 30000.0, iyy_kgm2 = 30000.0, izz_kgm2 = 30000.0, ixy_kgm2 = 0.0, ixz_kgm2 = 0.0, iyz_kgm2 = 0.0 }}

[[components]]
id = \"31000000-0000-0000-0000-000000000002\"
name = \"Propellant\"
initial_mass_kg = {number(propellant)}
minimum_mass_kg = 0.0
variable_mass = true
local_center_of_mass_m = [0.0, 0.0, 0.0]
origin_body_m = [0.0, 0.0, 0.0]
component_to_body = [1.0, 0.0, 0.0, 0.0]
parent_component = \"Bus\"
parent_anchor_m = [0.0, 0.0, 0.0]
child_anchor_m = [0.0, 0.0, 0.0]
child_to_parent_zero_orientation = [1.0, 0.0, 0.0, 0.0]
dofs = []
inertia = {{ ixx_kgm2 = {number(propellant_inertia)}, iyy_kgm2 = {number(propellant_inertia)}, izz_kgm2 = {number(propellant_inertia)}, ixy_kgm2 = 0.0, ixz_kgm2 = 0.0, iyz_kgm2 = 0.0 }}

[[thrusters]]
name = \"SCAT\"
mode = \"commanded\"
mount_component = \"Bus\"
propellant_component = \"Propellant\"
application_point_component_m = [0.0, 0.0, 0.0]
direction_component = [1.0, 0.0, 0.0]
ignition_time_mode = \"elapsed\"
ignition_elapsed_seconds = 0.0
never_shuts_down = true
shutdown_time_mode = \"elapsed\"
shutdown_elapsed_seconds = 0.0
maximum_thrust_n = 32.0

[[reaction_wheels]]
name = \"RW_X\"
mount_component = \"Bus\"
axis_component = [1.0, 0.0, 0.0]
initial_momentum_nms = {number(wheel[0])}
maximum_absolute_momentum_nms = 2000.0

[[reaction_wheels]]
name = \"RW_Y\"
mount_component = \"Bus\"
axis_component = [0.0, 1.0, 0.0]
initial_momentum_nms = {number(wheel[1])}
maximum_absolute_momentum_nms = 2000.0

[[reaction_wheels]]
name = \"RW_Z\"
mount_component = \"Bus\"
axis_component = [0.0, 0.0, 1.0]
initial_momentum_nms = {number(wheel[2])}
maximum_absolute_momentum_nms = 2000.0

[control]
mode = \"compiled_user_controller\"
unreal_controller_id = \"\"
controller_dll_file = \"../../controllers/{phase.controller_dll}\"

[gravity]
include_first_post_newtonian_correction = true

{gravity_text()}
[srp]
enabled = true
sun_body = \"Sun\"
pressure_at_one_au_pa = 4.5391e-6
compute_eclipse = true
occulting_bodies = [\"Earth\", \"Moon\"]
compute_component_shadows = false
global_fallback_optics = {{ absorption = 0.35, specular_reflection = 0.25, diffuse_reflection = 0.40 }}

{facets_text(phase.deployed)}
[atmosphere]
enabled = false

[aerodynamics]
enabled = false
"""


def run_process(arguments: list[str], log_path: Path) -> None:
    result = subprocess.run(
        arguments, cwd=ROOT, text=True, capture_output=True, check=False)
    log_path.write_text(
        result.stdout + ("\n[stderr]\n" + result.stderr if result.stderr else ""),
        encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed with exit code {result.returncode}; see {log_path}")


def main() -> None:
    for required in [RUNNER, KERNELS, KEY_STATES]:
        if not required.exists():
            raise FileNotFoundError(required)
    for phase in PHASES:
        dll = ROOT / "controllers" / phase.controller_dll
        if not dll.exists():
            raise FileNotFoundError(dll)

    start_identifier = os.environ.get("TG_JWST_START_PHASE", "A").upper()
    end_identifier = os.environ.get("TG_JWST_END_PHASE", "H").upper()
    identifiers = [phase.identifier for phase in PHASES]
    if start_identifier not in identifiers or end_identifier not in identifiers:
        raise ValueError("TG_JWST_START_PHASE/TG_JWST_END_PHASE must be A through H")
    start_index = identifiers.index(start_identifier)
    end_index = identifiers.index(end_identifier)
    if start_index > end_index:
        raise ValueError("JWST start phase follows end phase")

    if start_index == 0:
        state = first_external_state()
        previous_solution: Path | None = None
    else:
        predecessor = PHASES[start_index - 1]
        predecessor_dir = ROOT / "phases" / (
            f"{predecessor.identifier}_{predecessor.label.lower()}")
        previous_solution = predecessor_dir / "results" / (
            f"jwst_{predecessor.identifier.lower()}_"
            f"{predecessor.label.lower()}_solution.csv")
        if not previous_solution.exists():
            raise FileNotFoundError(previous_solution)
        state = terminal_state(previous_solution)
    manifest = ROOT / "analysis" / "phase_manifest.csv"
    manifest_rows = []
    if start_index > 0:
        with manifest.open(newline='',encoding='utf-8') as stream:
            prior=list(csv.DictReader(stream))
        manifest_rows=[r for r in prior if identifiers.index(r['phase']) < start_index]

    for phase in PHASES[start_index:end_index + 1]:
        phase_dir = ROOT / "phases" / f"{phase.identifier}_{phase.label.lower()}"
        results_dir = phase_dir / "results"
        phase_dir.mkdir(parents=True, exist_ok=True)
        results_dir.mkdir(parents=True, exist_ok=True)
        scenario = phase_dir / f"jwst_{phase.identifier.lower()}_{phase.label.lower()}.tgscn"
        scenario.write_text(scenario_text(phase, state), encoding="utf-8")
        started=time.monotonic()
        print('Running phase '+phase.identifier+' '+phase.label,flush=True)

        base_command = [str(RUNNER), str(scenario), "--kernel-dir", str(KERNELS)]
        run_process(base_command + ["--validate-only"],
                    results_dir / "validate_log.txt")
        run_process(base_command + ["--output", str(results_dir)],
                    results_dir / "runner_log.txt")

        solution = results_dir / f"{scenario.stem}_solution.csv"
        summary = results_dir / f"{scenario.stem}_summary.txt"
        if not solution.exists() or "success=true" not in summary.read_text(
                encoding="utf-8"):
            raise RuntimeError(f"phase {phase.identifier} did not produce success")

        manifest_rows.append({
            "phase": phase.identifier,
            "scenario_sha256": hashlib.sha256(scenario.read_bytes()).hexdigest(),
            "controller_sha256": hashlib.sha256((ROOT/'controllers'/phase.controller_dll).read_bytes()).hexdigest(),
            "runner_sha256": hashlib.sha256(RUNNER.read_bytes()).hexdigest(),
            "wall_seconds": time.monotonic()-started,
            "label": phase.label,
            "start_utc": phase.start_utc,
            "final_utc": phase.final_utc,
            "controller_dll": phase.controller_dll,
            "configuration": "deployed" if phase.deployed else "stowed",
            "initial_state_source": (
                "NAIF jwst_rec.bsp state at 2021-12-25T13:00:00Z"
                if previous_solution is None else str(previous_solution.relative_to(ROOT))),
            "scenario": str(scenario.relative_to(ROOT)),
            "solution": str(solution.relative_to(ROOT)),
            "purpose": phase.purpose,
        })

        print('Completed phase '+phase.identifier+' in '+format(time.monotonic()-started,'.2f')+' s',flush=True)
        state = terminal_state(solution)
        previous_solution = solution

    # Downstream phases must be repropagated after changing upstream inputs.
    with manifest.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(manifest_rows[0]))
        writer.writeheader()
        writer.writerows(manifest_rows)
    print(
        f"Continuous JWST chain complete: {len(manifest_rows)} phases "
        f"({start_identifier} through {end_identifier})")


if __name__ == "__main__":
    main()
