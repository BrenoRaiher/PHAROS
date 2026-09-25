
# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))

from pathlib import Path
import os
import argparse,csv,json,os
from continuous_chain import ROOT,KERNELS,VCVARS,write,process,solution,last

NAMES=['000331R_SK_LP0_V1P32.bsp','000331R_SK_V1P32_V2P12.bsp','000331R_SK_V2P12_EP15.bsp','010420R_SCPSE_EP1_JP83.bsp','040909R_SCPSE_01066_04199.bsp']
def kernels(sid):
    i=int(sid)
    indices=([0] if i<=2 else [0,1] if i==3 else [1] if i<=5 else [1,2] if i==6 else [2] if i<=10 else [3] if i<=12 else [3,4] if i==13 else [4])
    # Physical-moon ephemerides supersede copies in the reconstructed mission SPK;
    # DE442 then supplies the same planetary/SSB translation used by PHAROS.
    return [KERNELS/'naif0012.tls']+[ROOT/'sources/kernels'/NAMES[x] for x in indices]+[KERNELS/'jup230-short.bsp',KERNELS/'jup348.bsp',KERNELS/'sat252s.bsp',KERNELS/'de442.bsp']
def query(sid,out,extra,target='-82',observer='0'):
    args=[ROOT/'tools/SpiceReference.exe']
    for p in kernels(sid):args+=['--kernel',p]
    args+=['--target',target,'--observer',observer]+extra+['--output',out]
    process(args,out.with_suffix('.log'))
    write(out.with_suffix('.query.json'),json.dumps({'target':target,'observer':observer,'frame':'J2000','aberration':'NONE','kernel_load_order':[str(p.relative_to(_REPOSITORY)) for p in kernels(sid)],'arguments':[str(x) for x in extra]},indent=2))
def build():
    pharos=Path(os.environ.get('PHAROS_SOURCE', next(p for p in Path(__file__).resolve().parents if (p/'PHAROS.uproject').is_file()))); third=pharos/'Source/ThirdParty/CSPICE'
    d=ROOT/'tools/build';d.mkdir(parents=True,exist_ok=True)
    cmd=f'cl /nologo /std:c++17 /EHsc /O2 /MT /I"{third/"Include"}" /Fo"{d/"SpiceReference.obj"}" /Fe"{ROOT/"tools/SpiceReference.exe"}" "{ROOT/"tools/SpiceReference.cpp"}" "{third/"Lib/Win64/cspice.lib"}" /link oldnames.lib legacy_stdio_definitions.lib'
    write(d/'build.cmd',f'@echo off\ncall "{VCVARS}" >nul\nif errorlevel 1 exit /b %errorlevel%\n{cmd}\nexit /b %errorlevel%\n')
    process(['cmd.exe','/d','/c',d/'build.cmd'],d/'compile.log',d)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--build',action='store_true');p.add_argument('--dense',action='store_true');p.add_argument('--from',dest='first',type=int,default=1);p.add_argument('--to',type=int,default=20);a=p.parse_args()
    if a.build:build()
    specs=json.loads((ROOT/'provenance/segments.json').read_text())
    for spec in specs[a.first-1:a.to]:
        sid=spec['id'];out=ROOT/'expected'/f'{sid}_{"curve" if a.dense else "endpoints"}.csv'
        extra=['--states',solution(sid)] if a.dense else ['--utc',spec['start_utc'],'--utc',spec['end_utc']]
        query(sid,out,extra);print(f'Cassini reference {sid}',flush=True)
