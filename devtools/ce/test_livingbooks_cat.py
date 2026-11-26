#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Normal-input Cat playthrough, including ball and drawing assertions."""
from pathlib import Path
import sys,json,time,hashlib,re
import argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--exe',type=Path,required=True)
parser.add_argument('--assets',type=Path,required=True)
parser.add_argument('--extra',type=Path,required=True)
parser.add_argument('--root',type=Path,required=True)
parser.add_argument('--mode',choices=('play','read','dialogue'),default='play')
args=parser.parse_args()
sys.path.insert(0,str(Path(__file__).resolve().parent))
from test_cli_client import Client
if args.mode=='play':
 from PIL import Image
root=args.root.resolve();exe=args.exe.resolve()
c=Client(exe,root,args.extra.resolve());seen=[];checks=[]
result=dict(exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
def state():
 s=c.command('inspect')['adapter'];identity=(s['mode'],s['page'],s['subpage'])
 if identity not in seen:
  seen.append(identity);print('visited',identity,flush=True)
  if args.mode!='play':c.command('screenshot')
 if args.mode!='play' and identity==(5,3,2):
  clips=result.setdefault('dialogue_clips',{})
  for item in s['items']:
   sound=item.get('sound',65535)
   if sound==65535:continue
   clip=clips.setdefault(str(sound),{'min_samples':item['sound_samples'],'max_samples':0})
   clip['max_samples']=max(clip['max_samples'],item['sound_samples'])
 if args.mode=='read' and identity in ((5,3,2),(5,5,1),(5,7,1),(5,11,3),(5,13,1)):
  for item in s['items']:
   if item.get('sound',65535)!=1000:continue
   clips=result.setdefault('first_frame_clips',{})
   clip=clips.setdefault('%d.%d'%identity[1:],{'min_samples':item['sound_samples'],'max_samples':0})
   clip['max_samples']=max(clip['max_samples'],item['sound_samples'])
 (root/'latest-state.json').write_text(json.dumps(s,indent=2))
 return s
def wait(predicate,timeout=120):
 end=time.monotonic()+timeout
 while True:
  s=state()
  if predicate(s):return s
  assert time.monotonic()<end,('gameplay wait timed out',s['mode'],s['page'],s['subpage'],s['phase'])
  time.sleep(.5)
def input(**kw):
 c.command('input',**kw);time.sleep(.06)
def key(code):
 for phase in ('down','up'):input(kind='key',key=code,phase=phase)
def click(x,y):
 input(kind='mouse_move',x=x,y=y)
 for phase in ('down','up'):input(kind='mouse_button',button=1,x=x,y=y,phase=phase)
def item(s,name):return next(i for i in s['items'] if i['name']==name)
def center(i):
 l,t,r,b=i['rect'];return ((l+r)//2,(t+b)//2)
def shot(label):
 c.command('screenshot');p=max((root/'screenshots').glob('*.png'),key=lambda p:p.stat().st_mtime_ns)
 (root/(label+'.png')).write_bytes(p.read_bytes());return p
def ready(s):return s['mode']==6 and s['phase']==3 and s['cursor_visible']
def drag(start,end,release=True):
 input(kind='mouse_move',x=start[0],y=start[1]);input(kind='mouse_button',button=1,x=start[0],y=start[1],phase='down')
 for n in range(1,7):
  x=round(start[0]+(end[0]-start[0])*n/6);y=round(start[1]+(end[1]-start[1])*n/6)
  input(kind='mouse_move',x=x,y=y)
 if release:input(kind='mouse_button',button=1,x=end[0],y=end[1],phase='up')
def board():
 p=shot('drawing-latest');return Image.open(p).convert('RGB').crop((487,42,595,161))
try:
 c.launch(args.assets.resolve(),'windows','catinthehat','mohawk');time.sleep(2);key(27)
 wait(lambda s:s['mode']==2 and s['page']==1 and s['phase']==3 and s['cursor_visible'])
 if args.mode=='play':
  click(532,360)
  s=wait(lambda s:ready(s) and s['page']==2)
  shot('ball-before')
  drag(center(item(s,'Ball')),(300,200),False)
  s=state();assert item(s,'BallBase')['dragging'];assert item(s,'BallBase')['rect'][:2]==[300,200]
  shot('ball-held');checks.append('ball follows normal drag')
  input(kind='mouse_button',button=1,x=300,y=200,phase='up')
  time.sleep(1);s=state();assert not item(s,'BallBase')['dragging']
  assert item(s,'BallBase')['rect'][:2]!=[300,200], 'released ball did not move'
  shot('ball-moving');checks.append('released ball moves under native physics without cursor error')
  end=time.monotonic()+30;last=None;stable=0
  while True:
   s=state();position=item(s,'BallBase')['rect'][:2]
   settled=item(s,'Ball')['visible'] and s.get('variables',{}).get('TurnOn')==0
   stable=stable+1 if settled and position==last else 0
   if stable>=4:break
   assert time.monotonic()<end,'ball did not settle'
   last=position;time.sleep(.2)
  # Ensure the settled ball can be picked up again and dropped in the basket.
  drag(center(item(s,'Ball')),(610,420))
  s=wait(lambda s:s.get('variables',{}).get('InBasket')==1,10)
  shot('ball-in-basket');checks.append('basket accepts successful drop')
  key(275);s=wait(lambda s:ready(s) and s['page']==3 and s['subpage']==1)
  before=board();before.save(root/'drawing-before.png')
  click(520,163);input(kind='mouse_move',x=505,y=65)
  wait(lambda s:s.get('variables',{}).get('dTool')=='Pencil',10)
  drag((505,65),(570,135));input(kind='mouse_move',x=450,y=190);time.sleep(.4)
  after=board();after.save(root/'drawing-pencil.png')
  changed=sum(a!=b for a,b in zip(before.get_flattened_data(),after.get_flattened_data()))
  assert changed>30,('pencil did not draw',changed)
  checks.append('pencil paints persistent canvas pixels')
  click(561,165);input(kind='mouse_move',x=505,y=65)
  wait(lambda s:s.get('variables',{}).get('dTool')=='Eraser',10)
  drag((505,65),(570,135));input(kind='mouse_move',x=450,y=190);time.sleep(.4)
  erased=board();erased.save(root/'drawing-erased.png')
  remaining=sum(a!=b for a,b in zip(before.get_flattened_data(),erased.get_flattened_data()))
  assert remaining<changed,('eraser did not remove paint',changed,remaining)
  checks.append('eraser removes painted canvas pixels')
  # Retain every normal page/subpage transition and exercise scene hotspots.
  previous=(3,1);key(275)
  for transition in range(30):
   s=wait(lambda s:s['mode']!=6 or (ready(s) and (s['page'],s['subpage'])!=previous),180)
   if s['mode']!=6:break
   previous=(s['page'],s['subpage']);shot('scene-%d-%d'%previous)
   if previous==(4,2):
    for name in ('Milk','Fish','Book'):
     current=state();target=item(current,name)
     if not (target['loaded'] and target['visible'] and target['enabled']):
      raise AssertionError(('toss object unavailable',name,target))
     l,t,r,b=target['rect'];picked=False
     for fx,fy in ((.5,.5),(.25,.5),(.75,.5),(.5,.25),(.5,.75)):
      x=round(l+(r-l)*fx);y=round(t+(b-t)*fy)
      input(kind='mouse_move',x=x,y=y);input(kind='mouse_button',button=1,x=x,y=y,phase='down')
      current=state()
      if any(i['dragging'] and (i['name'].endswith('Base') or i['name']=='TossObj') for i in current['items']):
       picked=True;break
      input(kind='mouse_button',button=1,x=x,y=y,phase='up');time.sleep(.4)
     assert picked,('could not pick up toss object',name)
     for n in range(1,7):input(kind='mouse_move',x=round(x+(330-x)*n/6),y=round(y+(180-y)*n/6))
     input(kind='mouse_button',button=1,x=330,y=180,phase='up');time.sleep(2)
     state();shot('toss-'+name.lower());checks.append('picked up and tossed '+name)
   candidates=[i['name'] for i in s['items'] if i['loaded'] and i['visible'] and i['enabled']
     and re.search(r'QD$|Qd$|Click$|Button$|Control$',i['name'])
     and not re.search(r'Next|Prev|Text|Panel|LTW|Intro|Exit|Quit',i['name'])
     and 8<i['rect'][2]-i['rect'][0]<250 and 8<i['rect'][3]-i['rect'][1]<300]
   for name in candidates[:5]:
    current=state()
    if (current['page'],current['subpage'])!=previous:break
    target=item(current,name)
    if target['loaded'] and target['visible'] and target['enabled']:
     x,y=center(target)
     if 10<x<630 and 10<y<430:click(x,y);time.sleep(2);state()
   shot('scene-%d-%d-after'%previous)
   if previous==(13,1):break
   key(275)
  assert {x[1] for x in seen if x[0]==6}==set(range(1,14)),seen
 else:
  click(106,360);started=False;deadline=time.monotonic()+1500
  if args.mode=='dialogue':
   wait(lambda s:s['mode']==5 and s['page']==2)
   key(275);wait(lambda s:s['mode']==5 and s['page']==3)
  while time.monotonic()<deadline:
   current=state()
   if current['mode']==5:started=True
   if args.mode=='dialogue' and current['mode']==5 and current['page']==4:break
   if started and current['mode']!=5:break
   time.sleep(.2)
  else:raise TimeoutError('Read To Me did not complete')
  if args.mode=='read':assert {x[1] for x in seen if x[0]==5}==set(range(1,14)),seen
  clips=result.get('dialogue_clips',{})
  assert set(clips)=={str(x) for x in range(1000,1006)},clips
  assert all(x['max_samples']>x['min_samples'] for x in clips.values()),clips
  checks.append('all six Now! Now! dialogue clips advance audio sample positions')
  if args.mode=='read':
   clips=result.get('first_frame_clips',{})
   assert set(clips)=={'3.2','5.1','7.1','11.3','13.1'},clips
   assert all(x['max_samples']>x['min_samples'] for x in clips.values()),clips
   checks.append('five long first-frame narration clips advance audio sample positions')
 result['status']='passed'
except Exception as e:
 result.update(status='failed',error=str(e));print('FAILED',e,flush=True)
 try:shot('failure')
 except Exception:pass
finally:
 result.update(visited=seen,checks=checks)
 try:c.close()
 except Exception:c.abort()
 (root/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result),flush=True)
sys.exit(0 if result['status']=='passed' else 1)
