#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Normal offline A/B save history, later hermesbox link, two bounded replays.

No achievements. At most 24 forwarded session requests; failure ends the test.
Credentials are held only in an ACL-private runtime config, never in reports.
"""
import argparse
import configparser
import datetime
import hashlib
import http.server
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import struct
import tempfile
import threading
import urllib.error
import urllib.request
import shlex
from verify_assistance_hermesbox import PHP
from enhancement_oracle import read_save_enhancements


def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('exe','baseline','private','identity','artifacts'):ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--recovered-terminal',action='store_true',help='Use verified management delete-keep output: recovered R completed and older B archived before linking')
    ap.add_argument('--local-only',action='store_true',help='Verify offline half without opening a socket or using credentials')
    args=ap.parse_args();exe=args.exe.resolve(strict=True);baseline=args.baseline.resolve(strict=True)
    fixture=json.loads((baseline/'verification.json').read_text());assert fixture['status']=='VERIFIED' and (args.recovered_terminal or fixture['executable_sha256']==sha(exe))
    private=args.private.resolve(strict=True);credentials=configparser.ConfigParser(interpolation=None);credentials.read(private/'ce-private.ini',encoding='utf-8')
    assert credentials['scummvm']['ce_achievements_api_base_url']=='http://hermesbox:8123/api/v1'
    token=credentials['scummvm']['ce_link_token'];assert token
    root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True,exist_ok=False)
    source=baseline/'delete-keep' if args.recovered_terminal else baseline
    saves=root/'saves';shutil.copytree(source/'saves',saves)
    original_hashes={p.name:sha(p) for p in saves.glob('nox-ce-*')}
    original_sidecars={p.name:json.loads(p.read_text()) for p in saves.glob('nox-ce-*.json')}
    ini=configparser.ConfigParser(interpolation=None);ini.read(source/('management.ini' if args.recovered_terminal else 'gameplay.ini'),encoding='utf-8');assert not ini['scummvm'].get('ce_link_token')
    ini['nox-gameplay-test']['nox_history_validation']='7' if args.recovered_terminal else '6';ini['nox-gameplay-test']['nox_management_validation']='0';ini['nox-gameplay-test']['nox_history_linked']='false'
    local=root/'offline.ini'
    with local.open('w',encoding='utf-8') as out:ini.write(out)
    env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy')
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    commands=[]
    def run(config,phase):
        argv=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={root/(phase+"-engine.log")}', 'nox-gameplay-test']
        result=subprocess.run(argv,env=env,startupinfo=startup,capture_output=True,timeout=35)
        (root/(phase+'-stdout.txt')).write_bytes(result.stdout);(root/(phase+'-stderr.txt')).write_bytes(result.stderr)
        commands.append({'argv':argv,'exit':result.returncode});(root/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
        assert result.returncode==0,'CE process failed; inspect private runtime diagnostics'
        report=saves/('nox-offline-local.tsv' if phase=='offline' else 'nox-offline-linked.tsv')
        assert report.is_file() and report.read_text().startswith('PASS\t'),'Fresh offline/link acceptance missing'
    queue_file=saves/'ce-achievements-queue.json'
    if args.recovered_terminal:
        initial_records=json.loads((saves/'ce-playthroughs.json').read_text())['playthroughs']
        old_b=next(r for r in initial_records if not r['casual'])
        initial_queue=json.loads(queue_file.read_text())
        assert not old_b['playthrough'] and not old_b['sync_eligible'] and not any(e['request_type']=='playthrough_start' and e['client_playthrough_id']==old_b['client_playthrough_id'] for e in initial_queue),'Older local B already has a server start'
    run(local,'offline')
    offline=json.loads(queue_file.read_text())
    if args.recovered_terminal:
        assert len(offline)==15 and sum(e['state']=='permanent_failure' for e in offline)==1 and all(e['attempt_count']==0 for e in offline)
        assert sum(e['request_type']=='playthrough_start' for e in offline)==1 and sum(e['request_type']=='playthrough_end' for e in offline)==3
    else:
        assert len(offline)==12 and all(e['attempt_count']==0 and e['state']=='pending' for e in offline)
        assert sum(e['request_type']=='playthrough_start' for e in offline)==2 and sum(e['request_type']=='playthrough_event' for e in offline)==1
    (root/'offline-queue.json').write_text(json.dumps(offline,indent=2)+'\n')
    assert {p.name:sha(p) for p in saves.glob('nox-ce-*')}==original_hashes,'Offline loading rewrote worlds/sidecars'
    if args.local_only:
        report={'status':'VERIFIED','scope':'Offline half only; no later-link claim','executable_sha256':sha(exe),'offline_events':len(offline),'offline_attempts':0,'commands':commands,'artifacts':str(root)}
        (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return
    runtime=Path(tempfile.mkdtemp(prefix='codex-nox-m2-offline-link-')).resolve()
    owner=os.environ['USERDOMAIN']+'\\'+os.environ['USERNAME']
    subprocess.run(['icacls',str(runtime),'/inheritance:r','/grant:r',owner+':(OI)(CI)F'],check=True,capture_output=True)
    upstream=socket.getaddrinfo('hermesbox',8123,socket.AF_INET,socket.SOCK_STREAM)[0][4][0]
    opener=urllib.request.build_opener(urllib.request.ProxyHandler({}));requests=[];failures=[]
    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def perform(self):
            body=self.rfile.read(int(self.headers.get('Content-Length','0')))
            if len(requests)>=24 or not (self.path=='/api/v1/playthroughs' or self.path.startswith('/api/v1/playthroughs/')):
                failures.append('Request budget or route violation');self.send_response(503);self.end_headers();return
            if not body:
                requests.append({'method':self.command,'path':self.path,'error':'Missing JSON body','status':None})
                failures.append('Missing JSON body');self.send_response(400);self.end_headers();return
            data=json.loads(body);entry={'method':self.command,'path':self.path,'payload':data};requests.append(entry)
            req=urllib.request.Request('http://'+upstream+':8123'+self.path,data=body,method=self.command,headers={'Host':'hermesbox:8123','Authorization':'Bearer '+token,'Accept':'application/json','Content-Type':'application/json'})
            try:
                with opener.open(req,timeout=10) as response:code=response.status;payload=response.read()
            except urllib.error.HTTPError as error:code=error.code;payload=error.read()
            except Exception:
                code=502;payload=b'{}';failures.append('Upstream connection failed')
            entry['status']=code
            if code>=400:failures.append('Upstream HTTP '+str(code))
            self.send_response(code);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(payload)));self.end_headers();self.wfile.write(payload)
        do_POST=perform
        do_PATCH=perform
    server=http.server.HTTPServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:
        ini.read(local,encoding='utf-8');ini['nox-gameplay-test']['nox_history_linked']='true'
        ini['scummvm']['ce_link_token']=token;ini['scummvm']['ce_achievements_api_base_url']='http://127.0.0.1:'+str(server.server_port)+'/api/v1'
        config=runtime/'linked.ini'
        with config.open('w',encoding='utf-8') as out:ini.write(out)
        run(config,'linked')
        linked=json.loads(queue_file.read_text())
        if args.recovered_terminal:
            assert len(linked)==16 and sum(e['state']=='synced' for e in linked)==15 and sum(e['state']=='permanent_failure' for e in linked)==1
            assert all(e['attempt_count']==(0 if e['state']=='permanent_failure' else 1) for e in linked)
            assert len(requests)==15 and not failures
        else:assert len(linked)==14 and all(e['state']=='synced' and e['attempt_count']==1 for e in linked)
        by_key={(e['request_type'],e['event_id']):e for e in linked}
        for event in offline:
            now=by_key[(event['request_type'],event['event_id'])]
            assert all(now[k]==v for k,v in event.items() if k not in ('path','playthrough','signature','state','attempt_count','last_attempt_at','last_error')),'Deferred payload identity changed'
        if args.recovered_terminal:
            (saves/'nox-offline-linked.tsv').unlink()
            run(config,'cold-linked')
            final=json.loads(queue_file.read_text());assert final==linked and len(requests)==15,'Cold terminal reload changed queued history or transmitted more requests' 
        else:
            # Replay exactly one original start and one audit. Retain their IDs,
            # signed payloads, owner binding and attempt history.
            replay=[]
            for typ in ('playthrough_start','playthrough_event'):
                event=next(e for e in linked if e['request_type']==typ);event['state']='pending';replay.append((typ,event['event_id']))
            queue_file.write_text(json.dumps(linked,separators=(',',':')))
            (saves/'nox-offline-linked.tsv').unlink()
            run(config,'replay')
            final=json.loads(queue_file.read_text());assert len(final)==16 and all(e['state']=='synced' for e in final)
            final_by_key={(e['request_type'],e['event_id']):e for e in final}
            for key in replay:
                old=by_key[key];now=final_by_key[key]
                assert all(now[k]==old[k] for k in old if k not in ('state','attempt_count','last_attempt_at','last_error')) and now['attempt_count']==2,'Replay changed signed payload'
            assert not failures and len(requests)==18
    finally:
        server.shutdown();server.server_close();thread.join(timeout=2)
        (root/'transport.json').write_text(json.dumps({'requests':requests,'failures':failures,'budget':24},indent=2)+'\n')
    for p in saves.glob('nox-ce-*'):
        if p.suffix!='.json':assert sha(p)==original_hashes[p.name],'Online/replay rewrote saved world'
        else:
            before=original_sidecars[p.name];after=json.loads(p.read_text())
            assert all(after[k]==v for k,v in before.items() if k!='playthrough'),'Link changed sidecar world/owner/progress'

    records=json.loads((saves/'ce-playthroughs.json').read_text())['playthroughs'];account=json.loads((private/'request.json').read_text())
    audit_php=PHP.replace("'mode_status'=>$p->mode_status,","'status'=>$p->status,'started_at'=>$p->started_at?->toIso8601String(),'ended_at'=>$p->ended_at?->toIso8601String(),'mode_status'=>$p->mode_status,")
    command=['ssh','-i',str(args.identity.resolve(strict=True)),'-o','IdentitiesOnly=yes','-o','BatchMode=yes','ceachievements@hermesbox','cd /home/ceachievements/ce-achievements && php -r '+shlex.quote(audit_php)]
    audit=subprocess.run(command,input=json.dumps({'email':account['email'],'clients':list({r['client_playthrough_id'] for r in records}|({read_save_enhancements(p)['session'] for p in saves.glob('nox-ce-*.g1')} if args.recovered_terminal else set()))}).encode(),capture_output=True,timeout=30)
    assert audit.returncode==0,'Read-only server audit failed';remote=json.loads(audit.stdout)
    (root/'server-observation.json').write_text(json.dumps(remote,indent=2)+'\n')
    assert remote['non_admin'] and remote['achievement_definitions']==remote['achievement_events']==0 and len(remote['sessions'])==len(records)==2
    server_by={r['client_id']:r for r in remote['sessions']};assert len({r['server_id'] for r in remote['sessions']})==2
    for record in records:
        row=server_by[record['client_playthrough_id']];assert row['server_id']==record['playthrough'] and row['game']=='nox-archaist' and row['variant']=='apple2-v119'
        assert row['mode_status']==('casual' if record['casual'] else 'pure_l33t') and set(row['assistance_flags'] or [])==set(record['assistance_flags'])
        assert datetime.datetime.fromisoformat(row['started_at'])==datetime.datetime.fromisoformat(record['started_at']),'Original start time changed'
        if args.recovered_terminal:
            assert row['status']==record['status']==('completed' if record['casual'] else 'archived')
            assert datetime.datetime.fromisoformat(row['ended_at'])==datetime.datetime.fromisoformat(record['ended_at']),'Terminal time changed'

        save=row['saves'];assert len(save)==1;save=save[0]
        latest=max((p for p in saves.glob('nox-ce-*.g[01]') if read_save_enhancements(p)['session']==record['client_playthrough_id']),key=lambda p:struct.unpack_from('<I',p.read_bytes(),8)[0])
        state=read_save_enhancements(latest)
        assert save['client_save_id']==record['client_playthrough_id']+':slot:1' and str(save['slot'])=='1' and save['label']==state['description'] and save['hash']==sha(latest) and save['loaded']
        assert save['tainted']==record['casual'] and set(save['flags'] or [])==set(record['assistance_flags'])
        expected_events={e['event_id'] for e in final if e['request_type']=='playthrough_event' and e['client_playthrough_id']==record['client_playthrough_id']}
        assert {e['event_id'] for e in row['events']}==expected_events
    (root/'server-verification.json').write_text(json.dumps({'status':'VERIFIED','server':remote},indent=2)+'\n')
    assert all(sha(Path(p))==v for p,v in fixture['original_sha256'].items())
    report={'status':'VERIFIED','scope':'Recovered unknown-history R completed offline plus genuine older local-only B archived before backfill/link; exact worlds/identity/times/audits/terminal state and zero awards' if args.recovered_terminal else 'Ordinary two-session offline creation/save/cold load, offline assistance A/clean B, later link, immutable deferred metadata, two event replays and independent zero-award hermesbox audit','executable_sha256':sha(exe),'fixture':str(baseline),'fixture_executable_sha256':fixture['executable_sha256'],'recovered_terminal':args.recovered_terminal,'mode':read_save_enhancements(next(saves.glob('nox-ce-*.g1')))['saved_mode'],'original_sha256':fixture['original_sha256'],'offline_events':len(offline),'offline_attempts':0,'http_requests':len(requests),'request_budget':24,'commands':commands,'artifacts':str(root)}
    (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
