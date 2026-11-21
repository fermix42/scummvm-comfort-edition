#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Private v119 parity check. Usage: script CHECKOUT EXE GAME ARTIFACTS.

Requires the retained private fixtures described in README-parity.md.
"""
from pathlib import Path
import configparser,json,os,shutil,struct,subprocess,sys
ce,exe,game,root=map(lambda arg:Path(arg).resolve(),sys.argv[1:])
sys.path.insert(0,str(ce/'devtools/nox'))
from test_resources import run_validation_process
base=game/'conversion/combat-contract-research'
sources={
 'battle_reticle':base/'reticle-reference/input.bin',
 'battle_terrain':base/'tactical-terrain-reference/input.bin',
 'battle_action':game/'conversion/ce-native-acceptance528/combat531/original-oracle/input.bin',
 'battle_spell_damage':base/'spell-damage-reference/input.bin',
 'battle_area_spell':base/'area-spell-reference/input.bin',
 'battle_restoration':base/'restoration-reference/input.bin',
 'quick_battle':base/'quick-battle-reference9/input.bin',
 'battle_world_return':base/'battle-world-return-reference/input.bin',
}
root.mkdir()
stage=root/'scummvm.exe'
shutil.copy2(exe,stage)
for dll in exe.parent.glob('*.dll'):shutil.copy2(dll,root/dll.name)
work=root/'run';work.mkdir()
saves=work/'saves';saves.mkdir()
keys=work/'keys.bin'
keys.write_bytes(b''.join(struct.pack('<HH',*k)for k in [(ord(c.lower()),ord(c))for c in 'TRACE']+[(13,0)]*8))
cfg=configparser.ConfigParser()
cfg['scummvm']={'gui_return_to_launcher':'false','updates_check':'0','enable_unsupported_game_warning':'false'}
target=cfg['nox-native-startup']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'check_gamedata':'false','ce_playthrough_tracking':'false','nox_native_startup':'true','nox_native_startup_keys':str(keys)}
for label,path in sources.items():
 assert path.is_file(),path
 cfg['nox-native-startup']['nox_native_'+label+'_oracle']=str(path)
ini=work/'test.ini'
with ini.open('w')as stream:cfg.write(stream)
env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy')
hidden=subprocess.STARTUPINFO();hidden.dwFlags|=subprocess.STARTF_USESHOWWINDOW;hidden.wShowWindow=0
argv=[str(stage),'--config='+str(ini),'--savepath='+str(saves),'--logfile='+str(work/'engine.log'),'--no-console','nox-native-startup']
res=run_validation_process(argv,work,env,hidden,timeout=40)
reports=list(saves.glob('*.tsv'))
for report in reports:
 if 'oracle' in report.name:print(report.read_text().strip())
assert len([report for report in reports if 'oracle' in report.name])==len(sources),reports
