#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Revoke only the dedicated authorized Nox hermesbox test credential."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
PHP=r'''
require 'vendor/autoload.php';$app=require 'bootstrap/app.php';
$app->make(Illuminate\Contracts\Console\Kernel::class)->bootstrap();
if(gethostname()!=='hermesbox'||$app->environment()!=='local'||rtrim(config('app.url'),'/')!=='http://hermesbox:8123')throw new RuntimeException('Wrong environment');
$r=json_decode(file_get_contents('php://stdin'),true,512,JSON_THROW_ON_ERROR);
if(!preg_match('/^nox-m1-[a-f0-9]{16}@hermesbox\.test$/',$r['email']))throw new RuntimeException('Wrong account');
$user=App\Models\User::query()->where('email',$r['email'])->firstOrFail();
if($user->is_admin||$user->name!=='Dedicated Nox M1 test')throw new RuntimeException('Wrong owner');
$credential=App\Models\GameCredential::query()->where('user_id',$user->id)->where('name',$r['credential_name'])->where('token_hash',hash('sha256',$r['token']))->firstOrFail();
$credential->forceFill(['revoked_at'=>$credential->revoked_at??now()])->save();
echo json_encode(['host'=>gethostname(),'non_admin'=>true,'dedicated_credential_revoked'=>$credential->revoked_at!==null]);
'''

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--identity',type=Path,required=True);ap.add_argument('--private',type=Path,required=True);ap.add_argument('--report',type=Path,required=True);args=ap.parse_args()
    private=args.private.resolve(strict=True);request=json.loads((private/'request.json').read_text())
    payload={key:request[key] for key in ('email','credential_name','token')}
    command=['ssh','-i',str(args.identity.resolve(strict=True)),'-o','IdentitiesOnly=yes','-o','BatchMode=yes','ceachievements@hermesbox','cd /home/ceachievements/ce-achievements && php -r '+shlex.quote(PHP)]
    result=subprocess.run(command,input=json.dumps(payload).encode(),capture_output=True,timeout=30)
    (private/'revoke.stdout').write_bytes(result.stdout);(private/'revoke.stderr').write_bytes(result.stderr)
    if result.returncode: raise RuntimeError('Dedicated credential revocation failed; private diagnostics retained')
    report=json.loads(result.stdout);assert report['host']=='hermesbox' and report['non_admin'] and report['dedicated_credential_revoked']
    report.update({'status':'VERIFIED','scope':'Only exact dedicated test credential revoked; account and test history retained'})
    args.report.resolve().write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

if __name__=='__main__': main()
