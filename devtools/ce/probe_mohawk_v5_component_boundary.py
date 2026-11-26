# SPDX-License-Identifier: GPL-3.0-or-later
"""CE constructor reachability boundaries, separate from native original method cases."""
import argparse,hashlib,json
from pathlib import Path
import probe_mohawk_v5_vm as vm
from test_mohawk_v5_startup import integer
TYPES={'datafile':1001,'clock':1003,'font':1004,'aaDatabase':1005,'aaDirectory':1006,'aaQSelector':1008,'aaStringLib':1010,'aaUserHistory':1011,'encryptor':1012}

def probes():
 return [(name,integer(ident)+b'\x4d\x57\x01'+integer(0)+b'\x6e\x14'+integer(0)+b'\x6c\x16',1) for name,ident in TYPES.items()]

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ce',type=Path,required=True);p.add_argument('--extras',type=Path,required=True);p.add_argument('--game-data',type=Path,required=True);p.add_argument('--root',type=Path,required=True);a=p.parse_args()
 vm.probes=probes;vm.ce(a)
 target=a.root/'receipt.json';j=json.loads(target.read_text());j['kind']=__doc__;j['boundary_harness_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
 for row in j['results']:
  name=row['name'];supported=name in ['datafile','font']
  row['contract_checks']={'constructor_boundary':row.get('status')=='observed' and row.get('actual')==1 if supported else row.get('status')=='engine_error' and ('unknown LBX object type '+str(TYPES[name])) in '\n'.join(row.get('log_tail',[]))}
  row['boundary']='constructor-completed' if supported else 'unsupported-constructor'
 target.write_text(json.dumps(j,indent=2)+'\n');raise SystemExit(0 if all(all(r['contract_checks'].values()) for r in j['results']) else 1)
