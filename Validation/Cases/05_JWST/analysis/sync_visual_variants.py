"""Refresh presentation variants from current physical inputs, then preflight."""

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))

from pathlib import Path
import copy
import json
import re
import subprocess
import tomllib

ROOT = Path(__file__).resolve().parents[1]

def main():
    audit=[]
    for visual in sorted((ROOT/'phases').glob('*/*_with_visual_appearance.tgscn')):
        canonical=visual.with_name(visual.name.replace('_with_visual_appearance', ''))
        source=canonical.read_text(encoding='utf-8')
        old=visual.read_text(encoding='utf-8')
        old_component_starts=[m.start() for m in re.finditer(r'^\[\[components\]\]', old, re.M)]
        display=old[old_component_starts[2]:old.index('[[thrusters]]')]
        a=source.index('[[components]]'); b=source.index('[[thrusters]]')
        physical=source[a:b].split('[[components]]')[1:]
        assert len(physical)==2
        physical=''.join('[[components]]'+block.rstrip()+'\n\n[components.visual]\nvisible = false\n\n' for block in physical)
        updated=source[:a]+physical+'# Fixed, massless presentation children; excluded from SRP.\n'+display+source[b:]
        updated=updated.replace('name = "JWST CONTINUOUS ', 'name = "JWST CONTINUOUS VISUAL ', 1)
        parsed=tomllib.loads(updated); original=tomllib.loads(source)
        normalized=copy.deepcopy(parsed)
        normalized['scenario']['name']=original['scenario']['name']
        visible_children=normalized['components'][2:]
        assert len(visible_children)==18
        for component in visible_children:
            assert component['initial_mass_kg']==0 and not component['variable_mass']
            assert all(value==0 for value in component['inertia'].values())
            assert component['dofs']==[] and component['srp']['included_in_proxy'] is False
        normalized['components']=normalized['components'][:2]
        for component in normalized['components']:component.pop('visual')
        assert normalized==original, 'Visual input changes physical configuration'
        visual.write_text(updated, encoding='utf-8')
        for scenario in (canonical, visual):
            command=[str(_RUNTIME/'PHAROSScenarioRunner.exe'),str(scenario),'--kernel-dir',str(_KERNELS),'--validate-only']
            process=subprocess.run(command,cwd=ROOT,text=True,capture_output=True)
            log=scenario.parent/'results'/(scenario.stem+'_final_preflight.txt')
            log.write_text(process.stdout+'\n'+process.stderr,encoding='utf-8')
            assert process.returncode==0, str(log)
        audit.append({'canonical':str(canonical.relative_to(ROOT)), 'visual':str(visual.relative_to(ROOT)), 'physical_configuration_equal':True,'massless_fixed_display_children':18,'both_current_contract_preflights_pass':True})
    (ROOT/'analysis/visual_variant_audit.json').write_text(json.dumps(audit,indent=2),encoding='utf-8')
    print('Updated 8 visual variants; 16 current TGSCN preflights passed.')

if __name__=='__main__':main()
