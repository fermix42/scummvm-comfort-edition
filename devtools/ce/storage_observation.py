#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Narrow classification of observed OS-owned resources, never arbitrary profile writes.

Microsoft documents D3DSCache as a Windows DirectX component and the SAPI user
lexicon as shared among applications. CE's Windows speech backend creates an
ISpVoice; it has no lexicon registration/pronunciation-writing code. See the
consolidated report for primary references and attribution limitations.
"""
from pathlib import Path
import re


def native_io_path(path):
    """Use explicit wide Win32 paths for Python's retained-artifact file I/O."""
    import os
    path = Path(path).resolve()
    prefix = chr(92) * 2 + '?' + chr(92)
    if os.name == 'nt' and not str(path).startswith(prefix):
        return Path(prefix + str(path))
    return path


def system_resource_kind(relative_path):
    path = str(relative_path).replace('\\', '/').lower()
    if (path in ('appdata/local/nvidia', 'appdata/locallow/nvidia') or
            path.startswith(('appdata/local/nvidia/', 'appdata/locallow/nvidia/')) or
            re.fullmatch(r'appdata/local/[0-9a-f]{16}\.nvph', path)):
        return 'nvidia_driver_cache'
    if path in ('appdata/local/microsoft', 'appdata/local/microsoft/windows', 'appdata/local/microsoft/windows/caches'):
        return 'windows_shell_cache_directory'
    if re.fullmatch(r'appdata/local/d3dscache/[0-9a-f]{1,16}/[0-9a-f-]{36}(?:_ven_[0-9a-f]{4}&dev_[0-9a-f]{4}&subsys_[0-9a-f]{4,8}&rev_[0-9a-f]{2})?\.dxcache(?:-journal|-shm|-wal)?', path):
        return 'windows_direct3d_shader_cache'
    if re.fullmatch(r'appdata/roaming/microsoft/speech/files/userlexicons/sp_[0-9a-f]{32}\.dat', path):
        return 'windows_sapi_shared_user_lexicon'
    return None


def classify_notifications(events, profile):
    profile = Path(profile).resolve()
    known, unexpected = [], []
    for event in events:
        relative = Path(event['path']).relative_to(profile)
        kind = system_resource_kind(relative)
        if kind:
            known.append(dict(event, resource_kind=kind))
    # Ancestor directory notifications are retained but do not represent a CE
    # file write. Accept only added/modified ancestors of an actually observed
    # known resource. Unknown files, suffixes and other actions still fail.
    parents = set()
    for event in known:
        for parent in Path(event['path']).parents:
            if parent == profile:
                break
            if parent.is_relative_to(profile):
                parents.add(str(parent).lower())
    known_pairs = {(e['action'], e['path']) for e in known}
    for event in events:
        if (event['action'], event['path']) in known_pairs:
            continue
        if event['action'] in (1, 3) and event['path'].lower() in parents:
            continue
        unexpected.append(event)
    return known, unexpected
