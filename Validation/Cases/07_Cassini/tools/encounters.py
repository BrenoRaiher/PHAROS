"""Continuous cubic-Hermite encounter estimates, checked against direct SPICE."""

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))

import argparse,csv,json,time
import numpy as np
from audit import load,vector
from continuous_chain import ROOT,write,solution
from reference import query

EVENTS=[('Venus 1','03','venus','299','1998-04-26T13:44:41Z',6051800.),('Venus 2','06','venus','299','1999-06-24T20:29:54.892Z',6051800.),('Earth','10','earth','399','1999-08-18T03:28:25.614Z',6378136.6),('Jupiter','12','jupiter','599','2000-12-30T10:04:22Z',71492000.),('Phoebe','18','phoebe','609','2004-06-11T19:33:37Z',109400.)]

def hermite(p0,v0,p1,v1,dt):
    return np.array([p0,dt*v0,3*(p1-p0)-dt*(2*v0+v1),2*(p0-p1)+dt*(v0+v1)])
def evaluate(t,p,v,et):
    i=max(0,min(len(t)-2,np.searchsorted(t,et)-1));dt=t[i+1]-t[i];u=(et-t[i])/dt;c=hermite(p[i],v[i],p[i+1],v[i+1],dt)
    return np.polynomial.polynomial.polyval(u,c),np.polynomial.polynomial.polyval(u,c[1:]*np.arange(1,4)[:,None])/dt
def closest(t,p,v):
    index=int(np.argmin(np.linalg.norm(p,axis=1)));best=None
    for i in range(max(0,index-2),min(len(t)-1,index+2)):
        dt=t[i+1]-t[i];c=hermite(p[i],v[i],p[i+1],v[i+1],dt);poly=np.zeros(6)
        for ax in range(3):poly+=np.polynomial.polynomial.polymul(c[:,ax],c[1:,ax]*np.arange(1,4))
        candidates=[0.,1.]+[float(x.real) for x in np.polynomial.polynomial.polyroots(poly) if abs(x.imag)<1e-9 and 0<x.real<1]
        for u in candidates:
            r=np.polynomial.polynomial.polyval(u,c);distance=float(np.linalg.norm(r))
            if best is None or distance<best[1]:best=(float(t[i]+u*dt),distance)
    return best
def nominal(event):
    name,sid,body,target,utc,radius=event;stem=name.lower().replace(' ','_');out=ROOT/'expected/encounters';out.mkdir(parents=True,exist_ok=True)
    epoch=out/f'{stem}_epoch.csv';query(sid,epoch,['--utc',utc])
    anchor=float(next(csv.DictReader(epoch.open()))['et'])
    grid=out/f'{stem}_epochs.txt';write(grid,'\n'.join(f'{x:.17g}' for x in np.arange(anchor-1200,anchor+1201,1)))
    ref=out/f'{stem}_relative.csv';query(sid,ref,['--epochs',grid],observer=target)
    _,r=load(ref,['et','x_m','y_m','z_m','vx_mps','vy_mps','vz_mps']);rt,rr=closest(r[:,0],r[:,1:4],r[:,4:7])
    cols=['ephemeris_time_tdb_seconds_past_j2000']+[f'position_icrf_{a}_m' for a in 'xyz']+[f'velocity_icrf_{a}_mps' for a in 'xyz']+[f'body_{body}_position_icrf_{a}_m' for a in 'xyz']+[f'body_{body}_velocity_icrf_{a}_mps' for a in 'xyz']
    _,original=load(solution(sid),cols);original=original[np.abs(original[:,0]-anchor)<86400]
    dense=ROOT/'checks/encounters'/stem/'results'/f'{stem}_solution.csv'
    if dense.exists() and (dense.parents[1]/'metadata.json').exists():
        _,s=load(dense,cols)
        # A short continuation changes only temporal resolution. The delegated
        # controller retains its original mission elapsed time and burn schedule.
        extra=['force_thrust_icrf_'+a+'_n' for a in 'xyz']+['multibody_solve_succeeded']
        _,diag=load(dense,extra);assert np.all(diag[:,:3]==0) and np.all(diag[:,3]==1)
        method='Nominal-state continuation with original controller elapsed-time offset and 1 s integration/output; cubic Hermite closest approach. Direct-SPICE 1 s reference.'
    else:
        s=original
        method='Cubic Hermite interpolation of nominal recorded relative position and velocity; 1 s direct-SPICE reference.'
    pos=s[:,1:4]-s[:,7:10];vel=s[:,4:7]-s[:,10:13];st,sr=closest(s[:,0],pos,vel)
    sim_r,sim_v=evaluate(s[:,0],pos,vel,rt);ref_r,ref_v=evaluate(r[:,0],r[:,1:4],r[:,4:7],rt)
    # Piecewise-linear playback clearance is distinct from propagated trajectory clearance.
    original_pos=original[:,1:4]-original[:,7:10]
    d=original_pos[1:]-original_pos[:-1];alpha=np.clip(-np.sum(original_pos[:-1]*d,axis=1)/np.sum(d*d,axis=1),0,1)
    chord=float(np.linalg.norm(original_pos[:-1]+alpha[:,None]*d,axis=1).min())
    times=out/f'{stem}_closest_times.csv';query(sid,times,['--et',str(rt),'--et',str(st)])
    utcrows=list(csv.DictReader(times.open()))
    metric={'name':name,'segment':sid,'reference_radius_km':radius/1000,'reference_closest_utc':utcrows[0]['utc'],'simulated_closest_utc':utcrows[1]['utc'],'reference_closest_et':rt,'simulated_closest_et':st,'closest_time_difference_s':st-rt,'reference_range_km':rr/1000,'simulated_range_km':sr/1000,'range_difference_km':(sr-rr)/1000,'reference_altitude_km':(rr-radius)/1000,'simulated_altitude_km':(sr-radius)/1000,'minimum_playback_chord_altitude_km':(chord-radius)/1000,'position_error_at_reference_closest_km':float(np.linalg.norm(sim_r-ref_r)/1000),'velocity_error_at_reference_closest_mps':float(np.linalg.norm(sim_v-ref_v)),'estimation':method}
    write(ROOT/'analysis'/f'{stem}_encounter.json',json.dumps(metric,indent=2))
    np.savez_compressed(ROOT/'analysis'/f'{stem}_encounter.npz',sim_et=s[:,0],sim_position_m=pos,ref_et=r[:,0],ref_position_m=r[:,1:4],radius_m=radius)
    print(json.dumps(metric),flush=True)
    return metric
def soi():
    path=solution('20');body='saturn';cols=['ephemeris_time_tdb_seconds_past_j2000']+[f'position_icrf_{a}_m' for a in 'xyz']+[f'velocity_icrf_{a}_mps' for a in 'xyz']+[f'body_{body}_position_icrf_{a}_m' for a in 'xyz']+[f'body_{body}_velocity_icrf_{a}_mps' for a in 'xyz']+['mass_kg']
    _,s=load(path,cols);r=s[:,1:4]-s[:,7:10];v=s[:,4:7]-s[:,10:13]
    output=ROOT/'expected/soi_saturn_relative.csv';query('20',output,['--states',path],observer='699');_,ref=load(output,['et','x_m','y_m','z_m','vx_mps','vy_mps','vz_mps'])
    # GM of physical Saturn in the same runtime kernel as PHAROS.
    import re
    gm=float(re.search(r'BODY699_GM\s*=\s*\(\s*([\d.Ee+\-]+)',(_KERNELS/'gm_de440.tpc').read_text()).group(1))*1e9
    def orbit(r,v):
        rn=float(np.linalg.norm(r));vn=float(np.linalg.norm(v));h=np.cross(r,v);e=np.cross(v,h)/gm-r/rn;ecc=float(np.linalg.norm(e));energy=vn**2/2-gm/rn;a=-gm/(2*energy)
        return {'radius_km':rn/1000,'speed_kmps':vn/1000,'specific_energy_km2ps2':energy/1e6,'semimajor_axis_km':a/1000,'eccentricity':ecc,'periapsis_radius_km':a*(1-ecc)/1000}
    result={'physical_saturn_gm_m3ps2':gm,'initial_simulated':orbit(r[0],v[0]),'final_simulated':orbit(r[-1],v[-1]),'final_reference':orbit(ref[-1,1:4],ref[-1,4:7]),'initial_mass_kg':float(s[0,-1]),'final_mass_kg':float(s[-1,-1]),'initial_to_final_mass_loss_kg':float(s[0,-1]-s[-1,-1]),'capture_succeeded':orbit(r[-1],v[-1])['specific_energy_km2ps2']<0,'orbit_definition':'Instantaneous two-body Saturn-centered osculating elements; the propagated model also includes harmonics, moons, other bodies, SRP and finite thrust.'}
    write(ROOT/'analysis/soi_metrics.json',json.dumps(result,indent=2));print(json.dumps(result),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--event',choices=['venus_1','venus_2','earth','jupiter','phoebe','soi','all'],default='all');p.add_argument('--watch',action='store_true');a=p.parse_args()
    for e in EVENTS:
        if a.event in ['all',e[0].lower().replace(' ','_')]:
            if a.watch:
                while not (ROOT/'segments'/e[1]/'metadata.json').exists():time.sleep(5)
            nominal(e)
    if a.event in ['all','soi']:
        if a.watch:
            while not (ROOT/'segments/20/metadata.json').exists():time.sleep(5)
        soi()
