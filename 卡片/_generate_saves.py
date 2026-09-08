# -*- coding: utf-8 -*-
"""Resolve parsed cards to game resources and generate save JSON files (v3 + additive rank).
Reads _parsed_cards.json + ksg_resources.json. Writes:
  - 卡片导入存档/<id>.json        (staging copies)
  - copies into KsG空想圣杯-Windows版/saves and engine-godot/build/saves
  - _import_report.txt           (full mapping manifest)
"""
import json, os, re, shutil

BASE = r"D:\desktop\wolfkill"
CARDS_DIR = os.path.join(BASE, "卡片")
DB_PATH = os.path.join(BASE, "engine-godot", "data", "ksg_resources.json")
OUT_DIR = os.path.join(CARDS_DIR, "卡片导入存档")
REPORT = os.path.join(CARDS_DIR, "_import_report.txt")
GAME_SAVE_DIRS = [
    os.path.join(BASE, "KsG空想圣杯-Windows版", "saves"),
    os.path.join(BASE, "engine-godot", "build", "saves"),
]

# ---------- resource DB ----------
db = json.load(open(DB_PATH, encoding="utf-8"))["resources"]
BY_ID = {r["id"]: r for r in db}
BY_NAME = {}
for r in db:
    BY_NAME.setdefault(r["name"], []).append(r)

# ---------- name aliases: card template -> db name ----------
ALIAS = {
    "气息遮蔽": "气息感知",
    "阵地制造": "阵地制作",
    "已阵守护": "己阵防御",
    "恶龙之血凯": "恶龙之血铠",
    "星之开拓者": "星之开拓者EX",
    "信任的拍档": "信任的伙伴",
    "投影魔术": "投影魔术(从者)",
}


def norm(name):
    if not name:
        return ""
    s = name.replace(" ", "").replace("　", "")
    s = s.replace("（", "(").replace("）", ")")
    return s.strip()


def lookup(name):
    """Resolve a card block name to a db resource id (exact -> alias -> normalized)."""
    if not name:
        return None
    if name in ALIAS:
        name = ALIAS[name]
    if name in BY_NAME:
        return BY_NAME[name][0]
    n = norm(name)
    if n in BY_NAME:
        return BY_NAME[n][0]
    for dn, res in BY_NAME.items():
        if norm(dn) == n:
            return res[0]
    return None


# ---------- card identity curation ----------
DISPLAY_NAMES = {
    "saber 托尔芬(1).xlsx": "托尔芬",
    "Shielder.克珀珊特 (天海) (1).xlsx": "克珀珊特（天海）",
    "仇远.xlsx": "仇远",
    "卡斯托尔.xlsx": "卡斯托尔",
    "张角.xlsx": "张角",
    "杀 狗子.xlsx": "狗子",
    "玻吕克斯.xlsx": "玻吕克斯",
    "盾 奶龙.xlsx": "奶龙",
    "鲜血君主 蒙格.xlsx": "蒙格",
    "黎曦夜.xlsx": "黎曦夜",
}
CLASS_MAP = {"Saber": 1, "Lancer": 2, "Archer": 3, "Rider": 4, "Caster": 5,
             "Assassin": 6, "Berserker": 7, "Ruler": 8, "Avenger": 9, "Shielder": 10}
HIDDEN_MAP = {"天": 0, "地": 1, "人": 2, "星": 3, "兽": 4}
TRAIT_BITS = {"人型": 1, "神性": 2, "魔性": 4, "龙种": 8, "猛兽": 16, "魔兽": 32, "构装体": 64}
FACTION_MAP = {
    "秩序-善": (0, 0), "秩序-中庸": (0, 1), "秩序-恶": (0, 2),
    "中立-善": (1, 0), "中立-中庸": (1, 1), "中立-恶": (1, 2),
    "混沌-善": (2, 0), "混沌-中庸": (2, 1), "混沌-恶": (2, 2),
}
TRAIT_CURATION = {
    "鲜血君主 蒙格.xlsx": ["人型", "魔性", "神性"],
    "盾 奶龙.xlsx": ["人型", "龙种", "猛兽"],
}

RANK_ORDER = {"E": 0, "D": 1, "C": 2, "B": 3, "A": 4, "EX": 5}

start_id = 21
cards = json.load(open(os.path.join(CARDS_DIR, "_parsed_cards.json"), encoding="utf-8"))

report = []
report.append("=== 卡片导入清单 (模板 -> 游戏资源) ===\n")

saves = []
for idx, card in enumerate(cards):
    src = card["source"]
    name = DISPLAY_NAMES[src]
    cinfo = f"\n[{start_id + idx}] {name} (源: {src}) utype={card['utype']} class={card['class']} level={card['level']}"
    report.append(cinfo)
    # ---- identity ----
    cls_raw = (card.get("class") or "").replace("（职阶2）", "").replace("(职阶2)", "").strip()
    servant_class = CLASS_MAP.get(cls_raw, 0)
    hidden_raw = (card.get("hidden_attr") or "").strip()
    if card["utype"] == 2:
        hidden_attr = -1
    else:
        hidden_attr = HIDDEN_MAP.get(hidden_raw, 2 if hidden_raw in ("", "地属性") else 2)
    if hidden_raw == "地属性":
        hidden_attr = 1
    traits = 0
    trait_list = TRAIT_CURATION.get(src, card.get("traits") or [])
    for t in trait_list:
        traits |= TRAIT_BITS.get(t, 0)
    if traits == 0:
        traits = 1  # 保底人型
    faction = card.get("faction") or "中立-中庸"
    law, moral = FACTION_MAP.get(faction, (1, 1))
    level = card.get("level") or 40
    attr = list(card.get("attr") or [0] * 7)
    if card["utype"] == 2:
        attr[5] = 0  # 御主无宝具
        attr[6] = card["attr"][6] if len(card["attr"]) > 6 else 0
    else:
        attr[6] = 0
    _ = attr[5]

    # ---- resources ----
    skills, phantasms = [], []
    for blk in card.get("resources") or []:
        tpl = (blk.get("template") or "").strip()
        cname = (blk.get("name") or "").strip()
        search = tpl or cname or ""
        res = lookup(search)
        note_tpl = ""
        if res is None:
            report.append(f"  !! 未解析: 模板={tpl!r} 名称={cname!r} 等级={blk.get('rank')} → 跳过")
            continue
        rid, rkind = res["id"], res["kind"]
        # 若模板与名称不同且模板可解析,则以模板为准; 否则已用名称解析
        if not tpl and cname:
            note_tpl = f"原型:{res['name']}"
        elif tpl and tpl != res["name"]:
            note_tpl = f"原型:{tpl}"
        entry = {"id": rid}
        dname = cname if cname and cname != res["name"] else res["name"]
        if dname.startswith("(") and dname.endswith(")") and "自定义" in dname:
            dname = res["name"]
        if dname != res["name"]:
            entry["dname"] = dname
        rank = blk.get("rank")
        base_rank = res.get("rank") or "C"
        if rank and rank != base_rank:
            entry["rank"] = rank
        if note_tpl:
            entry["note"] = note_tpl
        if rkind == "np":
            phantasms.append(entry)
            report.append(f"  [宝具] {res['name']}(id={rid}) 卡面等级={rank or base_rank} (库基={base_rank}) 自定义名={dname or '-'} {note_tpl}")
        else:
            skills.append(entry)
            report.append(f"  [技能] {res['name']}(id={rid}) 卡面等级={rank or base_rank} (库基={base_rank}) 自定义名={dname or '-'} {note_tpl}")

    save = {
        "version": 3,
        "id": start_id + idx,
        "name": name,
        "true_name": name,
        "avatar_path": "",
        "utype": card["utype"],
        "servant_class": servant_class,
        "hidden_attr": hidden_attr,
        "level": level,
        "faction": 0,
        "law": law,
        "moral": moral,
        "traits": traits,
        "fp": 1,
        "cs": 3,
        "roaming": 0,
        "alive": True,
        "main_job": card.get("main_job") or "",
        "sub_job": card.get("sub_job") or "",
        "attr": attr,
        "attr_perm": [0, 0, 0, 0, 0, 0, 0],
        "mp_cap": 150,
        "mp_floor": -100,
        "mp_cur": 150,
        "skills": skills,
        "phantasms": phantasms,
        "items": [],
        "status": [],
    }
    saves.append(save)
    report.append(f"  职阶={servant_class} 隐属={hidden_attr} 特性位={traits}({trait_list}) 阵营={faction}({law},{moral}) attr={attr}")

# ---------- write ----------
os.makedirs(OUT_DIR, exist_ok=True)
for save in saves:
    path = os.path.join(OUT_DIR, f"{save['id']}.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(save, f, ensure_ascii=False, indent=2)
    for d in GAME_SAVE_DIRS:
        os.makedirs(d, exist_ok=True)
        shutil.copyfile(path, os.path.join(d, f"{save['id']}.json"))
    report.append(f"  写入: {path}")

with open(REPORT, "w", encoding="utf-8") as f:
    f.write("\n".join(report))
print("\n".join(report))
print(f"\n共生成 {len(saves)} 张卡存档 -> {OUT_DIR}")