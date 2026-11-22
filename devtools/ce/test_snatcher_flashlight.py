#!/usr/bin/env python3
"""Move the real mouse flashlight in an isolated snapshot; compare static menu pixels."""
import argparse, hashlib, json, shutil, struct, zlib
from pathlib import Path
from test_snatcher_integration import Client, sha
p=argparse.ArgumentParser()
for key in ('exe','game','checkpoint','resources','output'):p.add_argument('--'+key,type=Path,required=True)
p.add_argument('--expect-static',action='store_true');p.add_argument('--wheel-commands',action='store_true');a=p.parse_args();a.output.mkdir()
c=Client(a.exe,a.output/'profile',a.game,'gens');captures=[]
def png(path,data,w,h):
 def chunk(k,v):return struct.pack('>I',len(v))+k+v+struct.pack('>I',zlib.crc32(k+v))
 rows=b''.join(b'\0'+data[y*w*3:(y+1)*w*3] for y in range(h))
 path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))
try:
 cfg=c.root/'profile.ini';cfg.write_text(cfg.read_text().replace('autosave_period=1','autosave_period=0').replace('snatcher_decensor=false','snatcher_decensor=true').replace('[scummvm]','[scummvm]\nextrapath='+str(a.resources)))
 saves=c.root/'saves';saves.mkdir(exist_ok=True);shutil.copy2(a.checkpoint,saves/'cli-game.065')
 c.command('launch',engine='snatcher',game='snatcher',platform='segacd',path=str(a.game));c.command('wait',field='adapter.ready',equals=True,timeout_ms=30000);c.command('load',slot=65)
 c.command('advance',frames=24)
 initial=c.command('inspect')['adapter'];assert initial['interaction_pointer_active'] and not initial['selected_path']
 for slot,(x,y) in enumerate(((128,72),(100,50),(160,96),(128,72)),70):
  c.command('input',kind='mouse_move',x=x,y=y);c.command('advance',frames=24)
  state=c.command('inspect')['adapter'];assert state['packed_digest']==initial['packed_digest'],'Hover changed story state'
  assert (state['interaction_x16'],state['interaction_y16'])==((x-128)*65536,(y-72)*65536),'Mouse projection was not applied'
  assert not any(state.get(k) for k in ('blocker','runtime_error','render_error','audio_error'))
  r=c.command('input',kind='logical',action='scene_rgb_export',slot=slot);w,h=r['width'],r['height'];assert (w,h)==(256,224)
  data=(saves/r['filename']).read_bytes();png(a.output/f'point-{slot}.png',data,w,h)
  record=dict(mouse=[x,y],position=[state['interaction_x16'],state['interaction_y16']],menu_sha256=hashlib.sha256(data[144*w*3:]).hexdigest(),upper_sha256=hashlib.sha256(data[:144*w*3]).hexdigest(),state=state)
  captures.append(record);print(slot,record['position'],record['menu_sha256'],flush=True)
 menu_static=len({v['menu_sha256'] for v in captures})==1;flashlight_moved=len({v['upper_sha256'] for v in captures})>1
 assert flashlight_moved,'Flashlight did not move'
 if a.expect_static:assert menu_static,'Mouse movement shifted command-menu pixels'
 # Exercise the action that leaves the flashlight menu for a dialogue window.
 c.command('input',kind='mouse_move',x=100,y=50);c.command('advance',frames=24)
 wheel_checks=[];beam_preserved_on_click=False;selected_confirmed_once=False
 if a.wheel_commands:
  before=c.command('inspect')['adapter'];beam=(before['interaction_x16'],before['interaction_y16']);beam_region=before['interaction_region'];refs=before['choice_references'];index=refs.index(before['command_reference'])
  for direction in (1,-1,-1,1):
   index=(index+direction)%len(refs)
   c.command('input',kind='mouse_wheel',direction=direction,x=100,y=50);c.command('advance',frames=24)
   state=c.command('inspect')['adapter'];assert state['command_reference']==refs[index],'Wheel did not select the adjacent command'
   assert (state['interaction_x16'],state['interaction_y16'])==beam and state['interaction_region']==beam_region,'Wheel moved the flashlight or changed its hit region'
   wheel_checks.append(dict(direction=direction,reference=refs[index],beam=list(beam)))
  # Select this source menu's Look, then click immediately during its repaint.
  assert refs[1]==212,'Fixture no longer has Look in the expected row'
  while index!=1:
   index=(index+1)%len(refs);c.command('input',kind='mouse_wheel',direction=1,x=100,y=50)
   if index!=1:c.command('advance',frames=24)
  trace_serial=max(t['sequence'] for t in state['trace'])
  # Button coordinates alone must not retarget the already positioned beam.
  for phase in ('down','down','up'):
   c.command('input',kind='mouse_button',button=1,phase=phase,x=20,y=20);c.command('advance',frames=2)
 else:
  c.command('input',kind='mouse_move',x=40,y=156);c.command('advance',frames=4)
  for phase in ('down','up'):
   c.command('input',kind='mouse_button',button=1,phase=phase,x=40,y=156);c.command('advance',frames=4)
 for _ in range(160):
  dialogue=c.command('inspect')['adapter']
  if dialogue['runtime_status']==4 and dialogue['dialogue_waiting'] and not dialogue['font_publication_pending']:break
  c.command('advance',frames=6)
 else:raise AssertionError('Look dialogue did not become ready')
 if a.wheel_commands:
  beam_preserved_on_click=(dialogue['interaction_x16'],dialogue['interaction_y16'])==beam
  assert beam_preserved_on_click and dialogue['interaction_region']==beam_region,'Confirmation changed the aimed-at object'
  confirmations=[t for t in dialogue['trace'] if t['sequence']>trace_serial and t['category']=='player_command_confirmed']
  selected_confirmed_once=len(confirmations)==1 and confirmations[0]['a']==212
  assert selected_confirmed_once,'Click did not confirm Look exactly once'

 r=c.command('input',kind='logical',action='scene_rgb_export',slot=74);data=(saves/r['filename']).read_bytes();png(a.output/'look-dialogue.png',data,256,224)
 # The source caption starts at X16 (speaker) / X24 (body), leaving the
 # first sixteen columns black. Wrapped letters in this margin expose pan.
 assert dialogue['dialogue_glyphs']>0 and any(data[144*256*3:]),'Look caption did not render'
 margin=sum(any(data[(y*256+x)*3:(y*256+x+1)*3]) for y in range(144,224) for x in range(16))
 dialogue_aligned=margin==0
 if a.expect_static:assert dialogue_aligned,('Look caption wrapped into left margin',margin)
 for _ in range(160):
  state=c.command('inspect')['adapter']
  if state['runtime_status']==3 and state['command_ready'] and state['dialogue_waiting'] and not state['font_publication_pending']:break
  if state['runtime_status']==4 and state['dialogue_waiting']:
   for phase in ('down','up'):
    c.command('input',kind='mouse_button',button=1,phase=phase,x=100,y=180);c.command('advance',frames=2)
  c.command('advance',frames=6)
 else:raise AssertionError('Look did not return to command menu')
 returned=[]
 for slot,(x,y) in enumerate(((100,50),(160,96)),75):
  c.command('input',kind='mouse_move',x=x,y=y);c.command('advance',frames=24)
  r=c.command('input',kind='logical',action='scene_rgb_export',slot=slot);data=(saves/r['filename']).read_bytes();png(a.output/f'returned-{slot}.png',data,256,224);returned.append(hashlib.sha256(data[144*256*3:]).hexdigest())
 menu_return_static=len(set(returned))==1
 if a.expect_static:assert menu_return_static,'Returned menu moved with flashlight'
 result=dict(status='PASS',exe_sha256=sha(a.exe),wheel_commands=a.wheel_commands,wheel_checks=wheel_checks,beam_preserved_on_click=beam_preserved_on_click,selected_confirmed_once=selected_confirmed_once,menu_static=menu_static,flashlight_moved=flashlight_moved,look_dialogue_aligned=dialogue_aligned,look_state=dialogue,dialogue_margin_pixels=margin,menu_return_static=menu_return_static,checkpoint_unchanged=sha(a.checkpoint)==sha(saves/'cli-game.065'),captures=captures)
 (a.output/'receipt.json').write_text(json.dumps(result,indent=2));print('menu_static',menu_static,'flashlight_moved',flashlight_moved,'look_dialogue_aligned',dialogue_aligned,'menu_return_static',menu_return_static,flush=True)
finally:c.close()
