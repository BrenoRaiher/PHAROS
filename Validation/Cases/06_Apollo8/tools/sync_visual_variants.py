"""Attach inherited fixed massless CSM display parts to current physical scenarios."""

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))

from pathlib import Path
import copy,json,re,shutil,subprocess,tomllib,hashlib
ROOT=Path(__file__).resolve().parents[1]
def main():
    assets=ROOT/'00_Shared_Visualization_Assets'
    if not assets.is_dir():raise FileNotFoundError('Restore the packaged mission visualization assets before generating visual variants.')
    source=(ROOT/'provenance/templates/02_visual.tgscn').read_text(encoding='utf-8')
    starts=[m.start() for m in re.finditer(r'^\[\[components\]\]',source,re.M)]
    display=source[starts[2]:source.index('[[thrusters]]')]
    audit=[]
    for canonical in sorted((ROOT/'segments').glob('*/scenario/apollo8_like_??.tgscn')):
        text=canonical.read_text(encoding='utf-8');a=text.index('[[components]]');b=text.find('[[thrusters]]')
        if b<0:b=text.index('[control]')
        physical=''.join('[[components]]'+block.rstrip()+'\n\n[components.visual]\nvisible = false\n\n' for block in text[a:b].split('[[components]]')[1:])
        updated=text[:a]+physical+'# Fixed, massless display components; excluded from SRP.\n'+display+text[b:]
        visual=canonical.with_stem(canonical.stem+'_with_visual_appearance')
        original=tomllib.loads(text);parsed=tomllib.loads(updated);normal=copy.deepcopy(parsed)
        children=normal['components'][2:];assert len(children)==11
        for part in children:
            assert part['initial_mass_kg']==0 and not part['variable_mass'] and part['dofs']==[]
            assert all(v==0 for v in part['inertia'].values()) and part['srp']['included_in_proxy'] is False
            for key,value in part['visual'].items():
                if key.endswith('_file'):assert (visual.parent/value).resolve().is_file(),value
        normal['components']=normal['components'][:2]
        for part in normal['components']:part.pop('visual')
        assert normal==original
        visual.write_text(updated,encoding='utf-8')
        for scenario in [canonical,visual]:
            command=[str(_RUNTIME/'PHAROSScenarioRunner.exe'),str(scenario),'--kernel-dir',str(_KERNELS),'--validate-only']
            r=subprocess.run(command,capture_output=True,text=True,errors='replace')
            (canonical.parent.parent/'logs'/(scenario.stem+'_final_preflight.txt')).write_text(r.stdout+r.stderr,encoding='utf-8')
            assert r.returncode==0,scenario
        audit.append({'canonical':str(canonical.relative_to(ROOT)),'visual':str(visual.relative_to(ROOT)),'equal_physical_configuration':True,'fixed_massless_display_children':len(children),'preflights_pass':True})
    (ROOT/'analysis/visual_variant_audit.json').write_text(json.dumps(audit,indent=2),encoding='utf-8')
    (ROOT/'provenance/visual_asset_hashes.json').write_text(json.dumps({p.relative_to(assets).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in assets.rglob('*') if p.is_file()},indent=2),encoding='utf-8')
    print('Verified',len(audit),'visual variants and',2*len(audit),'current-format preflights.')
if __name__=='__main__':main()
