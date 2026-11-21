#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Private v119 parity check. Usage: script CHECKOUT EXE GAME ARTIFACTS.

Requires the retained private fixtures described in README-parity.md.
"""
from pathlib import Path
import configparser,os,subprocess,sys
ce,exe,game,root=map(lambda arg:Path(arg).resolve(),sys.argv[1:])
sys.path.insert(0,str(ce/'devtools/nox'))
from test_resources import run_validation_process
assert root.is_relative_to(exe.parent)
root.mkdir()
conversion=game/'conversion'
fixtures={
 'dialogue_voice498':('ce-native-dialogue-voice498-source.bin','dialogue-voice'),
 'dialogue_boundary':('ce-native-dialogue-boundary486-source.bin','dialogue-boundary'),
 'dialogue_control':('ce-native-dialogue478-original.bin','dialogue-control'),
 'world_story525':('ce-native-world-story525.fixture','world-story'),
 'portal':('ce-native-portal491-source.bin','portal'),
 'barrier':('ce-native-barrier488-source.bin','barrier'),
 'pressure_plate':('ce-native-pressure-plate490-source.bin','pressure-plate'),
 'dig':('ce-native-dig484-original.bin','dig'),
 'callbacks':('ce-native-callbacks479-original.bin','callbacks'),
}
failures=[]
for key,(filename,report) in fixtures.items():
 work=root/report
 work.mkdir()
 saves=work/'saves';saves.mkdir()
 keys=work/'keys.bin';keys.write_bytes(b'')
 cfg=configparser.ConfigParser()
 cfg['scummvm']={'gui_return_to_launcher':'false','enable_unsupported_game_warning':'false'}
 cfg['nox-native-startup']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'check_gamedata':'false','ce_playthrough_tracking':'false','nox_native_startup_keys':str(keys),'nox_native_'+key+'_fixture':str(conversion/filename)}
 ini=work/'test.ini'
 with ini.open('w') as stream:cfg.write(stream)
 hidden=subprocess.STARTUPINFO();hidden.dwFlags|=subprocess.STARTF_USESHOWWINDOW;hidden.wShowWindow=0
 env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy')
 argv=[str(exe),'--config='+str(ini),'--savepath='+str(saves),'--logfile='+str(work/'engine.log'),'--no-console','nox-native-startup']
 try:
  run_validation_process(argv,work,env,hidden,timeout=25)
 except AssertionError as e:
  failures.append((key,str(e)));print("FAILED",key,e);continue
 output=saves/('nox-native-'+report+'.tsv')
 assert output.is_file(),output
 print(output.read_text().strip())

assert not failures,failures
