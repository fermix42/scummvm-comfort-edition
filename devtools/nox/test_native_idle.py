#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual native idle frames, aggregate history cold continuation and gameplay invariance."""
import argparse,configparser,hashlib,json,os,shutil,struct,subprocess
from pathlib import Path
from test_resources import run_validation_process
from test_native_world_input import sections

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['exe','game','artifacts']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=a.artifacts.resolve();root.mkdir(parents=True,exist_ok=False)
    base=[(ord(c.lower()),ord(c))for c in 'TRACE']+[(13,0)]*8;east=(275,0);open_key=(111,79);up=(273,0)
    def run(label,keys,frames,checkpoint=None,capture=False,paced=False):
        folder=root/label;folder.mkdir();saves=folder/'saves';saves.mkdir();keyfile=folder/'keys.bin';keyfile.write_bytes(b''.join(struct.pack('<HH',*key)for key in keys))
        settings={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(a.game.resolve()),'nox_native_startup':'true','nox_native_startup_keys':str(keyfile),'nox_native_idle_frames':str(frames)}
        if capture:settings['nox_native_startup_capture']='true'
        if paced:settings['nox_native_idle_paced']='true'
        if checkpoint:shutil.copy2(checkpoint,saves/'nox-native-exploration.bin');settings['nox_native_startup_resume']='true'
        config=configparser.ConfigParser();config['scummvm']={'gui_return_to_launcher':'false','updates_check':'0'};config['nox-native-startup']=settings
        with (folder/'test.ini').open('w')as f:config.write(f)
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        argv=[str(a.exe.resolve()),'--config='+str(folder/'test.ini'),'--savepath='+str(saves),'--logfile='+str(folder/'engine.log'),'--no-console','nox-native-startup']
        process=run_validation_process(argv,folder,dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startup)
        (folder/'stdout.txt').write_bytes(process.stdout);(folder/'stderr.txt').write_bytes(process.stderr)
        fields=dict(part.split('=',1)for part in (saves/'nox-native-startup-report.txt').read_text().strip().split('\t')[1:]);assert fields['status']=='VERIFIED' and fields['complete_native_gameplay']=='0' and fields['compatibility_runtime']=='0'
        report=dict(part.split('=',1)for part in (saves/'nox-native-startup-animation-report.tsv').read_text().strip().split('\t')[1:])
        assert int(report['idle_frames'])==frames and report['cold_history_restored']==str(int(checkpoint is not None))
        trace=[list(map(int,line.split('\t')))for line in (saves/'nox-native-startup-animation.tsv').read_text().splitlines()];assert len(trace)==frames
        packet=saves/'nox-native-exploration-final.bin';parts=sections(packet.read_bytes());assert parts['ANIM'][:6]==b'NWAM\x01\x00' and len(parts['ANIM'])==16398
        phase,pattern=parts['ANIM'][6:8];target=parts['ANIM'][13]
        assert int(report['phase'])==phase and int(report['pattern'])==pattern and int(report['next_target'])==target
        assert parts['FRAM']==parts['ANIM'][14+8192*(target^1):14+8192*((target^1)+1)]
        for page in range(2):
            image=parts['ANIM'][14+page*8192:14+(page+1)*8192]
            for block in range(64):assert image[block*128+120:block*128+128]==bytes(8)
        return folder,packet,parts,trace,fields
    results=[]
    for label,keys in [('notice',base+[east]),('opened',base+[east,open_key,up])]:
        baseline,_,zero,_,_=run(label+'-zero',keys,0)
        full,full_packet,parts,trace,fields=run(label+'-sixteen',keys,16,capture=label=='opened')
        warm,warm_packet,_,warm_trace,_=run(label+'-eight',keys,8)
        cold,cold_packet,cold_parts,cold_trace,_=run(label+'-cold-eight',[],8,warm_packet)
        assert full_packet.read_bytes()==cold_packet.read_bytes(),('Cold idle history differs',label)
        assert parts['FRAM']!=zero['FRAM'],'Idle execution did not change the displayed world'
        for tag in parts:
            if tag not in ['FRAM','ANIM']:assert parts[tag]==zero[tag],('Idle mutated non-animation state',label,tag)
        for i,row in enumerate(cold_trace):assert row==[i+1]+trace[i+8][1:]
        fast,fast_packet,_,_,_=run(label+'-fast-four',[],4,warm_packet)
        paced,paced_packet,_,_,_=run(label+'-paced-four',[],4,warm_packet,paced=True)
        assert fast_packet.read_bytes()==paced_packet.read_bytes(),('Paced and accelerated results differ',label)
        timing=[list(map(int,line.split('\t')))for line in (paced/'saves/nox-native-startup-animation-timing.tsv').read_text().splitlines()]
        assert len(timing)==4 and all(row[0]==i+1 and row[1]>=33 for i,row in enumerate(timing)),timing
        assert fields['notice_pending']==str(int(label=='notice')) and fields['position']=='25/38'
        results.append(dict(case=label,checkpoint_sha256=sha(warm_packet),final_sha256=sha(full_packet),packet_bytes=full_packet.stat().st_size,frames=16,non_animation_sections_unchanged=True,cold_identical=True,paced_four_equals_accelerated=True,paced_intervals_ms=[row[1]for row in timing]))
    # Continue the naturally retained pending notice after eight actual idle
    # frames; dismiss with O, select Up, then run32 actual Wait operations.
    continuation,_,continued,_,_=run('idle-notice-open-waits',[open_key,up]+[(32,32)]*32,4,root/'notice-eight/saves/nox-native-exploration-final.bin')
    direct,_,direct_parts,_,_=run('direct-open-waits',base+[east,open_key,up]+[(32,32)]*32,0)
    for tag in ['CORE','WIND','TURN','ROUT','RAND','SCEN','INPT']:assert continued[tag]==direct_parts[tag],('Idle disrupted subsequent gameplay',tag)
    receipt=dict(status='VERIFIED',scope='Actual native engine idle frames at pending notice and Open result; aggregate8+8 cold equals16; actual33ms paced scheduler equals accelerated; gameplay invariance and subsequent Open/Wait retained',executable_sha256=sha(a.exe),input_only=True,controlled_setup=['Existing initial story/light/entropy setup','Native presentation phases/pattern initialized to zero before gameplay; deterministic frame budget is execution input, never an expected outcome'],cases=results,limitations=['Default33ms scheduler path exercised with real deadlines; original speed-profile/pacing equivalence remains unverified.','Command repaint synchronizes both native images from the actual native result; source-specific scroll/frame transition history outside idle remains separate.','Interior Keep/default appearance only; complete native campaign/ending/audio/session/legacy and M4.2/.3 acceptance remain required.'],artifacts={str(f.relative_to(root)):sha(f)for f in root.rglob('*')if f.is_file()})
    (root/'verification.json').write_text(json.dumps(receipt,indent=2)+'\n');print('VERIFIED native idle/aggregate cold and gameplay invariance',root)
if __name__=='__main__':main()
