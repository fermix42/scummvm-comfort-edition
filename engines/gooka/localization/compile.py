#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile reviewed UTF-8 English entries to the engine's CP1250 lookup table."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from functools import lru_cache

ROOT = Path(__file__).resolve().parent
REVIEW = {856: 'Invented animal name: zuborozec rendered tuskhorn; no visual confirmation.',
          918: 'Vymozkovany rendered mind-emptied, also used for the needle.',
          1338: 'Deliberate nonsense; phonetic wordplay adaptation needs native-speaker review.',
          1339: 'Deliberate garbled proverb; adaptation needs native-speaker review.',
          1373: 'Source uses Jantaris, elsewhere Janatris. Variant retained.',
          3216: 'Source calls sincha a mollusc; dialogue 1320 calls it a crustacean. Retained.',
          3221: 'Fictional chracht/krahut terms retained without invented definitions.'}

# Paragraph anchors for the encyclopaedia: never move headings or clues between paragraphs.
BLOCKS = {
    3204: ['The Bonians.', 'Farmers and', 'Their state', 'The Bonian population', 'They keep'],
    3205: ['the Cymiks,', 'The Bonians are'],
    3206: ['The Svans.', 'A kingdom', 'The Svans prize', 'Their total population'],
    3208: ['The Cymiks.', 'Nomads of', 'The Cymiks rank', 'Scattered across', 'Recent reports'],
    3210: ['The Bavends.', 'An advanced civilisation', 'The Bavend Republic', 'The Bavends are', 'Their population'],
    3211: ['The Chorians.', 'The Chorians inhabit', 'Their society', 'Like the Cymiks,', 'Their population', 'Their largest city'],
    3212: ['Freshwater crab.', 'A ten-legged', 'It lives in', 'Irikan crystals.', 'Probably the strongest'],
    3213: ['Asmat scorpions.', 'Enormous mountain', 'Their venom', 'A single sting'],
    3214: ['Algi.', 'The largest feline', 'In practice,'],
    3216: ['Sincha.', 'One of the most', 'Reliable reports'],
    3217: ['Galha.', 'A sea lizard', 'Rampol.', 'A climbing plant'],
    3218: ['Caustic stalactites.', 'An aggressive plant'],
    3219: ['Grango.', 'A lizard of', 'Tree korel.', 'A small furry', 'Korels are'],
    3220: ['Harila.', 'A small blue plant', 'Olamina.', 'A low-growing'],
    3221: ['Asmat stygra.', 'A ritual musical', 'The Asmat stygra', 'Likanu.', 'A creature about', 'Easily tamed']}


def controls(text):
    return re.findall(r'[\x01-\x1f]|@\d+|%[-+0-9.]*[a-zA-Z]', text)


def lay_out(source, translated):
    """Retain empty lines/alignment/picture commands; redistribute words per line."""
    chunks = re.split(r'([\x01-\x1f])', source)
    slots = [i for i in range(0, len(chunks), 2) if chunks[i].strip() and
             not re.fullmatch(r'@\d+', chunks[i].strip())]
    words = translated.split()
    total = sum(len(chunks[i].strip()) for i in slots)
    used = 0
    weight = 0
    for n, i in enumerate(slots):
        weight += len(chunks[i].strip())
        end = len(words) if n == len(slots)-1 else round(len(words)*weight/total)
        leading = re.match(r' *', chunks[i])[0]
        chunks[i] = leading+' '.join(words[used:end])
        used = end
    if used != len(words):
        raise ValueError('No slots for translated text')
    result = ''.join(chunks)
    if controls(source) != controls(result):
        raise ValueError('Control/format command changed')
    return result


def book_layout(rid, source, text):
    starts = BLOCKS.get(rid)
    if not starts:
        return lay_out(source, text)
    positions = [text.index(start) for start in starts] + [len(text)]
    if positions != sorted(positions) or positions[0] != 0:
        raise ValueError(f'Invalid paragraph anchors: {rid}')
    translations = iter(text[a:b].strip() for a,b in zip(positions,positions[1:]))
    paragraphs = source.split('\n\n')
    for i,p in enumerate(paragraphs):
        if p.strip() and not re.fullmatch(r'@\d+',p.strip()):
            paragraphs[i] = lay_out(p,next(translations))
    if next(translations,None) is not None:
        raise ValueError('Unused translated paragraph')
    return '\n\n'.join(paragraphs)


def fit_book(text, widths):
    """Fit each paragraph into its original line count and 250-pixel column."""
    paragraphs = text.split('\n\n')
    for index, paragraph in enumerate(paragraphs):
        parts = re.split(r'([\x01-\x1f])', paragraph)
        slots = [i for i in range(0,len(parts),2) if parts[i].strip() and
                 not re.fullmatch(r'@\d+',parts[i].strip())]
        words = ' '.join(parts[i].strip() for i in slots).split()
        def width(s):
            return sum(widths[b] for b in s.encode('cp1250'))
        @lru_cache(None)
        def solve(pos, line):
            if line == len(slots):
                return (0,()) if pos == len(words) else None
            best = None
            for end in range(pos+1,len(words)+1):
                segment = ' '.join(words[pos:end])
                w = width(segment)
                if w > 250:
                    break
                tail = solve(end,line+1)
                if tail is not None:
                    value = (250-w)**2+tail[0]
                    if best is None or value < best[0]:
                        best = (value,(segment,)+tail[1])
            return best
        if not slots:
            continue
        solution = solve(0,0)
        if solution is None:
            raise ValueError(f'Book paragraph does not fit {len(slots)} lines: {words[:10]}')
        for slot,line in zip(slots,solution[1]):
            parts[slot] = line
        paragraphs[index] = ''.join(parts)
    result = '\n\n'.join(paragraphs)
    if controls(result) != controls(text):
        raise ValueError('Book fitting changed controls')
    return result


def header(entries):
    lines = ['// SPDX-License-Identifier: GPL-3.0-or-later',
             '// Generated by localization/compile.py; edit english.json, not this file.',
             '#ifndef GOOKA_ENGLISH_H', '#define GOOKA_ENGLISH_H',
             'namespace Gooka {', 'struct EnglishText { uint16 id; const char *text; };',
             'static const EnglishText kEnglishText[] = {']
    for entry in entries:
        raw = entry['english'].encode('cp1250')
        if b'\0' in raw:
            raise ValueError('Embedded NUL')
        encoded = ''.join(f'\\x{b:02x}' for b in raw)
        lines.append(f'\t{{ {entry["id"]}, "{encoded}" }},')
    lines += ['};', '} // namespace Gooka', '#endif', '']
    return '\n'.join(lines)


def initialize(catalog):
    phrases = {}
    for line in (ROOT/'english.tsv').read_text(encoding='utf-8').splitlines():
        if not line or line.startswith('#'):
            continue
        key, text = line.split('\t', 1)
        if int(key) in phrases:
            raise ValueError('Duplicate translation ID')
        phrases[int(key)] = text
    entries = []
    for original in catalog:
        rid, source = original['id'], original['czech']
        if rid == 4000:
            lines = source.split('\n')
            lines[:8] = [
                '   And so Gooka saved himself, Lidra',
                'and his world from war and enslavement. Without Gurham\'s',
                'help, Penza had to surrender to Digor\'s troops.',
                'He was fairly tried and sentenced to death. Later,',
                'the sentence was commuted to exile. Gooka returned to',
                'Avaral and was happily reunited with Lidra. After long',
                'consultation with Digor, they decided that Glux\'s...',
                'But that is another story.']
            headings = {'Námět:':'Story:', 'Scénář:':'Script:', 'Grafika:':'Artwork:',
                        'Počítačové zpracování grafiky:':'Computer graphics:',
                        'Zvukové efekty:':'Sound effects:', 'Hudba:':'Music:',
                        'Hlasy:':'Voices:', 'Pomocné programy:':'Tools:',
                        'Výkonná produkce:':'Executive producer:',
                        'Úvodní a závěrečná sekvence:':'Intro and ending sequences:',
                        'Produkce:':'Production:', 'Program:':'Programming:', 'Betatesty:':'Beta testing:'}
            translated = '\n'.join(headings.get(line,line) for line in lines)
        elif not source.strip():
            translated = source
        else:
            translated = book_layout(rid, source, phrases.pop(rid))
        if controls(source) != controls(translated):
            raise ValueError(f'Changed formatting for {rid}')
        entry = dict(id=rid, english=translated, source_sha256=original['source_sha256'],
                     status='needs-context-review' if rid in REVIEW else 'translated',
                     note=REVIEW.get(rid,''))
        entries.append(entry)
    if phrases:
        raise ValueError(f'Unknown IDs: {sorted(phrases)}')
    (ROOT/'english.json').write_text(json.dumps(entries,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--initialize', type=Path, help='private owned Czech catalog; first-time layout only')
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--fit-book-font', type=Path, help='private 256 original font advance widths')
    args = parser.parse_args()
    if args.initialize:
        initialize(json.loads(args.initialize.read_text(encoding='utf-8')))
    entries = json.loads((ROOT/'english.json').read_text(encoding='utf-8'))
    if args.fit_book_font:
        widths = json.loads(args.fit_book_font.read_text())
        for entry in entries:
            if 3202 <= entry['id'] <= 3221:
                entry['english'] = fit_book(entry['english'],widths)
        (ROOT/'english.json').write_text(json.dumps(entries,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    ids = [e['id'] for e in entries]
    if ids != sorted(set(ids)):
        raise ValueError('IDs must be unique and sorted')
    compiled = header(entries)
    target = ROOT/'english.h'
    if args.check:
        if target.read_text(encoding='utf-8') != compiled:
            raise SystemExit('Generated English header is stale')
    else:
        target.write_text(compiled,encoding='utf-8')
    print(f'{len(entries)} entries; {sum(e["status"]=="needs-context-review" for e in entries)} flagged for review')
