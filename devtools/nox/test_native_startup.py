#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Replay ordinary character-creation keys; no gameplay state is supplied."""
import argparse,configparser,hashlib,json,os,struct,subprocess
from pathlib import Path
from test_resources import run_validation_process

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--exe',type=Path,required=True);p.add_argument('--game',type=Path,required=True)
    p.add_argument('--artifacts',type=Path,required=True);p.add_argument('--gender',type=int,default=0,choices=range(3));p.add_argument('--race',type=int,default=0,choices=range(5))
    p.add_argument('--wait-oracle',type=Path,help='Append32 Space inputs and compare original Wait state/clock')
    p.add_argument('--capture',action='store_true')
    p.add_argument('--creation-oracle',type=Path)
    p.add_argument('--name',default='TRACE')
    p.add_argument('--opening-oracle',type=Path)
    p.add_argument('--story-arrow',action='store_true',help='Separate actual MORE arrow-refusal workflow; no same-sequence full-frame oracle')
    a=p.parse_args();root=a.artifacts.resolve();root.mkdir(parents=True,exist_ok=False);(root/'saves').mkdir()
    keys=[]
    def key(code,ascii=0): keys.append((code,ascii))
    def text(value):
        for c in value:key(ord(c.lower()),ord(c))
    key(13);text('   ');key(13);key(27) # Blank names must not advance.
    text('DRAFT');key(13);key(275);key(13);key(275);key(13);key(27) # Restart at confirmation.
    text('WRONG');
    for _ in range(5):key(8)
    assert 0<len(a.name)<=14 and all(32<=ord(c)<=122 for c in a.name)
    text(a.name+'EXCESS' if len(a.name)==14 else a.name);key(13)
    key(276);key(275) # Bidirectional wrap, then select real requested choice.
    for _ in range(a.gender):key(275)
    key(13);key(276);key(275)
    for _ in range(a.race):key(275)
    key(13);key(13)
    assert not (a.story_arrow and a.opening_oracle), 'Use the original same-input sequence for full-frame comparison'
    if a.story_arrow:key(273)
    for _ in range(4):key(13)
    if a.wait_oracle:
        for _ in range(32):key(32,32)
    (root/'keys.bin').write_bytes(b''.join(struct.pack('<HH',*k) for k in keys))
    config=configparser.ConfigParser();config['scummvm']={'gui_return_to_launcher':'false','updates_check':'0'}
    config['nox-native-startup']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(a.game.resolve()),'nox_native_startup':'true','nox_native_startup_keys':str(root/'keys.bin')}
    if a.capture:config['nox-native-startup']['nox_native_startup_capture']='true'
    ini=root/'test.ini'
    with ini.open('w') as f:config.write(f)
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    command=[str(a.exe.resolve()),'--config='+str(ini),'--savepath='+str(root/'saves'),'--logfile='+str(root/'engine.log'),'--no-console','nox-native-startup']
    result=run_validation_process(command,root,dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startup)
    (root/'stdout.txt').write_bytes(result.stdout);(root/'stderr.txt').write_bytes(result.stderr)
    report=(root/'saves/nox-native-startup-report.txt').read_text()
    fields=dict(part.split('=',1) for part in report.strip().split('\t')[1:])
    assert fields['status']=='VERIFIED' and fields['name']==a.name and int(fields['keys'])==len(keys)
    assert int(fields['gender'])==a.gender and int(fields['race'])==a.race
    assert fields['map']=='8/7' and fields['position']=='24/38' and fields['compatibility_runtime']=='0'
    assert fields['opening_story_text']=='1' and fields['opening_backdrop']=='1' and fields['opening_story']=='1'
    party=(root/'saves/nox-native-startup-party.bin').read_bytes();world=(root/'saves/nox-native-startup-world.bin').read_bytes()
    assert len(party)==870 and party[:6]==b'NPAR\x01\x00'
    assert party[81:81+len(a.name)]==bytes(ord(c)|128 for c in a.name) and party[81+len(a.name)]==0
    assert world[:11]==b'NWLD\x01\x00\x08\x07\x18\x26\x01'
    wait_checked=0
    if a.wait_oracle:
        source=(a.wait_oracle/'result.bin').read_bytes();assert len(source)==65536
        clock=(root/'saves/nox-native-startup-clock.bin').read_bytes();assert clock[:6]==b'NCLK\x01\x00'
        expected_clock=bytes(source[address] for address in (0x2bf8,0x3878,0x3879,0x3ffc,0x367f,0x414))
        assert clock[6:]==expected_clock,('Wait clock differs',list(clock[6:]),list(expected_clock))
        assert int(fields['wait_commands'])==32 and fields['wait_entropy_setup']=='119' and fields['map_light_setup']=='1'
        for i in range(256):assert world[11+i]==source[0x400+i],('Wait quest field differs',i,world[11+i],source[0x400+i]);wait_checked+=1
        expected_map=source[0xba00:0xc000]+source[0x6ab0:0x6bb0]
        assert len(expected_map)==1792
        for i in range(1792):assert world[271+i]==expected_map[i],('Wait map record differs',i,world[271+i],expected_map[i]);wait_checked+=1
        observed=[list(map(int,line.split('\t'))) for line in (root/'saves/nox-native-startup-wait.tsv').read_text().splitlines()]
        expected=json.loads((a.wait_oracle/'metadata.json').read_text())['turns']
        assert len(observed)==len(expected)==32
        for i,(actual,original) in enumerate(zip(observed,expected)):
            assert actual==[i+1]+original['clock'],('Wait clock transition differs',i,actual,original['clock']);wait_checked+=6
    checked=0
    if a.creation_oracle:
        data=(a.creation_oracle/'input.bin').read_bytes();assert data[:8]==b'NPCR\x01\x00\x00\x00'
        count=struct.unpack_from('<I',data,8)[0];offset=12;expected=None
        for _ in range(count):
            gender,race,size=data[offset:offset+3];offset+=3
            name=data[offset:offset+size].decode();offset+=size
            records=data[offset:offset+864];offset+=864
            if (gender,race,name)==(a.gender,a.race,a.name):expected=records
        assert offset==len(data) and expected is not None
        # Equipment recalculation has separate source oracles. Compare every
        # creation-owned byte outside its declared derived-field writes.
        derived={7,8,9,10,11,20,21,22,23,64,65,70,*range(90,128)}
        for i in range(768):
            if i%128 not in derived:
                assert party[6+i]==expected[i],('Creation source mismatch',i,party[6+i],expected[i]);checked+=1
        assert party[774:]==expected[768:]
        checked+=96
    trace=[line.split('\t') for line in (root/'saves/nox-native-startup-input.tsv').read_text().splitlines()]
    assert len(trace)==len(keys) and [int(row[0]) for row in trace]==list(range(1,len(keys)+1))
    assert trace[0][1:]==['0','','0','0'] # Empty Return refused.
    assert trace[4][1:]==['0','   ','0','0'] # All-space Return refused.
    assert trace[5][1:]==['0','','0','0'] # Escape resets the name.
    assert trace[15][1:]==['3','DRAFT','1','1'] and trace[16][1:]==['0','','0','0'] # Confirm-stage restart.
    assert trace[26][1:]==['0','','0','0'] # Backspace removed WRONG.
    assert trace[-1][1:]==['4',a.name,str(a.gender),str(a.race)]
    story=[line.split('\t') for line in (root/'saves/nox-native-startup-story.tsv').read_text().splitlines()]
    assert story[0]==['start','229','14','17','1']
    assert [int(row[2]) for row in story[1:]]==([229] if a.story_arrow else [])+[428,643,714,714]
    assert story[-1][-1]=='1' and story[1][-1]=='0'
    opening_bytes=0
    if a.opening_oracle:
        expected=(a.opening_oracle/'input.bin').read_bytes();actual=(root/'saves/nox-native-startup-story-frames.bin').read_bytes()
        assert expected[:12]==b'NOST\x01\x00\x00\x00\x04\x00\x00\x00' and actual[:8]==b'NNSF\x01\x00\x04\x00'
        ep,ap=12,8;visible={((y&7)*1024+((y>>3)&7)*128+(y>>6)*40+x) for y in range(192) for x in range(40)}
        for page in range(4):
            assert actual[ap:ap+2]==expected[ep:ep+2];ap+=2;ep+=2
            plane,source=actual[ap:ap+8192],expected[ep:ep+8192];assert len(plane)==len(source)==8192
            for i in range(8192):
                if i in visible:assert plane[i]==source[i],('Opening source pixel mismatch',page,i,plane[i],source[i]);opening_bytes+=1
                else:assert plane[i]==0,('Native opaque gap changed',page,i)
            ap+=8192;ep+=8192
        assert ap==len(actual) and ep==len(expected)
        if a.capture:
            from PIL import Image
            metadata=json.loads((a.opening_oracle/'metadata.json').read_text());source=Path(metadata['last_source_png']);assert sha(source)==metadata['last_source_png_sha256']
            assert Image.open(root/'saves/nox-native-startup-story.png').convert('RGB').tobytes()==Image.open(source).convert('RGB').tobytes(), 'Native opening RGB presentation differs from actual original CE'
    receipt=dict(status='VERIFIED',scope='Native keyboard creation through four complete opening story pages and first prepared map scene; initial actor turn, ordinary gameplay, full saves, sessions and earned campaign remain required',executable_sha256=sha(a.exe),argv=command,input_only=True,keys=len(keys),original_wait_state_bytes_checked=wait_checked,wait_oracle_sha256=sha(a.wait_oracle/'result.bin') if a.wait_oracle else None,original_creation_bytes_checked=checked,original_opening_visible_bytes_checked=opening_bytes,opening_oracle_sha256=sha(a.opening_oracle/'input.bin') if a.opening_oracle else None,result=fields,artifacts={str(q.relative_to(root)):sha(q) for q in root.rglob('*') if q.is_file()})
    (root/'verification.json').write_text(json.dumps(receipt,indent=2)+'\n');print('VERIFIED native startup keyboard workflow',root)
if __name__=='__main__':main()
