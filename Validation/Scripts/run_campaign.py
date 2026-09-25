"""Run the current campaign inputs with the frozen current PHAROS executable."""

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))

from pathlib import Path
import argparse
import concurrent.futures
import hashlib
import json
import subprocess
import time

ROOT=Path(__file__).resolve().parents[1]
FEATURE='Cases/01-03_CoreVerification'

def run_one(path, repeat=False):
    if path.parent.name=='scenarios':
        output=path.parent.parent/'results'/path.stem
    elif path.parent.name=='timekeeping_edge_cases':
        output=path.parent/'results'/path.stem
    else:
        output=path.parent/'results'
    if repeat:
        output=ROOT/FEATURE/'diagnostics/reaction_wheel_output_boundary/results'
    output.mkdir(parents=True,exist_ok=True)
    label=str(path.relative_to(ROOT)).replace('\\','/')+(' [repeat]' if repeat else '')
    start=time.monotonic()
    log=output/(path.stem+'_runner_log.txt')
    with log.open('w',encoding='utf-8') as stream:
        result=subprocess.run([str(_RUNTIME/'PHAROSScenarioRunner.exe'),str(path),
            '--kernel-dir',str(_KERNELS),'--output',str(output)],
            cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,timeout=1200)
    record={'scenario':str(path.relative_to(ROOT)).replace('\\','/'),'repeat':repeat,
        'exit_code':result.returncode,'wall_seconds':time.monotonic()-start,
        'output_directory':str(output.relative_to(ROOT)).replace('\\','/'),
        'runner_sha256':hashlib.sha256((_RUNTIME/'PHAROSScenarioRunner.exe').read_bytes()).hexdigest(),
        'scenario_sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
    print(('PASS run ' if result.returncode==0 else 'FAIL run ')+label+' '+f'{record["wall_seconds"]:.2f}s',flush=True)
    return record

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--group',choices=['features','earth','all'],default='all')
    args=parser.parse_args()
    paths=[]
    if args.group in ('features','all'):
        paths.extend(sorted((ROOT/FEATURE/'cases').rglob('*.tgscn')))
        paths.extend(sorted((ROOT/FEATURE/'diagnostics').rglob('*.tgscn')))
    if args.group in ('earth','all'):
        paths.extend(sorted(p for p in (ROOT/'Cases/04_EarthOrbit/scenarios').glob('*.tgscn') if '_with_visual_' not in p.stem))
    jobs=[(p,False) for p in paths]
    if args.group in ('features','all'):
        jobs.append((ROOT/FEATURE/'cases/02_actuated_multibody/02e_reaction_wheel/reaction_wheel.tgscn',True))
    records=[]
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        futures=[pool.submit(run_one,p,repeat) for p,repeat in jobs]
        for future in concurrent.futures.as_completed(futures):
            records.append(future.result())
            (ROOT/'Support/Provenance'/('runs_'+args.group+'.json')).write_text(json.dumps(records,indent=2),encoding='utf-8')
    print('Completed',len(records),'runs;',sum(r['exit_code']!=0 for r in records),'process failures.',flush=True)
    raise SystemExit(any(r['exit_code']!=0 for r in records))

if __name__=='__main__': main()
