"""Resolve the two computed expressions in the reference call index.

Tests original bounded selectors only. Does not certify that indirect writes
cannot alter their operands in other contexts, nor execute their destinations.
"""
import json
from inventory_story08_1135 import ROOT, OUT, MPU, sha, write
from inspect_session import files

def main():
    resources=files(ROOT.parent/'Nox Archaist HDV (v119).HDV')
    movements=[];menus=[]
    for entry,target in [(0x92d8,0xed5b),(0x92ea,0xee20),(0x92fc,0xeedc),(0x930e,0xef91)]:
        cpu=MPU();cpu.memory[0x6000:0x9600]=resources['NA/GAME'];cpu.pc=entry
        before=bytes(cpu.memory[0x400:0x500]);steps=0
        while cpu.pc!=target:
            cpu.step();steps+=1;assert steps<20,(entry,hex(cpu.pc))
        assert cpu.memory[0x928f]|cpu.memory[0x9290]<<8==target
        assert bytes(cpu.memory[0x400:0x500])==before
        movements.append(dict(entry=entry,patched_target=target,steps=steps,story_page_unchanged=True))
    targets={1:0xa0ee,2:0xa3c6,3:0xa38a,4:0xa3b2,5:0xa39e,6:0xa2ea}
    destinations=set(targets.values())|{0x14a7}
    loader=resources['NA/LOADER.P']
    for selector in range(256):
        cpu=MPU();cpu.memory[0x9600:0x9600+len(loader)]=loader;cpu.pc=0xa09f;cpu.a=selector
        before=bytes(cpu.memory[0x400:0x500]);steps=0
        while cpu.pc not in destinations:
            cpu.step();steps+=1;assert steps<32,(selector,hex(cpu.pc))
        assert cpu.pc==targets.get(selector,0x14a7)
        assert bytes(cpu.memory[0x400:0x500])==before
        menus.append(dict(selector=selector,destination=cpu.pc,steps=steps,story_page_unchanged=True))
    candidates=json.loads((OUT/'writer-envelope.local.json').read_text())
    patch_stores=[dict(resource=r['resource'],offset=r['offset'],operation=r['operation'],operand=r['operand'])for r in candidates
                  if r['mode']=='abs'and r['operand']in (0x928f,0x9290)]
    assert len(patch_stores)==8 and all(r['resource']=='NA/GAME'and r['operation']=='STA'for r in patch_stores)
    result=dict(status='VERIFIED',scope='Bounded original movement patch and menu selector destinations only; no destination effect/native/global caller closure.',
                resources_sha256={n:sha(resources[n])for n in ['NA/GAME','NA/LOADER.P']},
                movement_setters=movements,menu_selectors=menus,static_direct_patch_candidates=patch_stores,
                source_cases=260,original_instruction_steps=sum(c['steps']for c in movements+menus),
                interpretation='Reference pointer(928F) is a four-setter patched movement call; entry[3] expands a fixed six-way loader menu selector, not an unconstrained runtime pointer. Invalid raw selector values lead to original14A7. Their descendants and all possible indirect operand writers remain separately qualified.',
                context_collision_example='PC A3B2 is a loader menu branch in menu_extras.inc and a different inventory branch in inventory_lists.inc; physical context is required.',
                remaining='Prove complete original incoming/operand-writer sets and the descendants/native effects, not just these bounded selector results.',
                native_execution=False,engine_launches=0,builds=0)
    write('original-dispatch-probes.json',result)
    print(json.dumps(dict(original_cases=260,steps=result['original_instruction_steps'],movement_targets=4,valid_menu_targets=6,invalid_selectors=250,engine_launches=0),indent=2))

if __name__=='__main__':
    main()
