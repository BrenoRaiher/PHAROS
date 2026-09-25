"""Validate current authored inputs without propagating or scanning archives."""

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))

from pathlib import Path
import concurrent.futures
import hashlib
import json
import subprocess
import time
from run_campaign import ROOT,FEATURE

def check(path):
    rel=path.relative_to(ROOT).as_posix()
    log=ROOT/'Support/Provenance/preflight_logs'/(hashlib.sha256(rel.encode()).hexdigest()[:8]+'.txt')
    log.parent.mkdir(parents=True,exist_ok=True)
    start=time.monotonic()
    result=subprocess.run([str(_RUNTIME/'PHAROSScenarioRunner.exe'),str(path),'--kernel-dir',str(_KERNELS),'--validate-only'],cwd=ROOT,capture_output=True,text=True,errors='replace',timeout=90)
    log.write_text(result.stdout+result.stderr,encoding='utf-8')
    return {'scenario':rel,'scenario_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'exit_code':result.returncode,'wall_seconds':time.monotonic()-start,'log':log.relative_to(ROOT).as_posix()}

def main():
    paths=sorted((ROOT/'Cases/04_EarthOrbit/scenarios').glob('*.tgscn'))
    paths+=sorted((ROOT/FEATURE/'cases').rglob('*.tgscn'))
    paths+=sorted((ROOT/FEATURE/'diagnostics').rglob('*.tgscn'))
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results=list(pool.map(check,paths))
    (ROOT/'Support/Provenance/preflight.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
    print('Preflight:',sum(r['exit_code']==0 for r in results),'/',len(results),'accepted.')
    raise SystemExit(any(r['exit_code'] for r in results))

if __name__=='__main__': main()
