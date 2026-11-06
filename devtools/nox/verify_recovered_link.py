#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only audit of an existing completed recovered/legacy terminal-link run.

Never reruns CE or sends HTTP. Resolve the newest snapshot by envelope sequence,
not generation suffix. Intended also for retaining evidence after verifier fixes.
"""
import argparse
import configparser
import datetime
import hashlib
import json
from pathlib import Path
import shlex
import struct
import subprocess
from enhancement_oracle import read_save_enhancements
from verify_assistance_hermesbox import PHP


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('exe','baseline','evidence','private','identity'):ap.add_argument('--'+name,type=Path,required=True)
    a=ap.parse_args();root=a.evidence.resolve(strict=True);baseline=a.baseline.resolve(strict=True);saves=root/'saves'
    fixture=json.loads((baseline/'verification.json').read_text());assert fixture['status']=='VERIFIED' and fixture['executable_sha256']==sha(a.exe)
    commands=json.loads((root/'commands.json').read_text());assert len(commands)==3 and all(c['exit']==0 for c in commands)
    for name in ('nox-offline-local.tsv','nox-offline-linked.tsv'):assert (saves/name).read_text().startswith('PASS\t')
    ini=configparser.ConfigParser(interpolation=None);ini.read(root/'offline.ini',encoding='utf-8');assert ini['nox-gameplay-test']['nox_history_validation']=='7' and not ini['scummvm'].get('ce_link_token')
    transport=json.loads((root/'transport.json').read_text());assert not transport['failures'] and len(transport['requests'])==15 and all(r['status'] in (200,201) for r in transport['requests'])
    patches=[r for r in transport['requests'] if r['method']=='PATCH'];assert len(patches)==3 and [r['payload']['status'] for r in patches]==['completed','abandoned','archived']
    offline=json.loads((root/'offline-queue.json').read_text());final=json.loads((saves/'ce-achievements-queue.json').read_text())
    assert len(offline)==15 and all(e['attempt_count']==0 for e in offline)
    assert len(final)==16 and sum(e['state']=='synced' for e in final)==15 and sum(e['state']=='permanent_failure' for e in final)==1
    assert all(e['attempt_count']==(1 if e['state']=='synced' else 0) for e in final)
    by_key={(e['request_type'],e['event_id']):e for e in final}
    for event in offline:
        now=by_key[(event['request_type'],event['event_id'])]
        assert all(now[k]==v for k,v in event.items() if k not in ('path','playthrough','signature','state','attempt_count','last_attempt_at','last_error'))
    source=baseline/'delete-keep/saves';latest={};all_owners=set()
    for p in saves.glob('nox-ce-*.g[01]'):
        assert sha(p)==sha(source/p.name),'Saved world changed'
        state=read_save_enhancements(p);owner=state['session'];all_owners.add(owner)
        sequence=struct.unpack_from('<I',p.read_bytes(),8)[0]
        if owner not in latest or sequence>latest[owner][0]:latest[owner]=(sequence,p,state)
    for p in saves.glob('nox-ce-*.json'):
        old=json.loads((source/p.name).read_text());now=json.loads(p.read_text())
        assert all(now[k]==v for k,v in old.items() if k!='playthrough')
    records=json.loads((saves/'ce-playthroughs.json').read_text())['playthroughs'];assert len(records)==2
    initial=json.loads((source/'ce-playthroughs.json').read_text())['playthroughs'];old_b=next(r for r in initial if not r['casual'])
    assert not old_b['sync_eligible'] and not old_b['playthrough']
    old_queue=json.loads((source/'ce-achievements-queue.json').read_text())
    assert not any(e['request_type']=='playthrough_start' and e['client_playthrough_id']==old_b['client_playthrough_id'] for e in old_queue)
    account=json.loads((a.private/'request.json').read_text())
    php=PHP.replace("'mode_status'=>$p->mode_status,","'status'=>$p->status,'started_at'=>$p->started_at?->toIso8601String(),'ended_at'=>$p->ended_at?->toIso8601String(),'mode_status'=>$p->mode_status,")
    cmd=['ssh','-i',str(a.identity.resolve(strict=True)),'-o','IdentitiesOnly=yes','-o','BatchMode=yes','ceachievements@hermesbox','cd /home/ceachievements/ce-achievements && php -r '+shlex.quote(php)]
    result=subprocess.run(cmd,input=json.dumps({'email':account['email'],'clients':list(all_owners)}).encode(),capture_output=True,timeout=30);assert result.returncode==0
    remote=json.loads(result.stdout);(root/'server-observation.json').write_text(json.dumps(remote,indent=2)+'\n')
    assert remote['non_admin'] and remote['achievement_definitions']==remote['achievement_events']==0 and len(remote['sessions'])==2
    rows={r['client_id']:r for r in remote['sessions']};assert len({r['server_id'] for r in rows.values()})==2
    for record in records:
        owner=record['client_playthrough_id'];row=rows[owner]
        assert row['server_id']==record['playthrough'] and row['game']=='nox-archaist' and row['variant']=='apple2-v119'
        assert row['status']==record['status']==('completed' if record['casual'] else 'archived')
        assert row['mode_status']==('casual' if record['casual'] else 'pure_l33t') and set(row['assistance_flags'] or [])==set(record['assistance_flags'])
        if record['casual']:assert set(record['assistance_flags'])=={'unknown_save_history','tainted_save_loaded'}
        else:assert record['client_playthrough_id']==old_b['client_playthrough_id'] and record['started_at']==old_b['started_at']
        for key in ('started_at','ended_at'):assert datetime.datetime.fromisoformat(row[key])==datetime.datetime.fromisoformat(record[key])
        assert len(row['saves'])==1;save=row['saves'][0];_,path,state=latest[owner]
        assert save['client_save_id']==owner+':slot:1' and str(save['slot'])=='1' and save['label']==state['description'] and save['hash']==sha(path) and save['loaded']
        assert save['tainted']==record['casual'] and set(save['flags'] or [])==set(record['assistance_flags'])
        expected={e['event_id'] for e in final if e['request_type']=='playthrough_event' and e['client_playthrough_id']==owner}
        assert {e['event_id'] for e in row['events']}==expected
    assert all(sha(Path(p))==v for p,v in fixture['original_sha256'].items())
    (root/'server-verification.json').write_text(json.dumps({'status':'VERIFIED','server':remote},indent=2)+'\n')
    report={'status':'VERIFIED','scope':'Existing run independently audited without CE/API replay: recovered Casual completed, genuine older B backfilled then archived, exact full-world runtime reports, immutable events, original start/end times, correct sequence-selected save hashes and zero awards','executable_sha256':sha(a.exe),'fixture':str(baseline),'mode':next(iter(latest.values()))[2]['saved_mode'],'http_requests':15,'verification_http_requests':0,'original_sha256':fixture['original_sha256'],'commands':commands,'artifacts':str(root)}
    (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
