#!/usr/bin/env python3
"""Focused save-preview/gallery checks using copied saves and SDL dummy devices."""
import argparse, json, shutil, struct, threading, zlib
from pathlib import Path
from test_snatcher_integration import Client, sha

p=argparse.ArgumentParser()
for name in ('exe','game','sidecar','snapshot','checkpoint','output'):
    p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();a.output.mkdir()
root=a.output/'profile'

def launch(c):
    c.command('launch',engine='snatcher',game='snatcher',platform='segacd',path=str(a.game))
    c.command('wait',field='adapter.ready',equals=True,timeout_ms=30000)
def listings(c):return {s['chooser_slot']:s for s in c.command('saves')['saves']}
def has_thumb(entry):return entry['thumbnail_width']==160 and entry['thumbnail_height'] in (100,120)
def tap(c,code):
    for phase in ('down','up'):
        c.command('input',kind='key',key=code,phase=phase,map_physical=True)
        c.command('advance',frames=2)
def preview(data):
    assert data[:4]==b'SNSV'
    assert struct.unpack('>I',data[8:12])[0]==len(data)
    assert zlib.crc32(data[:-4])==struct.unpack('>I',data[-4:])[0]
    at=80
    for _ in range(struct.unpack('>H',data[12:14])[0]):
        ident,version,flags,reserved,size=struct.unpack('>HHHHI',data[at:at+12]);at+=12
        if ident==11:
            assert version==1 and flags==0
            return at,data[at:at+size]
        at+=size
    raise AssertionError('Missing optional thumbnail section')
def verify_pixels(blob):
    assert blob[:4]==b'THMB' and blob[8]==2
    size=struct.unpack('>I',blob[4:8])[0];w,h=struct.unpack('>HH',blob[9:13])
    assert size==len(blob)==22+w*h*2 and w==160 and h in (100,120)
    assert len(set(struct.unpack('>'+str(w*h)+'H',blob[22:])))>8,'Blank/placeholder preview'
    return w,h

c=Client(a.exe,root,a.game,'gens')
c.watch.cancel();c.watch=threading.Timer(300,c.p.kill);c.watch.start()
try:
    cfg=root/'profile.ini'
    cfg.write_text(cfg.read_text().replace('autosave_period=1','autosave_period=300')
        .replace('snatcher_decensor=false','snatcher_decensor=true')
        .replace('[scummvm]','[scummvm]\nscale_factor=3\nscaler=normal\ngui_scale=100\ngui_saveload_chooser=grid\nextrapath='+str(a.sidecar.parent)))
    saves=root/'saves';saves.mkdir(exist_ok=True)
    shutil.copy2(a.snapshot,saves/'cli-game.065');shutil.copy2(a.checkpoint,saves/'cli-game.001')
    protected={n:sha(saves/n) for n in ('cli-game.065','cli-game.001')}
    launch(c);c.command('load',slot=65);c.command('advance',frames=60)
    assert listings(c)[65]['thumbnail_width']==0 and listings(c)[1]['thumbnail_width']==0
    c.command('save',slot=5,description='Thumbnail first generation')
    first=(saves/'cli-game.005').read_bytes();offset,blob=preview(first);verify_pixels(blob)
    (a.output/'manual.thmb').write_bytes(blob)
    assert has_thumb(listings(c)[5])
    policy=c.command('input',kind='logical',action='save_policy')
    assert policy['thumbnail_supported'] and policy['gallery_selected'],policy
    # Reach the real F7 chooser with gallery selected, then dismiss it.
    c.command('input',kind='key',key=288,phase='down',map_physical=True)
    c.command('wait',field='adapter.load_dialog_open',equals=True,timeout_ms=5000)
    c.command('input',kind='key',key=288,phase='up',map_physical=True)
    tap(c,27)
    c.command('wait',field='adapter.load_dialog_open',equals=False,timeout_ms=5000)
    c.command('input',kind='logical',action='autosave_now')
    assert has_thumb(listings(c)[0]);verify_pixels(preview((saves/'cli-game.000').read_bytes())[1])
    c.command('save',slot=5,description='Thumbnail second generation')
    assert (saves/'cli-game.005.bak').read_bytes()==first
    c.command('load',slot=5)
    expected=c.command('inspect')['adapter']
    # Bad preview dimensions with a valid envelope must hide the image, not
    # compromise the gameplay save or allocate unbounded image memory.
    malformed=bytearray(first);malformed[offset+9:offset+11]=b'\xff\xff'
    malformed[-4:]=struct.pack('>I',zlib.crc32(malformed[:-4]))
    (saves/'cli-game.099').write_bytes(malformed)
    # A corrupt primary must display the corresponding valid backup preview.
    (saves/'cli-game.005').write_bytes(b'corrupt primary')
    assert has_thumb(listings(c)[5])
    c.command('load',slot=5)
    for n,digest in protected.items():assert sha(saves/n)==digest
    # One actual authored internal checkpoint save, not an injected save outcome.
    c.command('load',slot=1)
    c.command('wait',field='adapter.original_resume_pending',equals=False,timeout_ms=15000)
    def ready():
        for _ in range(160):
            s=c.command('inspect')['adapter']
            assert not any(s.get(k) for k in ('blocker','runtime_error','render_error','audio_error')),s
            if s.get('command_ready') and s.get('mouse_ready') and s.get('choice_references'):return s
            if s.get('dialogue_waiting') and s.get('runtime_status')==4:tap(c,13)
            c.command('advance',frames=16)
        raise AssertionError('Checkpoint menu did not become ready')
    def pick(reference):
        s=ready();refs=s['choice_references'];assert reference in refs,(reference,refs)
        current=refs.index(s['command_reference'])
        for _ in range((refs.index(reference)-current)%len(refs)):tap(c,274)
        tap(c,13);c.command('advance',frames=16);return ready()
    assert ready()['resource']==67
    pick(551);choices=pick(430)['choice_references'];pick(choices[1])
    internal=(saves/'cli-game.002').read_bytes();assert internal[:4]==b'SGR2'
    assert struct.unpack('>H',internal[4:6])[0]==len(internal)-10
    assert zlib.crc32(internal[10:])==struct.unpack('>I',internal[6:10])[0]
    marker=internal.index(b'SNTH');length=struct.unpack('>I',internal[marker+4:marker+8])[0]
    internal_blob=internal[marker+8:];assert len(internal_blob)==length;verify_pixels(internal_blob)
    assert has_thumb(listings(c)[2]);c.command('load',slot=2)
    c.command('wait',field='adapter.original_resume_pending',equals=False,timeout_ms=15000)
    assert ready()['resource']==67
finally:c.close()

c=Client(a.exe,root,a.game,'gens',reuse=True)
try:
    launch(c)
    assert listings(c)[99]['thumbnail_width']==0
    c.command('load',slot=99)
    assert has_thumb(listings(c)[5]) and has_thumb(listings(c)[2]) and has_thumb(listings(c)[0])
    c.command('load',slot=5)
    restored=c.command('inspect')['adapter']
    for key in ('resource','node','packed_digest'):assert restored[key]==expected[key],key
    c.command('load',slot=2)
    c.command('wait',field='adapter.original_resume_pending',equals=False,timeout_ms=15000)
    for n,digest in protected.items():assert sha((root/'saves')/n)==digest
finally:c.close()
(a.output/'receipt.json').write_text(json.dumps(dict(status='PASS',executable_sha256=sha(a.exe),
    manual_thumbnail=True,autosave_thumbnail=True,internal_thumbnail=True,legacy_load=True,
    gallery=True,f7_gallery_dialog=True,backup_thumbnail=True,malformed_preview_ignored=True,
    cold_restore=True,owner_files_used_read_only=True,real_devices=False),indent=2))
print('Save thumbnails, gallery, legacy saves, backup metadata and cold restoration passed')
