#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Private v119 parity check. Usage: script CHECKOUT EXE GAME ARTIFACTS.

Requires the retained private fixtures described in README-parity.md.
"""
from pathlib import Path
import configparser,shutil,sys
ce,exe,game,root=map(lambda arg:Path(arg).resolve(),sys.argv[1:])
sys.path[:0]=[str(ce/'devtools/nox'),str(ce/'devtools/ce')]
from test_cli_client import Client
from test_encounter_restart import inspect,press,select_save
from test_exploration_map import shot
source=game/'conversion/ce-native-acceptance528/earned-modern'
saved=next((source/'forks/Ending1082').glob('*slot619.g0'))
c=Client(exe,root,exe.parent)
try:
 shutil.copy2(saved,root/'saves'/saved.name)
 sidecar=Path(str(saved)+'.ce-achievements.json')
 shutil.copy2(sidecar,root/'saves'/sidecar.name)
 shutil.copy2(source/'saves/ce-playthroughs.json',root/'saves')
 cfg=configparser.ConfigParser()
 cfg['scummvm']={'enable_unsupported_game_warning':'false','ce_achievements_popups_enabled':'false','disable_saved_screenshot_osd':'true'}
 cfg['cli-game']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'check_gamedata':'false','ce_playthrough_tracking':'true'}
 with (root/'profile.ini').open('w') as stream:cfg.write(stream)
 c.launch(game,'apple2',game='nox',engine='nox')
 c.command('wait',field='adapter.stage',equals='title',timeout_ms=10000)
 c.command('load',chooser_slot=select_save(c,619)['chooser_slot'])
 seen=c.command('wait',field='adapter.battle_results',equals=4,timeout_ms=7500)['adapter']
 shot(c,root)
 press(c,32,32)
 after=inspect(c)['adapter']
 for k in ('battle_results','quest_fields','party','inventory'):
  assert after[k]==seen[k],k
 print('PASS authentic earned ending cold load, original ending screen and permanent terminal state after Space')
finally:c.close()
