"""Supplement explicit-address writes with stack and MMIO context candidates.

Old inventories remain immutable. This is not reachable-context certification.
"""
import collections
import json
from inventory_story08_1135 import ROOT, OUT, MPU, sha, write
from audit_story08_envelope1135 import allocated_files
from inspect_session import files

STACK={0x48:'PHA',0x08:'PHP',0xda:'PHX',0x5a:'PHY',0x20:'JSR',0x00:'BRK'}

def scan(name,data):
    stacks=[];mmio=[];counts=collections.Counter()
    for offset,opcode in enumerate(data):
        mnemonic,mode=MPU.disassemble[opcode]
        if opcode in STACK:
            assert mnemonic==STACK[opcode]
            size=3 if opcode==0x20 else 1
            if offset+size<=len(data):
                counts['stack_'+mnemonic]+=1
                # JSR and BRK already have per-site coordinates in the control
                # envelopes; refer to them instead of duplicating large lists.
                if opcode not in (0x20,0):
                    stacks.append(dict(resource=name,offset=offset,operation=mnemonic,disposition='UNKNOWN',
                                       requirement='Resolve executable/bank/caller, stack value and later use; stack destinations are0100..01ff, not direct quest-page writes.'))
        if mode in ('abs','abx','aby')and offset+3<=len(data):
            operand=int.from_bytes(data[offset+1:offset+3],'little')
            length=255 if mode in ('abx','aby')else 0
            if any(0xc000<=((operand+i)&65535)<0xc100 for i in range(length+1)):
                counts['possible_mmio_touch']+=1
                mmio.append(dict(resource=name,offset=offset,operation=mnemonic,mode=mode,operand=operand,
                                 disposition='UNKNOWN',requirement='A soft-switch read can change bank/zero-page/IO context. Resolve reachability, exact effective address and downstream mutation/continuation.'))
    return stacks,mmio,dict(counts)

def main():
    image=(ROOT.parent/'Nox Archaist HDV (v119).HDV').read_bytes()
    payloads=files(ROOT.parent/'Nox Archaist HDV (v119).HDV')
    rounded,_=allocated_files(image)
    rows=[];summaries={}
    for label,resources in [('payload',payloads),('rounded',rounded),('raw',{'RAW_BASE_HDV':image})]:
        for name,raw in sorted(resources.items()):
            stacks,mmio,counts=scan(name,raw)
            rows.extend(dict(envelope=label,**r)for r in stacks+mmio)
            summaries[label+':'+name]=counts
    result=dict(status='VERIFIED',scope='Implicit stack/MMIO candidate supplement; no global original or native equivalence.',
                explicit_addressed_memory_write_opcodes=62,implicit_stack_write_opcodes=STACK,
                documented_instruction_memory_write_opcode_total=68,
                count_wording_correction='The earlier62-opcode audit covers explicit addressed memory stores/RMW, not all memory-writing instructions. Six implicit stack-writing opcodes are accounted here; JSR/BRK coordinates already exist in the control envelopes.',
                asynchronous_contexts='IRQ/NMI/RESET, conditional65816/ROM and generated instructions remain explicit UNKNOWN U03/U04/U05; not silently counted as original payload instructions.',
                resource_summary=summaries,artifacts=dict(context_candidates=write('stack-mmio-contexts.local.json',rows)),
                candidate_rows=len(rows),source_image_sha256=sha(image),native_execution=False,builds=0,engine_launches=0)
    write('stack-mmio-audit.json',result)
    print(json.dumps(dict(explicit_addressed_write_opcodes=62,implicit_stack_write_opcodes=6,total_memory_write_opcodes=68,context_candidate_rows=len(rows),engine_launches=0),indent=2))

if __name__=='__main__':
    main()
