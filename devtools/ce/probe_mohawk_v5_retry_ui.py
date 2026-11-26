# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual original application Retry/Cancel decisions drive its operation loop; explicit operation fixture."""
import argparse,ctypes as C,hashlib,json,shutil,threading,time
from ctypes import wintypes as W
from pathlib import Path
from probe_mohawk_v5_original import run_case
from probe_mohawk_v5_os_ui import png
from probe_mohawk_v5_retry import stub

def watch_sequence(exe,path,button,stop,result):
 buttons=button;
 user=C.WinDLL('user32');k=C.WinDLL('kernel32');k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];k.OpenProcess.restype=W.HANDLE;k.QueryFullProcessImageNameW.argtypes=[W.HANDLE,W.DWORD,W.LPWSTR,C.POINTER(W.DWORD)];k.CloseHandle.argtypes=[W.HANDLE]
 user.GetDlgItem.argtypes=[W.HWND,C.c_int];user.GetDlgItem.restype=W.HWND;user.SendMessageW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM];user.SendMessageW.restype=W.LPARAM
 user.GetWindowThreadProcessId.argtypes=[W.HWND,C.POINTER(W.DWORD)];user.IsWindowVisible.argtypes=[W.HWND];user.GetWindowTextW.argtypes=[W.HWND,W.LPWSTR,C.c_int]
 callback=C.WINFUNCTYPE(W.BOOL,W.HWND,W.LPARAM)
 def title(hwnd):
  buf=C.create_unicode_buffer(2048);user.GetWindowTextW(hwnd,buf,len(buf));return buf.value
 @callback
 def visit(hwnd,_):
  if len(result)>=len(buttons) or not user.IsWindowVisible(hwnd):return True
  button=buttons[len(result)]
  pid=W.DWORD();user.GetWindowThreadProcessId(hwnd,C.byref(pid));process=k.OpenProcess(0x1000,False,pid.value)
  if not process:return True
  try:
   buf=C.create_unicode_buffer(32768);size=W.DWORD(len(buf));valid=k.QueryFullProcessImageNameW(process,0,buf,C.byref(size)) and Path(buf.value).resolve()==exe
  finally:k.CloseHandle(process)
  if not valid:return True
  control=user.GetDlgItem(hwnd,button)
  if not control:return True
  try:
   time.sleep(.15);children=[]
   @callback
   def child(h,_):children.append(dict(text=title(h)));return True
   user.EnumChildWindows(hwnd,child,0)
   receipt=dict(pid=pid.value,window=int(hwnd),title=title(hwnd),children=children,button_id=button,button_text=title(control),screenshot=png(hwnd,path.with_stem(path.stem+'-'+str(len(result)+1))))
   result.append(receipt)
  except Exception as error:result.append(dict(error=str(error)))
  finally:user.SendMessageW(hwnd,0x111,button,int(control))
  return False
 while not stop.is_set() and len(result)<len(buttons):
  user.EnumWindows(visit,0);stop.wait(.02)


if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--original',type=Path,required=True);p.add_argument('--root',type=Path,required=True);a=p.parse_args()
 if hashlib.sha256(a.original.read_bytes()).hexdigest()!='219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966':raise ValueError('Unrecognized executable')
 a.root=a.root.resolve();a.root.mkdir(parents=True,exist_ok=False);exe=a.root/'CarmenTQ.exe';shutil.copyfile(a.original,exe);rows=[]
 for name,failures,buttons,attempts,status in [('retry-success',1,[4],2,0),('retry-cancel',None,[4,2],2,0x284c),('cancel-first',None,[2],1,0x284c)]:
  case=dict(name=name,raw=21,failures=failures,decision=1,policy=1,suppressed=0,real_ui=True,expected=status);events=[];stop=threading.Event();worker=threading.Thread(target=watch_sequence,args=(exe,a.root/(name+'.png'),buttons,stop,events),daemon=True);worker.start()
  try:row=run_case(exe,dict(case,output='int32',after_loader=True,allocation_size=0x10000,observe_native_words=True,observe_vm_error=True),15,stub_factory=stub)
  finally:stop.set();worker.join(2)
  row.pop('expected',None);row.pop('matches_static_contract',None);w=row.get('native_words',[]);checks={}
  if row['status']=='observed':
   checks=dict(memory_initialized=w[0]==0,filesystem_initialized=w[1]==0,no_vm_error=not row.get('vm_errors'),status=row['actual']==status,attempts=w[7]==attempts,bounded=w[31]==0,actual_ui_not_fixture=w[8]==0,ui_count=len(events)==len(buttons),buttons=[e.get('button_id') for e in events]==buttons,captured=all('error' not in e and e.get('screenshot') for e in events))
   if not all(checks.values()):row['status']='unqualified'
  row.update(name=name,ui_events=events,contract_checks=checks);rows.append(row);print(name,row['status'],row.get('actual'),w[7:11],events,flush=True)
 dependencies={name:hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest() for name in ['probe_mohawk_v5_original.py', 'probe_mohawk_v5_native_resources.py', 'mohawk_v5_native_io.py', 'probe_mohawk_v5_retry_ui.py', 'probe_mohawk_v5_retry.py', 'probe_mohawk_v5_os_ui.py']}
 files={str(f.relative_to(a.root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in a.root.rglob('*') if f.is_file()}
 (a.root/'receipt.json').write_text(json.dumps(dict(kind=__doc__,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),dependencies=dependencies,owned_files_before_receipt=files,results=rows),indent=2)+'\n');raise SystemExit(0 if all(r['status']=='observed' for r in rows) else 1)
