#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Create only the authorized hermesbox Nox test account/registration data.

Uses existing Laravel models over the dedicated SSH identity. Does not deploy
code or migrations. Secrets are runtime input over SSH and private local files,
never argv, source constants, stdout or an artifact report. Reusing --private
recovers the same account/credential after an interrupted outcome.
"""
import argparse
import json
import os
from pathlib import Path
import secrets
import shlex
import subprocess
import tempfile

PHP = r'''
require 'vendor/autoload.php';
$app=require 'bootstrap/app.php';
$app->make(Illuminate\Contracts\Console\Kernel::class)->bootstrap();
if (gethostname()!=='hermesbox' || $app->environment()!=='local' || rtrim(config('app.url'),'/')!=='http://hermesbox:8123') throw new RuntimeException('Refusing another environment');
$request=json_decode(file_get_contents('php://stdin'),true,512,JSON_THROW_ON_ERROR);
if (!preg_match('/^[a-f0-9]{64}$/',$request['token']) || !preg_match('/^[a-f0-9]{64}$/',$request['password']) ||
    !preg_match('/^nox-m1-[a-f0-9]{16}@hermesbox\.test$/',$request['email'])) throw new RuntimeException('Invalid dedicated account input');
$result=Illuminate\Support\Facades\DB::transaction(function() use($request){
    $existing=App\Models\Game::query()->where('slug','nox-archaist')->first();
    if ($existing && $existing->source_engine!=='nox') throw new RuntimeException('Unexpected registration owner');
    if ($existing && $existing->achievements()->exists()) throw new RuntimeException('Unexpected award definitions for zero-award test');
    $game=App\Models\Game::query()->updateOrCreate(['slug'=>'nox-archaist'],$request['game']);
    $variant=App\Models\GameVariant::query()->updateOrCreate(['game_id'=>$game->id,'key'=>'apple2-v119'],$request['variant']);
    $user=App\Models\User::query()->firstOrCreate(['email'=>$request['email']],[
        'name'=>'Dedicated Nox M1 test','password'=>$request['password'],'timezone'=>'America/Los_Angeles',
    ]);
    if ($user->is_admin || $user->name!=='Dedicated Nox M1 test') throw new RuntimeException('Unexpected test account owner/role');
    $user->forceFill(['email_verified_at'=>now(),'is_admin'=>false])->save();
    App\Models\GameCredential::query()->updateOrCreate(['user_id'=>$user->id,'name'=>$request['credential_name']],
        ['token_hash'=>hash('sha256',$request['token']),'revoked_at'=>null]);
    return ['host'=>gethostname(),'non_admin'=>!$user->is_admin,'game'=>$game->slug,'variant'=>$variant->key,
        'achievements'=>$game->achievements()->count()];
});
echo json_encode($result,JSON_THROW_ON_ERROR);
'''


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--identity',type=Path,required=True)
    parser.add_argument('--game',type=Path,required=True)
    parser.add_argument('--private',type=Path)
    args=parser.parse_args()
    identity=args.identity.resolve(strict=True)
    game=args.game.resolve(strict=True)
    private=args.private.resolve(strict=True) if args.private else Path(tempfile.mkdtemp(prefix='codex-nox-m1-hermesbox-')).resolve()
    if os.name=='nt':
        owner=os.environ['USERDOMAIN']+'\\'+os.environ['USERNAME']
        subprocess.run(['icacls',str(private),'/inheritance:r','/grant:r',owner+':(OI)(CI)F'],capture_output=True,check=True)
    request_file=private/'request.json'
    if request_file.exists():
        request=json.loads(request_file.read_text(encoding='utf-8'))
    else:
        nonce=secrets.token_hex(8)
        request={'email':f'nox-m1-{nonce}@hermesbox.test','password':secrets.token_hex(32),'token':secrets.token_hex(32),
                 'credential_name':'Nox M1 hermesbox '+nonce,
                 'game':{'title':'Nox Archaist','summary':'Base game v119 Play Sessions for the experimental ScummVM Comfort Edition engine. No achievements have been defined.',
                         'source_engine':'nox','source_game_id':'nox','ce_supported':True},
                 'variant':{'title':'Apple II — Base Game v119','platform':'Apple II','renderer':'Double high-resolution',
                            'notes':'Base game v119 only. Lord of Storms is deferred.','source_engine':'nox','source_game_id':'nox','source_variant':'v119',
                            'source_platform':'apple2','source_language':'en','enabled':True}}
        request_file.write_text(json.dumps(request,ensure_ascii=False),encoding='utf-8')
    command=['ssh','-i',str(identity),'-o','IdentitiesOnly=yes','-o','BatchMode=yes','ceachievements@hermesbox',
             'cd /home/ceachievements/ce-achievements && php -r '+shlex.quote(PHP)]
    result=subprocess.run(command,input=json.dumps(request).encode(),capture_output=True,timeout=30)
    (private/'provision.stdout').write_bytes(result.stdout)
    (private/'provision.stderr').write_bytes(result.stderr)
    if result.returncode:
        raise RuntimeError('Hermesbox provisioning failed; private diagnostics retained; reuse --private to recover')
    summary=json.loads(result.stdout)
    assert summary['host']=='hermesbox' and summary['non_admin'] and summary['achievements']==0
    config=private/'ce-private.ini'
    if not config.exists():
        config.write_text('[scummvm]\nconfirm_exit=false\ngui_return_to_launcher=false\nupdates_check=0\n'
                          'ce_achievements_api_base_url=http://hermesbox:8123/api/v1\nce_link_token='+request['token']+
                          '\n\n[nox-gameplay-test]\nengineid=nox\ngameid=nox\nplatform=apple2\npath='+str(game)+
                          '\nnox_gameplay_validation=true\nnox_online_validation=true\n',encoding='utf-8')
    # Paths reveal no secret values. Keep account credentials/config outside Git/reports.
    print(json.dumps({'status':'VERIFIED','scope':'dedicated test account and approved zero-award registration only',
                      'registration':summary,'private_configuration':str(config)},indent=2))


if __name__=='__main__':
    main()
