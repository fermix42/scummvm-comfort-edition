#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Play actual Gooka intro/outro with scaling off/on via existing native hooks."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import time
from test_cli_client import Client

parser = argparse.ArgumentParser(description=__doc__)
for name in ('exe','extra','game','artifacts'):
    parser.add_argument('--'+name,type=Path,required=True)
args = parser.parse_args()
root = args.artifacts.resolve()
root.mkdir(parents=True,exist_ok=False)
results = []
for enabled in (False,True):
    for video in (12,13):
        directory = root/f'{video}-{"scaled" if enabled else "original"}'
        directory.mkdir()
        (directory/'ce-test-cli.marker').write_text('ce-test-cli/1\n')
        (directory/'profile.ini').write_text(
            '[scummvm]\nenable_unsupported_game_warning=false\nconsole=false\nconfirm_exit=false\n'
            'gui_return_to_launcher_at_exit=false\n[cli-game]\nengineid=gooka\ngameid=gooka\n'
            f'platform=windows\npath={args.game.resolve()}\nlanguage=en\n'
            f'gooka_scale_videos={str(enabled).lower()}\ncheck_gamedata=false\n',encoding='utf-8')
        previous = os.environ.get('GOOKA_TEST')
        os.environ['GOOKA_TEST'] = f'raw:video:{video};quit'
        client = None
        try:
            client = Client(args.exe.resolve(),directory,args.extra.resolve())
            client.launch(args.game.resolve(),'windows',game='gooka',engine='gooka')
            time.sleep(4 if video==12 else 2)
            client.command('screenshot')
            if video==12:
                client.command('input',kind='key',key=27,ascii=27,phase='down')
            client.process.wait(timeout=20)
            assert client.process.returncode==0
            client.close()
        except BaseException:
            if client: client.abort()
            raise
        finally:
            if previous is None: os.environ.pop('GOOKA_TEST',None)
            else: os.environ['GOOKA_TEST']=previous
        expected = '640x480 at 0,0' if enabled else '400x300 at 120,90'
        log = (directory/'engine.log').read_text(encoding='utf-8',errors='replace')
        assert f'AVI {video}.AVI: 400x300 -> {expected}' in log
        screenshots = list(directory.rglob('*.png'))
        assert len(screenshots)==1,screenshots
        png = screenshots[0].read_bytes()
        assert png[:8]==b'\x89PNG\r\n\x1a\n'
        results.append(dict(video=video,scaling=enabled,rectangle=expected,
                            exit='skipped' if video==12 else 'natural completion',
                            screenshot=str(screenshots[0].relative_to(root)),
                            screenshot_size=struct.unpack('>II',png[16:24])))
report = dict(status='PASS',executable_sha256=hashlib.sha256(args.exe.read_bytes()).hexdigest(),cases=results,
              option='gooka_scale_videos',default=False,
              scope='Real decoded video frames, backend screenshots, intro skip and complete outro playback; no full-game acceptance.')
(root/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
