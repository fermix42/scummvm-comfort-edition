#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Strict observer classification boundaries; not application gameplay tests."""
from pathlib import Path
import unittest
from storage_observation import system_resource_kind, classify_notifications


class ObservationTests(unittest.TestCase):
    def test_direct3d_cache_and_journal_variants(self):
        base = 'AppData/Local/D3DSCache/129f7da9ce6d140/6F75932F-7DFC-4FB0-B4B8-12DE1AC415DA_VEN_10DE&DEV_2684&SUBSYS_5107&REV_A1.dxcache'
        for suffix in ('', '-journal', '-shm', '-wal'):
            self.assertEqual(system_resource_kind(base+suffix), 'windows_direct3d_shader_cache')

    def test_sapi_shared_resource_is_exact(self):
        self.assertEqual(system_resource_kind('AppData/Roaming/Microsoft/Speech/Files/UserLexicons/SP_9392584B205B4597A752C2E083593D9D.dat'), 'windows_sapi_shared_user_lexicon')
        self.assertIsNone(system_resource_kind('AppData/Roaming/Microsoft/Speech/Files/UserLexicons/ce-achievements-queue.json'))

    def test_application_profile_files_never_become_system_resources(self):
        for name in ('AppData/Roaming/ScummVM/scummvm.ini', 'AppData/Local/ScummVM/saves/efmi001.gsv',
                     'AppData/Local/D3DSCache/1234/ce-playthroughs.json', 'Documents/save.001'):
            self.assertIsNone(system_resource_kind(name))

    def test_unknown_notifications_remain_failures(self):
        profile = Path.cwd()/'disposable-profile'
        event = {'action': 1, 'path': str(profile/'AppData/Roaming/ScummVM/scummvm.ini')}
        known, unknown = classify_notifications([event], profile)
        self.assertEqual(known, [])
        self.assertEqual(unknown, [event])

    def test_known_resource_ancestors_are_limited_to_directory_changes(self):
        profile = Path.cwd()/'disposable-profile'
        parent = profile/'AppData/Local/D3DSCache/1234'
        file = parent/'F4EB2D6C-ED2B-4BDD-AD9D-F913287E6768.dxcache'
        events = [{'action': 1, 'path': str(parent)}, {'action': 3, 'path': str(file)}]
        known, unknown = classify_notifications(events, profile)
        self.assertEqual(len(known), 1)
        self.assertEqual(unknown, [])
        unknown_event = {'action': 2, 'path': str(parent)}
        self.assertEqual(classify_notifications(events+[unknown_event], profile)[1], [unknown_event])


if __name__ == '__main__':
    unittest.main(verbosity=2)
