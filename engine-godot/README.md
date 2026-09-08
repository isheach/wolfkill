# 空想圣杯 KsG · Godot 可视化版(engine-godot)

Godot 4.7.2 (Mono/C#) 实现的《空想圣杯》五书图形化版本:建卡 → 本地存档 → 对战结算。
规则引擎与命令行版(`../engine/` C 引擎)行为对齐,资源数据由脚本从 C 源码自动导出,不手写 JSON。

## 环境要求

| 组件 | 版本/路径 |
|---|---|
| Godot | `D:\Godot_v4.7.2-stable_mono_win64\Godot_v4.7.2-stable_mono_win64\Godot_v4.7.2-stable_mono_win64.exe`(注意实际路径比用户给出的多一层目录;可用 `Get-Command godot*` 或直接搜索) |
| .NET SDK | 8.x(本项目用 8.0.424),安装在 `C:\Users\UIC\.dotnet`;若 PowerShell 未继承新用户环境,请先 `$env:DOTNET_ROOT='C:\Users\UIC\.dotnet'; $env:PATH="$env:DOTNET_ROOT;"+$env:PATH` |
| 数据源 | C 引擎 `../engine/ksg_data.c` 与 `../engine/ksg_db.c` |

## 目录结构

```
engine-godot/
├── data/ksg_resources.json     ← 生成的数据契约(758 资源 / 1433 效果),勿手改
├── tools/export_resources.py   ← 从 C 源码导出 JSON
├── tools/validate_resources.py ← 校验 JSON(数量/枚举/效果完整性)
├── tools/engine-test/          ← 纯 C# 引擎测试(不依赖 Godot 渲染)
├── tools/card-battle-test/     ← 自定义导入卡 10×9 全对战/确定性/不变式验证
├── scripts/Model/              ← ResourceDef / UnitDef / StatusKind / LeylineDef 等
├── scripts/Engine/             ← KsgWorld / KsgBattle / KsgEffects / Dice
├── scripts/Data/ResourceDb.cs  ← JSON 加载与搜索
├── scripts/Save/               ← 存档(version=2, 稳定 ID, 本地 user://saves)
├── scripts/UI/                 ← 主菜单/建卡/单位一览/对战/词典/场景路由
├── scenes/main_menu.tscn       ← 入口场景
└── project.godot
```

## 数据导出与校验(数据契约)

必须顺序执行:先导出,再校验,之后 UI 才能拿到数据。

```powershell
# 在 engine-godot 目录
python tools/export_resources.py     # 输出 data/ksg_resources.json
python tools/validate_resources.py   # 校验: 758 资源/1433 效果/无空效果/枚举合法
```

约定(任务书阶段 E):资源库新增条目必须先通过导出与校验,再进入 UI;禁止手写与 C 数据脱节的 JSON。

## 构建

```powershell
cd engine-godot
$env:DOTNET_ROOT='C:\Users\UIC\.dotnet'; $env:PATH="$env:DOTNET_ROOT;"+$env:PATH
dotnet build KsgGodot.csproj
# 期望: 0 warning / 0 error
```

`.godot/**/*.cs` 与 `tools/**/*.cs` 已从编译中排除(避免缓存文件造成重复程序集属性)。

## 运行

```powershell
# 编辑/运行(GUI 窗口)
& "D:\Godot_v4.7.2-stable_mono_win64\Godot_v4.7.2-stable_mono_win64\Godot_v4.7.2-stable_mono_win64.exe" --path engine-godot

# 或直接双击 run.bat(自动定位 Godot + dotnet build + 启动)
```

> **关于 Windows 可执行版打包**：Godot 导出需要官方导出模板（`Godot_v4.7.2-stable_export_templates.tpz`，
> 约 1.2 GB，GitHub 下载慢时可达数小时）。在模板就绪前，交付形态为
> `run.bat + Godot 项目目录`（使用本机 Godot 直接运行）；模板下载完成后可执行
> `Godot --headless --path . --export-release "Windows Desktop" build/KsgGodot.exe` 生成单文件 exe。
> ⚠️ 导出需先在 Godot 编辑器里配置一次 export presets（已提供 `export_presets.cfg` 草稿）。

# Headless 冒烟(PowerShell/CI)
& "D:\Godot_v4.7.2-stable_mono_win64\Godot_v4.7.2-stable_mono_win64\Godot_v4.7.2-stable_mono_win64_console.exe" --headless --path engine-godot --quit-after 600
```

启动可预期输出:

```
资源库审计: 总资源=758, 总效果=1433, 技能/宝具/礼装=468/247/43, 未知枚举=0
资源库抽查: 12/12 通过（...）
资源库加载: 758 条
启动存档扫描: 发现并载入 N 张卡牌   (导入 10 张自定义卡后为 20)
```

## 操作流程

1. 主菜单 → 建卡向导:选从者/御主、职阶、隐藏属性、等级滑条、属性滑条分配、搜索技能/宝具/礼装加入、保存。
2. 单位一览:查看会话卡与本地存档,载入/删除/覆盖确认。
3. 对战:从已建卡选择左右双方(**单击可多选**,默认各 1 张;可调战场宽度)→ 战术 → 主要属性 → 主要/最终工序发技能宝具(可手动输入状态层数)→ 指令(冲锋/追击/掩护/死斗/撤退)→ 决胜;
   发动前可选目标(自身/敌方主力/敌方全体/己方全体);同一资源每工序限一次;死斗后禁止撤退;追击只加一次撤退成本。
4. 词典:状态/特效说明 + 全部资源原文(可搜索/过滤)。

## 测试

```powershell
cd engine-godot/tools/engine-test
$env:DOTNET_ROOT='C:\Users\UIC\.dotnet'; $env:PATH="$env:DOTNET_ROOT;"+$env:PATH
dotnet run
# 期望: 207 通过, 0 失败(覆盖建模/存档/战斗状态机/状态/即死/穿透/召唤/灵脉/圣杯/礼装次数/世界运营/多单位槽位与等级补正等)
```

卡片对战验证(自定义导入卡 10×9 全对战 + 确定性 + 不变式):

```powershell
cd engine-godot/tools/card-battle-test
$env:DOTNET_ROOT='C:\Users\UIC\.dotnet'; $env:PATH="$env:DOTNET_ROOT;"+$env:PATH
dotnet run
# 期望: 90/90 全对战零崩溃零规则违例, 90/90 确定性复跑, 4292 断言通过 / 0 失败
# 可选: dotnet run -- --debug <li> <ri>  打印单场战斗完整日志
```

Godot UI 自动测试(800×600 隔离目录)曾累计通过;临时测试场景在阶段 C 验收后已清理。

## 自定义卡片导入(卡片/ 文件夹)

- 解析: `../卡片/_parse_cards.py` 用 lxml 读取 xlsx(官方 xlsx 含无效 dataValidation XML,openpyxl 会拒绝),输出 `_parsed_cards.json` 与 `_parse_report.txt`。
- 生成: `../卡片/_generate_saves.py` 把每张卡的技能/宝具解析为"最近似的官方资源"(ALIAS 修正别名)+ 卡面等级,生成存档 `../卡片/卡片导入存档/{21..30}.json` 并复制到
  `KsG空想圣杯-Windows版/saves` 与 `build/saves`。
- 保真策略(用户已确认): 战斗执行引擎原生效果;原版 OC 自然语言效果无法表达,保留为 dname 与 note"原型:X"。
- 等级持久化: 存档 `version=3` 新增可选 `rank` 字段(购入等级 ≠ 库基等级时才写入),读档经 `CardBuildRules.RankedCopy` 按等级变体表/系数缩放恢复;旧档(无 rank)完全兼容。
- 职阶扩展: Shielder 为职阶序号 10,建卡/单位一览已支持,职阶技能"己阵防御"。
- 已导入卡片: 克珀珊特(天海)/托尔芬/仇远/卡斯托尔(御主)/张角/狗子/玻吕克斯(御主)/奶龙/蒙格/黎曦夜(御主)。

## 多卡对战(可多选, 规则书 3.2 / 第六节)

- 对战选择页左右两侧均可**单击多选**卡片: 普通单击切换该卡的选中状态(追加/移除), 无需 Ctrl;
  列表下方实时显示"已选 N 张 / 容量 M"。也可自定义战场宽度(1~7)。
- 战斗位构成按规则书 3.2 的宽度模板: 宽1=[主力], 宽2=[主力+支援], 宽3=[主力+辅助+支援],
  宽4=[主力+辅助+仆役+支援], 宽5+=[主力+辅助×(n-3)+仆役+支援]。
- 战斗属性: 主力位全额计入、辅助/仆役位减半、支援位不计入(规则书 3.1)。
- 仆役位只能由仆役类单位(召唤物/人偶/使魔 UType≥3)占据; 非仆役单位排到仆役位时, 该位
  降级为支援位(不贡献战斗属性)。超出容量(宽度)的单位在开战时被移出战斗位。
- 胜率计算(规则书 第六节): 基础胜率=双方战斗属性差, 另外**主力位单位等级提供 +等级×1% 胜率补正**;
  主力退场/撤退时按 辅助→仆役→支援 顺序替补并重算等级补正。
- 每方必有一名主力位; 主力退场后由剩余战斗位单位按序补位(规则书 3.2)。
- 引擎测试覆盖: 槽位模板 7 档、容量移出、仆役位、支援位属性、等级补正与主力替补(engine-test 207/207)。

## 存档位置

`user://saves/*.json` ——Godot 用户数据目录中(Windows 通常在 `%APPDATA%\Godot\app_userdata\<项目名>\saves`);
便携版在 exe 旁的 `saves/`(SaveManager.InitializePortable)。
- 版本 3:保存资源 ID + 可选购入等级 `rank`;版本 1 名称存档可兼容读取。
- 首次保存分配稳定正整数 ID;同一 ID 再次保存为覆盖。
- 每张卡恢复/使用资源时深拷贝,回转与储备不跨卡共享。

## 常见问题

| 现象 | 处理 |
|---|---|
| `dotnet` 找不到 | 先设 `$env:DOTNET_ROOT='C:\Users\UIC\.dotnet'` 并加入 PATH;确认 `dotnet --version` 输出 8.x |
| 资源库加载失败/审计数不对 | 重新执行 `python tools/export_resources.py` 与 `python tools/validate_resources.py`,确认 758/1287 |
| Godot 报重复程序集 | 删除 `.godot/mono/temp` 缓存后重新 build;`.godot` 是本机生成缓存,交付时可清理 |
| 沙箱提示无法读取根证书库 | 仅环境提示,不影响本项目运行(项目无网络依赖) |
| 无法打开场景 | 确保先用有 GUI 的 Godot 导入一次项目(或 `--headless --import`) |

## 与 C 引擎的关系

- C 引擎 `../engine/` 是可运行基线(控制台),Godot 版以其行为为对照;两者共用同一数据契约。
- 本目录不修改 `../engine/` 任何文件。
- 已实现规则:`docs/五书关系自查表.md` 提供逐条对照;Godot 版当前对"世界运营层"规则(昼夜行动补正、协助/介入、完整工房、固有结界、资金/储备经济)以数据/接口简化,在自查表中明确标注,属后续路线。

## 后续路线(未实现标记)

- 加符/EX 数值翻倍、固有结界生成链接、资金/储备经济完整结算
- C ↔ C# 固定种子回归对照自动化(目前 C# 侧已确定性;对照器待接线)
- UI 多语言/主题定制、音效与动画

## 变更记录

见 `../继续开发规划.md`(阶段 A/B/C 已完成验收;阶段 D 严格状态机与规则测试完成;阶段 E 世界规则与固定种子完成;阶段 F 本文档与一键验证/打包推进)。