#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""CE-native format/rule checks against an independent read-only map decoder.

This is NOT native-only gameplay, interpreter retirement or campaign acceptance.
Private images/decoded data remain in the chosen local artifact directory.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from test_resources import run_validation_process


def reverse_sector(encoded, output_size=256):
    cursor=len(encoded); position=output_size; buffer=0; result=bytearray(output_size)
    def take():
        nonlocal cursor
        assert cursor>0
        cursor-=1
        return encoded[cursor]
    def bit():
        nonlocal buffer
        carry=buffer>>7
        buffer=(buffer*2)&255
        if not buffer:
            value=take(); carry=value>>7; buffer=((value*2)|1)&255
        return carry
    def literal():
        nonlocal position
        assert position>0
        position-=1; result[position]=take()
    literal()
    while True:
        if bit()==0:
            literal(); continue
        width=1
        while bit()==0:
            width+=1; assert width<=17
        if width==17:
            assert position==0 and cursor==0
            return result
        length=1
        for _ in range(width-1): length=length*2+bit()
        length+=1
        low=take(); distance=(low&127)+1
        if low>=128:
            upper=0
            for _ in range(4): upper=upper*2+bit()
            distance+=(upper+1)*128
        assert length<=position and distance<=output_size-position
        for _ in range(length):
            position-=1; result[position]=result[position+distance]


def resource_bytes(image):
    data=image.read_bytes(); result={}
    def block(number):
        assert 0<=number<len(data)//512
        return data[number*512:(number+1)*512]
    def pointers(number):
        value=block(number) if number else bytes(512)
        return [value[i]+256*value[i+256] for i in range(256)]
    def directory(number,prefix):
        first=number; visited=set()
        while number:
            assert number not in visited; visited.add(number)
            value=block(number)
            for slot in range(13):
                if number==first and slot==0: continue
                record=value[4+slot*39:43+slot*39]; kind=record[0]>>4
                if not kind: continue
                name=prefix+record[1:1+(record[0]&15)].decode('ascii')
                key=int.from_bytes(record[17:19],'little'); size=int.from_bytes(record[21:24],'little')
                if kind==13: directory(key,name+'/'); continue
                assert 1<=kind<=3
                blocks=[key] if kind==1 else pointers(key) if kind==2 else [j for i in pointers(key) for j in pointers(i)]
                result[name]=b''.join(block(i) if i else bytes(512) for i in blocks[:(size+511)//512])[:size]
            number=int.from_bytes(value[2:4],'little')
    directory(2,'')
    return result


def expected_maps(image):
    files=resource_bytes(image); result={}; sectors=0
    md5=lambda value:hashlib.md5(value).hexdigest()
    for family in ['SURF','UM','RUIN','TOWN','TOWN2','CSTL','KEEP']:
        name='NA/DATA.MAP.'+family; data=files[name]; cursor=2 if family=='SURF' else 1
        while data[cursor]!=255:
            size=data[cursor] or 256; header=data[cursor:cursor+size]; ident=header[1]
            start=int.from_bytes(header[2:5],'little'); following=int.from_bytes(header[7:10],'little')
            compressed=family=='SURF' or (family=='UM' and ident not in [76,77])
            width=256 if compressed else 48; columns=width//16; terrain=bytearray(width*width)
            for sector in range(columns*columns):
                if compressed:
                    table=start-514
                    address=lambda i:data[table+i]+256*data[table+257+i]-2048
                    cells=reverse_sector(data[start+address(sector):start+address(sector+1)])
                else:
                    cells=data[start+sector*256:start+(sector+1)*256]
                for y in range(16):
                    at=(sector//columns*16+y)*width+sector%columns*16
                    terrain[at:at+16]=cells[y*16:y*16+16]
                sectors+=1
            result[name,ident]=(int(compressed),md5(terrain),md5(data[following:following+1792]),md5(header))
            cursor+=size
    assert len(result)==76
    return result,sectors


def expected_packets(image):
    result={}
    md5=lambda value:hashlib.md5(value).hexdigest()
    for name,data in resource_bytes(image).items():
        if not name.startswith('NA/DATA.TLK.'):continue
        offset=0; seen=set()
        for number in range(1,33):
            if offset>=len(data) or data[offset]==252 or offset+3>len(data):break
            actor=data[offset];length=int.from_bytes(data[offset+1:offset+3],'little')
            if not length or length>len(data)-offset-3:break
            if actor not in seen and actor%8==0:
                encoded=data[offset+3:offset+3+length]
                size=int.from_bytes(encoded[:2],'little');assert 0<size<=0x13fd
                decoded=reverse_sector(encoded[2:],size)
                result[name,actor]=(number,offset+3,size,md5(encoded),md5(decoded))
            seen.add(actor);offset+=3+length
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['exe','base','expansion','artifacts']:parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--maps',action='store_true')
    parser.add_argument('--world',action='store_true')
    parser.add_argument('--dialogue',action='store_true')
    parser.add_argument('--clock',action='store_true')
    parser.add_argument('--dialogue-rules',type=Path)
    parser.add_argument('--inventory',type=Path)
    parser.add_argument('--selection',type=Path)
    parser.add_argument('--openings',type=Path)
    parser.add_argument('--conversation',type=Path)
    parser.add_argument('--temple',type=Path)
    parser.add_argument('--inn',type=Path)
    parser.add_argument('--inn-membership',type=Path)
    parser.add_argument('--party-selector',type=Path)
    parser.add_argument('--dialogue-input',type=Path)
    parser.add_argument('--pages-chain',type=Path)
    parser.add_argument('--graphics',type=Path)
    parser.add_argument('--traversal',type=Path)
    parser.add_argument('--party',type=Path)
    parser.add_argument('--view',type=Path)
    parser.add_argument('--objects',type=Path)
    parser.add_argument('--visibility',type=Path)
    parser.add_argument('--light',type=Path)
    parser.add_argument('--terrain-light',type=Path)
    parser.add_argument('--personal-light',type=Path)
    parser.add_argument('--lighting-chain',type=Path)
    parser.add_argument('--actor-paint',type=Path)
    parser.add_argument('--actors',type=Path)
    parser.add_argument('--tile-paint',type=Path)
    parser.add_argument('--world-presentation',type=Path)
    parser.add_argument('--rng',type=Path)
    parser.add_argument('--npc-schedule',type=Path)
    parser.add_argument('--owned-path',type=Path)
    parser.add_argument('--npc-route-store',type=Path)
    parser.add_argument('--npc-route-follow',type=Path)
    parser.add_argument('--npc-navigation',type=Path)
    parser.add_argument('--npc-clock-pipeline',type=Path)
    parser.add_argument('--world-large-geometry',type=Path)
    parser.add_argument('--surface-offscreen',type=Path)
    parser.add_argument('--world-actor-phase',type=Path)
    parser.add_argument('--world-actor-turn',type=Path)
    parser.add_argument('--compact-world',type=Path)
    parser.add_argument('--text-window',type=Path)
    parser.add_argument('--text-glyph',type=Path)
    parser.add_argument('--message-strip',type=Path)
    parser.add_argument('--npc-route-recovery',type=Path)
    parser.add_argument('--actor-pursuit',type=Path)
    parser.add_argument('--npc-action-dispatch',type=Path)
    parser.add_argument('--npc-path',type=Path)
    parser.add_argument('--npc-prefetch',type=Path)
    parser.add_argument('--npc-reentry',type=Path)
    parser.add_argument('--npc-clock',type=Path)
    parser.add_argument('--npc-actor-actions',type=Path)
    parser.add_argument('--actor-motion',type=Path)
    parser.add_argument('--actor-intent',type=Path)
    parser.add_argument('--actor-step',type=Path)
    parser.add_argument('--actor-spatial',type=Path)
    parser.add_argument('--actor-outside',type=Path)
    parser.add_argument('--actor-movement-effects',type=Path)
    parser.add_argument('--npc-contact',type=Path)
    parser.add_argument('--npc-world-action',type=Path)
    parser.add_argument('--npc-world-ranged',type=Path)
    parser.add_argument('--npc-world-effects',type=Path)
    parser.add_argument('--npc-world-free',type=Path)
    parser.add_argument('--npc-large-contact',type=Path)
    parser.add_argument('--actor-contact',type=Path)
    parser.add_argument('--viewport-maps',type=Path)
    parser.add_argument('--world-viewport',type=Path)
    parser.add_argument('--occlusion',type=Path)
    parser.add_argument('--world-command',type=Path)
    parser.add_argument('--world-scene',type=Path)
    parser.add_argument('--world-story',type=Path)
    parser.add_argument('--story-patches',type=Path)
    parser.add_argument('--new-game',type=Path)
    parser.add_argument('--owned-weapon',type=Path)
    parser.add_argument('--weapon-ready',type=Path)
    parser.add_argument('--owned-armor',type=Path)
    parser.add_argument('--equipment-derived',type=Path)
    parser.add_argument('--armor-ready',type=Path)
    parser.add_argument('--equipment-ownership',type=Path)
    parser.add_argument('--battle-records',type=Path)
    parser.add_argument('--summon',type=Path)
    parser.add_argument('--vortex-cast',type=Path)
    parser.add_argument('--vortex-effect',type=Path)
    parser.add_argument('--spell-power',type=Path)
    parser.add_argument('--vortex-targets',type=Path)
    parser.add_argument('--cast-preparation',type=Path)
    parser.add_argument('--book',type=Path)
    parser.add_argument('--page-search',type=Path)
    parser.add_argument('--lock-party',type=Path)
    parser.add_argument('--lock',type=Path)
    parser.add_argument('--search-reward',type=Path)
    args=parser.parse_args()
    driver=Path(__file__).with_name('test_resources.py')
    command=[sys.executable,str(driver),'--exe',str(args.exe),'--base',str(args.base),'--expansion',str(args.expansion),'--artifacts',str(args.artifacts),'--native-video']
    if args.dialogue_rules:command+=['--native-dialogue-oracle',str(args.dialogue_rules.resolve())]
    if args.inventory:command+=['--native-inventory-oracle',str(args.inventory.resolve())]
    if args.selection:command+=['--native-dialogue-selection',str(args.selection.resolve())]
    if args.openings:command+=['--native-dialogue-openings',str(args.openings.resolve())]
    if args.conversation:command+=['--native-conversation',str(args.conversation.resolve())]
    if args.temple:command+=['--native-temple',str(args.temple.resolve())]
    if args.inn:command+=['--native-inn',str(args.inn.resolve())]
    if args.inn_membership:command+=['--native-inn-membership',str(args.inn_membership.resolve())]
    if args.party_selector:command+=['--native-party-selector',str(args.party_selector.resolve())]
    if args.dialogue_input:command+=['--native-dialogue-input',str(args.dialogue_input.resolve())]
    if args.pages_chain:command+=['--native-pages-chain',str(args.pages_chain.resolve())]
    if args.search_reward:command+=['--native-search-reward',str(args.search_reward.resolve())]
    if args.lock:command+=['--native-lock',str(args.lock.resolve())]
    if args.lock_party:command+=['--native-lock-party',str(args.lock_party.resolve())]
    if args.page_search:command+=['--native-page-search',str(args.page_search.resolve())]
    if args.book:command+=['--native-book',str(args.book.resolve())]
    if args.cast_preparation:command+=['--native-cast-preparation',str(args.cast_preparation.resolve())]
    if args.vortex_targets:command+=['--native-vortex-targets',str(args.vortex_targets.resolve())]
    if args.spell_power:command+=['--native-spell-power',str(args.spell_power.resolve())]
    if args.vortex_effect:command+=['--native-vortex-effect',str(args.vortex_effect.resolve())]
    if args.vortex_cast:command+=['--native-vortex-cast',str(args.vortex_cast.resolve())]
    if args.summon:command+=['--native-summon',str(args.summon.resolve())]
    if args.battle_records:command+=['--native-battle-records',str(args.battle_records.resolve())]
    if args.equipment_ownership:command+=['--native-equipment-ownership',str(args.equipment_ownership.resolve())]
    if args.armor_ready:command+=['--native-armor-ready',str(args.armor_ready.resolve())]
    if args.equipment_derived:command+=['--native-equipment-derived',str(args.equipment_derived.resolve())]
    if args.owned_armor:command+=['--native-owned-armor',str(args.owned_armor.resolve())]
    if args.weapon_ready:command+=['--native-weapon-ready',str(args.weapon_ready.resolve())]
    if args.npc_schedule:command+=['--native-npc-schedule',str(args.npc_schedule.resolve())]
    if args.npc_action_dispatch:command+=['--native-npc-action-dispatch',str(args.npc_action_dispatch.resolve())]
    if args.actor_pursuit:command+=['--native-actor-pursuit',str(args.actor_pursuit.resolve())]
    if args.npc_route_recovery:command+=['--native-npc-route-recovery',str(args.npc_route_recovery.resolve())]
    if args.message_strip:command+=['--native-message-strip',str(args.message_strip.resolve())]
    if args.world_large_geometry:command+=['--native-world-large-geometry',str(args.world_large_geometry.resolve())]
    if args.surface_offscreen:command+=['--native-surface-offscreen',str(args.surface_offscreen.resolve())]
    if args.world_actor_phase:command+=['--native-world-actor-phase',str(args.world_actor_phase.resolve())]
    if args.world_actor_turn:command+=['--native-world-actor-turn',str(args.world_actor_turn.resolve())]
    if args.compact_world:command+=['--native-compact-world',str(args.compact_world.resolve())]
    if args.text_window:command+=['--native-text-window',str(args.text_window.resolve())]
    if args.text_glyph:command+=['--native-text-glyph',str(args.text_glyph.resolve())]
    if args.npc_clock_pipeline:command+=['--native-npc-clock-pipeline',str(args.npc_clock_pipeline.resolve())]
    if args.npc_navigation:command+=['--native-npc-navigation',str(args.npc_navigation.resolve())]
    if args.npc_route_follow:command+=['--native-npc-route-follow',str(args.npc_route_follow.resolve())]
    if args.npc_route_store:command+=['--native-npc-route-store',str(args.npc_route_store.resolve())]
    if args.owned_path:command+=['--native-owned-path',str(args.owned_path.resolve())]
    if args.npc_path:command+=['--native-npc-path',str(args.npc_path.resolve())]
    if args.npc_prefetch:command+=['--native-npc-prefetch',str(args.npc_prefetch.resolve())]
    if args.npc_reentry:command+=['--native-npc-reentry',str(args.npc_reentry.resolve())]
    if args.npc_clock:command+=['--native-npc-clock',str(args.npc_clock.resolve())]
    if args.npc_actor_actions:command+=['--native-npc-actor-actions',str(args.npc_actor_actions.resolve())]
    if args.actor_motion:command+=['--native-actor-motion',str(args.actor_motion.resolve())]
    if args.actor_intent:command+=['--native-actor-intent',str(args.actor_intent.resolve())]
    if args.actor_step:command+=['--native-actor-step',str(args.actor_step.resolve())]
    if args.actor_spatial:command+=['--native-actor-spatial',str(args.actor_spatial.resolve())]
    if args.actor_outside:command+=['--native-actor-outside',str(args.actor_outside.resolve())]
    if args.actor_movement_effects:command+=['--native-actor-movement-effects',str(args.actor_movement_effects.resolve())]
    if args.npc_contact:command+=['--native-npc-contact',str(args.npc_contact.resolve())]
    if args.npc_world_action:command+=['--native-npc-world-action',str(args.npc_world_action.resolve())]
    if args.npc_world_ranged:command+=['--native-npc-world-ranged',str(args.npc_world_ranged.resolve())]
    if args.npc_world_effects:command+=['--native-npc-world-effects',str(args.npc_world_effects.resolve())]
    if args.npc_world_free:command+=['--native-npc-world-free',str(args.npc_world_free.resolve())]
    if args.npc_large_contact:command+=['--native-npc-large-contact',str(args.npc_large_contact.resolve())]
    if args.actor_contact:command+=['--native-actor-contact',str(args.actor_contact.resolve())]
    if args.viewport_maps:command+=['--native-viewport-maps',str(args.viewport_maps.resolve())]
    if args.world_viewport:command+=['--native-world-viewport',str(args.world_viewport.resolve())]
    if args.occlusion:command+=['--native-occlusion',str(args.occlusion.resolve())]
    if args.world_command:command+=['--native-world-command',str(args.world_command.resolve())]
    if args.world_scene:command+=['--native-world-scene',str(args.world_scene.resolve())]
    if args.world_story:command+=['--native-world-story',str(args.world_story.resolve())]
    if args.story_patches:command+=['--native-story-patches',str(args.story_patches.resolve())]
    if args.new_game:command+=['--native-new-game',str(args.new_game.resolve())]
    if args.owned_weapon:command+=['--native-owned-weapon',str(args.owned_weapon.resolve())]
    if args.rng:command+=['--native-rng',str(args.rng.resolve())]
    if args.world_presentation:command+=['--native-world-presentation',str(args.world_presentation.resolve())]
    if args.tile_paint:command+=['--native-tile-paint',str(args.tile_paint.resolve())]
    if args.actors:command+=['--native-actors',str(args.actors.resolve())]
    if args.actor_paint:command+=['--native-actor-paint',str(args.actor_paint.resolve())]
    if args.lighting_chain:command+=['--native-lighting-chain',str(args.lighting_chain.resolve())]
    if args.personal_light:command+=['--native-personal-light',str(args.personal_light.resolve())]
    if args.terrain_light:command+=['--native-terrain-light',str(args.terrain_light.resolve())]
    if args.light:command+=['--native-light',str(args.light.resolve())]
    if args.visibility:command+=['--native-visibility',str(args.visibility.resolve())]
    if args.objects:command+=['--native-objects',str(args.objects.resolve())]
    if args.view:command+=['--native-view',str(args.view.resolve())]
    if args.party:command+=['--native-party',str(args.party.resolve())]
    if args.traversal:command+=['--native-traversal',str(args.traversal.resolve())]
    if args.graphics:command+=['--native-graphics',str(args.graphics.resolve())]
    process=subprocess.run(command,capture_output=True,text=True)
    args.artifacts.mkdir(parents=True,exist_ok=True)
    (args.artifacts/'resource-driver.log').write_text(process.stdout+process.stderr)
    assert process.returncode==0,process.stdout+process.stderr
    root=sorted(args.artifacts.glob('*/verification.json'))[-1].parent
    report=(root/'saves/nox-resource-validation.tsv').read_text()
    gate=next(line for line in report.splitlines() if line.startswith('native-gates\t'))
    values=dict(part.split('=',1) for part in gate.split('\t')[1:])
    assert [int(values[k]) for k in ['maps','keyed','compound','plates']]==[76,38,5,14]
    result=dict(status='VERIFIED',scope='Native rule/data/owner units inside CE before Runtime construction; not ordinary native gameplay or M4 completion',
                command=command,resource_report=str((root/'verification.json').resolve()),gate_counts=values,
                executable_sha256=hashlib.sha256(args.exe.read_bytes()).hexdigest(),
                source_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (driver.parents[1].parent/'engines/nox').glob('native_*.h')},
                native_only_gameplay='NOT STARTED',earned_native_campaign='NOT STARTED')
    result['actual_compiled_native_units']=['native_gates.h','native_gates_validation.h']
    if args.search_reward:
        trace=next(line for line in report.splitlines() if line.startswith('native-search-reward\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert int(entries['cases'])==270 and int(entries['repeats'])==135
        result['search_reward']=dict(status='VERIFIED',trace=trace,scope='Controlled unlocked quest Search reward phase; lock/trap and ordinary input integration excluded',
            oracle_sha256={name:hashlib.sha256((args.search_reward/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_search.h','native_search_validation.h']
    if args.lock:
        trace=next(line for line in report.splitlines() if line.startswith('native-lock\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert int(entries['cases'])==297 and int(entries['damage_requests'])==55
        result['lock']=dict(status='VERIFIED',trace=trace,scope='Single Search lock attempt with real native RNG and recorded party effects; party callbacks and command integration unfinished',
            oracle_sha256={name:hashlib.sha256((args.lock/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_lock.h','native_lock_validation.h']
    if args.lock_party:
        trace=next(line for line in report.splitlines() if line.startswith('native-lock-party\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert int(entries['cases'])==297 and int(entries['promotions'])==165
        result['lock_party']=dict(status='VERIFIED',trace=trace,scope='Linked native lock RNG/effects and party record application; normal command and death presentation unfinished',
            oracle_sha256={name:hashlib.sha256((args.lock_party/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_lock_party_validation.h']
    if args.page_search:
        trace=next(line for line in report.splitlines() if line.startswith('native-page-search\t'))
        linked=next(line for line in report.splitlines() if line.startswith('native-search-dialogue-link\t'))
        assert 'awards=7' in trace and 'page_outcome_initialization=0' in linked
        result['page_search']=dict(status='VERIFIED',trace=trace,linked=linked,scope='Seven native Search awards feed actual native Axea/Valess/Nystul operations; controlled initial prerequisites/travel/RNG and component execution before Runtime construction',
            oracle_sha256={name:hashlib.sha256((args.page_search/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_page_search_validation.h']
    if args.book:
        trace=next(line for line in report.splitlines() if line.startswith('native-book\t'))
        assert 'cases=2800' in trace
        result['book']=dict(status='VERIFIED',trace=trace,scope='Owned book selection/eligibility and20-byte slot state; general equipment/UI/casting excluded',
            oracle_sha256={name:hashlib.sha256((args.book/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','caster.bin','caster-metadata.json']})
        if args.page_search:
            linked=next(line for line in report.splitlines() if line.startswith('native-bound-book\t'))
            assert 'actual_native_binding_inventory=1' in linked and 'actual_ready_slot1=12' in linked
            result['book']['linked_binding_readiness']=dict(status='VERIFIED',trace=linked,scope='Retained native Search and actual native NPC/binding inventory, starter refusal, controlled supported caster BEFORE actual native Ready; no slot outcome import')
        result['actual_compiled_native_units']+=['native_book.h','native_book_validation.h']
    if args.cast_preparation:
        trace=next(line for line in report.splitlines() if line.startswith('native-cast\t'))
        assert 'cases=520' in trace and 'mana_committed=0' in trace
        result['cast']=dict(status='VERIFIED',trace=trace,scope='Standard player cast prerequisites and cost/reach/target intent; no effect dispatch, mana commit or battle completion',
            oracle_sha256={name:hashlib.sha256((args.cast_preparation/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        if args.book and args.page_search:
            linked=next(line for line in report.splitlines() if line.startswith('native-bound-cast\t'))
            assert 'world_refusal=1' in linked and 'prepared_cost=87' in linked
            result['cast']['linked_readiness']=dict(status='VERIFIED',trace=linked)
        result['actual_compiled_native_units']+=['native_cast.h','native_cast_validation.h']
    if args.vortex_targets:
        trace=next(line for line in report.splitlines() if line.startswith('native-vortex-targets\t'))
        assert 'cases=9700' in trace and 'maximum_qualified=37' in trace
        result['vortex_targets']=dict(status='VERIFIED',trace=trace,scope='Resolved battle scene Vortex selection/order and source side/direction rules, at most37 targets; reachable actor count, damage/animation/turn integration unfinished',
            oracle_sha256={name:hashlib.sha256((args.vortex_targets/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_vortex_targets.h','native_vortex_targets_validation.h']
    if args.spell_power:
        trace=next(line for line in report.splitlines() if line.startswith('native-spell-power\t'))
        assert 'scaling_cases=34048' in trace and 'retained_draws=64' in trace
        result['spell_power']=dict(status='VERIFIED',trace=trace,scope='Spell level attenuation, source variation bounds and retained RNG draws; complete hit/defense/status/HP/effect/mana operation remains unfinished',
            oracle_sha256={name:hashlib.sha256((args.spell_power/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_spell_power.h','native_spell_power_validation.h']
    if args.vortex_effect:
        trace=next(line for line in report.splitlines() if line.startswith('native-vortex-effect\t'))
        assert 'cases=3600' in trace and 'killed=1350' in trace
        result['vortex_effect']=dict(status='VERIFIED',trace=trace,scope='Standard-player Vortex per-target normalization/refusal, complete HP/status/actor/lookup and kill-credit owner; whole cast/mana/animation/turn integration unfinished',
            oracle_sha256={name:hashlib.sha256((args.vortex_effect/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_vortex_effect.h','native_vortex_effect_validation.h']
    if args.vortex_cast:
        trace=next(line for line in report.splitlines() if line.startswith('native-vortex-cast\t'))
        assert 'cases=288' in trace and 'chains=96' in trace and 'killed=6' in trace
        result['vortex_cast']=dict(status='VERIFIED',trace=trace,scope='Owned logical Vortex preparation, marker, mana and ordered target effects with retained repeat/refusal chains; UI/animation/turn/world integration unfinished',oracle_sha256={name:hashlib.sha256((args.vortex_cast/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_vortex_cast.h','native_vortex_cast_validation.h']
        if args.book and args.page_search:
            linked=next(line for line in report.splitlines() if line.startswith('native-binding-vortex-link\t'))
            assert 'actual_native_kill=1' in linked and 'retained_mana_refusal=1' in linked
            result['binding_vortex_link']=dict(status='VERIFIED',trace=linked,scope='Native Search/Axea/Valess/Nystul/Ready output retained into logical Vortex kill and subsequent mana refusal; explicit controlled battle/caster setup, no earned travel or whole battle claim.')

    if args.summon:
        trace=next(line for line in report.splitlines() if line.startswith('native-summon-pool\t'))
        assert 'cases=68' in trace and 'allocated=64' in trace and 'full_refusals=4' in trace and 'retained_fill=16' in trace
        result['summon_pool']=dict(status='VERIFIED',trace=trace,scope='Source first unused/dead companion stat allocation, original-health snapshot and index; actor placement and whole spell/admission excluded',oracle_sha256={name:hashlib.sha256((args.summon/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_summon_pool.h','native_summon_validation.h']
    if args.battle_records:
        trace=next(line for line in report.splitlines() if line.startswith('native-battle-records\t'))
        assert 'cases=190' in trace and 'adjacent_compact_record_preserved=1' in trace
        result['battle_records']=dict(status='VERIFIED',trace=trace,scope='Original battle actor/stat ownership across16 enemy,16 companion and6 player slots; explicit typed write protocol, no admission/damage/movement claim',oracle_sha256={name:hashlib.sha256((args.battle_records/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_battle_records_validation.h']
    if args.equipment_ownership:
        trace=next(line for line in report.splitlines() if line.startswith('native-equipment-ownership\t'))
        assert 'cases=432' in trace and 'inventory_unchanged=1' in trace
        result['equipment_ownership']=dict(status='VERIFIED',trace=trace,scope='Read-only equipment category/weapon ownership, two-handed occupancy and all-party ready-copy count; eligibility/commit/derived stats/UI excluded',oracle_sha256={name:hashlib.sha256((args.equipment_ownership/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_equipment_ownership.h','native_equipment_ownership_validation.h']
    if args.armor_ready:
        trace=next(line for line in report.splitlines() if line.startswith('native-armor-ready\t'))
        assert 'cases=742' in trace and 'applied=348' in trace and 'refused=394' in trace
        result['armor_ready']=dict(status='VERIFIED',trace=trace,scope='World armor eligibility/unready and atomic owner/equipment/weight commit before deferred derived-stat recalculation; UI and full Ready acceptance excluded',oracle_sha256={name:hashlib.sha256((args.armor_ready/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_armor_ready.h','native_armor_ready_validation.h']
    if args.equipment_derived:
        trace=next(line for line in report.splitlines() if line.startswith('native-equipment-derived\t'))
        assert 'cases=1185' in trace and 'all_DEX_values=256' in trace and 'full_character_record=1' in trace
        result['equipment_derived']=dict(status='VERIFIED',trace=trace,scope='Complete equipment-derived128-byte player recalculation with weapons/armor/fists/names/weight/decimal chance; ordinary Ready/UI/battle integration excluded',oracle_sha256={name:hashlib.sha256((args.equipment_derived/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_equipment_derived.h','native_equipment_derived_validation.h']
    if args.owned_armor:
        trace=next(line for line in report.splitlines() if line.startswith('native-owned-armor\t'))
        assert 'cases=742' in trace and 'complete_commits=348' in trace and 'actual_Party_Inventory_owners=1' in trace
        result['owned_armor']=dict(status='VERIFIED',trace=trace,scope='Owned Party/Inventory armor transaction plus complete derived recalculation; atomic refusals and full record outputs match original linked operations. Ordinary input/UI/general weapons excluded.',oracle_sha256={name:hashlib.sha256((args.owned_armor/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_equipment_actions.h','native_owned_armor_validation.h']
    if args.weapon_ready:
        trace=next(line for line in report.splitlines() if line.startswith('native-weapon-ready\t'))
        assert 'cases=1111' in trace and 'committed=647' in trace
        result['weapon_ready']=dict(status='VERIFIED',trace=trace,scope='World weapon Ready/unready hand/owner/equipment/weight transaction before derived recalculation; ordinary UI and new shield selection excluded',oracle_sha256={name:hashlib.sha256((args.weapon_ready/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_weapon_ready.h','native_weapon_ready_validation.h']
    if args.npc_schedule:
        trace=next(line for line in report.splitlines() if line.startswith('native-npc-schedule\t'))
        first=next(line for line in report.splitlines() if line.startswith('native-new-game-world\t'))
        metadata=json.loads((args.npc_schedule/'metadata.json').read_text())
        assert ('cases='+str(metadata['cases'])) in trace and ('covered_maps='+str(len(metadata['maps']))) in trace and ('unsupported_castle_padding_maps='+str(len(metadata['unsupported_maps']))) in trace
        assert 'actual_NewGame_create=1' in first and 'NPC_at_10_8=1' in first
        result['npc_schedule']=dict(status='VERIFIED',trace=trace,scope=metadata['scope'],oracle_sha256={name:hashlib.sha256((args.npc_schedule/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','first-world.bin']})
        result['npc_schedule_remaining']=dict(status='IN PROGRESS',maps=metadata['unsupported_maps'],reason='Unaligned source padding schedule records require semantic resolution; no native parity claim.')
        result['new_game_world']=dict(status='IN PROGRESS',warm=first,scope='Actual native NewGame.create and NPC schedule match actual original first-world logical state; pending cold continuation. UI/normal loop/complete player save excluded.')
        result['actual_compiled_native_units']+=['native_npc_schedule_validation.h']
    if args.npc_action_dispatch:
        trace=next(line for line in report.splitlines() if line.startswith('native-npc-action-dispatch\t'))
        assert 'operations=39168' in trace and 'actual_approach_messages=1035' in trace
        result['npc_action_dispatch']=dict(status='IN PROGRESS',warm=trace,scope='2448 controlled intent-to-ordinary-motion/pursuit/route-recovery chains,39168 logical actions,1035 actual approach presentations, post-route actions retained. Resolved spatial inputs; complete contact/battle/raster/actor loop and ordinary native UI remain required',oracle_sha256={name:hashlib.sha256((args.npc_action_dispatch/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','cold.bin','continuation.json']})
        result['actual_compiled_native_units']+=['native_npc_action_dispatch.h','native_npc_action_dispatch_validation.h']
    if args.actor_pursuit:
        trace=next(line for line in report.splitlines() if line.startswith('native-actor-pursuit\t'))
        assert 'choices=26244' in trace
        result['actor_pursuit']=dict(status='IN PROGRESS',warm=trace,scope='Controlled requested pursuit direction:81 relative boundary contexts,81 resolved collision vectors,2 caller policy profiles and retained RNG. Ordinary policy mapping, movement/contact/battle/approach and full actor turn remain required',oracle_sha256={name:hashlib.sha256((args.actor_pursuit/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_actor_pursuit.h','native_actor_pursuit_validation.h']
    if args.npc_route_recovery:
        trace=next(line for line in report.splitlines() if line.startswith('native-npc-route-recovery\t'))
        assert 'operations=34953' in trace and 'actual_approach_messages=241' in trace
        result['npc_route_recovery']=dict(status='IN PROGRESS',warm=trace,scope='2304 controlled resolved-spatial route chains,34953 actual logical operations and241 real native approach36 presentations with continuation; counter2 explicitly controlled. Full collision derivation/actor turn/UI/campaign remain required',oracle_sha256={name:hashlib.sha256((args.npc_route_recovery/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','cold.bin']})
        result['actual_compiled_native_units']+=['native_npc_route_recovery.h','native_npc_route_recovery_validation.h']
    if args.message_strip:
        trace=next(line for line in report.splitlines() if line.startswith('native-message-strip\t'))
        assert 'cases=76' in trace and 'approach_message36=1' in trace
        result['message_strip']=dict(status='IN PROGRESS',warm=trace,scope='38 immutable GAME messages on actual saved-scene background, native clear/centering/glyph/cursor, both fonts; approach36 presentation. Caller continuation, dynamic/banked text, full UI/campaign remain required',oracle_sha256={name:hashlib.sha256((args.message_strip/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','initial.bin','cold.bin']})
        result['actual_compiled_native_units']+=['native_message_strip.h','native_message_strip_validation.h']
    if args.world_large_geometry:
        trace=next(line for line in report.splitlines() if line.startswith('native-world-large-geometry\t'))
        assert 'cases=23936' in trace and 'complete_source_vectors=23106' in trace and 'unsafe_source_prefix_guards=830' in trace
        result['world_large_geometry']=dict(status='VERIFIED',trace=trace,scope='Complete defined source world large vectors and bounded unsafe-source lookup prefixes;whole movement/contact/render/campaign acceptance remains',oracle_sha256={name:hashlib.sha256((args.world_large_geometry/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_world_large_geometry_validation.h','native_actor_spatial.h']
    if args.surface_offscreen:
        trace=next(line for line in report.splitlines() if line.startswith('native-surface-offscreen\t'))
        assert ('whole_turns=30' in trace or 'whole_turns=60' in trace or ('whole_turns=27' in trace and 'large_world=1' in trace)) and 'cpu_execution=0' in trace
        result['surface_offscreen']=dict(status='VERIFIED',trace=trace,scope='Whole source-ordered cache-near compact actor turns and state persistence;general large/NPC/cache-edge/campaign branches remain',oracle_sha256={name:hashlib.sha256((args.surface_offscreen/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_surface_offscreen_validation.h','native_world_actor_turn.h']
    if args.world_actor_phase:
        trace=next(line for line in report.splitlines() if line.startswith('native-world-actor-phase\t'))
        assert 'cases=15840' in trace and 'cpu_execution=0' in trace
        result['world_actor_phase']=dict(status='VERIFIED',trace=trace,scope='Source viewport entry/exit phase policy,not complete general world AI integration',oracle_sha256={name:hashlib.sha256((args.world_actor_phase/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_world_actor_phase.h','native_world_actor_phase_validation.h']
    if args.world_actor_turn:
        trace=next(line for line in report.splitlines() if line.startswith('native-world-actor-turn\t'))
        metadata=json.loads((args.world_actor_turn/'metadata.json').read_text())
        assert ('operations='+str(metadata['operations'])) in trace and 'ordered_occupancy=1' in trace and 'cpu_execution=0' in trace
        result['world_actor_turn']=dict(status='IN PROGRESS',warm=trace,scope=metadata['scope'],limitations=metadata['limitations'],oracle_sha256={name:hashlib.sha256((args.world_actor_turn/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','cold.bin']})
        result['actual_compiled_native_units']+=['native_world_actor_turn.h','native_world_actor_turn_validation.h']
    if args.compact_world:
        trace=next(line for line in report.splitlines() if line.startswith('native-compact-world\t'))
        assert 'operations=50555' in trace and 'battle_handoffs=578' in trace and 'cpu_execution=0' in trace
        metadata=json.loads((args.compact_world/'metadata.json').read_text())
        result['compact_world']=dict(status='IN PROGRESS',warm=trace,scope=metadata['scope'],limitations=metadata['limitations'],oracle_sha256={name:hashlib.sha256((args.compact_world/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','cold.bin']})
        result['actual_compiled_native_units']+=['native_compact_world_action.h','native_compact_world_action_validation.h']
    if args.text_window:
        trace=next(line for line in report.splitlines() if line.startswith('native-text-window\t'))
        assert 'pages=4' in trace and 'window_bytes=18432' in trace and 'owned_scroll_word_wrap=1' in trace
        metadata=json.loads((args.text_window/'metadata.json').read_text())
        result['text_window']=dict(status='IN PROGRESS',warm=trace,scope=metadata['scope'],limitations=metadata['limitations'],oracle_sha256={name:hashlib.sha256((args.text_window/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_text_window.h','native_text_window_validation.h']
    if args.text_glyph:
        trace=next(line for line in report.splitlines() if line.startswith('native-text-glyph\t'))
        assert 'cases=110592' in trace
        result['text_glyph']=dict(status='IN PROGRESS',warm=trace,scope='Original glyph/cursor primitive: all15360 visible bytes plus opaque native gaps in two owned HGR pages; all110592 old full CPU-memory digests retained separately and independently reconstructed. Complete window/message/UI remains required',oracle_sha256={name:hashlib.sha256((args.text_glyph/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','cold.bin','outcome-preservation.json']})
        result['actual_compiled_native_units']+=['native_text_glyph.h','native_text_glyph_validation.h']
    if args.npc_clock_pipeline:
        trace=next(line for line in report.splitlines() if line.startswith('native-npc-clock-pipeline\t'))
        assert 'Town1_2159_to_2200=1' in trace
        result['npc_clock_pipeline']=dict(status='IN PROGRESS',warm=trace,scope='Town1 source-backed21:59 entry/activation/prefetch followed by real native generation, busy guard, retained routes and22:00 activation/prefetch/generation; full actor turn, all-map pipeline88, recovery, UI and campaign remain required',oracle_sha256={name:hashlib.sha256((args.npc_clock_pipeline/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_npc_clock_pipeline_validation.h']
    if args.npc_navigation:
        trace=next(line for line in report.splitlines() if line.startswith('native-npc-navigation\t'))
        assert 'completed_goals=1493' in trace and 'unreachable_refused=2' in trace
        result['npc_navigation']=dict(status='IN PROGRESS',warm=trace,scope='1495 controlled linked queue->owned search->route allocation->actual logical NPC movement chains, independent BFS reachability and legal moves; full clock/prefetch, recovery, collision derivation, turn and earned campaign remain required',oracle_sha256={name:hashlib.sha256((args.npc_navigation/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_npc_navigation.h','native_npc_navigation_validation.h']
    if args.npc_route_follow:
        trace=next(line for line in report.splitlines() if line.startswith('native-npc-route-follow\t'))
        assert 'operations=26943' in trace
        result['npc_route_follow']=dict(status='IN PROGRESS',warm=trace,scope='Controlled unobstructed active-route movement chains; recovery and complete turn remain unverified',oracle_sha256={name:hashlib.sha256((args.npc_route_follow/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_npc_route_follow.h','native_npc_route_follow_validation.h']
    if args.npc_route_store:
        trace=next(line for line in report.splitlines() if line.startswith('native-npc-route-store\t'))
        assert 'operations=56' in trace and 'ordered_cleanup=1' in trace
        result['npc_route_store']=dict(status='IN PROGRESS',warm=trace,scope='Controlled completed-route allocations, not route following or complete NPC turn',oracle_sha256={name:hashlib.sha256((args.npc_route_store/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_npc_route_store.h','native_npc_route_store_validation.h']
    if args.owned_path:
        trace=next(line for line in report.splitlines() if line.startswith('native-owned-path\t'))
        assert 'controlled_routes=1495' in trace and 'reachable=1493' in trace and 'unreachable_refused=2' in trace and 'owned_visit_cells=2304' in trace
        result['owned_path']=dict(status='IN PROGRESS',warm=trace,scope='Controlled authored geometry: owned48x48 visit graph,1493 legal goal-reaching routes and2 independently unreachable layouts match BFS. Native state/RNG restored between calls. This explicit semantic replacement removes program/hardware visited reads; original outer-layout bit parity, full manager/normal-world/campaign behavior remain unverified. Pending fresh-process continuation.',oracle_sha256={name:hashlib.sha256((args.owned_path/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_owned_path_validation.h']
    if args.npc_path:
        trace=next(line for line in report.splitlines() if line.startswith('native-npc-path\t'))
        assert 'verified_paths=1493' in trace and 'unverified_source_error_contexts=2' in trace and 'controlled_zero_goal_hash_profile=1' in trace
        result['path_search']=dict(status='IN PROGRESS',warm=trace,scope='Controlled zero-goal-cache arithmetic and explicit BEFORE-operation workspace-tail inputs:1493 actual original successful route searches, full working-data hashes and RNG every yield, actual search-state roundtrip between calls. Pending fresh-process continuation; actual normal workspace initialization, two original error contexts, nonzero-goal outer hash, manager/following/full turn remain unverified.',oracle_sha256={name:hashlib.sha256((args.npc_path/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['path_search_remaining']=dict(status='IN PROGRESS',source_error_cases=[1010,1017],reason='Actual CE/original error context validation, nonzero-goal borrowed-layout resolution without CPU ABI, route-manager/following and full next-minute pipeline88 remain required.')
        result['actual_compiled_native_units']+=['native_npc_path_validation.h']
    if args.npc_prefetch:
        trace=next(line for line in report.splitlines() if line.startswith('native-npc-prefetch\t'))
        assert 'linked_entry_to_clock=17928' in trace and 'covered_maps=75' in trace and 'route_prefetch_calls=2' in trace
        result['prefetch_rules']=dict(status='VERIFIED',trace=trace,scope='17928 retained entry/clock/prefetch chains on75 maps: first and repeated same-minute prefetch only. Logical queue data persisted/restored between native operations; all final World/goals/requests/selections/cache fields match actual original phases. Full next-minute pipeline remains IN PROGRESS pending path generation. Capacity error presentation and full AI/normal turn excluded.',oracle_sha256={name:hashlib.sha256((args.npc_prefetch/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['npc_prefetch_pipeline']=dict(status='IN PROGRESS',reason='Actual source next-minute queue-capacity boundary reached when required path generation was omitted. Separate bounded repeated-prefetch rules do not satisfy full pipeline acceptance.')
        result['actual_compiled_native_units']+=['native_npc_clock_validation.h']
    if args.npc_reentry:
        trace=next(line for line in report.splitlines() if line.startswith('native-npc-reentry\t'))
        assert 'linked_entry_to_clock=17928' in trace and 'covered_maps=75' in trace and 'reentry=1' in trace
        result['npc_reentry']=dict(status='VERIFIED',trace=trace,scope='17928 linked previous-minute entry, next-minute authored activation and map-entry scheduling again on retained World/NPC state. Castle15 clock-mutated identity0 accepted as naturally resulting source state. Resolved phase boundaries; route-prefetch/full turn/earned travel excluded.',oracle_sha256={name:hashlib.sha256((args.npc_reentry/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_npc_clock_validation.h']
    if args.npc_clock:
        trace=next(line for line in report.splitlines() if line.startswith('native-npc-clock\t'))
        assert 'linked_entry_to_clock=17928' in trace and 'covered_maps=75' in trace and 'complete_AI_turn=0' in trace
        result['npc_clock']=dict(status='VERIFIED',trace=trace,scope='17928 linked native previous-minute map-entry to next-minute authored NPC activation operations on75 maps match actual original phases. Complete map records and unchanged goals/requests/logical selections compared; actual World result round trips. Route prefetch/generation and complete AI/normal turn excluded.',oracle_sha256={name:hashlib.sha256((args.npc_clock/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_npc_clock_validation.h']
    if args.npc_actor_actions:
        trace=next(line for line in report.splitlines() if line.startswith('native-npc-actor-actions\t'))
        assert 'cases=1536' in trace and 'retained_chains=24' in trace and 'continued_from_restored_owner=1' in trace
        result['npc_actor_actions']=dict(status='VERIFIED',trace=trace,scope='Linked native NPC intent/direction/logical movement and actual World record commit,1536 operations in24 retained chains; next operation uses restored actual map state. Resolved collision inputs controlled before operation. Interaction/raster/route/chase/full AI turn and earned progression excluded.',oracle_sha256={name:hashlib.sha256((args.npc_actor_actions/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_npc_actor_actions_validation.h']
    if args.actor_motion:
        trace=next(line for line in report.splitlines() if line.startswith('native-actor-motion\t'))
        assert 'cases=17010' in trace and 'linked_intent_to_choice=7290' in trace and 'complete_AI_turn=0' in trace
        result['actor_motion']=dict(status='VERIFIED',trace=trace,scope='Actual original wander/pattern candidate filtering, inclusive RNG selection and7290 linked initial-intent-to-choice operations. Collision derivation, route/chase, movement/attack commit and complete AI turn excluded.',oracle_sha256={name:hashlib.sha256((args.actor_motion/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_actor_motion_validation.h']
    if args.actor_intent:
        trace=next(line for line in report.splitlines() if line.startswith('native-actor-intent\t'))
        assert 'cases=43008' in trace and 'actual_RNG_draws=8960' in trace
        result['actor_intent']=dict(status='VERIFIED',trace=trace,scope='Original initial world actor intent routing, capability/shared-state/preferences and two retained RNG chains; actual default inclusive1..255 draws. Route/goal service, navigation choice, movement/attack/turn commit and ordinary native gameplay remain required.',oracle_sha256={name:hashlib.sha256((args.actor_intent/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_actor_intent_validation.h']
    for selected,key,prefix,expected in [(args.npc_world_ranged,'npc_world_ranged','native-npc-world-ranged',26112),(args.npc_world_effects,'npc_world_effects','native-npc-world-effects',21485),(args.npc_world_free,'npc_world_free','native-npc-world-free',21589),(args.npc_large_contact,'npc_large_contact','native-npc-large-contact',1728)]:
        if not selected:continue
        trace=next(line for line in report.splitlines() if line.startswith(prefix+'\t'))
        assert 'operations='+str(expected) in trace and 'injected_collision_decisions=0' in trace
        metadata=json.loads((selected/'metadata.json').read_text())
        result[key]=dict(status='IN PROGRESS',warm=trace,scope=metadata['scope'],limitations=metadata['limitations'],oracle_sha256={name:hashlib.sha256((selected/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','cold.bin']})
        for name in ['native_npc_world_action.h','native_npc_world_action_validation.h']:
            if name not in result['actual_compiled_native_units']:result['actual_compiled_native_units'].append(name)
    if args.npc_world_action:
        trace=next(line for line in report.splitlines() if line.startswith('native-npc-world-action\t'))
        assert 'operations=22742' in trace and 'scene_derived_collision_preview=1' in trace and 'injected_collision_decisions=0' in trace
        result['npc_world_action']=dict(status='IN PROGRESS',warm=trace,scope='Scene-derived small NPC preparation/gate/collision/contact/intent/logical-action phase with retained world/request/route/RNG/counters. Controlled prepared scene; complete scene assembly, ordered raster, large/offscreen actors, battle/conversation execution and ordinary native player loop remain required.',oracle_sha256={name:hashlib.sha256((args.npc_world_action/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','cold.bin']})
        result['actual_compiled_native_units']+=['native_npc_world_action.h','native_npc_world_action_validation.h']
    if args.npc_contact:
        trace=next(line for line in report.splitlines() if line.startswith('native-npc-contact\t'))
        assert 'cases=1566752' in trace and 'linked_operations=32' in trace
        result['npc_contact']=dict(status='IN PROGRESS',warm=trace,scope='Automatic NPC conversation request, actual source terrain/refusal behavior, admitted actor domain and eight linked four-operation chains. Ordinary command/UI opening and complete actor turn remain required.',oracle_sha256={name:hashlib.sha256((args.npc_contact/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','domain-preservation.json','continuation.json']})
        result['actual_compiled_native_units']+=['native_npc_contact.h','native_npc_contact_validation.h']
    if args.actor_contact:
        trace=next(line for line in report.splitlines() if line.startswith('native-actor-contact\t'))
        assert 'cases=48896' in trace and 'actual_NPC_request_effects=1' in trace and 'battle_execution=0' in trace
        result['actor_contact']=dict(status='VERIFIED',trace=trace,scope='Original small/large viewport spatial phase plus real automatic NPC request effects, refusal and battle handoff. Controlled stamped scenes; scene assembly, clipped/offscreen movement, actual battle/conversation and complete actor/player turn remain required.',oracle_sha256={name:hashlib.sha256((args.actor_contact/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_actor_contact_validation.h']
    if args.actor_movement_effects:
        trace=next(line for line in report.splitlines() if line.startswith('native-actor-movement-effects\t'))
        assert 'cases=124096' in trace and 'door_transitions=44224' in trace and 'ranged_requests=1920' in trace
        result['actor_movement_effects']=dict(status='VERIFIED',trace=trace,scope='Original intermediate movement-side open-object transition and ranged-effect request/direction cancellation, with actual owned-world commits and state roundtrip. Containing action/ordered turn, viewport-edge aliases, projectile damage/presentation and UI remain required.',oracle_sha256={name:hashlib.sha256((args.actor_movement_effects/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_actor_movement_effects.h','native_actor_movement_effects_validation.h']
    if args.actor_outside:
        trace=next(line for line in report.splitlines() if line.startswith('native-actor-outside\t'))
        assert 'rule_operations=49152' in trace and 'owned_window_operations=72' in trace and 'actual_source_directions=196896' in trace
        result['actor_outside']=dict(status='VERIFIED',trace=trace,scope='Original off-screen four-neighbor terrain policy and actual Keep7/UM101/SURF0 owned-window reads with world/window roundtrip. Cache edges, eligibility/gates/full actor turn remain required.',oracle_sha256={name:hashlib.sha256((args.actor_outside/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_actor_outside.h','native_actor_outside_validation.h']
    if args.actor_spatial:
        trace=next(line for line in report.splitlines() if line.startswith('native-actor-spatial\t'))
        assert 'cases=48896' in trace and 'contact_handoffs=556' in trace and 'cpu_execution=0' in trace
        result['actor_spatial']=dict(status='IN PROGRESS',warm=trace,scope='Actual source small viewport candidates and large leading-edge collision outcomes from controlled stamped scenes. Typed contact handoffs, not contact script/battle execution; clipped large/offscreen movement and complete actor turn remain required.',oracle_sha256={name:hashlib.sha256((args.actor_spatial/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','outcome-preservation.json']})
        result['actual_compiled_native_units']+=['native_actor_spatial_validation.h']
    if args.actor_step:
        trace=next(line for line in report.splitlines() if line.startswith('native-actor-step\t'))
        assert 'cases=224384' in trace and 'retained_candidate_continuations=96' in trace
        result['actor_step']=dict(status='VERIFIED',trace=trace,scope='Complete original world actor destination probe with source family rules, occupancy/object priority, actor traits and retained effective terrain. AI navigation/move/attack/turn commit and battle contexts remain required.',oracle_sha256={name:hashlib.sha256((args.actor_step/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_actor_step_validation.h']
    if args.viewport_maps:
        trace=next(line for line in report.splitlines() if line.startswith('native-viewport-maps\t'))
        assert 'maps=76' in trace and 'refreshes=304' in trace and 'coordinate_sector_owned=1' in trace
        result['viewport_maps']=dict(status='VERIFIED',trace=trace,scope='304 retained native viewport refreshes across76 authored campaign maps match actual original full canvas and world/mask/light state. Owned position derives coordinate-sector lighting. Controlled renderer contexts, not earned travel, UI or AI/ordinary native turn acceptance.',oracle_sha256={name:hashlib.sha256((args.viewport_maps/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_viewport_maps_validation.h']
    if args.world_viewport:
        trace=next(line for line in report.splitlines() if line.startswith('native-world-viewport\t'))
        assert 'full_canvas_bytes=8192' in trace and 'original_raster_including_cache_hits=1' in trace
        result['world_viewport']=dict(status='VERIFIED',trace=trace,scope='Four retained native viewport refreshes compare complete8192-byte canvas and resulting world/mask/light/presentation to actual original full raster. Menu/party UI, AI/normal input and earned campaign excluded.',oracle_sha256={name:hashlib.sha256((args.world_viewport/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_world_viewport_validation.h']
    if args.occlusion:
        trace=next(line for line in report.splitlines() if line.startswith('native-occlusion\t'))
        assert 'geometric_cases=402' in trace and 'ordered_actions=15646' in trace
        result['occlusion']=dict(status='VERIFIED',trace=trace,scope='Original geometric shadows, terrain/object opacity and ordered visibility clears/reveals match complete masks and semantic action sequence. Full rendered-frame/AI/normal turn integration remains required.',oracle_sha256={name:hashlib.sha256((args.occlusion/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_occlusion_validation.h']
    if args.world_command:
        trace=next(line for line in report.splitlines() if line.startswith('native-world-command\t'))
        assert 'changed_terrain_refusal=1' in trace and 'actor_refusal=1' in trace
        result['world_command']=dict(status='VERIFIED',trace=trace,scope='Directional CE event collision/position phase consumes actual native scene occupancy and changed terrain; original scene and independent source traversal decisions match. Full AI/clock/encounter turn, ordinary loop and earned campaign excluded.',oracle_sha256={name:hashlib.sha256((args.world_command/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_world_command_validation.h']
    if args.world_scene:
        trace=next(line for line in report.splitlines() if line.startswith('native-world-scene\t'))
        assert 'prior_visibility_retained=1' in trace and 'cpu_execution=0' in trace
        result['world_scene']=dict(status='VERIFIED',trace=trace,scope='Native story/window/projection/ordered object and actor presentation/visibility composition with naturally retained prior masks. AI turns, ordinary native input and full rendering excluded.',oracle_sha256={name:hashlib.sha256((args.world_scene/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_world_scene_validation.h']
    if args.world_story:
        trace=next(line for line in report.splitlines() if line.startswith('native-world-story\t'))
        assert 'phase=warm' in trace and 'actual_World_owners=1' in trace
        result['world_story']=dict(status='IN PROGRESS',warm=trace,scope='Native World record effects and owned logical terrain window; controlled spawn initialization. Partial World/window persistence, not full player save or earned campaign.',oracle_sha256={name:hashlib.sha256((args.world_story/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_world_story_validation.h']
    if args.story_patches:
        trace=next(line for line in report.splitlines() if line.startswith('native-story-patches\t'))
        assert 'source_rules=11' in trace and 'cpu_execution=0' in trace
        result['story_patches']=dict(status='VERIFIED',trace=trace,scope='Authored conditional terrain/object/actor/spawn effects; source rule corpus and all owner bytes. World/turn integration remains required.',oracle_sha256={name:hashlib.sha256((args.story_patches/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_story_patches_validation.h']
    if args.new_game:
        trace=next(line for line in report.splitlines() if line.startswith('native-new-game\t'))
        assert 'compared_bytes=2510' in trace and 'cpu_execution=0' in trace
        result['new_game']=dict(status='VERIFIED',trace=trace,scope='Native starting data preparation compared to actual original opening. Menu, initial actor/scene scheduling and native player loop excluded.',oracle_sha256={name:hashlib.sha256((args.new_game/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_new_game_validation.h']
    if args.owned_weapon:
        trace=next(line for line in report.splitlines() if line.startswith('native-owned-weapon\t'))
        assert 'cases=1111' in trace and 'complete_commits=647' in trace and 'actual_Party_Inventory_owners=1' in trace
        result['owned_weapon']=dict(status='VERIFIED',trace=trace,scope='Owned Party/Inventory weapon transaction plus complete derived recalculation; atomic refusals and full outputs match original linked operations. Ordinary UI, new shield selection and full native gameplay excluded.',oracle_sha256={name:hashlib.sha256((args.owned_weapon/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_owned_weapon_validation.h']
    if args.rng:
        trace=next(line for line in report.splitlines() if line.startswith('native-rng\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert [int(entries[k]) for k in ['single_cases','entropy_cases','streams','retained_draws']]==[416,260,8,128]
        result['rng']=dict(status='VERIFIED',trace=trace,scope='Non-interrupted original gameplay RNG, entropy initialization, retained streams and native persistence; input interruption/AI/combat integration unfinished',
            oracle_sha256={name:hashlib.sha256((args.rng/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','cold.bin']})
        result['actual_compiled_native_units']+=['native_random.h','native_random_validation.h']
    if args.world_presentation:
        trace=next(line for line in report.splitlines() if line.startswith('native-world-presentation\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert [int(entries[k]) for k in ['cases','presentations','maps','ordered_draws']]==[228,456,76,980]
        assert entries['full_records_and_lookups']=='1' and entries['retained_state']=='1'
        result['world_presentation']=dict(status='VERIFIED',trace=trace,scope='Complete quiescent world object/actor presentation ordering with retained records/counters; controlled contexts; actor AI/turn and party/UI integration incomplete',
            oracle_sha256={name:hashlib.sha256((args.world_presentation/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_world_presentation.h','native_world_presentation_validation.h']
    if args.tile_paint:
        trace=next(line for line in report.splitlines() if line.startswith('native-tile-paint\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert entries['cases']=='32776' and entries['full_plane_bytes']=='8192' and entries['atomic_refusal']=='1'
        result['tile_paint']=dict(status='VERIFIED',trace=trace,scope='Original cache-miss tile raster, light variants,4 animation phases and8/16-row writes; complete scene scheduling and ordinary gameplay unfinished',
            oracle_sha256={name:hashlib.sha256((args.tile_paint/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_tile_paint.h','native_tile_paint_validation.h']
    if args.actors:
        trace=next(line for line in report.splitlines() if line.startswith('native-actors\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert [int(entries[k]) for k in ['cases','absent','present','quest_removed','full_map_state']]==[12348,7716,4238,394,1792]
        result['actors']=dict(status='VERIFIED',trace=trace,scope='Controlled world actor admission/quest culling and appearance metadata; full1792 state comparison; AI/scheduling and ordinary gameplay unfinished',
            oracle_sha256={name:hashlib.sha256((args.actors/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_actors.h','native_actors_validation.h']
    if args.actor_paint:
        trace=next(line for line in report.splitlines() if line.startswith('native-actor-paint\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert entries['cases']=='6668' and entries['draws']=='8000' and all(entries[k]=='187' for k in ['full_terrain','actor_lookups','kind_lookups'])
        result['actor_paint']=dict(status='VERIFIED',trace=trace,scope='Controlled original actor paint/lookup/blend-request stage; admission/AI/record commit and actual blend raster remain incomplete',
            oracle_sha256={name:hashlib.sha256((args.actor_paint/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_actor_paint.h','native_actor_paint_validation.h']
    if args.lighting_chain:
        trace=next(line for line in report.splitlines() if line.startswith('native-lighting-chain\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert entries['cases']=='760' and entries['preparations']=='1520' and entries['maps']=='76' and entries['retained_state']=='1'
        result['lighting_chain']=dict(status='VERIFIED',trace=trace,scope='Linked native base/additional/terrain lighting with naturally retained mask/state; controlled initialization; full actor/scene scheduling and ordinary gameplay incomplete',
            oracle_sha256={name:hashlib.sha256((args.lighting_chain/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_lighting_chain_validation.h']
    if args.personal_light:
        trace=next(line for line in report.splitlines() if line.startswith('native-personal-light\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert entries['cases']=='8192' and entries['full_cells']=='187' and entries['resource_indices']=='90'
        result['personal_light']=dict(status='VERIFIED',trace=trace,scope='Original additional-light reveal with original source data; actor/scene scheduling and ordinary input integration unfinished',
            oracle_sha256={name:hashlib.sha256((args.personal_light/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_personal_light_validation.h']
    if args.terrain_light:
        trace=next(line for line in report.splitlines() if line.startswith('native-terrain-light\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert entries['cases']=='1824' and entries['maps']=='76' and entries['full_cells']=='187'
        result['terrain_light']=dict(status='VERIFIED',trace=trace,scope='Original complete terrain-light source/reveal composition in controlled contexts across76 maps; additional spell path and scene scheduling unfinished',
            oracle_sha256={name:hashlib.sha256((args.terrain_light/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_lighting.h','native_lighting_validation.h']
    if args.light:
        trace=next(line for line in report.splitlines() if line.startswith('native-light\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert entries['cases']=='10240' and entries['full_cells']=='187' and entries['atomic_invalid_state']=='1'
        result['light']=dict(status='VERIFIED',trace=trace,scope='Original base visibility-mask/state transitions; additional light effects, source selection and full scene scheduling unfinished',
            oracle_sha256={name:hashlib.sha256((args.light/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_light_validation.h']
    if args.visibility:
        trace=next(line for line in report.splitlines() if line.startswith('native-visibility\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert entries['cases']=='1065' and entries['full_cells']=='187' and int(entries['changed_cases'])>0
        result['visibility']=dict(status='VERIFIED',trace=trace,scope='Original data-defined visibility reveal kernel; source selection/light masks and complete scheduling unfinished',
            oracle_sha256={name:hashlib.sha256((args.visibility/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_visibility.h','native_visibility_validation.h']
    if args.objects:
        trace=next(line for line in report.splitlines() if line.startswith('native-objects\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert entries['cases']=='4718' and entries['full_records']=='256' and entries['lookup_cells']=='187'
        result['objects']=dict(status='VERIFIED',trace=trace,scope='Controlled single-object projection/door-settle/plate/draw-request phase; actor/visibility inputs explicit; full scene scheduling incomplete',
            oracle_sha256={name:hashlib.sha256((args.objects/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_objects.h','native_objects_validation.h']
    if args.view:
        trace=next(line for line in report.splitlines() if line.startswith('native-view\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert entries['historical_maps']=='7' and entries['terrain_cells']=='1309' and entries['unready_atomic']=='1'
        result['view']=dict(status='VERIFIED',trace=trace,scope='Native world terrain projection compared with actual historical CE arrays in all7 map families; visibility/lighting/actor composition and ordinary scene integration incomplete',
            oracle_sha256={name:hashlib.sha256((args.view/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_view_validation.h']
    if args.traversal:
        trace=next(line for line in report.splitlines() if line.startswith('native-traversal\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert int(entries['cases'])==1054720 and int(entries['styles'])==8
        assert entries['unready_atomic']=='1' and entries['actor_priority']=='1'
        result['traversal']=dict(status='VERIFIED',trace=trace,
            scope='Controlled source algorithm equivalence; not complete movement turns or native gameplay',
            oracle_sha256={name:hashlib.sha256((args.traversal/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','expected.bin']})
        result['actual_compiled_native_units']+=['native_traversal.h','native_traversal_validation.h']
    if args.graphics:
        trace=next(line for line in report.splitlines() if line.startswith('native-graphics\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert [int(entries[k]) for k in ['atlas_styles','historical_styles','fonts','cells','actual_screenshot_pixels']]==[8,7,2,187,215040]
        assert all(entries[k]=='1' for k in ['full_plane','visible_clear','atomic_cell_refusal'])
        result['graphics']=dict(status='VERIFIED',trace=trace,
            scope='Original asset directory/atlases/fonts and full standard-HGR pixel interpretation/cell blits versus historical CE screenshot; scene visibility/UI/other modes and ordinary renderer integration excluded',
            oracle_sha256={name:hashlib.sha256((args.graphics/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_graphics.h','native_graphics_validation.h']
    if args.dialogue_input:
        trace=next(line for line in report.splitlines() if line.startswith('native-dialogue-input\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert int(entries['cases'])==2253 and int(entries['resource_lexical_bytes'])==70
        result['dialogue_input']=dict(status='VERIFIED',trace=trace,
            oracle_sha256={name:hashlib.sha256((args.dialogue_input/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','expected.bin']})
        result['actual_compiled_native_units']+=['native_dialogue_input.h','native_dialogue_input_validation.h']
    if args.pages_chain:
        trace=next(line for line in report.splitlines() if line.startswith('native-pages-chain\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert [int(entries[k]) for k in ['cases','stages','conversations','actual_replies','authored_fields']]==[2,6,12,20,66]
        assert all(entries[k]=='1' for k in ['full1536_inventory','world_commit','retained_native_state'])
        result['pages_chain']=dict(status='VERIFIED',trace=trace,
            scope='Native Axea eighth-page/Valess research/Nystul binding with retained native flags/inventory; explicit controlled map profiles, physical admission/travel/UI/journal/full saves excluded',
            oracle_sha256={name:hashlib.sha256((args.pages_chain/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_chain_validation.h']
    if args.openings:
        trace=next(line for line in report.splitlines() if line.startswith('native-dialogue-openings\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert [int(entries[k]) for k in ['cases','matched','eligible']]==[1317,1025,1025]
        result['dialogue_openings']=dict(status='VERIFIED',trace=trace,
            oracle_sha256={name:hashlib.sha256((args.openings/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','expected.bin']})
    if args.party_selector:
        trace=next(line for line in report.splitlines() if line.startswith('native-party-selector\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert int(entries['cases'])==768 and entries['original_arrows_aliases_returns']=='1'
        result['party_selector']=dict(status='VERIFIED',trace=trace,scope='Original shared selector semantics and continuation,service UI acceptance separate',oracle_sha256={name:hashlib.sha256((args.party_selector/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_party_selection.h','native_party_selection_validation.h']
    if args.inn_membership:
        trace=next(line for line in report.splitlines() if line.startswith('native-inn-membership\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert int(entries['chains'])==240 and entries['all12slots']=='1' and entries['full_owned_records']=='1'
        result['inn_membership']=dict(status='VERIFIED',trace=trace,scope='Original full-owned leave/pickup and NGST3 persistence;ordinary L/P UI and cold integration acceptance separate',oracle_sha256={name:hashlib.sha256((args.inn_membership/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_inn_storage.h','native_inn_membership.h','native_inn_membership_validation.h']
    if args.inn:
        trace=next(line for line in report.splitlines() if line.startswith('native-inn\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert int(entries['rest_cases'])==120 and int(entries['sleep_cases'])==72 and entries['full768_records']=='1'
        result['inn']=dict(status='VERIFIED',trace=trace,scope='Original whole inn fee/rest and sleep clock;room/UI/storage acceptance separate',oracle_sha256={name:hashlib.sha256((args.inn/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_inn_service.h','native_inn_service_validation.h']
    if args.temple:
        trace=next(line for line in report.splitlines() if line.startswith('native-temple\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert int(entries['cases'])==960 and entries['pricing_payment_original']=='1'
        result['temple']=dict(status='VERIFIED',trace=trace,scope='Original pricing/payment and persisted semantic continuation,not full UI service',oracle_sha256={name:hashlib.sha256((args.temple/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_temple_service.h','native_temple_service_validation.h']
    if args.conversation:
        trace=next(line for line in report.splitlines() if line.startswith('native-conversation\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert [int(entries[k]) for k in ['cases','actual_replies','authored_fields']]==[2,6,66]
        assert all(entries[k]=='1' for k in ['full1536_inventory','exit_award','linked_repeat','closed_replay_refused'])
        result['conversation']=dict(status='VERIFIED',trace=trace,
            scope='Controlled native Nystul reply/effect/exit award and retained repeat/refusal versus actual M3 states. Physical admission, UI, journal publishing, save integration and earned campaign excluded.',
            oracle_sha256={name:hashlib.sha256((args.conversation/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_conversation.h','native_conversation_validation.h']
    if args.selection:
        trace=next(line for line in report.splitlines() if line.startswith('native-dialogue-selection\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert [int(entries[k]) for k in ['cases','matched','eligible']]==[16902,11700,11700]
        result['dialogue_selection']=dict(status='VERIFIED',trace=trace,
            scope='First eligible original response from declaration and packet start, including continuation past ineligible matches; effects, presentation and NPC services excluded',
            oracle_sha256={name:hashlib.sha256((args.selection/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','expected.bin']})
    if args.inventory:
        trace=next(line for line in report.splitlines() if line.startswith('native-inventory\t'))
        entries=dict(part.split('=',1) for part in trace.split('\t')[1:])
        assert int(entries['actual_m3_handoffs'])==4
        assert all(entries[name]=='1' for name in ['full1536_comparison','linked_stacks','unlimited','atomic_refusal','gold','exit_handoff'])
        result['inventory']=dict(status='VERIFIED',trace=trace,
            scope='Four exact inventory results versus frozen actual M3 Ankh/Nox Vortex awards, linked stacks and controlled rule boundaries. Full dialogue/services/new-game integration unverified.',
            oracle_sha256={name:hashlib.sha256((args.inventory/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']})
        result['actual_compiled_native_units']+=['native_inventory.h','native_inventory_validation.h']
    if args.maps:
        expected,sectors=expected_maps(args.base)
        actual={}
        for line in report.splitlines():
            fields=line.split('\t')
            if fields[0]=='native-map':actual[fields[1],int(fields[2])]=(int(fields[3]),*fields[4:7])
        assert actual==expected,'CE terrain/metadata/state-table hashes differ from independent full-resource decoder'
        result['maps']=dict(count=len(actual),decoded_sectors=sectors,terrain_metadata_and_state_tables='VERIFIED')
        result['actual_compiled_native_units']+=['native_maps.h','native_maps_validation.h']
    if args.dialogue:
        expected=expected_packets(args.base);actual={}
        for line in report.splitlines():
            fields=line.split('\t')
            if fields[0]=='native-dialogue':actual[fields[1],int(fields[2])]=(int(fields[3]),int(fields[4]),int(fields[5]),fields[6],fields[7])
        assert actual==expected,'Native packet identity/size/encoded/decoded hashes differ from independent decoder'
        assert len(actual)==439
        result['dialogue']=dict(status='VERIFIED',packets=439,identity_and_full_contents='VERIFIED',quest_matching_and_effects='NOT STARTED')
        result['actual_compiled_native_units']+=['native_lz.h','native_dialogue.h','native_dialogue_validation.h']
    if args.clock:
        clock=next(line for line in report.splitlines() if line.startswith('native-clock\t'))
        assert all(value in clock for value in ['half_turns=2880','minutes=1440','hours=24','original_light_expiry=1'])
        result['clock']=dict(status='VERIFIED',trace=clock,normal_scheduler_integration='NOT STARTED')
        result['actual_compiled_native_units']+=['native_clock.h','native_clock_validation.h']
    if args.dialogue_rules:
        rules=next(line for line in report.splitlines() if line.startswith('native-dialogue-rules\t'))
        values=dict(part.split('=',1) for part in rules.split('\t')[1:])
        assert int(values['cases'])==8451
        result['dialogue_rules']=dict(status='VERIFIED',trace=rules,
             scope='Matching, typed metadata and eligibility versus local legacy semantic oracle; quest-service side effects and normal conversation integration unverified',
             oracle_sha256={name:hashlib.sha256((args.dialogue_rules/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin','expected.bin']})
        result['actual_compiled_native_units']+=['native_dialogue_rules.h','native_dialogue_rules_validation.h']
        if (args.dialogue_rules/'effects.bin').is_file():
            effects=next(line for line in report.splitlines() if line.startswith('native-dialogue-effects\t'))
            effect_values=dict(part.split('=',1) for part in effects.split('\t')[1:])
            assert int(effect_values['compared'])==8449 and int(effect_values['unsupported'])==2
            assert effect_values['atomic_rejection']=='1' and effect_values['full_field_comparison']=='1'
            result['dialogue_field_transactions']=dict(status='IN PROGRESS',compared=8449,unsupported=2,trace=effects,
                scope='8449 contexts compare complete256 fields; two malformed original table-spill effects explicitly rejected without partial mutation and remain unverified for native gameplay',
                effects_sha256=hashlib.sha256((args.dialogue_rules/'effects.bin').read_bytes()).hexdigest())
    if args.world:
        warm=next(line for line in report.splitlines() if line.startswith('native-world\t'))
        assert 'phase=warm' in warm and 'actual_open=1' in warm and 'atomic_rejection=1' in warm
        (args.artifacts/'native-core-warm.tsv').write_text(report)
        cold=args.artifacts/'native-cold';(cold/'saves').mkdir(parents=True,exist_ok=False)
        if args.world_story:
            story_digests={}
            for name in ['nox-native-story-world.bin','nox-native-story-window.bin']:
                source=root/'saves'/name; target=cold/'saves'/name; shutil.copy2(source,target)
                story_digests[name]=hashlib.sha256(source.read_bytes()).hexdigest()
        if args.npc_schedule:
            npc_digests={}
            for name in ['nox-native-new-game-world.bin','nox-native-new-game-npc.bin'] + (['nox-native-castle15-world.bin','nox-native-castle15-npc.bin'] if len(metadata['maps'])==75 else []):
                source=root/'saves'/name; target=cold/'saves'/name; shutil.copy2(source,target)
                npc_digests[name]=hashlib.sha256(source.read_bytes()).hexdigest()
        if args.npc_path:
            path_digests={}
            for name in ['nox-native-path-world.bin','nox-native-path-window.bin','nox-native-path-search.bin','nox-native-path-rng.bin']:
                source=root/'saves'/name; target=cold/'saves'/name; shutil.copy2(source,target)
                path_digests[name]=hashlib.sha256(source.read_bytes()).hexdigest()
        if args.npc_action_dispatch:
            npc_action_digests={}
            for name in ['nox-native-npc-action-world.bin','nox-native-npc-action-schedule.bin','nox-native-npc-action-store.bin','nox-native-npc-action-state.bin','nox-native-npc-action-rng.bin','nox-native-npc-action-text.bin']:
                source=root/'saves'/name; target=cold/'saves'/name; shutil.copy2(source,target)
                npc_action_digests[name]=hashlib.sha256(source.read_bytes()).hexdigest()
        world_extra_digests={}
        for selected,key,family in [(args.npc_world_ranged,'npc_world_ranged','nox-native-world-ranged'),(args.npc_world_effects,'npc_world_effects','nox-native-world-effects'),(args.npc_world_free,'npc_world_free','nox-native-world-free'),(args.npc_large_contact,'npc_large_contact','nox-native-large-action')]:
            if not selected:continue
            world_extra_digests[key]={}
            for suffix in ['world','schedule','store','state','rng','text']+(['effects'] if key in ('npc_world_effects','npc_world_ranged') else []):
                name=family+'-'+suffix+'.bin';source=root/'saves'/name;target=cold/'saves'/name;shutil.copy2(source,target);world_extra_digests[key][name]=hashlib.sha256(source.read_bytes()).hexdigest()
        if args.npc_world_action:
            world_action_digests={}
            for name in ['nox-native-world-action-world.bin','nox-native-world-action-schedule.bin','nox-native-world-action-store.bin','nox-native-world-action-state.bin','nox-native-world-action-rng.bin','nox-native-world-action-text.bin']:
                source=root/'saves'/name;target=cold/'saves'/name;shutil.copy2(source,target);world_action_digests[name]=hashlib.sha256(source.read_bytes()).hexdigest()
        if args.npc_contact:
            contact_source=root/'saves/nox-native-npc-contact.bin';contact_copied=cold/'saves'/contact_source.name;shutil.copy2(contact_source,contact_copied);contact_digest=hashlib.sha256(contact_source.read_bytes()).hexdigest()
        if args.actor_spatial:
            spatial_source=root/'saves/nox-native-spatial-state.bin';spatial_copied=cold/'saves'/spatial_source.name;shutil.copy2(spatial_source,spatial_copied);spatial_digest=hashlib.sha256(spatial_source.read_bytes()).hexdigest()
        if args.actor_pursuit:
            pursuit_source=root/'saves/nox-native-pursuit-rng.bin';pursuit_copied=cold/'saves'/pursuit_source.name;shutil.copy2(pursuit_source,pursuit_copied);pursuit_digest=hashlib.sha256(pursuit_source.read_bytes()).hexdigest()
        if args.npc_route_recovery:
            recovery_digests={}
            for name in ['nox-native-recovery-world.bin','nox-native-recovery-schedule.bin','nox-native-recovery-store.bin','nox-native-recovery-state.bin','nox-native-recovery-rng.bin','nox-native-recovery-text.bin']:
                source=root/'saves'/name; target=cold/'saves'/name; shutil.copy2(source,target)
                recovery_digests[name]=hashlib.sha256(source.read_bytes()).hexdigest()
        if args.message_strip:
            message_source=root/'saves/nox-native-message-strip.bin';message_copied=cold/'saves'/message_source.name;shutil.copy2(message_source,message_copied);message_digest=hashlib.sha256(message_source.read_bytes()).hexdigest()
        if args.world_actor_turn:
            turn_hashes={}
            for name in ['world','state','schedule','store','rng','text','terrain']:
                path=root/('saves/nox-native-turn-'+name+'.bin');shutil.copy2(path,cold/'saves'/path.name);turn_hashes[path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
        if args.compact_world:
            compact_hashes={}
            for name in ['world','state','rng','effects']:
                path=root/('saves/nox-native-compact-'+name+'.bin');shutil.copy2(path,cold/'saves'/path.name);compact_hashes[path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
        if args.text_window:
            window_source=root/'saves/nox-native-text-window.bin';window_copied=cold/'saves'/window_source.name;shutil.copy2(window_source,window_copied);window_digest=hashlib.sha256(window_source.read_bytes()).hexdigest()
        if args.text_glyph:
            text_source=root/'saves/nox-native-text-glyph.bin';text_copied=cold/'saves'/text_source.name;shutil.copy2(text_source,text_copied);text_digest=hashlib.sha256(text_source.read_bytes()).hexdigest()
        if args.npc_clock_pipeline:
            pipeline_digests={}
            for name in ['nox-native-pipeline-world.bin','nox-native-pipeline-window.bin','nox-native-pipeline-schedule.bin','nox-native-pipeline-store.bin','nox-native-pipeline-search.bin','nox-native-pipeline-rng.bin']:
                source=root/'saves'/name; target=cold/'saves'/name; shutil.copy2(source,target)
                pipeline_digests[name]=hashlib.sha256(source.read_bytes()).hexdigest()
        if args.npc_navigation:
            navigation_digests={}
            for name in ['nox-native-nav-world.bin','nox-native-nav-window.bin','nox-native-nav-schedule.bin','nox-native-nav-store.bin','nox-native-nav-search.bin','nox-native-nav-rng.bin']:
                source=root/'saves'/name; target=cold/'saves'/name; shutil.copy2(source,target)
                navigation_digests[name]=hashlib.sha256(source.read_bytes()).hexdigest()
        if args.npc_route_follow:
            route_follow_digests={}
            for name in ['nox-native-follow-world.bin','nox-native-follow-schedule.bin','nox-native-follow-store.bin','nox-native-follow-motion.bin']:
                source=root/'saves'/name; target=cold/'saves'/name; shutil.copy2(source,target)
                route_follow_digests[name]=hashlib.sha256(source.read_bytes()).hexdigest()
        if args.npc_route_store:
            route_store_digests={}
            for name in ['nox-native-route-world.bin','nox-native-route-schedule.bin','nox-native-route-store.bin']:
                source=root/'saves'/name; target=cold/'saves'/name; shutil.copy2(source,target)
                route_store_digests[name]=hashlib.sha256(source.read_bytes()).hexdigest()
        if args.owned_path:
            owned_path_digests={}
            for name in ['nox-native-owned-path-world.bin','nox-native-owned-path-window.bin','nox-native-owned-path-search.bin','nox-native-owned-path-rng.bin']:
                source=root/'saves'/name; target=cold/'saves'/name; shutil.copy2(source,target)
                owned_path_digests[name]=hashlib.sha256(source.read_bytes()).hexdigest()
        original=root/'saves/nox-native-world-state.bin'
        copied=cold/'saves'/original.name;shutil.copy2(original,copied)
        ordinary_source=root/'saves/nox-native-ordinary-open.bin'
        ordinary_copy=cold/'saves'/ordinary_source.name;shutil.copy2(ordinary_source,ordinary_copy)
        ordinary_digest=hashlib.sha256(ordinary_source.read_bytes()).hexdigest()
        position_source=root/'saves/nox-native-position-state.bin'
        position_copy=cold/'saves'/position_source.name;shutil.copy2(position_source,position_copy)
        party_source=root/'saves/nox-native-party-state.bin'
        party_copy=cold/'saves'/party_source.name;shutil.copy2(party_source,party_copy)
        party_digest=hashlib.sha256(party_source.read_bytes()).hexdigest()
        if args.book and args.page_search:
            book_source=root/'saves/nox-native-book-state.bin'
            book_copied=cold/'saves'/book_source.name;shutil.copy2(book_source,book_copied)
            book_digest=hashlib.sha256(book_source.read_bytes()).hexdigest()

        if args.page_search:
            quest_source=root/'saves/nox-native-quest-checkpoint.bin'
            quest_copied=cold/'saves'/quest_source.name;shutil.copy2(quest_source,quest_copied)
            quest_digest=hashlib.sha256(quest_source.read_bytes()).hexdigest()

        if args.vortex_cast:
            vortex_source=root/'saves/nox-native-vortex-checkpoint.bin'
            vortex_copied=cold/'saves'/vortex_source.name;shutil.copy2(vortex_source,vortex_copied)
            vortex_digest=hashlib.sha256(vortex_source.read_bytes()).hexdigest()

        if args.rng:
            rng_source=root/'saves/nox-native-rng-state.bin'
            rng_copied=cold/'saves'/rng_source.name;shutil.copy2(rng_source,rng_copied)
            rng_digest=hashlib.sha256(rng_source.read_bytes()).hexdigest()

        position_digest=hashlib.sha256(position_source.read_bytes()).hexdigest()
        expected_digest=hashlib.sha256(original.read_bytes()).hexdigest()
        config=(root/'test.ini').read_text().replace('nox_resource_validation=true','nox_resource_validation=true\nnox_native_world_cold=true')
        (cold/'cold.ini').write_text(config)
        cold_command=[str(args.exe.resolve()),'--gfx-mode=surfacesdl','--config='+str((cold/'cold.ini').resolve()),
                      '--savepath='+str((cold/'saves').resolve()),'--logfile='+str((cold/'engine.log').resolve()),'nox-resource-test']
        startup=None
        if os.name=='nt':
            startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        process=run_validation_process(cold_command,cold,
                               dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startup)
        (cold/'stdout.txt').write_bytes(process.stdout);(cold/'stderr.txt').write_bytes(process.stderr)
        assert process.returncode==0,(process.returncode,process.stderr)
        cold_trace=(cold/'saves/nox-resource-validation.tsv').read_text()
        ordinary_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-ordinary-open\t'))
        assert 'actual_open_retained=1' in ordinary_cold and 'repeat_refusal=1' in ordinary_cold
        assert hashlib.sha256(ordinary_copy.read_bytes()).hexdigest()==ordinary_digest
        result['ordinary_open_persistence']=dict(status='VERIFIED',cold=ordinary_cold,checkpoint_sha256=ordinary_digest,scope='Controlled Keep25/38 ordinary Open and presentation result; fresh CE repeat refusal. Ordinary input/complete save/turn integration excluded.')
        if args.vortex_cast:
            vortex_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-vortex-checkpoint\t'))
            assert 'actual_result_restored=1' in vortex_cold and 'continued_cast=1' in vortex_cold and 'continued_mana_refusal=1' in vortex_cold
            assert hashlib.sha256(vortex_copied.read_bytes()).hexdigest()==vortex_digest
            result['vortex_persistence']=dict(status='VERIFIED',cold=vortex_cold,checkpoint_sha256=vortex_digest,scope='Partial logical Vortex owner checkpoint; fresh CE resumes actual native result without replaying first cast. Complete battle/save/turn/UI/session ownership excluded.')
            if args.book and args.page_search:
                linked_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-binding-vortex-link\t'))
                assert 'phase=restored-book' in linked_cold and 'actual_native_kill=1' in linked_cold
                result['binding_vortex_link']['restored_book_trace']=linked_cold
        if args.world_story:
            story_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-world-story\t'))
            assert 'actual_result_restored=1' in story_cold and 'continued_operation=1' in story_cold
            for name,digest in story_digests.items(): assert hashlib.sha256((cold/'saves'/name).read_bytes()).hexdigest()==digest
            result['world_story'].update(status='VERIFIED',cold=story_cold,checkpoint_sha256=story_digests)
        if args.npc_schedule:
            npc_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-new-game-world\t'))
            assert 'actual_result_restored=1' in npc_cold and 'continued_schedule=1' in npc_cold
            for name,digest in npc_digests.items(): assert hashlib.sha256((cold/'saves'/name).read_bytes()).hexdigest()==digest
            result['new_game_world'].update(status='VERIFIED',cold=npc_cold,checkpoint_sha256=npc_digests)
            if len(metadata['maps'])==75:
                castle_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-castle15-schedule\t'))
                assert 'actual_result_restored=1' in castle_cold and 'continued_schedule=1' in castle_cold and 'source_next_result=1' in castle_cold
                result['castle15_schedule_cold']=dict(status='VERIFIED',cold=castle_cold,checkpoint_sha256={name:digest for name,digest in npc_digests.items() if 'castle15' in name},scope='Actual native castle15 tagged schedule result restored in fresh CE, then next schedule compared with original operation result. Partial World/NPC state only; no full player save or earned travel claim.')

        if args.npc_path:
            path_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-npc-path\t'))
            assert 'actual_result_restored=1' in path_cold and 'continued_search=1' in path_cold
            for name,digest in path_digests.items(): assert hashlib.sha256((cold/'saves'/name).read_bytes()).hexdigest()==digest
            result['path_search'].update(status='VERIFIED',cold=path_cold,checkpoint_sha256=path_digests)
        if args.npc_action_dispatch:
            npc_action_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-npc-action-dispatch\t'))
            assert 'actual_message_result_restored=1' in npc_action_cold and 'continued_actual_moves_RNG=1' in npc_action_cold
            for name,digest in npc_action_digests.items(): assert hashlib.sha256((cold/'saves'/name).read_bytes()).hexdigest()==digest
            result['npc_action_dispatch'].update(status='VERIFIED',cold=npc_action_cold,checkpoint_sha256=npc_action_digests)
        for selected,key,prefix,marker in [(args.npc_world_ranged,'npc_world_ranged','native-npc-world-ranged','actual_queued_effect_restored=1'),(args.npc_world_effects,'npc_world_effects','native-npc-world-effects','actual_route_move_restored=1'),(args.npc_world_free,'npc_world_free','native-npc-world-free','actual_route_move_restored=1'),(args.npc_large_contact,'npc_large_contact','native-npc-large-contact','actual_queued_request_restored=1')]:
            if not selected:continue
            extra_cold=next(line for line in cold_trace.splitlines() if line.startswith(prefix+'\t'))
            assert marker in extra_cold and int(dict(cell.split('=',1) for cell in extra_cold.split('\t')[1:])['continued_operations'])>0
            for name,digest in world_extra_digests[key].items():assert hashlib.sha256((cold/'saves'/name).read_bytes()).hexdigest()==digest
            result[key].update(status='VERIFIED',cold=extra_cold,checkpoint_sha256=world_extra_digests[key])
        if args.npc_world_action:
            world_action_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-npc-world-action\t'))
            assert 'actual_route_move_restored=1' in world_action_cold and 'prepared_preferences_restored=1' in world_action_cold
            assert int(dict(cell.split('=',1) for cell in world_action_cold.split('\t')[1:])['continued_operations'])>0
            for name,digest in world_action_digests.items():assert hashlib.sha256((cold/'saves'/name).read_bytes()).hexdigest()==digest
            result['npc_world_action'].update(status='VERIFIED',cold=world_action_cold,checkpoint_sha256=world_action_digests)
        if args.npc_contact:
            contact_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-npc-contact\t'))
            assert 'actual_request_restored=1' in contact_cold and 'continued_requests_refusals=3' in contact_cold
            assert hashlib.sha256(contact_copied.read_bytes()).hexdigest()==contact_digest
            result['npc_contact'].update(status='VERIFIED',cold=contact_cold,checkpoint_sha256={'nox-native-npc-contact.bin':contact_digest})
        if args.actor_spatial:
            spatial_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-actor-spatial\t'))
            assert 'actual_candidate_cache_restored=1' in spatial_cold and 'continued_operations=43648' in spatial_cold
            assert hashlib.sha256(spatial_copied.read_bytes()).hexdigest()==spatial_digest
            result['actor_spatial'].update(status='VERIFIED',cold=spatial_cold,checkpoint_sha256={'nox-native-spatial-state.bin':spatial_digest})
        if args.actor_pursuit:
            pursuit_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-actor-pursuit\t'))
            assert 'actual_RNG_result_restored=1' in pursuit_cold and 'continued_choices=13121' in pursuit_cold
            assert hashlib.sha256(pursuit_copied.read_bytes()).hexdigest()==pursuit_digest
            result['actor_pursuit'].update(status='VERIFIED',cold=pursuit_cold,checkpoint_sha256={'nox-native-pursuit-rng.bin':pursuit_digest})
        if args.npc_route_recovery:
            recovery_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-npc-route-recovery\t'))
            assert 'actual_message_result_restored=1' in recovery_cold and 'continued_actual_moves_RNG=1' in recovery_cold
            for name,digest in recovery_digests.items(): assert hashlib.sha256((cold/'saves'/name).read_bytes()).hexdigest()==digest
            result['npc_route_recovery'].update(status='VERIFIED',cold=recovery_cold,checkpoint_sha256=recovery_digests)
        if args.message_strip:
            message_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-message-strip\t'))
            assert 'actual_result_restored=1' in message_cold and 'continued_message36_to37=1' in message_cold
            assert hashlib.sha256(message_copied.read_bytes()).hexdigest()==message_digest
            result['message_strip'].update(status='VERIFIED',cold=message_cold,checkpoint_sha256={'nox-native-message-strip.bin':message_digest})
        if args.world_actor_turn:
            turn_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-world-actor-turn\t'))
            assert 'actual_turn_restored=1' in turn_cold
            for name,digest in turn_hashes.items():assert hashlib.sha256((cold/'saves'/name).read_bytes()).hexdigest()==digest
            result['world_actor_turn'].update(status='VERIFIED',cold=turn_cold,checkpoint_sha256=turn_hashes)
        if args.compact_world:
            compact_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-compact-world\t'))
            assert 'actual_move_restored=1' in compact_cold
            for name,digest in compact_hashes.items():assert hashlib.sha256((cold/'saves'/name).read_bytes()).hexdigest()==digest
            result['compact_world'].update(status='VERIFIED',cold=compact_cold,checkpoint_sha256=compact_hashes)
        if args.text_window:
            window_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-text-window\t'))
            assert 'actual_more_restored=1' in window_cold and 'pages=4' in window_cold
            assert hashlib.sha256(window_copied.read_bytes()).hexdigest()==window_digest
            result['text_window'].update(status='VERIFIED',cold=window_cold,checkpoint_sha256={'nox-native-text-window.bin':window_digest})
        if args.text_glyph:
            text_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-text-glyph\t'))
            assert 'actual_result_restored=1' in text_cold and 'continued_glyph_and_CR=1' in text_cold
            assert hashlib.sha256(text_copied.read_bytes()).hexdigest()==text_digest
            result['text_glyph'].update(status='VERIFIED',cold=text_cold,checkpoint_sha256={'nox-native-text-glyph.bin':text_digest})
        if args.npc_clock_pipeline:
            pipeline_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-npc-clock-pipeline\t'))
            assert 'actual_result_restored=1' in pipeline_cold and 'continued_to_next_minute=1' in pipeline_cold
            for name,digest in pipeline_digests.items(): assert hashlib.sha256((cold/'saves'/name).read_bytes()).hexdigest()==digest
            result['npc_clock_pipeline'].update(status='VERIFIED',cold=pipeline_cold,checkpoint_sha256=pipeline_digests)
        if args.npc_navigation:
            navigation_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-npc-navigation\t'))
            assert 'actual_result_restored=1' in navigation_cold and 'continued_search_store_move_to_goal=1' in navigation_cold
            for name,digest in navigation_digests.items(): assert hashlib.sha256((cold/'saves'/name).read_bytes()).hexdigest()==digest
            result['npc_navigation'].update(status='VERIFIED',cold=navigation_cold,checkpoint_sha256=navigation_digests)
        if args.npc_route_follow:
            route_follow_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-npc-route-follow\t'))
            assert 'actual_result_restored=1' in route_follow_cold and 'continued_to_goal=1' in route_follow_cold
            for name,digest in route_follow_digests.items(): assert hashlib.sha256((cold/'saves'/name).read_bytes()).hexdigest()==digest
            result['npc_route_follow'].update(status='VERIFIED',cold=route_follow_cold,checkpoint_sha256=route_follow_digests)
        if args.npc_route_store:
            route_store_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-npc-route-store\t'))
            assert 'actual_result_restored=1' in route_store_cold and 'continued_allocations=4' in route_store_cold
            for name,digest in route_store_digests.items(): assert hashlib.sha256((cold/'saves'/name).read_bytes()).hexdigest()==digest
            result['npc_route_store'].update(status='VERIFIED',cold=route_store_cold,checkpoint_sha256=route_store_digests)
        if args.owned_path:
            owned_path_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-owned-path\t'))
            assert 'actual_result_restored=1' in owned_path_cold and 'continued_search=1' in owned_path_cold and 'legal_route_to_authored_goal=1' in owned_path_cold
            for name,digest in owned_path_digests.items(): assert hashlib.sha256((cold/'saves'/name).read_bytes()).hexdigest()==digest
            result['owned_path'].update(status='VERIFIED',cold=owned_path_cold,checkpoint_sha256=owned_path_digests)
        cold_line=next(line for line in cold_trace.splitlines() if line.startswith('native-world\t'))
        assert 'phase=cold' in cold_line and 'native_open_retained=1' in cold_line
        assert hashlib.sha256(copied.read_bytes()).hexdigest()==expected_digest
        result['world']=dict(status='VERIFIED',scope='Native World operations/serialization only; no ordinary input/rendered gameplay claim',
            warm=warm,cold=cold_line,cold_argv=cold_command,cold_exit=process.returncode,
            input_state_sha256=expected_digest,controlled_initialization='Source-valid world profiles and authorization BEFORE native operations; no earned Search/key award or full party initialization claim')
        position_warm=next(line for line in report.splitlines() if line.startswith('native-position\t'))
        position_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-position\t'))
        assert 'actual_open_then_move=1' in position_warm and 'complete_turn=0' in position_warm
        assert 'open_and_position_retained=1' in position_cold and 'continued_move=1' in position_cold
        assert hashlib.sha256(position_copy.read_bytes()).hexdigest()==position_digest
        result['position']=dict(status='VERIFIED',warm=position_warm,cold=position_cold,input_state_sha256=position_digest,
            scope='Linked Open/collision/position phases with resolved occupancy supplied explicitly; actor/visibility, clock, camera, encounters and complete player turn excluded',
            controlled_initialization='Map54,type7,position38/15,keyfield24=4 BEFORE native Open; destination visible object slot32 and actor absence are explicit resolved phase inputs; no outcome injected')
        result['actual_compiled_native_units']+=['native_position_validation.h','native_traversal.h']
        party_warm=next(line for line in report.splitlines() if line.startswith('native-party\t'))
        party_cold=next(line for line in cold_trace.splitlines() if line.startswith('native-party\t'))
        assert 'damage_continued=1' in party_cold and 'atomic_load_refusal=1' in party_warm
        assert hashlib.sha256(party_copy.read_bytes()).hexdigest()==party_digest
        result['party']=dict(status='VERIFIED',warm=party_warm,cold=party_cold,input_state_sha256=party_digest,
            scope='Owned initial character/equipment records and damage/persistence; full new-game transition, recruitment, inventory readiness and ordinary command loop incomplete')
        if args.party:
            creation=next(line for line in report.splitlines() if line.startswith('native-party-creation\t'))
            assert 'cases=105' in creation and 'full_records=864' in creation
            result['party']['creation']=creation
            result['party']['oracle_sha256']={name:hashlib.sha256((args.party/name).read_bytes()).hexdigest() for name in ['metadata.json','input.bin']}
        result['actual_compiled_native_units']+=['native_party.h','native_party_validation.h']
        if args.rng:
            warm_rng=next(line for line in report.splitlines() if line.startswith('native-rng-state\t'))
            cold_rng=next(line for line in cold_trace.splitlines() if line.startswith('native-rng-state\t'))
            assert 'native_saved=1' in warm_rng and 'original_continuation_draws=16' in cold_rng
            assert hashlib.sha256(rng_copied.read_bytes()).hexdigest()==rng_digest
            result['rng']['persistence']=dict(status='VERIFIED',warm=warm_rng,cold=cold_rng,input_state_sha256=rng_digest,
                scope='Actual fresh CE process loads native14-byte RNG owner and continues the remaining16 original stream draws; full native game save excluded')
        if args.page_search:
            warm_quest=next(line for line in report.splitlines() if line.startswith('native-quest-checkpoint\t'))
            cold_quest=next(line for line in cold_trace.splitlines() if line.startswith('native-quest-checkpoint\t'))
            assert 'phase=warm' in warm_quest and 'phase=cold' in cold_quest
            assert 'native-search-dialogue-link\t' in cold_trace
            assert hashlib.sha256(quest_copied.read_bytes()).hexdigest()==quest_digest
            result['quest_checkpoint']=dict(status='VERIFIED',warm=warm_quest,cold=cold_quest,checkpoint_sha256=quest_digest,
                scope='Actual fresh CE process restores native World/party/inventory/RNG/Search owners after seven native Search awards, then executes Axea/Valess/Nystul using restored prerequisites; complete game save and normal gameplay remain unfinished')
            result['actual_compiled_native_units']+=['native_quest_checkpoint.h']
        if args.book and args.page_search:
            cold_book=next(line for line in cold_trace.splitlines() if line.startswith('native-bound-book\t'))
            assert 'phase=cold' in cold_book and 'actual_assignment_restored=1' in cold_book and 'native-bound-cast\t' in cold_trace
            assert hashlib.sha256(book_copied.read_bytes()).hexdigest()==book_digest
            result['book_persistence']=dict(status='VERIFIED',cold=cold_book,checkpoint_sha256=book_digest,
                scope='Actual fresh CE loads the saved native Vortex shortcut/requirement assigned by native Ready; loaded assignment supplies real world refusal and battle preparation. Complete game save and actual effects remain unfinished.')
        result['actual_compiled_native_units']+=['native_world.h','native_world_validation.h']
        result['artifacts']={str(p.relative_to(args.artifacts)):hashlib.sha256(p.read_bytes()).hexdigest()
                             for p in [args.artifacts/'native-core-warm.tsv',copied,position_copy,party_copy,cold/'saves/nox-resource-validation.tsv',cold/'cold.ini']+([rng_copied] if args.rng else [])+([quest_copied] if args.page_search else [])+([book_copied] if args.book and args.page_search else [])+([vortex_copied] if args.vortex_cast else [])}
    output=args.artifacts/'native-core-verification.json'
    output.write_text(json.dumps(result,indent=2)+'\n')
    print('VERIFIED native semantic units:',output)


if __name__=='__main__':main()
