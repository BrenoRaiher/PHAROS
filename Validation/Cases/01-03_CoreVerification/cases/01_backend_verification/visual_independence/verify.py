"""Run the dedicated DAT-02 scenarios with the preserved PHAROS runner."""

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))

from pathlib import Path
import argparse
import csv
import hashlib
import json
import math
import subprocess
import tomllib

ROOT = Path(__file__).resolve().parent

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def physical_document(path):
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    for component in data["components"]:
        component.pop("visual", None)
    return data

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runtime", type=Path,
                        default=_RUNTIME)
    args = parser.parse_args()
    runner = args.runtime / "PHAROSScenarioRunner.exe"
    kernels = _KERNELS
    results = ROOT / "results"
    results.mkdir(exist_ok=True)
    names = ["reference", "primitives", "transformed", "hidden", "physical_control"]
    histories = {}
    records = []
    for name in names:
        scenario = ROOT / "scenarios" / (name + ".tgscn")
        command = [str(runner), str(scenario), "--kernel-dir", str(kernels),
                   "--output", str(results)]
        run = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (results / (name + ".log")).write_text(run.stdout + run.stderr, encoding="utf-8")
        if run.returncode:
            raise RuntimeError(name + ": " + run.stdout + run.stderr)
        solution = results / (name + "_solution.csv")
        summary = results / (name + "_summary.txt")
        with solution.open(newline="", encoding="utf-8-sig") as stream:
            table = list(csv.reader(stream))
        header = table[0]
        values = [[float(cell) for cell in row] for row in table[1:]]
        assert values and all(len(row) == len(header) for row in values)
        assert all(math.isfinite(x) for row in values for x in row)
        histories[name] = (header, values)
        records.append({"scenario": name, "scenario_sha256": sha(scenario),
                        "solution_sha256": sha(solution), "summary_sha256": sha(summary),
                        "rows": len(values), "columns": len(header), "exit_code": run.returncode})

    baseline = ROOT / "scenarios/reference.tgscn"
    comparisons = []
    for name in names[1:-1]:
        candidate = ROOT / "scenarios" / (name + ".tgscn")
        assert candidate.read_bytes() != baseline.read_bytes()
        assert physical_document(candidate) == physical_document(baseline)
        assert histories[name] == histories["reference"], name + ": numerical output changed"
        assert sha(results / (name + "_solution.csv")) == sha(results / "reference_solution.csv")
        comparisons.append({"variant": name, "physical_inputs_equal": True,
                            "complete_csv_byte_identical": True, "maximum_absolute_difference": 0.0})
    assert physical_document(ROOT / "scenarios/physical_control.tgscn") != physical_document(baseline)
    assert histories["physical_control"] != histories["reference"]
    assert histories["reference"][1][0] != histories["reference"][1][-1]
    report = {"requirement": "DAT-02", "passed": True,
              "runner_sha256": sha(runner), "runs": records,
              "comparisons": comparisons, "physical_control_changes_output": True,
              "reference_motion_is_nonconstant": True,
              "scope": "Visual metadata only; physical components, mass properties, loads, initial state and solver settings are fixed."}
    (ROOT / "metrics.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    main()
