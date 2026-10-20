#!/usr/bin/env python3
"""Sample an explicitly supplied process family; preserve raw OS fields."""
import argparse
import ctypes
import ctypes.util
import json
import math
import platform
import statistics
import struct
import subprocess
import time
from pathlib import Path

def percentile(values, p=95):
    if not values or not all(math.isfinite(x) for x in values):
        raise ValueError('Nonempty finite samples required; failed samples must be reported separately')
    ordered=sorted(values)
    return ordered[max(0, math.ceil(p/100*len(ordered))-1)]

def sample(pid):
    if platform.system()=='Darwin':
        library=ctypes.CDLL(ctypes.util.find_library('proc'))
        buffer=ctypes.create_string_buffer(1024)
        result=library.proc_pid_rusage(int(pid),2,ctypes.byref(buffer))
        if result: return {'pid':pid,'status':'Blocked','reason':'proc_pid_rusage denied/unavailable'}
        return {'pid':pid,'status':'Pass','resident_bytes':struct.unpack_from('Q',buffer.raw,64)[0],
                'physical_footprint_bytes':struct.unpack_from('Q',buffer.raw,72)[0]}
    if platform.system()=='Linux':
        status=Path(f'/proc/{pid}/status').read_text()
        rss=int(next(x.split()[1] for x in status.splitlines() if x.startswith('VmRSS:')))*1024
        return {'pid':pid,'status':'Pass','resident_bytes':rss,'physical_footprint':'Blocked: implement PSS/shared accounting'}
    return {'pid':pid,'status':'Blocked','reason':'Windows working-set/private/commit sampler pending native host'}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('pids',nargs='+',type=int);p.add_argument('--seconds',type=float,default=30)
    p.add_argument('--output',type=Path,required=True);args=p.parse_args()
    assert 0<args.seconds<=300
    raw=[];start=time.monotonic()
    while time.monotonic()-start<args.seconds:
        raw.append({'elapsed_seconds':time.monotonic()-start,'processes':[sample(pid) for pid in args.pids]})
        time.sleep(.25)
    report={'method':'Darwin proc_pid_rusage v2 physical footprint and resident size; Linux RSS supplementary',
            'family_accounting':'Retain per-process values. Sum is a conservative indicator and can double count shared allocations; not unique system physical memory.',
            'gpu_accounting':'Blocked: no separate reliable GPU allocation sampler; unified memory cannot be added blindly',
            'samples':raw}
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(f'Recorded {len(raw)} samples for {len(args.pids)} explicit processes')

if __name__=='__main__':main()
