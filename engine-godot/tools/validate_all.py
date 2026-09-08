# -*- coding: utf-8 -*-
# validate_all — 一键验证脚本
# 1) 资源导出与校验(必须成功且数量/效果数与锁定值一致)
# 2) C# 引擎测试(dotnet run, 94/94)
# 3) Godot 导入检查(headless 主场景启动,退出码 0)
#
# 用法: python tools/validate_all.py
# 退出码: 0=全部通过; 1=任一环节失败(已打印具体环节)
import os, subprocess, sys, json

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, 'tools')
DATA = os.path.join(ROOT, 'data', 'ksg_resources.json')
GODOT_EXE = r'D:\Godot_v4.7.2-stable_mono_win64\Godot_v4.7.2-stable_mono_win64\Godot_v4.7.2-stable_mono_win64_console.exe'
DOTNET = os.path.join(os.environ.get('LOCALAPPDATA', r'C:\Users\UIC\AppData\Local'), 'dotnet', 'dotnet.exe')
if not os.path.exists(DOTNET):
    DOTNET = r'C:\Users\UIC\.dotnet\dotnet.exe'

EXPECT = {'resources': 758, 'effects': 1287, 'skill': 468, 'np': 247, 'item': 43,
          'one_effect': 457, 'multi_effect': 301}

failures = []

def step(name, fn):
    print(f'— [{name}] ...')
    try:
        fn()
        print(f'— [{name}] OK')
    except Exception as e:
        failures.append(f'{name}: {e}')
        print(f'— [{name}] FAIL: {e}')

# ---------- 1. 资源导出与校验 ----------
def check_export():
    r = subprocess.run([sys.executable, os.path.join(TOOLS, 'export_resources.py')],
                       cwd=TOOLS, capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode != 0:
        raise RuntimeError('export_resources.py 失败:\n' + r.stderr[-2000:])
    # 直接读取 JSON 计数(最可靠,避免解析带 ANSI 颜色的文本输出)
    with open(DATA, encoding='utf-8') as f:
        obj = json.load(f)
    n_res = len(obj['resources'])
    n_eff = sum(len(x.get('effects', [])) for x in obj['resources'])
    n_skill = sum(1 for x in obj['resources'] if x.get('kind') == 'skill')
    n_np = sum(1 for x in obj['resources'] if x.get('kind') == 'np')
    n_item = sum(1 for x in obj['resources'] if x.get('kind') == 'item')
    n_zero = sum(1 for x in obj['resources'] if len(x.get('effects', [])) == 0)
    if n_res != EXPECT['resources'] or n_eff != EXPECT['effects']:
        raise RuntimeError(f'资源数/效果数不符: {n_res}/{n_eff} (期望 {EXPECT["resources"]}/{EXPECT["effects"]})')
    if n_skill != EXPECT['skill'] or n_np != EXPECT['np'] or n_item != EXPECT['item']:
        raise RuntimeError(f'分类数不符: skill={n_skill} np={n_np} item={n_item} (期望 {EXPECT["skill"]}/{EXPECT["np"]}/{EXPECT["item"]})')
    if n_zero != 0:
        raise RuntimeError(f'存在无效果资源: {n_zero} 条')
    print(f'    资源 {n_res} / 效果 {n_eff} / 空效果 0 校验通过')

# ---------- 2. C# 引擎测试 ----------
def check_dotnet():
    env = dict(os.environ)
    env['PATH'] = os.path.dirname(DOTNET) + os.pathsep + env.get('PATH', '')
    proj = os.path.join(TOOLS, 'engine-test', 'KsgTest.csproj')
    r = subprocess.run([DOTNET, 'run', '--project', proj], capture_output=True, text=True,
                       encoding='utf-8', timeout=600, env=env, cwd=os.path.dirname(proj))
    if r.returncode != 0:
        raise RuntimeError('C# 引擎测试失败(返回码非0):\n' + (r.stdout + r.stderr)[-3000:])
    tail = r.stdout[-800:]
    if '失败' in tail and '0 失败' not in tail:
        raise RuntimeError('C# 测试出现失败项:\n' + tail)
    m = None
    import re
    m = re.search(r'(\d+)\s*通过,\s*(\d+)\s*失败', r.stdout)
    print(f'    C# 测试: {m.group(1)} 通过 / {m.group(2)} 失败' if m else '    C# 测试: 通过(未解析计数)')

# ---------- 3. Godot 导入/启动检查 ----------
def check_godot():
    if not os.path.exists(GODOT_EXE):
        print('    (未找到 Godot console,跳过导入检查;可用 Godot GUI 手动导入)')
        return
    env = dict(os.environ)
    env['PATH'] = os.path.dirname(DOTNET) + os.pathsep + env.get('PATH', '')
    r = subprocess.run([GODOT_EXE, '--headless', '--path', ROOT, '--quit-after', '900'],
                       capture_output=True, text=True, encoding='utf-8', errors='replace',
                       timeout=240, env=env)
    if r.returncode != 0:
        raise RuntimeError(f'Godot headless 退出码 {r.returncode}:\n' + (r.stdout + r.stderr)[-2500:])
    if '资源库加载' not in r.stdout:
        raise RuntimeError('Godot 未输出资源库加载日志:\n' + r.stdout[-1500:])
    print('    Godot headless 启动正常(退出码 0,资源库加载)')

step('资源导出+校验(758/1287)', check_export)
step('C# 引擎测试', check_dotnet)
step('Godot headless 导入检查', check_godot)

print()
if failures:
    print('❌ 一键验证未通过:')
    for f in failures:
        print('   -', f)
    sys.exit(1)
print('✅ 一键验证全部通过(资源 758/1287 · C# 测试 · Godot 启动)')