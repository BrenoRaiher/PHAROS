"""Local step sensitivity from the identical retained incoming state.

This is deliberately separate from the continuous separation-to-Venus replay.
"""
import argparse,json
import numpy as np
from audit import load
from continuous_chain import ROOT,RUNNER,KERNELS,last,solution,scenario,write,process,sha

def main():
    p=argparse.ArgumentParser();p.add_argument('--segment',type=int,default=20);a=p.parse_args();sid=f'{a.segment:02d}'
    d=ROOT/'checks/local_half_step'/sid;seed=last(solution(f'{a.segment-1:02d}'));text=scenario(sid,seed,.5)
    text=text.replace('"../controllers/CassiniController.dll"','"'+(ROOT/'segments'/sid/'controllers/CassiniController.dll').as_posix()+'"')
    for kind in ['data','assets']:text=text.replace('"../../../'+kind+'/','"'+(ROOT/kind).as_posix()+'/')
    sc=d/'scenario'/f'cassini_{sid}.tgscn';write(sc,text);write(d/'seed_state.json',json.dumps(seed,indent=2))
    identities={'runner_sha256':sha(RUNNER),'scenario_sha256':sha(sc),'nominal_scenario_sha256':sha(ROOT/'segments'/sid/'scenario'/f'cassini_{sid}.tgscn'),'controller_sha256':sha(ROOT/'segments'/sid/'controllers/CassiniController.dll')}
    process([RUNNER,sc,'--kernel-dir',KERNELS,'--validate-only'],d/'logs/validate.log');process([RUNNER,sc,'--kernel-dir',KERNELS,'--output',d/'results'],d/'logs/run.log')
    columns=['ephemeris_time_tdb_seconds_past_j2000']+[f'position_icrf_{a}_m' for a in 'xyz']+[f'velocity_icrf_{a}_mps' for a in 'xyz']+['multibody_solve_succeeded']
    _,nominal=load(solution(sid),columns);_,refined=load(d/'results'/f'cassini_{sid}_solution.csv',columns)
    assert nominal.shape==refined.shape and np.array_equal(nominal[:,0],refined[:,0])
    dr=np.linalg.norm(nominal[:,1:4]-refined[:,1:4],axis=1);dv=np.linalg.norm(nominal[:,4:7]-refined[:,4:7],axis=1)
    result={'id':sid,**identities,'maximum_position_difference_m':float(dr.max()),'final_position_difference_m':float(dr[-1]),'maximum_velocity_difference_mps':float(dv.max()),'final_velocity_difference_mps':float(dv[-1]),'all_recorded_solves_succeeded':bool(np.all(nominal[:,-1]==1) and np.all(refined[:,-1]==1)),'same_initial_position_velocity':bool(np.array_equal(nominal[0,1:7],refined[0,1:7])),'method':'Local replay from the identical complete nominal incoming state, with maximum and initial integration steps halved. Output epochs, tolerances and controller are unchanged. This check does not include accumulated upstream mission sensitivity.'}
    write(d/'metadata.json',json.dumps(result,indent=2));write(ROOT/'analysis'/f'{sid}_local_refinement.json',json.dumps(result,indent=2));print(json.dumps(result),flush=True)

if __name__=='__main__':main()
