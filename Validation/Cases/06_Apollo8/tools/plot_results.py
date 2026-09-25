"""Scientific figures from retained PHAROS outputs and discrete NASA references."""
from pathlib import Path
import json,sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Circle
import mission_metrics as m
ROOT=m.ROOT;OUT=ROOT/'figures';OUT.mkdir(exist_ok=True)
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':10,'axes.spines.top':False,'axes.spines.right':False,'savefig.dpi':180,'axes.grid':True,'grid.alpha':.18})
colors={'outbound':'#2674ad','lunar':'#cb8030','return':'#18816e'}
def save(fig,name):
    fig.savefig(OUT/(name+'.png'),bbox_inches='tight',facecolor='white');fig.savefig(OUT/(name+'.svg'),bbox_inches='tight',facecolor='white');plt.close(fig)

def residual_figure():
    data=m.js(ROOT/'analysis/historical_comparisons.json')
    records=list(data['direct_states'].values());table=data['rounded_maneuver_positions']
    fig,axes=plt.subplots(2,1,sharex=True,layout='constrained')
    for j,key,scale in [(0,'position_error_m',1000),(1,'velocity_error_mps',1)]:
        ax=axes[j]
        ax.scatter([r['get_seconds']/3600 for r in records],[r[key]/scale for r in records],s=27,marker='o',facecolors='white',edgecolors='#2674ad',zorder=4,label='TRW States')
        ax.axvspan(248900.4/3600,321556.6/3600,color='#cb8030',alpha=.085,zorder=0)
        ax.set(xlim=(0,146.48),yscale='log')
    axes[0].scatter([r['get_seconds']/3600 for r in table],[r['position_error_m']/1000 for r in table],s=27,marker='+',linewidths=1,color='#bd4d16',label='Maneuver Table',zorder=5)
    calibrated=[r for r in records+table if r.get('position_informed_calibration')]
    axes[0].scatter([r['get_seconds']/3600 for r in calibrated],[r['position_error_m']/1000 for r in calibrated],s=67,marker='D',facecolors='none',edgecolors='#303030',linewidths=.8,label='Calibration Epochs',zorder=6)
    axes[0].set(ylim=(.65,155));axes[1].set(ylim=(.06,85))
    axes[0].text(79,.96,'Lunar Orbit',ha='center',va='top',transform=axes[0].get_xaxis_transform(),fontsize=8)
    for ax in axes:ax.grid(axis='y',which='major',alpha=.2)
    axes[0].legend(loc='lower left',fontsize=8,ncol=3)
    axes[0].set_ylabel('Position Residual (km)')
    axes[1].set(ylabel='Velocity Residual (m/s)',xlabel='Ground Elapsed Time (h)')
    return fig

def main():
    chain=m.load(ROOT);audit=m.js(ROOT/'analysis/final_audit.json')
    fig,ax=plt.subplots(1,2,figsize=(12,5.4),layout='constrained');seen=set()
    rp,rv=m.rv(chain['06']['rows'][-1]);bp,bv=m.rv(chain['06']['rows'][-1],'body_moon_');rp-=bp;rv-=bv
    e1=rp/m.norm(rp);e3=np.cross(rp,rv);e3/=m.norm(e3);e2=np.cross(e3,e1);basis=np.array([e1,e2]).T
    for sid,part in chain.items():
        group='outbound' if int(sid)<=5 else ('lunar' if int(sid)<=9 else 'return')
        p=np.array([m.rv(r)[0] for r in part['rows']]);e=np.array([m.rv(r,'body_earth_')[0] for r in part['rows']]);moon=np.array([m.rv(r,'body_moon_')[0] for r in part['rows']])
        ax[0].plot((p-e)[:,0]/1e6,(p-e)[:,1]/1e6,color=colors[group],lw=1.4,label=group.title() if group not in seen else None)
        ax[0].plot((moon-e)[:,0]/1e6,(moon-e)[:,1]/1e6,color='#8b9299',lw=.7,alpha=.7,label='Moon Path' if sid=='01' else None)
        projected=(p-moon)@basis/1000
        if 5<int(sid)<11:ax[1].plot(projected[:,0],projected[:,1],color=colors[group],lw=1,alpha=.8)
        seen.add(group)
    ax[0].add_patch(Circle((0,0),6.378,facecolor='#526b85',edgecolor='none'));ax[0].annotate('Earth',(0,0),xytext=(8,12),textcoords='offset points')
    ax[0].set(xlabel='ICRF X relative to Earth (10³ km)',ylabel='ICRF Y relative to Earth (10³ km)',title='Transfer and Return');ax[0].legend(loc='lower left');ax[0].set_aspect('equal',adjustable='datalim')
    ax[1].add_patch(Circle((0,0),1737.4,facecolor='#c3c5c9',edgecolor='#959ba2',lw=.8));ax[1].text(0,0,'Moon',ha='center',va='center',color='#444b54')
    refs=m.js(ROOT/'expected/historical_states.json')
    comparisons=m.js(ROOT/'analysis/historical_comparisons.json')
    for j in range(2):
        direct=np.array([r['position_earth_relative_j2000_m'][:2] if j==0 else np.array(r['position_moon_relative_j2000_m'])@basis for r in refs.values()])/(1e6 if j==0 else 1000)
        rounded=np.array([r['position_earth_relative_j2000_m'][:2] if j==0 else np.array(r['position_moon_relative_j2000_m'])@basis for r in comparisons['rounded_maneuver_positions']])/(1e6 if j==0 else 1000)
        ax[j].scatter(direct[:,0],direct[:,1],s=24,marker='x',linewidths=.85,color='#732c66',zorder=6,label='TRW States')
        ax[j].scatter(rounded[:,0],rounded[:,1],s=24,marker='+',linewidths=.85,color='#bd4d16',zorder=5,label='Maneuver Table')
    ax[0].legend(loc='lower left',fontsize=8)
    ax[1].set(xlabel='In-Plane u relative to Moon (km)',ylabel='In-Plane v relative to Moon (km)',title='Lunar Orbit Plane Projection',xlim=(-2400,2400),ylim=(-2400,2400));ax[1].set_aspect('equal');ax[1].legend(loc='lower left',fontsize=9)
    fig.suptitle('Apollo 8 Reconstruction — Current PHAROS',fontweight='bold');save(fig,'trajectory')
    save(residual_figure(),'state_residuals')
    fig,axes=plt.subplots(3,1,figsize=(11,8),sharex=True,layout='constrained')
    for sid,part in chain.items():
        rows=part['rows'];t=np.array([float(r['elapsed_time_seconds'])+part['meta']['start_get_seconds'] for r in rows])/3600
        axes[0].plot(t,[float(r['mass_kg'])/1000 for r in rows],color='#2674ad',lw=1.5)
        p=np.array([m.rv(r)[0] for r in rows]);moon=np.array([m.rv(r,'body_moon_')[0] for r in rows]);alt=(np.linalg.norm(p-moon,axis=1)-m.MOON_RADIUS)/1000
        if 5<int(sid)<10:axes[1].plot(t,alt,color='#cb8030',lw=1)
        force=np.array([[float(r['force_thrust_icrf_'+a+'_n']) for a in 'xyz'] for r in rows]);axes[2].plot(t,np.linalg.norm(force,axis=1)/1000,color='#18816e',lw=1)
    for b in audit['burns']:
        t=b['scheduled_ignition_get']/3600;axes[0].axvline(t,color='#bbb',lw=.5,ls=':')
    axes[0].set(ylabel='Mass (10³ kg)',title='Continuous State and Finite Burns');axes[1].set(ylabel='Lunar Altitude (km)',ylim=(0,400));axes[2].set(ylabel='Net Thrust (kN)',xlabel='Ground Elapsed Time (h)');save(fig,'mission_profiles')
    print('Saved 3 PNG/SVG figure pairs.')
if __name__=='__main__':main()
