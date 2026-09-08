# -*- coding: utf-8 -*-
"""Write dump to file with explicit UTF-8. Usage: python _dump_xlsx.py <file> <out> [sheetIndex...]"""
import sys, zipfile, io
from lxml import etree

NS = {'m': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main',
      'r': 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}


def col_idx(ref):
    import re
    m = re.match(r'([A-Z]+)(\d+)', ref)
    col = 0
    for ch in m.group(1):
        col = col * 26 + (ord(ch) - 64)
    return col, int(m.group(2))


def main(path, out, wanted=None):
    z = zipfile.ZipFile(path)
    buf = io.StringIO()
    shared = []
    if 'xl/sharedStrings.xml' in z.namelist():
        root = etree.fromstring(z.read('xl/sharedStrings.xml'))
        for si in root.findall('m:si', NS):
            parts = [t.text or '' for t in si.iter('{%s}t' % NS['m'])]
            shared.append(''.join(parts))
    wb = etree.fromstring(z.read('xl/workbook.xml'))
    sheets = wb.findall('m:sheets/m:sheet', NS)
    names = [s.get('name') for s in sheets]
    if wanted:
        wanted = [int(x) for x in wanted]
    else:
        wanted = list(range(len(sheets)))
    for idx in wanted:
        sname = names[idx]
        rids = sheets[idx].get('{%s}id' % NS['r'])
        rels = etree.fromstring(z.read('xl/_rels/workbook.xml.rels'))
        target = None
        for rel in rels:
            if rel.get('Id') == rids:
                target = rel.get('Target')
                break
        if not target.startswith('xl/'):
            target = 'xl/' + target
        sheet = etree.fromstring(z.read(target))
        rows = sheet.find('m:sheetData', NS)
        buf.write(f'\n########## SHEET {idx}: {sname} ##########\n')
        if rows is None:
            buf.write('(empty)\n')
            continue
        for row in rows.findall('m:row', NS):
            cells = []
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
                if val is not None:
                    cells.append(f'{ref}={val}')
            if cells:
                buf.write(' | '.join(cells) + '\n')
    with open(out, 'w', encoding='utf-8') as f:
        f.write(buf.getvalue())
    print('wrote', out)


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print('usage: _dump_xlsx.py <in.xlsx> <out.txt> [sheetIdx...]')
        sys.exit(1)
    main(sys.argv[1], sys.argv[2], sys.argv[3:])
