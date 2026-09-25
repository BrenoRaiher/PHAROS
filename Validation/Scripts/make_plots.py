"""Regenerate English validation figures exclusively from this campaign's results."""
from pathlib import Path
import csv
import json
import tomllib
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from run_campaign import ROOT,FEATURE
from additional_features import reference
from verification_figures import (coupled_stops_figure, environment_figure,
    earth_recovery_figure, earth_actuators_figure, earth_recovery_zoom_figure, earth_quaternions_figure, earth_attitude_history_figure)

OUT=ROOT/'Catalog/Figures'
P=ROOT/FEATURE
BLUE='#245a81'; ORANGE='#c05b23'; GREEN='#347961'

def rows(path):
    with path.open(encoding='utf-8',newline='') as stream:
        return list(csv.DictReader(stream))

def col(data,key):return np.array([float(r[key]) for r in data])

def independent_wheel_momenta(wheel):
    """Build the bus and total axial momenta without using total-momentum telemetry.

    This verification case has one rigid bus and a wheel along its z axis.
    Pure z rotation keeps that axis aligned with inertial z, so H_bus,z = Izz*wz.
    Read Izz from the scenario input, and wz and wheel storage from the CSV.
    """
    path=P/'cases/02_actuated_multibody/02e_reaction_wheel/reaction_wheel.tgscn'
    scenario=tomllib.loads(path.read_text(encoding='utf-8'))
    assert len(scenario['components'])==len(scenario['reaction_wheels'])==1
    bus=scenario['components'][0];definition=scenario['reaction_wheels'][0]
    assert not bus['variable_mass'] and not bus['dofs']
    assert bus['local_center_of_mass_m']==bus['origin_body_m']==[0,0,0]
    assert bus['component_to_body']==[1,0,0,0]
    assert definition['mount_component']==bus['name'] and definition['axis_component']==[0,0,1]
    assert bus['inertia']['ixz_kgm2']==bus['inertia']['iyz_kgm2']==0
    for axis in 'xy':
        assert np.all(col(wheel,f'angular_velocity_body_{axis}_radps')==0)
        assert np.all(col(wheel,f'quaternion_body_to_icrf_{axis}')==0)
    wheel_momentum=col(wheel,'reaction_wheel_momentum_0_nms')
    rigid_momentum=bus['inertia']['izz_kgm2']*col(wheel,'angular_velocity_body_z_radps')
    return wheel_momentum,rigid_momentum,rigid_momentum+wheel_momentum

def save(fig,name):
    for extension in ('png','svg'):
        fig.savefig(OUT/(name+'.'+extension),dpi=200,metadata={'Creator':'PHAROS Current Validation'} if extension=='svg' else None)
    plt.close(fig)

def solution(relative):return rows(P/relative)

def cm_comparison_data():
    """Read accepted runs and compute references from the prescribed inputs.

    The offset telemetry is an assembled PHAROS mass property.  Its independent
    reference uses the prescribed discharge integral, never recorded mass or CM.
    The omission effect instead subtracts the two recorded propagated velocities.
    """
    data = {}
    for kind in ('positive', 'negative', 'omitted'):
        for solver in ('rk4', 'dp54'):
            name = 'cm_' + kind + '_' + solver
            d = solution('cases/additional_features/' + name + '/results/' + name + '_solution.csv')
            t = col(d, 'elapsed_time_seconds')
            q0, slope = (.4, -.1) if kind == 'negative' else (.2, .1)
            consumed = q0*t + .5*slope*t*t
            mass_ref = 15. - consumed
            offset_ref = 3.*(5.-consumed)/mass_ref
            velocity = col(d, 'velocity_icrf_x_mps')
            velocity_ref = np.array([reference(kind, float(x))[2] for x in t])
            # The body axes stay parallel to the inertial axes; the hub is at
            # body x=0.  This subtraction removes any origin convention.
            offset = col(d, 'center_of_mass_body_x_m') - col(d, 'component_hub_origin_body_x_m')
            data[kind, solver] = {
                'time': t, 'velocity': velocity, 'velocity_ref': velocity_ref,
                'offset': offset, 'offset_ref': offset_ref,
                'mass': col(d, 'mass_kg'), 'mass_ref': mass_ref,
                'discharge': -col(d, 'mass_rate_kgps'),
                'thrust': col(d, 'force_thrust_icrf_x_n'),
                'initial_state': np.array([float(d[0][key]) for key in (
                    'position_icrf_x_m','position_icrf_y_m','position_icrf_z_m',
                    'velocity_icrf_x_mps','velocity_icrf_y_mps','velocity_icrf_z_mps',
                    'quaternion_body_to_icrf_w','quaternion_body_to_icrf_x',
                    'quaternion_body_to_icrf_y','quaternion_body_to_icrf_z',
                    'angular_velocity_body_x_radps','angular_velocity_body_y_radps',
                    'angular_velocity_body_z_radps','mass_kg')]),
            }
            assert t[0] == 0 and t[-1] == 2 and len(t) == 201
            assert np.max(np.abs(data[kind, solver]['discharge']-(q0+slope*t))) < 1e-14
            assert np.max(np.abs(data[kind, solver]['thrust']-98.0665*(q0+slope*t))) < 1e-12
            for axis in 'xyz':
                assert np.all(col(d, 'angular_velocity_body_'+axis+'_radps') == 0)
                assert np.all(col(d, 'quaternion_body_to_icrf_'+axis) == 0)
    for solver in ('rk4', 'dp54'):
        complete, omitted = data['positive', solver], data['omitted', solver]
        for key in ('time', 'initial_state', 'discharge', 'thrust', 'mass'):
            assert np.array_equal(complete[key], omitted[key]), (solver, key)
    return data


def cm_figure(data=None):
    """Separate mass redistribution, omission effect, and equation agreement."""
    data = cm_comparison_data() if data is None else data
    fig, axes = plt.subplots(3, 1, figsize=(10, 9.2), sharex=True, layout='constrained')
    for kind, label, color in [('positive', 'CM1 Reference', BLUE), ('negative', 'CM2 Reference', GREEN)]:
        d = data[kind, 'rk4']
        axes[0].plot(d['time'], d['offset_ref'], color=color, lw=1.3, label=label)
        for solver, marker, start in [('rk4', 'o', 0), ('dp54', 'x', 10)]:
            d = data[kind, solver]
            sample = np.arange(start, len(d['time']), 20)
            axes[0].plot(d['time'][sample], d['offset'][sample], ls='none',
                         marker=marker, ms=4, mfc='none', mew=.9, color=color)
    for label, marker in [('RK4', 'o'), ('DP5(4)', 'x')]:
        axes[0].plot([], [], color='black', ls='none', marker=marker, ms=4,
                     mfc='none', label=label)
    axes[0].set(ylabel='Centroid Offset\n'+r'($[\mathbf{r}_{C/O_B}]_{\mathcal{B},x}$) [m]', ylim=(.907, 1.035))
    axes[0].legend(loc='upper right', ncols=4, fontsize=8)

    complete, omitted = data['positive', 'rk4'], data['omitted', 'rk4']
    delta_ref = omitted['velocity_ref']-complete['velocity_ref']
    axes[1].plot(complete['time'], 1000*delta_ref, color='black', lw=1.3,
                 label='Independent Reference')
    for solver, marker, start, color in [('rk4', 'o', 0, BLUE), ('dp54', 'x', 10, ORANGE)]:
        complete, omitted = data['positive', solver], data['omitted', solver]
        sample = np.arange(start, len(complete['time']), 20)
        axes[1].plot(complete['time'][sample],
                     1000*(omitted['velocity']-complete['velocity'])[sample],
                     ls='none', marker=marker, ms=4, mfc='none', mew=.9,
                     color=color, label=solver.upper() if solver=='rk4' else 'DP5(4)')
    axes[1].set(ylabel='Omission Effect\n'+r'($[\mathbf{v}_{C/\mathcal{I},\mathrm{CM3}}]_{\mathcal{I},x}-[\mathbf{v}_{C/\mathcal{I},\mathrm{CM1}}]_{\mathcal{I},x}$)'+'\n'+'[mm/s]', ylim=(-1, 34))
    axes[1].legend(loc='upper left', ncols=3, fontsize=8)

    for kind, label, color in [('positive', 'CM1 Complete', BLUE),
                               ('negative', 'CM2 Complete', GREEN),
                               ('omitted', 'CM3 Omitted', ORANGE)]:
        for solver, ls in [('rk4', '-'), ('dp54', '--')]:
            d = data[kind, solver]
            axes[2].plot(d['time'], d['velocity']-d['velocity_ref'], color=color,
                         ls=ls, lw=1.1, label=label + (' RK4' if solver=='rk4' else ' DP5(4)'))
    axes[2].set(ylabel='Equation Agreement\n'+r'($[\mathbf{v}_{C/\mathcal{I},\mathrm{sim}}]_{\mathcal{I},x}-[\mathbf{v}_{C/\mathcal{I},\mathrm{ref}}]_{\mathcal{I},x}$)'+'\n'+'[m/s]',
                xlabel='Elapsed Time (t - t_0) [s]', ylim=(-1.6e-14, 6.5e-14))
    axes[2].ticklabel_format(axis='y', style='sci', scilimits=(0, 0), useMathText=True)
    axes[2].legend(loc='upper left', ncols=3, fontsize=8)
    for ax in axes:
        ax.set_xlim(0, 2)
    return fig


def main():
    OUT.mkdir(exist_ok=True)
    plt.rcParams.update({'font.family':'DejaVu Sans','font.size':10,'axes.spines.top':False,'axes.spines.right':False,'axes.grid':True,'grid.alpha':.18,'axes.titleweight':'bold','svg.fonttype':'none','savefig.facecolor':'white'})
    save(earth_recovery_figure(),'01_Earth_Attitude_Recovery')
    save(earth_actuators_figure(),'02_Earth_Actuator_Use')
    save(earth_recovery_zoom_figure(),'08_Earth_Recovery_Event')

    metrics=json.loads((P/'analysis/cases_01_03_metrics.json').read_text(encoding='utf-8'))
    fig,axes=plt.subplots(1,2,figsize=(11,5.3),layout='constrained')
    labels5=['Revolute Reaction','Prismatic Reaction','Coordinate Stop','Nested Joint Tree','Wheel Exchange','Commanded Thrust','External Torque','Prescribed Thrust']
    labels6=['SRP Plate','Earth Umbra','Component Shadow','Drag Fallback','Aero Database','Three-DOF Interpolation']
    for ax,group,labels,title in zip(axes,['case_2','case_3'],[labels5,labels6],['Multibody and Actuation','Environment Models']):
        ratios=[max((c.get('absolute_error',c.get('maximum_absolute_error',0))/c['tolerance'] for c in v['checks'] if c['tolerance']>0),default=0) for v in metrics[group].values()]
        y=np.arange(len(labels))
        ax.barh(y,np.maximum(ratios,1e-8),color=BLUE)
        for j,value in enumerate(ratios):
            if value==0:ax.text(1.4e-8,j,'0',va='center',fontsize=9)
        ax.set_yticks(y,labels);ax.invert_yaxis();ax.set_xscale('log');ax.set_xlim(1e-8,10)
        ax.axvline(1,color=ORANGE,ls='--',lw=1)
        ax.set(title=title,xlabel='Maximum Error / Acceptance Tolerance')
    passed=metrics['summary']['quantitative_checks_passed'];failed=metrics['summary']['quantitative_checks_failed']
    fig.suptitle(f'Analytic Checks: {passed} of {passed+failed} Passed',fontweight='bold')
    save(fig,'03_Analytic_Checks')

    save(cm_figure(),'04_Updated_CM_Translation')

    save(coupled_stops_figure(),'05_Coupled_Joint_Stops')
    save(environment_figure(),'06_Environment_Loads')
    save(earth_quaternions_figure(),'09_Earth_Quaternion_History')
    save(earth_attitude_history_figure(),'10_Earth_Attitude_History')

    files=list((P/'cases/02_actuated_multibody/02d_nested_tree/results').glob('*solution.csv'))
    if not files:files=list((P/'cases/02_actuated_multibody').glob('05d*/results/*solution.csv'))
    nested=rows(files[0]);wheel=solution('cases/02_actuated_multibody/02e_reaction_wheel/results/reaction_wheel_solution.csv')
    fig,axes=plt.subplots(2,1,figsize=(10,6.4),layout='constrained')
    h=np.array([col(nested,f'angular_momentum_about_cm_icrf_{a}_kgm2ps') for a in 'xyz']).T
    axes[0].plot(col(nested,'elapsed_time_seconds'),np.linalg.norm(h-h[0],axis=1),color=BLUE)
    axes[0].set(title='Nested Joint Tree: Inertial Angular-Momentum Drift',ylabel='|H(t) − H(0)| (N m s)',xlabel='Elapsed Time (s)')
    wh,rigid,total=independent_wheel_momenta(wheel)
    axes[1].plot(col(wheel,'elapsed_time_seconds'),wh,label='Wheel',color=BLUE)
    axes[1].plot(col(wheel,'elapsed_time_seconds'),rigid,label='Rigid Bodies',color=ORANGE)
    axes[1].plot(col(wheel,'elapsed_time_seconds'),total,label='Total',color='black',ls='--')
    axes[1].set(title='Internal Wheel Momentum Exchange',ylabel='Angular Momentum (N m s)',xlabel='Elapsed Time (s)');axes[1].legend()
    save(fig,'07_Multibody_Momentum')
    (OUT/'plot_environment.json').write_text(json.dumps({'matplotlib':matplotlib.__version__,'numpy':np.__version__},indent=2),encoding='utf-8')
    print('Generated ten figures as PNG and editable SVG.')

if __name__=='__main__': main()
