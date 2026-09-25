# PHAROS Scenario Runner

`PHAROSScenarioRunner` executes a PHAROS `.tgscn` scenario directly through the
PHAROS simulation engine, without launching Unreal Engine. It uses the same
parser, scenario compiler, SPICE catalog, controller DLL ABI, dynamics,
integrators, and result recorder as the packaged application.

The complete user workflow, scenario physical-input reference, controller build
procedure, controller API, and actuator examples are in the
[PHAROS Standalone Runner Guide](../../Docs/GettingStarted/StandaloneRunner.md).

## Build

From the project root:

```bat
Tools\TGScenarioRunner\BuildStandaloneRunner.bat
```

The script uses Visual Studio's x64 compiler and Unreal Engine 5.7's bundled
Eigen, Boost, and nanoflann headers. Set `UE_ENGINE_ROOT` before calling the
script if UE is installed elsewhere. The executable is written to:

```text
Tools\TGScenarioRunner\bin\PHAROSScenarioRunner.exe
```

The equivalent CMake target is `PHAROSScenarioRunner` in
`Source/TGSimCore/CMakeLists.txt`.

## Run

```bat
Tools\TGScenarioRunner\bin\PHAROSScenarioRunner.exe mission.tgscn
```

Options:

```text
--output <directory>      Output folder; defaults to the scenario folder
--kernel-dir <directory>  Folder containing the project's SPICE kernels
--validate-only           Parse, compile, and validate without propagation
```

The runner automatically checks for SPICE kernels beside the executable, in
the PHAROS project's `Content/SPICEKernels` directory, and in the current working
directory. `--kernel-dir` overrides this search.

When launched by Unreal, the runner also accepts `CANCEL` followed by a newline
on standard input. It reports machine-readable lines on standard output:

```text
PHAROS_EVENT<TAB>PROGRESS<TAB>percent<TAB>status
PHAROS_EVENT<TAB>LOG<TAB><TAB><TAB>message
PHAROS_EVENT<TAB>ERROR<TAB><TAB><TAB>message
PHAROS_EVENT<TAB>RESULT<TAB><TAB><TAB>csv-path
```

Normal human-readable CLI output remains available alongside these records.
Solution and summary files are written to `.tmp` files and atomically renamed
only after each complete file has been flushed successfully.

Example, using the path to your saved scenario:

```bat
Tools\TGScenarioRunner\bin\PHAROSScenarioRunner.exe ^
  Missions\ExampleMission\mission.tgscn ^
  --output Results\Mission
```

## Outputs

For `mission.tgscn`, the runner creates:

```text
mission_solution.csv
mission_summary.txt
```

The CSV is the complete AVS-style `SimulationResult::solution_array` with a
quoted header row. The summary records success, the simulation message, sample
count, and column count. A failed numerical run still writes both files and
returns a nonzero process exit code.

## Controller DLLs

When `[control].mode` is `compiled_user_controller`, the runner resolves
`controller_dll_file` relative to the `.tgscn`, loads it, verifies the required
controller exports and structure contract, creates one private controller
instance for the run, and adapts every callback through
`ControllerSDK/PHAROSControllerAPI.h`.

Each commanded-thruster output contains its optional analytic actual-discharge
derivative. Controllers must be built with the Controller SDK distributed with
the current application.

The DLL must already be compiled. PHAROS Scenario Runner does not compile
source code and does not use an Unreal controller-library ID.

## Exit Codes

| Code | Meaning |
|---|---|
| `0` | Validation or simulation succeeded |
| `2` | Command-line usage error |
| `3` | Scenario file could not be parsed |
| `4` | Controller DLL could not be loaded |
| `5` | SPICE kernels could not be loaded |
| `6` | Scenario could not compile to `SimulationRequest` |
| `7` | Output directory could not be created |
| `8` | Output files could not be written |
| `9` | Propagation completed with a failed `SimulationResult` |
