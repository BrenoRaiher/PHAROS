"""Audit recorded physical invariants and reconstruct exact-epoch reference curves."""
import argparse,csv,json,math,re,time,tomllib
import numpy as np
from continuous_chain import ROOT,write,solution,last,sha
from reference import query

def load(path,columns=None):
    with path.open(encoding='utf-8-sig') as f:header=next(csv.reader(f))
    use=None if columns is None else [header.index(c) for c in columns]
    a=np.loadtxt(path,delimiter=',',skiprows=1,usecols=use,ndmin=2)
    return (header if columns is None else columns),a
def vector(a,h,prefix,suffix=''):
    return a[:,[h.index(prefix+x+suffix) for x in 'xyz']]
def defines(sid):
    d={}
    for k,v in re.findall(r'(?m)^#define (\w+)\s+(.+)$',(ROOT/'segments'/sid/'controllers/ControllerConfig.h').read_text()):
        try:d[k]=float(v)
        except ValueError:d[k]=v.strip('"')
    return d
def inspect(sid):
    ref=ROOT/'expected'/f'{sid}_curve.csv'
    query(sid,ref,['--states',solution(sid)])
    h,a=load(solution(sid));_,r=load(ref,['et','x_m','y_m','z_m','vx_mps','vy_mps','vz_mps'])
    cfg=tomllib.loads((ROOT/'segments'/sid/'scenario'/f'cassini_{sid}.tgscn').read_text())
    spec=json.loads((ROOT/'provenance/segments.json').read_text())[int(sid)-1]
    checks=[]
    def gate(name,ok,measured=None,limit=None):checks.append({'name':name,'passed':bool(ok),'measured':measured,'limit':limit})
    col=lambda c:a[:,h.index(c)]
    gate('All recorded numeric cells finite',np.isfinite(a).all(),int(a.size))
    gate('Recorded articulated solves succeeded',np.all(col('multibody_solve_succeeded')==1),int(np.sum(col('multibody_solve_succeeded')!=1)),0)
    gate('Strictly increasing recorded epochs',np.all(np.diff(col(h[0]))>0))
    gate('Reference epochs match recorded epochs',np.array_equal(col(h[0]),r[:,0]))
    ep=list(csv.DictReader((ROOT/'expected'/f'{sid}_endpoints.csv').open()))
    endpoint_time_error=max(abs(col(h[0])[0]-float(ep[0]['et'])),abs(col(h[0])[-1]-float(ep[-1]['et'])))
    gate('Requested start and final epochs reached',endpoint_time_error<=1e-7,endpoint_time_error,1e-7)
    q=a[:,[h.index('quaternion_body_to_icrf_'+x) for x in 'wxyz']]
    qerr=float(np.max(np.abs(np.linalg.norm(q,axis=1)-1)))
    gate('Attitude quaternion normalized',qerr<=1e-12,qerr,1e-12)
    mass=col('mass_kg');pool=col('variable_component_mass_0_kg')
    gate('No propellant creation or exhaustion',np.max(np.diff(mass))<=1e-8 and np.min(pool)>0,float(np.min(pool)),0)
    mass_identity=float(np.max(np.abs(mass-pool-2523)))
    gate('Total mass equals dry mass plus inventory',mass_identity<=1e-8,mass_identity,1e-8)
    template=tomllib.loads((ROOT/'provenance/templates/01.tgscn').read_text());startpool=template['components'][1]
    ierr=0.
    for axis,dry in [('xx',5046),('yy',12249),('zz',12249),('xy',0),('xz',0),('yz',0)]:
        expected=dry+pool*startpool['inertia']['i'+axis+'_kgm2']/startpool['initial_mass_kg']
        ierr=max(ierr,float(np.max(np.abs(col('inertia_body_'+axis+'_kgm2')-expected))))
    gate('Inertia follows retained mass-distribution law',ierr<1e-6,ierr,1e-6)
    wheel=a[:,[h.index(f'reaction_wheel_momentum_{i}_nms') for i in range(3)]]
    wheelmax=float(np.max(np.abs(wheel)))
    gate('Wheel momenta remain within capacity',wheelmax<=36+1e-7,wheelmax,36+1e-7)
    control=vector(a,h,'torque_control_body_','_nm');torquemax=float(np.max(np.abs(control)))
    gate('Wheel torque limits respected',torquemax<=.14+1e-9,torquemax,.14+1e-9)
    d=defines(sid);expected_rate=np.zeros(len(a));nominal_mass_loss=0
    for i,t in enumerate(cfg.get('thrusters',[])):
        thrust=col(f'thruster_thrust_{i}_n');prefix='CASSINI_TRIM_' if 'Cruise' in t['name'] else 'CASSINI_'
        isp=d.get(prefix+'THRUSTER_ISP_S',1)
        expected_rate-=thrust/(isp*9.80665)
        gate(f'Thruster {i} respects force limits',float(thrust.min())>=-1e-9 and float(thrust.max())<=t['maximum_thrust_n']+1e-7,float(thrust.max()),t['maximum_thrust_n'])
        t0=d.get(prefix+'BURN_START_S',0);t1=d.get(prefix+'BURN_END_S',0)
        active=d.get(prefix+'ENABLED',0) if prefix=='CASSINI_TRIM_' else d.get('CASSINI_BURN_ENABLED',0)
        off=(col('elapsed_time_seconds')<t0-1e-7)|(col('elapsed_time_seconds')>t1+1e-7)
        gate(f'Thruster {i} off outside command interval',np.all(np.abs(thrust[off])<1e-10))
        if active:
            throttle=d[prefix+'THROTTLE'] if prefix=='CASSINI_TRIM_' else d['CASSINI_BURN_THROTTLE']
            nominal_mass_loss+=t['maximum_thrust_n']*throttle*(t1-t0)/(isp*9.80665)
    rate_error=float(np.max(np.abs(col('mass_rate_kgps')-expected_rate)))
    gate('Propellant rates match applied thrust and specific impulse',rate_error<1e-11,rate_error,1e-11)
    mass_loss=float(mass[0]-mass[-1]);mass_loss_error=mass_loss-nominal_mass_loss
    gate('Commanded burns delivered expected integrated mass flow',abs(mass_loss_error)<1e-4,mass_loss_error,1e-4)
    continuity={}
    if sid!='01':
        prev=last(solution(f'{int(sid)-1:02d}'))
        cols=[h[0],'mass_kg','variable_component_mass_0_kg']+[f'position_icrf_{x}_m' for x in 'xyz']+[f'velocity_icrf_{x}_mps' for x in 'xyz']+[f'quaternion_body_to_icrf_{x}' for x in 'wxyz']+[f'angular_velocity_body_{x}_radps' for x in 'xyz']+[f'inertia_body_{x}_kgm2' for x in ['xx','yy','zz','xy','xz','yz']]+[f'reaction_wheel_momentum_{i}_nms' for i in range(3)]
        continuity={c:float(a[0,h.index(c)]-float(prev[c])) for c in cols}
        gate('Complete previous state carried across boundary',max(map(abs,continuity.values()))<=1e-8,max(map(abs,continuity.values())),1e-8)
    pos=vector(a,h,'position_icrf_','_m');vel=vector(a,h,'velocity_icrf_','_mps')
    dp=pos-r[:,1:4];dv=vel-r[:,4:7];pe=np.linalg.norm(dp,axis=1);ve=np.linalg.norm(dv,axis=1)
    legacy=json.loads((ROOT/'provenance/legacy_endpoints'/f'{sid}.json').read_text())
    lpos=np.array([float(legacy[f'position_icrf_{x}_m']) for x in 'xyz']);lvel=np.array([float(legacy[f'velocity_icrf_{x}_mps']) for x in 'xyz'])
    log=(ROOT/'segments'/sid/'logs/run.log').read_text()
    gate('Runner reported successful completion','Simulation completed successfully.' in log)
    gate('No warning or error lines',not re.search(r'(?im)^(?:.*\b(?:warning|error|failed)\b.*)$',log))
    meta=json.loads((ROOT/'segments'/sid/'metadata.json').read_text())
    metrics={'id':sid,'label':spec['label'],'start_utc':spec['start_utc'],'end_utc':spec['end_utc'],'rows':len(a),'numeric_cells':int(a.size),'wall_seconds':meta['wall_seconds'],'endpoint_position_error_km':float(pe[-1]/1000),'endpoint_velocity_error_mps':float(ve[-1]),'maximum_sampled_position_error_km':float(pe.max()/1000),'rms_sampled_position_error_km':float(np.sqrt(np.mean(pe**2))/1000),'rms_time_weighted_position_error_km':float(np.sqrt(np.trapezoid(pe**2,r[:,0])/(r[-1,0]-r[0,0]))/1000),'maximum_sampled_velocity_error_mps':float(ve.max()),'endpoint_displacement_from_legacy_m':float(np.linalg.norm(pos[-1]-lpos)),'endpoint_velocity_change_from_legacy_mps':float(np.linalg.norm(vel[-1]-lvel)),'initial_mass_kg':float(mass[0]),'final_mass_kg':float(mass[-1]),'nominal_mass_loss_kg':nominal_mass_loss,'actual_mass_loss_kg':mass_loss,'maximum_wheel_momentum_nms':wheelmax,'saturated_output_rows':int(np.sum(np.any(np.abs(wheel)>36-1e-6,axis=1))),'continuity_differences':continuity,'checks':checks,'checks_passed':sum(x['passed'] for x in checks),'checks_total':len(checks)}
    metrics.update({'scenario_sha256':meta['scenario_sha256'],'controller_sha256':meta['controller_sha256'],'relative_tolerance':cfg['scenario']['relative_tolerance'],'absolute_tolerance':cfg['scenario']['absolute_tolerance']})
    out=ROOT/'analysis';out.mkdir(parents=True,exist_ok=True)
    np.savez_compressed(out/f'{sid}_curve.npz',et=r[:,0],position_m=pos,velocity_mps=vel,reference_position_m=r[:,1:4],reference_velocity_mps=r[:,4:7],position_error_m=pe,velocity_error_mps=ve,mass_kg=mass,wheel_momentum_nms=wheel)
    write(out/f'{sid}_metrics.json',json.dumps(metrics,indent=2))
    print(f'Audited Cassini {sid}: {pe[-1]/1000:.6f} km, {ve[-1]:.6f} m/s; {metrics["checks_passed"]}/{len(checks)} checks',flush=True)
    for c in checks:
        if not c['passed']:print('FINDING: '+json.dumps(c),flush=True)
    return metrics

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--watch',action='store_true');p.add_argument('--from',dest='first',type=int,default=1);p.add_argument('--to',type=int,default=20);a=p.parse_args()
    for i in range(a.first,a.to+1):
        sid=f'{i:02d}'
        if a.watch:
            while not (ROOT/'segments'/sid/'metadata.json').exists():time.sleep(5)
        inspect(sid)
