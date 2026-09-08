/* ksg_data.c — 空想圣杯引擎:资源数据库(依据五本书复刻的结构化条目) */
#include "ksg.h"

/* 便捷构造:技能 */
static int def_skill(ks_world_t *w, const char *name, int type, int rank,
                     int when, int cost, int recast, int feat)
{
    ks_res_t r;
    memset(&r, 0, sizeof(r));
    snprintf(r.name, KSG_NAME_MAX, "%s", name);
    r.kind = KS_R_SKILL;
    r.type = type;
    r.rank = rank;
    r.when = when;
    r.cost = cost;
    r.recast = recast;
    r.feat = feat;
    return ks_world_register_res(w, &r);
}

/* 便捷构造:宝具 */
static int def_np(ks_world_t *w, const char *name, int type, int focus, int rank,
                  int when, int cost, int recast, int feat)
{
    ks_res_t r;
    memset(&r, 0, sizeof(r));
    snprintf(r.name, KSG_NAME_MAX, "%s", name);
    r.kind = KS_R_PHANTASM;
    r.type = type;
    r.focus = focus;
    r.rank = rank;
    r.when = when;
    r.cost = cost;
    r.recast = recast;
    r.feat = feat;
    return ks_world_register_res(w, &r);
}

/* 便捷构造:礼装/科技造物 */
static int def_item(ks_world_t *w, const char *name, int rank, int when,
                    int cost, int recast, int feat, int count, int reserve,
                    int unique)
{
    ks_res_t r;
    memset(&r, 0, sizeof(r));
    snprintf(r.name, KSG_NAME_MAX, "%s", name);
    r.kind = KS_R_ITEM;
    r.type = KS_T_MAGIC;
    r.rank = rank;
    r.when = when;
    r.cost = cost;
    r.recast = recast;
    r.feat = feat;
    r.count_per_round = count;
    r.reserve = r.reserve_max = reserve;
    r.unique = unique;
    return ks_world_register_res(w, &r);
}

/* 附加一条效果 */
static void add_eff(ks_world_t *w, int rid, int flag, int attr, int value,
                    int status, int layers, int target)
{
    ks_res_t *r = &w->res[rid - 1];
    if (r->effect_count >= KSG_EOF_MAX) return;
    ks_effectline_t *el = &r->effects[r->effect_count++];
    el->chance_attr_base = -1;
    el->flag = flag;
    el->attr = attr;
    el->value = value;
    el->status = status;
    el->layers = layers;
    el->target = target;
}

static void set_text(ks_world_t *w, int rid, const char *txt) {
    ks_res_t *r = &w->res[rid - 1];
    snprintf(r->text, KSG_TEXT_MAX, "%s", txt ? txt : "");
}

/* ---------------- 从者职阶基础属性 ---------------- */

typedef struct { const char *name; int base[6]; } ks_class_sheet;

static const ks_class_sheet g_class_sheets[] = {
    { "Saber",     { 20, 20, 20, 20, 20, 0 } },
    { "Lancer",    { 10, 10, 30, 20, 10, 0 } },
    { "Archer",    { 20, 10, 20, 0,  20, 0 } },
    { "Rider",     { 10, 20, 20, 0,  20, 0 } },
    { "Caster",    { 0,  0,  0,  30, 20, 0 } },
    { "Assassin",  { 0,  0,  20, 0,  20, 0 } },
    { "Berserker", { 20, 20, 20, 0,  0,  0 } },
    { "Ruler",     { 10, 20, 10, 20, 30, 0 } },
    { "Avenger",   { 10, 20, 20, 20, 0,  0 } },
};

void ks_data_make_unit_sheet(ks_world_t *w, int uid, const char *sheet)
{
    ks_unit_t *u = &w->units[uid - 1];
    const ks_class_sheet *cs = NULL;
    for (size_t i = 0; i < sizeof(g_class_sheets) / sizeof(g_class_sheets[0]); i++) {
        if (!strcasecmp(g_class_sheets[i].name, sheet)) { cs = &g_class_sheets[i]; break; }
    }
    if (cs) {
        u->servant_class = (int)(cs - g_class_sheets) + 1;
        for (int a = 0; a < A_NP; a++) u->attr[a] = cs->base[a];
        u->utype = KS_U_SERVANT;
        /* 等级对应基础分配(规则:40→120,50→140,60→160,70→180) */
        int pts = 120 + (u->level - 40) * 2;
        /* 简化为把点数摊到筋力/耐久/敏捷/魔力/幸运上 */
        int extra = pts / 5;
        for (int a = 0; a < A_NP; a++) u->attr[a] += extra;
    } else if (!strcasecmp(sheet, "master")) {
        u->utype = KS_U_MASTER;
        u->attr[A_STR] = 10; u->attr[A_END] = 10; u->attr[A_AGI] = 10;
        u->attr[A_MAG] = 20; u->attr[A_LUK] = 10; u->attr[A_NP] = 0;
        u->attr[A_CIRCUIT] = 30;
        u->mp.cap = u->attr[A_CIRCUIT];
        u->mp.floor = -50;
        u->mp.cur = u->mp.cap;
    }
}

/* ---------------- 建库 ---------------- */

void ks_data_build(ks_world_t *w)
{
    int r;

    /* ============ 《空想从者资源库》 ============ */

    /* —— 基础资源库 · 职阶技能 —— */
        add_eff(w, r, EF_OTHER, 0, -10, 0, 0, 0);

    
    
    
    
    
    r = def_skill(w, "狂化", KS_T_CLASS, KS_RANK_B, KS_WHEN_PASSIVE, 15, 0, 0);
    set_text(w, r, "给予[筋力][耐久][敏捷][+10*次数]常驻补正;B级起[状态免疫:恐惧&魅惑]。");

    /* —— 基础资源库 · 保有技能 —— */
        add_eff(w, r, EF_WIN_UP, 0, 20, 0, 0, -1);

    
    
    r = def_skill(w, "勇猛", KS_T_TALENT, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    set_text(w, r, "始终给予[+15%]胜率补正,战术[强击/破袭]且未被克制时翻倍。");
    add_eff(w, r, EF_WIN_UP, 0, 15, 0, 0, -2);

        add_eff(w, r, EF_ATTR_UP, A_STR, 10, 0, 0, -2);

    
    
    
    
    
        add_eff(w, r, EF_WIN_DOWN, 0, 30, 0, 0, 0);

    r = def_skill(w, "二重召唤", KS_T_MAGIC, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    set_text(w, r, "建卡时获得所选职阶的职阶技能(C级模板),判定上同时视为所选职阶。");

    /* —— 基础资源库 · 宝具 —— */
        add_eff(w, r, EF_WIN_UP, 0, 80, 0, 0, -2);

    r = def_np(w, "天地乖离开辟之星", KS_NP_WORLD, FC_DECISIVE, KS_RANK_A, KS_WHEN_PROC, 100, 9, KS_F_MAIN);
    set_text(w, r, "[蓄力]最终工序[+90%]胜率并无效结界宝具;轰击:摧毁阵地/工房/神殿/结界,敌方全体-60%胜率。");
    add_eff(w, r, EF_WIN_UP, 0, 90, 0, 0, -2);
    add_eff(w, r, EF_WIN_DOWN, 0, 60, 0, 0, 0);

    
    
    r = def_np(w, "妄想心音", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_B, KS_WHEN_PROC, 50, 6, KS_F_MAIN);
    set_text(w, r, "对敌方非支援位单位进行[80%]的[即死]判定;目标幸运≥40则成功率减半。");
    add_eff(w, r, EF_DEATH, 0, 80, 0, 0, 0);

        add_eff(w, r, EF_DEATH, 0, 50, 0, 0, 0);

    
    
    
    /* —— 外典扩充包 —— */
    
    r = def_np(w, "对吾华丽父王的叛逆", KS_NP_ARMORY, FC_DECISIVE, KS_RANK_B, KS_WHEN_PROC, 70, 9, KS_F_MAIN);
    set_text(w, r, "最终工序:敌方非支援位全体[-35%]胜率惩罚;可追加令咒轰击。");
    add_eff(w, r, EF_WIN_DOWN, 0, 35, 0, 0, 0);

    r = def_np(w, "梵天啊，诅咒我身", KS_NP_CASTLE, FC_DECISIVE, KS_RANK_B, KS_WHEN_PROC, 80, 9, KS_F_MAIN);
    set_text(w, r, "[蓄力]最终工序给予敌方全体[灼伤4];轰击时额外灼伤并[爆燃]。");
    add_eff(w, r, EF_STATUS_GIVE, 0, 0, S_BURN, 4, 0);

    /* —— 京都扩充包 —— */
    
    
    
    
    /* —— 苍银扩充包 —— */
    
    
    /* —— 北欧扩充包 —— */
    
    r = def_np(w, "坏劫之天轮", KS_NP_CASTLE, FC_OFFENSE, KS_RANK_B, KS_WHEN_PROC, 80, 12, 0);
    set_text(w, r, "敌方全体[灼伤4];对敌方主力位进行[总灼伤层数*10%]负面判定,成功则全体[爆燃]。");
    add_eff(w, r, EF_STATUS_GIVE, 0, 0, S_BURN, 4, 0);

    /* —— 深池扩充包 —— */
    
    
    /* —— 军阵扩充包 —— */
    r = def_np(w, "泪之星·军神之剑", KS_NP_ARMORY, FC_DECISIVE, KS_RANK_B, KS_WHEN_PROC, 60, 6, KS_F_MAIN);
    set_text(w, r, "最终工序敌方非支援位[-35%]胜率惩罚;对军宝具解放时额外解放一次。");
    add_eff(w, r, EF_WIN_DOWN, 0, 35, 0, 0, 0);

    /* —— 90赤幕扩充包 —— */
    r = def_skill(w, "凯旋的魅力", KS_T_TALENT, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    set_text(w, r, "己方[战术]始终视为克制敌方[战术];给予己方战斗位全体[+20%]胜率补正。");
    add_eff(w, r, EF_WIN_UP, 0, 20, 0, 0, -1);

    r = def_np(w, "永远遥远的胜利之剑", KS_NP_ARMORY, FC_DECISIVE, KS_RANK_B, KS_WHEN_ANY, 50, 9, KS_F_MAIN);
    set_text(w, r, "随时宣言解放给予敌方主力位[-50%]胜率惩罚;可追加令咒轰击(全体-60%)。");
    add_eff(w, r, EF_WIN_DOWN, 0, 50, 0, 0, 0);

    /* —— 物语扩充包 —— */
    
    /* —— 少女扩充包 —— */
    
    /* —— 盈月扩充包 —— */
    
    /* ============ 《空想御主资源库》 ============ */

    /* —— 基础 · 主职业魔术师 —— */
    
    
    /* —— 子职业：人偶师 —— */
    
    /* —— 子职业：结界师 —— */
    
    /* —— 子职业：咒术师 —— */
    
    /* —— 主职业体术师 —— */
    
    /* —— 子职业：武术家 —— */
    
    /* —— 子职业：剑术师 —— */
    
    /* —— 通用技能·魔眼 —— */
    r = def_skill(w, "炎烧之魔眼", KS_T_MAGIC, KS_RANK_B, KS_WHEN_PROC, 20, 9, KS_F_ASSIST | KS_F_ENERGY);
    set_text(w, r, "给予敌方非支援位单位[灼伤2];爆发时结算灼伤或[爆燃]。");
    add_eff(w, r, EF_STATUS_GIVE, 0, 0, S_BURN, 2, 0);

    /* —— 通用技能·后勤 —— */
    
    /* —— 通用技能·战斗 —— */
    
    /* —— 月姬扩充包·异能者 —— */
    
    /* —— 月姬·死徒子职业 —— */
    
    /* —— 代行者主职业 —— */
    
    /* —— 赝作·执法者主职业 —— */
    
    /* —— 军部子职业 —— */
    
    /* —— 权贵子职业 —— */
    
    /* ============ 《空想礼装资源书》 ============ */

    
    
    r = def_item(w, "魔弹宝石", KS_RANK_C, KS_WHEN_PROC, 0, 0, KS_F_ASSIST, 1, 0, 0);
    set_text(w, r, "[支援]最终工序:给予敌方一位单位[-10%]胜率惩罚(非主力减半)。");
    add_eff(w, r, EF_WIN_DOWN, 0, 10, 0, 0, 1);

    r = def_item(w, "魔法箭矢", KS_RANK_C, KS_WHEN_PROC, 0, 0, KS_F_ASSIST, 1, 0, 0);
    set_text(w, r, "[支援]初始工序:给予敌方一单位除宝具外一项属性[-10]属性惩罚。");
    add_eff(w, r, EF_ATTR_DOWN, A_STR, 10, 0, 0, 1);

        add_eff(w, r, EF_MANA_UP, 0, 10, 0, 0, 0);

    
    
    
    
    
    
    
    
    
    
    r = def_item(w, "连接强化型魔术礼装", KS_RANK_C, KS_WHEN_PASSIVE, 10, 0, KS_F_UNIQUE, 1, 0, 1);
    set_text(w, r, "[唯一][筋力][耐久][敏捷]+10常驻补正;战斗开始时发动:补正翻倍。");

        add_eff(w, r, EF_WIN_DOWN, 0, 5, 0, 0, 1);

    
    /* ============ 科技造物 ============ */


    /* ============ 《空想工房建造书》 ============ */

    static const struct {
        const char *name;
        int kind, build, hp, scale;
        const char *effect;
    } cmpts[] = {
        { "信息基盘", CMPT_BASE, 1, 20, 10, "自阵营[情报调查]+30%基础成功率,[资料分析]+20%" },
        { "资源基盘", CMPT_BASE, 1, 20, 10, "工房持有者获得额外魔力池(上限+50)" },
        { "炼金基盘", CMPT_BASE, 1, 20, 10, "自阵营[礼装制作]+20%基础成功率" },
        { "强能法阵", CMPT_AUX, 2, 30, 2, "轮次结束时工房持有者+30魔力补给" },
        { "医疗装机", CMPT_AUX, 2, 30, 2, "每回合开始时任选单位的异常状态-3层" },
        { "聚合圆盘", CMPT_AUX, 2, 30, 2, "自身在此灵脉的礼装使用上限+6" },
        { "黑厄深阱", CMPT_LIMITED, 3, 40, 3, "己方主力位抗性+10%;宣言翻倍则本灵脉供魔减半" },
        { "魔能重炮", CMPT_LIMITED, 3, 40, 3, "己方战斗给予敌方主力位-40%胜率惩罚" },
        { "制裁机关", CMPT_LIMITED, 3, 40, 3, "敌方技能宝具效果下降1级" },
        { "增幅模块", CMPT_LIMITED, 3, 40, 3, "工房主[类型:魔术]技能效果等级+1" },
        { "传输阵式", CMPT_SUPPORT, 4, 50, 4, "随时将工房/灵脉魔力转移给任意单位" },
        { "集束光标", CMPT_SUPPORT, 4, 50, 4, "自阵营从者战斗+40%胜率补正(非主力减半)" },
        { "术控主脑", CMPT_SUPPORT, 4, 50, 4, "知悉任意战斗中双方战斗情况;一律令咒;每回合一次干涉-交流" },
    };
    for (size_t i = 0; i < sizeof(cmpts) / sizeof(cmpts[0]); i++) {
        if (w->cmpt_count >= (int)(sizeof(w->cmpt_count) * 8)) break;
        ks_component_t *c = &w->cmpts[w->cmpt_count++];
        snprintf(c->name, KSG_NAME_MAX, "%s", cmpts[i].name);
        c->kind = cmpts[i].kind;
        c->build = cmpts[i].build;
        c->hp = cmpts[i].hp;
        c->scale = cmpts[i].scale;
        c->effect = cmpts[i].effect;
        c->owner = 0;
    }

    /* 神殿组件 */
    ks_component_t *c = &w->cmpts[w->cmpt_count++];
    snprintf(c->name, KSG_NAME_MAX, "圣所基盘");
    c->kind = CMPT_BASE; c->build = 1; c->hp = 999; c->scale = 999;
    c->effect = "令当前灵脉基础魔力量翻倍(全场唯一)";
    c = &w->cmpts[w->cmpt_count++];
    snprintf(c->name, KSG_NAME_MAX, "大观星台");
    c->kind = CMPT_AUX; c->build = 3; c->hp = 60; c->scale = 3;
    c->effect = "同阵营[广泛侦查]默认成功;使魔无法干涉神殿所处灵脉";
}

/* ---------------- 打印 ---------------- */

void ks_data_print_res(ks_world_t *w, int rid)
{
    ks_res_t *r = &w->res[rid - 1];
    if (!r->id) return;
    printf("%s[%s] <%s> 时机:%s 魔耗:%d 回转:%d%s\n", r->name, ks_rank_name(r->rank),
           ks_reskind_name(r->kind), ks_when_name(r->when), r->cost, r->recast,
           r->feat ? " " : "");
    for (int f = 0; f < 12; f++)
        if (r->feat & (1 << f)) printf("    %s", ks_feat_name(1 << f));
    if (r->feat) printf("\n");
    if (r->reserve_max) printf("    [储备%d/%d]\n", r->reserve, r->reserve_max);
    if (r->text[0]) printf("    效果: %s\n", r->text);
    for (int i = 0; i < r->effect_count; i++) {
        ks_effectline_t *el = &r->effects[i];
        if (el->flag == EF_NONE) continue;
        printf("    [数据] %s", ks_eff_name(el->flag));
        if (el->attr > 0) printf(" %s", ks_attr_name(el->attr));
        if (el->value) printf(" %d", el->value);
        if (el->status) printf(" %s%d", ks_status_name(el->status), el->layers);
        printf("\n");
    }
}

void ks_data_print_unit(ks_world_t *w, int uid)
{
    ks_unit_t *u = &w->units[uid - 1];
    if (!u->id) return;
    printf("%s (%s) Lv%d 阵营%d 特性:", u->name, u->true_name, u->level, u->faction);
    for (int t = 0; t < 7; t++)
        if (u->traits & (1 << t)) printf(" %s", ks_trait_name(1 << t));
    printf("\n");
    printf("  %s:%d %s:%d %s:%d %s:%d %s:%d %s:%d\n",
           ks_attr_name(A_STR), u->attr[A_STR], ks_attr_name(A_END), u->attr[A_END],
           ks_attr_name(A_AGI), u->attr[A_AGI], ks_attr_name(A_MAG), u->attr[A_MAG],
           ks_attr_name(A_LUK), u->attr[A_LUK], ks_attr_name(A_NP), u->attr[A_NP]);
    printf("  魔力池 %d/%d(下限%d) FP%d 令咒%d\n", u->mp.cur, u->mp.cap, u->mp.floor, u->fp, u->cs);
    printf("  技能:");
    for (int i = 0; i < u->skill_count; i++)
        printf(" %s[%s]", w->res[u->skill_ids[i] - 1].name,
               ks_rank_name(w->res[u->skill_ids[i] - 1].rank));
    printf("\n  宝具:");
    for (int i = 0; i < u->phantasm_count; i++)
        printf(" %s[%s]", w->res[u->phantasm_ids[i] - 1].name,
               ks_rank_name(w->res[u->phantasm_ids[i] - 1].rank));
    printf("\n  礼装:");
    for (int i = 0; i < u->item_count; i++)
        printf(" %s", w->res[u->item_ids[i] - 1].name);
    printf("\n");
}
