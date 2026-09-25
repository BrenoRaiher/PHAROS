"""Generate scientific PNG/SVG plots from the retained current-core results."""
from pathlib import Path
import csv
import json
import math
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from analyze_continuous_chain import rotation_axes, angle_degrees

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'figures'

def read(path):
    with path.open(newline='',encoding='utf-8') as stream:return list(csv.DictReader(stream))

def save(fig, name):
    fig.savefig(OUT/(name+'.png'), dpi=190, facecolor='white')
    fig.savefig(OUT/(name+'.svg'), facecolor='white')
    plt.close(fig)

def main():
    OUT.mkdir(exist_ok=True)
    plt.rcParams.update({'font.family':'DejaVu Sans','font.size':10,'axes.titlesize':12,'axes.labelsize':10,'axes.spines.top':False,'axes.spines.right':False,'axes.grid':True,'grid.color':'#dce2e8','grid.alpha':.8,'lines.linewidth':1.5,'legend.frameon':False,'svg.fonttype':'none'})
    manifest=read(ROOT/'analysis/phase_manifest.csv')
    metrics=json.loads((ROOT/'analysis/continuous_jwst_metrics.json').read_text())
    residuals=read(ROOT/'analysis/state_residuals.csv')
    data={r['phase']:read(ROOT/r['solution']) for r in manifest}
    truth={p:read(ROOT/'truth/exact_samples'/f'{p}.csv') for p in data}
    t0=float(residuals[0]['et'])
    colors=['#206c9e','#be4b3b','#31856c']
    r_sim=[];r_ref=[]
    for phase in data:
        for a,b in zip(data[phase],truth[phase]):
            earth=np.array([float(a[f'body_earth_position_icrf_{k}_m']) for k in 'xyz'])
            r_sim.append((np.array([float(a[f'position_icrf_{k}_m']) for k in 'xyz'])-earth)/1000)
            r_ref.append(np.array([float(b[f'position_j2000_{k}_km']) for k in 'xyz'])-earth/1000)
    r_sim=np.array(r_sim);r_ref=np.array(r_ref)
    fig,axes=plt.subplots(1,2,figsize=(11.8,5.3),layout='constrained')
    fig.suptitle('JWST Transfer and Early L2 Coast',fontsize=16,fontweight='bold')
    for ax,j in zip(axes,[1,2]):
        ax.plot(r_sim[:,0]/1000,r_sim[:,j]/1000,color=colors[0],label='PHAROS')
        ax.plot(r_ref[:,0]/1000,r_ref[:,j]/1000,color='#33383e',ls='--',lw=1.1,label='NAIF Reconstruction')
        ax.scatter([0],[0],color=colors[2],s=35,label='Earth',zorder=5)
        ax.scatter(r_sim[0,0]/1000,r_sim[0,j]/1000,marker='o',s=38,facecolor='white',edgecolor=colors[0],zorder=6)
        ax.scatter(r_sim[-1,0]/1000,r_sim[-1,j]/1000,marker='s',s=34,color=colors[0],zorder=6)
        ax.set(xlabel='Earth-Relative X (10³ km)',ylabel=f'Earth-Relative {"YZ"[j-1]} (10³ km)')
        ax.set_aspect('equal',adjustable='datalim')
        ax.legend(loc='best',fontsize=9)
    fig.supxlabel('ICRF Axes; Moving Origin at Earth  •  25 December 2021 – 1 February 2022',fontsize=10)
    save(fig,'01_trajectory')
    days=np.array([(float(r['et'])-t0)/86400 for r in residuals])
    fig,axes=plt.subplots(2,2,figsize=(12,7.3),layout='constrained')
    fig.suptitle('Geometric State Differences: PHAROS Minus NAIF',fontsize=15,fontweight='bold')
    for row,(prefix,unit,scale,title) in enumerate([('dr','m',1000,'Position Difference (km)'),('dv','mps',1,'Velocity Difference (m/s)')]):
        norm_key='position_error_m' if row==0 else 'velocity_error_mps'
        axes[row,0].plot(days,[float(r[norm_key])/scale for r in residuals],color=colors[0])
        axes[row,0].set(ylabel=title,title='Vector Norm' if row==0 else None)
        for k,color in zip('xyz',colors):
            axes[row,1].plot(days,[float(r[f'{prefix}_{k}_{unit}'])/scale for r in residuals],color=color,label=k.upper())
        axes[row,1].set(ylabel=title,title='ICRF Components' if row==0 else None)
        axes[row,1].legend(ncol=3,loc='best')
    events=[('B','MCC-1a'),('D','MCC-1b'),('F','SRP Area Change'),('G','MCC-2')]
    for ax in axes.flat:
        for phase,label in events:
            x=(float(data[phase][0]['ephemeris_time_tdb_seconds_past_j2000'])-t0)/86400
            ax.axvline(x,color='#888f98',lw=.8,ls=':',zorder=0)
        ax.set_xlabel('Days Since 25 December 2021, 13:00 UTC')
    for phase,label in events:
        x=(float(data[phase][0]['ephemeris_time_tdb_seconds_past_j2000'])-t0)/86400
        axes[0,0].text(x,.98,label,rotation=90,va='top',ha='right',fontsize=8,transform=axes[0,0].get_xaxis_transform())
    save(fig,'02_state_residuals')
    fig,axes=plt.subplots(2,3,figsize=(12,6.1),layout='constrained')
    fig.suptitle('Finite Burns and Attitude Tracking',fontsize=15,fontweight='bold')
    for j,burn in enumerate(metrics['burn_performance']):
        samples=data[burn['phase']];time=np.array([float(r['elapsed_time_seconds']) for r in samples])
        thrust=[math.sqrt(sum(float(r[f'force_thrust_icrf_{k}_n'])**2 for k in 'xyz')) for r in samples]
        target=burn['retuned_command_delta_v_vector_icrf_mps']
        angles=[angle_degrees(rotation_axes(r)[0],target) for r in samples]
        axes[0,j].plot(time/60,thrust,color=colors[0]);axes[0,j].axhline(32,color='#a8adb3',ls='--',lw=1)
        axes[0,j].set(title=f'{burn["maneuver"].upper().replace("MCC", "MCC-")}  |  {burn["retuned_command_delta_v_mps"]:.3f} m/s',ylabel='Thrust (N)',ylim=(0,35))
        axes[1,j].plot(time/60,angles,color=colors[1]);axes[1,j].axvline(1,color='#a8adb3',ls=':',lw=1)
        axes[1,j].set(ylabel='Pointing Difference (deg)')
        axes[1,j].ticklabel_format(axis='y',style='sci',scilimits=(-2,3))
        for ax in axes[:,j]:ax.set_xlabel('Time Since Burn Start (min)')
    fig.supxlabel('32 N Nominal Thruster Limit (Dashed)  •  Settled Pointing Assessed After 60 s (Dotted)',fontsize=9)
    save(fig,'03_burns')
    dense=read(ROOT/'truth/reference_audit/dense_one_second.csv')
    nodes=read(ROOT/'truth/reference_audit/type13_nodes.csv')
    center=float(dense[-361]['ephemeris_time_tdb_seconds_past_j2000'])
    minutes=np.array([(float(r['ephemeris_time_tdb_seconds_past_j2000'])-center)/60 for r in dense])
    fig,axes=plt.subplots(2,1,figsize=(10,6.4),layout='constrained')
    fig.suptitle('Local Behavior of the Retained Reference SPK',fontsize=15,fontweight='bold')
    for axis,color in zip('xyz',colors):
        initial=1000*float(dense[0][f'velocity_j2000_{axis}_kmps'])
        axes[0].plot(minutes,[1000*float(r[f'velocity_j2000_{axis}_kmps'])-initial for r in dense],label=axis.upper(),color=color)
        first=nodes[0];t_first=float(first['ephemeris_time_tdb_seconds_past_j2000'])
        node_times=np.array([float(r['ephemeris_time_tdb_seconds_past_j2000']) for r in nodes])
        departure=[float(r[f'position_j2000_{axis}_km'])-float(first[f'position_j2000_{axis}_km'])-(t-t_first)*float(first[f'velocity_j2000_{axis}_kmps']) for r,t in zip(nodes,node_times)]
        axes[1].plot((node_times-center)/60,departure,'o-',label=axis.upper(),color=color,markersize=4)
    axes[0].set(ylabel='Velocity Change From\nPretransition Value (m/s)')
    axes[1].set(ylabel='Node Position Minus Constant-\nVelocity Extrapolation (km)')
    for ax in axes:
        ax.set_xlim(-4,4);ax.set_xlabel('Minutes From 10 January 2022, 00:00 UTC');ax.legend(ncol=3,loc='best')
    fig.supxlabel('Top: Geometric Barycentric Velocity  •  Bottom: Earth-Relative Tabulated Nodes  •  ICRF Axes',fontsize=9)
    save(fig,'04_reference_transition')
    baseline=ROOT/'provenance/trials/04_before_joint_retuning/analysis/state_residuals.csv'
    if (ROOT/'provenance/joint_retuning/nominal_assembly.json').exists() and baseline.exists():
        earlier=read(baseline)
        fig,axes=plt.subplots(2,1,figsize=(11,6.7),layout='constrained')
        fig.suptitle('JWST: Before and After Joint Burn Retuning',fontsize=15,fontweight='bold')
        for ax,key,scale,label in [(axes[0],'position_error_m',1000,'Position Difference (km)'),(axes[1],'velocity_error_mps',1,'Velocity Difference (m/s)')]:
            ax.plot([(float(r['et'])-t0)/86400 for r in earlier],[float(r[key])/scale for r in earlier],color='#89929c',ls='--',label='Before Joint Retuning')
            ax.plot(days,[float(r[key])/scale for r in residuals],color=colors[0],label='Retained Joint Refinement')
            ax.set(xlabel='Days Since 25 December 2021, 13:00 UTC',ylabel=label)
            ax.legend(loc='best')
        fig.supxlabel('Same Physical Model and Burn Schedule  •  Only MCC-1b and MCC-2 Vectors Adjusted',fontsize=10)
        save(fig,'05_retuning_comparison')
        print('Wrote 5 current-result figures in PNG and SVG.')
    else:
        print('Wrote 4 current-result figures in PNG and SVG.')

if __name__=='__main__':main()
