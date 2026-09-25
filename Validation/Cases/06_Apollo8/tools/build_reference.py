from pathlib import Path
import os
import subprocess
ROOT=Path(__file__).resolve().parents[1]
PROJECT=Path(os.environ.get('PHAROS_SOURCE', next(p for p in Path(__file__).resolve().parents if (p/'PHAROS.uproject').is_file())))
VCVARS=Path(os.environ.get('PHAROS_VCVARS', r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'))
build=ROOT/'tools/build';build.mkdir(exist_ok=True)
lines=['@echo off',f'call "{VCVARS}" >nul','if errorlevel 1 exit /b %errorlevel%']
for name in ['BodyReference','apollo_event_converter_frames']:
    source=ROOT/'tools'/f'{name}.cpp'
    lines += [f'cl /nologo /std:c++17 /EHsc /O2 /MT /I"{PROJECT/"Source/ThirdParty/CSPICE/Include"}" /Fo"{build/(name+".obj")}" /Fe"{ROOT/"tools"/(name+".exe")}" "{source}" "{PROJECT/"Source/ThirdParty/CSPICE/Lib/Win64/cspice.lib"}" /link oldnames.lib legacy_stdio_definitions.lib > "{build/(name+".log")}" 2>&1','if errorlevel 1 exit /b %errorlevel%']
lines+=['exit /b 0'];path=build/'build.cmd';path.write_text('\n'.join(lines),encoding='utf-8')
result=subprocess.run(['cmd.exe','/d','/c',str(path)],cwd=build)
print('Independent reference tools build exit:',result.returncode)
raise SystemExit(result.returncode)
