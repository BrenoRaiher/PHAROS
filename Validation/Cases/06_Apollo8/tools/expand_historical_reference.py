"""Convert the complete transcribed Appendix-A catalog; compare saved outputs only.
No simulation, controller, scenario, or calibration-input file is modified.
"""

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))

from pathlib import Path
import sys,csv,json,subprocess,hashlib,importlib.util
from datetime import datetime,timedelta,timezone
CASE=Path(__file__).resolve().parents[1]
STAGE=CASE
sys.path.insert(0,str(CASE/'tools'))
sys.dont_write_bytecode=True
import mission_metrics as m
import numpy as np
KERNELS=_KERNELS
R=np.array(((.9999714307930930,-.006932510238740845,-.003012955260858825),(.006932510237296334,.9999759698076381,-.00001044430021552792),(.003012955264182510,-.00001044334136026557,.9999954609854550)))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,data):
    p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
def utc(t):return (datetime(1968,12,21,12,51,tzinfo=timezone.utc)+timedelta(seconds=t)).isoformat(timespec='milliseconds').replace('+00:00','Z')
states=m.js(STAGE/'expected/historical_state_sources.json')
out={}
for s in states:
    # Query both origins for the two trajectory projections, independently of
    # interpolation in the simulation being assessed.
    bodies={}
    for body in ('Earth','Moon'):
        target=STAGE/'expected/historical_body_queries'/f'{s["event_id"]}_{body.lower()}.csv'
        target.parent.mkdir(parents=True,exist_ok=True)
        if not target.exists():
            subprocess.run([str(CASE/'tools/BodyReference.exe'),'--kernel',str(KERNELS/'naif0012.tls'),'--kernel',str(KERNELS/'de442.bsp'),'--target',body,'--utc',utc(s['get_seconds']),'--output',str(target)],capture_output=True,text=True,check=True)
        bodies[body]=m.rows(target)[0]
    body=bodies[s['center']]
    r=R@(np.array(s['position_ft'])*.3048);v=R@(np.array(s['velocity_ftps'])*.3048)
    r_abs=r+np.array([float(body[a+'_km'])*1000 for a in 'xyz'])
    v_abs=v+np.array([float(body['v'+a+'_kmps'])*1000 for a in 'xyz'])
    entry={**s,'utc':utc(s['get_seconds']),'et':float(body['et']),
           'position_relative_j2000_m':r.tolist(),'velocity_relative_j2000_mps':v.tolist(),
           'position_icrf_m':r_abs.tolist(),'velocity_icrf_mps':v_abs.tolist(),
           'conversion':'Mean NBY B1969.0 to J2000 rotation, followed by DE442 translation of the stated center.'}
    for b,data in bodies.items():
        entry['position_'+b.lower()+'_relative_j2000_m']=(r_abs-np.array([float(data[a+'_km'])*1000 for a in 'xyz'])).tolist()
    out[s['event_id']]=entry
dump(STAGE/'expected/historical_states.json',out)
chain=m.load(CASE)
audit=m.js(CASE/'analysis/final_audit.json')
comparisons={}
for name,s in out.items():
    if name=='initial_state':continue
    p,v,h=m.sample(chain,s['get_seconds'],s['et'])
    dr=p-s['position_icrf_m'];dv=v-s['velocity_icrf_mps']
    comparisons[name]={'get_seconds':s['get_seconds'],'get_hms':s['get_hms'],'label':s['label'],
        'position_error_m':m.norm(dr),'position_delta_m':dr.tolist(),'velocity_error_mps':m.norm(dv),'velocity_delta_mps':dv.tolist(),
        'interpolation_interval_s':h,'center':s['center'],'source':s['source'],
        'position_informed_calibration':s['position_informed_calibration']}
    print(f"{s['label']:25} {s['get_hms']:10} dr={m.norm(dr)/1000:10.6f} km dv={m.norm(dv):10.6f} m/s")
audit['direct_comparisons']=comparisons
audit['historical_reference_catalog']='expected/historical_states.json'
dump(STAGE/'analysis/final_audit.json',audit)
tables=[]
for row,res in zip(m.rows(CASE/'expected/apollo8_table5ii_truth_states.csv'),audit['table_comparisons']):
    assert row['event_id']==res['event_id']
    get=float(row['get_seconds']);part=next(p for p in chain.values() if p['meta']['start_get_seconds']<=get<=p['meta']['end_get_seconds'])
    et=part['ets'][0]+get-part['meta']['start_get_seconds']
    rp=np.array([float(row['position_icrf_'+a+'_m']) for a in 'xyz'])
    item={**res,'position_informed_calibration':row['event_id']=='lunar_orbit_insertion_ignition'}
    for body in ('earth','moon'):
        bp,bv,_=m.sample(chain,get,et,'body_'+body+'_')
        item['position_'+body+'_relative_j2000_m']=(rp-bp).tolist()
    tables.append(item)
dump(STAGE/'analysis/historical_comparisons.json',{'direct_states':comparisons,'rounded_maneuver_positions':tables,
    'initialization_state':'Excluded from residual statistics; used to initialize propagation.',
    'lunar_table_velocity':'No vector residual: the rounded heading conversion is not a precision velocity reference. Direct TRW states provide lunar vector velocity comparisons.',
    'source_counts':{'distinct_TRW_states_including_initial':19,'TRW_comparisons_after_initial':18,'rounded_maneuver_positions':12}})
old=m.js(CASE/'expected/direct_states.json')
corrections={}
for oldname,newname in [('lunar_rev1','lunar_rev1'),('lunar_rev9','lunar_rev10')]:
    corrections[oldname]={'corrected_event_id':newname,'corrected_source':out[newname]['source'],
        'old_position_ft':old[oldname]['position_ft'],'corrected_position_ft':out[newname]['position_ft'],
        'transcription_change_norm_m':m.norm((np.array(old[oldname]['position_ft'])-out[newname]['position_ft'])*.3048)}
dump(STAGE/'provenance/historical_reference_expansion.json',{
    'source_sha256':sha(CASE/'sources/Apollo_8_Trajectory_Reconstruction_Supplement_1.pdf'),
    'source':'Appendix A, printed A-3 through A-23; TRW postflight row, not RTCC inflight row.',
    'duplicates':'A-4/A-5 and A-6/A-7 repeat the same TRW state at the same epoch; each counted once.',
    'transcription':'New components rounded to ten significant digits. Maximum component rounding is 0.05 ft (0.01524 m) for positions and 0.0000005 ft/s (0.0000001524 m/s) for velocities. These are transcription rounding bounds, not historical accuracy estimates.',
    'corrected_calibration_mark_transcriptions':corrections,
    'calibration_provenance':'The frozen expected/direct_states.json and direct_state_sources.json preserve the original calibration inputs. The mark named lunar_rev9 there is revolution 10, and both lunar positions contain transcription errors. Final historical comparisons use corrected historical_states.json; no propagation or controller was changed.',
    'NBY1969_to_J2000':R.tolist(),'rotation_orthogonality_error':float(np.max(np.abs(R.T@R-np.eye(3)))),
    'solution_sha256':{sid:sha(m.solution(CASE,sid)) for sid in chain},
    'physical_inputs_sha256':{p.relative_to(CASE).as_posix():sha(p) for p in sorted((CASE/'segments').glob('*/scenario/*.tgscn'))},
    'controllers_sha256':{p.relative_to(CASE).as_posix():sha(p) for p in sorted((CASE/'segments').glob('*/controllers/controller_parameters.json'))},
})
