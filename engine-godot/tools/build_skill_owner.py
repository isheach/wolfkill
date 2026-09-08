# -*- coding: utf-8 -*-
"""从规则书 PDF 提取文本生成 skill_owner.json(技能从者/御主归属)。
用法: python tools/build_skill_owner.py
规则: 从者库标题 → servant; 御主库标题 → master; 两库都有 → both; 未匹配 → both(宽松)。
"""
import json, re, os

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
SERVANT_PDF = os.path.join(ROOT, '..', 'notes', 'extracted', '空想从者资源库SP1.17.txt')
MASTER_PDF = os.path.join(ROOT, '..', 'notes', 'extracted', '空想御主资源库SP1.17.txt')
OUT = os.path.join(ROOT, 'data', 'skill_owner.json')

RANK_PAT = r'((?:A B C D E)|(?:A B C D)|(?:A B C DE)|(?:A B C)|(?:A B DE)|(?:A B)|(?:B C D E)|(?:B C D)|(?:B C)|(?:C)|(?:E)|(?:A)|(?:B)|(?:D))'

def titles(src):
    out = set()
    if not os.path.exists(src):
        print('SKIP 缺失:', src)
        return out
    for l in open(src, encoding='utf-8').read().splitlines():
        s = l.strip()
        m = re.match(r'^(.{1,24}?)[\s　]*' + RANK_PAT + r'\s*(?:（.*?）)?\s*$', s)
        if m:
            out.add(m.group(1))
    return out

def norm(n):
    s = n.split('·')[0]
    s = re.sub(r'[（(].*?[)）]', '', s)
    s = s.replace('EX', '').replace(' ', '')
    return s.strip()

s_norm = {norm(t) for t in titles(SERVANT_PDF)}
m_norm = {norm(t) for t in titles(MASTER_PDF)}

d = json.load(open(os.path.join(ROOT, 'data', 'ksg_resources.json'), encoding='utf-8'))
owner = []
for r in d['resources']:
    if r['kind'] != 'skill':
        continue
    n = norm(r['name'])
    in_s = n in s_norm
    in_m = n in m_norm
    if in_s and in_m:
        own = 'both'
    elif in_s:
        own = 'servant'
    elif in_m:
        own = 'master'
    else:
        own = 'both'  # 自建/变体 → 宽松
    owner.append({'id': r['id'], 'name': r['name'], 'owner': own})

json.dump({'skills': owner}, open(OUT, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
from collections import Counter
print('归属分布:', dict(Counter(x['owner'] for x in owner)), '→', OUT)
