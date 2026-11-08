#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Render bilingual Gooka resources and restore the same original save both ways."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
from test_cli_client import Client
from test_gooka_runtime import place

parser = argparse.ArgumentParser(description=__doc__)
for name in ('exe','baseline','extra','game','save','artifacts'):
    parser.add_argument('--'+name,type=Path,required=True)
args = parser.parse_args()
root = args.artifacts.resolve()
root.mkdir(parents=True,exist_ok=False)
game = args.game.resolve()
digest = lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
raw = ('raw:wait:300;clear;text:450:403:20;text:451:403:45;text:452:403:70;'
       'text:1:403:110;text:63:403:135;text:3561:403:170;shot:screenshots/labels.png;'
       'clear;text:1644:403:20;text:2440:403:130;shot:screenshots/puzzle.png;'
       'clear;text:3204:403:1;shot:screenshots/book.png;quit')


def run(exe,directory,lang,script,skip=False):
    directory.mkdir(parents=True,exist_ok=True)
    (directory/'ce-test-cli.marker').write_text('ce-test-cli/1\n')
    (directory/'profile.ini').write_text(
        '[scummvm]\nenable_unsupported_game_warning=false\nconsole=false\nconfirm_exit=false\n'
        'gui_return_to_launcher_at_exit=false\nautosave_period=0\nce_achievements_popups_enabled=false\n'
        '[cli-game]\nengineid=gooka\ngameid=gooka\nplatform=windows\n'
        f'path={game}\nlanguage={lang}\nsubtitles=true\ncheck_gamedata=false\n',encoding='utf-8')
    previous = os.environ.get('GOOKA_TEST')
    os.environ['GOOKA_TEST'] = script
    client = None
    try:
        client = Client(exe.resolve(),directory,args.extra.resolve())
        state = client.launch(game,'windows',game='gooka',engine='gooka')
        assert state['status']=='running'
        if skip:
            client.command('input',kind='key',key=27,ascii=27,phase='down')
        client.process.wait(timeout=60)
        assert client.process.returncode==0
        client.close()
    except BaseException:
        if client: client.abort()
        raise
    finally:
        if previous is None: os.environ.pop('GOOKA_TEST',None)
        else: os.environ['GOOKA_TEST']=previous


for name,exe,lang in [('czech',args.exe,'cz'),('english',args.exe,'en'),('baseline',args.baseline,'cz')]:
    run(exe,root/name,lang,raw)
for image in ('labels','puzzle','book'):
    cz = root/f'czech/screenshots/{image}.png'
    en = root/f'english/screenshots/{image}.png'
    old = root/f'baseline/screenshots/{image}.png'
    assert digest(cz)==digest(old),(image,'Czech presentation changed')
    assert digest(cz)!=digest(en),(image,'English not selected')

for name,lang in [('czech','cz'),('english','en')]:
    run(args.exe,root/('menu-'+name),lang,
        'newgame;esc;ticks:200;click:320:330;ticks:300;shot:screenshots/dialogue.png;esc;ticks:200;'
        'click:628:412;ticks:2;shot:screenshots/menu.png;quit',skip=True)
assert digest(root/'menu-czech/screenshots/menu.png') != digest(root/'menu-english/screenshots/menu.png')
assert digest(root/'menu-czech/screenshots/dialogue.png') != digest(root/'menu-english/screenshots/dialogue.png')
for name in ('czech','english'):
    profile = (root/('menu-'+name)/'profile.ini').read_text(encoding='utf-8')
    assert 'lang_Czech' in profile and 'lang_English' in profile

shared = root/'shared-save'
(shared/'saves').mkdir(parents=True)
shutil.copyfile(args.save,shared/'saves/cli-game.002')
before = digest(shared/'saves/cli-game.002')
for lang,slot in [('en',3),('cz',4)]:
    run(args.exe,shared,lang,f'load:2;shot:screenshots/restored-{lang}.png;save:{slot};quit',skip=True)
    assert place(shared/f'saves/cli-game.{slot:03d}')==place(args.save)
    assert digest(shared/'saves/cli-game.002')==before
    log = (shared/'engine.log').read_text(encoding='utf-8',errors='replace')
    assert 'load slot 2: No error' in log and f'save slot {slot}: No error' in log
report = dict(status='PASS',exe_sha256=digest(args.exe),original_czech_pixel_identical=True,
              english_rendered=True,actual_game_menu_rendered=True,
              actual_game_dialogue_rendered=True,
              normal_language_selector_options=['Czech','English'],
              resource_screens=['menu/inventory labels','book puzzle clues','encyclopaedia'],
              original_save_compatible_in=['en','cz'],save_directory_shared=True,
              original_save_unchanged=True,
              scope='Real game resource viewer rendering and native save APIs; GUI Language list follows standard ADGF_ADDENGLISH, not automated widget navigation.')
(root/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
