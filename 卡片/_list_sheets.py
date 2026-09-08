# -*- coding: utf-8 -*-
"""List all sheets of every card xlsx to understand workbook layouts."""
import zipfile, io, glob, os
from lxml import etree

NS = {'m': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main',
      'r': 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}
DIR = r"D:\desktop\wolfkill\卡片"
out = io.StringIO()
for path in sorted(glob.glob(os.path.join(DIR, '*.xlsx'))):
    z = zipfile.ZipFile(path)
    wb = etree.fromstring(z.read('xl/workbook.xml'))
    sheets = wb.findall('m:sheets/m:sheet', NS)
    out.write(f"\n=== {os.path.basename(path)} ({len(sheets)} sheets) ===\n")
    for i, s in enumerate(sheets):
        out.write(f"  [{i}] {s.get('name')}\n")
open(os.path.join(DIR, '_sheets.txt'), 'w', encoding='utf-8').write(out.getvalue())
print('done')
