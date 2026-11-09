#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused opening-voice subtitle timing, toggle and skip regression."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import zlib
from test_cli_client import Client
from test_gooka_runtime import place


def white_caption_pixels(path):
    data = path.read_bytes()
    width,height,depth,kind,_,_,interlace = struct.unpack('>IIBBBBB',data[16:29])
    assert (width,height,depth,kind,interlace)==(640,480,8,2,0)
    compressed = bytearray()
    pos = 8
    while pos < len(data):
        size = struct.unpack_from('>I',data,pos)[0]
        if data[pos+4:pos+8] == b'IDAT': compressed.extend(data[pos+8:pos+8+size])
        pos += size+12
    raw = zlib.decompress(compressed)
    stride = width*3
    previous = bytearray(stride)
    count = 0
    for y in range(52):
        start = y*(stride+1)
        filt = raw[start]
        row = bytearray(raw[start+1:start+1+stride])
        for x in range(stride):
            a = row[x-3] if x >= 3 else 0
            b = previous[x]
            c = previous[x-3] if x >= 3 else 0
            if filt == 1: predictor = a
            elif filt == 2: predictor = b
            elif filt == 3: predictor = (a+b)//2
            elif filt == 4:
                p = a+b-c
                pa,pb,pc = abs(p-a),abs(p-b),abs(p-c)
                predictor = a if pa<=pb and pa<=pc else b if pb<=pc else c
            else:
                assert filt == 0
                predictor = 0
            row[x] = (row[x]+predictor)&255
        if y >= 16:
            count += sum(row[x]>=248 and row[x+1]>=248 and row[x+2]>=248 for x in range(0,stride,3))
        previous = row
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('exe','extra','game','artifacts'):
        parser.add_argument('--'+name,type=Path,required=True)
    args = parser.parse_args()
    root = args.artifacts.resolve()
    root.mkdir(parents=True,exist_ok=False)
    results = []
    for lang in ('en','cz'):
        for mode in ('enabled','disabled','skip'):
            directory = root/(lang+'-'+mode)
            directory.mkdir()
            (directory/'ce-test-cli.marker').write_text('ce-test-cli/1\n')
            subtitles = mode!='disabled'
            (directory/'profile.ini').write_text(
                '[scummvm]\nenable_unsupported_game_warning=false\nconsole=false\n'
                'confirm_exit=false\ngui_return_to_launcher_at_exit=false\nautosave_period=0\n'
                'ce_achievements_popups_enabled=false\n[cli-game]\nengineid=gooka\ngameid=gooka\n'
                f'platform=windows\npath={args.game.resolve()}\nlanguage={lang}\n'
                f'subtitles={str(subtitles).lower()}\ncheck_gamedata=false\n',encoding='utf-8')
            script = 'newgame;wait:500;shot:screenshots/during.png;'
            if mode=='skip': script += 'esc;ticks:200;'
            else: script += 'wait:7300;'
            script += 'shot:screenshots/after.png;'
            if mode!='skip': script += 'wait:3500;ticks:200;'
            script += 'save:1;quit'
            previous = os.environ.get('GOOKA_TEST')
            os.environ['GOOKA_TEST'] = script
            client = None
            try:
                client = Client(args.exe.resolve(),directory,args.extra.resolve())
                client.launch(args.game.resolve(),'windows',game='gooka',engine='gooka')
                client.command('input',kind='key',key=27,ascii=27,phase='down')
                client.process.wait(timeout=40)
                assert client.process.returncode==0
                client.close()
            except BaseException:
                if client: client.abort()
                raise
            finally:
                if previous is None: os.environ.pop('GOOKA_TEST',None)
                else: os.environ['GOOKA_TEST']=previous
            during = white_caption_pixels(directory/'screenshots/during.png')
            after = white_caption_pixels(directory/'screenshots/after.png')
            assert (during>3000)==subtitles,(lang,mode,during)
            assert after<3000,(lang,mode,after,'caption persisted')
            state = place(directory/'saves/cli-game.001')
            assert state['room']==800,(lang,mode,state)
            results.append(dict(language=lang,mode=mode,during_caption_pixels=during,
                                after_caption_pixels=after,final_room=state['room']))
    report = dict(status='PASS',executable_sha256=hashlib.sha256(args.exe.read_bytes()).hexdigest(),
                  voice_3585_seconds=7.523265306122449,cases=results,
                  scope='Real audio completion/skip and captured caption band; original reply and exit sequence retained.')
    (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__': main()
