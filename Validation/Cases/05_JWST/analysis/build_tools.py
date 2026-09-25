"""Build current-contract mission controllers and independent CSPICE extractor."""

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))

from pathlib import Path
import os
import argparse
import subprocess

ROOT=Path(__file__).resolve().parents[1]
PROJECT=Path(os.environ.get('PHAROS_SOURCE', next(p for p in Path(__file__).resolve().parents if (p/'PHAROS.uproject').is_file())))

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--vcvars',type=Path,default=Path(os.environ.get('PHAROS_VCVARS', r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat')))
    args=parser.parse_args()
    build=ROOT/'analysis/build';build.mkdir(exist_ok=True)
    lines=['@echo off',f'call "{args.vcvars}" >nul','if errorlevel 1 exit /b %errorlevel%']
    for source in sorted((ROOT/'controllers').glob('*.cpp')):
        lines += [f'cl /nologo /std:c++17 /EHsc /O2 /MT /LD /DTG_CONTROLLER_BUILD=1 /I"{_RUNTIME/"ControllerSDK"}" /I"{source.parent}" /Fo"{build/(source.stem+".obj")}" /Fe"{source.with_suffix(".dll")}" "{source}" > "{build/(source.stem+".log")}" 2>&1','if errorlevel 1 exit /b %errorlevel%']
    source=ROOT/'analysis/SpiceReference.cpp'
    lines += [f'cl /nologo /std:c++17 /EHsc /O2 /MT /I"{PROJECT/"Source/ThirdParty/CSPICE/Include"}" /Fo"{build/"SpiceReference.obj"}" /Fe"{ROOT/"analysis/SpiceReference.exe"}" "{source}" "{PROJECT/"Source/ThirdParty/CSPICE/Lib/Win64/cspice.lib"}" /link oldnames.lib legacy_stdio_definitions.lib > "{build/"SpiceReference.log"}" 2>&1','if errorlevel 1 exit /b %errorlevel%','exit /b 0']
    cmd=build/'build.cmd';cmd.write_text('\n'.join(lines),encoding='utf-8')
    r=subprocess.run(['cmd.exe','/d','/c',str(cmd)],cwd=build)
    print('JWST controller and reference-tool build exit:',r.returncode,flush=True)
    raise SystemExit(r.returncode)

if __name__=='__main__':main()
