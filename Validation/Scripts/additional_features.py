"""Runner-level references for the corrected open-system balance and coupled stops."""

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))

from pathlib import Path
import concurrent.futures
import argparse
import csv
import json
import math
import subprocess
import uuid
import numpy as np
from run_campaign import ROOT, run_one

OUT=ROOT/'Cases/01-03_CoreVerification/cases/additional_features'
G0=9.80665
KINDS=('positive','negative','omitted','constant','zero_start','prescribed','variable_isp','shared')
CONTACTS={
    'single': {'initial':[(0.,1.)], 'duration':.2, 'omega':.2, 'coordinates':[.1], 'rates':[0.]},
    'both_lock': {'initial':[(0.,1.),(0.,1.)], 'duration':.2, 'omega':1/3, 'coordinates':[.1,.1], 'rates':[0.,0.]},
    'release': {'initial':[(0.,1.),(.09,.1)], 'duration':.2, 'omega':.2, 'coordinates':[.1,.09], 'rates':[0.,-.1]},
    'resting': {'initial':[(0.,1.),(.1,0.)], 'duration':.2, 'omega':.2, 'coordinates':[.1,.08], 'rates':[0.,-.2]},
    'sequential': {'initial':[(0.,1.),(0.,.5)], 'duration':.4, 'omega':.26, 'coordinates':[.092,.1], 'rates':[-.06,0.]},
    'near': {'initial':[(0.,1.),(.08999999,.1)], 'duration':.2, 'omega':.2, 'coordinates':[.1,.08999999], 'rates':[0.,-.1]},
}

def component(name,mass,inertia,parent='',offset=0.,variable=False,dofs='[]'):
    return f'''
[[components]]
id = "{uuid.uuid5(uuid.NAMESPACE_URL,'pharos-validation/'+name)}"
name = "{name}"
initial_mass_kg = {mass}
minimum_mass_kg = {1.0 if variable else mass}
variable_mass = {str(variable).lower()}
local_center_of_mass_m = [0.0,0.0,0.0]
origin_body_m = [{offset},0.0,0.0]
component_to_body = [1.0,0.0,0.0,0.0]
parent_component = "{parent}"
parent_anchor_m = [{offset},0.0,0.0]
child_anchor_m = [0.0,0.0,0.0]
child_to_parent_zero_orientation = [1.0,0.0,0.0,0.0]
dofs = {dofs}
inertia = {{ixx_kgm2={inertia[0]},iyy_kgm2={inertia[1]},izz_kgm2={inertia[2]},ixy_kgm2=0.0,ixz_kgm2=0.0,iyz_kgm2=0.0}}
'''

def header(name,duration,solver):
    return f'''format = "TGSCN"
generator = "Current PHAROS independent verification campaign"
[scenario]
name = "{name}"
start_utc = "2030-01-01T00:00:00Z"
end_mode = "duration"
duration_seconds = {duration}
integrator = "{solver}"
maximum_integrator_step_seconds = {0.2 if name.startswith('stop_') else 0.02}
initial_integrator_step_seconds = 0.01
absolute_tolerance = 1e-12
relative_tolerance = 1e-12
output_mode = "fixed_interval"
output_step_seconds = 0.01
maximum_integration_steps = 100000
maximum_output_samples = 1000
[initial_state]
position_icrf_m = [0.0,0.0,0.0]
velocity_icrf_mps = [0.0,0.0,0.0]
attitude_body_to_icrf = [1.0,0.0,0.0,0.0]
angular_velocity_body_radps = [0.0,0.0,0.0]
'''

ENV='''
[gravity]
include_first_post_newtonian_correction = false
bodies = []
[srp]
enabled = false
[atmosphere]
enabled = false
[aerodynamics]
enabled = false
'''

def generate():
    OUT.mkdir(parents=True,exist_ok=True)
    ctrl=OUT/'controllers'
    ctrl.mkdir(exist_ok=True)
    template=(_RUNTIME/'ControllerSDK/ControllerTemplate.cpp').read_text(encoding='utf-8')
    body='''
        const double t=Input.ElapsedSimulationTimeSeconds;
        for (uint64_t i=0;i<Input.ThrusterCount && i<Output.ThrusterCommandCount;++i)
        {
            const auto& v=Input.Thrusters[i].Name;
            const std::string name(v.Data, static_cast<size_t>(v.Length));
            double q=.2+.1*t, qdot=.1;
            if (name.find("negative")!=std::string::npos) {q=.4-.1*t;qdot=-.1;}
            if (name.find("constant")!=std::string::npos) {q=.2;qdot=0.;}
            if (name.find("zero_start")!=std::string::npos) {q=.1*t;qdot=.1;}
            if (name.find("shared")!=std::string::npos) {q=.1+.05*t;qdot=.05;}
            auto& c=Output.ThrusterCommands[i];
            c.Throttle=q;
            c.SpecificImpulseSeconds=10.;
            if (name.find("omitted")==std::string::npos)
            {
                c.MassFlowDerivativeProvided=1;
                c.MassFlowDerivativeKilogramsPerSecondSquared=qdot;
            }
        }
'''
    template=template.replace('#include <new>','#include <new>\n#include <string>')
    template=template.replace('        (void)Input;\n        (void)Output;',body)
    (ctrl/'DischargeController.cpp').write_text(template,encoding='utf-8')
    build=_RUNTIME/'controller_build'
    script=build/'new_features.cmd'
    script.write_text(f'''@echo off
call "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /O2 /MT /LD /DTG_CONTROLLER_BUILD=1 /I"{_RUNTIME/'ControllerSDK'}" /Fo"{build/'DischargeController.obj'}" /Fe"{ctrl/'DischargeController.dll'}" "{ctrl/'DischargeController.cpp'}" > "{build/'DischargeController.log'}" 2>&1
exit /b %errorlevel%
''',encoding='utf-8')
    result=subprocess.run(['cmd.exe','/d','/c',str(script)],cwd=build)
    if result.returncode: raise RuntimeError('Discharge controller did not compile.')
    specifications=[]
    for solver,tag in [('fixed_step_rk4','rk4'),('adaptive_dormand_prince_54','dp54')]:
        for kind in KINDS:
            name='cm_'+kind+'_'+tag
            folder=OUT/name
            folder.mkdir(exist_ok=True)
            s=header(name,2.,solver)+component('Hub',10.,[2.,2.,2.])+component('Tank',5.,[1.,1.,1.],'Hub',3.,True)
            prescribed=kind in ('prescribed','variable_isp')
            for j in range(2 if kind=='shared' else 1):
                s+=f'''
[[thrusters]]
name = "{kind}_{j}"
mode = "{'prescribed_profile' if prescribed else 'commanded'}"
mount_component = "{'Tank' if j else 'Hub'}"
propellant_component = "Tank"
application_point_component_m = [0.0,0.0,0.0]
direction_component = [1.0,0.0,0.0]
ignition_time_mode = "elapsed"
ignition_elapsed_seconds = 0.0
never_shuts_down = true
maximum_thrust_n = {10*G0:.17g}
'''
                if prescribed:
                    (folder/'thrust.csv').write_text(f'0,0\n3,{3*G0:.17g}\n4,0\n',encoding='utf-8')
                    s+='prescribed_thrust = {source="csv",constant_value=0.0,csv_file="thrust.csv"}\n'
                    if kind=='variable_isp':
                        (folder/'isp.csv').write_text('0,10\n3,16\n4,18\n',encoding='utf-8')
                        s+='prescribed_specific_impulse = {source="csv",constant_value=0.0,csv_file="isp.csv"}\n'
                    else: s+='prescribed_specific_impulse = {source="constant",constant_value=10.0,csv_file=""}\n'
            s+='\n[control]\nmode="'+('none' if prescribed else 'compiled_user_controller')+'"\n'
            if not prescribed:s+='controller_dll_file="../controllers/DischargeController.dll"\n'
            (folder/(name+'.tgscn')).write_text(s+ENV,encoding='utf-8')
            specifications.append({'name':name,'kind':kind,'group':'cm','solver':solver})
        for kind,data in CONTACTS.items():
            name='stop_'+kind+'_'+tag
            folder=OUT/name
            folder.mkdir(exist_ok=True)
            s=header(name,data['duration'],solver)+component('Hub',5.,[1.,1.,2.])
            for j,(eta,rate) in enumerate(data['initial']):
                dof='[{id="'+str(uuid.uuid5(uuid.NAMESPACE_URL,f'stop/joint/{j}'))+'",name="Hinge '+str(j)+'",motion="rotation",axis=[0.0,0.0,1.0],'+f'initial_coordinate={math.degrees(eta):.17g},initial_rate={math.degrees(rate):.17g},minimum_coordinate={math.degrees(-1.):.17g},maximum_coordinate={math.degrees(.1):.17g},maximum_absolute_rate={math.degrees(10.):.17g},maximum_absolute_effort=100.0'+'}]'
                s+=component('Rotor '+str(j),1.,[.3,.3,.5],'Hub',dofs=dof)
            s+='\n[control]\nmode="none"\n'
            (folder/(name+'.tgscn')).write_text(s+ENV,encoding='utf-8')
            specifications.append({'name':name,'kind':kind,'group':'stop','solver':solver,**data})
    (OUT/'specifications.json').write_text(json.dumps(specifications,indent=2),encoding='utf-8')
    return specifications

NODES,WEIGHTS=np.polynomial.legendre.leggauss(32)
def integral(f,t):
    return .5*t*sum(w*f(.5*t*(x+1)) for x,w in zip(NODES,WEIGHTS))

def discharge(kind,t):
    if kind=='variable_isp':
        return (.5-5/(10+2*t),10/(10+2*t)**2,.5*t-2.5*math.log1p(.2*t),G0*t)
    q0,s=(.4,-.1) if kind=='negative' else (.2,0.) if kind=='constant' else (0.,.1) if kind in ('zero_start','prescribed') else (.2,.1)
    q=q0+s*t
    return q,s,q0*t+.5*s*t*t,10*G0*q

def reference(kind,t):
    q,s,consumed,force=discharge(kind,t)
    mass=15-consumed
    q0=discharge(kind,0)[0]
    material_initial_velocity=30*q0/225
    material_a=lambda u: discharge(kind,u)[3]/(15-discharge(kind,u)[2])
    velocity=material_initial_velocity+integral(material_a,t)-30*q/mass**2
    position=material_initial_velocity*t+integral(lambda u:(t-u)*material_a(u),t)+3*(5-consumed)/mass-1
    if kind=='omitted':
        correction=lambda u:30*discharge(kind,u)[1]/(15-discharge(kind,u)[2])**2
        velocity+=integral(correction,t)
        position+=integral(lambda u:(t-u)*correction(u),t)
    return mass,position,velocity,force/mass

def analyze(specs):
    cases=[]
    for spec in specs:
        p=OUT/spec['name']/'results'/(spec['name']+'_solution.csv')
        with p.open(encoding='utf-8') as stream:
            rows=[{k:float(v) for k,v in row.items()} for row in csv.DictReader(stream)]
        checks=[]
        def check(name,error,tolerance):
            checks.append({'metric':name,'error':float(error),'tolerance':tolerance,'pass':bool(error<=tolerance)})
        check('all recorded ABA solves successful',sum(r['multibody_solve_succeeded']!=1 for r in rows),0)
        if spec['group']=='cm':
            refs=[reference(spec['kind'],r['elapsed_time_seconds']) for r in rows]
            for index,(column,tolerance) in enumerate([('mass_kg',5e-11),('position_icrf_x_m',5e-9),('velocity_icrf_x_mps',5e-9),('base_origin_acceleration_body_x_mps2',5e-10)]):
                check(column,max(abs(r[column]-v[index]) for r,v in zip(rows,refs)),tolerance)
            check('no transverse acceleration response',max(abs(r[k]) for r in rows for k in ('position_icrf_y_m','position_icrf_z_m','angular_velocity_body_z_radps')),1e-11)
        else:
            last=rows[-1]
            check('terminal base angular velocity',abs(last['angular_velocity_body_z_radps']-spec['omega']),5e-8)
            for j,(coordinate,rate) in enumerate(zip(spec['coordinates'],spec['rates'])):
                check('joint '+str(j)+' terminal coordinate',abs(last[f'articulation_coordinate_{j}_rad_or_m']-coordinate),5e-8)
                check('joint '+str(j)+' terminal rate',abs(last[f'articulation_rate_{j}_radps_or_mps']-rate),5e-8)
                check('joint '+str(j)+' upper-limit penetration',max(0.,max(r[f'articulation_coordinate_{j}_rad_or_m'] for r in rows)-.1),5e-10)
            h0=.5*sum(rate for _,rate in spec['initial'])
            check('total angular-momentum conservation',max(abs(r['angular_momentum_about_cm_icrf_z_kgm2ps']-h0) for r in rows),5e-9)
            def energy(r):
                omega=r['angular_velocity_body_z_radps']
                return omega*omega+.25*sum((omega+r[f'articulation_rate_{j}_radps_or_mps'])**2 for j in range(len(spec['initial'])))
            check('kinetic energy does not increase',max(0.,max(energy(r) for r in rows)-energy(rows[0])),5e-9)
        cases.append({'name':spec['name'],'checks':checks,'pass':all(c['pass'] for c in checks)})
    result={'cases':cases,'case_count':len(cases),'checks_passed':sum(c['pass'] for x in cases for c in x['checks']),
            'checks_failed':sum(not c['pass'] for x in cases for c in x['checks'])}
    (OUT/'metrics.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print('Additional features:',result['checks_passed'],'passed,',result['checks_failed'],'failed.',flush=True)
    for case in cases:
        for c in case['checks']:
            if not c['pass']: print(case['name'],c,flush=True)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--analyze-only',action='store_true')
    args=parser.parse_args()
    if args.analyze_only:
        analyze(json.loads((OUT/'specifications.json').read_text(encoding='utf-8')))
        return
    specs=generate()
    paths=[OUT/s['name']/(s['name']+'.tgscn') for s in specs]
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        results=list(pool.map(run_one,paths))
    (ROOT/'Support/Provenance/runs_additional_features.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
    if any(r['exit_code'] for r in results):raise RuntimeError('A new scenario failed.')
    analyze(specs)

if __name__=='__main__': main()
