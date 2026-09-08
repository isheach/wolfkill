# -*- coding: utf-8 -*-
"""从规则书 PDF 提取文本生成 rank_variants.json(技能等级范围+各等级真实数值)。
用法: python tools/build_rank_variants.py
依赖: notes/extracted/空想从者资源库SP1.17.txt 与 空想御主资源库SP1.17.txt(PDF转文本, 页面标记行 '===== PAGE n =====')
输出: data/rank_variants.json (Godot 启动加载, 建卡时按等级显示规则书数值)
"""
import json, re, os, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
PDFS = [
    os.path.join(ROOT, '..', 'notes', 'extracted', '空想从者资源库SP1.17.txt'),
    os.path.join(ROOT, '..', 'notes', 'extracted', '空想御主资源库SP1.17.txt'),
]
OUT = os.path.join(ROOT, 'data', 'rank_variants.json')

RANK_PAT = r'((?:A B C D E)|(?:A B C D)|(?:A B C DE)|(?:A B C)|(?:A B DE)|(?:A B)|(?:B C D E)|(?:B C D)|(?:B C)|(?:C)|(?:E)|(?:A))'

def build():
    pdf_blocks = {}
    all_lines = []
    for src in PDFS:
        if not os.path.exists(src):
            print('SKIP 缺失:', src)
            continue
        all_lines += open(src, encoding='utf-8').read().splitlines()
    i, L = 0, len(all_lines)
    while i < L:
        s = all_lines[i].strip()
        m = re.match(r'^(.{1,24}?)[\s　]*' + RANK_PAT + r'\s*(?:（.*?）)?\s*$', s)
        if m:
            name = m.group(1); ranks = m.group(2).split()
            block = []
            j = i + 1
            while j < L:
                t = all_lines[j].strip()
                if re.match(r'^.{1,24}?[\s　]*' + RANK_PAT + r'\s*(?:（.*?）)?\s*$', t) or t.startswith('====='):
                    break
                block.append(t); j += 1
            pdf_blocks[name] = (ranks, block)
            i = j
        else:
            i += 1

    def pick_pdf(name):
        if name in pdf_blocks: return name
        norm_b = name.replace('(', '（').replace(')', '）')
        for pn in pdf_blocks:
            if pn == norm_b: return pn
        cand = []
        for pn in pdf_blocks:
            flat = re.sub(r'[（(].*?[)）]', '', pn)
            if flat in name: cand.append((len(flat), pn))
        if cand:
            cand.sort(reverse=True); return cand[0][1]
        sn = re.sub(r'[（(].*?[)）]', '', name).replace('EX', '').replace(' ', '')
        for pn in pdf_blocks:
            pn_sn = re.sub(r'[（(].*?[)）]', '', pn).replace(' ', '')
            if pn_sn == sn: return pn
        return None

    def find_series(block):
        res = []
        for b in block:
            for m2 in re.finditer(r'((?:[+\-]?\d+/)+[+\-]?\d+%?)', b):
                res.append([int(t) for t in re.findall(r'[+\-]?\d+', m2.group(1))])
        return res

    srcjson = os.path.join(ROOT, 'data', 'ksg_resources.json')
    d = json.load(open(srcjson, encoding='utf-8'))
    skills = [r for r in d['resources'] if r['kind'] == 'skill']

    variants = []
    for r in skills:
        pn = pick_pdf(r['name'])
        if pn is None: continue
        ranks, block = pdf_blocks[pn]
        # DE/… 拆成单字母
        rr2 = []
        for x in ranks:
            rr2.extend(list(x)) if re.fullmatch(r'[ABCDE]{2,}', x) else rr2.append(x)
        ranks = rr2
        n = len(ranks)
        series = find_series(block)
        srs = []
        if series:
            for s in series:
                vals = s[:n]
                if len(vals) < n:
                    last = vals[-1] if vals else 0
                    vals = vals + [last] * (n - len(vals))
                srs.append(vals)
        else:
            eff = [e.get('value', 0) for e in r.get('effects', []) if isinstance(e.get('value', 0), (int, float)) and e['value'] != 0]
            srs = [[v] * n for v in eff]
        variants.append({'id': r['id'], 'name': r['name'], 'rank_range': ranks,
                         'series': srs, 'effect_count': len(r.get('effects', []))})

    json.dump({'version': 2, 'skills': variants}, open(OUT, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    n_real = sum(1 for v in variants if any(len(s) > 1 and len(set(s)) > 1 for s in v['series']))
    print(f'等级变体表: {len(variants)}/{len(skills)} 技能 (真实逐级数值 {n_real} 个) → {OUT}')

if __name__ == '__main__':
    build()
