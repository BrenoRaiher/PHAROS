
# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))

from pathlib import Path
import os
os.environ.setdefault('PHAROS_VCVARS', r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat')
import json
import subprocess

ROOT=Path(__file__).resolve().parents[1]
PROJECT=Path(os.environ.get('PHAROS_SOURCE', next(p for p in Path(__file__).resolve().parents if (p/'PHAROS.uproject').is_file())))
SOURCE=ROOT/'Support/Provenance/current_source'
BUILD=_RUNTIME/'verification_build'
TESTS=ROOT/'Cases/01-03_CoreVerification/cases/01_backend_verification'
UE=Path(os.environ.get('UE_ENGINE_ROOT', r'C:\Program Files\Epic Games\UE_5.7'))
if UE.name != 'Engine': UE=UE/'Engine'

def main():
    BUILD.mkdir(parents=True,exist_ok=True)
    (BUILD/'obj').mkdir(exist_ok=True)
    TESTS.mkdir(parents=True,exist_ok=True)
    fixtures=TESTS/'controller_contract'
    fixtures.mkdir(exist_ok=True)
    # Fixtures are retained locally. The two old DLLs are deliberate negative
    # contract fixtures, never controllers for propagated scenarios.
    for name in ('CurrentFixture.cpp','MismatchFixture.cpp','ControllerContractChecks.cpp','LegacyV3.dll','LegacyV4.dll'):
        if not (fixtures/name).is_file():
            raise FileNotFoundError(fixtures/name)

    include=[SOURCE/'Source/TGSimCore/include',PROJECT/'Source/ThirdParty/tomlplusplus',
        UE/'Source/ThirdParty/Eigen',UE/'Source/ThirdParty/Boost/Deploy/boost-1.85.0/include',
        UE/'Source/ThirdParty/nanoflann/1.4.2/include',SOURCE/'ControllerSDK',SOURCE/'Tools/TGScenarioRunner']
    options=['/nologo','/std:c++17','/EHsc','/O2','/MT','/W4','/wd4100','/wd4127','/wd4244','/wd4267','/wd4324','/wd4702']+[f'/I"{p}"' for p in include]
    sources=sorted((SOURCE/'Source/TGSimCore/src').rglob('*.cpp'))
    rsp=BUILD/'core.rsp'
    rsp.write_text('\n'.join(options+['/c',f'/Fo"{BUILD / "obj"}\\\\"']+[f'"{p}"' for p in sources]),encoding='utf-8')
    script=['@echo off','call "%PHAROS_VCVARS%" >nul','if errorlevel 1 exit /b %errorlevel%',
        f'cl @"{rsp}" > "{BUILD / "core_build.log"}" 2>&1','if errorlevel 1 exit /b %errorlevel%',
        f'lib /nologo /OUT:"{BUILD / "TGSimCore.lib"}" "{BUILD / "obj"}\\*.obj" > "{BUILD / "library.log"}" 2>&1','if errorlevel 1 exit /b %errorlevel%']
    targets=[('TGSimCoreValidation','TGSIM_STANDALONE_VALIDATION',SOURCE/'Source/TGSimCore/tests/TGSimCoreValidationMain.cpp'),
             ('TGScenarioRoundTripTest','TGSIM_STANDALONE_SCENARIO_TEST',SOURCE/'Source/TGSimCore/tests/TGScenarioRoundTripMain.cpp'),
             ('ControllerContractChecks','TGSIM_CONTROLLER_CONTRACT_TEST',fixtures/'ControllerContractChecks.cpp')]
    for name,macro,mainfile in targets:
        target_rsp=BUILD/(name+'.rsp')
        args=options+[f'/D{macro}=1',f'/Fo"{BUILD / (name+".obj")}"',f'/Fe"{TESTS / (name+".exe")}"',f'"{mainfile}"']
        if name=='ControllerContractChecks':
            args += [f'"{SOURCE / "Tools/TGScenarioRunner/StandaloneControllerAdapter.cpp"}"']
            # Multiple source files require a directory for /Fo.
            args=[x for x in args if not x.startswith('/Fo')]+[f'/Fo"{BUILD}\\\\"']
        args += [f'"{BUILD / "TGSimCore.lib"}"']
        target_rsp.write_text('\n'.join(args),encoding='utf-8')
        script += [f'cl @"{target_rsp}" > "{BUILD / (name+"_build.log")}" 2>&1','if errorlevel 1 exit /b %errorlevel%']
    for name in ('CurrentFixture','MismatchFixture'):
        fixture_rsp=BUILD/(name+'.rsp')
        fixture_rsp.write_text('\n'.join(options+['/LD','/DTG_CONTROLLER_BUILD=1',f'/Fo"{BUILD / (name+".obj")}"',f'/Fe"{fixtures / (name+".dll")}"',f'"{fixtures / (name+".cpp")}"']),encoding='utf-8')
        script += [f'cl @"{fixture_rsp}" > "{BUILD / (name+"_build.log")}" 2>&1','if errorlevel 1 exit /b %errorlevel%']
    script += ['echo Fresh backend, scenario, and controller-contract tests built.','exit /b 0']
    cmd=BUILD/'build.cmd'
    cmd.write_text('\n'.join(script),encoding='utf-8')
    result=subprocess.run(['cmd.exe','/d','/c',str(cmd)],cwd=BUILD)
    if result.returncode: raise SystemExit(result.returncode)
    status=[]
    for name,log in [('TGSimCoreValidation','backend_validation_log.txt'),('TGScenarioRoundTripTest','scenario_roundtrip_log.txt'),('ControllerContractChecks','controller_contract_log.txt')]:
        args=[str(TESTS/(name+'.exe'))]
        if name=='ControllerContractChecks': args += ['LegacyV3.dll','LegacyV4.dll']
        with (TESTS/log).open('w',encoding='utf-8') as stream:
            r=subprocess.run(args,cwd=fixtures if name=='ControllerContractChecks' else TESTS,stdout=stream,stderr=subprocess.STDOUT)
        status.append({'executable':name,'exit_code':r.returncode,'log':log})
        print(name+': exit '+str(r.returncode),flush=True)
    (TESTS/'test_status.json').write_text(json.dumps(status,indent=2),encoding='utf-8')
    raise SystemExit(any(s['exit_code'] for s in status))

if __name__=='__main__': main()
