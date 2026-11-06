#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Linked native Open -> movement through the door -> second notice, with cold continuations."""
import argparse,configparser,hashlib,json,os,shutil,struct,subprocess
from pathlib import Path
from test_resources import run_validation_process
from test_native_world_input import sections

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['exe','game','artifacts','reference']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--battlefields',type=Path)
    p.add_argument('--encounter-catalog',type=Path)
    p.add_argument('--encounter-formation',type=Path)
    p.add_argument('--battlefield-choice',type=Path)
    p.add_argument('--battle-command-reference',type=Path)
    p.add_argument('--battle-bars',type=Path)
    p.add_argument('--battle-frame',type=Path)
    p.add_argument('--battle-motion',type=Path)
    p.add_argument('--battle-combat',action='store_true')
    p.add_argument('--battle-return',action='store_true')
    p.add_argument('--import-checkpoint',type=Path,action='append',default=[])
    a=p.parse_args();root=a.artifacts.resolve();root.mkdir(parents=True,exist_ok=False)
    for label in ['door-step','north-notice']:assert (a.reference/('nox-original-world-'+label+'.bin')).is_file()
    base=[(ord(c.lower()),ord(c))for c in 'TRACE']+[(13,0)]*8
    east=(275,0);up=(273,0);open_key=(111,79);enter=(13,13)
    def run(label,keys,checkpoint=None,capture=False):
        folder=root/label;folder.mkdir();saves=folder/'saves';saves.mkdir();keyfile=folder/'keys.bin';keyfile.write_bytes(b''.join(struct.pack('<HH',*key)for key in keys))
        settings={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(a.game.resolve()),'nox_native_startup':'true','nox_native_startup_keys':str(keyfile)}
        if capture:settings['nox_native_startup_capture']='true'
        if a.battlefields and label=='door-step':settings['nox_native_battlefields_oracle']=str((a.battlefields/'input.bin').resolve())
        if a.encounter_catalog and label=='door-step':settings['nox_native_encounter_catalog_oracle']=str((a.encounter_catalog/'input.bin').resolve())
        if a.encounter_formation and label=='door-step':settings['nox_native_encounter_formation_oracle']=str((a.encounter_formation/'input.bin').resolve())
        if a.battlefield_choice and label=='door-step':settings['nox_native_battlefield_choice_oracle']=str((a.battlefield_choice/'input.bin').resolve())
        if a.battle_bars and label=='door-step':settings['nox_native_battle_bars_oracle']=str((a.battle_bars/'input.bin').resolve())
        if a.battle_frame and label=='door-step':settings['nox_native_battle_frame_oracle']=str((a.battle_frame/'input.bin').resolve())
        if a.battle_motion and label=='door-step':settings['nox_native_battle_motion_oracle']=str((a.battle_motion/'input.bin').resolve())
        if checkpoint:shutil.copy2(checkpoint,saves/'nox-native-exploration.bin');settings['nox_native_startup_resume']='true'
        config=configparser.ConfigParser();config['scummvm']={'gui_return_to_launcher':'false','updates_check':'0'};config['nox-native-startup']=settings
        with (folder/'test.ini').open('w')as f:config.write(f)
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        argv=[str(a.exe.resolve()),'--config='+str(folder/'test.ini'),'--savepath='+str(saves),'--logfile='+str(folder/'engine.log'),'--no-console','nox-native-startup']
        result=run_validation_process(argv,folder,dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startup)
        (folder/'stdout.txt').write_bytes(result.stdout);(folder/'stderr.txt').write_bytes(result.stderr)
        fields=dict(part.split('=',1)for part in (saves/'nox-native-startup-report.txt').read_text().strip().split('\t')[1:]);assert fields['status']=='VERIFIED' and fields['compatibility_runtime']=='0'
        if a.battlefields and label=='door-step':
            oracle=(saves/'nox-native-battlefields-oracle.tsv').read_text();assert 'cases=160' in oracle and 'bytes=29920' in oracle and 'cpu_execution=0' in oracle
        if a.encounter_catalog and label=='door-step':
            oracle=(saves/'nox-native-encounter-catalog-oracle.tsv').read_text();assert 'cases=222' in oracle and 'descriptor_bytes=888' in oracle and 'cpu_execution=0' in oracle
        if a.encounter_formation and label=='door-step':
            oracle=(saves/'nox-native-encounter-formation-oracle.tsv').read_text();assert 'cases=8' in oracle and 'record_bytes=20472' in oracle and 'unused_scratch_alias_bytes_excluded=8' in oracle and 'cpu_execution=0' in oracle
        if a.battlefield_choice and label=='door-step':
            oracle=(saves/'nox-native-battlefield-choice-oracle.tsv').read_text();assert 'cases=110' in oracle and 'cpu_execution=0' in oracle
        if a.battle_bars and label=='door-step':
            oracle=(saves/'nox-native-battle-bars-oracle.tsv').read_text();assert 'cases=567' in oracle and 'cpu_execution=0' in oracle
        if a.battle_frame and label=='door-step':
            oracle=(saves/'nox-native-battle-frame-oracle.tsv').read_text();assert 'visible_bytes=7680' in oracle and 'cpu_execution=0' in oracle
        if a.battle_motion and label=='door-step':
            oracle=(saves/'nox-native-battle-motion-oracle.tsv').read_text();assert 'linked_turns=5' in oracle and 'enemy_moves=5' in oracle and 'controlled_refusals=1' in oracle and 'full_record_bytes=17604' in oracle and 'cpu_execution=0' in oracle
        packet=saves/'nox-native-exploration-final.bin';return folder,packet,sections(packet.read_bytes()),fields
    initial=base+[east,open_key,up]
    step,step_packet,step_parts,step_fields=run('door-step',initial+[up])
    notice,notice_packet,notice_parts,notice_fields=run('north-notice',initial+[up,up],capture=True)
    results=[]
    for label,folder,parts,fields in [('door-step',step,step_parts,step_fields),('north-notice',notice,notice_parts,notice_fields)]:
        raw=(a.reference/('nox-original-world-'+label+'.bin')).read_bytes();world=(folder/'saves/nox-native-startup-world.bin').read_bytes()
        assert world[6:11]==bytes([8,7,25,37 if label=='door-step' else 36,1])
        expected=raw[0xba00:0xc000]+raw[0x6ab0:0x6bb0]
        assert world[11:267]==raw[0x400:0x500]
        for i in range(1792):assert world[271+i]==expected[i],('World differs',label,i,world[271+i],expected[i])
        assert (folder/'saves/nox-native-startup-clock.bin').read_bytes()[6:]==bytes(raw[i]for i in [0x2bf8,0x3878,0x3879,0x3ffc,0x367f,0x414])
        assert parts['TURN'][-3]==raw[0x257e]
        assert fields['notice_pending']==str(int(label=='north-notice'))
        if label=='north-notice':
            indices=[(y&7)*1024+((y>>3)&7)*128+(y>>6)*40+x for y in range(8,104)for x in range(25,38)]
            diffs=[sum(parts['FRAM'][i]!=raw[page+i]for i in indices)for page in [0x2000,0x4000]]
            assert min(diffs)==0,('Second notice text differs',diffs)
        results.append(dict(case=label,position=fields['position'],notice_pending=fields['notice_pending'],world_source_bytes=2048,source_sha256=sha(a.reference/('nox-original-world-'+label+'.bin'))))
    cold,cold_packet,_,_=run('cold-step-next-notice',[up],step_packet)
    assert cold_packet.read_bytes()==notice_packet.read_bytes(),'Cold door step did not produce the same actual notice'
    direct,direct_packet,_,_=run('dismiss-and-wait',initial+[up,up,enter,(32,32)])
    resumed,resumed_packet,_,_=run('cold-notice-dismiss-and-wait',[enter,(32,32)],notice_packet)
    assert direct_packet.read_bytes()==resumed_packet.read_bytes(),'Pending second notice cold continuation differs'
    if (a.reference/'nox-original-world-encounter-handoff.bin').is_file():
        handoff,handoff_packet,parts,_=run('encounter-handoff',initial+[up,up,enter,up],capture=True)
        raw=(a.reference/'nox-original-world-encounter-handoff.bin').read_bytes()
        world=(handoff/'saves/nox-native-startup-world.bin').read_bytes()
        assert world[6:11]==bytes([8,7,25,35,1])
        assert world[11:267]==raw[0x400:0x500]
        expected=raw[0xba00:0xc000]+raw[0x6ab0:0x6bb0]
        for i in range(1792):assert world[271+i]==expected[i],('Handoff world differs',i,world[271+i],expected[i])
        pending_clock=bytes(raw[i]for i in [0x2bf8,0x3878,0x3879,0x3ffc,0x367f,0x414])
        presented=parts.get('BTTL',b'')[:8] in [b'NBTS\x02\x00\x03\x01',b'NBTS\x03\x00\x07\x01',b'NBTS\x04\x00\x07\x01',b'NBTS\x05\x00\x07\x01',b'NBTS\x06\x00\x07\x01',b'NBTS\x07\x00\x07\x01',b'NBTS\x08\x00\x07\x01',b'NBTS\x09\x00\x07\x01',b'NBTS\x0a\x00\x07\x01',b'NBTS\x0b\x00\x07\x01']
        expected_clock=pending_clock
        if presented:
            assert pending_clock==bytes([1,16,1,0,0,0]),'Update source-supported clock expectation for a different encounter profile'
            expected_clock=bytes([0,16,2,0,0,0])
        assert (handoff/'saves/nox-native-startup-clock.bin').read_bytes()[6:]==expected_clock
        event=dict(part.split('=',1)for part in (handoff/'saves/nox-native-encounter-handoff.tsv').read_text().strip().split('\t')[1:])
        assert event['bank']=='0' and int(event['slot'])==raw[0xbf8] and int(event['bank'])==raw[0xbf7] and int(event['direction'])==raw[0x2cfd] and int(event['cell'])==raw[0x719] and event.get('battle_completed',event.get('battle_executed'))=='0',event
        assert event['clock_pending']==str(int(not presented))
        assert parts['INPT'][8:]==bytes([1,int(event['bank']),int(event['slot']),int(event['direction']),int(event['cell'])])
        cold_handoff,cold_handoff_packet,_,_=run('cold-encounter-handoff',[],handoff_packet)
        assert handoff_packet.read_bytes()==cold_handoff_packet.read_bytes(),'Cold handoff lost pending turn or event'
        for index,checkpoint in enumerate(a.import_checkpoint):
            imported,imported_packet,_,_=run('import-checkpoint-%02d'%index,[],checkpoint)
            assert imported_packet.read_bytes()==handoff_packet.read_bytes(),'Old private checkpoint import lost phase, repeated RNG or clock retirement'
            results.append(dict(case='private-checkpoint-import',source_sha256=sha(checkpoint),source_header=checkpoint.read_bytes()[:8].hex(),result_sha256=sha(imported_packet),matches_linked_current_state=True))
        if a.encounter_formation:
            data=(a.encounter_formation/'input.bin').read_bytes();assert data[:12]==b'NFCS'+struct.pack('<II',2,8)
            rows=[data[12+i*5136:12+(i+1)*5136]for i in range(8)]
            source=next(row for row in rows if row[:8]==notice_parts['RAND'][6:])
            formed=parts['BTTL'];offset=8 if presented else 7
            assert (presented and len(formed)==8+2560+8192+({4:19,5:46,6:190,7:214,8:246,9:262,10:262,11:264}.get(formed[4],0))) or (formed[:7]==b'NBTS\x01\x00\x01' and len(formed)==2567)
            expected=bytearray(source[2568:5128])
            if presented:
                assert a.battle_command_reference and parts['TURN'][-3]==2
                command=(a.battle_command_reference/'nox-original-world-battle-command.bin').read_bytes()
                # Initial source actor passes face the party/enemy before the
                # first command; compare those actual resulting flag bytes.
                expected[2048+3]=command[0xbd03];expected[2304+3]=command[0xbf03]
            assert formed[offset:offset+800]==expected[:800],'Linked formation party/template differ from same-RNG original operation'
            assert formed[offset+2048:offset+2560]==expected[2048:],'Linked formation actor positions differ'
            assert parts['RAND'][6:]==source[-8:],'Linked formation RNG did not retain original draws'
            assert formed==(handoff/'saves/nox-native-encounter-formation.bin').read_bytes()
            assert formed==(cold_handoff/'saves/nox-native-encounter-formation.bin').read_bytes(),'Cold formation rerolled or lost records'
        if a.encounter_catalog:
            data=(a.encounter_catalog/'input.bin').read_bytes();assert data[:8]==b'NECS\x02\x00\x00\x00'
            choices=[data[12+i*41:12+(i+1)*41]for i in range(struct.unpack_from('<I',data,8)[0])]
            expected=next(choice for choice in choices if choice[:2]==bytes([8,164]));assert expected[4]==0
            selection=(handoff/'saves/nox-native-encounter-selection.bin').read_bytes()
            assert selection==b'NENC\x01\x00'+expected[:4]+expected[5:]
            assert selection==(cold_handoff/'saves/nox-native-encounter-selection.bin').read_bytes()
            assert int.from_bytes(selection[16:18],'little')==12,'Authored living Rat HP differs'
        if a.battle_command_reference:
            command=(a.battle_command_reference/'nox-original-world-battle-command.bin').read_bytes()
            if presented:assert expected_clock==bytes(command[i]for i in [0x2bf8,0x3878,0x3879,0x3ffc,0x367f,0x414]),'Native battle-entry clock differs from actual source first command'
            selection=(handoff/'saves/nox-native-encounter-selection.bin').read_bytes()
            assert selection[14:]==command[65536+0x1900:65536+0x1920],'Fixed monster candidate differs from actual original first battle'
            terrain=(handoff/'saves/nox-native-encounter-terrain.bin').read_bytes()
            assert terrain==command[0x800:0x8bb],'Actual native encounter battlefield differs'
            for name in ['nox-native-encounter-selection.bin','nox-native-encounter-terrain.bin','nox-native-encounter-preparation.tsv']:
                assert (handoff/'saves'/name).read_bytes()==(cold_handoff/'saves'/name).read_bytes(),('Prepared cold encounter differs',name)
        results.append(dict(case='encounter-handoff',position='25/35',battle_executed=False,presented=presented,formation_verified=bool(a.encounter_formation),event=event,clock_pending=not presented,cold_identical=True))
        if a.battle_motion:
            moves=[up,east,(274,0),(276,0)]
            direct,direct_packet,direct_parts,_=run('battle-four-turns',initial+[up,up,enter,up]+moves)
            half,half_packet,_,_=run('battle-two-turns',moves[:2],handoff_packet)
            resumed,resumed_packet,_,_=run('cold-battle-two-more',moves[2:],half_packet)
            assert direct_packet.read_bytes()==resumed_packet.read_bytes(),'Cold tactical turns lost records, RNG, image or parent state'
            trace=(direct/'saves/nox-native-battle-movement.tsv').read_text().splitlines();assert len(trace)==4
            rows=[line.split('\t') for line in trace]
            assert [tuple(map(int,row[2:4]))for row in rows]==[(4,7),(5,7),(5,8),(4,8)]
            assert all(row[-2:]==['50','12']for row in rows),'Distant turns changed living party/enemy HP'
            assert (direct/'saves/nox-native-startup-world.bin').read_bytes()==world,'Battle motion mutated parent exploration records'
            assert direct_parts['CORE']==parts['CORE'],'Battle motion advanced world clock or mutated parent core'
            assert direct_parts['RAND']!=parts['RAND'],'Enemy turns did not consume actual RNG'
            results.append(dict(case='linked-native-distant-battle-turns',player_moves=4,enemy_movement_turns=4,damage_turns=0,cold_split='2+2',cold_packet_identical=True))
            close,close_packet,close_parts,_=run('battle-approach-adjacent',[east],direct_packet)
            refusal,refusal_packet,_,_=run('battle-occupied-refusal',[east],close_packet)
            assert close_packet.read_bytes()==refusal_packet.read_bytes(),'Occupied attempt retired an enemy turn/RNG or changed owned state'
            state=close_parts['BTTL'];assert state[8+2048:8+2050]==bytes([5,8]) and state[8+2304:8+2306]==bytes([6,8]),'Native inline pursuit did not approach adjacency'
            refused_event=dict(part.split('=',1)for part in (refusal/'saves/nox-native-encounter-handoff.tsv').read_text().strip().split('\t')[1:])
            assert refused_event['occupied_refusals']=='1' and refused_event['tactical_move_commands']=='0'
            results.append(dict(case='native-approach-and-occupied-refusal',approach_enemy_turns=1,occupied_refusals=1,refusal_state_rng_unchanged=True,damage_turns=0))
            if a.battle_combat:
                attack=[(97,65),up,(13,13)]
                hit,hit_packet,hit_parts,_=run('battle-native-attack',attack,close_packet,capture=True)
                aim,aim_packet,aim_parts,_=run('battle-native-target-pending',attack[:2],close_packet)
                restored,restored_packet,_,_=run('battle-native-cold-confirm',attack[2:],aim_packet)
                assert hit_packet.read_bytes()==restored_packet.read_bytes(),'Cold target continuation changed actual attack/turn/RNG result'
                state=hit_parts['BTTL'];meta=8+2560
                assert state[:8] in (b'NBTS\x05\x00\x07\x01',b'NBTS\x06\x00\x07\x01',b'NBTS\x07\x00\x07\x01',b'NBTS\x08\x00\x07\x01',b'NBTS\x09\x00\x07\x01',b'NBTS\x0a\x00\x07\x01',b'NBTS\x0b\x00\x07\x01') and len(state)=={5:10806,6:10950,7:10974,8:11006,9:11022,10:11022,11:11024}[state[4]]
                assert int.from_bytes(state[8+770:8+772],'little')==0 and state[8+2307]==255 and state[8+2311]==255,'Native attack did not remove living Rat'
                assert int.from_bytes(state[meta:meta+2],'little')==2 and state[meta+2:meta+4]==bytes([0,255]) and state[meta+10]==1,'Native victory-pending/credit state differs'
                assert aim_parts['BTTL'][meta+2]==1 and aim_parts['RAND']==close_parts['RAND'],'Target selection spent RNG or lost pending input'
                assert hit_parts['CORE']==close_parts['CORE'],'Attack mutated parent world/clock before results'
                cold_hit,cold_hit_packet,_,_=run('battle-native-cold-dead',[],hit_packet)
                assert cold_hit_packet.read_bytes()==hit_packet.read_bytes(),'Restoration rerolled/repeated lethal damage or credit'
                results.append(dict(case='native-attack-and-cold-target',controlled_initialization='None beyond prior linked native checkpoint',living_rat_hp=12,actual_native_kill=True,credit=2,cold_target_identical=True,cold_dead_identical=True,battle_return=False))
                if a.battle_return:
                    result_folder,result_packet,result_parts,_=run('battle-native-results',[(27,27)],hit_packet,capture=True)
                    assert result_parts['BTTL'][8+5:8+7]==bytes([2,0]),'Victory did not allocate actual earned pool to party XP'
                    if result_parts['BTTL'][meta+19]==1:
                        result_folder,result_packet,result_parts,_=run('battle-native-leave-item',[(108,76)],result_packet,capture=True)
                    assert result_parts['BTTL'][meta+19]==2,'Results did not reach final acknowledgement'
                    _,cold_summary_packet,_,_=run('battle-native-cold-results',[],result_packet)
                    assert cold_summary_packet.read_bytes()==result_packet.read_bytes(),'Cold result state repeated XP,loot or currency'
                    returned,return_packet,return_parts,_=run('battle-native-return',[(13,13)],result_packet,capture=True)
                    assert return_parts['BTTL'] in (b'NBTS\x05\x00\x00\x01',b'NBTS\x06\x00\x00\x01',b'NBTS\x07\x00\x00\x01',b'NBTS\x08\x00\x00\x01',b'NBTS\x09\x00\x00\x01',b'NBTS\x0a\x00\x00\x01',b'NBTS\x0b\x00\x00\x01'),'Battle did not relinquish owned battle input/state'
                    world=(returned/'saves/nox-native-startup-world.bin').read_bytes()
                    assert world[6:11]==bytes([8,7,25,35,1]) and world[11+9]&1 and world[271+768+4+2]==164 and world[271+768+4+3]==255,'Return lost quest completion or source-backed defeated actor retirement'
                    party=(returned/'saves/nox-native-startup-party.bin').read_bytes()
                    assert int.from_bytes(party[6+5:6+7],'little')==2,'Return lost earned XP'
                    completion=dict(part.split('=',1)for part in (returned/'saves/nox-native-battle-completion.tsv').read_text().strip().split('\t')[1:])
                    assert completion['completed']=='1' and completion['last_credit']=='2' and completion['party_XP']=='2'
                    _,cold_return_packet,_,_=run('battle-native-cold-return',[],return_packet)
                    assert cold_return_packet.read_bytes()==return_packet.read_bytes(),'Cold returned world differs'
                    after_return,_,after_return_parts,after_return_fields=run('battle-native-world-action',[(32,32)],return_packet)
                    assert after_return_fields['wait_commands']=='1' and after_return_parts['BTTL']==return_parts['BTTL']
                    assert (after_return/'saves/nox-native-startup-clock.bin').read_bytes()!=(returned/'saves/nox-native-startup-clock.bin').read_bytes(),'Subsequent world action failed to retire a turn'
                    results.append(dict(case='native-complete-rat-battle-and-return',actual_native_kill=True,earned_pool_in_controlled_native_run=2,actual_loot_decisions=True,cold_results_identical=True,world_quest_bit=True,defeated_actor_removed=True,subsequent_world_action=True,battle_return=True,full_earned_campaign=False))
    d=dict(status='VERIFIED',scope='Actual native creation -> first notice -> Open -> north through opened door -> second authored notice -> dismiss/Wait; two actual cold continuations',executable_sha256=sha(a.exe),input_only=True,cases=results,controlled_setup=['Existing initial native light/entropy and presentation phases; no world/items/quest outcomes injected'],linked_progression=['Opened door retained into movement','Door-step checkpoint -> fresh CE -> second notice matches uninterrupted bytes','Second notice checkpoint -> fresh CE -> dismiss/Wait matches uninterrupted bytes'],limitations=['Keep interior on foot, current qualified actor domain only','Complete W1-W6, native-only release and earned campaign remain required'],artifacts={str(f.relative_to(root)):sha(f)for f in root.rglob('*')if f.is_file()})
    (root/'verification.json').write_text(json.dumps(d,indent=2)+'\n');print('VERIFIED linked native door/notice/cold workflow',root)
if __name__=='__main__':main()
