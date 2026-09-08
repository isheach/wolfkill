/* ksg_ui.c — 空想圣杯引擎:交互式建卡与对战界面 */
#include "ksg.h"

/* ================= UI 基础 ================= */

void ks_ui_pause(const char *msg) {
    printf("%s", msg ? msg : "按回车继续...");
    fflush(stdout);
    int c;
    do { c = getchar(); } while (c != '\n' && c != EOF);
}

void ks_ui_ask_str(const char *prompt, char *buf, int size) {
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, size, stdin)) { buf[0] = 0; return; }
    buf[strcspn(buf, "\n")] = 0;
}

int ks_ui_ask_int(const char *prompt, int lo, int hi, int def) {
    char buf[32];
    for (;;) {
        printf("%s[%d]: ", prompt, def);
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) return (feof(stdin) ? -999 : def);
        buf[strcspn(buf, "\n")] = 0;
        if (buf[0] == 0) return def;
        int v = atoi(buf);
        if (v >= lo && v <= hi) return v;
        printf("  输入范围 %d~%d\n", lo, hi);
    }
}

/* 菜单:返回选项索引(0..count-1),或 -1 取消 */
int ks_ui_menu(const char *title, const char **items, int count) {
    printf("\n──── %s ────\n", title);
    for (int i = 0; i < count; i++)
        printf("  %2d) %s\n", i + 1, items[i]);
    printf("  %2d) 取消\n", 0);
    int r = ks_ui_ask_int("请选择", 0, count, 1);
    if (r == -999) return -1;   /* EOF:取消 */
    return r - 1;
}

/* ================= 资源浏览 ================= */

/* 按“面向/类型”列出技能 */
static int list_skills_by_type(ks_world_t *w, int type, char *buf, int bufsize) {
    int n = 0;
    for (int i = 0; i < w->res_count; i++) {
        ks_res_t *r = &w->res[i];
        if (r->kind == KS_R_SKILL && r->type == type) {
            n += snprintf(buf + n, bufsize - n, "%s[%s](魔%d/回%d)\n",
                          r->name, ks_rank_name(r->rank), r->cost, r->recast);
        }
    }
    return n;
}

/* 按面向列出宝具 */
static int list_np_by_focus(ks_world_t *w, int focus, char *buf, int bufsize) {
    int n = 0;
    for (int i = 0; i < w->res_count; i++) {
        ks_res_t *r = &w->res[i];
        if (r->kind == KS_R_PHANTASM && r->focus == focus) {
            n += snprintf(buf + n, bufsize - n, "%s[%s](魔%d/回%d)\n",
                          r->name, ks_rank_name(r->rank), r->cost, r->recast);
        }
    }
    return n;
}

static void list_all_items(ks_world_t *w, char *buf, int bufsize) {
    int n = 0;
    for (int i = 0; i < w->res_count; i++) {
        ks_res_t *r = &w->res[i];
        if (r->kind == KS_R_ITEM || r->kind == KS_R_TECH)
            n += snprintf(buf + n, bufsize - n, "%s[%s]\n", r->name, ks_rank_name(r->rank));
    }
}

/* ================= 建卡:从者 ================= */

typedef struct {
    const char *name;
    int base[6];
    int feat_cnt;
    const char *skills[3];   /* 职阶技能(演示用) */
} class_sheet_t;

static const class_sheet_t CLASS_TABLE[] = {
    { "Saber",     {20,20,20,20,20,0}, 2, {"对魔力","骑乘",NULL} },
    { "Lancer",    {10,10,30,20,10,0}, 1, {"对魔力",NULL,NULL} },
    { "Archer",    {20,10,20,0,20,0},  2, {"对魔力","单独行动",NULL} },
    { "Rider",     {10,20,20,0,20,0},  2, {"对魔力","骑乘",NULL} },
    { "Caster",    {0,0,0,30,20,0},    2, {"道具制作","阵地制作",NULL} },
    { "Assassin",  {0,0,20,0,20,0},    1, {"气息遮蔽",NULL,NULL} },
    { "Berserker", {20,20,20,0,0,0},   1, {"狂化",NULL,NULL} },
    { "Ruler",     {10,20,10,20,30,0}, 3, {"对魔力","真名识破","神明裁决"} },
    { "Avenger",   {10,20,20,20,0,0},  3, {"复仇者","忘却补正","自我回复"} },
};

static const char *FOCUS_ITEMS[] = {
    "决战", "即死", "魔剑", "防御", "进攻", "增益", "召唤", "状态", "补给", "特殊", "特攻"
};

int ks_ui_create_servant(ks_world_t *w) {
    printf("\n════════════ 从者建卡 ════════════\n");
    char name[KSG_NAME_MAX], tname[KSG_NAME_MAX];
    ks_ui_ask_str("角色代号(显示名): ", name, sizeof(name));
    ks_ui_ask_str("真名: ", tname, sizeof(tname));
    if (!name[0]) strcpy(name, "无名从者");
    if (!tname[0]) strcpy(tname, name);

    /* 1. 选职阶 */
    const char *cls_items[16];
    int cls_n = (int)(sizeof(CLASS_TABLE) / sizeof(CLASS_TABLE[0]));
    char cls_buf[24][64];
    for (int i = 0; i < cls_n; i++) {
        snprintf(cls_buf[i], sizeof(cls_buf[i]), "%s  筋%d 耐%d 敏%d 魔%d 幸%d",
                 CLASS_TABLE[i].name, CLASS_TABLE[i].base[0], CLASS_TABLE[i].base[1],
                 CLASS_TABLE[i].base[2], CLASS_TABLE[i].base[3], CLASS_TABLE[i].base[4]);
        cls_items[i] = cls_buf[i];
    }
    int ci = ks_ui_menu("选择职阶", cls_items, cls_n);
    if (ci < 0) return -1;
    const class_sheet_t *cs = &CLASS_TABLE[ci];

    /* 2. 选隐藏属性(规则:附录一——决定初始等级范围与RP/特性) */
    static const char *hidden_items[] = {
        "天(60~70:神性/魔性) ",
        "地(50~70:本土幻想英灵)",
        "人(40~60:历史英灵)  ",
        "星(40~60:人类希望)  ",
        "兽(固定70:对人类威胁)",
    };
    int hi = ks_ui_menu("选择隐藏属性", hidden_items, 5);
    if (hi < 0) hi = 2;   /* 默认人 */
    int lv_lo = 40, lv_hi = 70, lv_def = 60;
    switch (hi) {
        case 0: lv_lo = 60; lv_hi = 70; lv_def = 60; break;   /* 天 */
        case 1: lv_lo = 50; lv_hi = 70; lv_def = 60; break;   /* 地 */
        case 2: /* 人 */ lv_hi = 60; break;
        case 3: /* 星 */ lv_hi = 60; break;
        case 4: lv_lo = 70; lv_hi = 70; lv_def = 70; break;   /* 兽 */
    }

    /* 3. 选初始等级(范围由隐藏属性决定) */
    char lvprompt[64];
    snprintf(lvprompt, sizeof(lvprompt), "初始等级(%d~%d)", lv_lo, lv_hi);
    int level = ks_ui_ask_int(lvprompt, lv_lo, lv_hi, lv_def);

    /* 4. 分配属性(每5点,总和 120+(lv-40)*2) */
    int pts = 120 + (level - 40) * 2;
    int alloc[6] = {0,0,0,0,0,0};
    printf("\n分配属性(Lv%d → 可分配 %d 点,每项上限60,5点为一档)\n", level, pts);
    printf("  [A-][10] 取消分配, 1~6 选择属性, +/- 增减5点\n");
    for (;;) {
        printf("\r  剩余:%3d  | ", pts);
        for (int i = 0; i < 6; i++)
            printf("%s:%d(%d) ", ks_attr_name(i), cs->base[i] + alloc[i], alloc[i]);
        printf("\n");
        char buf[16];
        if (!fgets(buf, sizeof(buf), stdin)) break;
        buf[strcspn(buf, "\n")] = 0;
        if (buf[0] == 0 || buf[0] == 'd') break;
        if (buf[0] >= '1' && buf[0] <= '6') {
            int ai = buf[0] - '1';
            int op = buf[1] == '+' ? 1 : (buf[1] == '-' ? -1 : 0);
            if (op == 0) {
                printf("  用法: <属性号><+或-> 例如 1+ 为筋力+5\n");
                continue;
            }
            if (op > 0) {
                if (pts < 5) { printf("  点数不足\n"); continue; }
                if (alloc[ai] >= 60) { printf("  该属性已达上限60\n"); continue; }
                alloc[ai] += 5; pts -= 5;
            } else {
                if (alloc[ai] <= 0) { printf("  该属性分配为0\n"); continue; }
                alloc[ai] -= 5; pts += 5;
            }
        } else if (buf[0] == 'a' || buf[0] == 'A') {
            /* 自动均分 */
            while (pts > 0) {
                for (int i = 0; i < 6 && pts > 0; i++)
                    if (alloc[i] < 60) { alloc[i] += 5; pts -= 5; }
            }
        } else {
            printf("  用法: 1+ / 2- / a(自动) / 回车(完成)\n");
        }
    }

    /* 4. 选保有技能:六面向各一个 */
    static const char *skill_faces[] = { "天赋", "技艺", "祝福", "荣冠", "兵器", "魔术" };
    int face_types[] = { KS_T_TALENT, KS_T_TECHNIQUE, KS_T_BLESS,
                         KS_T_CROWN, KS_T_WEAPON, KS_T_MAGIC };
    int chosen_skill[6] = {0,0,0,0,0,0};
    printf("\n── 保有技能选择(每种面向至多1个) ──\n");
    for (int f = 0; f < 6; f++) {
        printf("\n[%s] 可选技能:\n", skill_faces[f]);
        char listbuf[4096] = "";
        list_skills_by_type(w, face_types[f], listbuf, sizeof(listbuf));
        printf("%s", listbuf[0] ? listbuf : "(资源库暂无该面向条目)\n");
        printf("请输入要购买的技能名(直接回车跳过): ");
        char sname[KSG_NAME_MAX];
        ks_ui_ask_str("", sname, sizeof(sname));
        if (sname[0]) {
            int rid = ks_world_find_res(w, sname);
            if (rid) { chosen_skill[f] = rid; printf("  已购买: %s\n", sname); }
            else printf("  未找到 %s,跳过\n", sname);
        }
    }

    /* 5. 选宝具:一个主宝具 + 可选一个 */
    int chosen_np[2] = {0, 0};
    printf("\n── 宝具选择(至多2件,每件面向不可重复) ──\n");
    for (int k = 0; k < 2; k++) {
        printf("\n第 %d 件宝具:\n", k + 1);
        int fi = ks_ui_menu("宝具面向", FOCUS_ITEMS, 11);
        if (fi < 0) break;
        char listbuf[4096] = "";
        list_np_by_focus(w, fi + 1, listbuf, sizeof(listbuf));
        printf("%s", listbuf[0] ? listbuf : "(该面向暂无条目)\n");
        char sname[KSG_NAME_MAX];
        ks_ui_ask_str("请输入宝具名(回车跳过): ", sname, sizeof(sname));
        if (sname[0]) {
            int rid = ks_world_find_res(w, sname);
            if (rid) { chosen_np[k] = rid; printf("  已获得: %s\n", sname); }
            else { printf("  未找到 %s,跳过\n", sname); k--; }
        } else {
            k--; /* 重新选,直到2件或明确取消 */
            if (k < 0) break;
        }
    }

    /* 6. 创建单位 */
    int uid = ks_unit_new(w, name, tname, KS_U_SERVANT, level, 0);
    ks_unit_t *u = &w->units[uid - 1];
    u->servant_class = ci + 1;
    u->hidden_attr = hi;
    for (int a = 0; a < 6; a++) u->attr[a] = cs->base[a] + alloc[a];
    u->mp.cap = 150; u->mp.floor = -100; u->mp.cur = 50;
    u->fp = 1; u->cs = 3;
    u->traits = TR_HUMAN;
    /* 隐藏属性附加特性(规则:附录一——天=神性/魔性,兽=对人类威胁,星=希望) */
    if (hi == 0) u->traits |= TR_DIVINITY;
    if (u->utype == KS_U_SERVANT) {
        /* 兽固定持有[对人类威胁](以特性占位) */
    }
    u->al_law = KS_MID; u->al_moral = KS_NEUT;
    /* 职阶技能 */
    for (int s = 0; s < cs->feat_cnt; s++) {
        int rid = ks_world_find_res(w, cs->skills[s]);
        if (rid) ks_unit_add_res(w, uid, rid);
    }
    /* 保有技能 */
    for (int f = 0; f < 6; f++)
        if (chosen_skill[f]) ks_unit_add_res(w, uid, chosen_skill[f]);
    /* 宝具 */
    for (int k = 0; k < 2; k++)
        if (chosen_np[k]) ks_unit_add_res(w, uid, chosen_np[k]);

    printf("\n✅ 从者创建完成: %s (%s) Lv%d [%s](隐藏属性:%s)\n", u->name, u->true_name, level,
           cs->name,
           (hi == 0) ? "天" : (hi == 1) ? "地" : (hi == 2) ? "人" : (hi == 3) ? "星" : "兽");
    ks_data_print_unit(w, uid);
    return uid;
}

/* ================= 建卡:御主 ================= */

int ks_ui_create_master(ks_world_t *w) {
    printf("\n════════════ 御主建卡 ════════════\n");
    char name[KSG_NAME_MAX], tname[KSG_NAME_MAX];
    ks_ui_ask_str("角色代号(显示名): ", name, sizeof(name));
    ks_ui_ask_str("真名: ", tname, sizeof(tname));
    if (!name[0]) strcpy(name, "无名御主");
    if (!tname[0]) strcpy(tname, name);

    static const char *jobs[] = { "魔术师", "体术师", "异能者", "代行者", "执法者" };
    int ji = ks_ui_menu("选择主职业", jobs, 5);
    if (ji < 0) return -1;

    int level = ks_ui_ask_int("初始等级(10/20/30/40)", 10, 40, 40);

    /* 属性:80点,+回路 */
    int pts = 80;
    int base[6] = {0,0,0,0,10,0};
    int alloc[6] = {0,0,0,0,0,0};
    int circuit = 0;
    switch (ji) {
        case 0: base[0]=0; base[1]=0; base[2]=0; base[3]=20; base[4]=10; break; /* 魔术师 */
        case 1: base[0]=20; base[1]=20; base[2]=20; base[3]=0; base[4]=10; break; /* 体术师 */
        case 2: base[0]=0; base[1]=0; base[2]=0; base[3]=10; base[4]=30; break; /* 异能者 */
        case 3: base[0]=0; base[1]=15; base[2]=0; base[3]=15; base[4]=10; break; /* 代行者 */
        case 4: base[0]=15; base[1]=20; base[2]=15; base[3]=0; base[4]=0; break; /* 执法者 */
    }
    printf("\n分配属性(共80点,5点一档,上限50;回路单独0~50)\n");
    printf("  用法: 1~5<+> 增加 1~5<-> 减少, 6<+>/6<- 调整回路, a 自动, 回车完成\n");
    for (;;) {
        printf("\r  剩余:%3d | ", pts);
        for (int i = 0; i < 5; i++)
            printf("%s:%d(%d) ", ks_attr_name(i), base[i] + alloc[i], alloc[i]);
        printf("回路:%d(%d)\n", circuit, circuit / 5 * 5);
        char buf[16];
        if (!fgets(buf, sizeof(buf), stdin)) break;
        buf[strcspn(buf, "\n")] = 0;
        if (buf[0] == 0 || buf[0] == 'd') break;
        if (buf[0] >= '1' && buf[0] <= '6') {
            int ai = buf[0] - '1';
            int op = buf[1] == '+' ? 1 : (buf[1] == '-' ? -1 : 0);
            if (op == 0) { printf("  用法: <号><+或->\n"); continue; }
            if (ai == 5) {
                if (op > 0) {
                    if (circuit >= 50) { printf("  回路已达上限50\n"); continue; }
                    if (pts < 5) { printf("  点数不足\n"); continue; }
                    circuit += 5; pts -= 5;
                } else {
                    if (circuit <= 0) continue;
                    circuit -= 5; pts += 5;
                }
            } else {
                if (op > 0) {
                    if (pts < 5) { printf("  点数不足\n"); continue; }
                    if (alloc[ai] >= 50) { printf("  该属性已达上限50\n"); continue; }
                    alloc[ai] += 5; pts -= 5;
                } else {
                    if (alloc[ai] <= 0) continue;
                    alloc[ai] -= 5; pts += 5;
                }
            }
        } else if (buf[0] == 'a' || buf[0] == 'A') {
            while (pts > 0) {
                for (int i = 0; i < 5 && pts > 0; i++)
                    if (alloc[i] < 50) { alloc[i] += 5; pts -= 5; }
            }
        }
    }

    /* 技能:从通用库中选购 */
    printf("\n── 御主技能(输入技能名购买,回车完成) ──\n");
    printf("可用技能(部分):\n");
    char allbuf[4096] = "";
    for (int i = 0; i < w->res_count; i++) {
        ks_res_t *r = &w->res[i];
        if (r->kind == KS_R_SKILL) {
            if (r->type == KS_T_MAGIC || r->type == KS_T_TECHNIQUE || r->type == KS_T_TALENT)
                snprintf(allbuf + strlen(allbuf), sizeof(allbuf) - strlen(allbuf),
                         "%s ", r->name);
        }
    }
    printf("%s\n", allbuf);
    int chosen[8] = {0,0,0,0,0,0,0,0};
    for (int k = 0; k < 8; k++) {
        char sname[KSG_NAME_MAX];
        ks_ui_ask_str("购买技能(回车完成): ", sname, sizeof(sname));
        if (!sname[0]) break;
        int rid = ks_world_find_res(w, sname);
        if (rid) { chosen[k] = rid; printf("  已购买: %s\n", sname); }
        else { printf("  未找到 %s\n", sname); k--; }
    }

    /* 礼装 */
    int item_id = 0;
    printf("\n── 礼装(可选) ──\n");
    char itembuf[4096] = "";
    list_all_items(w, itembuf, sizeof(itembuf));
    printf("%s", itembuf);
    char sname[KSG_NAME_MAX];
    ks_ui_ask_str("购买礼装(回车跳过): ", sname, sizeof(sname));
    if (sname[0]) item_id = ks_world_find_res(w, sname);

    int uid = ks_unit_new(w, name, tname, KS_U_MASTER, level, 0);
    ks_unit_t *u = &w->units[uid - 1];
    for (int a = 0; a < A_NP; a++) u->attr[a] = base[a] + alloc[a];
    u->attr[A_NP] = 0;
    u->attr[A_CIRCUIT] = circuit;
    u->mp.cap = circuit; u->mp.floor = -50; u->mp.cur = circuit;
    u->fp = 1; u->cs = 3;
    u->traits = TR_HUMAN;
    u->al_law = KS_MID; u->al_moral = KS_NEUT;
    for (int k = 0; k < 8; k++)
        if (chosen[k]) ks_unit_add_res(w, uid, chosen[k]);
    if (item_id) ks_unit_add_res(w, uid, item_id);

    printf("\n✅ 御主创建完成: %s (%s) Lv%d [%s]\n", u->name, u->true_name, level,
           jobs[ji]);
    ks_data_print_unit(w, uid);
    return uid;
}

/* ================= 对战(触发→结算) ================= */

/* 选择对战双方:两个单位,设置阵营1/2;mode=1 仅两单位, mode=2 各阵营全部单位 */
static int pick_duelants(ks_world_t *w, int *a, int *b, int *mode) {
    if (w->unit_count < 2) {
        printf("当前世界没有足够的单位。请先建卡或运行 demo。\n");
        return 0;
    }
    printf("\n── 选择对战双方 ──\n");
    for (int i = 0; i < w->unit_count; i++)
        printf("  %2d) %s (Lv%d %s)\n", i + 1, w->units[i].name,
               w->units[i].level, ks_utype_name(w->units[i].utype));
    int a2 = ks_ui_ask_int("选择左方单位编号", 1, w->unit_count, 1) - 1;
    int b2;
    do {
        b2 = ks_ui_ask_int("选择右方单位编号", 1, w->unit_count, 2) - 1;
        if (b2 == a2) printf("  不能选择同一单位\n");
    } while (b2 == a2);
    static const char *modes[] = { "仅这两个单位(1v1)", "连同各自阵营全部单位(阵营战)" };
    int m = ks_ui_menu("对战规模", modes, 2);
    *mode = (m == 1) ? 2 : 1;
    /* 阵营设置:把其余单位剥离出阵营1/2,避免误入战斗 */
    for (int i = 0; i < w->unit_count; i++) {
        if (i == a2 || i == b2) continue;
        if (*mode == 1 && (w->units[i].faction == 1 || w->units[i].faction == 2))
            w->units[i].faction = 3;   /* 旁观阵营 */
    }
    w->units[a2].faction = 1;
    w->units[b2].faction = 2;
    *a = w->units[a2].id;
    *b = w->units[b2].id;
    return 1;
}

/* 对战准备:给参战单位补满魔力、重置回转(公平起跑) */
static void duel_prepare(ks_world_t *w, int a, int b) {
    for (int i = 0; i < w->unit_count; i++) {
        ks_unit_t *u = &w->units[i];
        if (u->id != a && u->id != b) continue;
        if (u->utype == KS_U_SERVANT) { u->mp.cur = 100; u->mp.cap = 150; u->mp.floor = -100; }
        else if (u->utype == KS_U_MASTER) { u->mp.cur = u->attr[A_CIRCUIT]; u->mp.cap = u->attr[A_CIRCUIT]; u->mp.floor = -50; }
        u->fp = 1;
        for (int j = 0; j < u->phantasm_count; j++)
            w->res[u->phantasm_ids[j] - 1].cur_recast =
                w->res[u->phantasm_ids[j] - 1].recast;
        for (int j = 0; j < u->skill_count; j++) {
            ks_res_t *r = &w->res[u->skill_ids[j] - 1];
            r->cur_recast = (r->recast > 0) ? 0 : 0; /* 非常驻技能攒回转:给一半 */
            if (r->recast > 0) r->cur_recast = r->recast / 2;
        }
    }
}

/* 提交一个单位当前时机可发动的能力(可多选,0结束);返回提交数 */
static int duel_collect(ks_world_t *w, int side, int when, int *uids, int *rids, int max) {
    ks_battle_t *b = &w->battle;
    int mid = (side == 1) ? b->left_main : b->right_main;
    if (!mid) return 0;
    ks_unit_t *u = &w->units[mid - 1];
    const char *tname = (when == KS_TIME_BATTLE_START) ? "战斗开始时"
                       : (when == KS_TIME_PROC_OPEN) ? "初始工序"
                       : (when == KS_TIME_PROC_MAIN) ? "主要工序" : "最终工序";
    printf("\n【%s】%s (%s) ─ 提交能力 ─\n", side == 1 ? "左方" : "右方", u->name, tname);
    printf("  当前魔力 %d/%d | FP%d | 令咒%d\n", u->mp.cur, u->mp.cap, u->fp, u->cs);
    int castable[KSG_SKILL_MAX + KSG_PHANTASM_MAX];
    int n = ks_battle_unit_castable(w, mid, when, castable, KSG_SKILL_MAX + KSG_PHANTASM_MAX);
    if (n == 0) {
        printf("  (无可发动的能力)\n");
        return 0;
    }
    int got = 0;
    for (;;) {
        printf("  可发动:\n");
        for (int i = 0; i < n; i++) {
            ks_res_t *r = &w->res[castable[i] - 1];
            printf("    %2d) %s[%s] %s 魔耗%d 回转%d\n", i + 1, r->name,
                   ks_rank_name(r->rank), ks_reskind_name(r->kind), r->cost, r->recast);
        }
        int pick = ks_ui_ask_int("选择发动(可多选,0完成)", 0, n, 0);
        if (pick <= 0) break;
        int rid = castable[pick - 1];
        ks_res_t *r = &w->res[rid - 1];
        if (r->cost > u->mp.cur) {
            printf("  魔力不足(%d < %d),取消该项\n", u->mp.cur, r->cost);
            continue;
        }
        /* 同一能力不能重复提交 */
        int dup = 0;
        for (int k = 0; k < got; k++)
            if (rids[k] == rid) dup = 1;
        if (dup) {
            printf("  %s 已提交过,请选择其他\n", r->name);
            continue;
        }
        /* 是否[蓄力]或[爆发](具有特效时可选) */
        int has_charge = (r->feat & KS_F_BURST_READY) != 0;
        int has_burst = (r->feat & KS_F_ENERGY) != 0;
        int mode = 0;   /* 0=普通 1=蓄力 2=爆发 */
        if (has_charge && has_burst)
            mode = ks_ui_ask_int("选择: 0=直接 1=蓄力 2=爆发", 0, 2, 0);
        else if (has_charge)
            mode = ks_ui_ask_int("选择: 0=直接 1=蓄力", 0, 1, 0);
        else if (has_burst)
            mode = ks_ui_ask_int("选择: 0=直接 2=爆发", 0, 2, 0);
        uids[got] = mid;
        rids[got] = rid;
        got++;
        printf("  ✓ 已提交: %s%s\n", r->name,
               mode == 1 ? "(蓄力)" : mode == 2 ? "(爆发)" : "");
        /* 记录模式到附加数组(用高位传递) */
        if (mode) rids[got - 1] += (mode == 1) ? 10000 : 20000;
        if (got >= max) break;
    }
    return got;
}

/* 双方提交并统一按结算链结算 */
static void duel_phase_submit2(ks_world_t *w, int when) {
    int uids[64], rids[64];
    int n = 0;
    n += duel_collect(w, 1, when, uids + n, rids + n, 64 - n);
    n += duel_collect(w, 2, when, uids + n, rids + n, 64 - n);
    if (n > 0) {
        /* 解析蓄力/爆发标记(高10000/20000位) */
        int u2[64], r2[64], m2 = 0;
        for (int i = 0; i < n; i++) {
            int mode = 0;
            if (rids[i] >= 20000) { mode = 2; rids[i] -= 20000; }
            else if (rids[i] >= 10000) { mode = 1; rids[i] -= 10000; }
            if (mode == 1) {
                /* [蓄力]:进入蓄力队列,由工序tick自动解算 */
                ks_charge_begin(w, uids[i], rids[i], 1);
                continue;
            }
            ks_res_t *r = &w->res[rids[i] - 1];
            if (mode == 2 && r->cost > 0 && w->units[uids[i] - 1].mp.cur < r->cost) {
                printf("  ✖ %s [爆发]失败:魔力不足\n", w->units[uids[i] - 1].name);
                continue;   /* 爆发失败则本次完全放弃 */
            }
            u2[m2] = uids[i];
            r2[m2] = rids[i];
            m2++;
        }
        /* 提交按结算链处理 */
        ks_battle_chain_resolve(w, u2, r2, m2);
        /* 爆发项在链结算后额外再结算一次效果 */
        for (int i = 0; i < n; i++) {
            int rid = rids[i];
            int mode = 0;
            if (rid >= 20000) { mode = 2; rid -= 20000; }
            else if (rid >= 10000) { mode = 1; rid -= 10000; }
            if (mode == 2) {
                ks_unit_t *u = &w->units[uids[i] - 1];
                ks_res_t *r = &w->res[rid - 1];
                ks_log(w, "  ✦✦ %s [爆发]%s 效果再次生效(额外魔耗%d)", u->name, r->name, r->cost);
                ks_mp_pay(w, uids[i], r->cost);
                ks_apply_res_effects(w, uids[i], rid, 0);
            }
        }
    }
    else printf("  (双方均未提交能力)\n");
    ks_battle_print(w);
}

/* 战斗工序统一流程:询问阵营 side 提交能力并结算(旧版保留) */
static int duel_phase_submit(ks_world_t *w, int side, int when) {
    ks_battle_t *b = &w->battle;
    int mid = (side == 1) ? b->left_main : b->right_main;
    if (!mid) return 0;
    ks_unit_t *u = &w->units[mid - 1];
    const char *tname = (when == KS_TIME_BATTLE_START) ? "战斗开始时"
                       : (when == KS_TIME_PROC_OPEN) ? "初始工序"
                       : (when == KS_TIME_PROC_MAIN) ? "主要工序" : "最终工序";
    printf("\n【%s】%s (%s) ─ 时机:%s ─\n", side == 1 ? "左方" : "右方",
           u->name, tname, tname);
    printf("  当前魔力 %d/%d | FP%d\n", u->mp.cur, u->mp.cap, u->fp);
    int rids[KSG_SKILL_MAX + KSG_PHANTASM_MAX];
    int n = ks_battle_unit_castable(w, mid, when, rids, KSG_SKILL_MAX + KSG_PHANTASM_MAX);
    if (n == 0) {
        printf("  (无可发动的能力)\n");
    } else {
        printf("  可发动:\n");
        for (int i = 0; i < n; i++) {
            ks_res_t *r = &w->res[rids[i] - 1];
            printf("    %2d) %s[%s] %s 魔耗%d 回转%d%c\n", i + 1, r->name,
                   ks_rank_name(r->rank), ks_reskind_name(r->kind), r->cost, r->recast,
                   r->recast > 0 && r->cur_recast < r->recast ? '!' : ' ');
        }
        int pick = ks_ui_ask_int("发动哪个(0=不发动)", 0, n, 0);
        if (pick <= 0) return 0;
        int rid = rids[pick - 1];
        ks_res_t *r = &w->res[rid - 1];
        /* 魔耗检查 */
        if (r->cost > u->mp.cur) {
            printf("  魔力不足(%d < %d),发动失败\n", u->mp.cur, r->cost);
            return 0;
        }
        if (r->recast > 0 && r->cur_recast < r->recast) {
            printf("  回转未完成(%d/%d),无法发动\n", r->cur_recast, r->recast);
            return 0;
        }
        ks_battle_cast_skill(w, mid, rid);
    }
    return 1;
}

/* 令咒使用:返回 1 使用成功 */
static int duel_command_spell(ks_world_t *w, int side) {
    ks_battle_t *b = &w->battle;
    int mid = (side == 1) ? b->left_main : b->right_main;
    if (!mid) return 0;
    ks_unit_t *u = &w->units[mid - 1];
    if (u->cs <= 0) {
        printf("  %s 没有令咒了!\n", u->name);
        return 0;
    }
    static const char *uses[] = {
        "属性补正(+30任一属性,初始工序)",
        "战况修正(重骰随机属性,主要工序)",
        "胜率补正(+30%胜率,最终工序,非主力减半)",
        "用于撤退(转化为2临时FP)",
        "从者召来(立即加入当前灵脉战斗)",
        "抵消即死/异常状态(下个效果)",
        "抗性强化(+30%抗性上升)",
        "强制命令(简易:令自身+10%最终胜率)",
    };
    int sel = ks_ui_menu("令咒用法", uses, 8);
    if (sel < 0) return 0;
    u->cs--;
    switch (sel) {
        case 0: { /* 属性补正 */
            static const char *attrs[] = { "筋力", "耐久", "敏捷", "魔力", "幸运", "宝具" };
            int a = ks_ui_menu("选择补正属性", attrs, 6);
            if (a < 0) { u->cs++; return 0; }
            u->attr_mod[A_STR + a] += 30;
            printf("  ✦ 令咒[属性补正]!%s %%%s+30\n", u->name, ks_attr_name(A_STR + a));
            break;
        }
        case 1: /* 战况修正 */
            ks_battle_roll_rand_attr(w);
            printf("  ✦ 令咒[战况修正]!随机属性已重骰\n");
            break;
        case 2: { /* 胜率补正 */
            int v = (u->battle_slot == KS_SLOT_MAIN) ? 30 : 15;
            ks_battle_add_win(w, side, v);
            printf("  ✦ 令咒[胜率补正]!%s +%d%%胜率\n", u->name, v);
            break;
        }
        case 3: /* 用于撤退 */
            u->tp_fp += 2;
            printf("  ✦ 令咒[用于撤退]!%s 获得2临时FP(回合结束失去)\n", u->name);
            break;
        case 4: /* 从者召来:让己方主力补一次行动(简化) */
            u->acted = 0;
            printf("  ✦ 令咒[从者召来]!%s 立即抵达战场,本回合可再行动(战斗内获得[-20%%胜率惩罚])\n", u->name);
            ks_battle_add_win(w, side, -20);
            break;
        case 5: /* 抵消即死/异常:免除自身持有的弱化异常状态 */
            for (int i = 0; i < KSG_STATUS_MAX; i++) {
                int k = u->status[i].kind;
                if (ks_status_class(k) >= 2 && (k != S_SEAL)) {
                    u->status[i].kind = S_NONE; u->status[i].layers = 0;
                }
            }
            printf("  ✦ 令咒[抵消]!%s 的弱化/异常状态被清除\n", u->name);
            break;
        case 6: /* 抗性强化 */
            ks_unit_gain_status(w, mid, S_RESUP, 30, 0);
            printf("  ✦ 令咒[抗性强化]!%s 获得[抗性上升:+30%%]\n", u->name);
            break;
        case 7: /* 强制命令 */
            if (side == 1) b->left_final_win += 10; else b->right_final_win += 10;
            printf("  ✦ 令咒[强制命令]!%s 执行绝对命令,最终胜率+10%%\n", u->name);
            break;
    }
    ks_battle_print(w);
    return 1;
}

/* 双方选择战术 */
static void duel_tactics(ks_world_t *w) {
    static const char *tacs[] = { "强击(+属性)", "破袭(底限穿透)", "试探(撤退节省)", "扼守(胜率)" };
    printf("\n── 战术选择(克制环: 强击>破袭>试探>扼守>强击) ──\n");
    int t1 = ks_ui_menu("左方战术", tacs, 4);
    int t2 = ks_ui_menu("右方战术", tacs, 4);
    ks_battle_tactics(w, 1, T_STRIKE + (t1 < 0 ? 0 : t1));
    ks_battle_tactics(w, 2, T_STRIKE + (t2 < 0 ? 0 : t2));
}

/* 双方选择主要属性 */
static void duel_main_attr(ks_world_t *w) {
    static const char *attrs[] = { "筋力", "耐久", "敏捷", "魔力", "幸运", "宝具" };
    printf("\n── 选择主要属性(战斗属性=主要属性+随机属性) ──\n");
    int a1 = ks_ui_menu("左方主要属性", attrs, 6);
    int a2 = ks_ui_menu("右方主要属性", attrs, 6);
    ks_battle_attr_pick(w, 1, A_STR + (a1 < 0 ? 0 : a1));
    ks_battle_attr_pick(w, 2, A_STR + (a2 < 0 ? 0 : a2));
    ks_battle_roll_rand_attr(w);
    ks_battle_battery_check(w);
}

int ks_ui_duel(ks_world_t *w) {
    int a, b, mode;
    if (!pick_duelants(w, &a, &b, &mode)) return 0;
    printf("\n━━━━━━━━━━━━ 对战开始 %s vs %s (%s) ━━━━━━━━━━━━\n",
           w->units[a - 1].name, w->units[b - 1].name,
           mode == 1 ? "1v1" : "阵营战");
    /* 1v1 时只保留两个单位在场,其余标记旁观 */
    if (mode == 1) {
        for (int i = 0; i < w->unit_count; i++) {
            ks_unit_t *u = &w->units[i];
            if (u->id != a && u->id != b) {
                u->faction = 3;
                u->in_battle = 0;
            }
        }
    }
    duel_prepare(w, a, b);
    ks_battle_cfg_t cfg = { .width = 4, .day = w->day, .leyline = 1 };
    ks_battle_start(w, &cfg);
    ks_battle_print(w);

    /* 战斗开始时:战术 + 可发动能力(链式结算) */
    duel_tactics(w);
    duel_phase_submit2(w, KS_TIME_BATTLE_START);

    /* 初始工序 */
    printf("\n━━ 初始工序 ━━\n");
    duel_main_attr(w);
    duel_phase_submit2(w, KS_TIME_PROC_OPEN);

    /* 主要工序 */
    printf("\n━━ 主要工序 ━━\n");
    duel_phase_submit2(w, KS_TIME_PROC_MAIN);
    /* 令咒(每方每工序可询问一次) */
    if (ks_ui_ask_int("左方是否使用[令咒]?(1=是)", 0, 1, 0))
        duel_command_spell(w, 1);
    if (ks_ui_ask_int("右方是否使用[令咒]?(1=是)", 0, 1, 0))
        duel_command_spell(w, 2);
    /* 冲锋/追击/掩护/撤退 指令 */
    if (ks_ui_ask_int("左方是否[冲锋]?(1=是)", 0, 1, 0))
        ks_battle_order(w, 1, O_CHARGE);
    if (ks_ui_ask_int("右方是否[冲锋]?(1=是)", 0, 1, 0))
        ks_battle_order(w, 2, O_CHARGE);
    if (ks_ui_ask_int("左方是否[追击]?(1=是)", 0, 1, 0))
        ks_battle_order(w, 1, O_PURSUE);
    if (ks_ui_ask_int("右方是否[追击]?(1=是)", 0, 1, 0))
        ks_battle_order(w, 2, O_PURSUE);
    if (ks_ui_ask_int("左方是否[掩护]?(1=是)", 0, 1, 0))
        ks_battle_order(w, 1, O_COVER);
    if (ks_ui_ask_int("右方是否[掩护]?(1=是)", 0, 1, 0))
        ks_battle_order(w, 2, O_COVER);
    /* 撤退(消耗FP,成功后进入游荡) */
    if (ks_ui_ask_int("左方是否[撤退]?(1=是,需FP)", 0, 1, 0)) {
        ks_battle_retreat(w, 1);
        if (!w->battle.active) return 1;
    }
    if (ks_ui_ask_int("右方是否[撤退]?(1=是,需FP)", 0, 1, 0)) {
        ks_battle_retreat(w, 2);
        if (!w->battle.active) return 1;
    }
    for (int i = 0; i < w->unit_count; i++)
        if (w->units[i].in_battle) { ks_status_proc_tick(w, w->units[i].id); ks_recast_tick_proc(w, w->units[i].id); }
    ks_battle_print(w);

    /* 最终工序 */
    printf("\n━━ 最终工序 ━━\n");
    duel_phase_submit2(w, KS_TIME_PROC_FINAL);
    if (ks_ui_ask_int("左方是否[死斗]?(1=是)", 0, 1, 0))
        ks_battle_order(w, 1, O_DUEL);
    if (ks_ui_ask_int("右方是否[死斗]?(1=是)", 0, 1, 0))
        ks_battle_order(w, 2, O_DUEL);
    ks_battle_print(w);

    /* 决胜 */
    printf("\n━━ 决胜检定 ━━\n");
    ks_battle_effective(w);
    ks_battle_end(w);
    return (w->battle.left_ok || 1) ? 1 : 2;
}

/* ================= 主菜单 ================= */

/* ================= 词典查询(附录三) ================= */

static void ui_show_status_dict(void) {
    printf("\n════════ 词典 : 状态一览(规则书附录三) ════════\n");
    printf("── 强化状态 ──\n");
    printf("  [回避]    令自身受到的下一个(非自身来源)技能/宝具效果无效化\n");
    printf("  [无敌]    不受来源自身外的任意效果影响,战斗/回合结束时移除\n");
    printf("  [必中]    令目标的[回避]无效化\n");
    printf("  [无敌贯通] 无视[无敌]效果\n");
    printf("  [保护]    将对他人的效果转移到自身,被保护者无法再保护其他单位\n");
    printf("  [抗性上升] 以自身为目标的负面判定最终成功率惩罚\n");
    printf("  [状态抵抗] 对应状态判定最终成功率减半,至少-20%%\n");
    printf("  [状态免疫] 对应状态判定默认失败,赋予时直接免除并清除已有\n");
    printf("── 弱化状态 ──\n");
    printf("  [疲惫]    除宝具外全属性-5*层;5层以上时有即死判定风险\n");
    printf("  [残废]    每回合开始50%%即死判定,失败需令咒否则退场\n");
    printf("  [迟滞]    非职阶技能发动结算延迟到下一工序;每工序-1层\n");
    printf("  [诅咒]    受到的负面判定成功率+5*层%%;施用时-1层;至多20层\n");
    printf("  [封印]    指定技能/宝具无效化;战斗结束或回合结束-1层\n");
    printf("  [技能封印] 无法发动技能;每次战斗/回合结束-1层\n");
    printf("  [宝具封印] 无法解放宝具;每次战斗/回合结束-1层\n");
    printf("  [抗性下降] 自身发起的负面判定获得最终成功率补正\n");
    printf("── 异常状态 ──\n");
    printf("  [中毒]    每回合开始20*层%%负面判定,成功则筋/耐/敏随机-10;层数-1\n");
    printf("  [灼伤]    每回合开始层数*20%%负面判定,成功则四属性随机-10;层数-1\n");
    printf("  [冻结]    筋/耐/敏-5*层;每回合开始层数-1;休整时-2\n");
    printf("  [感电]    轮次结束时-5*层魔力后清除;至多20层\n");
    printf("  [石化]    筋/耐/敏属性补正-60;无法改变随机属性\n");
    printf("  [晕眩]    无法发动非职阶技能;每工序开始层数*20%%判定,失败解除\n");
    printf("  [魅惑]    同战斗位+5%%*层胜率,对立-5%%*层;3层属性+10/对立随机属性劣势;6层无法袭击;9层无法进入对立位\n");
    printf("  [混乱]    单体目标随机选取;层数每次生效-1;回合结束清除\n");
    printf("  [恐惧]    发动非职阶技能时50%%判定失败;被重复赋予转为[混乱1]\n");
    printf("  [抗性破除] 使对应状态抵抗/免疫/抗性上升对本次判定无效化\n");
    printf("  [特性赋予] 获得指定特性\n");
    printf("\n── 特效词条 ──\n");
    printf("  [主力位/辅助位/仆役位/支援位] 仅在该战斗位才能发动/生效\n");
    printf("  [支援]   可在主力位/辅助位/支援位发动(不同于[支援位])\n");
    printf("  [反击]   在指定效果生效前优先结算\n");
    printf("  [蓄力]   宣言后延迟到指定工序/经过一定工序后生效\n");
    printf("  [爆发]   额外支付一份发动条件令效果再次生效一次\n");
    printf("  [骑乘]   允许[机动]时同灵脉单位[协助];允许[冲锋]\n");
    printf("  [必中]/[无敌贯通] 穿透[回避]/[无敌]\n");
}

/* 展示单位完整状态与效果 */
static void ui_show_unit_detail(ks_world_t *w, int uid) {
    ks_data_print_unit(w, uid);
    ks_status_print_unit(w, uid);
    ks_unit_t *u = &w->units[uid - 1];
    printf("  临时补正: ");
    for (int a = 0; a < A_NP; a++)
        if (u->attr_mod[a]) printf("%s%+d ", ks_attr_name(a), u->attr_mod[a]);
    printf("| 常驻补正: ");
    for (int a = 0; a < A_CIRCUIT; a++)
        if (u->attr_perm[a]) printf("%s%+d ", ks_attr_name(a), u->attr_perm[a]);
    printf("\n");
}

void ks_ui_main(ks_world_t *w) {
    for (;;) {
        if (feof(stdin)) { printf("\n(输入结束,退出)\n"); return; }
        printf("\n");
        printf("╔══════════════════════════════════════════════╗\n");
        printf("║        空想圣杯 KsG · 交互控制台              ║\n");
        printf("║ 建卡 → 选技能/宝具 → 对战触发 → 结算胜率      ║\n");
        printf("╚══════════════════════════════════════════════╝\n");
        static const char *items[] = {
            "从者建卡(职阶/属性/技能/宝具)",
            "御主建卡(职业/属性/技能/礼装)",
            "单位一览(含状态/补正)",
            "词典查询(状态·特效原文)",
            "开始对战(触发→结算)",
            "自动沙盘演示",
            "进入指令 Shell(高级)",
        };
        int sel = ks_ui_menu("主菜单", items, 7);
        switch (sel) {
            case 0: ks_ui_create_servant(w); break;
            case 1: ks_ui_create_master(w); break;
            case 2:
                for (int i = 0; i < w->unit_count; i++)
                    ui_show_unit_detail(w, w->units[i].id);
                if (!w->unit_count) printf("(尚无单位)\n");
                break;
            case 3: ui_show_status_dict(); break;
            case 4: ks_ui_duel(w); break;
            case 5: demo_simple(w); break;
            default: return;
        }
    }
}