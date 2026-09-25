"""Audit the retained continuous mission independently of its historical fit."""
from pathlib import Path
import csv,json,hashlib,tomllib,math
import numpy as np
import continuous_chain as c
import mission_metrics as m
ROOT=c.ROOT
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def unit(x):return x/m.norm(x)
def target(row,params):
    if params['target_mode']=='fixed_icrf':return np.array(params['direction'])
    p,v=m.rv(row);bp,bv=m.rv(row,'body_moon_');r=p-bp;v=v-bv;rad=unit(r);tan=v-np.dot(v,rad)*rad
    if params['target_mode']=='circularize_moon':return unit(math.sqrt(m.MU_MOON/m.norm(r))*unit(tan)-v)
    return -unit(tan+params['radial_bias']*m.norm(tan)*rad+params['normal_bias']*m.norm(tan)*unit(np.cross(rad,tan)))
def table_comparisons(chain):
    out=[]
    for ref in c.truth_rows().values():
        get=float(ref['get_seconds'])
        if get>c.SEGMENTS['13']['end_get']:continue
        part=next(p for p in chain.values() if p['meta']['start_get_seconds']<=get<=p['meta']['end_get_seconds'])
        et=part['ets'][0]+get-part['meta']['start_get_seconds']
        p,v,h=m.sample(chain,get,et);bp,bv,_=m.sample(chain,get,et,'body_'+ref['body'].lower()+'_')
        rp=p-bp;rv=v-bv;rt=np.array([float(ref['relative_position_icrf_'+a+'_m']) for a in 'xyz'])
        dr=p-np.array([float(ref['position_icrf_'+a+'_m']) for a in 'xyz'])
        out.append({'event_id':ref['event_id'],'get_seconds':get,'position_error_m':m.norm(dr),'radius_error_m':m.norm(rp)-m.norm(rt),'speed_error_mps':m.norm(rv)-float(ref['speed_ftps'])*.3048,'flight_path_angle_error_deg':math.degrees(math.asin(np.dot(rp,rv)/m.norm(rp)/m.norm(rv)))-float(ref['flight_path_angle_deg']),'interpolation_interval_s':h,'note':'Rounded report coordinates. Lunar Cartesian velocity reconstructed from heading is excluded; use radius, speed and flight-path angle. Epoch is the table epoch, which may differ from the detailed actual firing schedule.'})
    return out
def main():
    chain=m.load(ROOT);assert list(chain)==[f'{i:02d}' for i in range(1,14)]
    checks=[];segments=[];boundaries=[];burns=[];count=0;cells=0
    def check(name,passed,detail=None):checks.append({'name':name,'pass':bool(passed),'detail':detail})
    for sid,part in chain.items():
        rows=part['rows'];names=list(rows[0]);data=np.array([[float(row[k]) for k in names] for row in rows]);index={k:i for i,k in enumerate(names)}
        col=lambda k:data[:,index[k]]
        count+=len(rows);cells+=data.size
        q=data[:,[index['quaternion_body_to_icrf_'+a] for a in 'wxyz']];qerror=float(np.max(np.abs(np.linalg.norm(q,axis=1)-1)))
        dt=np.diff(col('elapsed_time_seconds'));mass=col('mass_kg');pool=col('variable_component_mass_0_kg')
        scenario=ROOT/'segments'/sid/'scenario'/f'apollo8_like_{sid}.tgscn';spec=tomllib.loads(scenario.read_text(encoding='utf-8'))
        check(sid+' finite numeric outputs',np.isfinite(data).all(),int(data.size))
        check(sid+' articulated solve',np.all(col('multibody_solve_succeeded')==1))
        check(sid+' time ordering and endpoints',np.all(dt>0) and abs(col('elapsed_time_seconds')[0])<1e-8 and abs(col('elapsed_time_seconds')[-1]-(c.SEGMENTS[sid]['end_get']-c.SEGMENTS[sid]['start_get']))<.002)
        check(sid+' normalized attitude',qerror<1e-10,qerror)
        check(sid+' nonincreasing mass and positive pool',np.max(np.diff(mass))<1e-6 and np.min(pool)>100. and np.max(np.abs(mass-(13000.+pool)))<1e-7)
        check(sid+' nonpositive mass rate',np.max(col('mass_rate_kgps'))<1e-10)
        check(sid+' current format and source provenance','format_version' not in spec and part['meta']['scenario_sha256']==sha(scenario) and part['meta']['runner_sha256']==sha(c.RUNNER))
        checks_thrust=[]
        for j,thr in enumerate(spec.get('thrusters',[])):
            f=col(f'thruster_thrust_{j}_n');checks_thrust.append(np.min(f)>=-1e-10 and np.max(f)<=thr['maximum_thrust_n']*(1+1e-12))
        check(sid+' actuator thrust bounds',all(checks_thrust))
        minimum={}
        for body in ['moon','earth']:
            p=data[:,[index['position_icrf_'+a+'_m'] for a in 'xyz']];bp=data[:,[index['body_'+body+'_position_icrf_'+a+'_m'] for a in 'xyz']]
            minimum[body]=float(np.min(np.linalg.norm(p-bp,axis=1)-col('body_'+body+'_reference_radius_m')))
            check(sid+' recorded '+body+' clearance',minimum[body]>0,minimum[body])
        # The carried inertia per unit pool mass is unchanged at each state handoff.
        inertia=np.stack([data[:,[index['inertia_body_'+a+b+'_kgm2'] for b in 'xyz']] for a in 'xyz'],axis=1)
        eig=np.linalg.eigvalsh(inertia);check(sid+' positive physical inertia',np.all(eig>0) and np.all(eig[:,0]+eig[:,1]>=eig[:,2]-1e-8))
        if sid!='01':
            previous=f'{int(sid)-1:02d}';d=c.continuity_metrics(m.solution(ROOT,previous),m.solution(ROOT,sid));boundaries.append({'from':previous,'to':sid,**d});check(sid+' complete state continuity',all(v==0 for v in d.values()),d)
        if c.SEGMENTS[sid]['kind']=='burn':
            params=m.js(ROOT/'segments'/sid/'controllers/controller_parameters.json');j=0 if params['use_sps'] else 1
            active=(col('thruster_thrust_'+str(j)+'_n')>1e-6);angles=[]
            for row in np.array(rows,dtype=object)[active]:
                direction=target(row,params);q=tuple(float(row['quaternion_body_to_icrf_'+a]) for a in 'wxyz');tb=c.target_body_from_quaternion(q,direction)
                angles.append(math.degrees(math.atan2(math.hypot(tb[1],tb[2]),tb[0])))
            maximum=max(angles);check(sid+' powered pointing gate',maximum<=1.000001,maximum)
            t=col('elapsed_time_seconds');check(sid+' main thrust within scheduled window',np.all((t[active]>=params['burn_start']-1e-6)&(t[active]<=params['burn_end']+1e-6)))
            accel=np.linalg.norm(data[:,[index['force_thrust_icrf_'+a+'_n'] for a in 'xyz']],axis=1)/mass
            scalar=float(np.sum(accel[:-1]*dt));vector=m.js(ROOT/'segments'/sid/'analysis/analysis.json')['burn_execution']
            burns.append({'segment':sid,'burn':c.SEGMENTS[sid]['burn'],'scheduled_ignition_get':c.SEGMENTS[sid]['start_get']+params['burn_start'],'scheduled_cutoff_get':c.SEGMENTS[sid]['start_get']+params['burn_end'],'scalar_integrated_thrust_acceleration_mps':scalar,'vector_integral_norm_mps':vector['integrated_thrust_delta_v_mps'],'integration_note':'0.1 s output left-rule quadrature; scalar and vector integrals differ for a curved thrust direction. Both include ullage if present.','maximum_recorded_powered_misalignment_deg':maximum,'parameters':params})
        segments.append({'id':sid,'sample_count':len(rows),'start_get_s':part['meta']['start_get_seconds'],'end_get_s':part['meta']['end_get_seconds'],'initial_mass_kg':float(mass[0]),'final_mass_kg':float(mass[-1]),'minimum_recorded_altitudes_m':minimum})
    check('single external translational seed',sum(bool(p['meta'].get('external_initial_state')) for p in chain.values())==1)
    check('final consumables mass target',abs(float(chain['13']['rows'][-1]['mass_kg'])-31768*.45359237)<.001)
    # Original-archive immutability was a preparation check, not a physical result.
    # The portable export is checked separately by Tools/Repository/check_repository.py.
    output={'checks':checks,'passed':sum(x['pass'] for x in checks),'total':len(checks),'samples':count,'numeric_cells':cells,'segments':segments,'boundaries':boundaries,'burns':burns,'direct_comparisons':m.direct_comparisons(ROOT,chain),'table_comparisons':table_comparisons(chain),'lunar_orbits':{sid:m.orbit(chain[sid]['rows'][-1]) for sid in ['06','08','09']},'separation_earth_state':m.orbit(chain['13']['rows'][-1],'earth'),'runner_sha256':sha(c.RUNNER)}
    (ROOT/'analysis/final_audit.json').write_text(json.dumps(output,indent=2),encoding='utf-8')
    print(json.dumps({'passed':output['passed'],'total':output['total'],'samples':count,'failures':[x for x in checks if not x['pass']],'direct_errors_m':{k:v['position_error_m'] for k,v in output['direct_comparisons'].items()}},indent=2))
    if output['passed']!=output['total']:raise SystemExit(1)
if __name__=='__main__':main()
