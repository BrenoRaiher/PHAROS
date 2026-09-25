"""Verification figure layouts from recorded results; no simulation execution."""
from pathlib import Path
import csv, hashlib, json, math, tomllib
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from run_campaign import ROOT, FEATURE

P = ROOT / FEATURE
BLUE, ORANGE, GREEN = '#245a81', '#c05b23', '#347961'
SOURCES = {}

def source(path):
    path = Path(path)
    SOURCES[str(path.relative_to(ROOT)).replace('\\','/')] = hashlib.sha256(path.read_bytes()).hexdigest()
    return path

def records(path):
    with source(path).open(encoding='utf-8', newline='') as f:
        return list(csv.DictReader(f))

def values(rows, key):
    return np.array([float(r[key]) for r in rows])

def document(path):
    return json.loads(source(path).read_text(encoding='utf-8'))

def coupled_stops_figure():
    fig, axes = plt.subplots(3, 1, figsize=(6.55, 6.4), layout='constrained')
    for ax, kind in zip(axes, ('both_lock','release','sequential')):
        run = 'stop_'+kind+'_rk4'
        path = P/'cases/additional_features'/run/(run+'.tgscn')
        cfg = tomllib.loads(source(path).read_text())
        hub, first, second = cfg['components']
        ih = hub['inertia']['izz_kgm2']
        ir = [c['inertia']['izz_kgm2'] for c in (first,second)]
        q = [math.radians(c['dofs'][0]['initial_coordinate']) for c in (first,second)]
        w = [math.radians(c['dofs'][0]['initial_rate']) for c in (first,second)]
        limit = math.radians(first['dofs'][0]['maximum_coordinate'])
        t1 = (limit-q[0])/w[0]
        end = cfg['scenario']['duration_seconds']
        if kind == 'both_lock':
            base = sum(i*v for i,v in zip(ir,w))/(ih+sum(ir))
            times = [0,t1,end]
            rates = [[0,base,base],[w[0],0,0],[w[1],0,0]]
        else:
            base = ir[0]*w[0]/(ih+ir[0])
            if kind == 'release':
                times = [0,t1,end]
                rates = [[0,base,base],[w[0],0,0],[w[1],w[1]-base,w[1]-base]]
            else:
                t2 = t1+(limit-q[1]-w[1]*t1)/(w[1]-base)
                base2 = (ih*base+ir[1]*w[1])/(ih+ir[1])
                times = [0,t1,t2,end]
                rates = [[0,base,base2,base2],[w[0],0,base-base2,base-base2],
                         [w[1],w[1]-base,0,0]]
        # These piecewise lines follow momentum/contact references from the inputs.
        # They are not interpolated PHAROS output histories.
        for y, color, ls, name in zip(rates, ('black',BLUE,ORANGE), ('-','--',':'),
                                     ('Base','Joint 1','Joint 2')):
            ax.step(times,y,where='post',color=color,ls=ls,lw=1.25,label=name)
        for solver, marker, offset in (('rk4','o',0),('dp54','x',1)):
            run='stop_'+kind+'_'+solver
            data=records(P/'cases/additional_features'/run/'results'/(run+'_solution.csv'))
            t=values(data,'elapsed_time_seconds')
            # Use one sample selection for all three rates, distinct between integrators.
            idx=np.arange(offset,len(t),5)
            for j,(key,color) in enumerate(zip(
                    ('angular_velocity_body_z_radps','articulation_rate_0_radps_or_mps','articulation_rate_1_radps_or_mps'),
                    ('black',BLUE,ORANGE))):
                ax.plot(t[idx],values(data,key)[idx],ls='none',marker=marker,color=color,
                        ms=(4.5,6.0,3.8)[j],mfc='none',mew=.8)
        ax.set_xlim(0,end)
        ax.set_ylim(-.2,1.32)
        ax.set_yticks([-.1,0,.5,1])
    axes[0].plot([],[],ls='none',marker='o',mfc='none',color='.25',label='RK4')
    axes[0].plot([],[],ls='none',marker='x',color='.25',label='DP5(4)')
    return fig

def environment_figure():
    metrics=document(P/'analysis/cases_01_03_metrics.json')['case_3']
    fig,axes=plt.subplots(1,2,figsize=(7,3.8),layout='constrained')
    rad=[metrics['03a_absorbing_plate_at_one_au']['checks'][1],
         metrics['03b_earth_umbra_on_off']['checks'][1],
         *metrics['03c_component_shadow_on_off']['checks'][:2]]
    axes[0].bar(range(4),[c['expected']*1e6 for c in rad],width=.62,
                color='#cfdae2',edgecolor=BLUE,label='Reference')
    axes[0].plot(range(4),[c['actual']*1e6 for c in rad],ls='none',marker='o',
                 color='black',ms=4,mfc='none',label='PHAROS')
    axes[0].text(1,.3,'0',ha='center',fontsize=8)
    axes[0].axvline(1.5,color='.55',ls=':',lw=.8)
    axes[0].set_ylim(-.3,12)
    axes[0].set_xticks(range(4),['Plate\nE02','Umbra\nE05','Shadow Off\nE08','Shadow On\nE09'])
    aero=[metrics['03d_rarefied_constant_drag_fallback']['checks'][2],
          metrics['03e_aerodynamic_database_and_moment_transport']['checks'][1],
          metrics['03e_aerodynamic_database_and_moment_transport']['checks'][3]]
    ratios=[c['absolute_error']/c['tolerance'] for c in aero]
    axes[1].bar(range(3),ratios,color=BLUE,width=.55)
    axes[1].axhline(1,color='.3',ls='--',lw=.9,label='Acceptance Limit')
    for j,(c,ratio) in enumerate(zip(aero,ratios)):
        unit=r'\mathrm{N}' if j<2 else r'\mathrm{N\,m}'
        exponent=math.floor(math.log10(c['absolute_error']))
        mantissa=c['absolute_error']/10**exponent
        axes[1].text(j,ratio+.035,r'$'+format(mantissa,'.2f')+r'\!\times\!10^{'+str(exponent)+'}$'+'\n'+r'$'+unit+'$',
                     ha='center',va='bottom',fontsize=7.5)
    axes[1].set_ylim(0,1.24)
    axes[1].set_xticks(range(3),['Fallback\nForce Error\nE13','Database\nForce Error\nE16',r'CM $z$ Moment'+'\nError\nE18'])
    return fig

def earth_data():
    m=document(ROOT/'Cases/04_EarthOrbit/earth_attitude_recovery_metrics.json')
    raw=records(ROOT/'Cases/04_EarthOrbit/earth_attitude_recovery_timeseries.csv')
    data={case:[r for r in raw if r['case']==case] for case in m['cases']}
    return m,data

def acquisition_end(data):
    ends=[]
    for rows in data.values():
        t=values(rows,'elapsed_time_seconds')
        good=(values(rows,'sun_pointing_error_deg')<.5)&(values(rows,'angular_rate_radps')<.002)
        for i in range(len(t)-40):
            if good[i:i+41].all() and t[i+40]-t[i]>=20-1e-9:
                ends.append(t[i+40]);break
    return max(ends)

def earth_events(axes,metrics,data):
    finish=max(float(r['elapsed_time_seconds']) for rows in data.values() for r in rows)
    for ax in axes:
        ax.set_xlim(0,finish)
        ax.axvspan(0,acquisition_end(data),color='.5',alpha=.1,zorder=-2)
        for event in metrics['disturbance_schedule']['events']:
            ax.axvline(event['start_s'],color='.45',ls=':',lw=.65,zorder=-1)
    axes[0].text(.025,.72,'Initial\nAcquisition',transform=axes[0].transAxes,fontsize=7.5,
                 bbox=dict(facecolor='white',edgecolor='none',alpha=.85,pad=1))


def earth_quaternions_figure():
    metrics,data=earth_data()
    fig,axes=plt.subplots(2,1,figsize=(6.55,5.0),sharex=True,layout='constrained')
    for ax,run in zip(axes,('04a_four_wheel_recovery','04b_thruster_recovery')):
        rows=records(ROOT/'Cases/04_EarthOrbit/results'/run/(run+'_solution.csv'))
        t=values(rows,'elapsed_time_seconds')
        for j,(entry,color,ls) in enumerate(zip(('w','x','y','z'),
                    (BLUE,ORANGE,GREEN,'#8064a2'),('-','--','-.',':'))):
            ax.plot(t,values(rows,'quaternion_body_to_icrf_'+entry),
                    color=color,ls=ls,lw=1,label=rf'$q_{j}$')
        ax.set_ylim(-1.05,1.05)
        ax.set_yticks([-1,-.5,0,.5,1])
    earth_events(axes,metrics,data)
    return fig

def earth_recovery_figure():
    metrics,data=earth_data()
    fig,axes=plt.subplots(2,1,figsize=(6.55,4.7),sharex=True,layout='constrained')
    for case,label,color,ls in [('four_wheel','Four Wheels',BLUE,'-'),('attitude_thruster','Attitude Thrusters',ORANGE,'--')]:
        d=data[case];t=values(d,'elapsed_time_seconds')
        for ax,key in zip(axes,('sun_pointing_error_deg','angular_rate_radps')):
            ax.plot(t,np.maximum(values(d,key),1e-8),color=color,ls=ls,lw=1,label=label)
    for ax,limit in zip(axes,(.5,.002)):
        ax.set_yscale('log');ax.axhline(limit,color='black',ls='--',lw=.8,label='Recovery Limit')
    axes[0].set_ylim(1e-5,300)
    axes[1].set_ylim(1e-8,.08)
    earth_events(axes,metrics,data)
    return fig


def earth_attitude_history_figure():
    metrics,data=earth_data()
    fig,axes=plt.subplots(4,1,figsize=(6.55,8.05),sharex=True,layout='constrained')
    for case,label,color,ls in [('four_wheel','Four Wheels',BLUE,'-'),('attitude_thruster','Attitude Thrusters',ORANGE,'--')]:
        d=data[case];t=values(d,'elapsed_time_seconds')
        for ax,key in zip(axes[:2],('sun_pointing_error_deg','angular_rate_radps')):
            ax.plot(t,np.maximum(values(d,key),1e-8),color=color,ls=ls,lw=1,label=label)
    for ax,limit in zip(axes[:2],(.5,.002)):
        ax.set_yscale('log');ax.axhline(limit,color='black',ls='--',lw=.8,label='Recovery Limit')
    axes[0].set_ylim(1e-5,300);axes[1].set_ylim(1e-8,.08)
    for ax,run in zip(axes[2:],('04a_four_wheel_recovery','04b_thruster_recovery')):
        rows=records(ROOT/'Cases/04_EarthOrbit/results'/run/(run+'_solution.csv'))
        t=values(rows,'elapsed_time_seconds')
        for j,(entry,color,ls) in enumerate(zip(('w','x','y','z'),(BLUE,ORANGE,GREEN,'#8064a2'),('-','--','-.',':'))):
            ax.plot(t,values(rows,'quaternion_body_to_icrf_'+entry),color=color,ls=ls,lw=1,label=rf'$q_{j}$')
        ax.set_ylim(-1.05,1.05);ax.set_yticks([-1,-.5,0,.5,1])
    earth_events(axes,metrics,data)
    return fig

def earth_actuators_figure():
    metrics,data=earth_data()
    fig,axes=plt.subplots(2,1,figsize=(6.55,4.5),sharex=True,layout='constrained')
    d=data['four_wheel'];t=values(d,'elapsed_time_seconds')
    for j,ls in enumerate(('-','--','-.',':')):
        axes[0].plot(t,values(d,f'wheel_{j}_nms'),ls=ls,lw=1,label=rf'$h_{j+1}$')
    axes[0].axhline(50,color='black',ls='--',lw=.8,label='Capacity')
    axes[0].axhline(-50,color='black',ls='--',lw=.8)
    axes[0].set_ylim(-55,75)
    for case,label,color,ls in [('four_wheel','Four Wheels',BLUE,'-'),('attitude_thruster','Attitude Thrusters',ORANGE,'--')]:
        d=data[case];mass=values(d,'mass_kg')
        axes[1].plot(values(d,'elapsed_time_seconds'),(mass[0]-mass)*1000,color=color,ls=ls,label=label)
    earth_events(axes,metrics,data)
    return fig

def earth_recovery_zoom_figure():
    metrics,data=earth_data()
    longest=max((e for c in metrics['cases'].values() for e in c['events']),key=lambda e:e['recovery_duration_s'])
    event_id=longest['event'];origin=longest['recovery_enabled_time_s']
    fig=plt.figure(figsize=(6.55,5.9),layout='constrained')
    grid=fig.add_gridspec(3,1,height_ratios=(.6,2,2))
    timeline=fig.add_subplot(grid[0])
    axes=[fig.add_subplot(grid[1]),fig.add_subplot(grid[2])]
    for case,label,color,ls in [('four_wheel','Four Wheels',BLUE,'-'),('attitude_thruster','Attitude Thrusters',ORANGE,'--')]:
        d=data[case];t=values(d,'elapsed_time_seconds')-origin
        mask=(t>=-10)&(t<=205)
        event=metrics['cases'][case]['events'][event_id-1]
        begin=event['sustained_recovery_time_s']-origin
        for ax,key in zip(axes,('sun_pointing_error_deg','angular_rate_radps')):
            y=np.maximum(values(d,key),1e-8)
            ax.plot(t[mask],y[mask],color=color,ls=ls,lw=1.1,label=label)
            # The shaded windows use the qualifying epochs selected by the archived analysis.
            ax.axvspan(begin,begin+20,color=color,alpha=.12,zorder=-2)
            ax.axvline(begin,color=color,ls='-.',lw=.8)
            ax.axvline(begin+20,color=color,ls=':',lw=.8)
            selected=(t>=begin-1e-7)&(t<=begin+20+1e-7)
            assert np.all(y[selected] < (.5 if key.startswith('sun_') else .002))
        axes[0].text(begin+10,.85,'20 s',transform=axes[0].get_xaxis_transform(),color=color,
                     ha='center',fontsize=8)
    for ax,limit in zip(axes,(.5,.002)):
        ax.set_yscale('log');ax.set_xlim(-10,205)
        ax.axhline(limit,color='black',ls='--',lw=.8,label='Recovery Limit')
        ax.axvline(0,color='.3',lw=.8)
    axes[0].set_ylim(.006,100)
    axes[1].set_ylim(1e-5,.06)
    # A separate strip resolves the short pulse without covering either history.
    ignition=longest['start_s']-origin
    timeline.axvspan(ignition,-5,ymax=.7,color='#d7bca9',alpha=.85)
    timeline.axvspan(-5,0,ymax=.7,color='.85')
    timeline.vlines([-5,0],0,.72,color='.3',lw=.8)
    timeline.set(xlim=(-7,.5),ylim=(0,1),yticks=[],xticks=[ignition,-5,0],xticklabels=[format(ignition,'.2f'),'-5','0'])
    timeline.text((ignition-5)/2,.35,'Pulse',ha='center',va='center',fontsize=8)
    timeline.text(-2.5,.35,'5 s Holdoff',ha='center',va='center',fontsize=8)
    timeline.text(-5,.77,'Shutdown',ha='center',va='bottom',fontsize=8)
    timeline.text(0,.77,'Re-enable',ha='center',va='bottom',fontsize=8)
    timeline.grid(False)
    return fig
