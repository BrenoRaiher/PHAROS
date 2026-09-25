# Contributing to PHAROS

Contributions are welcome across the Physics Core, graphical application,
controllers, scenarios, analysis tools, and documentation. Bug reports and
reproducible comparisons are useful contributions too.

## Find the right starting point

| Contribution | Source and guidance |
|---|---|
| Dynamics, physical models, or integration | [Physics Core](Source/TGSimCore/) and [architecture](Docs/Development/PhysicsCore.md) |
| Graphical application and playback | [Application source](Source/TG/) and [integration guide](Docs/Development/ApplicationIntegration.md) |
| Scenario format or command-line execution | [Scenario format](Docs/Reference/TGSCN.md) and [runner](Tools/TGScenarioRunner/) |
| User controllers | [Controller SDK](ControllerSDK/README.md) and its template |
| Verification, mission comparisons, or plots | [Assessment cases](Validation/README.md) and [reproduction guide](Docs/GettingStarted/Reproduce.md) |
| Documentation | [Documentation index](Docs/README.md) |

## Set up your working copy

1. Clone the repository if you have write access, or fork it and clone your
   fork. Install Git LFS before cloning, then fetch the binary assets with
   `git lfs pull`.
2. Follow the [build guide](Docs/GettingStarted/Build.md) for the component you
   plan to change. The Physics Core has a standalone CMake build; application
   development uses Unreal Engine. The [Visual Studio configuration](.vsconfig)
   lists the development components that can be imported into its installer.
3. Use a focused branch for your work, for example:

   ```powershell
   git switch -c fix/scenario-round-trip
   ```

The full recorded histories and frozen tools are optional. Use the reproduction
guide when your change needs those records, and perform reruns in a working
copy so the original evidence remains available for comparison.

## Report bugs and propose improvements

Search the [issue tracker](https://github.com/BrenoRaiher/PHAROS/issues) before
opening a new report. For a bug, include:

- The PHAROS release or commit and whether you used the runner, editor, or
  packaged application.
- The smallest reproducible scenario, relevant controller code, and steps to
  reproduce the behavior.
- What you expected, what happened, and relevant log excerpts. For numerical
  discrepancies, identify the reference calculation and propagation settings.
- Relevant build and system details; the [support guide](Docs/Release/SUPPORT.md)
  lists useful diagnostics.

For a feature or substantial design change, describe the problem it solves,
the proposed behavior, and any effect on existing scenarios or interfaces.
An issue or draft pull request is a useful place to coordinate this work.
Small corrections can go directly into a contribution.

Follow [SECURITY.md](SECURITY.md) for security reports, and remove private data
from public examples and logs.

## Make a focused change

- Follow the naming and formatting of the surrounding code. Keep unrelated
  refactoring or formatting changes separate.
- Keep the Physics Core independent of Unreal types and scene operations.
  Preserve its documented units, reference frames, time convention, and
  state definitions, or explicitly document an intentional change.
- For physics or numerical changes, state the governing relation, assumptions,
  and expected response. Use a reproducible reference comparison to show the
  effect of the change.
- Document changes to the scenario schema, controller interface, or output
  format, including any migration or controller rebuild needed. Update the
  affected guides and templates alongside the implementation.
- Keep scenarios portable with relative resource paths. Follow the existing
  Git LFS rules for binary assets and keep generated builds, caches, and local
  simulation output out of commits.

Tie new numerical results to their source commit, inputs, and calculation
settings. Identify corrections and new results separately from existing
records; updating code does not retroactively verify it against the recorded
campaign.

## Check your change

Run the lightweight repository check from the repository root with Python
3.11 or newer available as `python`:

```powershell
.\Tools\Repository\Test-PublicRepository.ps1
```

This is the check run by the repository's GitHub Actions workflow. It checks
source-tree structure, Python syntax, and scenario syntax and resource paths.
Use the additional checks appropriate to your change:

| Changed area | Relevant verification |
|---|---|
| Physics Core | Build and run `TGSimCoreValidation`; exercise the affected model with a focused reference comparison. |
| Scenario parsing or serialization | Build and run `TGScenarioRoundTripTest`; check representative scenarios with the runner's `--validate-only` option. |
| Controller interface | Rebuild affected controller DLLs against the SDK and exercise their commands in a representative scenario. |
| Application or playback | Build the application and check the affected interaction; include a screenshot when it helps demonstrate the result. |
| Analysis or plotting | Run the affected analysis on identified histories and inspect the resulting values or figures. |
| Documentation | Check links, commands, and formatting against the implementation. |

The native executables are targets in
[CMakeLists.txt](Source/TGSimCore/CMakeLists.txt). Use a build with assertions
enabled, such as Debug, for the scenario round-trip checks. Report the commands
you ran and their results, including relevant checks you could not perform.
Long mission reconstructions are appropriate when the change warrants them;
ordinary documentation edits do not require a new simulation campaign.

## Submit the contribution

Before submitting or merging, check that:

- [ ] The change has a clear purpose and includes the affected documentation.
- [ ] Relevant checks and their outcomes are recorded.
- [ ] Any compatibility change and any new scientific results are identified.
- [ ] New commits carry your sign-off, as described below.
- [ ] Added dependencies or assets retain their licenses and attribution.

Use a descriptive commit message explaining the change. A pull request should
state the problem, the resulting behavior, and how it was checked; link an
existing issue when applicable.

Contributors without write access submit pull requests from their forks.
Collaborators with write access may push changes and merge pull requests
without the original author's approval. The checklist applies to either path;
the [collaboration guidelines](.github/GOVERNANCE.md) describe the shared
maintenance responsibilities.

## Contribution origin and attribution

Submit first-party contributions under the existing [MIT License](LICENSE).
Contributors retain their copyright; no copyright assignment is requested.
Preserve third-party terms and record new dependencies or assets in the
[notices and provenance records](Docs/Legal/ASSET_PROVENANCE.md).

For new contributions, read and certify the
[Developer Certificate of Origin 1.1](https://developercertificate.org/) by
adding your own sign-off:

```powershell
git commit -s
```

This adds a `Signed-off-by` trailer using your Git identity. Use a name and
email address you can publish; a GitHub noreply address is an option. Sign-offs
remain in the public commit history. Sign only for yourself and preserve
others' authorship and applicable sign-offs when incorporating their work.
Existing history does not need to be rewritten.

The sign-off declares the origin and submission rights of your contribution;
it is distinct from cryptographic commit signing.

For source publication or packaged releases, follow the
[publication guide](Docs/Release/PUBLISHING.md), including its separate file,
licensing, and release checks.
