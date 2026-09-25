import csv
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "expected" / "nby_direct_states_source.csv"
BODY_SOURCES = (
    ROOT / "expected" / "post_first_midcourse_body_query_output.csv",
    ROOT / "expected" / "post_third_midcourse_body_query_output.csv",
)
OUT = ROOT / "expected" / "nby_direct_truth_states.json"
FOOT_TO_METER = 0.3048
NBY1969_TO_J2000 = (
    (0.9999714307930930, -0.006932510238740845, -0.003012955260858825),
    (0.006932510237296334, 0.9999759698076381, -0.00001044430021552792),
    (0.003012955264182510, -0.00001044334136026557, 0.9999954609854550),
)


def rotate(vector):
    return tuple(sum(row[index] * vector[index] for index in range(3)) for row in NBY1969_TO_J2000)


with SOURCE.open(newline="", encoding="utf-8") as stream:
    sources = list(csv.DictReader(stream))
bodies = {}
for body_source in BODY_SOURCES:
    with body_source.open(newline="", encoding="utf-8") as stream:
        bodies.update({row["event_id"]: row for row in csv.DictReader(stream)})

payload = {}
for source in sources:
    body = bodies[source["event_id"]]
    position_nby = tuple(float(source[f"position_{axis}_ft"]) * FOOT_TO_METER for axis in "xyz")
    velocity_nby = tuple(float(source[f"velocity_{axis}_ftps"]) * FOOT_TO_METER for axis in "xyz")
    position_relative = rotate(position_nby)
    velocity_relative = rotate(velocity_nby)
    body_position = tuple(
        float(body[f"position_icrf_{axis}_m"]) - float(body[f"relative_position_icrf_{axis}_m"])
        for axis in "xyz"
    )
    body_velocity = tuple(
        float(body[f"velocity_icrf_{axis}_mps"]) - float(body[f"relative_velocity_icrf_{axis}_mps"])
        for axis in "xyz"
    )
    payload[source["event_id"]] = {
        "event_id": source["event_id"],
        "get_seconds": float(source["get_seconds"]),
        "source_frame": source["frame"],
        "position_earth_relative_j2000_m": position_relative,
        "velocity_earth_relative_j2000_mps": velocity_relative,
        "position_icrf_m": tuple(body_position[index] + position_relative[index] for index in range(3)),
        "velocity_icrf_mps": tuple(body_velocity[index] + velocity_relative[index] for index in range(3)),
        "source": source["source"],
        "conversion": "Pure mean-NBY-1969 to J2000 precession rotation, then DE442 Earth barycentric translation",
    }

OUT.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
(ROOT / "expected" / "post_first_midcourse_nav_update_truth_state.json").write_text(
    json.dumps(payload["post_first_midcourse_nav_update"], indent=2) + "\n", encoding="utf-8")
(ROOT / "expected" / "post_third_midcourse_nav_update_truth_state.json").write_text(
    json.dumps(payload["post_third_midcourse_nav_update"], indent=2) + "\n",
    encoding="utf-8")
print(json.dumps(payload, indent=2))
