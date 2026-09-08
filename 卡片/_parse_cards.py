# -*- coding: utf-8 -*-
"""Parse every 角色卡 sheet in the 卡片 xlsx workbooks into a normalized JSON structure.
Reads raw XML via lxml. Output: _parsed_cards.json (+_parse_report.txt)
"""
import zipfile, io, glob, os, json, re
from lxml import etree

NS = {'m': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main',
      'r': 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}
DIR = r"D:\desktop\wolfkill\卡片"
OUT = r"D:\desktop\wolfkill\卡片\_parsed_cards.json"
REPORT = r"D:\desktop\wolfkill\卡片\_parse_report.txt"

ID_LABELS = {'职阶': 'class', '职业': 'main_job', '主职业': 'main_job', '灵基': 'lingji',
             '阵营': 'faction', '特性': 'traits', '隐属': 'hidden_attr', '性别': 'gender',
             '体型': 'size', '时代': 'era', '注释': 'note'}

SECTION_HEADERS = {'职阶技能': 'skill', '保有技能': 'skill', '保有技能（额外栏位）': 'skill',
                   '额外技能': 'skill', '宝具': 'np', '礼装': 'item', '额外栏位': 'extra',
                   '工房构件': 'workshop'}

NP_TYPE_KEYWORDS = ['对人', '对军', '对城', '对界', '对宝具', '对人魔剑', '对人宝具', '对结界',
                    '对恶', '对咒', '对灾', '对精灵', '对巨', '对龙', '对神', '对英灵', '对从者']


def colnum(c):
    n = 0
    for ch in c:
        n = n * 26 + (ord(ch) - 64)
    return n


def colname(n):
    s = ''
    while n > 0:
        n, rem = divmod(n - 1, 26)
        s = chr(65 + rem) + s
    return s


def parse_cells(z, sheet_target):
    sheet = etree.fromstring(z.read(sheet_target))
    cells = {}
    rows = sheet.find('m:sheetData', NS)
    if rows is None:
        return cells
    shared = []
    if 'xl/sharedStrings.xml' in z.namelist():
        root = etree.fromstring(z.read('xl/sharedStrings.xml'))
        for si in root.findall('m:si', NS):
            parts = [t.text or '' for t in si.iter('{%s}t' % NS['m'])]
            shared.append(''.join(parts))
    for row in rows.findall('m:row', NS):
        r = int(row.get('r'))
        for c in row.findall('m:c', NS):
            ref = c.get('r')
            t = c.get('t')
            v = c.find('m:v', NS)
            isel = c.find('m:is', NS)
            if isel is not None:
                val = ''.join(x.text or '' for x in isel.iter('{%s}t' % NS['m']))
            elif v is not None:
                val = v.text
            else:
                val = ''
            if t == 's' and val != '':
                val = shared[int(val)]
            if val is not None and str(val).strip():
                cells[(ref[0], r)] = str(val).strip()
    return cells


def sheet_target(z, idx):
    wb = etree.fromstring(z.read('xl/workbook.xml'))
    sheets = wb.findall('m:sheets/m:sheet', NS)
    rids = sheets[idx].get('{%s}id' % NS['r'])
    rels = etree.fromstring(z.read('xl/_rels/workbook.xml.rels'))
    tgt = None
    for rel in rels:
        if rel.get('Id') == rids:
            tgt = rel.get('Target')
            break
    if not tgt.startswith('xl/'):
        tgt = 'xl/' + tgt
    return tgt


def rank_parse(s):
    if not s:
        return None
    s = re.sub(r'[（(].*?[)）]', '', s)
    s = s.replace('级', '').replace('点', '').replace(' ', '').strip()
    if s in ('-', '/', '—', '无', 'None'):
        return None
    m = re.match(r'^([A-E]+|[EX]+)([+]*)?$', s, re.I)
    if not m:
        return None
    base = m.group(1).upper()
    if 'X' in base:
        return 'EX'
    return base[0]


def num_parse(s):
    if not s:
        return None
    s = s.replace('点', '').replace('级', '').replace(' ', '').strip()
    try:
        return float(s)
    except ValueError:
        return None


def right_val(cells, col, r):
    """Value at col+1, falling back to col+2."""
    for off in (1, 2):
        v = cells.get((colname(colnum(col) + off), r), '')
        if v:
            return v
    return ''


def main():
    report = io.StringIO()
    cards = []
    for path in sorted(glob.glob(os.path.join(DIR, '*.xlsx'))):
        base = os.path.basename(path)
        z = zipfile.ZipFile(path)
        wb = etree.fromstring(z.read('xl/workbook.xml'))
        sheets = wb.findall('m:sheets/m:sheet', NS)
        card_sheet = None
        for i, s in enumerate(sheets):
            if s.get('name', '').strip() == '角色卡':
                card_sheet = i
                break
        if card_sheet is None:
            report.write(f"{base}: NO 角色卡 sheet\n")
            continue
        cells = parse_cells(z, sheet_target(z, card_sheet))
        card = {'source': base, 'name': None, 'utype': None, 'class': None, 'main_job': None,
                'sub_job': None, 'lingji': None, 'faction': None, 'law': None, 'moral': None,
                'traits': [], 'hidden_attr': None, 'gender': None, 'size': None, 'era': None,
                'level': None, 'attr': [0] * 7, 'resources': []}
        report.write(f"\n########## {base} ##########\n")

        # ---- identity: labels at L column, value 1-2 cols right ----
        for (col, r), v in cells.items():
            if col == 'L' and v in ID_LABELS:
                val = right_val(cells, col, r)
                if v == '特性':
                    if val and val not in card['traits']:
                        card['traits'].append(val)
                else:
                    card[ID_LABELS[v]] = val
        # fallback left-side labels (E/F or B/C)
        if not card.get('class'):
            for (col, r), v in cells.items():
                if v == '职阶':
                    fv = right_val(cells, col, r)
                    if fv:
                        card['class'] = fv
                        break
        if not card.get('main_job'):
            for (col, r), v in cells.items():
                if v in ('职业', '主职业'):
                    fv = right_val(cells, col, r)
                    if fv:
                        card['main_job'] = fv
                        break
        # 灵基 value fallback (E10=灵基 F10=英灵)
        if not card.get('lingji'):
            for (col, r), v in cells.items():
                if v == '灵基':
                    fv = right_val(cells, col, r)
                    if fv:
                        card['lingji'] = fv
                        break
        # 子职
        for (col, r), v in cells.items():
            if v == '子职':
                sv = right_val(cells, col, r)
                if sv and not re.match(r'^\d', sv):
                    card['sub_job'] = sv

        # utype: master iff L-block 职业/主职业 present
        card['utype'] = 2 if card.get('main_job') else 1

        # traits: from 特性 label, collect following non-numeric values, dedupe
        for (col, r), v in cells.items():
            if v == '特性':
                for off in (1, 2, 3, 4):
                    tv = cells.get((colname(colnum(col) + off), r), '')
                    if not tv or tv in ('0', '0.0'):
                        continue
                    if tv not in card['traits'] and not re.match(r'^-?\d', tv):
                        card['traits'].append(tv)

        # ---- attributes from 合计 row ----
        attr_row = None
        for (col, r), v in cells.items():
            if v == '合计':
                attr_row = r
                break
        if attr_row:
            header = {}
            for (col, r), v in cells.items():
                if r == attr_row - 1 and v in ('等级', '筋力', '耐久', '敏捷', '魔力', '幸运', '宝具', '回路'):
                    header[v] = col
            level = cells.get(('C', attr_row))
            card['level'] = int(float(level)) if level else None
            attr_slot = {'筋力': 0, '耐久': 1, '敏捷': 2, '魔力': 3, '幸运': 4, '宝具': 5, '回路': 6}
            for label, slot in attr_slot.items():
                if label in header:
                    val = cells.get((header[label], attr_row))
                    if val:
                        card['attr'][slot] = int(float(val))
        report.write(f"  身份: utype={card['utype']} class={card['class']} job={card['main_job']}/{card['sub_job']} "
                     f"lingji={card['lingji']} faction={card['faction']} traits={card['traits']} "
                     f"hidden={card['hidden_attr']} level={card['level']} attr={card['attr']}\n")

        # ---- resource blocks ----
        # section headers per column (B=skills side, L=right side)
        section_of = {}
        rows_sorted = sorted({r for (_, r) in cells})
        cur = {'B': None, 'L': None}
        for r in rows_sorted:
            bv = cells.get(('B', r), '')
            lv = cells.get(('L', r), '')
            if bv in SECTION_HEADERS:
                cur['B'] = SECTION_HEADERS[bv]
            if lv in SECTION_HEADERS:
                cur['L'] = SECTION_HEADERS[lv]
            section_of[r] = (cur['B'], cur['L'])

        # unique 模板 rows with the side(s) that have the label
        tmpl_events = []
        for (col, r), v in cells.items():
            if v == '模板' and col in ('B', 'L'):
                tmpl_events.append((col, r))
        for (side, tr) in tmpl_events:
            tmpl = right_val(cells, side, tr)
            sec = section_of.get(tr, (None, None))[0 if side == 'B' else 1]
            block = {'template': tmpl}
            lo, hi = ('B', 'J') if side == 'B' else ('L', 'T')
            for dr in range(tr, tr + 4):
                for ((col, r), v) in cells.items():
                    if r != dr or col < lo or col > hi:
                        continue
                    if v == '名称':
                        block['name'] = right_val(cells, col, r)
                    elif v == '等级':
                        rk = rank_parse(right_val(cells, col, r))
                        if rk:
                            block['rank'] = rk
                    elif v in ('类型', '构件种类'):
                        block['type'] = right_val(cells, col, r)
                    elif v == '回转':
                        rc = num_parse(right_val(cells, col, r))
                        if rc is not None:
                            block['recast'] = rc
                    elif v == '魔力消耗':
                        co = num_parse(right_val(cells, col, r))
                        if co is not None:
                            block['cost'] = co
                    elif v in ('发动时机', '发动时机、'):
                        block['timing'] = right_val(cells, col, r)
                    elif v == '资源':
                        rr = num_parse(right_val(cells, col, r))
                        if rr is not None:
                            block['resource_cost'] = rr
            if sec == 'workshop':
                report.write(f"  [工房构件] {tmpl or block.get('name','')} (跳过, 非资源)\n")
                continue
            if not block.get('name') and not tmpl:
                continue
            rtype = block.get('type', '')
            if sec == 'np':
                kind = 'np'
            elif sec == 'item':
                kind = 'np' if any(k in rtype for k in NP_TYPE_KEYWORDS) else 'skill'
            elif sec == 'extra':
                kind = 'np' if any(k in rtype for k in NP_TYPE_KEYWORDS) else 'skill'
            else:
                kind = 'np' if any(k in rtype for k in NP_TYPE_KEYWORDS) else 'skill'
            block['kind'] = kind
            block['section'] = sec or 'skill'
            card['resources'].append(block)
            report.write(f"  [{kind:5}] 模板={tmpl!r:26} 名称={block.get('name','')!r:30} 等级={block.get('rank')} "
                         f"类型={rtype!r:14} 回转={block.get('recast')} 魔耗={block.get('cost')} "
                         f"时机={block.get('timing','')!r:14} 资源={block.get('resource_cost')}\n")

        if not card.get('name'):
            card['name'] = os.path.splitext(base)[0]
        cards.append(card)

    with open(OUT, 'w', encoding='utf-8') as f:
        json.dump(cards, f, ensure_ascii=False, indent=1)
    with open(REPORT, 'w', encoding='utf-8') as f:
        f.write(report.getvalue())
    print('parsed', len(cards), 'cards ->', OUT)


if __name__ == '__main__':
    main()