# Bundled Controller Toolchain

The packaged Win64 application expects a self-contained LLVM-MinGW toolchain at:

```text
Build/ControllerToolchain/Win64/
```

Its compiler entry point is:

```text
bin/x86_64-w64-mingw32-clang++.exe
```

`TG.Build.cs` stages this directory beside the packaged executable as
`ControllerToolchain`. The runtime also accepts `PHAROS_CONTROLLER_COMPILER`
(and its alias `TG_CONTROLLER_COMPILER`) and a
GNU-compatible `clang++.exe` or `g++.exe` on `PATH` for development builds.

The distributable toolchain must include its upstream license and notice files.
Controller DLLs are built with a C ABI and static runtime linkage; C++ objects,
allocations, exceptions, file handles, and standard-library types never cross
the DLL boundary.

Run `Install-LLVMMinGW.ps1` from PowerShell to download the pinned stable
official archive, verify its SHA-256 digest, install it into `Win64`, and remove
non-x86-64 compilers, debuggers, Python tools, and target libraries that PHAROS does
not package.
