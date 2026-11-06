#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Replay authentic earlier clean start replies through an isolated LAN test relay.

All writes go to the already-authorized dedicated hermesbox account through the
real API. Credentials/config stay ACL-private outside artifacts. No website code
is deployed. The relay forwards every request unchanged and reuses only an older
response body for the SAME client/server identity. No achievements are sent.
"""
import argparse
import configparser
import datetime
import http.server
import json
import os
import socket
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--exe',type=Path,required=True)
    ap.add_argument('--game',type=Path,required=True)
    ap.add_argument('--private',type=Path,required=True)
    ap.add_argument('--artifacts',type=Path,required=True)
    ap.add_argument('--classic',action='store_true')
    args=ap.parse_args()
    root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    root.mkdir(parents=True,exist_ok=False)
    private=Path(tempfile.mkdtemp(prefix='codex-nox-m2-relay-')).resolve()
    if os.name=='nt':
        owner=os.environ['USERDOMAIN']+'\\'+os.environ['USERNAME']
        subprocess.run(['icacls',str(private),'/inheritance:r','/grant:r',owner+':(OI)(CI)F'],check=True,capture_output=True)
    ini=configparser.ConfigParser(interpolation=None)
    ini.read(args.private.resolve(strict=True)/'ce-private.ini',encoding='utf-8')
    assert ini['scummvm']['ce_achievements_api_base_url']=='http://hermesbox:8123/api/v1'
    assert ini['scummvm'].get('ce_link_token')
    lock=threading.Lock()
    cached={}
    summary={'received_requests':0,'requests':0,'upstream_family':'IPv4','delayed_clean_replies':0,'old_clean_load_payloads':0,'failures':[]}
    upstream=socket.getaddrinfo('hermesbox',8123,socket.AF_INET,socket.SOCK_STREAM)[0][4][0]
    opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))

    # Existing site contract is60 requests/minute. Reserve enough budget for
    # this bounded scenario; never raise the server limit or hammer429 retries.
    budget_wait=0
    while True:
        probe=urllib.request.Request('http://'+upstream+':8123/api/v1/link',headers={'Host':'hermesbox:8123','Authorization':'Bearer '+ini['scummvm']['ce_link_token'],'Accept':'application/json'})
        try:
            with opener.open(probe,timeout=10) as response:
                remaining=int(response.headers.get('X-RateLimit-Remaining','0'))
                reset=int(response.headers.get('X-RateLimit-Reset',str(int(time.time())+61)))
        except urllib.error.HTTPError as error:
            if error.code!=429: raise
            remaining=0;reset=int(time.time())+int(error.headers.get('Retry-After','61'))
        if remaining>=40: break
        delay=max(1,min(65,reset-int(time.time())+1));budget_wait+=delay
        deadline=time.monotonic()+delay
        while time.monotonic()<deadline: time.sleep(min(5,deadline-time.monotonic()))
        if budget_wait>130: raise RuntimeError('Dedicated API budget unavailable after bounded wait')
    summary['api_budget_wait_seconds']=budget_wait

    class Relay(http.server.BaseHTTPRequestHandler):
        def log_message(self,*_): pass  # Never log Authorization or request dumps.
        def do_GET(self): self.forward()
        def do_POST(self): self.forward()
        def do_PATCH(self): self.forward()
        def forward(self):
            if self.path=='/nox-health':
                self.send_response(200);self.send_header('Content-Length','0');self.end_headers();return
            try:
                with lock: summary['received_requests']+=1
                assert self.path=='/api/v1/link' or self.path.startswith('/api/v1/playthroughs')
                assert '?' not in self.path
                size=int(self.headers.get('Content-Length','0'));assert 0<=size<=1048576
                body=self.rfile.read(size) if size else None
                parsed=json.loads(body) if body else {}
                request=urllib.request.Request('http://'+upstream+':8123'+self.path,data=body,method=self.command,
                    headers={'Host':'hermesbox:8123','Authorization':self.headers.get('Authorization',''),'Accept':'application/json','Content-Type':'application/json'})
                try:
                    with opener.open(request,timeout=20) as response: code,data=response.status,response.read()
                except urllib.error.HTTPError as error: code,data=error.code,error.read()
                with lock:
                    summary['requests']+=1
                    if code not in (200,201): summary['failures'].append({'path':self.path,'http':code})
                    if self.path=='/api/v1/playthroughs' and code in (200,201):
                        actual=json.loads(data)['playthrough'];client=parsed['client_playthrough_id']
                        assert actual['client_playthrough_id']==client
                        if client not in cached: cached[client]=data
                        else:
                            earlier=json.loads(cached[client])['playthrough']
                            assert earlier['id']==actual['id'] and earlier['game']==actual['game'] and earlier['variant']==actual['variant']
                            if earlier['mode_status']=='pure_l33t' and actual['mode_status']=='casual': summary['delayed_clean_replies']+=1
                            data=cached[client]  # Authentic old reply, never forged identity/mode.
                    if self.path.endswith('/saves') and parsed.get('label')=='Before dialogue' and parsed.get('last_loaded_at'):
                        assert parsed.get('tainted') is True
                        assert set(parsed.get('assistance_flags',[]))=={'nox_controlled_dialogue_fixture','tainted_save_loaded'}
                        summary['old_clean_load_payloads']+=1
                self.send_response(code);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
            except Exception as error:
                with lock: summary['failures'].append({'type':type(error).__name__})
                self.send_response(502);self.end_headers()

    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Relay)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    with opener.open(f'http://127.0.0.1:{server.server_port}/nox-health',timeout=5) as health:
        assert health.status==200
    ini['scummvm']['ce_achievements_api_base_url']=f'http://127.0.0.1:{server.server_port}/api/v1'
    config=private/'ce-private.ini'
    with config.open('w',encoding='utf-8') as out: ini.write(out)
    command=[sys.executable,str(Path(__file__).with_name('test_dialogue.py')),'--exe',str(args.exe.resolve(strict=True)),
        '--game',str(args.game.resolve(strict=True)),'--private-config',str(config),'--artifacts',str(root/'ce'),'--lifecycle']
    if args.classic: command.append('--classic')
    try:
        result=subprocess.run(command,capture_output=True,timeout=180)
        (root/'client-stdout.txt').write_bytes(result.stdout);(root/'client-stderr.txt').write_bytes(result.stderr)
    finally:
        server.shutdown();server.server_close();thread.join(timeout=5)
    reports=list((root/'ce').glob('*/verification.json'))
    summary.update({'status':'IN PROGRESS','client_exit':result.returncode,'client_report':str(reports[0]) if len(reports)==1 else None,
        'scope':'Real hermesbox API via loopback relay; authentic earlier clean response replay after assistance, immutable signed event retries and old-clean-save tainted metadata. No offline-to-link or complete lifecycle claim.'})
    passed=result.returncode==0 and len(reports)==1 and not summary['failures'] and summary['delayed_clean_replies']>0 and summary['old_clean_load_payloads']>=2
    if passed: summary['status']='VERIFIED'
    (root/'relay-verification.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))
    if not passed: raise SystemExit(1)


if __name__=='__main__': main()
