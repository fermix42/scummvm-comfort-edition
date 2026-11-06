#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Provision an isolated loopback website/account and exercise Nox CE transport.

The database, generated token/config and PHP diagnostics live in a private temp
directory outside the artifact report and both repositories. Never prints tokens.
No existing environment file, account, database or production server is changed.
"""
import argparse
import base64
import configparser
import datetime
import json
import os
from pathlib import Path
import secrets
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request


BOOTSTRAP = r'''<?php
require getenv('NOX_WEBSITE').'/vendor/autoload.php';
$app = require getenv('NOX_WEBSITE').'/bootstrap/app.php';
$app->make(Illuminate\Contracts\Console\Kernel::class)->bootstrap();
if (Illuminate\Support\Facades\DB::connection()->getDriverName() !== 'sqlite' ||
    Illuminate\Support\Facades\DB::connection()->getDatabaseName() !== getenv('DB_DATABASE')) {
    throw new RuntimeException('Refusing non-isolated database');
}
'''
CREATE_ACCOUNT = BOOTSTRAP + r'''
$user = App\Models\User::factory()->create([
    'name' => 'Dedicated Nox M1 local test',
    'email' => 'nox-m1-'.bin2hex(random_bytes(8)).'@example.invalid',
    'password' => bin2hex(random_bytes(32)),
    'is_admin' => false,
]);
if ($user->is_admin) throw new RuntimeException('Test account must not be admin');
$token = bin2hex(random_bytes(32));
App\Models\GameCredential::query()->create([
    'user_id' => $user->id, 'name' => 'Ephemeral Nox M1 loopback token',
    'token_hash' => hash('sha256', $token),
]);
$ini = "[scummvm]\nconfirm_exit=false\ngui_return_to_launcher=false\nupdates_check=0\n".
    'ce_achievements_api_base_url='.getenv('NOX_API_URL')."\nce_link_token=".$token."\n\n".
    "[nox-gameplay-test]\nengineid=nox\ngameid=nox\nplatform=apple2\npath=".
    getenv('NOX_GAME')."\nnox_gameplay_validation=true\nnox_online_validation=true\n";
if (file_put_contents(getenv('NOX_PRIVATE_CONFIG'), $ini) === false) throw new RuntimeException('Cannot write private CE config');
echo json_encode(['non_admin' => !$user->is_admin, 'games' => App\Models\Game::query()->count(),
    'variants' => App\Models\GameVariant::query()->count(), 'achievements' => App\Models\Achievement::query()->count()]);
'''
REVOKE = BOOTSTRAP + r'''
$count=App\Models\GameCredential::query()->whereNull('revoked_at')->update(['revoked_at'=>now()]);
echo json_encode(['revoked' => $count]);
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--php', type=Path, required=True)
    parser.add_argument('--website', type=Path, required=True)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--exe', type=Path)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--prepare-only', action='store_true')
    args = parser.parse_args()
    php, website, game = (p.resolve(strict=True) for p in (args.php, args.website, args.game))
    assert not (website / 'bootstrap/cache/config.php').exists(), 'Cached website config would bypass test environment'
    private = Path(tempfile.mkdtemp(prefix='codex-nox-m1-private-')).resolve()
    assert private.is_relative_to(Path(tempfile.gettempdir()).resolve())
    database, config = private / 'isolated.sqlite', private / 'ce-private.ini'
    database.touch(exist_ok=False)
    root = args.artifacts.resolve() / datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    root.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ, APP_ENV='local', APP_DEBUG='false', APP_KEY='base64:'+base64.b64encode(secrets.token_bytes(32)).decode(),
               DB_CONNECTION='sqlite', DB_DATABASE=str(database), DB_URL='', CACHE_STORE='array', SESSION_DRIVER='array',
               QUEUE_CONNECTION='sync', MAIL_MAILER='array', LOG_CHANNEL='null', NOX_WEBSITE=str(website),
               NOX_GAME=str(game), NOX_PRIVATE_CONFIG=str(config), LARAVEL_PUBLIC_PATH=str(website / 'public'))
    flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0

    def run_php(arguments, label):
        result = subprocess.run([str(php), *arguments], cwd=website, env=env, capture_output=True, timeout=90, creationflags=flags)
        # PHP output is private: bootstrap could contain sensitive exception context.
        (private / (label+'.stdout')).write_bytes(result.stdout)
        (private / (label+'.stderr')).write_bytes(result.stderr)
        if result.returncode:
            raise RuntimeError(f'Isolated PHP {label} failed (exit {result.returncode}); private diagnostics retained')
        return result.stdout

    server = None
    account_created = False
    evidence = {'status': 'IN PROGRESS', 'scope': 'isolated loopback environment/account/link probe only', 'production_touched': False}
    try:
        run_php(['artisan', 'migrate', '--force', '--no-interaction'], 'migrate')
        run_php(['artisan', 'db:seed', '--class=NoxArchaistSeeder', '--force', '--no-interaction'], 'seed')
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        env['NOX_API_URL'] = f'http://127.0.0.1:{port}/api/v1'
        setup = private / 'create-account.php'
        setup.write_text(CREATE_ACCOUNT, encoding='utf-8')
        account_created = True
        account = json.loads(run_php([str(setup)], 'create-account'))
        assert account == {'non_admin': True, 'games': 1, 'variants': 1, 'achievements': 0}
        router = website / 'vendor/laravel/framework/src/Illuminate/Foundation/resources/server.php'
        with (private / 'server.log').open('wb') as output:
            server = subprocess.Popen([str(php), '-S', f'127.0.0.1:{port}', '-t', str(website/'public'), str(router)],
                                      cwd=website/'public', env=env, stdout=output, stderr=subprocess.STDOUT, creationflags=flags)
        for _ in range(100):
            assert server.poll() is None, 'Isolated PHP server exited'
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{port}/games/nox-archaist', timeout=2) as response:
                    if response.status == 200:
                        break
            except OSError:
                time.sleep(0.1)
        else:
            raise RuntimeError('Isolated website did not become ready')
        private_config = configparser.ConfigParser(interpolation=None)
        private_config.read(config, encoding='utf-8')
        request = urllib.request.Request(env['NOX_API_URL']+'/link', headers={'Authorization': 'Bearer '+private_config['scummvm']['ce_link_token'], 'Accept': 'application/json'})
        with urllib.request.urlopen(request, timeout=10) as response:
            link = json.load(response)
            assert response.status == 200 and link['status'] == 'valid'
        evidence.update(account=account, endpoint=env['NOX_API_URL'], http_link_status=200, supported_game_detail_status=200)
        if not args.prepare_only:
            assert args.exe, '--exe required for CE transport'
            result = subprocess.run([sys.executable, str(Path(__file__).with_name('test_gameplay.py')), '--exe', str(args.exe.resolve(strict=True)),
                                     '--game', str(game), '--artifacts', str(root/'ce'), '--private-config', str(config)],
                                    capture_output=True, timeout=240, creationflags=flags)
            (private/'ce-driver.stdout').write_bytes(result.stdout)
            (private/'ce-driver.stderr').write_bytes(result.stderr)
            assert result.returncode == 0, 'CE online driver failed; private diagnostics retained'
            evidence['scope'] = 'CE online Play Sessions and cold gameplay save/load in isolated loopback website'
        evidence['status'] = 'VERIFIED'
    finally:
        if server is not None:
            server.terminate()
            server.wait(timeout=10)
        if account_created:
            revoke = private/'revoke.php'
            revoke.write_text(REVOKE, encoding='utf-8')
            revoked = json.loads(run_php([str(revoke)], 'revoke'))
            evidence['credentials_revoked'] = revoked['revoked'] == 1
        if config.exists():
            config.write_text('[scummvm]\n# Ephemeral test credential revoked.\n', encoding='utf-8')
        (root/'verification.json').write_text(json.dumps(evidence, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(evidence, indent=2))


if __name__ == '__main__':
    main()
