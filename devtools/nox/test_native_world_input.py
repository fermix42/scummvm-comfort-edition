#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real native arrow/notice/forwarded Open input and pending-phase cold continuation."""
import argparse,configparser,hashlib,json,os,shutil,struct,subprocess
from pathlib import Path
from test_resources import run_validation_process

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def current_world_tables(world):
    """Select the active registry explicitly; new games also retain Surface."""
    assert world[:6]==b'NWLD\x01\x00'
    count=struct.unpack_from('<H',world,267)[0]
    assert 0<count<=76 and len(world)==269+1794*count
    family={0:0,2:1,4:1,3:2,6:3,5:4,7:5,8:6}[world[6]]
    records={}
    for at in range(269,len(world),1794):
        identity=tuple(world[at:at+2]);assert identity not in records
        records[identity]=world[at+2:at+1794]
    return records[(family,world[7])]
def sections(packet):
    assert packet[:8] in [b'NPXS\x03\x00\x77\x00',b'NPXS\x04\x00\x77\x00'] and struct.unpack_from('<I',packet,8)[0]==len(packet)-28
    assert hashlib.md5(packet[28:]).digest()==packet[12:28]
    result={};at=28
    while at<len(packet):
        tag=packet[at:at+4].decode();size=struct.unpack_from('<I',packet,at+4)[0];assert tag not in result
        result[tag]=packet[at+8:at+8+size];assert len(result[tag])==size;at+=8+size
    assert at==len(packet);return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['exe','game','artifacts','reference']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--wait-reference',type=Path,required=True)
    p.add_argument('--status-reference',type=Path)
    p.add_argument('--animation-reference',type=Path)
    a=p.parse_args()
    for label in ['first-east','open-return']:assert (a.reference/('nox-original-world-'+label+'.bin')).is_file(),a.reference
    assert (a.wait_reference/'result.bin').is_file() and (a.wait_reference/'metadata.json').is_file()
    root=a.artifacts.resolve();root.mkdir(parents=True,exist_ok=False)
    base=[(ord(c.lower()),ord(c))for c in 'TRACE']+[(13,0)]*8
    east=(275,0);open_key=(111,ord('O'));up=(273,0);space=(32,32)
    tail=[open_key,up,open_key,up]+[space]*32
    def run(label,keys,checkpoint=None):
        folder=root/label;folder.mkdir();saves=folder/'saves';saves.mkdir();keyfile=folder/'keys.bin'
        keyfile.write_bytes(b''.join(struct.pack('<HH',*key)for key in keys))
        config=configparser.ConfigParser();config['scummvm']={'gui_return_to_launcher':'false','updates_check':'0'}
        settings={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(a.game.resolve()),'nox_native_startup':'true','nox_native_startup_keys':str(keyfile)}
        if a.status_reference:settings['nox_native_status_oracle']=str((a.status_reference/'input.bin').resolve())
        if a.animation_reference:settings['nox_native_animation_oracle']=str((a.animation_reference/'input.bin').resolve())
        if checkpoint:
            shutil.copy2(checkpoint,saves/'nox-native-exploration.bin');settings['nox_native_startup_resume']='true'
            if a.animation_reference:
                settings['nox_native_animation_cold']='true'
                for name in ['nox-native-animation-world.bin','nox-native-animation-state.bin']:shutil.copy2(checkpoint.parent/name,saves/name)
        config['nox-native-startup']=settings
        with (folder/'test.ini').open('w')as f:config.write(f)
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        argv=[str(a.exe.resolve()),'--config='+str(folder/'test.ini'),'--savepath='+str(saves),'--logfile='+str(folder/'engine.log'),'--no-console','nox-native-startup']
        process=run_validation_process(argv,folder,dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startup)
        (folder/'stdout.txt').write_bytes(process.stdout);(folder/'stderr.txt').write_bytes(process.stderr)
        report=dict(part.split('=',1)for part in (saves/'nox-native-startup-report.txt').read_text().strip().split('\t')[1:])
        assert report['status']=='VERIFIED' and report['compatibility_runtime']=='0' and report['complete_native_gameplay']=='0'
        assert report['exploration_restored']==str(int(checkpoint is not None))
        if a.status_reference:
            status=(saves/'nox-native-status-oracle.tsv').read_text();assert 'cases=1452' in status and 'visible_bytes=464640' in status and 'cpu_execution=0' in status
        if a.animation_reference:
            animation=(saves/'nox-native-animation-oracle.tsv').read_text()
            assert 'world_unchanged=1' in animation and 'cpu_execution=0' in animation
            assert ('operations=56' in animation and 'actual_phase_pages_restored=1' in animation and 'continued_first_case=24' in animation) if checkpoint else 'operations=64' in animation
        final=saves/'nox-native-exploration-final.bin';return folder,report,final,sections(final.read_bytes())
    notice,notice_report,notice_packet,notice_parts=run('first-east',base+[east])
    assert notice_report['position']=='25/38' and notice_report['notice_pending']=='1' and notice_report['open_direction_pending']=='0'
    def compare_original(folder,parts,label):
        original=(a.reference/('nox-original-world-'+label+'.bin')).read_bytes();assert len(original)==131072
        world=(folder/'saves/nox-native-startup-world.bin').read_bytes()
        assert world[6:11]==bytes([8,7,25,38,1])
        for i in range(256):assert world[11+i]==original[0x400+i],('Quest differs',label,i,world[11+i],original[0x400+i])
        expected=original[0xba00:0xc000]+original[0x6ab0:0x6bb0]
        for i in range(1792):assert world[271+i]==expected[i],('Map differs',label,i,world[271+i],expected[i])
        clock=bytes(original[i]for i in [0x2bf8,0x3878,0x3879,0x3ffc,0x367f,0x414])
        assert (folder/'saves/nox-native-startup-clock.bin').read_bytes()[6:]==clock
        assert parts['TURN'][-3]==original[0x257e],('Facing differs',label,parts['TURN'][-3],original[0x257e])
        assert parts['CORE'][-5:]==original[0x7059:0x705b]+original[0x7057:0x7059]+original[0x367d:0x367e],('Owned currency/food/appearance differs',label)
        return original
    original=compare_original(notice,notice_parts,'first-east')
    frame=notice_parts['FRAM'];indices=[(y&7)*1024+((y>>3)&7)*128+(y>>6)*40+x for y in range(8,104)for x in range(25,38)]
    diffs=[sum(frame[i]!=original[page+i]for i in indices)for page in [0x2000,0x4000]]
    (root/'notice-frame-comparison.json').write_text(json.dumps(dict(region='13 columns x12 text rows',page_differences=diffs))+'\n')
    assert min(diffs)==0,('Authored notice presentation differs',diffs)
    opened,opened_report,opened_packet,opened_parts=run('opened',base+[east,open_key,up])
    assert opened_report['notice_pending']=='0' and opened_report['open_direction_pending']=='0'
    compare_original(opened,opened_parts,'open-return')
    full,full_report,full_packet,full_parts=run('uninterrupted',base+[east]+tail)
    original_wait=(a.wait_reference/'result.bin').read_bytes();wait_metadata=json.loads((a.wait_reference/'metadata.json').read_text())
    final_world=(full/'saves/nox-native-startup-world.bin').read_bytes()
    assert final_world[11:267]==original_wait[0x400:0x500]
    assert current_world_tables(final_world)==original_wait[0xba00:0xc000]+original_wait[0x6ab0:0x6bb0]
    commands=[list(map(int,line.split('\t')))for line in (full/'saves/nox-native-startup-commands.tsv').read_text().splitlines()]
    assert len(commands)==37 and commands[2][2:10]==commands[4][2:10] and commands[2][-1]==commands[4][-1]==0 and commands[3][-1]==2
    waits=[list(map(int,line.split('\t')))for line in (full/'saves/nox-native-startup-wait.tsv').read_text().splitlines()]
    assert len(waits)==len(wait_metadata['turns'])==32
    for i,row in enumerate(waits):assert row==[i+1]+wait_metadata['turns'][i]['clock']
    cold,cold_report,cold_packet,cold_parts=run('cold-notice',tail,notice_packet)
    assert cold_packet.read_bytes()==full_packet.read_bytes(),'Cold notice continuation differs from uninterrupted actual keys'
    pending,pending_report,pending_packet,pending_parts=run('open-direction',base+[east,open_key])
    assert pending_report['notice_pending']=='0' and pending_report['open_direction_pending']=='1'
    cold_direction,_,direction_packet,_=run('cold-open-direction',tail[1:],pending_packet)
    assert direction_packet.read_bytes()==full_packet.read_bytes(),'Cold Open direction continuation differs'
    receipt=dict(status='VERIFIED',scope='Actual native keys through first east/authored notice/forwarded O/direction Open/repeat refusal/32 Wait and two pending-phase cold continuations; incomplete native game',executable_sha256=sha(a.exe),input_only=True,controlled_setup=['Existing story/light1/entropy119 initialization retained'],source_reference={str(f):sha(f)for f in a.reference.glob('nox-original-world-*.bin')},original_state_bytes_checked=2*(2048+6+1),notice_visible_bytes_checked=len(indices),final_checkpoint_sha256=sha(full_packet),packet_bytes=full_packet.stat().st_size,results=dict(notice=notice_report,opened=opened_report,full=full_report,cold=cold_report,pending=pending_report),limitations=['Initial interior Keep domain only; unsupported encounter/interaction/terrain boundaries explicit.','Party/HUD/audio/session integration and complete save/native campaign acceptance remain required.'],artifacts={str(f.relative_to(root)):sha(f)for f in root.rglob('*')if f.is_file()})
    (root/'verification.json').write_text(json.dumps(receipt,indent=2)+'\n');print('VERIFIED native world input and cold continuation',root)
if __name__=='__main__':main()
