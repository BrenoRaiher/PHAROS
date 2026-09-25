"""Generate standalone scientific figures from the retained numerical evidence."""
from pathlib import Path
import csv,json
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
from continuous_chain import ROOT
from audit import load

OUT=ROOT/'figures';OUT.mkdir(exist_ok=True)
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':10,'axes.titlesize':13,'axes.labelsize':10,'figure.dpi':140,'savefig.dpi':180,'axes.spines.top':False,'axes.spines.right':False,'svg.fonttype':'none'})
BLUE='#1b6394';ORANGE='#c36b19';GREEN='#32805c'
def finish(fig,name):
    for ext in ['png','svg']:fig.savefig(OUT/f'{name}.{ext}',bbox_inches='tight')
    plt.close(fig)
def main():
    data=[np.load(ROOT/'analysis'/f'{i:02d}_curve.npz') for i in range(1,21)]
    origin=data[0]['et'][0];et=np.concatenate([d['et'] for d in data]);x=(et-origin)/86400
    pe=np.concatenate([d['position_error_m'] for d in data])/1000;ve=np.concatenate([d['velocity_error_mps'] for d in data])
    figures=[]
    fig,rows=plt.subplots(3,1,figsize=(11,7),sharex=True,layout='constrained',gridspec_kw={'height_ratios':[.13,1,1]})
    event_axis=rows[0];axs=rows[1:];event_axis.axis('off');event_axis.set_ylim(0,1)
    axs[0].semilogy(x,np.maximum(pe,1e-8),color=BLUE,lw=1);axs[1].semilogy(x,np.maximum(ve,1e-10),color=BLUE,lw=1)
    axs[0].set(ylabel='Position Residual (km)');axs[1].set(ylabel='Velocity Residual (m/s)',xlabel='Days Since Separation')
    for ax in axs:
        ax.grid(True,alpha=.22,which='both');ax.set_xlim(0,x[-1])
    for i,e in enumerate(['venus_1','venus_2','earth','jupiter','phoebe']):
        m=json.loads((ROOT/'analysis'/f'{e}_encounter.json').read_text());day=(m['reference_closest_et']-origin)/86400
        for ax in axs:ax.axvline(day,color='#555555',alpha=.35,lw=.8)
        event_axis.text(day,.15+(i%2)*.65,m['name'],ha='right' if i==4 else 'center',va='center',fontsize=9)
    axs[0].set_ylim(max(float(pe[pe>1e-6].min())*.5,1e-5),float(pe.max())*1.3)
    axs[1].set_ylim(1e-7,float(ve.max())*2)
    fig.suptitle('Cassini: Full Continuous Reconstruction Residuals\nBounded Maneuver Calibration; One External Initial State',fontsize=12)
    finish(fig,'cassini_state_residuals')

    p=np.concatenate([d['position_m'] for d in data])/149597870700.;r=np.concatenate([d['reference_position_m'] for d in data])/149597870700.
    fig,axs=plt.subplots(1,2,figsize=(11,5.5),layout='constrained')
    for ax in axs:
        ax.plot(r[:,0],r[:,1],color=ORANGE,lw=1.6,ls='--',label='Reconstructed Reference');ax.plot(p[:,0],p[:,1],color=BLUE,lw=.9,label='PHAROS');ax.set_aspect('equal');ax.grid(True,alpha=.2);ax.set(xlabel='Barycentric J2000 X (au)',ylabel='Barycentric J2000 Y (au)')
    axs[0].set_title('Separation to Saturn\nOrbit Insertion');axs[1].set(xlim=(-1.5,1.5),ylim=(-1.5,1.5),title='Inner Solar System Detail')
    for ax in axs:ax.plot(p[0,0],p[0,1],'o',color=GREEN,ms=5)
    for stem in ['venus_1','venus_2','earth','jupiter','phoebe']:
        m=json.loads((ROOT/'analysis'/f'{stem}_encounter.json').read_text());t=m['simulated_closest_et'];point=[np.interp(t,et,p[:,i]) for i in range(2)]
        for j,ax in enumerate(axs):
            if j==1 and max(abs(v) for v in point)>1.5:continue
            ax.plot(*point,'o',color=ORANGE,ms=4)
            if j==0 and stem in ['venus_1','venus_2','earth']:continue
            offsets={'venus_1':(-8,-17),'venus_2':(9,8),'earth':(8,-15),'jupiter':(8,8),'phoebe':(8,-15)}
            ax.annotate(m['name'],point,xytext=offsets[stem],textcoords='offset points',fontsize=9)
    axs[0].legend(loc='center left',fontsize=8);fig.suptitle('Cassini Trajectory in the J2000 XY Plane')
    finish(fig,'cassini_trajectory')

    fig,axs=plt.subplots(3,2,figsize=(11,10),layout='constrained')
    for ax,stem in zip(axs.flat,['venus_1','venus_2','earth','jupiter','phoebe']):
        d=np.load(ROOT/'analysis'/f'{stem}_encounter.npz');m=json.loads((ROOT/'analysis'/f'{stem}_encounter.json').read_text());t0=m['reference_closest_et']
        # Use the common 20-minute neighborhood covered by the 1-second reference grid.
        u=np.abs(d['sim_et']-t0)<=1200;xx=(d['sim_et'][u]-t0)/60
        sr=(np.linalg.norm(d['sim_position_m'][u],axis=1)-d['radius_m'])/1000
        rr=(np.linalg.norm(d['ref_position_m'],axis=1)-d['radius_m'])/1000
        reference_min=m['reference_altitude_km'];ax.plot((d['ref_et']-t0)/60,rr-reference_min,color=ORANGE,ls='--',lw=1.5,label='Reference');ax.plot(xx,sr-reference_min,color=BLUE,marker='.',ms=3,lw=1,label='PHAROS Samples')
        ax.set(title=m['name'],xlabel='Minutes From Reference Closest Approach',ylabel='Altitude − Reference Minimum (km)',xlim=(-20,20));ax.grid(True,alpha=.2)
        ax.text(.03,.72 if stem=='jupiter' else .96,f'Range Difference: {m["range_difference_km"]:+.3f} km\nTime Difference: {m["closest_time_difference_s"]:+.3f} s',transform=ax.transAxes,va='top',fontsize=9)
    axs[0,0].legend(loc='lower right',fontsize=8);axs[-1,-1].axis('off');axs[-1,-1].text(0,1,'Each curve uses the same body radius.\n\nThe reported minima are estimated from\npositions and velocities using cubic Hermite\ninterpolation. Reference states are sampled\ndirectly from SPICE at 1 s.\n\nClosest-range agreement differs from\nposition agreement at a common epoch.',va='top',fontsize=11)
    fig.suptitle('Flyby Geometry Around the Reconstructed Closest Approaches');finish(fig,'cassini_encounters')

    mass=np.concatenate([d['mass_kg'] for d in data]);wheel=np.concatenate([d['wheel_momentum_nms'] for d in data])
    fig,axs=plt.subplots(2,1,figsize=(11,6.5),sharex=True,layout='constrained');axs[0].plot(x,mass,color=BLUE);axs[0].set(ylabel='Total Mass (kg)',title='Mass and Reaction-Wheel Histories')
    for i,a in enumerate('XYZ'):axs[1].plot(x,wheel[:,i],lw=.65,label=f'{a} Wheel')
    for sign in [-1,1]:axs[1].axhline(sign*36,color='#444',ls='--',lw=.7)
    axs[1].set(xlabel='Days Since Separation',ylabel='Wheel Momentum (N m s)',ylim=(-39,39));axs[1].legend(ncol=3,fontsize=9,loc='upper center',bbox_to_anchor=(.5,1.12))
    for ax in axs:ax.grid(True,alpha=.2);ax.set_xlim(0,x[-1])
    finish(fig,'cassini_mass_and_wheels')

    summary=json.loads((ROOT/'analysis/summary.json').read_text());count=summary['continuous_sensitivity_segments']
    refs=[json.loads((ROOT/'analysis'/f'{i:02d}_refinement.json').read_text()) for i in range(1,count+1)]
    fig,ax=plt.subplots(figsize=(10,4),layout='constrained');ids=np.arange(1,count+1);ax.bar(ids,[r['maximum_recorded_position_difference_m'] for r in refs],color=BLUE,label='Maximum Within Segment');ax.plot(ids,[r['final_position_difference_m'] for r in refs],'o-',color=ORANGE,label='Segment Endpoint',ms=4)
    ax.set(xlabel='Segment',ylabel='Position Difference (m)',yscale='log',title=f'Continuous Fixed-Command Step Sensitivity Through Segment {count:02d}');ax.set_xticks(ids);ax.grid(axis='y',alpha=.2,which='both');ax.legend();finish(fig,'cassini_numerical_refinement')
    (OUT/'README.md').write_text('# Cassini Figures\n\nEach figure is supplied as PNG and SVG.\n\n'+ '\n'.join(f'- [{name.replace("cassini_", "").replace("_", " ").title()}]({name}.png) · [SVG]({name}.svg)' for name in ['cassini_state_residuals','cassini_trajectory','cassini_encounters','cassini_mass_and_wheels','cassini_numerical_refinement'])+'\n',encoding='utf-8')
    print('Created five scientific figures in PNG and SVG.')

if __name__=='__main__':main()
