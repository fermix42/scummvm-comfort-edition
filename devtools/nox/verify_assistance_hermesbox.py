#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only hermesbox audit for the three-save assisted A / clean B scenario."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
from enhancement_oracle import read_save_enhancements

PHP=r'''
require 'vendor/autoload.php';$app=require 'bootstrap/app.php';
$app->make(Illuminate\Contracts\Console\Kernel::class)->bootstrap();
if(gethostname()!=='hermesbox'||$app->environment()!=='local'||rtrim(config('app.url'),'/')!=='http://hermesbox:8123')throw new RuntimeException('Wrong environment');
$request=json_decode(file_get_contents('php://stdin'),true,512,JSON_THROW_ON_ERROR);
$user=App\Models\User::query()->where('email',$request['email'])->firstOrFail();
$rows=App\Models\GamePlaythrough::query()->where('user_id',$user->id)->whereIn('client_playthrough_id',$request['clients'])
 ->with(['game','gameVariant','saves','events'])->get()->map(fn($p)=>[
 'client_id'=>$p->client_playthrough_id,'server_id'=>$p->public_id,'game'=>$p->game->slug,'variant'=>$p->gameVariant->key,
 'mode_status'=>$p->mode_status,'assistance_flags'=>$p->assistance_flags,
 'events'=>$p->events->map(fn($e)=>['event_id'=>$e->event_id,'type'=>$e->type,'flags'=>$e->assistance_flags])->all(),
 'saves'=>$p->saves->map(fn($s)=>['client_save_id'=>$s->client_save_id,'slot'=>$s->slot,'label'=>$s->label,'hash'=>$s->save_hash,'loaded'=>$s->last_loaded_at!==null,'tainted'=>$s->tainted,'flags'=>$s->assistance_flags])->all(),
 ])->all();
echo json_encode(['non_admin'=>!$user->is_admin,'sessions'=>$rows,
 'achievement_definitions'=>App\Models\Achievement::query()->whereHas('game',fn($q)=>$q->where('slug','nox-archaist'))->count(),
 'achievement_events'=>App\Models\AchievementEvent::query()->where('user_id',$user->id)->count()],JSON_THROW_ON_ERROR);
'''


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--identity',type=Path,required=True);ap.add_argument('--private',type=Path,required=True);ap.add_argument('--ce-evidence',type=Path,required=True)
    args=ap.parse_args();root=args.ce_evidence.resolve(strict=True)
    ce=json.loads((root/'verification.json').read_text());records=json.loads((root/'saves/ce-playthroughs.json').read_text())['playthroughs']
    account=json.loads((args.private.resolve(strict=True)/'request.json').read_text())
    request={'email':account['email'],'clients':[r['client_playthrough_id'] for r in records]}
    command=['ssh','-i',str(args.identity.resolve(strict=True)),'-o','IdentitiesOnly=yes','-o','BatchMode=yes','ceachievements@hermesbox','cd /home/ceachievements/ce-achievements && php -r '+shlex.quote(PHP)]
    result=subprocess.run(command,input=json.dumps(request).encode(),capture_output=True,timeout=30)
    if result.returncode: raise RuntimeError('Read-only server audit failed')
    server=json.loads(result.stdout);failures=[]
    try:
        assert server['non_admin'] and server['achievement_definitions']==server['achievement_events']==0
        assert len(server['sessions'])==len(records)==2
        by_client={r['client_id']:r for r in server['sessions']}
        expected={}
        for path in (root/'saves').glob('nox-ce-*.g1'):
            state=read_save_enhancements(path)
            expected.setdefault(state['session'],{})[str(state['slot'])]={'label':state['description'],'hash':hashlib.sha256(path.read_bytes()).hexdigest()}
        queue=json.loads((root/'saves/ce-achievements-queue.json').read_text())
        for record in records:
            row=by_client[record['client_playthrough_id']]
            assert row['server_id']==record['playthrough'] and row['game']=='nox-archaist' and row['variant']=='apple2-v119'
            assisted=record['casual'];reasons={'nox_controlled_dialogue_fixture','tainted_save_loaded'} if assisted else set()
            assert row['mode_status']==('casual' if assisted else 'pure_l33t') and set(row['assistance_flags'] or [])==reasons
            assert len(row['saves'])==(2 if assisted else 1)
            for save in row['saves']:
                slot=str(save['slot']);local=expected[record['client_playthrough_id']][slot]
                assert save['client_save_id']==record['client_playthrough_id']+':slot:'+slot
                assert save['label']==local['label'] and save['hash']==local['hash'] and save['loaded']
                assert save['tainted']==assisted and set(save['flags'] or [])==reasons
            queued_audits={event['event_id'] for event in queue if event['request_type']=='playthrough_event' and event['playthrough']==record['playthrough']}
            assert {event['event_id'] for event in row['events']}==queued_audits
            for event in row['events']: assert set(event['flags'] or [])<=reasons
    except (AssertionError,KeyError) as error: failures.append(type(error).__name__)
    report={'status':'IN PROGRESS' if failures else 'VERIFIED','scope':'Read-only hermesbox exact client/server session, three save hashes/labels/load metadata, permanent reasons and audit-event replay; zero achievement invariant','executable_sha256':ce['executable_sha256'],'server':server,'failures':failures}
    (root/'server-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
    if failures: raise SystemExit(1)


if __name__=='__main__': main()
