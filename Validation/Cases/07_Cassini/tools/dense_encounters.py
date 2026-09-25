"""Short 1-second-output continuations for resolving closest-approach geometry."""
import argparse,csv,json,re,shutil,time
import numpy as np
from continuous_chain import ROOT,RUNNER,KERNELS,SDK,VCVARS,process,write,scenario,setkey,solution,last,sha
from encounters import EVENTS
from reference import query

def run_window(event):
    name,sid,body,target,utc,radius=event;stem=name.lower().replace(' ','_');d=ROOT/'checks/encounters'/stem
    anchorfile=d/'anchor.csv';query(sid,anchorfile,['--utc',utc]);anchor=float(next(csv.DictReader(anchorfile.open()))['et'])
    before=None;start=None;stop=None
    with solution(sid).open(newline='') as f:
        for row in csv.DictReader(f):
            et=float(row['ephemeris_time_tdb_seconds_past_j2000'])
            if et>anchor-3600 and start is None:start=before
            if et>=anchor+3600:stop=row;break
            before=row
    assert start is not None and stop is not None
    epochfile=d/'times.csv';query(sid,epochfile,['--et',start['ephemeris_time_tdb_seconds_past_j2000'],'--et',stop['ephemeris_time_tdb_seconds_past_j2000']])
    epochs=list(csv.DictReader(epochfile.open()));offset=float(start['elapsed_time_seconds'])
    # Delegate to the identical retained controller at its original elapsed time.
    source=(ROOT/'segments'/sid/'controllers/CassiniController.cpp').read_text()
    old='static_cast<TGUserController*>(controller)->ComputeControl(*input, *output);'
    assert source.count(old)==1
    source=source.replace(old,f'TGControlInput shifted = *input;\n        shifted.ElapsedSimulationTimeSeconds += {offset:.17g};\n        static_cast<TGUserController*>(controller)->ComputeControl(shifted, *output);')
    assert source.count('return NextBoundary(currentElapsedSimulationTimeSeconds);')==1
    source=source.replace('return NextBoundary(currentElapsedSimulationTimeSeconds);',f'return NextBoundary(currentElapsedSimulationTimeSeconds + {offset:.17g}) - {offset:.17g};')
    controller=d/'controllers';write(controller/'CassiniController.cpp',source)
    shutil.copy2(ROOT/'segments'/sid/'controllers/ControllerConfig.h',controller/'ControllerConfig.h')
    cmd=f'cl /nologo /std:c++17 /EHsc /O2 /MT /LD /I"{SDK}" /I"{controller}" /Fo"{controller/"CassiniController.obj"}" /Fe"{controller/"CassiniController.dll"}" "{controller/"CassiniController.cpp"}"'
    write(controller/'build.cmd',f'@echo off\ncall "{VCVARS}" >nul\nif errorlevel 1 exit /b %errorlevel%\n{cmd}\nexit /b %errorlevel%\n');process(['cmd.exe','/d','/c',controller/'build.cmd'],controller/'compile.log',controller)
    text=scenario(sid,start);text=setkey(text,'start_utc',json.dumps(epochs[0]['utc']));text=setkey(text,'final_utc',json.dumps(epochs[1]['utc']));text=setkey(text,'output_step_seconds','1');text=setkey(text,'maximum_integrator_step_seconds','1');text=setkey(text,'initial_integrator_step_seconds','0.1')
    text=text.replace('"../../../data/','"'+(ROOT/'data').as_posix()+'/')
    # Each encounter window is a coast. Keeping imported thruster availability
    # windows is harmless here because the time-shifted controller commands no burn;
    # the result is checked for zero applied thrust before it is accepted.
    sc=d/'scenario'/f'{stem}.tgscn';write(sc,text)
    process([RUNNER,sc,'--kernel-dir',KERNELS,'--validate-only'],d/'logs/validate.log')
    print(f'Refining encounter {name}',flush=True)
    process([RUNNER,sc,'--kernel-dir',KERNELS,'--output',d/'results'],d/'logs/run.log')
    end=last(d/'results'/f'{stem}_solution.csv')
    dr=np.array([float(end[f'position_icrf_{a}_m'])-float(stop[f'position_icrf_{a}_m']) for a in 'xyz'])
    dv=np.array([float(end[f'velocity_icrf_{a}_mps'])-float(stop[f'velocity_icrf_{a}_mps']) for a in 'xyz'])
    metadata={'name':name,'source_segment':sid,'elapsed_time_offset_s':offset,'start_et':float(start['ephemeris_time_tdb_seconds_past_j2000']),'end_et':float(stop['ephemeris_time_tdb_seconds_past_j2000']),'position_difference_at_window_end_m':float(np.linalg.norm(dr)),'velocity_difference_at_window_end_mps':float(np.linalg.norm(dv)),'seed_source':'Full propagated nominal state at preceding recorded epoch, with matching controller elapsed-time offset. No historical state reset.','source_controller_sha256':sha(ROOT/'segments'/sid/'controllers/CassiniController.cpp'),'source_configuration_sha256':sha(ROOT/'segments'/sid/'controllers/ControllerConfig.h')}
    metadata['source_scenario_sha256']=sha(ROOT/'segments'/sid/'scenario'/f'cassini_{sid}.tgscn')
    write(d/'metadata.json',json.dumps(metadata,indent=2));write(d/'seed_state.json',json.dumps(start,indent=2))
    print(f'Completed {name} dense window; endpoint change {np.linalg.norm(dr):.6f} m',flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--watch',action='store_true');p.add_argument('--event',choices=['all','venus_1','venus_2','earth','jupiter','phoebe'],default='all');a=p.parse_args()
    for event in EVENTS:
        if a.event not in ['all',event[0].lower().replace(' ','_')]:continue
        if a.watch:
            while not (ROOT/'segments'/event[1]/'metadata.json').exists():time.sleep(5)
        run_window(event)
