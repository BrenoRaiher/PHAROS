# Connecting Unreal Engine to TGSimCore

This document defines exactly how Unreal HUD data, `.tgscn` files, runtime
services, TGSimCore, and simulation results connect. The packaged application launches the standalone runner and plays back its recorded results.

## 1. The Four Data Layers

```text
UMG widgets
    |
    v
FTGSimulationScenario                 Unreal-authored scenario draft
    |
    v
ScenarioDocument <-> .tgscn           Portable, lossless authored scenario
    |
    v
SimulationRequest                     Pure backend input plus runtime services
    |
    v
SimulationEngine::Run()
    |
    v
SimulationResult                      Typed history plus numeric solution array
```

The boundaries have different responsibilities:

| Layer | Contains Unreal-only fields | Contains runtime objects | Contains physics results |
|---|---:|---:|---:|
| `FTGSimulationScenario` | Yes | No | No |
| `ScenarioDocument` / `.tgscn` | Yes | No | No |
| `SimulationRequest` | No | Yes: SPICE provider and optional controller | No |
| `SimulationResult` | No | No | Yes |

Do not pass widgets, actors, meshes, materials, or other `UObject` pointers into
TGSimCore. Do not implement force models or unit/frame conversion in widgets.

## 2. Implemented Entry Points

### Unreal file callbacks

`UTGScenarioFileLibrary` exposes these functions directly to Blueprint:

```cpp
bool ExportScenarioToTgscn(
    const UObject* WorldContextObject,
    const FTGSimulationScenario& Scenario,
    const FString& FilePath,
    FText& OutMessage);

bool ImportScenarioFromTgscn(
    const UObject* WorldContextObject,
    const FString& FilePath,
    FTGSimulationScenario& OutScenario,
    FText& OutMessage);
```

Source:
`Source/TG/Public/Simulation/TGScenarioFileLibrary.h`.

### Native Unreal adapters

`FTGScenarioDocumentAdapter` provides:

```cpp
void ToPortableDocument(
    const FTGSimulationScenario& Scenario,
    const FString& ResolvedControllerDllPath,
    tgsim::scenario::ScenarioDocument& OutDocument);

bool FromPortableDocument(
    const tgsim::scenario::ScenarioDocument& Document,
    const FString& SourceScenarioFilePath,
    FTGSimulationScenario& OutScenario,
    FString& OutMessage);

bool BuildSimulationRequest(
    const FTGSimulationScenario& Scenario,
    const UTGControllerLibrarySubsystem* ControllerLibrary,
    const FString& SourceScenarioFilePath,
    tgsim::SimulationRequest& OutRequest,
    FString& OutMessage);
```

Source:
`Source/TG/Public/Simulation/TGScenarioDocumentAdapter.h`.

### Pure TGSimCore file and compiler APIs

```cpp
LoadScenarioFile(path, document, diagnostics);
SaveScenarioFile(path, document, diagnostics);
ParseScenarioText(text, document, diagnostics);
SerializeScenarioText(document);
CompileScenario(document, scenario_path, services, request, diagnostics);
```

Sources:

- `Source/TGSimCore/include/TGSim/Scenario/ScenarioFile.h`
- `Source/TGSimCore/include/TGSim/Scenario/ScenarioCompiler.h`

## 3. Normal HUD Editing Workflow

`UTGSimulationSubsystem` owns the current `FTGSimulationScenario` draft.
Configuration panels should follow this sequence:

1. Obtain `UTGSimulationSubsystem` from the current `UGameInstance`.
2. Call `GetCurrentScenarioDraft()` to obtain a value copy.
3. Modify that copy with the user's current panel values.
4. Call `SetCurrentScenarioDraft(ModifiedScenario)`.
5. Never assume that modifying a local copy changes the subsystem automatically.

Every `SetCurrentScenarioDraft` call increments the draft revision and
invalidates any previously accepted Review result. The simulation button must
therefore use the subsystem's current draft, not a stale scenario retained by a
widget.

## 4. Exporting the HUD Scenario to `.tgscn`

### Blueprint wiring

1. Read the current scenario with `GetCurrentScenarioDraft`.
2. Open a save-file dialog and obtain the selected path.
3. Call `Export Scenario To Tgscn`.
4. Branch on its Boolean return value.
5. Show `OutMessage` on both success and failure.

The function appends `.tgscn` when the selected path has no such extension.

### Native equivalent

```cpp
UTGSimulationSubsystem* Scenarios =
    GetGameInstance()->GetSubsystem<UTGSimulationSubsystem>();

const FTGSimulationScenario Scenario =
    Scenarios->GetCurrentScenarioDraft();

FText Message;
const bool bExported =
    UTGScenarioFileLibrary::ExportScenarioToTgscn(
        this,
        Scenario,
        SelectedFilePath,
        Message);
```

Internally, export performs:

```text
FTGSimulationScenario
    -> PrepareSolarRadiationPressureGeometry
    -> ToPortableDocument
    -> ScenarioDocument
    -> SaveScenarioFile
    -> .tgscn
```

All frontend information is written, including visual geometry, STL and texture
paths, colors, SRP authoring/cache fields, and the Unreal controller-library ID.
Those presentation/cache values never enter `SimulationRequest`. Finalized
`srp.facets` are also written, but they are physical proxy geometry and do enter
the backend when SRP is enabled.

For a selected library controller, export asks
`UTGControllerLibrarySubsystem` for its ready managed DLL path and fails if
that controller is untrusted, missing, not built, or API-incompatible. An
imported standalone DLL path can be preserved when no library ID is set;
exporting that path does not load or approve the DLL.

Export does not replace Review validation. A user may save a draft that is not
yet runnable.

## 5. Importing `.tgscn` into the HUD

### Blueprint wiring

1. Open a file dialog restricted to `.tgscn`.
2. Call `Import Scenario From Tgscn`.
3. When it returns true, call `SetCurrentScenarioDraft(OutScenario)`.
4. Refresh every configuration panel from the new subsystem draft.
5. Send the user to Review before enabling simulation.
6. Display `OutMessage`; a successful import can still contain an actionable
   controller warning.

### Native equivalent

```cpp
FTGSimulationScenario ImportedScenario;
FText Message;

if (UTGScenarioFileLibrary::ImportScenarioFromTgscn(
        this,
        SelectedFilePath,
        ImportedScenario,
        Message))
{
    UTGSimulationSubsystem* Scenarios =
        GetGameInstance()->GetSubsystem<UTGSimulationSubsystem>();
    Scenarios->SetCurrentScenarioDraft(ImportedScenario);
}
```

Internally, import performs:

```text
.tgscn
    -> LoadScenarioFile
    -> ScenarioDocument
    -> FromPortableDocument
    -> FTGSimulationScenario
```

Relative external-file paths are resolved against the directory containing the
imported `.tgscn` and placed into the existing Unreal path fields as absolute
paths. Absolute paths remain absolute.

Import parses the complete schema, checks types, rejects unknown/misspelled
keys, and restores frontend data. It does not trust or load arbitrary native
DLLs. If a file contains a standalone DLL path but no usable Unreal controller
ID, the scenario imports with a warning; the user must import/trust that DLL in
the Controller Library and select the resulting controller record.

Import success means that the file is structurally readable. Review and backend
request validation remain authoritative for whether the scenario can run.

## 6. Review Gate Before Running

The Run Simulation callback must first call:

```cpp
FText Reason;
if (!SimulationSubsystem->CanSimulateCurrentScenario(Reason))
{
    // Display Reason and route the user back to Review.
    return;
}
```

This checks that the current draft revision is exactly the revision accepted by
the Review panel. Any panel edit or imported replacement invalidates that
acceptance.

Review acceptance is necessary but not sufficient. External CSV files or a
controller DLL may have changed after Review. The runner compiles the prepared
scenario, and `SimulationEngine::Run` validates the resulting request before
propagation. Either stage may reject it.

## 7. Native In-Process Request Construction

`FTGScenarioDocumentAdapter::BuildSimulationRequest` is available to native
Unreal callers that need an in-process request. It is not the graphical
application's Run path, which exports a prepared `.tgscn` and launches the
runner as described in section 8. The helper is native C++, not a Blueprint
node, because `SimulationRequest` contains standard C++ containers and shared
pointers rather than reflected Unreal types.

```cpp
UGameInstance* GameInstance = GetGameInstance();
UTGSimulationSubsystem* Scenarios =
    GameInstance->GetSubsystem<UTGSimulationSubsystem>();
UTGControllerLibrarySubsystem* Controllers =
    GameInstance->GetSubsystem<UTGControllerLibrarySubsystem>();

const FTGSimulationScenario Scenario =
    Scenarios->GetCurrentScenarioDraft();

tgsim::SimulationRequest Request;
FString BuildMessage;

if (!FTGScenarioDocumentAdapter::BuildSimulationRequest(
        Scenario,
        Controllers,
        SourceScenarioFilePath,
        Request,
        BuildMessage))
{
    // Return to the game thread UI with BuildMessage.
    return;
}
```

The function performs the following operations:

1. Converts `FTGSimulationScenario` to `ScenarioDocument`.
2. Loads the packaged SPICE kernels.
3. Creates and attaches `FTGSpiceEphemerisProvider`.
4. Resolves the selected Unreal controller ID.
5. Loads one private controller DLL instance for this run when required.
6. Converts authored UTC values to SPICE ET/TDB.
7. Resolves component names and topologically orders the component tree.
8. Assigns contiguous variable-mass indices and deterministic flattened DOF
   order. `SimulationConfigBuilder` assigns articulation state offsets when the
   engine normalizes the completed request.
9. Parses propulsion, gravity, atmosphere, and aerodynamic CSV files.
10. Resolves relative paths against `SourceScenarioFilePath`.
11. Omits Unreal-only presentation and authoring-cache fields.
12. Returns a complete `tgsim::SimulationRequest`.

`SourceScenarioFilePath` should be the path of the `.tgscn` from which the
current draft was imported. For a HUD-created draft whose file-picker fields
are already absolute, it may be empty. Imported paths are currently expanded to
absolute paths, and retaining the source path preserves the resolution context for subsequent exports.

`BuildMessage` may contain warnings when the Boolean return value is true. Do
not interpret every nonempty message as failure.

## 8. Validating and Running the Backend

`UTGSimulationRunSubsystem` is the implemented packaged-app coordinator. It
creates an immutable reviewed snapshot, exports the exact run `.tgscn`, and
launches the generic `PHAROSScenarioRunner` as a hidden child process. The runner
loads SPICE and the selected controller, compiles `SimulationRequest`, validates
it, and calls synchronous `SimulationEngine::Run` in the isolated process.

The game thread only polls nonblocking process pipes and updates reflected
state/delegates. TGSimCore remains sequential; process isolation is not parallel
physics. It prevents a crashing or non-returning user controller from corrupting
or permanently occupying the Unreal process.

## 9. Progress and Cancellation

The runner's `ISimulationObserver` emits tab-delimited progress and log records
through standard output. Unreal sends `CANCEL` through standard input. The
backend polls that request during setup and integration; after a short grace
period Unreal may safely terminate the complete child process. The same process
termination enforces `MaximumWallClockRuntimeSeconds`, including when native
user-controller code never returns.

## 10. Receiving and Visualizing Results

`tgsim::SimulationResult` contains:

```cpp
bool success;
std::string message;
std::vector<tgsim::TelemetrySample> samples;
std::vector<std::string> column_names;
std::vector<std::vector<double>> solution_array;
```

These containers remain inside the process that runs TGSimCore. Native
callers can inspect `samples` directly:

- `sample.state` contains spacecraft CM state and propagated internal states;
- `sample.component_poses` contains each component pose relative to B;
- `sample.celestial_bodies` contains recorded ICRF body ephemerides;
- `sample.applied_force_torque` contains force/torque breakdowns;
- `sample.center_of_mass_body_m` and `sample.inertia_body_kgm2` contain the
  instantaneous assembled properties.

The runner serializes `column_names` and `solution_array` to CSV. The
graphical application reads that file for playback and inspection; it does not
share the runner's `SimulationResult` object.

After the runner finishes:

1. Verify successful process exit and a complete committed CSV.
2. Retain the exact `.tgscn`/CSV paths in the completed-run descriptor.
3. Broadcast the Blueprint completion delegate and travel to visualization.
4. Let the playback actor load and interpolate the committed CSV.
5. Convert meters to Unreal centimeters only in the visualization layer.
6. Shift barycentric ICRF positions by the selected visualization origin before
   converting to `FVector`; do not send astronomical absolute coordinates
   directly into the Unreal scene.
7. Apply the recorded body-to-ICRF attitude and component-to-body transforms.

Celestial-body states are already recorded in each sample so normal playback
does not need another concurrent SPICE query.

## 11. Run Subsystem API

`UTGSimulationRunSubsystem` is a separate `UGameInstanceSubsystem`, keeping
saved-draft ownership and transient process/run ownership independent. Its
Blueprint surface includes:

```cpp
UFUNCTION(BlueprintCallable)
bool StartCurrentSimulation(FText& OutError);

UFUNCTION(BlueprintCallable)
void CancelSimulation();

UFUNCTION(BlueprintPure)
bool IsSimulationRunning() const;

UFUNCTION(BlueprintPure)
float GetProgressPercent() const;

UPROPERTY(BlueprintAssignable)
FTGSimulationRunProgress OnRunProgress;

UPROPERTY(BlueprintAssignable)
FTGSimulationRunCompleted OnRunCompleted;
```

It also exposes run state/error getters, timeout/cancel failure events, latest
completed `.tgscn`/CSV paths, visualization travel, and configuration-return
travel. Native-only members own:

```text
child-process and pipe handles
current run paths and process log
monotonic timeout/cancellation deadlines
latest completed-run descriptor
```

`StartCurrentSimulation` should be the only function called by the HUD's Run
button, so Blueprint cannot omit Review, export, launch, or output checks.

## 12. Direct `.tgscn` Runs Inside Unreal

For a user-visible imported file, prefer this path:

```text
ImportScenarioFromTgscn
    -> SetCurrentScenarioDraft
    -> Review
    -> StartCurrentSimulation
    -> PHAROSScenarioRunner
    -> CompileScenario
    -> SimulationEngine::Run
```

This keeps the imported values visible and editable and applies the packaged
application's controller trust policy.

A native tool may instead use the lower-level backend path:

```text
LoadScenarioFile
    -> inspect ScenarioDocument
    -> attach SPICE/controller services
    -> CompileScenario
    -> ValidateRequest
    -> Run
```

`LoadScenarioFile` alone cannot produce a runnable request because a text file
cannot contain live `IEphemerisProvider` or `IController` objects. The caller
must attach those services between parsing and compilation. The standalone
`PHAROSScenarioRunner` is the working reference implementation of this lower-level
path.

## 13. Error Handling Contract

| Stage | Success indication | Diagnostic |
|---|---|---|
| Import/export | returned `bool` | `FText OutMessage` |
| Scenario compilation | returned `bool` | formatted scenario diagnostics in `FString OutMessage` |
| Request validation | empty string | nonempty validation-error string |
| Propagation | `SimulationResult.success` | `SimulationResult.message` |

Never discard a failure message. Never start propagation with a failed request.
Warnings may accompany a successful import or request build and should be shown
without treating the operation as failed.
