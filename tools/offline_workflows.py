#!/usr/bin/env python3
"""Execute real CAD test workflows inside a macOS network-denial sandbox."""
import argparse,json,os,socket,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--probe',action='store_true');p.add_argument('--output',type=Path);a=p.parse_args()
if a.probe:
    outcomes=[]
    for family,kind,destination in [(socket.AF_INET,socket.SOCK_STREAM,('1.1.1.1',443)),(socket.AF_INET,socket.SOCK_DGRAM,('1.1.1.1',53)),(socket.AF_INET6,socket.SOCK_STREAM,('::1',65534))]:
        s=socket.socket(family,kind);s.settimeout(.2)
        try:
            if kind==socket.SOCK_DGRAM:s.sendto(b'blocked-probe',destination)
            else:s.connect(destination)
        except PermissionError:outcomes.append('DeniedByOS')
        except OSError as e:raise SystemExit(f'Blocked: probe did not prove network denial: {e}')
        else:raise SystemExit('Fail: network operation unexpectedly allowed')
        finally:s.close()
    print(json.dumps({'IPv4_TCP':outcomes[0],'IPv4_UDP':outcomes[1],'IPv6_TCP':outcomes[2]}));raise SystemExit(0)
if sys.platform!='darwin':raise SystemExit('Blocked: native Windows/Linux network isolation not provisioned')
assert a.output;a.output.mkdir(parents=True,exist_ok=True)
# Deny IP network sockets; retain local Unix IPC needed by the single worker.
profile='(version 1) (allow default) (deny network* (remote ip "*:*")) (deny network* (local ip "*:*"))'
commands=[('network-denial-probe',[sys.executable,str(Path(__file__).resolve()),'--probe'])]
for module,case in [(3,'shapeEditing'),(4,'lifecycle'),(5,'lifecycle'),(6,'lifecycle'),(7,'lifecycle'),(8,'lifecycle')]:commands.append((f'module{module}',[str(ROOT/f'build/macos-debug/module{module}_contracts'),case]))
for target in ('module3_solver','module4_geometry','module5_b2','module6_exchange','module7_b3','module8_drawing'):
 command=[str(ROOT/'build/macos-debug'/target)]
 if target in ('module5_b2','module7_b3'):command.append(str(a.output/(target+'.json')))
 if target=='module8_drawing':command += [str(a.output/'B5-portable-updated.pdf'),str(a.output/'B5.json')]
 commands.append(('frozen-'+target,command))
record={'scope':'Actual available-Mac Part/sketch/history/exchange/assembly/drawing IPC workflows and frozen B1/B2/B3/B4/B5 engine/store acceptance corpora with OS-denied IPv4/IPv6 network; headless UI, not installed release','profile':profile,'steps':[]}
env=os.environ.copy();env['QT_QPA_PLATFORM']='offscreen'
for name,command in commands:
    with (a.output/(name+'.txt')).open('w') as f:r=subprocess.run(['/usr/bin/sandbox-exec','-p',profile,*command],cwd=ROOT,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=240)
    record['steps'].append({'name':name,'command':command,'exit_code':r.returncode})
    (a.output/'offline.json').write_text(json.dumps(record,indent=2)+'\n')
    if r.returncode:raise SystemExit(f'Fail: {name}; see {a.output}')
record['status']='Pass';(a.output/'offline.json').write_text(json.dumps(record,indent=2)+'\n');print('Pass: actual core workflows under verified IP network denial')
