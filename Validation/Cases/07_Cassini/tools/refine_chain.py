"""Continuous half-maximum-step replay, following completed nominal segments."""
import argparse,time
from continuous_chain import ROOT,run

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--from',dest='first',type=int,default=1);p.add_argument('--to',type=int,default=20);a=p.parse_args()
    base=ROOT/'checks/half_step'
    for i in range(a.first,a.to+1):
        sid=f'{i:02d}'
        while not (ROOT/'segments'/sid/'metadata.json').exists():time.sleep(5)
        run(sid,base,0.5)
