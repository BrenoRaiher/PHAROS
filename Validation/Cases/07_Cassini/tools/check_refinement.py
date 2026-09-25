"""Compare equal-output-epoch independent continuous chains."""
import argparse,json,time
import numpy as np
from audit import load
from continuous_chain import ROOT,solution,write

def inspect(sid):
    columns=['ephemeris_time_tdb_seconds_past_j2000']+[f'position_icrf_{x}_m' for x in 'xyz']+[f'velocity_icrf_{x}_mps' for x in 'xyz']+['multibody_solve_succeeded']
    _,a=load(solution(sid),columns);_,b=load(solution(sid,ROOT/'checks/half_step'),columns)
    assert a.shape==b.shape and np.array_equal(a[:,0],b[:,0])
    dp=np.linalg.norm(a[:,1:4]-b[:,1:4],axis=1);dv=np.linalg.norm(a[:,4:7]-b[:,4:7],axis=1)
    result={'id':sid,'rows':len(a),'same_recorded_epochs':True,'both_chains_all_recorded_solves_succeeded':bool(np.all(a[:,-1]==1) and np.all(b[:,-1]==1)),'maximum_recorded_position_difference_m':float(dp.max()),'final_position_difference_m':float(dp[-1]),'maximum_recorded_velocity_difference_mps':float(dv.max()),'final_velocity_difference_mps':float(dv[-1]),'method':'Same initial state, current controller binaries, output epochs and tolerances; initial and maximum integrator steps halved throughout a separate continuous 20-segment chain.'}
    result.update({'nominal_scenario_sha256':json.loads((ROOT/'segments'/sid/'metadata.json').read_text())['scenario_sha256'],'refined_scenario_sha256':json.loads((ROOT/'checks/half_step'/sid/'metadata.json').read_text())['scenario_sha256']})
    result.update({'nominal_controller_sha256':json.loads((ROOT/'segments'/sid/'metadata.json').read_text())['controller_sha256'],'refined_controller_sha256':json.loads((ROOT/'checks/half_step'/sid/'metadata.json').read_text())['controller_sha256']})
    result['method']='Same separation state, current controller binaries, output epochs and tolerances; initial and maximum integrator steps halved throughout a separate continuous chain. No intermediate state resets or controller refits. This record describes the completed prefix through Segment '+sid+'.'
    write(ROOT/'analysis'/f'{sid}_refinement.json',json.dumps(result,indent=2));print(f'Refinement {sid}: max {dp.max():.6f} m, endpoint {dp[-1]:.6f} m',flush=True)
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--watch',action='store_true');p.add_argument('--from',dest='first',type=int,default=1);p.add_argument('--to',type=int,default=20);a=p.parse_args()
    for i in range(a.first,a.to+1):
        sid=f'{i:02d}'
        if a.watch:
            while not (ROOT/'checks/half_step'/sid/'metadata.json').exists():time.sleep(5)
        inspect(sid)
