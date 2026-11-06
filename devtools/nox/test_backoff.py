#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded loopback-only CE Retry-After/cold-process verification. No achievements.

--reproduce stops an owned pre-fix CE child after exactly two premature requests.
Normal mode uses eight HTTP requests total across numeric/date/default429 and
permanent401/409/422 cases. No real account or external API is used.
"""
import argparse
import calendar
import configparser
import datetime
import email.utils
import hashlib
import http.server
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import threading
import time


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('exe','game','artifacts'):ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--reproduce',action='store_true');args=ap.parse_args()
    exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True)
    original={str(p):sha(p) for p in game.glob('*.HDV')};assert original
    root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True,exist_ok=False)
    private=Path(tempfile.mkdtemp(prefix='codex-nox-m2-backoff-')).resolve()
    owner=os.environ['USERDOMAIN']+'\\'+os.environ['USERNAME']
    subprocess.run(['icacls',str(private),'/inheritance:r','/grant:r',owner+':(OI)(CI)F'],capture_output=True,check=True)
    token=secrets.token_hex(32) # Local mock only; never sent outside loopback or included in evidence.
    env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy')
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    cases=[]
    names=('reproduce',) if args.reproduce else ('numeric','http-date','default','401','409','422')
    for name in names:
        work=root/name;saves=work/'saves';saves.mkdir(parents=True)
        requests=[];second=threading.Event();release=threading.Event();failures=[]
        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self,*a):pass
            def do_POST(self):
                raw=self.rfile.read(int(self.headers.get('Content-Length','0')))
                if len(requests)>=2 or self.path!='/api/v1/playthroughs':
                    failures.append('Request budget or route exceeded');self.send_response(503);self.end_headers();return
                payload=json.loads(raw);requests.append({'path':self.path,'payload':payload,'body':raw.decode(),'at':time.monotonic()})
                if args.reproduce and len(requests)==2:second.set();release.wait(10)
                code=int(name) if name.isdigit() else 429 if len(requests)==1 or args.reproduce else 201
                try:
                    self.send_response(code)
                    if code==429 and name!='default':
                        retry=email.utils.formatdate(time.time()+12,usegmt=True) if name=='http-date' else '12'
                        self.send_header('Retry-After',retry)
                    self.send_header('Content-Type','application/json')
                    response={'playthrough':{'id':'local-backoff-session','client_playthrough_id':payload['client_playthrough_id'],'game':payload['game'],'variant':payload['variant'],'mode_status':'pure_l33t'}} if code==201 else {'message':'Bounded local response'}
                    data=json.dumps(response).encode();self.send_header('Content-Length',str(len(data)));self.end_headers()
                    self.wfile.write(data)
                except (BrokenPipeError,ConnectionResetError):
                    if not args.reproduce:failures.append("Unexpected connection close")
        server=http.server.HTTPServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        config=private/(name+'.ini');ini=configparser.ConfigParser(interpolation=None)
        ini['scummvm']={'gui_return_to_launcher':'false','confirm_exit':'false','updates_check':'0','ce_link_token':token,'ce_achievements_api_base_url':'http://127.0.0.1:'+str(server.server_port)+'/api/v1'}
        ini['nox-gameplay-test']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'nox_history_validation':'0' if args.reproduce else '8','nox_backoff_phase':'1','nox_backoff_permanent':'true' if name.isdigit() else 'false'}
        if args.reproduce:ini['nox-gameplay-test'].update({'nox_gameplay_validation':'true','nox_online_validation':'true'})
        with config.open('w',encoding='utf-8') as out:ini.write(out)
        commands=[]
        def argv(phase):return [str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={work/(phase+"-engine.log")}', 'nox-gameplay-test']
        def run(phase):
            ini.read(config,encoding='utf-8');ini['nox-gameplay-test']['nox_backoff_phase']=str(phase)
            with config.open('w',encoding='utf-8') as out:ini.write(out)
            command=argv(str(phase));result=subprocess.run(command,env=env,startupinfo=startup,capture_output=True,timeout=25)
            (work/(str(phase)+'-stdout.txt')).write_bytes(result.stdout);(work/(str(phase)+'-stderr.txt')).write_bytes(result.stderr)
            commands.append({'argv':command,'exit':result.returncode});(work/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
            assert result.returncode==0 and (saves/f'nox-backoff-{phase}.tsv').read_text().startswith('PASS\t'),str(work)
            return json.loads((saves/'ce-achievements-queue.json').read_text())
        try:
            if args.reproduce:
                command=argv('reproduce');child=subprocess.Popen(command,env=env,startupinfo=startup,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                try:
                    assert second.wait(20),'Pre-fix second request not observed'
                    child.kill();stdout,stderr=child.communicate(timeout=10)
                    (work/'stdout.txt').write_bytes(stdout);(work/'stderr.txt').write_bytes(stderr)
                    commands.append({'argv':command,'exit':child.returncode,'owned_child_stopped_after':2})
                finally:
                    if child.poll() is None:child.kill();child.communicate(timeout=10)
                    release.set()
                assert len(requests)==2 and requests[0]['body']==requests[1]['body'] and requests[1]['at']-requests[0]['at']<1
                cases.append({'case':name,'premature_interval_seconds':requests[1]['at']-requests[0]['at'],'requests':2,'commands':commands})
            else:
                first=run(1);assert len(first)==len(requests)==1 and first[0]['attempt_count']==1
                expected='permanent_failure' if name.isdigit() else 'pending';assert first[0]['state']==expected
                before=dict(first[0]);ini.read(config,encoding='utf-8')
                deadline=ini['scummvm'].get('ce_sync_retry_not_before')
                if not name.isdigit():
                    assert deadline and ini['scummvm']['ce_sync_retry_endpoint']==ini['scummvm']['ce_achievements_api_base_url']
                    remaining=int(deadline)-calendar.timegm(time.localtime());assert 1<=remaining<=(61 if name=='default' else 13)
                else:assert not deadline
                cold=run(2);assert cold==first and len(requests)==1,'Cold retry changed payload/attempts or contacted endpoint'
                if name in ('numeric','http-date'):
                    remaining=int(deadline)-calendar.timegm(time.localtime())
                    if remaining>0:time.sleep(remaining+0.2)
                    final=run(3);assert len(final)==1 and final[0]['state']=='synced' and final[0]['attempt_count']==2 and len(requests)==2
                    assert requests[0]['body']==requests[1]['body'] and requests[1]['at']-requests[0]['at']>=12,'Retry sent early or changed payload'
                    assert all(final[0][k]==v for k,v in before.items() if k not in ('state','attempt_count','last_attempt_at','last_error','playthrough')),'Signed event identity changed'
                else:final=cold
                cases.append({'case':name,'requests':len(requests),'cold_attempt_count_unchanged':True,'final_state':final[0]['state'],'commands':commands})
            assert not failures
        finally:
            release.set();server.shutdown();server.server_close();thread.join(timeout=2)
            (work/'transport.json').write_text(json.dumps({'requests':requests,'failures':failures,'budget':2},indent=2)+'\n')
    assert all(sha(Path(p))==v for p,v in original.items())
    for p in root.rglob('*'):
        if p.is_file():assert token.encode() not in p.read_bytes(),'Local mock token leaked to artifact'
    report={'status':'VERIFIED','scope':'Bounded pre-fix two-request reproduction' if args.reproduce else 'Actual CE numeric/date Retry-After cooldown/cold restart/same signed request resumption, default429 cold hold and permanent401/409/422; loopback only, eight HTTP requests, no achievement events','executable_sha256':sha(exe),'cases':cases,'http_requests':sum(c['requests'] for c in cases),'original_sha256':original,'artifacts':str(root)}
    (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
