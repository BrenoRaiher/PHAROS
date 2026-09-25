"""Generate current-format appearance variants from the same continuous states."""
import argparse,json,time,tomllib
from continuous_chain import ROOT,RUNNER,KERNELS,write,scenario,last,solution,process

def sync(sid):
    previous=None if sid=='01' else last(solution(f'{int(sid)-1:02d}'))
    p=ROOT/'segments'/sid/'scenario'/f'cassini_{sid}_visual.tgscn';write(p,scenario(sid,previous,visual=True))
    a=tomllib.loads((p.parent/f'cassini_{sid}.tgscn').read_text());b=tomllib.loads(p.read_text())
    visuals=b['components'][2:]
    assert visuals and all(c['initial_mass_kg']==0 and c['minimum_mass_kg']==0 and not c['variable_mass'] and all(v==0 for v in c['inertia'].values()) and not c['dofs'] and not c['srp']['included_in_proxy'] for c in visuals)
    b['components']=b['components'][:2];b['scenario']['name']=a['scenario']['name']
    assert a==b,(sid,'Physical configuration mismatch')
    process([RUNNER,p,'--kernel-dir',KERNELS,'--validate-only'],p.parent.parent/'logs/validate_visual.log')
    write(p.parent.parent/'visual_verification.json',json.dumps({'id':sid,'massless_display_components':len(visuals),'excluded_from_srp_proxy':True,'physical_configuration_equal':True,'preflight_passed':True},indent=2))
    print(f'Visual variant {sid} validated',flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--watch',action='store_true');p.add_argument('--from',dest='first',type=int,default=1);p.add_argument('--to',type=int,default=20);a=p.parse_args()
    for i in range(a.first,a.to+1):
        sid=f'{i:02d}'
        if a.watch:
            while not (ROOT/'segments'/sid/'metadata.json').exists():time.sleep(5)
        sync(sid)
