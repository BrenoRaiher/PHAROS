"""Common measured-state comparisons for the current Apollo reconstruction."""
from pathlib import Path
import csv,json,math,bisect
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
MU_MOON=4.9028001184575496e12
MOON_RADIUS=1737400.
def norm(x):return float(np.linalg.norm(x))
def rows(path):
    with path.open(newline='',encoding='utf-8') as f:return list(csv.DictReader(f))
def js(path):return json.loads(path.read_text(encoding='utf-8'))
def solution(folder,sid):return folder/'segments'/sid/'results'/f'apollo8_like_{sid}_solution.csv'
def load(folder):
    loaded={}
    for p in sorted((folder/'segments').glob('*/metadata.json')):
        sid=p.parent.name
        if len(sid)!=2 or not sid.isdigit():continue
        metadata=js(p);data=rows(solution(folder,sid))
        loaded[sid]={'meta':metadata,'rows':data,'ets':[float(r['ephemeris_time_tdb_seconds_past_j2000']) for r in data]}
    return loaded
def rv(row,prefix=''):
    return (np.array([float(row[prefix+'position_icrf_'+a+'_m']) for a in 'xyz']),np.array([float(row[prefix+'velocity_icrf_'+a+'_mps']) for a in 'xyz']))
def interpolate(part,et,prefix=''):
    k=bisect.bisect_left(part['ets'],et)
    if k<len(part['ets']) and abs(part['ets'][k]-et)<5e-7:return (*rv(part['rows'][k],prefix),0.)
    if k==0 or k==len(part['ets']):
        j=0 if k==0 else -1
        if abs(part['ets'][j]-et)<.002:return (*rv(part['rows'][j],prefix),abs(part['ets'][j]-et))
        raise ValueError(f"Reference ET outside propagated segment: {et} {part['ets'][0]} {part['ets'][-1]}")
    p0,v0=rv(part['rows'][k-1],prefix);p1,v1=rv(part['rows'][k],prefix)
    h=part['ets'][k]-part['ets'][k-1];u=(et-part['ets'][k-1])/h;slope=(p1-p0)/h
    a=3*slope-2*v0-v1;b=-2*slope+v0+v1
    return p0+h*(u*v0+u*u*a+u*u*u*b),v0+2*u*a+3*u*u*b,h
def sample(chain,get,et,prefix=''):
    choices=[p for p in chain.values() if p['meta']['start_get_seconds']-1e-6<=get<=p['meta']['end_get_seconds']+1e-6]
    if not choices:raise ValueError('GET outside chain: '+str(get))
    return interpolate(choices[-1],et,prefix)
def direct_comparisons(folder,chain=None,reference_file=None):
    chain=load(folder) if chain is None else chain
    references=js(reference_file or ROOT/'expected/historical_states.json');out={}
    for name,truth in references.items():
        if name=='initial_state':continue
        if not any(p['meta']['start_get_seconds']<=truth['get_seconds']<=p['meta']['end_get_seconds'] for p in chain.values()):continue
        p,v,h=sample(chain,truth['get_seconds'],truth['et']);dr=p-truth['position_icrf_m']
        result={'get_seconds':truth['get_seconds'],'position_error_m':norm(dr),'position_delta_m':dr.tolist(),'interpolation_interval_s':h,'center':truth['center'],'source':truth['source']}
        if 'velocity_icrf_mps' in truth:
            dv=v-truth['velocity_icrf_mps'];result.update({'velocity_error_mps':norm(dv),'velocity_delta_mps':dv.tolist()})
        out[name]=result
    return out
def orbit(row,body='moon'):
    p,v=rv(row);bp,bv=rv(row,'body_'+body+'_');r=p-bp;v=v-bv;rn=norm(r);vv=np.dot(v,v)
    mu=MU_MOON if body=='moon' else 3.9860043550702266e14
    radius=MOON_RADIUS if body=='moon' else 6378136.6
    sma=1/(2/rn-vv/mu);e=norm(((vv-mu/rn)*r-np.dot(r,v)*v)/mu)
    return {'radius_m':rn,'altitude_m':rn-radius,'speed_mps':norm(v),'flight_path_angle_deg':math.degrees(math.asin(np.dot(r,v)/(rn*norm(v)))),'semimajor_axis_m':sma,'eccentricity':e,'periapsis_altitude_m':sma*(1-e)-radius,'apoapsis_altitude_m':sma*(1+e)-radius if sma>0 else None,'period_s':2*math.pi*math.sqrt(sma**3/mu) if sma>0 else None}
def lunar_metrics(folder):
    # Reproduce the frozen calibration objective, including its original transcription.
    # Published historical comparisons use the corrected, expanded catalog instead.
    chain=load(folder);direct=direct_comparisons(folder,chain,ROOT/'expected/direct_states.json');states={sid:orbit(chain[sid]['rows'][-1]) for sid in ['06','08','09']}
    residual=[]
    for name in ['lunar_rev1','lunar_rev9']:residual.extend(np.array(direct[name]['position_delta_m'])/10000.)
    for sid,peri,apo in [('06',60.,168.5),('08',59.7,60.7)]:
        residual.extend([(states[sid]['periapsis_altitude_m']-peri*1852.)/5000.,(states[sid]['apoapsis_altitude_m']-apo*1852.)/10000.])
    return {'objective_norm':norm(residual),'weighted_residual':list(map(float,residual)),'direct':direct,'lunar_orbits':states}
if __name__=='__main__':
    result=lunar_metrics(ROOT)
    (ROOT/'analysis/lunar_metrics.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result,indent=2))
