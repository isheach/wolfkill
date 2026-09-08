# -*- coding: utf-8 -*-
"""Analyze card dumps: extract resource template names (skills/phantasms) and cross-check against ksg_resources.json.
Usage: python _analyze_cards.py
"""
import json, re, os, glob

DB = r"D:\desktop\wolfkill\engine-godot\data\ksg_resources.json"
CARDS_DIR = r"D:\desktop\wolfkill\卡片"

import sys
OUT = open(r"D:\desktop\wolfkill\卡片\_analyze_out.txt", 'w', encoding='utf-8')
def P(*a):
    print(*a, file=OUT)
    print(*a)

data = json.load(open(DB, encoding='utf-8'))
res = data['resources']
by_name = {}
for r in res:
    n = r.get('name', '')
    by_name.setdefault(n, []).append(r)

P("DB total:", len(res), "unique names:", len(by_name))

# Extract template/name values from dump files
# A "block" on the servant sheet is rows with 模板/名称 labels. Template value is the official name;
# custom 名称 is the OC name. We collect both and mark existence.

def load_dump(path):
    txt = open(path, encoding='utf-8').read()
    return txt

def extract_names(txt):
    """Return list of (label, value) pairs for 模板 and 名称 cells."""
    out = []
    for line in txt.splitlines():
        # cells like B20=模板 C20=阵地制造
        for m in re.finditer(r'([A-Z]+)(\d+)=(模板|名称)', line):
            col = m.group(1)
            row = m.group(2)
            label = m.group(3)
            # find value in same row, same or next column
            continue
        # simpler: find all =模板 and =名称 then grab the next =XXX value cell
    return out

def cells_of_line(line):
    cells = {}
    for m in re.finditer(r'([A-Z]+)(\d+)=([^|]*)', line):
        cells[(m.group(1), m.group(2))] = m.group(3).strip()
    return cells

def colnum(c):
    n = 0
    for ch in c:
        n = n*26 + (ord(ch)-64)
    return n

def nextcol(c):
    n = colnum(c)
    return n+1

def colname(n):
    s = ''
    while n > 0:
        n, rem = divmod(n-1, 26)
        s = chr(65+rem) + s
    return s

results = {}  # name -> {cards:[], kind:set}
for f in sorted(glob.glob(os.path.join(CARDS_DIR, '_*.txt'))):
    base = os.path.basename(f)
    if base.startswith('_dump_out') or base == '_dump_xlsx.py':
        continue
    txt = load_dump(f)
    for line in txt.splitlines():
        cells = cells_of_line(line)
        # find a 模板 label and its value: value usually in next column
        labels = [ (c,r,v) for (c,r),v in cells.items() if v == '模板' ]
        for (c,r,v) in labels:
            nxt = colname(colnum(c)+1)
            val = cells.get((nxt, r), '')
            if val:
                results.setdefault(val, {'cards': set(), 'kind': set()})
                results[val]['cards'].add(base)
                results[val]['kind'].add('template')
        labels = [ (c,r,v) for (c,r),v in cells.items() if v == '名称' ]
        for (c,r,v) in labels:
            nxt = colname(colnum(c)+1)
            val = cells.get((nxt, r), '')
            if val:
                results.setdefault(val, {'cards': set(), 'kind': set()})
                results[val]['cards'].add(base)
                results[val]['kind'].add('name')

P(f"\n== Referenced names found in cards: {len(results)} ==\n")
found = 0
missing = []
for n, info in sorted(results.items(), key=lambda kv: -len(kv[1]['cards'])):
    in_db = n in by_name
    if in_db:
        found += 1
    else:
        missing.append(n)
    cards = ','.join(sorted(info['cards'])[:4])
    P(f"[{'OK ' if in_db else 'MISS'}] {n!r:40} cards={cards}")

P(f"\nFound in DB: {found}/{len(results)}")
P(f"\n== Missing (need mapping/creation): {len(missing)} ==")
for n in missing:
    P("  -", repr(n))
OUT.close()
