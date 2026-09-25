# Corresponding Third-Party Sources

## Eigen

`Eigen-3.4.0.zip` contains the exact Eigen headers supplied
with Unreal Engine 5.7.4 and used to build PHAROS, together with their license,
Unreal's modification notes, and the supplied macro patch. These source files
remain under their individual notices and MPL-2.0 where applicable, not the
PHAROS MIT License. PHAROS makes no additional changes to these headers.

The archive excludes unused `unsupported` extensions and Unreal build-module
code. Providing these sources
alongside the executable makes the covered source available without requiring
an Unreal Engine account or an additional request.

Upstream: https://gitlab.com/libeigen/eigen/-/tree/3.4.0

## Controller Compiler

The bundled compiler is the x86-64 subset of llvm-mingw release `20260616`,
LLVM/Clang `22.1.8`. Binaries are unmodified; only unrelated tools and target
architectures are omitted. All retained headers and libraries keep their
upstream notices. The installer preserves the target `share` notice tree.

- Build scripts (ISC): https://github.com/mstorsjo/llvm-mingw/tree/20260616
- LLVM source: https://github.com/llvm/llvm-project/tree/llvmorg-22.1.8
- MinGW source, including runtime and winpthreads:
  https://github.com/mingw-w64/mingw-w64/tree/c28e9555bb8800c53449f42a465ad9a5676fce88
- Release archive: https://github.com/mstorsjo/llvm-mingw/releases/tag/20260616
- Expected archive SHA-256:
  `b9b68a4d276e16fa25802aaba458e4638f64b3884c290aaccdc2d87083b6ca35`

Full notices, including file-specific runtime exceptions, accompany PHAROS
in `Licenses/`. The LGPL text is retained for covered MinGW files; its
presence does not relicense PHAROS or other independent toolchain components.
