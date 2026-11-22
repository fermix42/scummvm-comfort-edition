#!/usr/bin/env python3
"""F12 evidence persistence, failure reporting and cold snapshot recovery."""
import argparse
import json
from pathlib import Path
import shutil
from test_snatcher_integration import Client, sha

def main():
    parser = argparse.ArgumentParser()
    for key in ('exe', 'game', 'checkpoint', 'resources', 'output'):
        parser.add_argument('--'+key, type=Path, required=True)
    a = parser.parse_args()
    a.output = a.output.resolve()
    a.output.mkdir()
    root = a.output/'profile'
    c = Client(a.exe, root, a.game, 'gens')
    alive = True
    try:
        p = root/'profile.ini'
        p.write_text(p.read_text().replace('autosave_period=1', 'autosave_period=0')
            .replace('snatcher_decensor=false', 'snatcher_decensor=true')
            .replace('[scummvm]', '[scummvm]\nextrapath='+str(a.resources)))
        saves = root/'saves'
        saves.mkdir(exist_ok=True)
        shutil.copyfile(a.checkpoint, saves/'cli-game.065')
        c.command('launch', engine='snatcher', game='snatcher', platform='segacd', path=str(a.game))
        c.command('wait', field='adapter.ready', equals=True, timeout_ms=30000)
        c.command('load', slot=65)
        c.command('advance', frames=120)
        c.command('save', slot=5, description='Existing save must survive F12')
        protected = sha(saves/'cli-game.005')
        before = c.command('inspect')['adapter']
        def f12(phase):
            c.command('input', kind='key', key=293, phase=phase, map_physical=True)
            c.command('advance', frames=2)
        f12('down')
        first = c.command('inspect')['adapter']
        assert first['investigation_complete'], first['investigation_status']
        assert 'safe to restart' in first['investigation_status']
        stem = first['investigation_capture']
        report = json.loads((saves/(stem+'.json')).read_text())
        result = json.loads((saves/(stem+'.result.json')).read_text())
        assert result['snapshot_validated'] and not result['error']
        assert report['history'] and len(report['history']) <= 1800
        assert report['format'] == 'snatcher-investigation/3'
        stages = ('lock', 'prepare', 'scale', 'upload', 'render', 'present', 'metadata', 'other')
        graphics = [sample['graphics'] for sample in report['history']]
        assert all(g['routine_gl_debug_polling'] is False for g in graphics)
        assert all(g['frames'] > 0 and g['backend'] == 'surface-sdl' for g in graphics)
        roles = ('game', 'cursor', 'cursor_mask', 'overlay', 'osd_message', 'osd_icon', 'unattributed')
        operations = ('update', 'convert_scale', 'edge_prepare', 'gl_bind_texture', 'gl_pixel_store',
                      'gl_tex_sub_image', 'gl_tex_image', 'gl_error_clear', 'gl_error_check')
        # Dummy video uses surface SDL, so GL-specific metrics must remain empty.
        for g in graphics:
            assert set(g['textures']) == set(roles)
            for texture in g['textures'].values():
                assert texture['uploads'] == 0 and texture['uploaded_bytes'] == 0
                assert 'last_upload' not in texture and 'slowest_upload' not in texture
                assert all(texture[op] == dict(max_ms=0, total_ms=0, calls=0) for op in operations)

        assert any(g['renderer'] == 'software' and g['video_driver'] == 'dummy'
                   and g['drawable_width'] > 0 and g['drawable_height'] > 0 for g in graphics)
        assert any(g['upload']['calls'] > 0 and g['render']['calls'] > 0
                   and g['present']['calls'] > 0 for g in graphics)
        for g in graphics:
            for stage in stages:
                timing = g[stage]
                assert 0 <= timing['max_ms'] <= timing['total_ms']
                assert timing['calls'] > 0 or timing['total_ms'] == 0
        assert any(s['process_metrics_known'] and s['private_bytes'] > 0 for s in report['history'])
        assert report['engine']['trace']
        f12('down')  # Holding F12 must not generate another capture.
        assert c.command('inspect')['adapter']['investigation_capture'] == stem
        f12('up')
        f12('down')
        second = c.command('inspect')['adapter']
        assert second['investigation_complete'] and second['investigation_capture'] != stem
        f12('up')
        assert sha(saves/'cli-game.005') == protected
        assert c.command('inspect')['adapter']['packed_digest'] == before['packed_digest']
        # A naturally unsaveable diagnostic fixture must still produce a report,
        # but must never display the successful completion message.
        c.command('input', kind='logical', action='scene_layout_fixture', preset=0)
        f12('down')
        failed = c.command('inspect')['adapter']
        assert not failed['investigation_complete'] and 'SAVE FAILED' in failed['investigation_status']
        failed_stem = failed['investigation_capture']
        assert (saves/(failed_stem+'.json')).exists()
        assert not json.loads((saves/(failed_stem+'.result.json')).read_text())['snapshot_validated']
        # Terminate without a graceful game quit, then recover the finalized
        # snapshot in a fresh process. This simulates process loss, not power loss.
        c.p.kill(); c.p.wait(timeout=15); c.watch.cancel(); c.stderr.close(); alive = False
        (a.output/'capture-transcript.json').write_text(json.dumps(c.records, indent=2))
        shutil.copyfile(saves/(stem+'.save'), saves/'cli-game.080')
        c = Client(a.exe, root, a.game, 'gens', reuse=True)
        alive = True
        c.command('launch', engine='snatcher', game='snatcher', platform='segacd', path=str(a.game))
        c.command('wait', field='adapter.ready', equals=True, timeout_ms=30000)
        c.command('load', slot=80)
        restored = c.command('inspect')['adapter']
        for field in ('packed_digest', 'resource', 'node', 'entry_active', 'entry_grid_index'):
            assert restored[field] == report['engine'][field], field
        assert sha(saves/'cli-game.005') == protected
        (a.output/'results.json').write_text(json.dumps(dict(exe_sha256=sha(a.exe),
            capture=stem, duplicate_suppressed=True, unique_capture=True,
            failure_reported=True, cold_recovery=True, existing_save_preserved=True, graphics_timings=True, per_texture_schema=True, routine_gl_polling_disabled=True), indent=2))
        print('F12 capture, held-key suppression, failure report, and cold recovery passed')
    finally:
        if alive:
            c.close()

if __name__ == '__main__':
    main()
