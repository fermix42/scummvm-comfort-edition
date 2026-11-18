"""Index known call sites, authored instances and unresolved context boundaries.

This deliberately does not turn unbanked PC matches into a verified call graph.
"""
import collections
import json
import re
from inventory_story08_1135 import ROOT, OUT, CE, sha, write
from inspect_session import files

def authored_instances(resources):
    result=[];maps=0;counts=collections.Counter()
    groups={18:'key-door',19:'compound-door',54:'pressure-plate',56:'portal',48:'control',49:'control',55:'notice-or-speech',53:'search'}
    for family in ('SURF','UM','RUIN','TOWN2','TOWN','CSTL','KEEP'):
        resource='NA/DATA.MAP.'+family;raw=resources[resource];cursor=2 if family=='SURF'else 1
        while raw[cursor]!=255:
            maps+=1;ident=raw[cursor+1];offset=int.from_bytes(raw[cursor+7:cursor+10],'little')
            tables=raw[offset:offset+1792];assert len(tables)==1792
            for slot in range(0,256,4):
                row=tables[1024+slot:1028+slot]
                if not row[2] or row[2]==255:
                    continue
                operation=groups.get(row[3]&191)
                if operation:
                    counts[operation]+=1
                    result.append(dict(resource=resource,map=ident,offset=offset+1024+slot,slot=slot,mechanism=operation,
                                       disposition='UNKNOWN',
                                       native_counterpart={'key-door':'WorldOpen/World.open/keyDoor','compound-door':'WorldOpen/World.open/compoundDoor',
                                       'pressure-plate':'World stationary/move completion/World.activatePlate','portal':'WorldPortal.prepare/WorldTravel',
                                       'control':'WorldOpen.run/World.open','notice-or-speech':'WorldNotice/PraevocSpeechRules','search':'SearchInput/World.search/searchQuestReward'}[operation],
                                       requirement='Operator source/evidence reviewed by family; exact record width, entry/activation, aliases and complete dynamic producer/caller context require separate proof. No individual gameplay PASS inferred.'))
            cursor+=raw[cursor] or 256
    assert maps==76
    # Preserve the distinction between a syntactic record candidate and a
    # source-qualified executable object; the four-byte scanning can overlap.
    effects=json.loads((ROOT/'ce-native-dialogue-effects500-local.json').read_text())
    for i,mutation in enumerate(effects['records']):
        result.append(dict(id='dialogue-'+str(i),**mutation,mechanism='dialogue-assignment'if mutation['kind']==177 else'dialogue-bit-effect',
                           disposition='VERIFIED equivalent'if mutation['field']<80 else'UNKNOWN',
                           native_counterpart='parseDialogueResponse/selectDialogueResponse/commitDialogueFields/Conversation/World.acceptConversation',
                           scope='Bounded decoded authored-header operation and capacities; not proof of every caller or execution context.',
                           requirement='For derived lookup alias, establish prototype/context and refresh ordering before claiming whole-context equivalence.'if mutation['field']>=80 else'Global caller closure remains in separate UNKNOWN mechanisms.'))
    contacts=json.loads((ROOT/'ce-native-scripted-contacts502-local.json').read_text())
    for name in ('fixed_predicates','special_contacts'):
        for i,record in enumerate(contacts[name]):
            result.append(dict(id=name+'-'+str(i),**record,mechanism=name,
                               disposition='VERIFIED equivalent',scope='Initial authored encoding/typed operator only; arbitrary dynamic activation is not included.',
                               native_counterpart='Actors/BattleWorldReturn.effects/BattleAftermath/BattleLoot/retireWorldContact',
                               requirement='Dynamic or imported/generated definitions and complete activation callers remain UNKNOWN separately.'))
    for record in json.loads((ROOT/'ce-native-producers494-local.json').read_text())['story_rules']:
        result.append(dict(**record,mechanism='map-patch-rule',disposition='VERIFIED equivalent',
                           native_counterpart='WorldInput/WorldViewport/WorldStory.apply/StoryMapPatches.apply',
                           scope='The11 authored rules/four effect kinds; earlier whole-operation original/native and current source review. Global bank-entry/persistence semantics remain separate.',
                           requirement='All code callers and temporal interactions remain in separate UNKNOWN context rows.'))
    definitions=resources['NA/SRTN.INVENTORY']
    for i in range(168):
        row=definitions[0x2c00+i*32:0x2c20+i*32]
        if row[8]&32:
            result.append(dict(resource='NA/SRTN.INVENTORY',offset=0x2c00+i*32,item=i,counter_field=row[7],
                               mechanism='misc-counter-definition',disposition='VERIFIED equivalent',
                               native_counterpart='searchQuestReward/BattleLoot/Inventory counter-context policies',
                               scope='Two original counted misc definitions; caller-retained quantity/BCD mode and all lifetime edges remain separate.',
                               requirement='No new broad commerce-lifetime equivalence is inferred.'))
    return result,dict(counts)

def reference_calls():
    rows=[];case_index=collections.defaultdict(list);definitions=[]
    for path in sorted((ROOT/'native').glob('*.inc'))+sorted((ROOT/'native').glob('*.cpp')):
        pc=None;lines=path.read_text(encoding='utf-8-sig').splitlines()
        for number,line in enumerate(lines,1):
            case=re.match(r'\s*case\s+(0x[0-9a-f]+)\s*:',line)
            if case:
                pc=int(case[1],16);case_index[pc].append(dict(path=str(path.relative_to(ROOT)),line=number))
            for m in re.finditer(r'\b(call|jump)\(([^,)]+)',line):
                if re.search(r'\b(?:void|bool|uint8_t|uint16_t|uint32_t|unsigned|int)\s+(?:call|jump)\s*\(',line):
                    definitions.append(dict(path=str(path.relative_to(ROOT)),line=number,reason='Helper declaration/definition, not a call site.'))
                    continue
                operand=m[2].strip()
                rows.append(dict(path=str(path.relative_to(ROOT)),line=number,case_pc=pc,
                                 kind=m[1],operand=operand,target=int(operand,16)if re.fullmatch('0x[0-9a-f]+',operand)else None))
    for row in rows:
        matches=case_index.get(row['target'],[])
        row['possible_reference_entries']=matches
        row['disposition']='UNKNOWN'
        row['requirement']='Resolve physical bank/overlay, actual call boundary and state preconditions; an address match alone does not identify the original callee.'
    collisions={str(pc):values for pc,values in case_index.items()if len(values)>1}
    return rows,collisions,definitions

def reference_index_audit():
    previous=json.loads((OUT/'reference-writers.local.json').read_text())
    seen={(r['path'],r['line'])for r in previous}
    additions=[]
    for path in sorted((ROOT/'native').glob('*.inc'))+sorted((ROOT/'native').glob('*.cpp')):
        for number,line in enumerate(path.read_text(encoding='utf-8-sig').splitlines(),1):
            if (str(path.relative_to(ROOT)),number)in seen:
                continue
            if re.search(r'(?:\+\+|--)[^;]*\b(?:m|h|aux|zero)\[|\b(?:m|h|aux|zero)\[[^]]+\]\s*(?:\+\+|--)|std::fill(?:_n)?\s*\(',line):
                additions.append(dict(path=str(path.relative_to(ROOT)),line=number,source=line.strip(),
                                      disposition='UNKNOWN',requirement='Reference text is an auxiliary index, not an AST-complete original write inventory. Resolve represented original range and native caller.'))
    return additions

def main():
    resources=files(ROOT.parent/'Nox Archaist HDV (v119).HDV')
    instances,counts=authored_instances(resources)
    calls,collisions,definitions=reference_calls()
    missed=reference_index_audit()
    artifacts=dict(instances=write('authored-instance-matrix.local.json',instances),
                   calls=write('reference-call-index-v2.local.json',calls),
                   excluded_definitions=write('reference-call-definition-exclusions.local.json',definitions),
                   collisions=write('reference-pc-collisions.local.json',collisions),
                   reference_index_additions=write('reference-index-additions.local.json',missed))
    result=dict(status='VERIFIED',scope='Reconciliation index and completeness audit of retained reference coordinates; no global call-chain proof.',
                authored_instances=len(instances),instance_dispositions=dict(collections.Counter(r['disposition']for r in instances)),
                syntactic_object_candidates=counts,call_sites=len(calls),computed_reference_calls=sum(r['target']is None for r in calls),
                colliding_unbanked_pc_values=len(collisions),reference_mutation_index_additions=len(missed),artifacts=artifacts,
                excluded_helper_definitions=len(definitions),prior_trace_summary_sha256=sha((OUT/'trace-summary.json').read_bytes()),
                interpretation='Revision2 excludes four helper definitions formerly counted as calls; original revision retained. The lexical writer index also omitted increment/decrement/fill spellings, retained as additional sites. Raw byte writer scanning is independent. No reference coverage or PC uniqueness claim certifies the executable universe.')
    write('trace-summary-v2.json',result)
    print(json.dumps({k:v for k,v in result.items()if k!='artifacts'},indent=2))

if __name__=='__main__':
    main()
