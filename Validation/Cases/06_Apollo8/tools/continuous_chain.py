from __future__ import annotations

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))


import argparse
import hashlib
import time
import tomllib
import csv
import json
import math
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VALIDATION_ROOT = _VALIDATION
WORKSPACE_ROOT = VALIDATION_ROOT.parent
RUNNER = Path(os.environ.get("PHAROS_RUNNER", _RUNTIME/"PHAROSScenarioRunner.exe"))
KERNELS = Path(os.environ.get("PHAROS_KERNELS", _KERNELS))
TRUTH_CSV = ROOT / "expected/apollo8_table5ii_truth_states.csv"
SDK = _RUNTIME/"ControllerSDK"
VCVARS = Path(os.environ.get("PHAROS_VCVARS", r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"))
STEP_FACTOR = float(os.environ.get("APOLLO_STEP_FACTOR", "1"))
RANGE_ZERO = datetime(1968, 12, 21, 12, 51, 0, tzinfo=timezone.utc)
G0 = 9.80665
POUND_TO_KG = 0.45359237
FOOT_TO_METER = 0.3048
BUS_MASS_KG = 13000.0
BUS_DIAGONAL = (22215.3931855140, 70663.242657240, 72245.988780388)
RCS_ISP_SECONDS = 289.0
SPS_ISP_SECONDS = 313.0
RCS_CLUSTER_MAXIMUM_THRUST_N = 1780.0
MEASURED_SPS_THRUST_N = 20498.160967192176 * 4.4482216152605
BURN_DISPLAY_NAMES = {
    "first_midcourse": "First midcourse correction",
    "second_midcourse": "Second midcourse correction",
    "lunar_orbit_insertion": "Lunar orbit insertion",
    "lunar_orbit_circularization": "Lunar orbit circularization",
    "transearth_injection": "Transearth injection",
    "third_midcourse": "Third midcourse correction",
}
BURN_CONTROLLER_STEMS = {
    "first_midcourse": "FirstMidcourse",
    "second_midcourse": "SecondMidcourse",
    "lunar_orbit_insertion": "LunarOrbitInsertion",
    "lunar_orbit_circularization": "LunarOrbitCircularization",
    "transearth_injection": "TransearthInjection",
    "third_midcourse": "ThirdMidcourse",
}


@dataclass
class State:
    position: tuple[float, float, float]
    velocity: tuple[float, float, float]
    quaternion: tuple[float, float, float, float]
    angular_velocity: tuple[float, float, float]
    mass: float
    inertia: tuple[float, float, float, float, float, float]


BURNS = {
    "first_midcourse": {
        "pre": "first_midcourse_ignition",
        "post": "first_midcourse_cutoff",
        "actual_dv_fps": 24.8,
        "duration": 2.4,
        "use_sps": True,
        "target_mass_lb": 63307.0,
        "sps_maximum_thrust_n": MEASURED_SPS_THRUST_N,
        "ullage_dv_fps": 0.0,
    },
    "second_midcourse": {
        "pre": "second_midcourse_ignition",
        "post": "second_midcourse_cutoff",
        "actual_dv_fps": 2.175752743305176,
        "duration": 11.8,
        "use_sps": False,
        "target_mass_lb": 62845.0,
        "ullage_dv_fps": 0.0,
    },
    "lunar_orbit_insertion": {
        "pre": "lunar_orbit_insertion_ignition",
        "post": "lunar_orbit_insertion_cutoff",
        "actual_dv_fps": 2976.05,
        "duration": 246.9,
        "use_sps": True,
        "target_mass_lb": 62827.0,
        "sps_maximum_thrust_n": MEASURED_SPS_THRUST_N,
        "ullage_dv_fps": 0.0,
        "target_mode": "retrograde_tangential_moon",
        "radial_bias": 0.020,
    },
    "lunar_orbit_circularization": {
        "pre": "lunar_orbit_circularization_ignition",
        "post": "lunar_orbit_circularization_cutoff",
        "actual_dv_fps": 134.12565004502306,
        "duration": 9.6,
        "use_sps": True,
        "target_mass_lb": 46716.0,
        "sps_maximum_thrust_n": MEASURED_SPS_THRUST_N,
        "ullage_dv_fps": 0.0,
        "target_mode": "circularize_moon",
        "circularization_stop_mps": 0.25,
    },
    "transearth_injection": {
        "pre": "transearth_injection_ignition",
        "post": "transearth_injection_cutoff",
        "actual_dv_fps": 3520.2119749651115,
        "duration": 203.7,
        "use_sps": True,
        "target_mass_lb": 45931.0,
        "ullage_dv_fps": 3.5,
        "target_mode": "fixed_icrf",
    },
    "third_midcourse": {
        "pre": "third_midcourse_ignition",
        "post": "third_midcourse_cutoff",
        "actual_dv_fps": 4.850164945648756,
        "duration": 15.0,
        "use_sps": False,
        "target_mass_lb": 32008.0,
        "ullage_dv_fps": 0.0,
        "target_mode": "fixed_icrf",
    },
}


TUNING_FILE = ROOT / 'expected/controller_tuning.json'
if TUNING_FILE.exists():
    for _burn_name, _overrides in json.loads(TUNING_FILE.read_text(encoding='utf-8'))['burn_overrides'].items():
        BURNS[_burn_name].update(_overrides)

SEGMENTS = {
    "02": {
        "slug": "02_first_midcourse_controlled_maneuver",
        "kind": "burn",
        "burn": "first_midcourse",
        "start_get": 39599.5,
        "burn_start": 60.0,
        "end_get": 39661.9,
        "previous": ROOT / "segments" / "01_post_separation_to_first_midcourse" / "results" /
            "apollo8_like_01_post_separation_to_first_midcourse_solution.csv",
        "events": ["first_midcourse_ignition"],
    },
    "03": {
        "slug": "03_translunar_coast_to_second_midcourse_attitude_start",
        "kind": "coast",
        "start_get": 39661.9,
        "end_get": 219535.0,
        "previous_segment": "02",
        "events": [],
    },
    "04": {
        "slug": "04_second_midcourse_controlled_maneuver",
        "kind": "burn",
        "burn": "second_midcourse",
        "start_get": 219535.0,
        "burn_start": 60.9,
        "burn_end": 72.7,
        "end_get": 219607.8,
        "previous_segment": "03",
        "events": ["second_midcourse_ignition", "second_midcourse_cutoff"],
    },
    "05": {
        "slug": "05_lunar_approach_to_lunar_orbit_insertion_attitude_start",
        "kind": "coast",
        "start_get": 219607.8,
        "end_get": 248840.4,
        "previous_segment": "04",
        "events": [],
    },
    "06": {
        "slug": "06_lunar_orbit_insertion_controlled_maneuver",
        "kind": "burn",
        "burn": "lunar_orbit_insertion",
        "start_get": 248840.4,
        "burn_start": 60.0,
        "end_get": 249147.3,
        "previous_segment": "05",
        "events": ["lunar_orbit_insertion_ignition", "lunar_orbit_insertion_cutoff"],
    },
    "07": {
        "slug": "07_lunar_orbit_to_lunar_orbit_circularization_attitude_start",
        "kind": "coast",
        "start_get": 249147.3,
        "end_get": 264846.6,
        "previous_segment": "06",
        "events": [],
    },
    "08": {
        "slug": "08_lunar_orbit_circularization_controlled_maneuver",
        "kind": "burn",
        "burn": "lunar_orbit_circularization",
        "start_get": 264846.6,
        "burn_start": 60.0,
        "burn_end": 70.42,
        "end_get": 264917.02,
        "previous_segment": "07",
        "events": ["lunar_orbit_circularization_ignition", "lunar_orbit_circularization_cutoff"],
    },
    "09": {
        "slug": "09_lunar_orbit_to_transearth_injection_attitude_start",
        "kind": "coast",
        "start_get": 264917.02,
        "end_get": 321496.6,
        "previous_segment": "08",
        "events": [],
    },
    "10": {
        "slug": "10_transearth_injection_controlled_maneuver",
        "kind": "burn",
        "burn": "transearth_injection",
        "start_get": 321496.6,
        "burn_start": 60.0,
        "ullage_start": 45.0,
        "ullage_end": 60.0,
        "end_get": 321760.3,
        "previous_segment": "09",
        "events": ["transearth_injection_ignition", "transearth_injection_cutoff"],
    },
    "11": {
        "slug": "11_transearth_coast_to_third_midcourse_attitude_start",
        "kind": "coast",
        "start_get": 321760.3,
        "end_get": 374334.0,
        "previous_segment": "10",
        "events": [],
    },
    "12": {
        "slug": "12_third_midcourse_controlled_maneuver",
        "kind": "burn",
        "burn": "third_midcourse",
        "start_get": 374334.0,
        "burn_start": 66.0,
        "end_get": 374415.0,
        "previous_segment": "11",
        "events": ["third_midcourse_ignition", "third_midcourse_cutoff"],
    },
    "13": {
        "slug": "13_transearth_coast_to_csm_separation",
        "kind": "mass_trim_coast",
        "start_get": 374415.0,
        "end_get": 527328.0,
        "previous_segment": "12",
        "target_final_mass_lb": 31768.0,
        "events": [],
    },
}


# Compact paths keep this portable on Windows. Labels retain mission terminology.
for sid, spec in SEGMENTS.items():
    spec['label'] = spec['slug'][3:].replace('_', ' ')
    spec['slug'] = sid
SEGMENTS['02'].pop('previous', None)
SEGMENTS['02']['previous_segment'] = '01'
SEGMENTS['01'] = {'slug':'01', 'label':'Post-separation coast', 'kind':'initial',
                  'start_get':17154.0, 'end_get':39599.5, 'events':[]}
# Table 6.9-II gives the detailed actual maneuver timing. Table 5-II
# epochs remain separate, rounded trajectory comparison marks.
SEGMENTS['01']['end_get'] = 39539.2
SEGMENTS['02']['start_get'] = 39539.2
SEGMENTS['02']['end_get'] = 39601.9
SEGMENTS['03']['start_get'] = 39601.9
SEGMENTS['04']['burn_start'] = 60.9
SEGMENTS['04']['burn_end'] = 72.7
SEGMENTS['08']['burn_end'] = 69.6
SEGMENTS['12']['burn_start'] = 66.0
BURNS['third_midcourse']['duration'] = 15.0
SEGMENTS['12']['end_get'] = 374415.0
SEGMENTS['13']['start_get'] = 374415.0
SEGMENTS = dict(sorted(SEGMENTS.items()))

def file_hash(path):
    digest=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1048576), b''):digest.update(block)
    return digest.hexdigest()

def run_initial():
    initial=tomllib.loads((ROOT/'provenance/templates/01.tgscn').read_text(encoding='utf-8'))
    raw=initial['initial_state'];body=initial['components'][0];tensor=body['inertia']
    state=State(tuple(raw['position_icrf_m']),tuple(raw['velocity_icrf_mps']),
                tuple(raw['attitude_body_to_icrf']),tuple(raw['angular_velocity_body_radps']),
                body['initial_mass_kg'],tuple(tensor[k] for k in ['ixx_kgm2','iyy_kgm2','izz_kgm2','ixy_kgm2','ixz_kgm2','iyz_kgm2']))
    directory=ROOT/'segments/01';scenario=directory/'scenario/apollo8_like_01.tgscn'
    write_text(scenario,scenario_text('Apollo 8 post-separation coast',17154.,39539.2,state,'[control]\nmode = "none"\n'))
    solution=run_scenario(scenario,directory/'results',directory/'logs')
    write_text(directory/'metadata.json',json.dumps({'segment_id':'01','slug':'01','start_get_seconds':17154.,'end_get_seconds':39539.2,'start_utc':iso_utc(17154.),'end_utc':iso_utc(39539.2),'external_initial_state':True,'solution':str(solution.relative_to(ROOT)),'scenario_sha256':file_hash(scenario),'runner_sha256':file_hash(RUNNER)},indent=2))
    print('Completed Apollo segment 01 (single external initial state)',flush=True)


def vector_norm(values):
    return math.sqrt(sum(value * value for value in values))


def iso_utc(get_seconds: float) -> str:
    value = RANGE_ZERO + timedelta(seconds=get_seconds)
    return value.strftime("%Y-%m-%dT%H:%M:%S.") + f"{value.microsecond // 1000:03d}Z"


def write_text(path: Path, text: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def read_csv_last(path: Path) -> dict[str, str]:
    last = None
    with path.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            last = row
    if last is None:
        raise RuntimeError(f"No data rows in {path}")
    return last


def state_from_solution_row(row: dict[str, str]) -> State:
    return State(
        tuple(float(row[f"position_icrf_{axis}_m"]) for axis in "xyz"),
        tuple(float(row[f"velocity_icrf_{axis}_mps"]) for axis in "xyz"),
        tuple(float(row[f"quaternion_body_to_icrf_{axis}"]) for axis in "wxyz"),
        tuple(float(row[f"angular_velocity_body_{axis}_radps"]) for axis in "xyz"),
        float(row["mass_kg"]),
        (
            float(row["inertia_body_xx_kgm2"]),
            float(row["inertia_body_yy_kgm2"]),
            float(row["inertia_body_zz_kgm2"]),
            float(row["inertia_body_xy_kgm2"]),
            float(row["inertia_body_xz_kgm2"]),
            float(row["inertia_body_yz_kgm2"]),
        ),
    )


def state_from_solution(path: Path) -> State:
    return state_from_solution_row(read_csv_last(path))


def truth_rows() -> dict[str, dict[str, str]]:
    with TRUTH_CSV.open(newline="", encoding="utf-8") as stream:
        return {row["event_id"]: row for row in csv.DictReader(stream)}


def state_from_truth(row: dict[str, str]) -> State:
    return State(
        tuple(float(row[f"position_icrf_{axis}_m"]) for axis in "xyz"),
        tuple(float(row[f"velocity_icrf_{axis}_mps"]) for axis in "xyz"),
        (1.0, 0.0, 0.0, 0.0),
        (0.0, 0.0, 0.0),
        1.0,
        (1.0, 1.0, 1.0, 0.0, 0.0, 0.0),
    )


def fmt(values) -> str:
    return "[" + ", ".join(f"{value:.17g}" for value in values) + "]"


def component_text(state: State, simple: bool = False) -> str:
    if simple:
        return """
[[components]]
id = "a8ffffff-0000-0000-0000-000000000001"
name = "Truth Coast Probe"
initial_mass_kg = 1.0
minimum_mass_kg = 1.0
variable_mass = false
local_center_of_mass_m = [0.0, 0.0, 0.0]
origin_body_m = [0.0, 0.0, 0.0]
component_to_body = [1.0, 0.0, 0.0, 0.0]
parent_component = ""
parent_anchor_m = [0.0, 0.0, 0.0]
child_anchor_m = [0.0, 0.0, 0.0]
child_to_parent_zero_orientation = [1.0, 0.0, 0.0, 0.0]
dofs = []
inertia = { ixx_kgm2 = 1.0, iyy_kgm2 = 1.0, izz_kgm2 = 1.0, ixy_kgm2 = 0.0, ixz_kgm2 = 0.0, iyz_kgm2 = 0.0 }
"""

    tank_mass = state.mass - BUS_MASS_KG
    if tank_mass <= 100.0:
        raise RuntimeError(f"Propellant/consumables pool too small: {tank_mass}")
    tank_diagonal = tuple(state.inertia[index] - BUS_DIAGONAL[index] for index in range(3))
    if min(tank_diagonal) <= 0.0:
        raise RuntimeError(f"Tank inertia became non-positive: {tank_diagonal}")
    ixy, ixz, iyz = state.inertia[3:]
    return f"""
[[components]]
id = "a8000001-0000-0000-0000-000000000001"
name = "Apollo 8 CSM dry structure"
initial_mass_kg = {BUS_MASS_KG:.17g}
minimum_mass_kg = {BUS_MASS_KG:.17g}
variable_mass = false
local_center_of_mass_m = [0.0, 0.0, 0.0]
origin_body_m = [0.0, 0.0, 0.0]
component_to_body = [1.0, 0.0, 0.0, 0.0]
parent_component = ""
parent_anchor_m = [0.0, 0.0, 0.0]
child_anchor_m = [0.0, 0.0, 0.0]
child_to_parent_zero_orientation = [1.0, 0.0, 0.0, 0.0]
dofs = []
inertia = {{ ixx_kgm2 = {BUS_DIAGONAL[0]:.17g}, iyy_kgm2 = {BUS_DIAGONAL[1]:.17g}, izz_kgm2 = {BUS_DIAGONAL[2]:.17g}, ixy_kgm2 = {ixy:.17g}, ixz_kgm2 = {ixz:.17g}, iyz_kgm2 = {iyz:.17g} }}

[[components]]
id = "a8000002-0000-0000-0000-000000000002"
name = "Propellant and consumables pool"
initial_mass_kg = {tank_mass:.17g}
minimum_mass_kg = 100.0
variable_mass = true
local_center_of_mass_m = [0.0, 0.0, 0.0]
origin_body_m = [0.0, 0.0, 0.0]
component_to_body = [1.0, 0.0, 0.0, 0.0]
parent_component = "Apollo 8 CSM dry structure"
parent_anchor_m = [0.0, 0.0, 0.0]
child_anchor_m = [0.0, 0.0, 0.0]
child_to_parent_zero_orientation = [1.0, 0.0, 0.0, 0.0]
dofs = []
inertia = {{ ixx_kgm2 = {tank_diagonal[0]:.17g}, iyy_kgm2 = {tank_diagonal[1]:.17g}, izz_kgm2 = {tank_diagonal[2]:.17g}, ixy_kgm2 = 0.0, ixz_kgm2 = 0.0, iyz_kgm2 = 0.0 }}
"""


def thruster(name, point, direction, maximum, duration) -> str:
    return f"""
[[thrusters]]
name = "{name}"
mode = "commanded"
mount_component = "Apollo 8 CSM dry structure"
propellant_component = "Propellant and consumables pool"
application_point_component_m = {fmt(point)}
direction_component = {fmt(direction)}
ignition_time_mode = "elapsed"
ignition_elapsed_seconds = 0.0
never_shuts_down = false
shutdown_time_mode = "elapsed"
shutdown_elapsed_seconds = {duration + 1.0:.17g}
maximum_thrust_n = {maximum:.17g}
"""


def burn_thrusters(duration: float, sps_maximum_thrust_n: float) -> str:
    blocks = [
        thruster("SPS Main Engine", (0, 0, 0), (1, 0, 0), sps_maximum_thrust_n, duration),
        thruster("SM RCS Translation Cluster", (0, 0, 0), (1, 0, 0), RCS_CLUSTER_MAXIMUM_THRUST_N, duration),
        thruster("Balanced Vent Plus", (0, 0, 0), (0, 1, 0), 100.0, duration),
        thruster("Balanced Vent Minus", (0, 0, 0), (0, -1, 0), 100.0, duration),
        thruster("Pitch Plus A", (2, 0, 0), (0, 0, -1), 445.0, duration),
        thruster("Pitch Plus B", (-2, 0, 0), (0, 0, 1), 445.0, duration),
        thruster("Pitch Minus A", (2, 0, 0), (0, 0, 1), 445.0, duration),
        thruster("Pitch Minus B", (-2, 0, 0), (0, 0, -1), 445.0, duration),
        thruster("Yaw Plus A", (2, 0, 0), (0, 1, 0), 445.0, duration),
        thruster("Yaw Plus B", (-2, 0, 0), (0, -1, 0), 445.0, duration),
        thruster("Yaw Minus A", (2, 0, 0), (0, -1, 0), 445.0, duration),
        thruster("Yaw Minus B", (-2, 0, 0), (0, 1, 0), 445.0, duration),
        thruster("Roll Plus A", (0, 2, 0), (0, 0, 1), 445.0, duration),
        thruster("Roll Plus B", (0, -2, 0), (0, 0, -1), 445.0, duration),
        thruster("Roll Minus A", (0, 2, 0), (0, 0, -1), 445.0, duration),
        thruster("Roll Minus B", (0, -2, 0), (0, 0, 1), 445.0, duration),
    ]
    return "".join(blocks)


def vent_thrusters(duration: float) -> str:
    return "".join([
        thruster("Balanced Vent Plus", (0, 0, 0), (0, 1, 0), 100.0, duration),
        thruster("Balanced Vent Minus", (0, 0, 0), (0, -1, 0), 100.0, duration),
    ])


def gravity_text() -> str:
    return """
[gravity]
include_first_post_newtonian_correction = true

[[gravity.bodies]]
catalog_key = "Sun"
gravity_enabled = true
automatic_activation_radius_m = 0.0
barycenter_resolution_radius_m = 0.0
harmonic_model_csv_file = ""
maximum_harmonic_degree = 0

[[gravity.bodies]]
catalog_key = "EarthMoonBarycenter"
gravity_enabled = false
automatic_activation_radius_m = 0.0
barycenter_resolution_radius_m = 0.0
harmonic_model_csv_file = ""
maximum_harmonic_degree = 0

[[gravity.bodies]]
catalog_key = "Earth"
gravity_enabled = true
automatic_activation_radius_m = 0.0
barycenter_resolution_radius_m = 0.0
harmonic_model_csv_file = "../../../data/earth_j2.csv"
maximum_harmonic_degree = 2

[[gravity.bodies]]
catalog_key = "Moon"
gravity_enabled = true
automatic_activation_radius_m = 0.0
barycenter_resolution_radius_m = 0.0
harmonic_model_csv_file = "../../../data/moon_boeing_r2.csv"
maximum_harmonic_degree = 3

[srp]
enabled = false

[atmosphere]
enabled = false

[aerodynamics]
enabled = false
"""


def scenario_text(
    name: str,
    start_get: float,
    end_get: float,
    state: State,
    control_text: str,
    thrusters_text: str = "",
    simple: bool = False,
    burn: bool = False,
) -> str:
    duration = end_get - start_get
    maximum_step = (0.25 if burn else 20.0) * STEP_FACTOR
    output_step = 0.1 if burn else min(60.0, max(1.0, duration))
    if simple:
        maximum_step = min(0.25, max(0.01, duration / 1000.0))
        output_step = duration
    return f"""format = "TGSCN"
generator = "Apollo 8 current PHAROS reconstruction, 2026-09-09"

[scenario]
name = "{name}"
start_utc = "{iso_utc(start_get)}"
end_mode = "final_utc"
final_utc = "{iso_utc(end_get)}"
integrator = "adaptive_dormand_prince_54"
maximum_integrator_step_seconds = {maximum_step:.17g}
initial_integrator_step_seconds = {min(0.1, maximum_step):.17g}
absolute_tolerance = 1.0e-5
relative_tolerance = 1.0e-12
output_mode = "fixed_interval"
output_step_seconds = {output_step:.17g}
maximum_integration_steps = 5000000
maximum_output_samples = 1000000
mass_flow_convention = "thrust_includes_exhaust_momentum"

[initial_state]
position_icrf_m = {fmt(state.position)}
velocity_icrf_mps = {fmt(state.velocity)}
attitude_body_to_icrf = {fmt(state.quaternion)}
angular_velocity_body_radps = {fmt(state.angular_velocity)}
{component_text(state, simple)}
{thrusters_text}
{control_text}
{gravity_text()}
"""


def run_process(command, log_path: Path, cwd: Path = ROOT):
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True, errors="replace")
    write_text(log_path, result.stdout + result.stderr)
    if result.returncode != 0:
        raise RuntimeError(f"Command failed ({result.returncode}); see {log_path}")


def run_scenario(scenario: Path, results: Path, logs: Path) -> Path:
    results.mkdir(parents=True, exist_ok=True)
    logs.mkdir(parents=True, exist_ok=True)
    common = [str(RUNNER), str(scenario), "--kernel-dir", str(KERNELS)]
    print(f"Running {scenario.stem}",flush=True)
    started=time.monotonic()
    run_process(common + ["--validate-only"], logs / "validate.log")
    run_process(common + ["--output", str(results)], logs / "run.log")
    solution = results / f"{scenario.stem}_solution.csv"
    if not solution.exists():
        raise RuntimeError(f"Runner did not create {solution}")
    print(f"Completed {scenario.stem} in {time.monotonic()-started:.2f} s",flush=True)
    return solution


def compile_controller(controller_dir: Path, source_name: str, dll_name: str):
    source=controller_dir/source_name;dll=controller_dir/dll_name
    script=controller_dir/'build.cmd'
    command=f'cl /nologo /std:c++17 /EHsc /O2 /MT /LD /I"{SDK}" /I"{controller_dir}" /Fo"{controller_dir/source.with_suffix(".obj").name}" /Fe"{dll}" "{source}"'
    write_text(script,'@echo off\n'+f'call "{VCVARS}" >nul\nif errorlevel 1 exit /b %errorlevel%\n'+command+'\nexit /b %errorlevel%\n')
    run_process(['cmd.exe','/d','/c',str(script)],controller_dir/'compile.log',cwd=controller_dir)


def segment_solution(segment_id: str) -> Path:
    spec = SEGMENTS[segment_id]
    slug = spec["slug"]
    stem = f"apollo8_like_{slug}"
    return ROOT / "segments" / slug / "results" / f"{stem}_solution.csv"


def previous_solution(spec) -> Path:
    if "previous" in spec:
        return spec["previous"]
    return segment_solution(spec["previous_segment"])


def calibrate_burns():
    truth = truth_rows()
    output = {}
    for name, spec in BURNS.items():
        pre = truth[spec["pre"]]
        post = truth[spec["post"]]
        start_get = float(pre["get_seconds"])
        end_get = float(post["get_seconds"])
        directory = ROOT / "calibration" / name
        scenario_dir = directory / "scenario"
        scenario = scenario_dir / f"truth_{name}_gravity_only.tgscn"
        write_text(
            scenario,
            scenario_text(
                f"Apollo 8 truth-pair gravity coast for {name}",
                start_get,
                end_get,
                state_from_truth(pre),
                "[control]\nmode = \"none\"\n",
                simple=True,
            ),
        )
        solution = run_scenario(scenario, directory / "results", directory / "logs")
        coast_end = state_from_solution(solution)
        post_velocity = tuple(float(post[f"velocity_icrf_{axis}_mps"]) for axis in "xyz")
        raw = tuple(post_velocity[index] - coast_end.velocity[index] for index in range(3))
        magnitude = vector_norm(raw)
        direction = tuple(value / magnitude for value in raw)
        output[name] = {
            "truth_pre_event": spec["pre"],
            "truth_post_event": spec["post"],
            "truth_pair_duration_seconds": end_get - start_get,
            "gravity_only_end_velocity_icrf_mps": coast_end.velocity,
            "raw_required_delta_v_icrf_mps": raw,
            "raw_required_delta_v_magnitude_mps": magnitude,
            "controller_target_direction_icrf_unit": direction,
            "reported_actual_delta_v_mps": spec["actual_dv_fps"] * FOOT_TO_METER,
            "reported_actual_delta_v_fps": spec["actual_dv_fps"],
        }
    write_text(ROOT / "expected" / "burn_vectors.json", json.dumps(output, indent=2) + "\n")
    lines = ["# Burn-vector construction", "", "Each direction is the normalized difference between the NASA post-burn velocity and a TGSimCore gravity-only propagation from the NASA pre-burn state. Its magnitude is not fitted; each controller uses the independently reported achieved delta-v magnitude.", "", "| Maneuver | Truth-pair inferred dv (m/s) | Reported achieved dv (m/s) |", "|---|---:|---:|"]
    for name, values in output.items():
        display_name = BURN_DISPLAY_NAMES.get(name, name.upper())
        lines.append(f"| {display_name} | {values['raw_required_delta_v_magnitude_mps']:.6f} | {values['reported_actual_delta_v_mps']:.6f} |")
    write_text(ROOT / "expected" / "burn_vectors.md", "\n".join(lines) + "\n")
    print(ROOT / "expected" / "burn_vectors.json")


def required_constant_thrust(mass_kg, delta_v_mps, duration, isp_seconds):
    final_mass = mass_kg * math.exp(-delta_v_mps / (isp_seconds * G0))
    return (mass_kg - final_mass) * isp_seconds * G0 / duration


def burn_configuration(name: str, spec: dict, vector: dict):
    burn = BURNS[name]
    direction = vector[name]["controller_target_direction_icrf_unit"]
    target_mass = burn["target_mass_lb"] * POUND_TO_KG
    ullage_dv = burn.get("ullage_dv_fps", 0.0) * FOOT_TO_METER
    main_dv = (burn["actual_dv_fps"] - burn.get("ullage_dv_fps", 0.0)) * FOOT_TO_METER
    if burn["use_sps"]:
        maximum = burn.get("sps_maximum_thrust_n")
        if maximum is None:
            maximum = required_constant_thrust(
                target_mass, main_dv, burn["duration"], SPS_ISP_SECONDS)
        required = required_constant_thrust(
            target_mass, main_dv, burn["duration"], SPS_ISP_SECONDS)
        throttle = required / maximum
    else:
        maximum = RCS_CLUSTER_MAXIMUM_THRUST_N
        required = required_constant_thrust(
            target_mass, main_dv, burn["duration"], RCS_ISP_SECONDS)
        throttle = required / maximum
    if throttle > 1.000001:
        raise RuntimeError(f"{name} requires throttle {throttle:.6f} > 1")
    ullage_duration = spec.get("ullage_end", 0.0) - spec.get("ullage_start", 0.0)
    ullage_throttle = 0.0
    if ullage_dv > 0.0:
        ullage_required = required_constant_thrust(
            target_mass, ullage_dv, ullage_duration, RCS_ISP_SECONDS)
        ullage_throttle = ullage_required / RCS_CLUSTER_MAXIMUM_THRUST_N
    burn_end = spec.get("burn_end", spec["burn_start"] + burn["duration"])
    mode_name = burn.get("target_mode", "fixed_icrf")
    target_modes = {
        "fixed_icrf": 0,
        "retrograde_moon": 1,
        "prograde_moon": 2,
        "retrograde_tangential_moon": 3,
        "prograde_tangential_moon": 4,
        "circularize_moon": 5,
        "retrograde_earth": 1,
        "prograde_earth": 2,
    }
    return {
        "direction": direction,
        "target_mode": mode_name,
        "target_mode_id": target_modes[mode_name],
        "target_body_naif_id": (
            301 if mode_name.endswith("_moon")
            else 399 if mode_name.endswith("_earth")
            else 0
        ),
        "radial_bias": burn.get("radial_bias", 0.0),
        "normal_bias": burn.get("normal_bias", 0.0),
        "circularization_stop_mps": burn.get("circularization_stop_mps", 0.0),
        "target_mass": target_mass,
        "burn_start": spec["burn_start"],
        "burn_end": burn_end,
        "throttle": throttle,
        "use_sps": burn["use_sps"],
        "sps_maximum_thrust_n": maximum if burn["use_sps"] else MEASURED_SPS_THRUST_N,
        "main_required_thrust_n": required,
        "main_target_delta_v_mps": main_dv,
        "ullage_enabled": ullage_dv > 0.0,
        "ullage_start": spec.get("ullage_start", -1.0),
        "ullage_end": spec.get("ullage_end", -1.0),
        "ullage_throttle": ullage_throttle,
        "ullage_target_delta_v_mps": ullage_dv,
        "total_target_delta_v_mps": burn["actual_dv_fps"] * FOOT_TO_METER,
    }


def generate_burn_controller(directory: Path, values: dict, name: str) -> str:
    directory.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "tools" / "ApolloBurnController.cpp", directory / "ApolloBurnController.cpp")
    x, y, z = values["direction"]
    config = f"""#pragma once
#define APOLLO_TARGET_X ({x:.17g})
#define APOLLO_TARGET_Y ({y:.17g})
#define APOLLO_TARGET_Z ({z:.17g})
#define APOLLO_TARGET_MODE {values['target_mode_id']}
#define APOLLO_TARGET_BODY_NAIF_ID {values['target_body_naif_id']}
#define APOLLO_RADIAL_BIAS ({values['radial_bias']:.17g})
#define APOLLO_NORMAL_BIAS ({values['normal_bias']:.17g})
#define APOLLO_CIRCULARIZATION_STOP_MPS ({values['circularization_stop_mps']:.17g})
#define APOLLO_BURN_START_S ({values['burn_start']:.17g})
#define APOLLO_BURN_END_S ({values['burn_end']:.17g})
#define APOLLO_BURN_THROTTLE ({values['throttle']:.17g})
#define APOLLO_USE_SPS {1 if values['use_sps'] else 0}
#define APOLLO_SPS_ISP_SECONDS ({SPS_ISP_SECONDS:.17g})
#define APOLLO_TARGET_PREBURN_MASS_KG ({values['target_mass']:.17g})
#define APOLLO_ULLAGE_ENABLED {1 if values['ullage_enabled'] else 0}
#define APOLLO_ULLAGE_START_S ({values['ullage_start']:.17g})
#define APOLLO_ULLAGE_END_S ({values['ullage_end']:.17g})
#define APOLLO_ULLAGE_THROTTLE ({values['ullage_throttle']:.17g})
"""
    write_text(directory / "BurnConfig.h", config)
    controller_stem = BURN_CONTROLLER_STEMS.get(name, name.upper())
    dll_name = f"Apollo{controller_stem}Controller.dll"
    compile_controller(directory, "ApolloBurnController.cpp", dll_name)
    write_text(directory / "controller_parameters.json", json.dumps(values, indent=2) + "\n")
    return dll_name


def generate_mass_trim_controller(directory: Path, initial_mass, target_mass, duration) -> str:
    directory.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "tools" / "ApolloMassTrimController.cpp", directory / "ApolloMassTrimController.cpp")
    write_text(directory / "MassTrimConfig.h", f"""#pragma once
#define APOLLO_INITIAL_MASS_KG ({initial_mass:.17g})
#define APOLLO_TARGET_FINAL_MASS_KG ({target_mass:.17g})
#define APOLLO_TRIM_DURATION_S ({duration:.17g})
""")
    dll_name = "ApolloFinalCoastController.dll"
    compile_controller(directory, "ApolloMassTrimController.cpp", dll_name)
    return dll_name


def nearest_solution_row(solution: Path, elapsed_target: float):
    nearest = None
    nearest_error = math.inf
    with solution.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            error = abs(float(row["elapsed_time_seconds"]) - elapsed_target)
            if error < nearest_error:
                nearest = row
                nearest_error = error
    return nearest, nearest_error


def compare_event(solution: Path, start_get: float, event_id: str):
    truth = truth_rows()[event_id]
    event_get = float(truth["get_seconds"])
    row, sample_error = nearest_solution_row(solution, event_get - start_get)
    actual_position = tuple(float(row[f"position_icrf_{axis}_m"]) for axis in "xyz")
    actual_velocity = tuple(float(row[f"velocity_icrf_{axis}_mps"]) for axis in "xyz")
    truth_position = tuple(float(truth[f"position_icrf_{axis}_m"]) for axis in "xyz")
    truth_velocity = tuple(float(truth[f"velocity_icrf_{axis}_mps"]) for axis in "xyz")
    position_delta = tuple(actual_position[i] - truth_position[i] for i in range(3))
    velocity_delta = tuple(actual_velocity[i] - truth_velocity[i] for i in range(3))
    body = truth["body"].lower()
    body_position = tuple(float(row[f"body_{body}_position_icrf_{axis}_m"]) for axis in "xyz")
    body_velocity = tuple(float(row[f"body_{body}_velocity_icrf_{axis}_mps"]) for axis in "xyz")
    actual_relative_position = tuple(actual_position[i] - body_position[i] for i in range(3))
    actual_relative_velocity = tuple(actual_velocity[i] - body_velocity[i] for i in range(3))
    truth_relative_position = tuple(float(truth[f"relative_position_icrf_{axis}_m"]) for axis in "xyz")
    truth_relative_velocity = tuple(float(truth[f"relative_velocity_icrf_{axis}_mps"]) for axis in "xyz")
    radius = vector_norm(truth_relative_position)
    latitude = math.radians(float(truth["latitude_deg"]))
    angle_half_step = math.radians(0.005)
    position_resolution = math.sqrt(
        (radius * angle_half_step) ** 2 +
        (radius * max(abs(math.cos(latitude)), 1.0e-9) * angle_half_step) ** 2 +
        (0.05 * 1852.0) ** 2
    )
    speed = vector_norm(truth_relative_velocity)
    velocity_resolution = math.sqrt(
        (0.5 * FOOT_TO_METER) ** 2 + 2.0 * (speed * angle_half_step) ** 2
    )
    return {
        "event_id": event_id,
        "event_get_seconds": event_get,
        "actual_sample_get_seconds": start_get + float(row["elapsed_time_seconds"]),
        "sample_time_error_seconds": sample_error,
        "position_delta_icrf_m": position_delta,
        "position_error_m": vector_norm(position_delta),
        "velocity_delta_icrf_mps": velocity_delta,
        "velocity_error_mps": vector_norm(velocity_delta),
        "relative_radius_error_m": vector_norm(actual_relative_position) - vector_norm(truth_relative_position),
        "relative_speed_error_mps": vector_norm(actual_relative_velocity) - vector_norm(truth_relative_velocity),
        "table_position_quantization_half_step_m": position_resolution,
        "table_velocity_quantization_half_step_mps": velocity_resolution,
        "position_error_over_quantization": vector_norm(position_delta) / position_resolution,
        "velocity_error_over_quantization": vector_norm(velocity_delta) / velocity_resolution,
        "source_note": (
            "NASA Table 5-II converted with ITRF93; ignition altitude corrected "
            "from the internally inconsistent printed 165,561.5 nmi to "
            "167,561.5 nmi"
            if event_id == "third_midcourse_ignition"
            else "NASA Table 5-II converted with ITRF93 or MOON_ME_DE421"
        ),
    }


def target_body_from_quaternion(quaternion, target):
    w, x, y, z = quaternion
    norm = math.sqrt(w*w + x*x + y*y + z*z)
    w, x, y, z = (value / norm for value in (w, x, y, z))
    matrix = (
        (1-2*(y*y+z*z), 2*(x*y-w*z), 2*(x*z+w*y)),
        (2*(x*y+w*z), 1-2*(x*x+z*z), 2*(y*z-w*x)),
        (2*(x*z-w*y), 2*(y*z+w*x), 1-2*(x*x+y*y)),
    )
    return tuple(sum(matrix[row][column] * target[row] for row in range(3)) for column in range(3))


def analyze_burn(solution: Path, values: dict):
    rows = []
    with solution.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    integrated = [0.0, 0.0, 0.0]
    for previous, current in zip(rows, rows[1:]):
        dt = float(current["elapsed_time_seconds"]) - float(previous["elapsed_time_seconds"])
        for index, axis in enumerate("xyz"):
            a0 = float(previous[f"force_thrust_icrf_{axis}_n"]) / float(previous["mass_kg"])
            # The recorded value at a hard switch belongs to one side of the
            # discontinuity. A left-rule sum avoids inventing a half output
            # interval of thrust before ignition.
            integrated[index] += a0 * dt
    start_row, _ = nearest_solution_row(solution, values["burn_start"])
    end_row, _ = nearest_solution_row(solution, values["burn_end"])
    quaternion = tuple(float(start_row[f"quaternion_body_to_icrf_{axis}"]) for axis in "wxyz")
    target_direction = values["direction"]
    circularization_final_error = None
    if values["target_mode"] == "circularize_moon":
        def circularization_delta(row):
            relative_position = tuple(
                float(row[f"position_icrf_{axis}_m"]) -
                float(row[f"body_moon_position_icrf_{axis}_m"])
                for axis in "xyz")
            relative_velocity = tuple(
                float(row[f"velocity_icrf_{axis}_mps"]) -
                float(row[f"body_moon_velocity_icrf_{axis}_mps"])
                for axis in "xyz")
            radius = vector_norm(relative_position)
            radial = tuple(value / radius for value in relative_position)
            radial_speed = sum(relative_velocity[i] * radial[i] for i in range(3))
            transverse = tuple(
                relative_velocity[i] - radial_speed * radial[i] for i in range(3))
            transverse_norm = vector_norm(transverse)
            tangent = tuple(value / transverse_norm for value in transverse)
            circular_speed = math.sqrt(4.9028001184575496e12 / radius)
            return tuple(
                circular_speed * tangent[i] - relative_velocity[i] for i in range(3))
        delta = circularization_delta(start_row)
        delta_norm = vector_norm(delta)
        target_direction = tuple(value / delta_norm for value in delta)
        circularization_final_error = vector_norm(circularization_delta(end_row))
    elif values["target_mode"] != "fixed_icrf":
        relative_velocity = tuple(
            float(start_row[f"velocity_icrf_{axis}_mps"]) -
            float(start_row[f"body_moon_velocity_icrf_{axis}_mps"])
            for axis in "xyz")
        if "tangential" in values["target_mode"]:
            relative_position = tuple(
                float(start_row[f"position_icrf_{axis}_m"]) -
                float(start_row[f"body_moon_position_icrf_{axis}_m"])
                for axis in "xyz")
            radial_norm = vector_norm(relative_position)
            radial = tuple(value / radial_norm for value in relative_position)
            radial_speed = sum(relative_velocity[i] * radial[i] for i in range(3))
            relative_velocity = tuple(
                relative_velocity[i] - radial_speed * radial[i] for i in range(3))
            transverse_norm = vector_norm(relative_velocity)
            relative_velocity = tuple(
                relative_velocity[i] + values["radial_bias"] * transverse_norm * radial[i]
                for i in range(3))
            orbit_normal = (
                radial[1] * relative_velocity[2] - radial[2] * relative_velocity[1],
                radial[2] * relative_velocity[0] - radial[0] * relative_velocity[2],
                radial[0] * relative_velocity[1] - radial[1] * relative_velocity[0],
            )
            normal_norm = vector_norm(orbit_normal)
            orbit_normal = tuple(value / normal_norm for value in orbit_normal)
            relative_velocity = tuple(
                relative_velocity[i] + values["normal_bias"] * transverse_norm * orbit_normal[i]
                for i in range(3))
        sign = -1.0 if values["target_mode"].startswith("retrograde") else 1.0
        norm = vector_norm(relative_velocity)
        target_direction = tuple(sign * value / norm for value in relative_velocity)
    target_body = target_body_from_quaternion(quaternion, target_direction)
    angle = math.degrees(math.atan2(math.hypot(target_body[1], target_body[2]), target_body[0]))
    achieved = vector_norm(integrated)
    result = {
        "burn_start_alignment_error_deg": angle,
        "target_mode": values["target_mode"],
        "burn_start_mass_kg": float(start_row["mass_kg"]),
        "burn_end_mass_kg": float(end_row["mass_kg"]),
        "target_preburn_mass_kg": values["target_mass"],
        "integrated_thrust_acceleration_delta_v_icrf_mps": integrated,
        "integrated_thrust_delta_v_mps": achieved,
        "target_total_delta_v_mps": values["total_target_delta_v_mps"],
        "delta_v_error_mps": achieved - values["total_target_delta_v_mps"],
        "main_command_throttle": values["throttle"],
        "main_required_thrust_n": values["main_required_thrust_n"],
        "sps_maximum_thrust_n": values["sps_maximum_thrust_n"],
        "ullage_command_throttle": values["ullage_throttle"],
    }
    if circularization_final_error is not None:
        result["circularization_final_velocity_error_mps"] = circularization_final_error
        result["circularization_stop_threshold_mps"] = values["circularization_stop_mps"]
    return result


def continuity_metrics(previous: Path, current: Path):
    old = state_from_solution(previous)
    with current.open(newline="", encoding="utf-8") as stream:
        first = next(csv.DictReader(stream))
    new = state_from_solution_row(first)
    return {
        "position_discontinuity_m": vector_norm(tuple(new.position[i] - old.position[i] for i in range(3))),
        "velocity_discontinuity_mps": vector_norm(tuple(new.velocity[i] - old.velocity[i] for i in range(3))),
        "quaternion_component_max_discontinuity": max(abs(new.quaternion[i] - old.quaternion[i]) for i in range(4)),
        "angular_velocity_discontinuity_radps": vector_norm(tuple(new.angular_velocity[i] - old.angular_velocity[i] for i in range(3))),
        "mass_discontinuity_kg": new.mass - old.mass,
        "inertia_component_max_discontinuity_kgm2": max(abs(new.inertia[i] - old.inertia[i]) for i in range(6)),
    }


def write_analysis(directory: Path, continuity, comparisons, burn_analysis=None):
    payload = {"continuity": continuity, "checkpoint_comparisons": comparisons}
    if burn_analysis is not None:
        payload["burn_execution"] = burn_analysis
    write_text(directory / "analysis.json", json.dumps(payload, indent=2) + "\n")
    lines = ["# Segment analysis", "", "## State continuity", ""]
    for key, value in continuity.items():
        lines.append(f"- `{key}`: `{value:.12g}`")
    if burn_analysis is not None:
        lines.extend(["", "## Controller and maneuver", ""])
        for key, value in burn_analysis.items():
            if isinstance(value, list):
                lines.append(f"- `{key}`: `{value}`")
            elif isinstance(value, (int, float)):
                lines.append(f"- `{key}`: `{value:.12g}`")
    if comparisons:
        lines.extend(["", "## Historical checkpoints", "", "| Event | Position error (km) | Velocity error (m/s) | Position / source resolution |", "|---|---:|---:|---:|"])
        for item in comparisons:
            lines.append(f"| {item['event_id']} | {item['position_error_m']/1000:.6f} | {item['velocity_error_mps']:.6f} | {item['position_error_over_quantization']:.3f} |")
    write_text(directory / "analysis.md", "\n".join(lines) + "\n")


def run_segment(segment_id: str):
    if segment_id == "01":
        return run_initial()
    spec = SEGMENTS[segment_id]
    previous = previous_solution(spec)
    if not previous.exists():
        raise RuntimeError(f"Missing previous solution: {previous}")
    initial_state = state_from_solution(previous)
    slug = spec["slug"]
    directory = ROOT / "segments" / slug
    scenario_dir = directory / "scenario"
    controller_dir = directory / "controllers"
    stem = f"apollo8_like_{slug}"
    scenario = scenario_dir / f"{stem}.tgscn"
    duration = spec["end_get"] - spec["start_get"]
    burn_values = None

    if spec["kind"] == "burn":
        vectors_path = ROOT / "expected" / "burn_vectors.json"
        if not vectors_path.exists():
            raise RuntimeError("Retained burn vectors are required; do not implicitly recalibrate.")
        vectors = json.loads(vectors_path.read_text(encoding="utf-8"))
        burn_values = burn_configuration(spec["burn"], spec, vectors)
        dll = generate_burn_controller(controller_dir, burn_values, spec["burn"])
        control = f"[control]\nmode = \"compiled_user_controller\"\nunreal_controller_id = \"\"\ncontroller_dll_file = \"../controllers/{dll}\"\n"
        thrusters = burn_thrusters(duration, burn_values["sps_maximum_thrust_n"])
        burn_flag = True
    elif spec["kind"] == "mass_trim_coast":
        target_mass = spec["target_final_mass_lb"] * POUND_TO_KG
        dll = generate_mass_trim_controller(controller_dir, initial_state.mass, target_mass, duration)
        control = f"[control]\nmode = \"compiled_user_controller\"\nunreal_controller_id = \"\"\ncontroller_dll_file = \"../controllers/{dll}\"\n"
        thrusters = vent_thrusters(duration)
        burn_flag = False
    else:
        control = "[control]\nmode = \"none\"\n"
        thrusters = ""
        burn_flag = False

    write_text(
        scenario,
        scenario_text(
            f"Apollo 8 {spec['label']}", spec["start_get"], spec["end_get"],
            initial_state, control, thrusters, burn=burn_flag,
        ),
    )
    solution = run_scenario(scenario, directory / "results", directory / "logs")
    continuity = continuity_metrics(previous, solution)
    comparisons = [compare_event(solution, spec["start_get"], event) for event in spec["events"]]
    burn_analysis = analyze_burn(solution, burn_values) if burn_values else None
    write_analysis(directory / "analysis", continuity, comparisons, burn_analysis)
    metadata = {
        "segment_id": segment_id,
        "slug": slug,
        "start_get_seconds": spec["start_get"],
        "end_get_seconds": spec["end_get"],
        "start_utc": iso_utc(spec["start_get"]),
        "end_utc": iso_utc(spec["end_get"]),
        "previous_solution": str(previous.relative_to(ROOT)),
        "solution": str(solution.relative_to(ROOT)),
        "scenario_sha256": file_hash(scenario),
        "runner_sha256": file_hash(RUNNER),
        "controller_sha256": file_hash(controller_dir/dll) if spec["kind"] in ("burn","mass_trim_coast") else None,
    }
    write_text(directory / "metadata.json", json.dumps(metadata, indent=2) + "\n")
    print(json.dumps({"segment":segment_id,"checkpoint_position_errors_km":[round(c["position_error_m"]/1000,3) for c in comparisons],"burn_delta_v_mps":burn_analysis["integrated_thrust_delta_v_mps"] if burn_analysis else None}),flush=True)




def run_direct_checkpoint_diagnostic(event_id: str):
    previous_segment = (
        "12" if event_id == "post_third_midcourse_nav_update" else "02")
    previous = segment_solution(previous_segment)
    if not previous.exists():
        raise RuntimeError(f"Segment {previous_segment} must be complete first")
    start_get = SEGMENTS[previous_segment]["end_get"]
    truths = json.loads((ROOT / "expected" / "nby_direct_truth_states.json").read_text(encoding="utf-8"))
    truth = truths[event_id]
    end_get = truth["get_seconds"]
    directory = ROOT / "diagnostics" / event_id
    scenario = directory / "scenario" / f"apollo8_like_{event_id}.tgscn"
    write_text(
        scenario,
        scenario_text(
            f"Apollo 8 coast to direct {event_id} trajectory update",
            start_get, end_get, state_from_solution(previous),
            "[control]\nmode = \"none\"\n",
        ),
    )
    solution = run_scenario(scenario, directory / "results", directory / "logs")
    actual = state_from_solution(solution)
    position_delta = tuple(actual.position[i] - truth["position_icrf_m"][i] for i in range(3))
    velocity_delta = tuple(actual.velocity[i] - truth["velocity_icrf_mps"][i] for i in range(3))
    comparison = {
        "event_id": truth["event_id"],
        "position_delta_icrf_m": position_delta,
        "position_error_m": vector_norm(position_delta),
        "velocity_delta_icrf_mps": velocity_delta,
        "velocity_error_mps": vector_norm(velocity_delta),
        "source": truth["source"],
    }
    payload = {
        "continuity": continuity_metrics(previous, solution),
        "comparison": comparison,
    }
    write_text(directory / "analysis" / "analysis.json", json.dumps(payload, indent=2) + "\n")
    write_text(
        directory / "analysis" / "analysis.md",
        f"# Direct {event_id} checkpoint\n\n"
        f"- Position error: `{comparison['position_error_m']/1000.0:.6f} km`\n"
        f"- Velocity error: `{comparison['velocity_error_mps']:.6f} m/s`\n"
        f"- Truth: {truth['source']}.\n",
    )
    print(json.dumps(comparison, indent=2))


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("calibrate")
    one = sub.add_parser("run-segment")
    one.add_argument("segment", choices=SEGMENTS)
    all_parser = sub.add_parser("run-all")
    all_parser.add_argument("--from-segment", default="01", choices=SEGMENTS)
    all_parser.add_argument("--through", default="13", choices=SEGMENTS)
    direct = sub.add_parser("direct-diagnostic")
    direct.add_argument(
        "event",
        choices=(
            "post_first_midcourse_nav_update",
            "pre_second_midcourse_nav_update",
            "post_third_midcourse_nav_update",
        ),
    )
    args = parser.parse_args()
    if args.command == "calibrate":
        raise SystemExit("Legacy unconstrained calibration is archived only. Use the bounded current retuning workflow.")
    elif args.command == "run-segment":
        run_segment(args.segment)
    elif args.command == "run-all":
        ids = list(SEGMENTS)
        start = ids.index(args.from_segment)
        end = ids.index(args.through)
        if end < start:
            raise SystemExit("--through precedes --from-segment")
        for segment_id in ids[start:end + 1]:
            run_segment(segment_id)
    elif args.command == "direct-diagnostic":
        run_direct_checkpoint_diagnostic(args.event)


if __name__ == "__main__":
    main()
