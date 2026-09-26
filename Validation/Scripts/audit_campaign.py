"""Independent output-grid, physical-tail, impulse, and integrity acceptance checks."""

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))

from pathlib import Path
import os
os.environ.setdefault('PHAROS_VCVARS', r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat')
import csv
import argparse
import hashlib
import importlib.util
import json
import math
import subprocess
import tomllib
from run_campaign import ROOT, FEATURE

P=ROOT/FEATURE

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def build_time_reference():
    folder=_RUNTIME/'time_reference'
    if (folder/'SpiceTimeReference.exe').is_file():
        return folder/'SpiceTimeReference.exe'
    folder.mkdir(exist_ok=True)
    source=folder/'SpiceTimeReference.cpp'
    source.write_text('''#include "SpiceUsr.h"
#include <iostream>
#include <iomanip>
int main(int argc,char** argv){
    if(argc!=4)return 2;
    furnsh_c(argv[1]);
    SpiceDouble start,end;
    str2et_c(argv[2],&start);str2et_c(argv[3],&end);
    std::cout<<std::setprecision(17)<<end-start<<"\\n";
    return failed_c()?1:0;
}
''',encoding='utf-8')
    project=Path(os.environ.get('PHAROS_SOURCE', next(p for p in Path(__file__).resolve().parents if (p/'PHAROS.uproject').is_file())))
    cmd=folder/'build.cmd'
    cmd.write_text(f'''@echo off
call "%PHAROS_VCVARS%" >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /O2 /MT /EHsc /I"{project/'Source/ThirdParty/CSPICE/Include'}" /Fo"{folder/'time.obj'}" /Fe"{folder/'SpiceTimeReference.exe'}" "{source}" "{project/'Source/ThirdParty/CSPICE/Lib/Win64/cspice.lib'}" /link oldnames.lib legacy_stdio_definitions.lib > "{folder/'build.log'}" 2>&1
exit /b %errorlevel%
''',encoding='utf-8')
    r=subprocess.run(['cmd.exe','/d','/c',str(cmd)],cwd=folder)
    if r.returncode: raise RuntimeError('Independent CSPICE time reference failed to build.')
    return folder/'SpiceTimeReference.exe'

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--records',nargs='+',help='Run record filenames in Support/Provenance; later files take precedence.')
    args=parser.parse_args()
    time_reference=build_time_reference()
    merged={}
    for name in args.records or ('runs_all.json','runs_angle_migration.json','runs_additional_features.json','runs_nested_output_refinement_20260918.json'):
        for record in json.loads((ROOT/'Support/Provenance'/name).read_text(encoding='utf-8')):
            merged[(record['scenario'],record.get('repeat',False))]=record
    checks=[]
    integrity=[]
    special=[]
    numeric_cells=0
    total_rows=0
    absolute_collisions=0
    def check(case,metric,error,tolerance):
        c={'case':case,'metric':metric,'error':float(error),'tolerance':float(tolerance),'pass':bool(error<=tolerance)}
        checks.append(c)
        return c
    for (relative,repeat),record in sorted(merged.items()):
        path=ROOT/relative
        doc=tomllib.loads(path.read_text(encoding='utf-8'))
        scenario=doc['scenario']
        name=path.stem+(' [repeat]' if repeat else '')
        output=ROOT/record['output_directory']
        solution=output/(path.stem+'_solution.csv')
        with solution.open(encoding='utf-8',newline='') as stream:
            reader=csv.reader(stream)
            columns=next(reader)
            strings=list(reader)
        malformed=sum(len(row)!=len(columns) for row in strings)
        array=[[float(v) for v in row] for row in strings]
        rows=[dict(zip(columns,row)) for row in array]
        numeric_cells+=sum(len(row) for row in array)
        total_rows+=len(rows)
        nonfinite=sum(not math.isfinite(v) for row in array for v in row)
        times=[r['elapsed_time_seconds'] for r in rows]
        et=[r['ephemeris_time_tdb_seconds_past_j2000'] for r in rows]
        collisions=sum(b==a for a,b in zip(et,et[1:]))
        absolute_collisions+=collisions
        q_error=max(abs(math.sqrt(sum(r['quaternion_body_to_icrf_'+a]**2 for a in 'wxyz'))-1.) for r in rows)
        if scenario['end_mode']=='duration': duration=float(scenario['duration_seconds'])
        else:
            out=subprocess.check_output([str(time_reference),str(_KERNELS/'naif0012.tls'),scenario['start_utc'],scenario['final_utc']],text=True)
            duration=float(out)
        expected=None
        if scenario.get('output_mode','fixed_interval')=='fixed_interval':
            dt=float(scenario['output_step_seconds'])
            expected=[k*dt for k in range(math.floor(duration/dt)+1) if k*dt<duration]+[duration]
        check(name,'runner exit status',record['exit_code'],0)
        summary=(output/(path.stem+'_summary.txt')).read_text(encoding='utf-8')
        check(name,'runner summary success',int('success=true' not in summary),0)
        check(name,'CSV rectangularity and unique headers',malformed+len(columns)-len(set(columns)),0)
        check(name,'all numeric cells finite',nonfinite,0)
        check(name,'elapsed epochs strictly increasing',sum(b<=a for a,b in zip(times,times[1:])),0)
        check(name,'requested terminal elapsed epoch',abs(times[-1]-duration),0)
        check(name,'all recorded articulated solves successful',sum(r['multibody_solve_succeeded']!=1. for r in rows),0)
        check(name,'quaternion normalization',q_error,1e-10)
        if expected is not None:
            check(name,'fixed output grid length',abs(len(expected)-len(times)),0)
            mismatches=abs(len(expected)-len(times))
            mismatches+=sum(abs(a-b)>4*math.ulp(max(abs(a),abs(b))) for a,b in zip(times,expected))
            check(name,'fixed output epochs',mismatches,0)
        integrity.append({'scenario':relative,'repeat':repeat,'rows':len(rows),'columns':len(columns),
            'solution':str(solution.relative_to(ROOT)).replace('\\','/'),'solution_sha256':sha(solution),
            'absolute_et_collisions':collisions,'final_elapsed_seconds':times[-1],
            'expected_duration_seconds':duration,'maximum_quaternion_norm_error':q_error})

        if 'diagnostics' in path.parts:
            # Free-flight scheduling probes: verify that the state, not just the clock, advances.
            if not doc.get('thrusters') and not doc.get('reaction_wheels'):
                p0=doc['initial_state']['position_icrf_m'];v0=doc['initial_state']['velocity_icrf_mps']
                error=max(abs(r[f'position_icrf_{a}_m']-(p0[j]+v0[j]*r['elapsed_time_seconds'])) for r in rows for j,a in enumerate('xyz'))
                scale=max(abs(p0[j]+v0[j]*duration) for j in range(3))
                # Use the timekeeping audit's 1 nm ordinary-position
                # tolerance and scale it by binary64 ULPs for billion-second runs.
                # The dedicated sub-picosecond probe tests its nonzero displacement.
                tolerance=max(1e-15 if name=='adaptive_subpicosecond_tail' else 1e-9,8*math.ulp(scale))
                check(name,'force-free position follows elapsed time',error,tolerance)
                tail=max(abs((rows[-1][f'position_icrf_{a}_m']-rows[-2][f'position_icrf_{a}_m'])-v0[j]*(times[-1]-times[-2])) for j,a in enumerate('xyz'))
                check(name,'last interval advances physical state',tail,tolerance)
            # Constant, collinear prescribed burns have a closed rocket-equation reference.
            if len(doc.get('thrusters',[]))==1:
                thruster=doc['thrusters'][0]
                if thruster['mode']=='prescribed_profile' and thruster['prescribed_thrust']['source']=='constant':
                    thrust=thruster['prescribed_thrust']['constant_value']
                    isp=thruster['prescribed_specific_impulse']['constant_value']
                    start=max(0.,thruster['ignition_elapsed_seconds'])
                    stop=min(duration,thruster.get('shutdown_elapsed_seconds',duration))
                    expected_loss=max(0.,stop-start)*thrust/(isp*9.80665)
                    loss=rows[0]['mass_kg']-rows[-1]['mass_kg']
                    check(name,'finite-window propellant loss',abs(loss-expected_loss),1e-12)
                    expected_dv=-isp*9.80665*math.log1p(-expected_loss/rows[0]['mass_kg'])
                    actual_dv=rows[-1]['velocity_icrf_x_mps']-rows[0]['velocity_icrf_x_mps']
                    check(name,'finite-window rocket-equation velocity',abs(actual_dv-expected_dv),1e-9)
                    if stop==duration:
                        check(name,'shutdown boundary has zero thrust',abs(rows[-1]['thruster_thrust_0_n']),0)
                    special.append({'case':name,'expected_propellant_loss_kg':expected_loss,'actual_propellant_loss_kg':loss,'velocity_error_mps':actual_dv-expected_dv})
                elif thruster['mode']=='prescribed_profile' and thruster['prescribed_thrust']['source']=='csv' and thruster['prescribed_specific_impulse']['source']=='constant':
                    # Profiles use time since ignition. Integrate each linear
                    # segment analytically over the active portion of the run.
                    ignition=thruster['ignition_elapsed_seconds']
                    left=max(0.,-ignition)
                    right=min(duration,thruster.get('shutdown_elapsed_seconds',duration))-ignition
                    with (path.parent/thruster['prescribed_thrust']['csv_file']).open(encoding='utf-8') as stream:
                        points=[tuple(map(float,row)) for row in csv.reader(stream)]
                    impulse=0.
                    for (t0,f0),(t1,f1) in zip(points,points[1:]):
                        lo=max(left,t0);hi=min(right,t1)
                        if hi>lo:
                            slope=(f1-f0)/(t1-t0)
                            impulse+=(hi-lo)*(f0+slope*((lo+hi)/2-t0))
                    isp=thruster['prescribed_specific_impulse']['constant_value']
                    loss=rows[0]['mass_kg']-rows[-1]['mass_kg']
                    check(name,'clipped piecewise-linear profile impulse',abs(loss*isp*9.80665-impulse),1e-9)
                    check(name,'clipped profile propellant loss',abs(loss-impulse/(isp*9.80665)),1e-12)
                    special.append({'case':name,'expected_impulse_ns':impulse,'mass_derived_impulse_ns':loss*isp*9.80665})
            if path.parent.parent.name=='commanded_thruster_step_convergence':
                q=40/(250*9.80665);b=.2*q;a=q-b;iz=1002-b*10
                omega=40/a*math.expm1(a/b*math.log1p(-b*10/1002))
                check(name+' '+path.parent.name,'commanded angular-momentum reference',abs(rows[-1]['angular_momentum_about_cm_icrf_z_kgm2ps']-iz*omega),1e-8)
                impulse=(rows[0]['mass_kg']-rows[-1]['mass_kg'])*250*9.80665
                check(name+' '+path.parent.name,'commanded finite impulse',abs(impulse-400),1e-6)
    original=P/'cases/02_actuated_multibody/02e_reaction_wheel/results/reaction_wheel_solution.csv'
    repeat=P/'diagnostics/reaction_wheel_output_boundary/results/reaction_wheel_solution.csv'
    check('reaction_wheel_repeat','byte-identical deterministic repeat',int(sha(original)!=sha(repeat)),0)
    every=P/'diagnostics/reaction_wheel_output_boundary/reaction_wheel_every_step.tgscn'
    with (every.parent/'results/reaction_wheel_every_step_solution.csv').open(encoding='utf-8') as f:
        rows=[{k:float(v) for k,v in row.items()} for row in csv.DictReader(f)]
    check('reaction_wheel_every_step','wheel capacity',max(0.,max(abs(r['reaction_wheel_momentum_0_nms']) for r in rows)-.2),1e-12)
    check('reaction_wheel_every_step','total angular-momentum conservation',max(abs(r['angular_momentum_about_cm_icrf_z_kgm2ps']) for r in rows),1e-10)
    result={'run_count':len(integrity),'rows':total_rows,'numeric_cells':numeric_cells,'absolute_et_collisions':absolute_collisions,
        'checks_passed':sum(c['pass'] for c in checks),'checks_failed':sum(not c['pass'] for c in checks),
        'integrity':integrity,'checks':checks,'burn_references':special,
        'time_note':'Elapsed time orders samples. Absolute ET may collide in binary64 for sub-ULP intervals; those collisions are reported separately.'}
    (ROOT/'Support/Provenance/current_run_records.json').write_text(json.dumps(list(merged.values()),indent=2),encoding='utf-8')
    (P/'analysis/campaign_integrity.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print('Integrity and diagnostics:',result['checks_passed'],'passed,',result['checks_failed'],'failed;',total_rows,'rows.',flush=True)
    for c in checks:
        if not c['pass']:print(c,flush=True)

if __name__=='__main__':main()
