#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare32 real native Wait inputs with16 inputs, cold restore,16 inputs."""
import argparse,configparser,hashlib,json,os,shutil,struct,subprocess,sys
from pathlib import Path
from test_resources import run_validation_process

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['exe','game','artifacts','creation-oracle','opening-oracle','wait-oracle']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--name',default='TRACE');p.add_argument('--gender',type=int,default=0,choices=range(3));p.add_argument('--race',type=int,default=0,choices=range(5))
    a=p.parse_args();root=a.artifacts.resolve();root.mkdir(parents=True,exist_ok=False)
    command=[sys.executable,str(Path(__file__).with_name('test_native_startup.py')),'--exe',str(a.exe.resolve()),'--game',str(a.game.resolve()),'--artifacts',str(root/'uninterrupted'),
        '--creation-oracle',str(a.creation_oracle.resolve()),'--opening-oracle',str(a.opening_oracle.resolve()),'--wait-oracle',str(a.wait_oracle.resolve()),'--name',a.name,'--gender',str(a.gender),'--race',str(a.race)]
    full=subprocess.run(command,capture_output=True,timeout=105);(root/'uninterrupted.stdout.txt').write_bytes(full.stdout);(root/'uninterrupted.stderr.txt').write_bytes(full.stderr)
    assert full.returncode==0,full.stderr.decode(errors='replace')
    original=json.loads((root/'uninterrupted/verification.json').read_text());assert original['status']=='VERIFIED' and original['executable_sha256']==sha(a.exe)
    keys=(root/'uninterrupted/keys.bin').read_bytes();spaces=struct.pack('<HH',32,32)*16;assert keys.endswith(spaces*2)
    def run(label,key_bytes,checkpoint=None,resume=False,corrupt=False):
        folder=root/label;folder.mkdir();saves=folder/'saves';saves.mkdir();(folder/'keys.bin').write_bytes(key_bytes)
        if checkpoint:
            data=checkpoint.read_bytes()
            if corrupt:data=data[:-1]+bytes([data[-1]^1])
            (saves/'nox-native-exploration.bin').write_bytes(data)
        config=configparser.ConfigParser();config['scummvm']={'gui_return_to_launcher':'false','updates_check':'0'}
        settings={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(a.game.resolve()),'nox_native_startup':'true','nox_native_startup_keys':str(folder/'keys.bin')}
        if resume:settings['nox_native_startup_resume']='true'
        else:settings['nox_native_checkpoint_wait']='16'
        config['nox-native-startup']=settings
        with (folder/'test.ini').open('w') as f:config.write(f)
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        argv=[str(a.exe.resolve()),'--config='+str(folder/'test.ini'),'--savepath='+str(saves),'--logfile='+str(folder/'engine.log'),'--no-console','nox-native-startup']
        try:
            process=run_validation_process(argv,folder,dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startup)
            (folder/'stdout.txt').write_bytes(process.stdout);(folder/'stderr.txt').write_bytes(process.stderr)
            assert not corrupt,'Corrupted checkpoint unexpectedly accepted'
        except AssertionError as error:
            if not corrupt:raise
            failure=saves/'nox-native-validation-failure.txt';assert failure.exists() and 'checkpoint rejected' in failure.read_text()
            assert not (saves/'nox-native-exploration-final.bin').exists()
            (folder/'expected-refusal.txt').write_text(str(error));return folder,None
        report=(saves/'nox-native-startup-report.txt').read_text();fields=dict(item.split('=',1)for item in report.strip().split('\t')[1:])
        assert fields['status']=='VERIFIED' and fields['wait_commands']=='16' and fields['exploration_restored']==str(int(resume))
        assert fields['story_performed']==str(int(not resume)) and fields['name']==a.name and fields['compatibility_runtime']=='0'
        assert int(fields['gender'])==a.gender and int(fields['race'])==a.race
        assert fields['complete_native_gameplay']=='0'
        return folder,fields
    warm,warm_report=run('warm16',keys[:-len(spaces)])
    checkpoint=warm/'saves/nox-native-exploration.bin';assert checkpoint.read_bytes()==(warm/'saves/nox-native-exploration-final.bin').read_bytes()
    checkpoint_hash=sha(checkpoint);cold,cold_report=run('cold16',spaces,checkpoint,resume=True)
    assert sha(cold/'saves/nox-native-exploration.bin')==checkpoint_hash
    for name in ['nox-native-startup-party.bin','nox-native-startup-world.bin','nox-native-startup-clock.bin','nox-native-exploration-final.bin']:
        assert (cold/'saves'/name).read_bytes()==(root/'uninterrupted/saves'/name).read_bytes(),('Cold continuation differs',name)
    final=(cold/'saves/nox-native-exploration-final.bin').read_bytes();assert final[:8]==b'NPXS\x04\x00\x77\x00'
    assert struct.unpack_from('<I',final,8)[0]==len(final)-28 and hashlib.md5(final[28:]).digest()==final[12:28]
    all_turns=[list(map(int,line.split('\t')))for line in (root/'uninterrupted/saves/nox-native-startup-wait.tsv').read_text().splitlines()]
    resumed_turns=[list(map(int,line.split('\t')))for line in (cold/'saves/nox-native-startup-wait.tsv').read_text().splitlines()]
    assert len(resumed_turns)==16
    for i,row in enumerate(resumed_turns):assert row==[i+1]+all_turns[i+16][1:]
    refusal,_=run('checksum-refusal',spaces,checkpoint,resume=True,corrupt=True)
    receipt=dict(status='VERIFIED',scope='Native creation/story/16 actual Space waits -> one aggregate checkpoint -> fresh CE restore/16 waits; byte-identical to uninterrupted32-input state',
        executable_sha256=sha(a.exe),source_workflow=str((root/'uninterrupted/verification.json').relative_to(root)),source_workflow_sha256=sha(root/'uninterrupted/verification.json'),
        checkpoint_sha256=checkpoint_hash,final_checkpoint_sha256=hashlib.sha256(final).hexdigest(),packet_bytes=len(final),warm=warm_report,cold=cold_report,checksum_refusal=True,
        limitations=['Current admitted exploration/Wait boundary only; not complete save-game/menu/legacy import acceptance.','Combat/dialogue continuations, sessions/assistance and audio still require native integration.','Explicit initial story/light/entropy setup retained; no earned native campaign claim.'],
        artifacts={str(q.relative_to(root)):sha(q)for q in root.rglob('*')if q.is_file()})
    (root/'verification.json').write_text(json.dumps(receipt,indent=2)+'\n');print('VERIFIED native exploration cold continuation',root,'packet',len(final))
if __name__=='__main__':main()
