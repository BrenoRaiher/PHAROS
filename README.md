# PHAROS

**Build a spacecraft. Simulate its coupled motion. Explore the results.**

PHAROS — **Platform for Hierarchical Articulated Rigid-Body and Orbital
Simulation** — combines spacecraft orbital, attitude, and articulated-body
dynamics in a modular C++ Physics Core, with a Windows application for scenario
setup and interactive 3D playback. Model moving assemblies, configure forces
and actuators, and supply your own C++ controller. Run the calculation, then
inspect the resulting motion from the scale of an individual joint to an
interplanetary trajectory.

[**Download for Windows**](https://github.com/BrenoRaiher/PHAROS/releases/latest)
· [Watch the demos](#see-pharos-in-action)
· [Brief User Guide (PDF)](Docs/PHAROS_BriefUserGuide.pdf)
· [Complete thesis (PDF)](Docs/PHAROS_CompleteThesis.pdf)
· [Run from the terminal](#run-from-the-terminal)
· [Build from source](Docs/GettingStarted/Build.md)

## See PHAROS in action

These recordings show PHAROS running on a desktop. **Click a preview or video
title to watch the full 1080p MP4** without installing the application.

| AURORA articulation | Earth-orbit attitude recovery |
| --- | --- |
| [![AURORA articulated spacecraft in PHAROS](Docs/Media/Previews/aurora-articulation.jpg)](https://media.githubusercontent.com/media/BrenoRaiher/PHAROS/main/Docs/Media/Videos/aurora-articulation.mp4) | [![PHAROS spacecraft playback above Earth](Docs/Media/Previews/earth-orbit-attitude-recovery.jpg)](https://media.githubusercontent.com/media/BrenoRaiher/PHAROS/main/Docs/Media/Videos/earth-orbit-attitude-recovery.mp4) |
| [**Watch · 2:00**](https://media.githubusercontent.com/media/BrenoRaiher/PHAROS/main/Docs/Media/Videos/aurora-articulation.mp4) | [**Watch · 9:05**](https://media.githubusercontent.com/media/BrenoRaiher/PHAROS/main/Docs/Media/Videos/earth-orbit-attitude-recovery.mp4) |
| Twelve driven degrees of freedom: folding solar wings, a nested robotic arm, and a telescoping antenna boom with a two-axis gimbal. | Attitude motion, telemetry, vectors, plots, and camera controls around Earth. |

| Apollo 8 lunar arrival | Cassini at Saturn |
| --- | --- |
| [![Apollo 8 reconstruction in PHAROS](Docs/Media/Previews/apollo8-lunar-arrival.jpg)](https://media.githubusercontent.com/media/BrenoRaiher/PHAROS/main/Docs/Media/Videos/apollo8-lunar-arrival.mp4) | [![Cassini approaching Saturn in PHAROS](Docs/Media/Previews/cassini-saturn-arrival.jpg)](https://media.githubusercontent.com/media/BrenoRaiher/PHAROS/main/Docs/Media/Videos/cassini-saturn-arrival.mp4) |
| [**Watch · 2:38**](https://media.githubusercontent.com/media/BrenoRaiher/PHAROS/main/Docs/Media/Videos/apollo8-lunar-arrival.mp4) | [**Watch · 5:05**](https://media.githubusercontent.com/media/BrenoRaiher/PHAROS/main/Docs/Media/Videos/cassini-saturn-arrival.mp4) |
| Spacecraft orientation and trajectory inspection during a reconstructed lunar arrival. | A Saturn approach viewed alongside the planet, moons, trajectory, and direction vectors. |

## What you can model

- **Coupled spacecraft motion:** orbital and attitude propagation, hierarchical
  rigid components, nested revolute and prismatic joints, and changing mass properties.
- **Actuators and user control:** joint motors, reaction wheels, prescribed or
  commanded thrusters, propellant depletion, and independently compiled C++ controllers.
- **A configurable environment:** SPICE celestial ephemerides, selected gravity
  sources and harmonics, solar radiation pressure, eclipses, component shadows,
  atmospheric models, and aerodynamic coefficient tables.
- **Numerical propagation:** fixed-step RK4 or adaptive Dormand–Prince 5(4),
  actuator and joint-limit events, recorded state histories, and CSV output.
- **Interactive inspection:** spacecraft and solar-system views, adjustable
  playback, telemetry, vectors, and plots of recorded quantities. Primitive
  shapes and imported STL meshes provide the spacecraft's visual appearance.

The graphical application and standalone runner use the same Physics Core.
The 3D application displays the computed history; Unreal Engine supplies the
interface and rendering, while the Physics Core computes spacecraft dynamics.

## Install and run

**Windows x64 · Windows 11 is the tested platform.**

The [Brief User Guide (PDF)](Docs/PHAROS_BriefUserGuide.pdf) provides
illustrated instructions for installation, scenario creation, controller
setup, simulation execution, and playback.

1. Open [GitHub Releases](https://github.com/BrenoRaiher/PHAROS/releases/latest)
   and download the **PHAROS setup executable** from the release's assets.
   The version 1.0 installer is `PHAROS-1.0-Setup.exe` (approximately 1.43 GB);
   its [SHA-256 checksum](Docs/Release/PHAROS-1.0-Setup.exe.sha256) is available
   for verification.
2. Run the installer, then launch **PHAROS** from the Start Menu or desktop
   shortcut. Unreal Editor and Visual Studio are **not required** to use the
   installed application. The installer includes the runtime resources,
   SPICE kernels, standalone runner, and controller compiler.
3. Create a scenario or import a `.tgscn` file. Define the initial state,
   components, joints, actuators, enabled environment models, and integration
   settings. Keep any referenced meshes, controller DLLs, and input tables
   with the scenario.
4. Review the configuration and run the simulation. Open its results for
   playback, adjust the timeline and camera, and inspect telemetry or plots.
   Scenarios and results remain available in the **Scenario Library**.

To add your own control logic, use **Controller Library** to import a C++
source file and build it with the bundled compiler, or import a prebuilt
controller DLL. Complete the application's trusted-code confirmation and
select that controller for the scenario. Start with the
[Controller SDK and template](ControllerSDK/README.md).

See the [scenario input reference](Docs/Reference/ScenarioInputs.md) for
configuration fields and the [TGSCN reference](Docs/Reference/TGSCN.md) for
portable scenario files. Installation and compatibility details are in the
[Windows application guide](Tools/Release/InstallerReadme.md).

## Run from the terminal

`PHAROSScenarioRunner.exe` runs `.tgscn` scenarios without opening the graphical
application. Use it for automated runs, parameter studies, and workflows that
consume CSV results. **A TGSCN is an input file; it is not an executable and
does not need to be compiled by the user.**

For the installer's default location, open PowerShell in the folder containing
your `mission.tgscn` and run:

```powershell
$pharosRoot = Join-Path $env:LOCALAPPDATA 'Programs\PHAROS'
& "$pharosRoot\PHAROS\Binaries\Win64\PHAROSScenarioRunner.exe" `
  .\mission.tgscn `
  --kernel-dir "$pharosRoot\PHAROS\Content\SPICEKernels" `
  --output .\Results
```

Change `$pharosRoot` if you selected another installation folder. The runner
writes `mission_solution.csv` and `mission_summary.txt` into `Results`.
Add `--validate-only` to check inputs without propagation. Any selected
controller must already be compiled into a compatible DLL.

Kernels are separate data files supplied with PHAROS, not embedded in the
runner executable. The command above uses the installed kernel directory.
The [standalone guide](Docs/GettingStarted/StandaloneRunner.md) covers portable
runner folders, scenario authoring, command-line options, and controllers.
Runner source and build instructions are in
[Tools/TGScenarioRunner](Tools/TGScenarioRunner/README.md).

## Explore the source and evidence

| I want to… | Start here |
|---|---|
| Build or extend PHAROS | [Build guide](Docs/GettingStarted/Build.md) · [Contributing](CONTRIBUTING.md) |
| Understand the simulation engine | [Physics Core](Docs/Development/PhysicsCore.md) · [Multibody dynamics](Docs/Reference/MultibodyDynamics.md) |
| Inspect verification and mission comparisons | [Case catalog](Validation/README.md) · [Reproduction guide](Docs/GettingStarted/Reproduce.md) |
| Browse all documentation | [Documentation index](Docs/README.md) |

The assessment material includes controlled verification cases and simplified,
calibrated JWST, Apollo 8, and Cassini mission comparisons. Inputs, controller
sources, references, and compact summaries are included. The
[PHAROS Reproducibility Bundle ZIP](https://github.com/BrenoRaiher/PHAROS/raw/refs/heads/main/PHAROS-Reproducibility-Bundle.zip)
in the repository root contains full recorded histories, frozen executables
and controller libraries, and the captured assessment source. Download and
extract it, then follow the [reproduction guide](Docs/GettingStarted/Reproduce.md)
to restore the files into the checkout. The ZIP is stored with Git LFS.

For the mathematical formulation, physical models, software architecture,
and interpretation of the verification and mission comparisons, read the
[complete undergraduate thesis (PDF)](Docs/PHAROS_CompleteThesis.pdf).
It also documents scenario and controller interfaces. The
[Brief User Guide (PDF)](Docs/PHAROS_BriefUserGuide.pdf) is available
separately for practical instructions on using PHAROS.

Both documents can be saved for offline reading:
[download the user guide](https://github.com/BrenoRaiher/PHAROS/raw/refs/heads/main/Docs/PHAROS_BriefUserGuide.pdf)
· [download the complete thesis](https://github.com/BrenoRaiher/PHAROS/raw/refs/heads/main/Docs/PHAROS_CompleteThesis.pdf).

### Repository map

| Folder | Contents |
|---|---|
| `Source/` | Physics Core, application code, native checks, and required third-party libraries |
| `Content/`, `Config/`, `Build/` | Unreal assets, configuration, build resources, and kernel data |
| `ControllerSDK/` | Controller API, template, and compilation instructions |
| `Tools/` | Standalone runner, release tools, and repository checks |
| `Docs/` | User guide and complete thesis PDFs, user and developer documentation, demos, release information, and notices |
| `Validation/` | Assessment inputs, references, controllers, and result summaries |

Install Git LFS before cloning and run `git lfs pull` to obtain the assets,
kernels, and demo recordings. Follow the build guide for development setup.
Collaborators with write access can develop and merge changes under the
[collaboration guidelines](.github/GOVERNANCE.md).

## License and citation

First-party PHAROS source and documentation use the [MIT License](LICENSE).
Unreal Engine and third-party code, data, and assets retain their respective
terms; see [licenses and attribution](Docs/Legal/README.md).
PHAROS is not certified for flight operations or safety-critical decisions.

**Please cite PHAROS when using it in your work**, identifying the release or
commit used. [CITATION.cff](CITATION.cff) provides the citation metadata.

Copyright (c) 2026 Breno Raiher.
