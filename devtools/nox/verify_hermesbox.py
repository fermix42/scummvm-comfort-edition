#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only server/database audit against CE's actual session and save identities."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

PHP=r'''
require 'vendor/autoload.php';$app=require 'bootstrap/app.php';
$app->make(Illuminate\Contracts\Console\Kernel::class)->bootstrap();
if(gethostname()!=='hermesbox'||$app->environment()!=='local')throw new RuntimeException('Wrong environment');
$request=json_decode(file_get_contents('php://stdin'),true,512,JSON_THROW_ON_ERROR);
$user=App\Models\User::query()->where('email',$request['email'])->firstOrFail();
$rows=App\Models\GamePlaythrough::query()->where('user_id',$user->id)->whereIn('client_playthrough_id',$request['clients'])
    ->with(['game','gameVariant','saves'])->get()->map(fn($p)=>[
        'client_id'=>$p->client_playthrough_id,'server_id'=>$p->public_id,'title'=>$p->title,
        'game'=>$p->game->slug,'variant'=>$p->gameVariant->key,
        'saves'=>$p->saves->map(fn($s)=>['client_save_id'=>$s->client_save_id,'slot'=>$s->slot,'label'=>$s->label,
            'save_hash'=>$s->save_hash,'loaded'=>$s->last_loaded_at!==null])->all(),
    ])->all();
echo json_encode(['non_admin'=>!$user->is_admin,'sessions'=>$rows,
    'achievement_definitions'=>App\Models\Achievement::query()->whereHas('game',fn($q)=>$q->where('slug','nox-archaist'))->count(),
    'achievement_events'=>App\Models\AchievementEvent::query()->where('user_id',$user->id)->count()],JSON_THROW_ON_ERROR);
'''


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--identity',type=Path,required=True)
    parser.add_argument('--private',type=Path,required=True)
    parser.add_argument('--ce-evidence',type=Path,required=True)
    args=parser.parse_args()
    root=args.ce_evidence.resolve(strict=True)
    ce=json.loads((root/'verification.json').read_text())
    records=json.loads((root/'saves/ce-playthroughs.json').read_text())['playthroughs']
    request=json.loads((args.private.resolve(strict=True)/'request.json').read_text())
    payload={'email':request['email'],'clients':[row['client_playthrough_id'] for row in records]}
    command=['ssh','-i',str(args.identity.resolve(strict=True)),'-o','IdentitiesOnly=yes','-o','BatchMode=yes',
             'ceachievements@hermesbox','cd /home/ceachievements/ce-achievements && php -r '+shlex.quote(PHP)]
    result=subprocess.run(command,input=json.dumps(payload).encode(),capture_output=True,timeout=30)
    if result.returncode:
        raise RuntimeError('Read-only hermesbox audit failed')
    server=json.loads(result.stdout)
    failures=[]
    if not server['non_admin'] or server['achievement_definitions'] or server['achievement_events']:
        failures.append('Non-admin/zero-achievement invariant failed')
    if len(records)!=2 or len(server['sessions'])!=2:
        failures.append('Two matching server sessions missing')
    if len({row['playthrough'] for row in records})!=2:
        failures.append('CE registry server session IDs are not distinct')
    if len({row['server_id'] for row in server['sessions']})!=2:
        failures.append('Website server session IDs are not distinct')
    by_client={row['client_id']:row for row in server['sessions']}
    for record in records:
        row=by_client.get(record['client_playthrough_id'])
        if not row or row['server_id']!=record['playthrough']:
            failures.append('CE/server session identity mismatch: '+record['title'])
            continue
        if row['game']!='nox-archaist' or row['variant']!='apple2-v119' or len(row['saves'])!=1:
            failures.append('Wrong variant or save ownership/count: '+record['title'])
            continue
        save=row['saves'][0]
        expected=record['client_playthrough_id']+':slot:1'
        if save['client_save_id']!=expected or str(save['slot'])!='1' or not save['loaded']:
            failures.append('Save/reload identity mismatch: '+record['title'])
        expected_label={'M1 session A':'Session A slot1','M1 session B':'Session B slot1'}.get(record['title'])
        if not expected_label or save['label']!=expected_label:
            failures.append('Human save label was not preserved: '+record['title'])
        hashes=[]
        for sidecar in (root/'saves').glob('nox-ce-*.g1.ce-achievements.json'):
            metadata=json.loads(sidecar.read_text())
            if metadata.get('client_playthrough_id')==record['client_playthrough_id']:
                physical=sidecar.with_name(sidecar.name.removesuffix('.ce-achievements.json'))
                hashes.append(hashlib.sha256(physical.read_bytes()).hexdigest())
        if save['save_hash'] not in hashes:
            failures.append('Server hash does not match local durable generation: '+record['title'])
    evidence={'status':'IN PROGRESS' if failures else 'VERIFIED','scope':'hermesbox database compared with actual CE cold-save session/hash identities',
              'executable_sha256':ce['executable_sha256'],'server':server,'failures':failures}
    (root/'server-verification.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(json.dumps(evidence,indent=2))
    if failures:
        raise SystemExit(1)


if __name__=='__main__':
    main()
