"""Rerun the complete retained chain with half the maximum integration step."""
from pathlib import Path
import json,shutil
import numpy as np
import continuous_chain as c
import mission_metrics as m
ROOT=c.ROOT;FINE=ROOT/'diagnostics/refinement'
def main():
    for sub in ['data','expected','tools']:(FINE/sub).mkdir(parents=True,exist_ok=True)
    for p in (ROOT/'data').glob('*.csv'):shutil.copy2(p,FINE/'data'/p.name)
    for name in ['burn_vectors.json','controller_tuning.json']:shutil.copy2(ROOT/'expected'/name,FINE/'expected'/name)
    for p in (ROOT/'tools').glob('Apollo*Controller.cpp'):shutil.copy2(p,FINE/'tools'/p.name)
    (FINE/'provenance/templates').mkdir(parents=True,exist_ok=True)
    shutil.copy2(ROOT/'provenance/templates/01.tgscn',FINE/'provenance/templates/01.tgscn')
    c.ROOT=FINE;c.STEP_FACTOR=.5
    for sid in c.SEGMENTS:c.run_segment(sid)
    coarse=m.load(ROOT);fine=m.load(FINE);result=[]
    for sid,a in coarse.items():
        b=fine[sid];assert len(a['rows'])==len(b['rows'])
        dr=[];dv=[];dm=[];dq=[]
        for x,y in zip(a['rows'],b['rows']):
            assert abs(float(x['elapsed_time_seconds'])-float(y['elapsed_time_seconds']))<1e-6
            p,v=m.rv(x);fp,fv=m.rv(y);dr.append(m.norm(p-fp));dv.append(m.norm(v-fv));dm.append(abs(float(x['mass_kg'])-float(y['mass_kg'])))
            q=np.array([float(x['quaternion_body_to_icrf_'+k]) for k in 'wxyz']);fq=np.array([float(y['quaternion_body_to_icrf_'+k]) for k in 'wxyz']);dq.append(min(m.norm(q-fq),m.norm(q+fq)))
        result.append({'segment':sid,'maximum_position_difference_m':max(dr),'endpoint_position_difference_m':dr[-1],'maximum_velocity_difference_mps':max(dv),'maximum_mass_difference_kg':max(dm),'maximum_quaternion_difference':max(dq)})
    output={'method':'Same output epochs, tolerances and controller settings; half the maximum internal step (10 s coasts / 0.125 s burns). Full chain rerun from the one original seed.','segments':result,'maximum_position_difference_m':max(x['maximum_position_difference_m'] for x in result),'pass':max(x['maximum_position_difference_m'] for x in result)<100,'criterion':'Under 100 m throughout the thirteen-phase mission to the pre-separation endpoint; much smaller than kilometer-scale historical reconstruction residuals.'}
    (ROOT/'analysis/step_refinement.json').write_text(json.dumps(output,indent=2),encoding='utf-8');print(json.dumps(output,indent=2))
    if not output['pass']:raise SystemExit(1)
if __name__=='__main__':main()
