from pathlib import Path
import os
os.environ.setdefault('PHAROS_VCVARS', r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat')
import subprocess
ROOT=Path(__file__).resolve().parents[1]
DEPENDENCY=(Path(os.environ.get('PHAROS_SOURCE', next(p for p in Path(__file__).resolve().parents if (p/'PHAROS.uproject').is_file())))/'Source/ThirdParty/CSPICE')
build=ROOT/'analysis/build';build.mkdir(exist_ok=True)
command=build/'inventory_build.cmd'
lines=['@echo off',r'call "%PHAROS_VCVARS%" >nul',
f'cl /nologo /std:c++17 /EHsc /O2 /MT /I"{DEPENDENCY/"Include"}" /Fo"{build/"SpkInventory.obj"}" /Fe"{ROOT/"analysis/SpkInventory.exe"}" "{ROOT/"analysis/SpkInventory.cpp"}" "{DEPENDENCY/"Lib/Win64/cspice.lib"}" /link oldnames.lib legacy_stdio_definitions.lib']
command.write_text('\n'.join(lines),encoding='utf-8')
subprocess.run(['cmd.exe','/d','/c',str(command)],cwd=build,check=True)
subprocess.run([str(ROOT/'analysis/SpkInventory.exe'),str(ROOT/'truth/kernels/jwst_rec.bsp'),str(ROOT/'truth/reference_audit/spk_segments.csv')],check=True)
