"""Audit a rapid reference transition visible in the residual plot.

Locates the largest departure from integrated velocity between consecutive
coast samples, then queries the same SPK more closely without changing it.
"""

# Shared repository locations; validation cases retain their internal paths.
from pathlib import Path as _RepoPath
import os as _repo_os
_REPOSITORY = next(p for p in _RepoPath(__file__).resolve().parents if (p / 'PHAROS.uproject').is_file())
_VALIDATION = _REPOSITORY / 'Validation'
_RUNTIME = _VALIDATION / 'Support/Runtime'
_KERNELS = _RepoPath(_repo_os.environ.get('PHAROS_KERNELS', _REPOSITORY / 'Content/SPICEKernels'))

from pathlib import Path
import csv
import json
import math
import subprocess
import struct
import bisect

ROOT=Path(__file__).resolve().parents[1]

def read(path):
    with path.open(newline='',encoding='utf-8') as f:return list(csv.DictReader(f))

def position_defect(a,b,kind):
    if kind=='reference':
        r=lambda s,k:1000*float(s[f'position_j2000_{k}_km'])
        v=lambda s,k:1000*float(s[f'velocity_j2000_{k}_kmps'])
    else:
        r=lambda s,k:float(s[f'position_icrf_{k}_m'])
        v=lambda s,k:float(s[f'velocity_icrf_{k}_mps'])
    dt=float(b['ephemeris_time_tdb_seconds_past_j2000'])-float(a['ephemeris_time_tdb_seconds_past_j2000'])
    vector=[r(b,k)-r(a,k)-dt*(v(a,k)+v(b,k))/2 for k in 'xyz']
    return math.sqrt(sum(x*x for x in vector))

def query(epochs, name):
    folder=ROOT/'truth/reference_audit';folder.mkdir(exist_ok=True)
    etfile=folder/(name+'_et.txt');output=folder/(name+'.csv')
    etfile.write_text('\n'.join(format(t,'.17g') for t in epochs),encoding='utf-8')
    runtime=_KERNELS
    command=[str(ROOT/'analysis/SpiceReference.exe'),'--kernel',str(runtime/'naif0012.tls'),'--kernel',str(runtime/'de442.bsp'),'--kernel',str(ROOT/'truth/kernels/jwst_rec.bsp'),'--et-file',str(etfile),'--output',str(output)]
    subprocess.run(command,check=True,capture_output=True,text=True)
    return read(output)

def main():
    reference=read(ROOT/'truth/exact_samples/F.csv')
    manifest=read(ROOT/'analysis/phase_manifest.csv')
    simulation=read(ROOT/next(r['solution'] for r in manifest if r['phase']=='F'))
    index=max(range(1,len(reference)),key=lambda j:position_defect(reference[j-1],reference[j],'reference'))
    left=float(reference[index-1]['ephemeris_time_tdb_seconds_past_j2000'])
    right=float(reference[index]['ephemeris_time_tdb_seconds_past_j2000'])
    # Locate the sharp hourly change, then resolve its interpolation behavior.
    coarse=query([left+(right-left)*k/60 for k in range(61)],'coarse')
    j=max(range(1,len(coarse)),key=lambda k:position_defect(coarse[k-1],coarse[k],'reference'))
    lo=float(coarse[j-1]['ephemeris_time_tdb_seconds_past_j2000']);hi=float(coarse[j]['ephemeris_time_tdb_seconds_past_j2000'])
    fine=query([lo+(hi-lo)*k/60 for k in range(61)],'fine')
    k=max(range(1,len(fine)),key=lambda n:position_defect(fine[n-1],fine[n],'reference'))
    a,b=fine[k-1],fine[k]
    dense=query([left+float(k) for k in range(int(right-left)+361)],'dense_one_second')
    baseline_velocity=[1000*float(reference[index-1][f'velocity_j2000_{axis}_kmps']) for axis in 'xyz']
    deviations=[math.sqrt(sum((1000*float(r[f'velocity_j2000_{axis}_kmps'])-baseline_velocity[i])**2 for i,axis in enumerate('xyz'))) for r in dense]
    max_velocity_step=max(math.sqrt(sum((1000*float(y[f'velocity_j2000_{axis}_kmps'])-1000*float(x[f'velocity_j2000_{axis}_kmps']))**2 for axis in 'xyz')) for x,y in zip(dense,dense[1:]))
    # Inspect the underlying type-13 interpolation nodes using the DAF addresses
    # provided by CSPICE and the documented type-13 layout. No SPK is modified.
    segments=read(ROOT/'truth/reference_audit/spk_segments.csv')
    segment=next(s for s in reversed(segments) if float(s['start_et'])<=right<=float(s['end_et']) and s['target']=='-170')
    assert segment['type']=='13'
    with (ROOT/'truth/kernels/jwst_rec.bsp').open('rb') as spk:
        assert b'LTL-IEEE' in spk.read(1024)
        def doubles(address,count):
            spk.seek((address-1)*8)
            return struct.unpack('<'+'d'*count,spk.read(8*count))
        first=int(segment['first_address']);last=int(segment['last_address'])
        window_minus_one,n=doubles(last-1,2);n=int(n)
        epochs=doubles(first+6*n,n)
        mid=bisect.bisect_left(epochs,right)
        nodes=[]
        for j in range(max(0,mid-4),min(n,mid+5)):
            state=doubles(first+6*j,6)
            nodes.append({'node_index':j,'ephemeris_time_tdb_seconds_past_j2000':epochs[j],**{f'position_j2000_{axis}_km':state[i] for i,axis in enumerate('xyz')},**{f'velocity_j2000_{axis}_kmps':state[i+3] for i,axis in enumerate('xyz')}})
    node_path=ROOT/'truth/reference_audit/type13_nodes.csv'
    with node_path.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=list(nodes[0]));w.writeheader();w.writerows(nodes)
    largest_node_pair=max(zip(nodes,nodes[1:]),key=lambda pair:position_defect(*pair,'reference'))
    result={'scope':'Largest sharp coast-F reference change visible at the stored comparison epochs; same frozen JWST SPK queried directly.',
            'coarse_interval_utc':[reference[index-1]['utc'],reference[index]['utc']],
            'reference_hourly_position_minus_trapezoidal_velocity_m':position_defect(reference[index-1],reference[index],'reference'),
            'simulation_same_interval_position_minus_trapezoidal_velocity_m':position_defect(simulation[index-1],simulation[index],'simulation'),
            'fine_interval_utc':[a['utc'],b['utc']],
            'fine_interval_seconds':float(b['ephemeris_time_tdb_seconds_past_j2000'])-float(a['ephemeris_time_tdb_seconds_past_j2000']),
            'reference_fine_position_minus_trapezoidal_velocity_m':position_defect(a,b,'reference'),
            'reference_velocity_change_mps':math.sqrt(sum((1000*float(b[f'velocity_j2000_{axis}_kmps'])-1000*float(a[f'velocity_j2000_{axis}_kmps']))**2 for axis in 'xyz')),
            'dense_sample_count':len(dense),
            'maximum_dense_velocity_departure_from_pretransition_state_mps':max(deviations),
            'maximum_one_second_reference_velocity_change_mps':max_velocity_step,
            'spk_segment':segment,
            'interpolation_degree':2*(int(window_minus_one)+1)-1,
            'largest_neighboring_node_position_minus_trapezoidal_velocity_m':position_defect(*largest_node_pair,'reference'),
            'largest_node_interval_s':float(largest_node_pair[1]['ephemeris_time_tdb_seconds_past_j2000'])-float(largest_node_pair[0]['ephemeris_time_tdb_seconds_past_j2000']),
            'interpretation':'The retained type-13 ephemeris contains a sharp change between neighboring tabulated positions; Hermite interpolation produces a brief large velocity excursion. The PHAROS trajectory remains smooth over the same hourly interval. Stored-grid mission residuals include the position change but do not resolve the entire brief velocity excursion. No reference samples were edited or excluded to improve the mission scores; no claim is made about the source orbit-determination cause or a reference uncertainty bound.'}
    (ROOT/'analysis/reference_continuity_audit.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result,indent=2),flush=True)

if __name__=='__main__':main()
