# Unreal Simulation Adapter

This folder contains Unreal-facing glue code. `UTGSimulationRunSubsystem`
prepares a reviewed scenario and launches `PHAROSScenarioRunner` in a separate
process. `FTGSimulationAdapter` provides SPICE and controller attachment for
native in-process request construction, as well as UTC/ET conversion helpers;
the graphical Run action uses the runner process instead.

The intended direction is:

```text
Blueprint/UI
  <-> FTGSimulationScenario
  <-> ScenarioDocument / .tgscn
  -> PHAROSScenarioRunner process
       -> shared ScenarioCompiler -> SimulationRequest
       -> TGSimCore result history -> CSV
  -> Unreal CSV playback, actors, splines, widgets, and animation
```

Do not implement physics in this folder.

`Docs/Reference/ScenarioInputs.md` is the HUD contract. The exhaustive
`FTGSimulationScenario` conversion and `.tgscn` Blueprint callbacks are
implemented by `TGScenarioDocumentAdapter` and `UTGScenarioFileLibrary`.
Controller source/DLL persistence, background compilation, and managed-library
validation live under `Simulation/Control`. During graphical and command-line
runs, `Tools/TGScenarioRunner/StandaloneControllerAdapter` loads the run-local
controller inside the runner process.

The `.tgscn` schema is documented in `Docs/Reference/TGSCN.md`. Its portable parser
and compiler live in TGSimCore so the standalone command-line runner and Unreal
cannot drift into two different scenario interpretations.

The exact widget/file/request/run/result wiring is documented in
`Docs/Development/ApplicationIntegration.md`. `UTGSimulationRunSubsystem` launches
the runner, forwards progress and cancellation, and hands the completed scenario
and CSV to playback.
