# SPDX-License-Identifier: GPL-3.0-or-later
"""Real original Retry/Cancel dialogs, scoped only to the copied probe process."""
import argparse,ctypes as C,hashlib,json,shutil,struct,threading,time,zlib
from ctypes import wintypes as W
from pathlib import Path
from probe_mohawk_v5_original import run_case,u32
from mohawk_v5_native_io import Native,boot


def png(window,path):
 user=C.WinDLL('user32');gdi=C.WinDLL('gdi32');rect=W.RECT();user.GetWindowRect(window,C.byref(rect));width=rect.right-rect.left;height=rect.bottom-rect.top
 if not 40<=width<=2000 or not 40<=height<=1500:raise ValueError('Unexpected dialog dimensions')
 user.GetDC.argtypes=[W.HWND];user.GetDC.restype=W.HDC;user.GetWindowRect.argtypes=[W.HWND,C.POINTER(W.RECT)];user.ReleaseDC.argtypes=[W.HWND,W.HDC];gdi.DeleteObject.argtypes=[W.HGDIOBJ];gdi.DeleteDC.argtypes=[W.HDC];gdi.CreateCompatibleDC.argtypes=[W.HDC];gdi.CreateCompatibleDC.restype=W.HDC
 gdi.CreateCompatibleBitmap.argtypes=[W.HDC,C.c_int,C.c_int];gdi.CreateCompatibleBitmap.restype=W.HBITMAP;gdi.SelectObject.argtypes=[W.HDC,W.HGDIOBJ];gdi.SelectObject.restype=W.HGDIOBJ
 gdi.GetDIBits.argtypes=[W.HDC,W.HBITMAP,W.UINT,W.UINT,C.c_void_p,C.c_void_p,W.UINT];user.PrintWindow.argtypes=[W.HWND,W.HDC,W.UINT]
 screen=user.GetDC(window);dc=gdi.CreateCompatibleDC(screen);bitmap=gdi.CreateCompatibleBitmap(screen,width,height);old=gdi.SelectObject(dc,bitmap)
 try:
  if not user.PrintWindow(window,dc,2):raise OSError('PrintWindow failed')
  header=struct.pack('<IiiHHIIiiII',40,width,-height,1,32,0,width*height*4,0,0,0,0);buf=C.create_string_buffer(width*height*4)
  if gdi.GetDIBits(dc,bitmap,0,height,buf,header,0)!=height:raise OSError('GetDIBits failed')
  raw=buf.raw;lines=b''.join(b'\0'+b''.join(bytes((raw[o+2],raw[o+1],raw[o])) for o in range(y*width*4,(y+1)*width*4,4)) for y in range(height))
  def chunk(tag,payload):return struct.pack('>I',len(payload))+tag+payload+struct.pack('>I',zlib.crc32(tag+payload)&0xffffffff)
  path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(lines))+chunk(b'IEND',b''))
  return dict(width=width,height=height,sha256=hashlib.sha256(path.read_bytes()).hexdigest())
 finally:
  gdi.SelectObject(dc,old);gdi.DeleteObject(bitmap);gdi.DeleteDC(dc);user.ReleaseDC(window,screen)


def watch(exe,path,button,stop,result):
 user=C.WinDLL('user32');k=C.WinDLL('kernel32');k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];k.OpenProcess.restype=W.HANDLE;k.QueryFullProcessImageNameW.argtypes=[W.HANDLE,W.DWORD,W.LPWSTR,C.POINTER(W.DWORD)];k.CloseHandle.argtypes=[W.HANDLE]
 user.GetDlgItem.argtypes=[W.HWND,C.c_int];user.GetDlgItem.restype=W.HWND;user.SendMessageW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM];user.SendMessageW.restype=W.LPARAM
 user.GetWindowThreadProcessId.argtypes=[W.HWND,C.POINTER(W.DWORD)];user.IsWindowVisible.argtypes=[W.HWND];user.GetWindowTextW.argtypes=[W.HWND,W.LPWSTR,C.c_int]
 callback=C.WINFUNCTYPE(W.BOOL,W.HWND,W.LPARAM)
 def title(hwnd):
  buf=C.create_unicode_buffer(2048);user.GetWindowTextW(hwnd,buf,len(buf));return buf.value
 @callback
 def visit(hwnd,_):
  if result or not user.IsWindowVisible(hwnd):return True
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
   receipt=dict(pid=pid.value,window=int(hwnd),title=title(hwnd),children=children,button_id=button,button_text=title(control),screenshot=png(hwnd,path))
   result.append(receipt)
  except Exception as error:result.append(dict(error=str(error)))
  finally:user.SendMessageW(hwnd,0x111,button,int(control))
  return False
 while not stop.is_set() and not result:
  user.EnumWindows(visit,0);stop.wait(.02)


def stub(remote,write,output,case):
 n=Native(remote,write,output);boot(n);manager=remote+0x2800;dialog=remote+0x3000;title=remote+0x3400;text=remote+0x3500;data=remote+0x3600
 write(title,b'ThinkQuick original comparison probe\0');write(text,b'Controlled original Retry/Cancel comparison probe\0')
 n.call(0x4497cf,manager+4,[title,0xffff]);n.s+=b'\xc7\x05'+u32(0x49afb0)+u32(manager)
 if case['mode']=='helper':n.call(0x451550,dialog,[0x8009,text,0])
 else:
  write(data,struct.pack('<5I',3 if case['mode'].endswith('category3') else 1,1,0,0,text));n.call(0x42db7e,args=[data])
 n.s+=b'\x0f\xb7\xc0\xa3'+u32(output)
 n.s+=b'\x0f\xb7\x05'+u32(dialog+4);n.save(5)
 return n.s

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--original',type=Path,required=True);p.add_argument('--root',type=Path,required=True);a=p.parse_args()
 if hashlib.sha256(a.original.read_bytes()).hexdigest()!='219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966':raise ValueError('Unrecognized executable')
 a.root=a.root.resolve();a.root.mkdir(parents=True,exist_ok=False);exe=a.root/'CarmenTQ.exe';shutil.copyfile(a.original,exe);rows=[]
 for mode,button in [('helper',4),('helper',2),('application',4),('application',2),('application-category3',4),('application-category3',2)]:
  name=mode+('-retry' if button==4 else '-cancel');events=[];stop=threading.Event();worker=threading.Thread(target=watch,args=(exe,a.root/(name+'.png'),button,stop,events),daemon=True);worker.start()
  try:row=run_case(exe,dict(name=name,mode=mode,expected=(8 if button==4 else 1) if mode=='helper' else (1 if button==4 else 0),output='int32',after_loader=True,allocation_size=0x10000,observe_native_words=True,observe_vm_error=True),10,stub_factory=stub)
  finally:stop.set();worker.join(2)
  expected=(8 if button==4 else 1) if mode=='helper' else (1 if button==4 else 0)
  row.pop('expected',None);row.pop('matches_static_contract',None);checks={};w=row.get('native_words',[])
  if row.get('status')=='observed':
   checks=dict(value=row.get('actual')==expected,memory_initialized=w[0]==0,filesystem_initialized=w[1]==0,no_vm_error=not row.get('vm_errors'),ui_captured=len(events)==1 and 'error' not in events[0])
   if checks['ui_captured']:
    event=events[0];checks.update(title=event['title']=='ThinkQuick original comparison probe',button_id=event['button_id']==button,button_text=event['button_text']==('&Retry' if button==4 else 'Cancel'),retry_cancel_buttons=all(any(c['text']==t for c in event['children']) for t in ['&Retry','Cancel']),screenshot=(a.root/(name+'.png')).read_bytes().startswith(b'\x89PNG\r\n\x1a\n'))
   if mode=='helper':checks['stored_internal_result']=w[5]==expected
   if not all(checks.values()):row['status']='unqualified'
  row.update(name=name,ui_events=events,contract_checks=checks);rows.append(row);print(name,row.get('status'),row.get('actual'),events,flush=True)
 dependencies={name:hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest() for name in ['probe_mohawk_v5_os_ui.py','probe_mohawk_v5_original.py','mohawk_v5_native_io.py','probe_mohawk_v5_native_resources.py']}
 assert sorted(f.name for f in a.root.iterdir())==sorted(['CarmenTQ.exe']+[r['name']+'.png' for r in rows])
 (a.root/'receipt.json').write_text(json.dumps(dict(kind=__doc__,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),dependencies=dependencies,results=rows),indent=2)+'\n')
 raise SystemExit(0 if all(r['status']=='observed' and r['contract_checks'] and all(r['contract_checks'].values()) for r in rows) else 1)
