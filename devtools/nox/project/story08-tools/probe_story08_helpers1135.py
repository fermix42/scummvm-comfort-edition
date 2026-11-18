"""Bounded original copy/fill writer probes; no Engine/gameplay or native claim."""
import json
from inventory_story08_1135 import ROOT, OUT, sha, write, MPU
from inspect_session import files

class ObservedMemory(list):
    def __init__(self):
        super().__init__([0]*65536)
        self.recording=False;self.writes=[];self.pc=0
    def __setitem__(self,address,value):
        if self.recording and isinstance(address,int):
            self.writes.append((self.pc,address,self[address],value))
        return super().__setitem__(address,value)

def word(memory,address,value):
    memory[address]=value&255;memory[address+1]=value>>8

def run(resources,kind,start,source,count):
    memory=ObservedMemory();cpu=MPU(memory=memory)
    memory[0xc00:0x2000]=resources['NA/LWR.MAIN.RTNS']
    # Ordinary opaque input bytes, not quest or reward outcomes.
    for i in range(0x300,0x900):
        memory[i]=(i*37+11)&255
    if kind=='copy':
        word(memory,0xc2,start);word(memory,0xc4,source);word(memory,0xc6,source+count-1)
        entry=0x1441
    else:
        word(memory,0xeb,start);word(memory,0x2ef8,start+count);memory[0x2efa]=0xa5
        entry=0x1473
    cpu.pc=entry;cpu.sp=0xfd;cpu.stPushWord(0x2ff)
    before=bytes(memory);expected=bytearray(before)
    if kind=='copy':
        # Source1441 copies through an inclusive endpoint, in forward order.
        for i in range(count):
            expected[start+i]=expected[source+i]
    else:
        expected[start:start+count]=bytes([0xa5])*count
    memory.recording=True;steps=0
    while cpu.pc!=0x300:
        memory.pc=cpu.pc;cpu.step();steps+=1
        assert steps<12000,(kind,start,source,count,hex(cpu.pc))
    assert bytes(memory[start:start+count])==expected[start:start+count]
    observed=[w for w in memory.writes if 0x300<=w[1]<0x900]
    assert len(observed)==count,(kind,len(observed),count)
    nonstory_overlap = (kind=='copy' and source<start<source+count)
    return dict(kind=kind,entry=entry,destination=start,source=source if kind=='copy'else None,count=count,
                steps=steps,writes=observed,input_state_sha256=sha(before),output_state_sha256=sha(bytes(memory)),
                writes_in_0400_04ff=sum(0x400<=w[1]<0x500 for w in observed),
                forward_overlap=nonstory_overlap,
                native_equivalence='UNKNOWN until every original caller and alias context is mapped to its typed native transaction.')

def main():
    resources=files(ROOT.parent/'Nox Archaist HDV (v119).HDV')
    cases=[]
    for dst,src,n in [(0x400,0x700,1),(0x400,0x700,16),(0x3ff,0x700,18),
                      (0x450,0x700,32),(0x4ff,0x700,2),(0x400,0x600,256),
                      (0x401,0x400,16),(0x400,0x401,16),(0x480,0x400,256)]:
        cases.append(run(resources,'copy',dst,src,n))
    for dst,n in [(0x400,0),(0x400,1),(0x400,16),(0x3ff,18),(0x4ff,2),(0x400,256),(0x400,257)]:
        cases.append(run(resources,'fill',dst,0,n))
    result=dict(status='VERIFIED',scope='Sixteen actual original helper operations and write addresses; no native or complete caller-closure claim.',
                source_resource='NA/LWR.MAIN.RTNS',source_sha256=sha(resources['NA/LWR.MAIN.RTNS']),
                copy_range=dict(offset=0x841,entry=0x1441,end=0x1473),fill_range=dict(offset=0x873,entry=0x1473,end=0x14a7),
                cases=cases, original_instruction_steps=sum(c['steps']for c in cases),
                finding='Copy helper has inclusive source endpoint and forward sequential overlap behavior; fill uses an exclusive end. Both can mutate0400..04ff from caller-derived pointers.',
                remaining='Resolve original caller pointer/value/bank sets, including overlap, before asserting corresponding native transactions are exhaustive equivalents.',
                native_code_changed=False,engine_launches=0,builds=0)
    write('original-helper-probes.json',result)
    print(json.dumps(dict(cases=len(cases),steps=result['original_instruction_steps'],story_page_writes=sum(c['writes_in_0400_04ff']for c in cases),engine_launches=0,native_equivalence='UNKNOWN'),indent=2))

if __name__=='__main__':
    main()
