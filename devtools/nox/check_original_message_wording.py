"""Reject reintroduced hand-written game feedback at the restored UI sinks.

This is a source policy check, not proof that every asset lookup or PC label is
original. Run the focused resource and gameplay checks recorded with RC1146.
"""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[2]
FILES=('native_search_input.h','native_inventory_window.h','native_world_cast.h')
allowed={'Preferences: ScummVM Game Options.'} # Explicit CE settings route, not game prose.
literal=re.compile(r'"((?:[^"\\]|\\.)*)"')
failures=[]
for name in FILES:
    path=ROOT/'engines/nox'/name
    for number,line in enumerate(path.read_text().splitlines(),1):
        if not (re.search(r'\b_message\s*=',line) or 'show(archive,frame,' in line or 'prompt(archive,frame,' in line):
            continue
        for text in literal.findall(line):
            if re.search(r'[A-Za-z]{2}',text) and text not in allowed:
                failures.append(f'{name}:{number}: literal game feedback: {text}')
p=ROOT/'engines/nox/native_battle_presentation.h'
s=p.read_text();block=s[s.index('    bool quickMessages('):s.index('    bool defeatText(')]
for text in literal.findall(block):
    if re.search(r'[A-Za-z]{2}',text): failures.append('Quick result literal: '+text)
if failures: raise SystemExit('\n'.join(failures))
print('PASS restored feedback sinks use asset text; explicit CE settings route retained')
