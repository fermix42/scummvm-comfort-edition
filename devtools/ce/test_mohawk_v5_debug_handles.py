# SPDX-License-Identifier: GPL-3.0-or-later
"""Native debugger guard: owned host event handles survive original thread exits."""
import argparse,hashlib,json,shutil
from pathlib import Path
from probe_mohawk_v5_original import run_case,u32


def stub(remote,write,output,case):
 child=remote+0x3000;write(child,b'\x31\xc0\xc2\x04\0');s=bytearray()
 for i in range(2):
  for arg in [remote+0x4000,0,0,child,0,0]:s+=b'\x68'+u32(arg)
  s+=b'\xff\x15'+u32(0x4881a0)+b'\x68'+u32(5000)+b'\x50\xff\x15'+u32(0x48829c)
  s+=b'\xff\x05'+u32(output)
 return s


if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--original',type=Path,required=True);p.add_argument('--root',type=Path,required=True);a=p.parse_args()
 if hashlib.sha256(a.original.read_bytes()).hexdigest()!='219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966':raise ValueError('Unrecognized executable')
 a.root=a.root.resolve();a.root.mkdir(parents=True,exist_ok=False);exe=a.root/'CarmenTQ.exe';shutil.copyfile(a.original,exe)
 result=run_case(exe,dict(name='debug-host-handle-lifetime',after_loader=True,allocation_size=0x10000,output='int32',expected=2,observe_native_words=True,verify_debug_handle_lifetime=True),8,stub_factory=stub)
 init=result.get('initialization',{});passed=result.get('status')=='observed' and result.get('actual')==2 and init.get('host_handles_survived_thread_exits') and init.get('host_handle_count',0)>=2 and init.get('thread_exit_count',0)>=2
 out=dict(kind=__doc__,passed=bool(passed),result=result,harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),transport_sha256=hashlib.sha256(Path(__file__).with_name('probe_mohawk_v5_original.py').read_bytes()).hexdigest());(a.root/'receipt.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2));raise SystemExit(0 if passed else 1)
