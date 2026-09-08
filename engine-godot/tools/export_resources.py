#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""export_resources.py — 把 C 引擎资源数据(ksg_data.c + ksg_db.c)解析为 Godot 可加载的 JSON。
解析目标:
  - def_skill / def_np / def_item / db_skill / db_np / def_item_compat 定义
  - 紧随的 add_eff(...) / db_eff(...) 效果行
  - set_text(...) / db_set_text(...) 原文文本
输出: engine-godot/data/ksg_resources.json
"""
import ast
import copy
import json
import os
import re
import sys

SRC_DIR = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'engine'))
SRC_FILES = [
    os.path.join(SRC_DIR, 'ksg_data.c'),
    os.path.join(SRC_DIR, 'ksg_db.c'),
]
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'data')
OUT = os.path.join(OUT_DIR, 'ksg_resources.json')

# ---------- C 参数解析辅助 ----------

def cstr(s):
    """解析一个或多个相邻的 C/Python 兼容字符串字面量。"""
    literals = re.findall(r'"(?:\\.|[^"\\])*"', s, re.S)
    if not literals:
        return s.strip()
    parts = []
    for literal in literals:
        try:
            parts.append(ast.literal_eval(literal))
        except (SyntaxError, ValueError):
            parts.append(literal[1:-1].replace('\\"', '"').replace('\\\\', '\\'))
    return ''.join(parts)

def parse_int(s):
    s = s.strip()
    m = re.match(r'^([+-]?\d+)\b', s)
    return int(m.group(1)) if m else 0

def parse_scalar(s):
    """数值保持为 int，C 枚举/标识符保持为符号字符串，避免静默丢值。"""
    s = s.strip()
    if re.fullmatch(r'[+-]?\d+', s):
        return int(s)
    return s

RANK_MAP = {'EX': 'EX', 'A': 'A', 'B': 'B', 'C': 'C', 'D': 'D', 'E': 'E', 'NEG': '-', 'NONE': '-'}
WHEN_MAP = {'KS_WHEN_PASSIVE': 'passive', 'KS_WHEN_ACT': 'act',
            'KS_WHEN_BATTLE_START': 'battle_start', 'KS_WHEN_PROC': 'proc',
            'KS_WHEN_ANY': 'any'}

def parse_when(s):
    s = s.strip()
    return WHEN_MAP.get(s, s)

def parse_rank(s):
    s = s.strip()
    m = re.match(r'^(EX|A|B|C|D|E|-)', s.upper())
    if m:
        return m.group(1)
    m2 = re.match(r'KS_RANK_(\w+)', s)
    if m2:
        return RANK_MAP.get(m2.group(1).upper(), m2.group(1).upper())
    return '?'

def parse_enum_call(s):
    """解析一个枚举常量或简单表达式调用,如 KS_F_MAIN | KS_F_PIERCE / FC_DECISIVE / KC_NONE"""
    s = s.strip()
    # 收集所有 KS_*/FC_*/KC_*/TR_*/S_* 标识符
    return re.findall(r'\b(?:KS|FC|KC|TR)_[A-Z0-9_]+', s)

def parse_effect_flag(s):
    """把 C 效果旗标枚举转为字符串"""
    s = s.strip()
    return s

def parse_target(t):
    t = t.strip()
    if t == '-2': return 'self'
    if t == '-1': return 'ally_all'
    if t == '0': return 'enemy_all'
    m = re.match(r'^(-?\d+)$', t)
    if m: return 'enemy_%d' % abs(int(m.group(1)))
    return t

def parse_attr(t):
    t = t.strip()
    return t  # 保留 A_STR 等原文,由 Godot 端映射

def parse_status(t):
    return t.strip()

# ---------- 语法解析 ----------

def parse_call(src, pos, fname):
    """在 src 中查找 fname( 并解析其括号参数(支持嵌套调用、逗号、字符串)。返回(参数列表, 结束位置)。
    只匹配调用点(前面有 空格、=、逗号 等),不匹配函数声明。"""
    # 逐处找,且前面必须是 'r = ' 或逗号/空格/等号
    i = src.find(fname + '(', pos)
    while i >= 0:
        pre = src[i-1] if i > 0 else ''
        if pre in ' =,;:([' or pre in '\n\t\r' or pre == 'w':
            break
        i = src.find(fname + '(', i + 1)
    if i < 0:
        return None, -1
    i += len(fname) + 1
    depth = 0
    args = []
    cur = ''
    instr = False
    while i < len(src):
        ch = src[i]
        if instr:
            cur += ch
            if ch == '"':
                instr = False
            i += 1
            continue
        if ch == '"':
            instr = True
            cur += ch
        elif ch == '(':
            depth += 1
            cur += ch
        elif ch == ')':
            if depth == 0:
                args.append(cur.strip())
                return args, i + 1
            depth -= 1
            cur += ch
        elif ch == ',' and depth == 0:
            args.append(cur.strip())
            cur = ''
        else:
            cur += ch
        i += 1
    return None, -1

def parse_args(src, pos, fname):
    """兼容封装"""
    args, end = parse_call(src, pos, fname)
    return args, end, pos

def parse_resources(src):
    """解析一份 C 源文件,返回资源列表。"""
    resources = []
    # 定义语句模式: r = def_xxx(w, "name", ...);  或  r = db_xxx(w, "name", ...);
    # 或块式科技造物: { ks_res_t t; ... snprintf(t.name, KSG_NAME_MAX, "%s", "名"); ...}
    pos = 0
    pat = re.compile(r'(def_skill|def_np|def_item|db_skill|db_np|def_item_compat|db_item|db_tech)\(w,')
    while True:
        m = pat.search(src, pos)
        if not m:
            break
        kind = m.group(1)
        args, end = parse_call(src, m.start(), kind)
        if not args:
            pos = m.end()
            continue
        name = cstr(args[1]) if len(args) > 1 else '?'
        res = {
            'name': name,
            'kind': kind,
            'type': '',
            'focus': '',
            'rank': '',
            'when': '',
            'cost': 0,
            'recast': 0,
            'feat': [],
            'count_per_round': 0,
            'reserve': 0,
            'reserve_max': 0,
            'unique': 0,
            'effects': [],
            'text': '',
        }
        # 不同构造函数参数布局:
        # def_skill(w, name, type, rank, when, cost, recast, feat)
        # def_np  (w, name, type, focus, rank, when, cost, recast, feat)
        # def_item(w, name, rank, when, cost, recast, feat, count, reserve, unique)
        # def_item_compat 同 def_item
        if kind in ('def_skill', 'db_skill'):
            if len(args) >= 8:
                res['type'] = args[2].strip()
                res['rank'] = parse_rank(args[3])
                res['when'] = parse_when(args[4])
                res['cost'] = parse_int(args[5])
                res['recast'] = parse_int(args[6])
                res['feat'] = parse_enum_call(args[7]) if len(args) > 7 else []
            res['kind'] = 'skill'
        elif kind in ('def_np', 'db_np'):
            if len(args) >= 9:
                res['type'] = args[2].strip()
                res['focus'] = args[3].strip()
                res['rank'] = parse_rank(args[4])
                res['when'] = parse_when(args[5])
                res['cost'] = parse_int(args[6])
                res['recast'] = parse_int(args[7])
                res['feat'] = parse_enum_call(args[8]) if len(args) > 8 else []
            res['kind'] = 'np'
        elif kind in ('def_item', 'def_item_compat', 'db_item'):
            if len(args) >= 10:
                res['type'] = 'KS_T_MAGIC'
                res['rank'] = parse_rank(args[2])
                res['when'] = parse_when(args[3])
                res['cost'] = parse_int(args[4])
                res['recast'] = parse_int(args[5])
                res['feat'] = parse_enum_call(args[6])
                res['count_per_round'] = parse_int(args[7])
                res['reserve'] = parse_int(args[8])
                res['reserve_max'] = res['reserve']
                res['unique'] = parse_int(args[9])
            res['kind'] = 'item'
        elif kind == 'db_tech':
            if len(args) >= 8:
                res['type'] = 'KS_T_WEAPON'
                res['rank'] = parse_rank(args[2])
                res['when'] = parse_when(args[3])
                res['cost'] = parse_int(args[4])
                res['recast'] = parse_int(args[5])
                res['feat'] = parse_enum_call(args[6])
                res['reserve'] = parse_int(args[7])
                res['reserve_max'] = res['reserve']
            res['kind'] = 'item'

        # 解析随后的效果行与文本:在当前定义块之后顺序查找 add_eff/db_eff/set_text/db_set_text
        block_pos = end
        # 块边界:下一个资源定义
        nxt = pat.search(src, m.end())
        block_end = nxt.start() if nxt else len(src)
        if block_end < end:
            block_end = len(src)

        # 用效果变量的实际赋值顺序解析，避免把 db_eff(..., e) 降级成占位符。
        sub = src[end:block_end]
        clean_sub = strip_c_comments(sub)
        res['effects'] = parse_block_effects(clean_sub, name)

        # 文本
        for func in ('set_text', 'db_set_text'):
            p3 = 0
            while True:
                args3, e3, s3 = parse_args(clean_sub, p3, func)
                if not args3:
                    break
                p3 = e3
                if len(args3) >= 3:
                    res['text'] = cstr(args3[2])

        resources.append(res)
        pos = m.end()

    return resources

def split_top(s):
    """按顶层逗号分割(带引号/括号)"""
    parts = []
    cur = ''
    depth = 0
    instr = False
    for ch in s:
        if instr:
            cur += ch
            if ch == '"': instr = False
        elif ch == '"':
            instr = True; cur += ch
        elif ch == '(':
            depth += 1; cur += ch
        elif ch == ')':
            depth -= 1; cur += ch
        elif ch == ',' and depth == 0:
            parts.append(cur); cur = ''
        else:
            cur += ch
    if cur.strip(): parts.append(cur)
    return parts

PARSE_WARNINGS = []

def strip_c_comments(src):
    """移除 // 与 /* */ 注释，同时保留字符串内容和换行位置。"""
    out = []
    i = 0
    state = 'code'
    while i < len(src):
        ch = src[i]
        nxt = src[i + 1] if i + 1 < len(src) else ''
        if state == 'code':
            if ch == '"':
                state = 'string'
                out.append(ch)
            elif ch == "'":
                state = 'char'
                out.append(ch)
            elif ch == '/' and nxt == '/':
                state = 'line_comment'
                out.extend('  ')
                i += 1
            elif ch == '/' and nxt == '*':
                state = 'block_comment'
                out.extend('  ')
                i += 1
            else:
                out.append(ch)
        elif state == 'string':
            out.append(ch)
            if ch == '\\' and nxt:
                out.append(nxt)
                i += 1
            elif ch == '"':
                state = 'code'
        elif state == 'char':
            out.append(ch)
            if ch == '\\' and nxt:
                out.append(nxt)
                i += 1
            elif ch == "'":
                state = 'code'
        elif state == 'line_comment':
            if ch in '\r\n':
                state = 'code'
                out.append(ch)
            else:
                out.append(' ')
        else:  # block_comment
            if ch == '*' and nxt == '/':
                state = 'code'
                out.extend('  ')
                i += 1
            elif ch in '\r\n':
                out.append(ch)
            else:
                out.append(' ')
        i += 1
    return ''.join(out)

def split_c_statements(src):
    """按顶层分号切分资源定义块，保留括号/字符串中的分号。"""
    statements = []
    cur = []
    depth = 0
    instr = False
    inchar = False
    i = 0
    while i < len(src):
        ch = src[i]
        cur.append(ch)
        if instr:
            if ch == '\\' and i + 1 < len(src):
                i += 1
                cur.append(src[i])
            elif ch == '"':
                instr = False
        elif inchar:
            if ch == '\\' and i + 1 < len(src):
                i += 1
                cur.append(src[i])
            elif ch == "'":
                inchar = False
        elif ch == '"':
            instr = True
        elif ch == "'":
            inchar = True
        elif ch == '(':
            depth += 1
        elif ch == ')':
            depth = max(0, depth - 1)
        elif ch == ';' and depth == 0:
            statements.append(''.join(cur[:-1]).strip())
            cur = []
        i += 1
    if ''.join(cur).strip():
        statements.append(''.join(cur).strip())
    return statements

def default_effect():
    """JSON 中使用明确默认值；times=1 表示 C 引擎中 0 所代表的一次生效。"""
    return {
        'flag': 'EF_NONE',
        'attr': '',
        'value': 0,
        'status': '',
        'layers': 0,
        'rank': 0,
        'target': 'enemy_all',
        'times': 1,
        'cond': 'KC_NONE',
        'cond_arg': 0,
        'cond_arg2': 0,
        'chance': 0,
        'chance_attr_base': -1,
        'chance_neg': False,
        'luck_halve': False,
        'cap': 0,
        'desc': '',
    }

def parse_effect_ctor(expr):
    """解析 ksg_db.c 的 E_ATTR/E_WIN/E_STATUS/E_DEATH/E_MANA 构造器。"""
    m = re.fullmatch(r'\s*(E_ATTR|E_WIN|E_STATUS|E_DEATH|E_MANA)\s*\((.*)\)\s*', expr, re.S)
    if not m:
        return None
    func = m.group(1)
    args = [x.strip() for x in split_top(m.group(2))]
    eff = default_effect()
    if func == 'E_ATTR' and len(args) == 4:
        eff['flag'] = args[0]
        eff['attr'] = args[1]
        eff['value'] = parse_scalar(args[2])
        eff['target'] = parse_target(args[3])
    elif func == 'E_WIN' and len(args) == 3:
        eff['flag'] = args[0]
        eff['value'] = parse_scalar(args[1])
        eff['target'] = parse_target(args[2])
    elif func == 'E_STATUS' and len(args) == 3:
        eff['flag'] = 'EF_STATUS_GIVE'
        eff['status'] = args[0]
        eff['layers'] = parse_scalar(args[1])
        eff['target'] = parse_target(args[2])
    elif func == 'E_DEATH' and len(args) == 3:
        eff['flag'] = 'EF_DEATH'
        eff['chance'] = parse_int(args[0])
        eff['target'] = parse_target(args[1])
        eff['luck_halve'] = parse_int(args[2]) != 0
    elif func == 'E_MANA' and len(args) == 3:
        eff['flag'] = args[0]
        eff['value'] = parse_scalar(args[1])
        eff['target'] = parse_target(args[2])
    else:
        return None
    return eff

def effect_from_add(args):
    """解析 add_eff(w, r, flag, attr, value, status, layers, target)。"""
    if len(args) < 8:
        return None
    eff = default_effect()
    eff['flag'] = args[2].strip()
    eff['attr'] = '' if args[3].strip() == '0' else args[3].strip()
    eff['value'] = parse_scalar(args[4])
    eff['status'] = '' if args[5].strip() == '0' else args[5].strip()
    eff['layers'] = parse_scalar(args[6])
    eff['target'] = parse_target(args[7])
    return eff

def assign_effect_field(eff, field, raw):
    raw = raw.strip()
    if field in ('flag', 'attr', 'status', 'cond'):
        eff[field] = raw
    elif field == 'target':
        eff[field] = parse_target(raw)
    elif field == 'desc':
        eff[field] = cstr(raw)
    elif field in ('chance_neg', 'luck_halve'):
        eff[field] = parse_int(raw) != 0
    elif field == 'times':
        value = parse_int(raw)
        eff[field] = value if value > 0 else 1
    elif field in ('value', 'layers', 'rank', 'cond_arg', 'cond_arg2', 'chance_attr_base'):
        eff[field] = parse_scalar(raw)
    elif field in ('chance', 'cap'):
        eff[field] = parse_int(raw)

def parse_block_effects(src, resource_name):
    """按 C 语句顺序模拟局部 E 变量，输出无 VAR_E 占位符的效果行。"""
    effects = []
    variables = {}
    fields = ('flag|attr|value|status|layers|rank|target|times|cond|cond_arg|cond_arg2|'
              'chance|chance_attr_base|chance_neg|luck_halve|cap|desc')

    for statement in split_c_statements(src):
        s = statement.strip().lstrip('{}').strip()
        if not s:
            continue

        declaration = re.search(r'\bE\s+(e\d*)\s*(?:=\s*(.+))?$', s, re.S)
        if declaration:
            name = declaration.group(1)
            init = declaration.group(2)
            variables[name] = parse_effect_ctor(init) if init else default_effect()
            if variables[name] is None:
                PARSE_WARNINGS.append(f'{resource_name}: 无法解析 {name} 初始化: {init}')
                variables[name] = default_effect()
            continue

        reset = re.search(r'\bmemset\s*\(\s*&\s*(e\d*)\s*,\s*0\s*,', s)
        if reset:
            variables[reset.group(1)] = default_effect()
            continue

        reassignment = re.search(r'\b(e\d*)\s*=\s*(E_[A-Z]+\s*\(.*\))$', s, re.S)
        if reassignment:
            parsed = parse_effect_ctor(reassignment.group(2))
            if parsed is None:
                PARSE_WARNINGS.append(f'{resource_name}: 无法解析 {reassignment.group(1)} 重赋值')
            else:
                variables[reassignment.group(1)] = parsed
            continue

        assignment = re.search(rf'\b(e\d*)\.({fields})\s*=\s*(.+)$', s, re.S)
        if assignment:
            name, field, raw = assignment.groups()
            if name not in variables:
                variables[name] = default_effect()
            assign_effect_field(variables[name], field, raw)
            continue

        args, _ = parse_call(s, 0, 'add_eff')
        if args:
            parsed = effect_from_add(args)
            if parsed is None:
                PARSE_WARNINGS.append(f'{resource_name}: add_eff 参数不足')
            else:
                effects.append(parsed)
            continue

        args, _ = parse_call(s, 0, 'db_eff')
        if args and len(args) >= 3:
            expr = args[2].strip()
            parsed = parse_effect_ctor(expr)
            if parsed is not None:
                effects.append(parsed)
            elif re.fullmatch(r'e\d*', expr) and expr in variables:
                effects.append(copy.deepcopy(variables[expr]))
            else:
                PARSE_WARNINGS.append(f'{resource_name}: 无法解析 db_eff 第三参数: {expr}')
            continue
    return effects

def main():
    PARSE_WARNINGS.clear()
    all_res = []
    for path in SRC_FILES:
        if not os.path.exists(path):
            print('SKIP 文件不存在:', path)
            continue
        with open(path, encoding='utf-8') as f:
            src = f.read()
        res = parse_resources(src)
        print(f'{os.path.basename(path)}: 解析出 {len(res)} 条')
        all_res.extend(res)

    if PARSE_WARNINGS:
        print('解析失败：仍有未解析的效果变量/调用：', file=sys.stderr)
        for warning in PARSE_WARNINGS:
            print('  -', warning, file=sys.stderr)
        return 1

    # 统一补 id
    for i, r in enumerate(all_res):
        r['id'] = i + 1

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', encoding='utf-8') as f:
        json.dump({
            'schema_version': 1,
            'generated_from': [os.path.basename(p) for p in SRC_FILES],
            'resources': all_res,
        }, f, ensure_ascii=False, indent=1)

    print(f'总计 {len(all_res)} 条 -> {OUT}')

    # 统计
    kinds = {}
    for r in all_res:
        kinds[r['kind']] = kinds.get(r['kind'], 0) + 1
    print('分类:', kinds)

    # 抽样打印几条,便于人工核验
    print('\n抽样:')
    empty_eff = [r for r in all_res if not r['effects']]
    print('无效果行条目数(应0):', len(empty_eff))
    for r in all_res[:5]:
        print(' ', r['name'], r['kind'], r['rank'], 'eff=', len(r['effects']))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
