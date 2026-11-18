"""Rebind retained authored census rows to immutable v119 input bytes.

No historical census is rewritten and no original/native suite is rerun.
"""
import collections
import json
import sys
from inventory_story08_1135 import ROOT, OUT, sha, write
from inspect_session import files

def main():
    resources=files(ROOT.parent/'Nox Archaist HDV (v119).HDV')
    catalog=json.loads((ROOT/'ce-m3-dialogue-census29-local.json').read_text())
    effects=json.loads((ROOT/'ce-native-dialogue-effects500-local.json').read_text())
    code=(ROOT/'census_dialogue29_m3.py').read_text()
    ns={}
    # Import only the pure function, avoiding the historical module's output
    # writers. This uses the same hash-bound decompressor, not invented text.
    exec(code[code.index('def decode(encoded):'):code.index('\nold={}')],ns)
    mutations=[];malformed=[];headers=0
    for record in catalog['records']:
        raw=resources[record['resource']]
        assert sha(raw)==record['resource_sha256']
        text=ns['decode'](raw[record['packet_offset']:record['packet_offset']+record['encoded_length']])
        assert sha(text)==record['decoded_sha256']
        cursor=0
        while cursor<len(text):
            start=text.find(b'\xa4',cursor)
            if start<0:
                break
            if text[start:start+2]not in (b'\xa4\xa1',b'\xa4\xc9'):
                cursor=start+1;continue
            end=text.find(b'\xae',start);zero=text.find(b'\0',end)
            if end<0 or zero<0:
                break
            header=text[start:end];cursor=zero+1;headers+=1
            for tag in (177,178):
                position=0
                while True:
                    position=header.find(bytes([164,208,tag]),position)
                    if position<0:
                        break
                    base=dict(resource=record['resource'],actor=record['actor'],header_offset=start,kind=tag)
                    if position+5>len(header):
                        malformed.append(base);break
                    field,value=header[position+3:position+5]
                    mutations.append(dict(base,field=field,value=value));position+=5
    assert mutations==effects['records']and malformed==effects['malformed_parameter_headers']
    assert headers==effects['response_headers']
    checked_resource_bindings=0
    for name,key in [('ce-native-producers494-local.json','resources_sha256'),('ce-native-scripted-contacts502-local.json','source_sha256')]:
        old=json.loads((ROOT/name).read_text())
        for resource,h in old[key].items():
            assert sha(resources[resource])==h
            checked_resource_bindings+=1
    dependencies={}
    for relative in ['inspect_session.py','census_dialogue29_m3.py','local-python/py65/devices/mpu65c02.py','local-python/py65/devices/mpu6502.py',
                     'local-python/py65-1.2.0.dist-info/METADATA','ce-m3-dialogue-census29-local.json',
                     'ce-native-dialogue-effects500-local.json','ce-native-producers494-local.json','ce-native-scripted-contacts502-local.json']:
        dependencies[relative]=sha((ROOT/relative).read_bytes())
    tails=catalog['unparsed_tails']
    write('retained-dialogue-tails.local.json',tails)
    result=dict(status='VERIFIED',scope='Retained valid-packet mutations and resource input hashes match immutable v119; not a proof that malformed/trailing bytes are unselectable.',
                valid_packets=len(catalog['records']),decoded_headers=headers,complete_effects=len(mutations),malformed_parameter_tails=len(malformed),
                unparsed_resource_tails=len(tails),unparsed_resource_tail_bytes=sum(t['length']for t in tails),
                tail_disposition='UNKNOWN until original selector termination/entry and retained-buffer bounds are established; do not mistake a parser stopping for original unreachability.',
                checked_resource_bindings=checked_resource_bindings,dependencies_sha256=dependencies,python_version=sys.version,
                files_rewritten=0,engine_launches=0,builds=0)
    write('retained-input-validation.json',result)
    print(json.dumps({k:v for k,v in result.items()if k not in ['dependencies_sha256','python_version']},indent=2))

if __name__=='__main__':
    main()
