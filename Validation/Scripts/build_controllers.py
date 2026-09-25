"""Rebuild the five active campaign DLLs against the frozen current SDK."""

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))

from pathlib import Path
import argparse
import subprocess
from run_campaign import ROOT,FEATURE

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--vcvars',type=Path,default=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'))
    args=parser.parse_args()
    build=_RUNTIME/'controller_build'
    build.mkdir(parents=True,exist_ok=True)
    sources=[ROOT/'Cases/04_EarthOrbit/controllers/EarthFourWheelRecoveryController.cpp',ROOT/'Cases/04_EarthOrbit/controllers/EarthThrusterRecoveryController.cpp',ROOT/FEATURE/'cases/02_actuated_multibody/controllers/ActuatorValidationController.cpp',ROOT/FEATURE/'diagnostics/scheduler_edge_audit/EdgeAuditController.cpp',ROOT/FEATURE/'cases/additional_features/controllers/DischargeController.cpp']
    lines=['@echo off',f'call "{args.vcvars}" >nul','if errorlevel 1 exit /b %errorlevel%']
    for source in sources:
        lines.extend([f'cl /nologo /std:c++17 /EHsc /O2 /MT /LD /DTG_CONTROLLER_BUILD=1 /I"{_RUNTIME/"ControllerSDK"}" /I"{source.parent}" /Fo"{build/(source.stem+".obj")}" /Fe"{source.with_suffix(".dll")}" "{source}" > "{build/(source.stem+".log")}" 2>&1','if errorlevel 1 exit /b %errorlevel%'])
    lines.append('exit /b 0')
    script=build/'build_current.cmd'
    script.write_text('\n'.join(lines),encoding='utf-8')
    result=subprocess.run(['cmd.exe','/d','/c',str(script)],cwd=build)
    print('Current controller build exit:',result.returncode)
    raise SystemExit(result.returncode)

if __name__=='__main__': main()
