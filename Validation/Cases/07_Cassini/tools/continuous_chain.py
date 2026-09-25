"""Rebuild and propagate the accepted Cassini sequence with current PHAROS."""

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))

from pathlib import Path
import argparse,csv,hashlib,json,os,re,subprocess,time,tomllib
ROOT=Path(__file__).resolve().parents[1]
RUNTIME=_RUNTIME
RUNNER=RUNTIME/'PHAROSScenarioRunner.exe'
KERNELS=_KERNELS
SDK=RUNTIME/'ControllerSDK'
VCVARS=Path(os.environ.get('PHAROS_VCVARS',r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'))
RTOL=float(os.environ.get('CASSINI_RELATIVE_TOLERANCE','1e-14'))
ATOL=float(os.environ.get('CASSINI_ABSOLUTE_TOLERANCE','1e-8'))

def write(p,s):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(s,encoding='utf-8',newline='\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def last(p):
    with p.open('rb') as f:
        h=f.readline().decode('utf-8-sig').strip();f.seek(0,2);n=f.tell();f.seek(max(0,n-100000));lines=f.read().decode().splitlines()
    return dict(zip(next(csv.reader([h])),next(csv.reader([lines[-1]]))))
def fmt(v):return '['+', '.join(format(float(x),'.17g') for x in v)+']'
def process(args,log,cwd=None):
    log.parent.mkdir(parents=True,exist_ok=True)
    with log.open('w',encoding='utf-8') as f:r=subprocess.run([str(x) for x in args],cwd=cwd,stdout=f,stderr=subprocess.STDOUT)
    if r.returncode:raise RuntimeError(f'{args[0]} failed {r.returncode}: {log.read_text()[-5000:]}')
def compile(sid):
    d=ROOT/'segments'/sid/'controllers';src=d/'CassiniController.cpp';dll=d/'CassiniController.dll'
    cmd=f'cl /nologo /std:c++17 /EHsc /O2 /MT /LD /I"{SDK}" /I"{d}" /Fo"{d/"CassiniController.obj"}" /Fe"{dll}" "{src}"'
    write(d/'build.cmd',f'@echo off\ncall "{VCVARS}" >nul\nif errorlevel 1 exit /b %errorlevel%\n{cmd}\nexit /b %errorlevel%\n')
    process(['cmd.exe','/d','/c',d/'build.cmd'],d/'compile.log',d)
def solution(sid,base=None):return (base or ROOT/'segments')/sid/'results'/f'cassini_{sid}_solution.csv'
def setkey(text,key,value):
    text,n=re.subn(r'(?m)^'+re.escape(key)+r'\s*=.*$',lambda m:key+' = '+value,text)
    assert n==1,(key,n)
    return text
def scenario(sid,previous=None,factor=1,visual=False):
    t=(ROOT/'provenance/templates'/f'{sid}{"_visual" if visual else ""}.tgscn').read_text(encoding='utf-8-sig')
    if visual:
        # The final legacy appearance template uses different physical-component
        # labels. Match stable IDs and rename every named reference consistently.
        physical=tomllib.loads((ROOT/'provenance/templates'/f'{sid}.tgscn').read_text(encoding='utf-8-sig'))
        names={c['id']:c['name'] for c in physical['components']}
        for c in tomllib.loads(t)['components']:
            if c['id'] in names and c['name']!=names[c['id']]:
                t=t.replace('"'+c['name']+'"','"'+names[c['id']]+'"')
    t=re.sub(r'(?m)^format_version\s*=.*\n','',t)
    t=setkey(t,'generator','"Current PHAROS Cassini reconstruction, September 2026"')
    t=setkey(t,'controller_dll_file','"../controllers/CassiniController.dll"')
    def relocate(m):
        key,path=m.groups()
        return key+' = "../../../'+('data' if key=='harmonic_model_csv_file' else 'assets')+'/'+Path(path).name+'"'
    t=re.sub(r'(?m)^((?:harmonic_model_csv|stl|[a-z_]*texture)_file)\s*=\s*"([^"]+)"',relocate,t)
    cfg=tomllib.loads(t)
    t=setkey(t,'maximum_integrator_step_seconds',str(cfg['scenario']['maximum_integrator_step_seconds']*factor))
    t=setkey(t,'initial_integrator_step_seconds',str(min(cfg['scenario']['initial_integrator_step_seconds']*factor,cfg['scenario']['maximum_integrator_step_seconds']*factor)))
    t=setkey(t,'relative_tolerance',str(RTOL))
    t=setkey(t,'absolute_tolerance',str(ATOL))
    if previous:
        mappings={'position_icrf_m':[f'position_icrf_{a}_m' for a in 'xyz'],'velocity_icrf_mps':[f'velocity_icrf_{a}_mps' for a in 'xyz'],'attitude_body_to_icrf':[f'quaternion_body_to_icrf_{a}' for a in 'wxyz'],'angular_velocity_body_radps':[f'angular_velocity_body_{a}_radps' for a in 'xyz']}
        for key,cols in mappings.items():t=setkey(t,key,fmt([previous[c] for c in cols]))
        blocks=re.split(r'(?m)(?=^\[\[components\]\])',t)
        matches=0
        for n,b in enumerate(blocks):
            if 'name = "Cassini propellant inventory"' not in b:continue
            end=b.find('\n[',2);comp=b if end<0 else b[:end];rest='' if end<0 else b[end:]
            comp=setkey(comp,'initial_mass_kg',format(float(previous['variable_component_mass_0_kg']),'.17g'))
            fields=['xx','yy','zz','xy','xz','yz'];dry=[5046,12249,12249,0,0,0]
            tensor='{ '+', '.join('i'+a+'_kgm2 = '+format(float(previous[f'inertia_body_{a}_kgm2'])-d,'.17g') for a,d in zip(fields,dry))+' }'
            comp=setkey(comp,'inertia',tensor);blocks[n]=comp+rest;matches+=1
        assert matches==1;t=''.join(blocks)
        i=iter(range(3))
        t,n=re.subn(r'(?m)^initial_momentum_nms\s*=.*$',lambda m:'initial_momentum_nms = '+format(float(previous[f'reaction_wheel_momentum_{next(i)}_nms']),'.17g'),t)
        assert n==3
    tomllib.loads(t)
    return t
def run(sid,base=None,factor=1):
    base=base or ROOT/'segments';d=base/sid
    previous=last(solution(f'{int(sid)-1:02d}',base)) if sid!='01' else None
    t=scenario(sid,previous,factor)
    if base!=ROOT/'segments':
        t=t.replace('"../controllers/CassiniController.dll"','"'+(ROOT/'segments'/sid/'controllers/CassiniController.dll').as_posix()+'"')
        for kind in ['data','assets']:t=t.replace('"../../../'+kind+'/','"'+(ROOT/kind).as_posix()+'/')
    p=d/'scenario'/f'cassini_{sid}.tgscn';write(p,t)
    (d/'metadata.json').unlink(missing_ok=True)
    identities={'runner_sha256':sha(RUNNER),'scenario_sha256':sha(p),'controller_sha256':sha(ROOT/'segments'/sid/'controllers/CassiniController.dll'),'controller_source_sha256':sha(ROOT/'segments'/sid/'controllers/CassiniController.cpp'),'controller_configuration_sha256':sha(ROOT/'segments'/sid/'controllers/ControllerConfig.h')}
    common=[RUNNER,p,'--kernel-dir',KERNELS]
    print(f'Running Cassini {sid} step factor {factor}',flush=True);start=time.monotonic()
    process(common+['--validate-only'],d/'logs/validate.log')
    process(common+['--output',d/'results'],d/'logs/run.log')
    row=last(solution(sid,base))
    write(d/'metadata.json',json.dumps({'id':sid,'external_initial_state':sid=='01','previous_segment':None if sid=='01' else f'{int(sid)-1:02d}',**identities,'maximum_step_factor':factor,'wall_seconds':time.monotonic()-start},indent=2))
    write(d/'final_state.json',json.dumps(row,indent=2))
    print(f'Completed Cassini {sid} in {time.monotonic()-start:.1f} s; mass {float(row["mass_kg"]):.3f} kg',flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--from',dest='first',type=int,default=1);p.add_argument('--to',type=int,default=20);p.add_argument('--compile',action='store_true');p.add_argument('--factor',type=float,default=1);p.add_argument('--base',type=Path);args=p.parse_args()
    for i in range(args.first,args.to+1):
        sid=f'{i:02d}'
        if args.compile:compile(sid)
        run(sid,args.base,args.factor)
