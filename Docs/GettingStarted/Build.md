# Building PHAROS

## Requirements

- Windows x64 and Unreal Engine 5.7.4, installed under its own license.
- Visual Studio with the C++ workloads and components in `.vsconfig`.
- Git and Git LFS. Run `git lfs install` before cloning, then `git lfs pull`.
- Binary assets and SPICE kernels must be present, not LFS pointer files.

## First Checkout

Run `git lfs pull` to obtain the binary assets and complete kernel set,
including `Content/SPICEKernels/ura111.bsp`, before running the editor or backend.
`Tools/Release/InstallExternalKernels.ps1` can verify that kernel against the
pinned SHA-256 or restore a missing copy directly from NASA/JPL SSD. It leaves
an existing matching file untouched and refuses to overwrite a different file.

## Editor

Generate Visual Studio project files from `PHAROS.uproject`. Build
`PHAROSEditor` for Win64 Development, then open the project in Unreal Editor.
The internal `TG` and `TGSimCore` module names are stable asset identifiers.

Unreal generates the solution, `Binaries`, `Intermediate`, `DerivedDataCache`,
and `Saved` locally. They are deliberately excluded from Git. `Saved` may
contain private scenarios, controller source, browser caches, and crash data.

## Standalone Backend

Run `Tools\TGScenarioRunner\BuildStandaloneRunner.bat` from an x64 Visual
Studio developer terminal. The script can also discover Visual Studio using
`vswhere`. Set `UE_ENGINE_ROOT` to the engine's `Engine` directory when it is
installed outside the default location.

For CMake, see `Source/TGSimCore/README.md` and
`Source/TGSimCore/CMakeLists.txt`. Supply Eigen, Boost, nanoflann, and CSPICE
through the documented `TGSIM_*` cache variables. The Unreal installation
supplies the tested Eigen 3.4.0, Boost 1.85.0, and nanoflann 1.4.2 headers.

## Controller Compiler

Run `Build\ControllerToolchain\Install-LLVMMinGW.ps1` to install the pinned
compiler used for native user controllers. The script verifies the download
checksum. Its generated `Win64` directory is intentionally excluded from Git
and is included in a Shipping package by the Unreal build rules.

## Shipping

1. Build the standalone runner so the package receives the current backend.
2. Install the controller compiler and build `PHAROS` for Win64 Shipping.
3. Package Windows from Unreal Editor. The startup and visualization maps,
   runtime widgets, SDK, kernels, legal bundle, and release documents are
   configured by the project.
4. Run `Tools\Release\PrepareWindowsPackage.ps1 -PackageRoot <Windows-folder>`.
   This prepares root-level notices, documentation, checksums, and PHAROS
   launcher metadata. Run it before signing the executables.
5. Run `Tools\Release\Test-Publication.ps1 -PackageRoot <Windows-folder>` and
   complete `Docs/Release/CLEAN_MACHINE_TEST_MATRIX.md` with the actual build.

Packaging is a separate operation; building source does not publish or upload
anything. Installer creation and certificate signing are separate release steps.

## Source Publication

Run `Tools\Release\Test-Publication.ps1` before staging changes for Git.
Review `git status` and the first commit, including LFS files. See
`Docs/Release/PUBLISHING.md` for the intended repository contents and any
remaining asset-permission decisions. Preserve copyright and third-party
notices in all copies.
