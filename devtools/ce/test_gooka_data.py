#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Native external-translation validation; isolated roots, original game read only."""
import argparse,copy,hashlib,json,os,struct,sys,zlib
from pathlib import Path
from test_cli_client import Client
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'engines/gooka/localization'))
from compile import data_file
p=argparse.ArgumentParser()
for k in ('exe','game','data','artifacts'):p.add_argument('--'+k,type=Path,required=True)
a=p.parse_args()
for k in ('exe','game','data','artifacts'):setattr(a,k,getattr(a,k).resolve())
a.artifacts.mkdir(parents=True,exist_ok=False)
good=a.data.read_bytes();entries=json.loads((Path(__file__).resolve().parents[2]/'engines/gooka/localization/english.json').read_text(encoding='utf-8'))
def seal(packet):
 b=bytearray(packet);struct.pack_into('>I',b,16,len(b)-24);struct.pack_into('>I',b,20,zlib.crc32(b[24:]));return bytes(b)
def change(offset,value):
 b=bytearray(good);b[offset:offset+len(value)]=value;return seal(b)
crc=bytearray(good);crc[-1]^=1
future=bytearray(good);struct.pack_into('>H',future,8,2)
encoding=bytearray(good);struct.pack_into('>H',encoding,10,1252)
second=24+22+struct.unpack_from('>I',good,26)[0]
wrong=copy.deepcopy(entries);wrong[0]['english']='Needle@1'
updated=copy.deepcopy(entries);updated[0]['english']='Updated needle'
cases=[('missing',None,'en','gooka.dat is missing'),('truncated',good[:100],'en','corrupt or truncated'),('checksum',bytes(crc),'en','corrupt or truncated'),('future-version',bytes(future),'en','unsupported format'),('encoding',bytes(encoding),'en','encoding'),('partial',data_file(entries[:-1]),'en','every text resource'),('duplicate',change(second,good[24:26]),'en','duplicate'),('wrong-source',change(30,bytes([good[30]^1])),'en','does not match'),('embedded-nul',change(46,b'\0'),'en','invalid CP1250'),('bad-entry-length',change(26,b'\xff'*4),'en','invalid text entries'),('formatting',data_file(wrong),'en','formatting or picture'),('trailing',seal(good+b'X'),'en','trailing or unowned'),('valid',good,'en',None),('updated',data_file(updated),'en',None),('czech-missing',None,'cz',None),('czech-corrupt',bytes(crc),'cz',None)]
rows=[];pictures={};old=os.environ.get('GOOKA_TEST')
os.environ['GOOKA_TEST']='raw:wait:20;clear;text:1:403:20;shot:screenshots/text.png;quit'
try:
 for name,payload,lang,expected in cases:
  root=a.artifacts/name;root.mkdir();(root/'ce-test-cli.marker').write_text('ce-test-cli/1\n');extra=root/'data';extra.mkdir()
  if payload is not None:(extra/'gooka.dat').write_bytes(payload)
  (root/'profile.ini').write_text('[scummvm]\nenable_unsupported_game_warning=false\nconfirm_exit=false\nconsole=false\ngui_return_to_launcher_at_exit=false\nautosave_period=0\n[cli-game]\nengineid=gooka\ngameid=gooka\nplatform=windows\ncheck_gamedata=false\n'+f'path={a.game}\nlanguage={lang}\n')
  c=Client(a.exe,root,extra)
  try:
   if expected:
    request=dict(cmd='launch',engine='gooka',game='gooka',platform='windows',path=str(a.game));c.record({'request':request});c.process.stdin.write(json.dumps(request)+'\n');c.process.stdin.flush()
    try:
     c.receive(timeout=20)
    except RuntimeError:
     response=next(r['response'] for r in reversed(c.records) if r.get('response',{}).get('error')=='engine_fatal')
    else:raise AssertionError('Invalid English data unexpectedly started')
    assert expected in response.get('message',''),response
    assert 'gooka.dat' in response['message'] and 'select Czech' in response['message']
    c.process.wait(timeout=15);assert c.process.returncode==2
    rows.append(dict(case=name,rejected=True,message=response['message']))
   else:
    assert c.launch(a.game,'windows',game='gooka',engine='gooka')['status']=='running'
    c.process.wait(timeout=15);assert c.process.returncode==0
    pictures[name]=hashlib.sha256((root/'screenshots/text.png').read_bytes()).hexdigest();rows.append(dict(case=name,started=True))
  finally:
   if c.process.poll() is None:c.abort()
   else:c.close(expected=2 if expected else 0)
 assert pictures['valid']!=pictures['updated'],'Data-only correction did not change rendered text'
 assert pictures['czech-missing']==pictures['czech-corrupt']
finally:
 if old is None:os.environ.pop('GOOKA_TEST',None)
 else:os.environ['GOOKA_TEST']=old
report=dict(status='PASS',exe_sha256=hashlib.sha256(a.exe.read_bytes()).hexdigest(),data_sha256=hashlib.sha256(good).hexdigest(),cases=rows,data_only_update_rendered=True,czech_independent_of_data=True)
(a.artifacts/'verification.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
