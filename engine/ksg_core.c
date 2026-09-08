/* ksg_core.c — 空想圣杯引擎:世界/回合/灵脉/行动/魔力/回转/判定/时点/契约 */
#include "ksg.h"

ks_world_t g_w;

void ks_console_utf8(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

/* ---------------- 名称表 ---------------- */

const char *ks_time_name(int t) {
    static const char *n[] = {
        "轮次开始时", "(昼)回合开始时", "(昼)行动提交", "(昼)行动结算", "(昼)回合结束时",
        "(夜)回合开始时", "(夜)行动提交", "(夜)行动结算", "(夜)回合结束时",
        "轮次结束时", "战斗开始时", "初始工序", "主要工序", "最终工序", "战斗结束时", "随时"
    };
    if (t < 0 || t >= KS_TIME_COUNT) return "?"; return n[t];
}

const char *ks_act_name(int a) {
    static const char *n[] = { "-", "机动", "魂食", "干涉", "解放", "制造", "建设",
                               "侦查", "调查", "休整", "摧毁工房" };
    if (a <= 0 || a >= KS_ACT_COUNT) return "?"; return n[a];
}

const char *ks_attr_name(int a) {
    static const char *n[] = { "筋力", "耐久", "敏捷", "魔力", "幸运", "宝具", "回路" };
    if (a < 0 || a >= A_ATTR_COUNT) return "?"; return n[a];
}

const char *ks_slot_name(int s) {
    static const char *n[] = { "未参战", "主力位", "辅助位", "仆役位", "支援位" };
    if (s < 0 || s > KS_SLOT_REAR) return "?"; return n[s];
}

const char *ks_utype_name(int u) {
    switch (u) {
        case KS_U_SERVANT: return "从者";
        case KS_U_MASTER: return "御主";
        case KS_U_SUMMON: return "召唤物";
        case KS_U_DOLL: return "人偶";
        case KS_U_FAMILIAR: return "使魔";
    }
    return "单位";
}

const char *ks_reskind_name(int r) {
    switch (r) {
        case KS_R_SKILL: return "技能";
        case KS_R_PHANTASM: return "宝具";
        case KS_R_ITEM: return "礼装";
        case KS_R_TECH: return "科技造物";
    }
    return "资源";
}

const char *ks_skilltype_name(int t) {
    switch (t) {
        case KS_T_CLASS: return "职阶";
        case KS_T_TALENT: return "天赋";
        case KS_T_TECHNIQUE: return "技艺";
        case KS_T_BLESS: return "祝福";
        case KS_T_CROWN: return "荣冠";
        case KS_T_WEAPON: return "兵器";
        case KS_T_MAGIC: return "魔术";
    }
    return "类型";
}

const char *ks_nptype_name(int t) {
    switch (t) {
        case KS_NP_HUMAN: return "对人";
        case KS_NP_ARMORY: return "对军";
        case KS_NP_CASTLE: return "对城";
        case KS_NP_WORLD: return "对界";
        case KS_NP_BOUND: return "结界";
        case KS_NP_MAKLESS: return "无类型";
    }
    return "类型";
}

int ks_rank_value(int r) { return r; } /* E=1..EX=6, -=0 */

const char *ks_rank_name(int r) {
    static const char *n[] = { "-", "E", "D", "C", "B", "A", "EX" };
    if (r < 0 || r > KS_RANK_EX) return "?"; return n[r];
}

int ks_rank_from_str(const char *s) {
    if (!s) return KS_RANK_E;
    if (!strcmp(s, "EX") || !strcmp(s, "ex")) return KS_RANK_EX;
    if (!strcmp(s, "A")) return KS_RANK_A;
    if (!strcmp(s, "B")) return KS_RANK_B;
    if (!strcmp(s, "C")) return KS_RANK_C;
    if (!strcmp(s, "D")) return KS_RANK_D;
    if (!strcmp(s, "E")) return KS_RANK_E;
    if (!strcmp(s, "-")) return KS_RANK_NEG;
    return KS_RANK_E;
}

const char *ks_when_name(int w) {
    switch (w) {
        case KS_WHEN_PASSIVE: return "常驻";
        case KS_WHEN_ACT: return "行动阶段";
        case KS_WHEN_BATTLE_START: return "战斗开始时";
        case KS_WHEN_PROC: return "指定工序";
        case KS_WHEN_ANY: return "随时";
    }
    return "时机";
}

const char *ks_feat_name(int f) {
    switch (f) {
        case KS_F_MAIN: return "[主力位]";
        case KS_F_SUPPORT: return "[辅助位]";
        case KS_F_SERVANT: return "[仆役位]";
        case KS_F_REAR: return "[支援位]";
        case KS_F_ASSIST: return "[支援]";
        case KS_F_COUNTER: return "[反击]";
        case KS_F_RIDE: return "[骑乘]";
        case KS_F_BURST_READY: return "[蓄力]";
        case KS_F_PIERCE: return "[必中]";
        case KS_F_INV_PIERCE: return "[无敌贯通]";
        case KS_F_ENERGY: return "[爆发]";
    }
    return "";
}

const char *ks_status_name(int s) {
    switch (s) {
        case S_EVADE: return "回避";
        case S_INVINCIBLE: return "无敌";
        case S_PROTECT: return "保护";
        case S_RESUP: return "抗性上升";
        case S_STATE_RES: return "状态抵抗";
        case S_STATE_IM: return "状态免疫";
        case S_EFFECT_IM: return "效果免疫";
        case S_TIRED: return "疲惫";
        case S_CRIPPLED: return "残废";
        case S_LAG: return "迟滞";
        case S_CURSE: return "诅咒";
        case S_SEAL: return "封印";
        case S_SKILL_SEAL: return "技能封印";
        case S_NP_SEAL: return "宝具封印";
        case S_RESDOWN: return "抗性下降";
        case S_POISON: return "中毒";
        case S_BURN: return "灼伤";
        case S_FREEZE: return "冻结";
        case S_ELECTRIC: return "感电";
        case S_STONE: return "石化";
        case S_STUN: return "晕眩";
        case S_CHARM: return "魅惑";
        case S_CONFUSE: return "混乱";
        case S_FEAR: return "恐惧";
        case S_RES_BREAK: return "抗性破除";
        case S_TRAIT_GIVE: return "特性赋予";
    }
    return "状态";
}

int ks_status_class(int s) {
    if (s >= S_EVADE && s <= S_EFFECT_IM) return 1;  /* 强化 */
    if (s >= S_TIRED && s <= S_RESDOWN) return 2;    /* 弱化 */
    return 3;                                        /* 异常 */
}

const char *ks_contract_name(int c) {
    switch (c) {
        case C_HOLY: return "圣杯契约";
        case C_ALLIANCE: return "同盟契约";
        case C_TRUCE: return "不战契约";
        case C_MANA: return "魔力契约";
        case C_FORCE: return "强制契约";
        case C_SLAVE: return "奴役契约";
        case C_DUEL: return "决斗契约";
    }
    return "契约";
}

const char *ks_focus_name(int f) {
    switch (f) {
        case FC_DECISIVE: return "决战";
        case FC_INSTAKILL: return "即死";
        case FC_SWORD: return "魔剑";
        case FC_DEFENSE: return "防御";
        case FC_OFFENSE: return "进攻";
        case FC_BUFF: return "增益";
        case FC_SUMMON: return "召唤";
        case FC_STATUS: return "状态";
        case FC_SUPPLY: return "补给";
        case FC_SPECIAL: return "特殊";
        case FC_ANTITRAIT: return "特攻";
    }
    return "面向";
}

int ks_focus_from_str(const char *s) {
    if (!s) return FC_NONE;
    if (!strcmp(s, "决战")) return FC_DECISIVE;
    if (!strcmp(s, "即死")) return FC_INSTAKILL;
    if (!strcmp(s, "魔剑")) return FC_SWORD;
    if (!strcmp(s, "防御")) return FC_DEFENSE;
    if (!strcmp(s, "进攻")) return FC_OFFENSE;
    if (!strcmp(s, "增益")) return FC_BUFF;
    if (!strcmp(s, "召唤")) return FC_SUMMON;
    if (!strcmp(s, "状态")) return FC_STATUS;
    if (!strcmp(s, "补给")) return FC_SUPPLY;
    if (!strcmp(s, "特殊")) return FC_SPECIAL;
    if (!strcmp(s, "特攻")) return FC_ANTITRAIT;
    return FC_NONE;
}

const char *ks_tactic_name(int t) {
    switch (t) {
        case T_STRIKE: return "强击";
        case T_RAID: return "破袭";
        case T_PROBE: return "试探";
        case T_HOLD: return "扼守";
    }
    return "战术";
}

const char *ks_order_name(int o) {
    switch (o) {
        case O_CHARGE: return "冲锋";
        case O_PURSUE: return "追击";
        case O_COVER: return "掩护";
        case O_DUEL: return "死斗";
    }
    return "指令";
}

const char *ks_eff_name(int e) {
    switch (e) {
        case EF_ATTR_UP: return "属性补正";
        case EF_ATTR_DOWN: return "属性惩罚";
        case EF_WIN_UP: return "胜率补正";
        case EF_WIN_DOWN: return "胜率惩罚";
        case EF_MANA_UP: return "魔力供给";
        case EF_MANA_DOWN: return "魔力消耗";
        case EF_STATUS_GIVE: return "赋予状态";
        case EF_STATUS_REMOVE: return "移除状态";
        case EF_RECAST: return "回转";
        case EF_FP_UP: return "FP增加";
        case EF_FP_DOWN: return "FP减少";
        case EF_DEATH: return "即死";
        case EF_INFO: return "信息";
        case EF_CS: return "令咒";
        case EF_OTHER: return "其他";
    }
    return "效果";
}

const char *ks_cmpt_kind_name(int k) {
    switch (k) {
        case CMPT_BASE: return "基本组件";
        case CMPT_AUX: return "辅助组件";
        case CMPT_LIMITED: return "限定组件";
        case CMPT_SUPPORT: return "支援组件";
    }
    return "组件";
}

const char *ks_trait_name(int t) {
    switch (t) {
        case TR_HUMAN: return "人型";
        case TR_DIVINITY: return "神性";
        case TR_DEMONIC: return "魔性";
        case TR_DRAGON: return "龙种";
        case TR_BEAST: return "猛兽";
        case TR_MONSTER: return "魔兽";
        case TR_GOLEM: return "构装体";
    }
    return "";
}

/* 阵营显示 */
const char *ks_law_name(int v) { return v == KS_LAW ? "秩序" : v == KS_CHAOS ? "混沌" : "中立"; }
const char *ks_moral_name(int v) { return v == KS_GOOD ? "善" : v == KS_EVIL ? "恶" : "中庸"; }

/* ---------------- RNG / 日志 ---------------- */

void ks_rng_seed(ks_world_t *w, unsigned seed) { w->rng_state = seed ? seed : (unsigned)time(NULL); }

unsigned ks_rng_next(ks_world_t *w) {
    /* xorshift32 */
    unsigned x = w->rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    w->rng_state = x;
    return x;
}

int ks_roll(ks_world_t *w) { return (int)(ks_rng_next(w) % 100) + 1; }
int ks_roll_bool(ks_world_t *w, int percent) { return ks_roll(w) <= ks_clamp(percent, 0, 100); }

void ks_log(ks_world_t *w, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vprintf(fmt, ap); va_end(ap);
    printf("\n");
}

void ks_verbose(ks_world_t *w, const char *fmt, ...) {
    if (w->verbosity <= 0) return;
    va_list ap; va_start(ap, fmt);
    vprintf(fmt, ap); va_end(ap);
    printf("\n");
}

/* ---------------- 单位 ---------------- */

int ks_unit_new(ks_world_t *w, const char *name, const char *tname, int utype,
                int level, int faction) {
    if (w->unit_count >= (int)(sizeof(w->units) / sizeof(w->units[0]))) return -1;
    ks_unit_t *u = &w->units[w->unit_count];
    memset(u, 0, sizeof(*u));
    u->id = w->unit_count + 1;
    u->faction = faction;
    u->utype = utype;
    u->level = level;
    u->alive = 1;
    u->fp = 1;
    snprintf(u->name, KSG_NAME_MAX, "%s", name);
    snprintf(u->true_name, KSG_NAME_MAX, "%s", tname ? tname : name);
    /* 默认能力(未说全为 5) */
    for (int i = 0; i < A_ATTR_COUNT; i++) u->attr[i] = 5;
    u->mp.cap = 150; u->mp.floor = -100; u->mp.cur = 50;
    w->unit_count++;
    return u->id;
}

void ks_unit_set_attr(ks_world_t *w, int uid, int attr, int base, int mod) {
    ks_unit_t *u = &w->units[uid - 1];
    u->attr[attr] = base;
    u->attr_mod[attr] = mod;
}

void ks_unit_set_mp(ks_world_t *w, int uid, int cap, int floor, int cur) {
    ks_unit_t *u = &w->units[uid - 1];
    u->mp.cap = cap; u->mp.floor = floor; u->mp.cur = cur;
}

void ks_unit_add_extra_pool(ks_world_t *w, int uid, int cap, int cur) {
    ks_unit_t *u = &w->units[uid - 1];
    for (int i = 0; i < KSG_POOL_MAX; i++) {
        if (u->mp_extra[i].cap == 0) {
            u->mp_extra[i].cap = cap;
            u->mp_extra[i].floor = 0;
            u->mp_extra[i].cur = cur;
            return;
        }
    }
}

int ks_unit_attr_value(ks_world_t *w, int uid, int attr) {
    ks_unit_t *u = &w->units[uid - 1];
    int v = u->attr[attr] + u->attr_mod[attr] + u->attr_perm[attr];
    /* 疲惫惩罚 */
    ks_stat_t *st = NULL;
    for (int i = 0; i < KSG_STATUS_MAX; i++)
        if (u->status[i].kind == S_TIRED) { st = &u->status[i]; break; }
    if (st && attr != A_NP) v -= 5 * st->layers;
    /* 冻结惩罚:[筋力][耐久][敏捷]-5*层 */
    if (attr == A_STR || attr == A_END || attr == A_AGI) {
        for (int i = 0; i < KSG_STATUS_MAX; i++)
            if (u->status[i].kind == S_FREEZE) v -= 5 * u->status[i].layers;
    }
    /* 石化:削减[筋力][耐久][敏捷]属性补正60 */
    if (attr == A_STR || attr == A_END || attr == A_AGI) {
        for (int i = 0; i < KSG_STATUS_MAX; i++)
            if (u->status[i].kind == S_STONE) v -= 60;
    }
    /* 初始属性下限为5;但[宝具][回路]允许为0(御主无宝具) */
    if (attr == A_NP || attr == A_CIRCUIT) return ks_max(0, v);
    return ks_max(5, v);
}

int ks_unit_effective_attr(ks_world_t *w, int uid, int attr) {
    return ks_unit_attr_value(w, uid, attr);
}

int ks_unit_gain_status(ks_world_t *w, int uid, int kind, int layers, int src) {
    ks_unit_t *u = &w->units[uid - 1];
    if (!u->alive) return 0;
    /* 状态免疫:拥有对应[状态免疫]则直接拒绝生效 */
    for (int i = 0; i < KSG_STATUS_MAX; i++) {
        ks_stat_t *st = &u->status[i];
        if (st->kind == S_STATE_IM && st->layers == kind) {
            ks_verbose(w, "  · %s [状态免疫:%s]无效化赋予", u->name, ks_status_name(kind));
            return 0;
        }
    }
    int add = layers ? layers : 1;
    /* 层数上限(规则书):诅咒20、感电20、魅惑9 */
    int cap = 0;
    switch (kind) {
        case S_CURSE: cap = 20; break;
        case S_ELECTRIC: cap = 20; break;
        case S_CHARM: cap = 9; break;
        default: break;
    }
    for (int i = 0; i < KSG_STATUS_MAX; i++) {
        if (u->status[i].kind == kind) {
            u->status[i].layers += add;
            if (cap > 0 && u->status[i].layers > cap) u->status[i].layers = cap;
            u->status[i].source = src;
            return i;
        }
    }
    for (int i = 0; i < KSG_STATUS_MAX; i++) {
        if (u->status[i].kind == S_NONE) {
            u->status[i].kind = kind;
            u->status[i].layers = (cap > 0 && add > cap) ? cap : add;
            u->status[i].source = src;
            return i;
        }
    }
    return -1;
}

int ks_unit_has_status(ks_world_t *w, int uid, int kind) {
    ks_unit_t *u = &w->units[uid - 1];
    for (int i = 0; i < KSG_STATUS_MAX; i++)
        if (u->status[i].kind == kind) return 1;
    return 0;
}

int ks_unit_state_res_check(ks_world_t *w, int uid, int kind) {
    /* 状态抵抗:负面判定最终成功率减半,至少-20% (简化) */
    ks_unit_t *u = &w->units[uid - 1];
    for (int i = 0; i < KSG_STATUS_MAX; i++) {
        if (u->status[i].kind == S_STATE_RES && u->status[i].layers == kind) return 20;
        if (u->status[i].kind == S_STATE_IM && u->status[i].layers == kind) return 100; /* 免疫 */
    }
    return 0;
}

int ks_unit_resist(ks_world_t *w, int uid, int base) {
    /* 抗性上升:负面判定最终成功率惩罚 */
    ks_unit_t *u = &w->units[uid - 1];
    int res = 0;
    for (int i = 0; i < KSG_STATUS_MAX; i++) {
        if (u->status[i].kind == S_RESUP) res += u->status[i].layers;
        if (u->status[i].kind == S_RESDOWN) res -= u->status[i].layers;
    }
    return ks_clamp(base - res, 0, 100);
}

int ks_unit_add_res(ks_world_t *w, int uid, int id) {
    ks_unit_t *u = &w->units[uid - 1];
    ks_res_t *r = &w->res[id - 1];
    if (!r->id) return -1;
    switch (r->kind) {
        case KS_R_SKILL:
            if (u->skill_count < KSG_SKILL_MAX) u->skill_ids[u->skill_count++] = id;
            break;
        case KS_R_PHANTASM:
            if (u->phantasm_count < KSG_PHANTASM_MAX) u->phantasm_ids[u->phantasm_count++] = id;
            break;
        case KS_R_ITEM:
        case KS_R_TECH:
            if (u->item_count < KSG_ITEM_MAX) u->item_ids[u->item_count++] = id;
            break;
    }
    return 0;
}

void ks_status_tick(ks_world_t *w, int uid) {
    ks_status_round_end_tick(w, uid);
}

void ks_status_proc_tick(ks_world_t *w, int uid) {
    ks_status_proc_end_tick(w, uid);
}

/* 效果结算引擎移至 ksg_effects.c */

void ks_unit_tick(ks_world_t *w, int uid) {
    /* 轮次结束时点 */
    ks_unit_t *u = &w->units[uid - 1];
    ks_status_tick(w, uid);
    /* 等级魔耗+常驻魔耗→结算 */
    int lvl_cost = u->level / 2;
    int passive_cost = 0;
    for (int i = 0; i < u->skill_count; i++) {
        ks_res_t *r = &w->res[u->skill_ids[i] - 1];
        if (r->when == KS_WHEN_PASSIVE) passive_cost += r->cost;
    }
    for (int i = 0; i < u->phantasm_count; i++) {
        ks_res_t *r = &w->res[u->phantasm_ids[i] - 1];
        if (r->when == KS_WHEN_PASSIVE) passive_cost += r->cost;
    }
    ks_mp_pay(w, uid, lvl_cost + passive_cost);
    ks_mp_settle(w, uid);
    /* 回路供魔(御主) */
    if (u->utype == KS_U_MASTER) ks_mp_gain(w, uid, ks_unit_attr_value(w, uid, A_CIRCUIT));
}

int ks_unit_mp_sum(ks_world_t *w, int uid) {
    ks_unit_t *u = &w->units[uid - 1];
    int s = u->mp.cur;
    for (int i = 0; i < KSG_POOL_MAX; i++) s += u->mp_extra[i].cur;
    return s;
}

void ks_unit_transfer_mp(ks_world_t *w, int from, int to, int n) {
    ks_unit_t *f = &w->units[from - 1], *t = &w->units[to - 1];
    int real = ks_min(n, ks_max(0, f->mp.cur));
    f->mp.cur -= real;
    t->mp.cur += real;
    ks_verbose(w, "  · 魔力转移 %s→%s %d", f->name, t->name, real);
}

/* ---------------- 魔力 ---------------- */

int ks_mp_pay(ks_world_t *w, int uid, int n) {
    ks_unit_t *u = &w->units[uid - 1];
    int pay = ks_min(n, u->mp.cur);
    u->mp.cur -= pay;
    if (pay < n) ks_verbose(w, "  · %s 魔力不足,仅支付 %d/%d", u->name, pay, n);
    /* 低于下限:强制退场(简化) */
    if (u->mp.cur < u->mp.floor && u->utype == KS_U_SERVANT) {
        ks_verbose(w, "  · %s 魔力低于下限,从者消散!", u->name);
        u->alive = 0;
    } else if (u->mp.cur < u->mp.floor && u->utype == KS_U_MASTER) {
        u->status[0].kind = S_TIRED; u->status[0].layers += 1;
        ks_verbose(w, "  · %s 魔力低于下限,获得疲惫1", u->name);
    }
    return pay;
}

void ks_mp_gain(ks_world_t *w, int uid, int n) {
    ks_unit_t *u = &w->units[uid - 1];
    u->mp.cur += n;
    if (u->mp.cur > u->mp.cap) u->mp.cur = u->mp.cap;
}

void ks_mp_settle(ks_world_t *w, int uid) {
    ks_unit_t *u = &w->units[uid - 1];
    /* 移除溢出 */
    if (u->mp.cur > u->mp.cap) u->mp.cur = u->mp.cap;
    /* 魔力不足:从者除宝具外全属性-10惩罚(每-20魔力) */
    if (u->mp.cur < 0 && u->utype == KS_U_SERVANT) {
        int pen = (int)((-u->mp.cur) / 20) * 10;
        for (int a = 0; a < A_NP; a++) u->attr_mod[a] -= pen;
        ks_verbose(w, "  · %s 魔力不足 %, 全属性-%d", u->name, -u->mp.cur, pen);
    }
}

/* ---------------- 回转 ---------------- */

void ks_recast_tick_round(ks_world_t *w, int uid) {
    ks_unit_t *u = &w->units[uid - 1];
    for (int i = 0; i < u->skill_count; i++) {
        ks_res_t *r = &w->res[u->skill_ids[i] - 1];
        if (r->recast <= 0) continue;
        r->cur_recast += 3;
        if (r->cur_recast > r->recast) r->cur_recast = r->recast;
    }
    for (int i = 0; i < u->phantasm_count; i++) {
        ks_res_t *r = &w->res[u->phantasm_ids[i] - 1];
        if (r->recast <= 0) continue;
        r->cur_recast += 3;
        if (r->cur_recast > r->recast) r->cur_recast = r->recast;
    }
}

void ks_recast_tick_proc(ks_world_t *w, int uid) {
    ks_unit_t *u = &w->units[uid - 1];
    for (int i = 0; i < u->skill_count; i++) {
        ks_res_t *r = &w->res[u->skill_ids[i] - 1];
        if (r->recast <= 0) continue;
        r->cur_recast += 1;
        if (r->cur_recast > r->recast) r->cur_recast = r->recast;
    }
    for (int i = 0; i < u->phantasm_count; i++) {
        ks_res_t *r = &w->res[u->phantasm_ids[i] - 1];
        if (r->recast <= 0) continue;
        r->cur_recast += 1;
        if (r->cur_recast > r->recast) r->cur_recast = r->recast;
    }
}

int ks_res_use(ks_world_t *w, int uid, int rid) {
    ks_unit_t *u = &w->units[uid - 1];
    ks_res_t *r = &w->res[rid - 1];
    if (!r->id) return 0;
    if (r->kind == KS_R_PHANTASM || r->kind == KS_R_SKILL) {
        if (r->cur_recast < r->recast) return 0; /* 回转未完 */
        if (r->cost > 0 && ks_unit_mp_sum(w, uid) < r->cost) return 0; /* 魔力不足 */
    } else if (r->kind == KS_R_ITEM || r->kind == KS_R_TECH) {
        /* 礼装:单件每轮次数(可复用=1),以及单位整体每轮次数(规则:礼装资源书
           御主每轮上限5,从者0,仆役0;Caster视为御主的场合放宽) */
        if (r->kind == KS_R_ITEM && r->used_this_round >= r->count_per_round) return 0;
        int max_uses = 0;
        if (u->utype == KS_U_MASTER) max_uses = 5;
        else if (u->utype == KS_U_SERVANT && u->servant_class == 5) max_uses = 5; /* Caster */
        else max_uses = 0;
        if (u->item_uses_this_round >= max_uses) {
            ks_verbose(w, "  · %s 本轮礼装使用已达上限(%d)", u->name, max_uses);
            return 0;
        }
        if (r->kind == KS_R_ITEM) u->item_uses_this_round++;
    }
    if (r->kind == KS_R_ITEM) r->used_this_round++;
    if (r->cost > 0) ks_mp_pay(w, uid, r->cost);
    /* 回转入账 */
    if (r->kind == KS_R_SKILL || r->kind == KS_R_PHANTASM) {
        r->cur_recast = 0; /* 冷却开始 */
    }
    return 1;
}

void ks_res_gain_recast(ks_world_t *w, int uid, int rid, int n) {
    ks_res_t *r = &w->res[rid - 1];
    if (!r->id || r->recast <= 0) return;
    r->cur_recast += n;
    if (r->cur_recast > r->recast) r->cur_recast = r->recast;
}

/* ---------------- 判定 ---------------- */

int ks_check_final(ks_world_t *w, ks_check_t *c) {
    int base = ks_clamp(c->base + c->base_mod - c->base_pen, 0, 100);
    int fin = ks_clamp(base + c->final_mod - c->final_pen, 0, 100);
    return fin;
}

/* 游荡限制:行动/技能/宝具判定成功率至多10%(规则 8.1) */
int ks_check_roaming_cap(ks_world_t *w, int uid, int rate) {
    if (uid > 0 && uid <= w->unit_count && w->units[uid - 1].roaming) {
        if (rate > 10) {
            ks_verbose(w, "  · %s 处于[游荡状态],判定成功率上限10%%(%d%%→10%%)",
                       w->units[uid - 1].name, rate);
            rate = 10;
        }
    }
    return rate;
}

int ks_check_roll(ks_world_t *w, int uid, ks_check_t *c, int *final_rate) {
    int f = ks_check_final(w, c);
    f = ks_check_roaming_cap(w, uid, f);   /* 游荡成功率上限10% */
    if (final_rate) *final_rate = f;
    int r = ks_roll(w);
    return r <= f;
}

int ks_check_negative(ks_world_t *w, int uid, int target, ks_check_t *c) {
    /* 负面判定:基础成功率受目标抗性上升惩罚 */
    c->neg = 1;
    if (target > 0) {
        c->final_pen += ks_unit_resist(w, target, 0);
    }
    return ks_check_roll(w, uid, c, NULL);
}

/* ---------------- 行动注册/执行 ---------------- */

ks_action_entry_t g_actions[KSG_ACTION_MAX];
int g_actions_n = 0;

static int ks_act_order(int type) {
    /* 规则书 3.12: 机动-魂食-干涉-解放-制造-建设-侦查-调查-休整-摧毁工房 */
    switch (type) {
        case KS_ACT_MOVEMENT: return 0;
        case KS_ACT_SOULFEED: return 1;
        case KS_ACT_INTERVENE: return 2;
        case KS_ACT_UNLOCK: return 3;
        case KS_ACT_CRAFT: return 4;
        case KS_ACT_BUILD: return 5;
        case KS_ACT_RECON: return 6;
        case KS_ACT_INVESTIGATE: return 7;
        case KS_ACT_REST: return 8;
        case KS_ACT_DEMOLISH: return 9;
    }
    return 99;
}

int ks_action_register(ks_world_t *w, int type, ks_action_fn fn, const char *help) {
    if (g_actions_n >= KSG_ACTION_MAX) return -1;
    g_actions[g_actions_n].type = type;
    g_actions[g_actions_n].order = ks_act_order(type);
    g_actions[g_actions_n].fn = fn;
    g_actions[g_actions_n].help = help;
    return g_actions_n++;
}

void ks_action_do(ks_world_t *w, int uid, int type, const char *arg) {
    ks_unit_t *u = &w->units[uid - 1];
    if (!u->alive) return;
    if (u->acted) { ks_verbose(w, "  %s 本回合已行动过", u->name); return; }
    for (int i = 0; i < g_actions_n; i++) {
        if (g_actions[i].type == type) {
            w->active_action = type;
            u->acted = 1;
            g_actions[i].fn(w, uid, arg);
            w->active_action = KS_ACT_NONE;
            return;
        }
    }
    ks_log(w, "未知行动 %d", type);
}

static void ks_action_swap(ks_action_entry_t *a, ks_action_entry_t *b) {
    ks_action_entry_t t = *a; *a = *b; *b = t;
}

void ks_action_tick(ks_world_t *w) {
    /* 按规则书上行动处理顺序排序并逐个执行 */
    for (int i = 0; i < g_actions_n; i++)
        for (int j = i + 1; j < g_actions_n; j++)
            if (g_actions[j].order < g_actions[i].order)
                ks_action_swap(&g_actions[j], &g_actions[i]);
    for (int i = 0; i < g_actions_n; i++) {
        /* 只执行本回合已提交的行动(这里简化:管理者按单位轮询) */
    }
}

/* ---------------- 内置行动 ---------------- */

static ks_leyline_t *ks_find_leyline(ks_world_t *w, int id) {
    if (id < 1 || id > w->leyline_count) return NULL;
    return &w->leylines[id - 1];
}

int ks_act_intervene(ks_world_t *w, int uid, const char *arg) {
    ks_unit_t *u = &w->units[uid - 1];
    int lid = arg ? atoi(arg) : 1;
    ks_leyline_t *l = ks_find_leyline(w, lid);
    if (!l) { ks_log(w, "  %s 干涉失败:灵脉 %d 不存在", u->name, lid); return 1; }
    ks_log(w, "  %s [干涉]→%s(魔力量%d 人流量%d)", u->name, l->name, l->mana, l->flow);
    /* 交流环节:同灵脉单位 */
    for (int i = 0; i < w->unit_count; i++) {
        ks_unit_t *o = &w->units[i];
        if (o->id == uid || !o->alive) continue;
        ks_verbose(w, "    · 交流:与 %s 处于同一灵脉,可进行契约/袭击/进驻/私聊", o->name);
    }
    ks_battle_cfg_t cfg = { 0 };
    cfg.leyline = lid;
    cfg.width = 4;
    cfg.day = w->day;
    /* 若有敌方单位在目标灵脉,自动进入战斗(演示) */
    for (int i = 0; i < w->unit_count; i++) {
        ks_unit_t *o = &w->units[i];
        if (o->faction != u->faction && o->alive &&
            o->battle_side == 0 && ks_find_leyline(w, lid) == l &&
            /* 简化:敌方在"当前灵脉"才战斗 */
            strcmp(o->name, "") != 0) {
            break;
        }
    }
    return 0;
}

int ks_act_recon(ks_world_t *w, int uid, const char *arg) {
    ks_unit_t *u = &w->units[uid - 1];
    ks_check_t c = { 0 };
    c.base = (arg && strstr(arg, "oco")) ? 60 : 30; /* 定向 60 / 广泛 30 */
    ks_log(w, "  %s [侦查]%s 基础成功率%d%%", u->name,
           (c.base == 60) ? "定向侦查" : "广泛侦查", c.base);
    if (ks_check_negative(w, uid, 0, &c)) {
        ks_log(w, "    · 判定成功!获取全部灵脉的灵脉信息(%d 处)", w->leyline_count);
        for (int i = 0; i < w->leyline_count; i++) {
            ks_leyline_t *l = &w->leylines[i];
            ks_verbose(w, "    · %s:魔力量%d 人流量%d 工房:%s", l->name, l->mana, l->flow,
                       l->has_workshop ? "有" : "无");
        }
    } else {
        ks_log(w, "    · 判定失败,未获得信息");
    }
    return 0;
}

int ks_act_rest(ks_world_t *w, int uid, const char *arg) {
    ks_unit_t *u = &w->units[uid - 1];
    ks_log(w, "  %s [休整]", u->name);
    /* 规则 8.1:游荡中的单位进行[休整]时,获得[疲惫1] */
    if (u->roaming) {
        ks_unit_gain_status(w, uid, S_TIRED, 1, 0);
        ks_log(w, "    · 游荡中休整!获得[疲惫1]");
    }
    /* 补魔:御主与从者同时执行,从者获得回路属性数值的魔力补给 */
    for (int i = 0; i < w->unit_count; i++) {
        ks_unit_t *o = &w->units[i];
        if (o->faction == u->faction && o->utype == KS_U_MASTER && o->id != uid) {
            int gain = ks_unit_attr_value(w, o->id, A_CIRCUIT);
            ks_mp_gain(w, uid, gain);
            ks_log(w, "    · 补魔完成!%s 获得 %d 魔力补给(御主回路)", u->name, gain);
        }
    }
    /* 自我调整:下回合行动判定+20% */
    {
        ks_check_t c = { 0 }; c.base = 100;
        ks_verbose(w, "    · 自我调整:下回合行动判定+20%%");
    }
    return 0;
}

int ks_act_soulfeed(ks_world_t *w, int uid, const char *arg) {
    ks_unit_t *u = &w->units[uid - 1];
    ks_leyline_t *l = ks_find_leyline(w, 1);
    if (!l || l->flow <= 0) { ks_log(w, "  %s 魂食失败:人流量为0", u->name); return 1; }
    int mode = (arg && strstr(arg, "flagrant")) ? 2 : (arg && strstr(arg, "unlimited")) ? 3 : 1;
    ks_log(w, "  %s [魂食]:%s", u->name,
           mode == 1 ? "遮蔽魂食(30%)" : mode == 2 ? "恶性魂食" : "无限制魂食");
    if (mode == 1) {
        ks_check_t c = { 0 }; c.base = 30;
        int ok = ks_check_negative(w, uid, 0, &c);
        if (ok) {
            l->flow -= 1;
            ks_mp_gain(w, uid, 60);
            ks_log(w, "    · 遮蔽成功!人流量-1,获得 60 魔力。魂食消息通告全局");
            /* 通告:其他单位获得介入指令(简化:全局唤醒) */
            for (int i = 0; i < w->unit_count; i++)
                if (w->units[i].id != uid && w->units[i].alive)
                    w->units[i].battle_side = 0; /* NOOP */
        } else {
            ks_log(w, "    · 遮蔽失败!立即通告全局");
        }
    } else if (mode == 2) {
        l->flow -= 1;
        ks_mp_gain(w, uid, 60);
        ks_log(w, "    · 立即通告全局,人流量-1,获得60魔力。回合结束时人流量-2");
    } else {
        l->flow = 0; l->mana = 0;
        ks_mp_gain(w, uid, 60 * l->flow);
        ks_log(w, "    · 屠杀姿态!人流量/魔力量归零");
    }
    return 0;
}

int ks_act_movement(ks_world_t *w, int uid, const char *arg) {
    ks_unit_t *u = &w->units[uid - 1];
    int lid = arg ? atoi(arg) : (w->leyline_count > 1 ? 2 : 1);
    ks_leyline_t *l = ks_find_leyline(w, lid);
    if (!l) { ks_log(w, "  %s 机动失败:目标灵脉不存在", u->name); return 1; }
    ks_log(w, "  %s [机动]→%s(立即通告全局,暴露自身所处灵脉,其他机动单位获得[介入])",
           u->name, l->name);
    u->battle_side = 0;
    u->roaming = 0;
    return 0;
}

int ks_act_craft(ks_world_t *w, int uid, const char *arg) {
    ks_unit_t *u = &w->units[uid - 1];
    int can = 0;
    for (int i = 0; i < u->skill_count; i++) {
        ks_res_t *r = &w->res[u->skill_ids[i] - 1];
        if (!strcmp(r->name, "道具制作") || !strcmp(r->name, "阵地制作")) can = 1;
    }
    if (u->utype == KS_U_MASTER) can = 1;
    if (!can) { ks_log(w, "  %s 无法进行[礼装制作](需道具制作/相关技能)", u->name); return 1; }
    int base = ks_unit_attr_value(w, uid, A_MAG) * 2;
    ks_check_t c = { 0 }; c.base = base;
    ks_log(w, "  %s [礼装制作] 基础成功率=%d%%", u->name, base);
    if (ks_check_negative(w, uid, 0, &c))
        ks_log(w, "    · 制作成功!获得礼装");
    else
        ks_log(w, "    · 制作失败");
    return 0;
}

/* 工房组件索引查询 */
static ks_component_t *ks_find_cmpt(ks_world_t *w, const char *name) {
    for (int i = 0; i < w->cmpt_count; i++)
        if (!strcmp(w->cmpts[i].name, name)) return &w->cmpts[i];
    return NULL;
}

int ks_act_build(ks_world_t *w, int uid, const char *arg) {
    ks_unit_t *u = &w->units[uid - 1];
    ks_leyline_t *l = &w->leylines[0];
    ks_log(w, "  %s [工房建造]", u->name);
    /* 规则(工房建造书):需[阵地制作]或魔术师职业 */
    int can = (u->utype == KS_U_MASTER);
    for (int i = 0; i < u->skill_count; i++)
        if (!strcmp(w->res[u->skill_ids[i] - 1].name, "阵地制作")) can = 1;
    if (!can) {
        ks_log(w, "    无法建造:需要[阵地制作]技能或魔术师职业");
        return 1;
    }
    if (!w->cmpt_count) { ks_log(w, "    (无可用组件模板)"); return 1; }

    /* 找现有工房或创建 */
    ks_workshop_t *ws = NULL;
    for (int i = 0; i < w->workshop_count; i++)
        if (w->workshops[i].owner == uid) { ws = &w->workshops[i]; break; }
    if (!ws) {
        if (w->workshop_count < (int)(sizeof(w->workshops) / sizeof(w->workshops[0]))) {
            ws = &w->workshops[w->workshop_count++];
            memset(ws, 0, sizeof(*ws));
            ws->leyline = 1;
            ws->owner = uid;
            for (int i = 0; i < 8; i++) ws->components[i] = -1;
            if (arg && arg[0]) ws->leyline = atoi(arg);
            l->has_workshop = 1;
            ks_log(w, "    建立新工房于灵脉%d(工房主:%s)", ws->leyline, u->name);
        } else {
            ks_log(w, "    (工房数量已达上限)");
            return 1;
        }
    }

    /* 选择一个组件并建设(每次行动+[建设1]) */
    printf("  可选组件:\n");
    for (int i = 0; i < w->cmpt_count; i++)
        printf("    %2d) %s [%s] 需建设%d 稳态%d 规模%d\n", i + 1, w->cmpts[i].name,
               ks_cmpt_kind_name(w->cmpts[i].kind), w->cmpts[i].build,
               w->cmpts[i].hp, w->cmpts[i].scale);
    int pick = ks_ui_ask_int("选择要建设的组件(0=取消)", 0, w->cmpt_count, 0);
    if (pick <= 0) return 1;
    int ci = pick - 1;
    /* 同组件不能重复 */
    int slot = -1;
    for (int i = 0; i < 8; i++) {
        if (ws->components[i] == ci) { slot = i; break; }
    }
    if (slot < 0) {
        for (int i = 0; i < 8; i++) {
            if (ws->components[i] < 0) { slot = i; break; }
        }
        if (slot < 0) { ks_log(w, "    (工房组件槽位已满)"); return 1; }
        ws->components[slot] = ci;
        ws->build_prog[slot] = 0;
        ks_log(w, "    新组件 %s 开始建设(0/%d)", w->cmpts[ci].name, w->cmpts[ci].build);
    }
    int need = w->cmpts[ci].build;
    if (ws->build_prog[slot] >= need) {
        ks_log(w, "    %s 已建设完成(%d/%d)", w->cmpts[ci].name, ws->build_prog[slot], need);
        return 1;
    }
    ws->build_prog[slot] += 1;
    ws->total_scale += (ws->build_prog[slot] == need) ? w->cmpts[ci].scale : 0;
    ks_log(w, "    %s 建设进度 %d/%d%s", w->cmpts[ci].name,
           ws->build_prog[slot], need,
           ws->build_prog[slot] >= need ? " —— 完成!" : "");
    /* 神殿判定:建成[圣所基盘]时,工房升级为神殿(规则:工房建造书 3) */
    if (!strcmp(w->cmpts[ci].name, "圣所基盘") && ws->build_prog[slot] >= need) {
        l->has_shrine = 1;
        l->has_workshop = 0;
        ks_log(w, "    ✨ 神殿基盘完成!此灵脉现为[神殿],与魔术工房互斥");
    }
    return 0;
}

int ks_act_investigate(ks_world_t *w, int uid, const char *arg) {
    ks_unit_t *u = &w->units[uid - 1];
    ks_log(w, "  %s [调查]", u->name);
    /* 情报调查50% / 资料分析30% / 真名猜测 */
    ks_check_t c = { 0 }; c.base = 50;
    if (ks_check_negative(w, uid, 0, &c))
        ks_log(w, "    · 情报调查成功:获得目标的卡面信息(基本资料/能力面板/技能/礼装)");
    else
        ks_log(w, "    · 情报调查失败");
    return 0;
}

int ks_act_unlock(ks_world_t *w, int uid, const char *arg) {
    ks_unit_t *u = &w->units[uid - 1];
    /* 解放:选择宝具 */
    int rid = arg ? atoi(arg) : (u->phantasm_count ? u->phantasm_ids[0] : 0);
    if (!rid) { ks_log(w, "  %s 没有可解放的宝具", u->name); return 1; }
    ks_res_t *r = &w->res[rid - 1];
    if (!ks_res_use(w, uid, rid)) {
        ks_log(w, "  %s 解放失败:%s(回转未完成或魔力不足)", u->name, r->name);
        return 1;
    }
    ks_log(w, "  %s [解放]%s", u->name, r->name);
    return 0;
}

int ks_act_demolish(ks_world_t *w, int uid, const char *arg) {
    ks_unit_t *u = &w->units[uid - 1];
    ks_leyline_t *l = ks_find_leyline(w, 1);
    ks_log(w, "  %s [摧毁工房]", u->name);
    if (l && l->has_workshop) {
        l->has_workshop = 0;
        ks_log(w, "    · 摧毁了 %s 的魔术工房!", l->name);
    } else ks_log(w, "    · 当前灵脉没有魔术工房");
    return 0;
}

/* ---------------- 世界 ---------------- */

void ks_world_init(ks_world_t *w, unsigned seed) {
    memset(w, 0, sizeof(*w));
    ks_rng_seed(w, seed);
    w->round = 1;
    w->day = 1;
    w->phase = KS_TIME_ROUND_START;
    w->verbosity = 1;
    ks_data_build(w);
    ks_db_load(w);
    /* 注册行动 */
    ks_action_register(w, KS_ACT_MOVEMENT, ks_act_movement, "机动 [灵脉id]");
    ks_action_register(w, KS_ACT_SOULFEED, ks_act_soulfeed, "魂食 [shelter|flagrant|unlimited]");
    ks_action_register(w, KS_ACT_INTERVENE, ks_act_intervene, "干涉 [灵脉id]");
    ks_action_register(w, KS_ACT_UNLOCK, ks_act_unlock, "解放 [宝具id]");
    ks_action_register(w, KS_ACT_CRAFT, ks_act_craft, "制造");
    ks_action_register(w, KS_ACT_BUILD, ks_act_build, "建设");
    ks_action_register(w, KS_ACT_RECON, ks_act_recon, "侦查 [broad|covert]");
    ks_action_register(w, KS_ACT_INVESTIGATE, ks_act_investigate, "调查");
    ks_action_register(w, KS_ACT_REST, ks_act_rest, "休整");
    ks_action_register(w, KS_ACT_DEMOLISH, ks_act_demolish, "摧毁工房");
}

void ks_world_round_start(ks_world_t *w) {
    w->phase = KS_TIME_ROUND_START;
    ks_log(w, "====== 第 %d 轮次开始(第 %d 天) ======", w->round, w->round);
    /* 每单位可行动复位 */
    for (int i = 0; i < w->unit_count; i++) {
        w->units[i].acted = 0;
        /* 回合开始时点:状态判定(中毒/灼伤/残废/晕眩等) */
        if (w->units[i].alive) ks_status_round_start_tick(w, w->units[i].id);
    }
    /* 礼装每轮次使用次数重置(规则:礼装资源书) */
    for (int i = 0; i < w->res_count; i++)
        w->res[i].used_this_round = 0;
    for (int i = 0; i < w->unit_count; i++)
        w->units[i].item_uses_this_round = 0;
}

void ks_world_day_night(ks_world_t *w) { w->day = !w->day; }

void ks_world_round_end(ks_world_t *w) {
    w->phase = KS_TIME_ROUND_END;
    ks_log(w, "------ 轮次结束结算 ------");
    for (int i = 0; i < w->unit_count; i++) {
        if (!w->units[i].alive) continue;
        ks_unit_t *u = &w->units[i];
        int before = u->mp.cur;
        ks_unit_tick(w, u->id);
        /*
         * 规则书 8.1 游荡状态的特别影响:
         *  - 游荡单位无法在回合结束时获得回转补充
         *  - 御主:每回合结束 耐久*2% 疲惫判定,失败获得[疲惫1]
         *  - 从者:每回合结束 等级% 判定,成功产生[等级-30]魔耗,失败获得[疲惫1]
         */
        if (!u->roaming) {
            ks_recast_tick_round(w, u->id);   /* 非游荡才获回转 */
        } else {
            ks_verbose(w, "  · %s 处于[游荡灵脉],回合结束无法获得回转补充", u->name);
        }
        /* 召唤物维护:等级/2 魔耗(词典 3.3),无魔力池→所有者代付 */
        if (u->utype == KS_U_SUMMON || u->utype == KS_U_DOLL) {
            int cost = u->level / 2;
            if (cost > 0) {
                if (u->mp.cap > 0) {
                    ks_mp_pay(w, u->id, cost);
                    ks_log(w, "  · 召唤物[%s] 维护消耗 %d 魔力", u->name, cost);
                } else {
                    /* 无魔力池:由所有者(源单位)代付 */
                    int owner = u->mp.floor != 0 ? 0 : u->faction;   /* 简化:阵营同侧御主 */
                    ks_log(w, "  · 召唤物[%s] 无魔力池,维护费 %d 由所有者代付", u->name, cost);
                    for (int j = 0; j < w->unit_count; j++) {
                        if (w->units[j].utype == KS_U_MASTER && w->units[j].faction == u->faction) {
                            ks_mp_pay(w, w->units[j].id, cost);
                            owner = w->units[j].id;
                            break;
                        }
                    }
                    if (owner == 0) {
                        ks_log(w, "    · 无御主代付!召唤物[%s] 立即退场", u->name);
                        u->alive = 0;
                    }
                }
            }
        }
        if (u->roaming) {
            if (u->utype == KS_U_MASTER) {
                int rate = ks_unit_attr_value(w, u->id, A_END) * 2;
                ks_log(w, "  · %s [游荡]疲惫判定 %d%%", u->name, rate);
                if (!ks_roll_bool(w, rate)) {
                    ks_unit_gain_status(w, u->id, S_TIRED, 1, 0);
                    ks_log(w, "    判定失败,获得[疲惫1]!");
                }
            } else if (u->utype == KS_U_SERVANT) {
                int rate = u->level;
                ks_log(w, "  · %s [游荡]等级判定 %d%%", u->name, rate);
                if (ks_roll_bool(w, rate)) {
                    int cost = u->level - 30;
                    if (cost > 0) {
                        ks_mp_pay(w, u->id, cost);
                        ks_log(w, "    判定成功!产生 %d 魔力消耗", cost);
                    }
                } else {
                    ks_unit_gain_status(w, u->id, S_TIRED, 1, 0);
                    ks_log(w, "    判定失败,获得[疲惫1]!");
                }
            }
        }
        ks_log(w, "  [%s] %d → %d(等级/常驻魔耗结算)", u->name, before, u->mp.cur);
    }
    /* 灵脉供魔:给灵脉主 */
    for (int i = 0; i < w->leyline_count; i++) {
        ks_leyline_t *l = &w->leylines[i];
        if (l->owner > 0 && l->owner <= w->unit_count) {
            ks_unit_t *o = &w->units[l->owner - 1];
            ks_mp_gain(w, o->id, l->mana);
            ks_verbose(w, "  灵脉供魔:%s → %s %d", l->name, o->name, l->mana);
        }
    }
    w->round++;
    ks_world_day_night(w);
    /* 昼夜切换:进入下一回合,各单位行动机会重置 */
    for (int i = 0; i < w->unit_count; i++) w->units[i].acted = 0;
}

int ks_world_advance(ks_world_t *w) {
    /* 简化:回合内推进 */
    if (w->phase == KS_TIME_DAY_ACT) {
        w->phase = KS_TIME_DAY_RESOLVE;
        for (int i = 0; i < w->unit_count; i++)
            if (w->units[i].alive) ks_action_tick(w);
        ks_world_day_night(w);
        w->phase = KS_TIME_NIGHT_ACT;
        return 1;
    } else if (w->phase == KS_TIME_NIGHT_ACT) {
        w->phase = KS_TIME_NIGHT_RESOLVE;
        ks_world_round_end(w);
        w->phase = KS_TIME_ROUND_START;
        ks_world_round_start(w);
        return 1;
    }
    return 0;
}

int ks_world_register_res(ks_world_t *w, ks_res_t *r) {
    if (w->res_count >= (int)(sizeof(w->res) / sizeof(w->res[0]))) return -1;
    r->id = w->res_count + 1;
    w->res[w->res_count] = *r;
    w->res_count++;
    return r->id;
}

int ks_world_find_res(ks_world_t *w, const char *name) {
    for (int i = 0; i < w->res_count; i++)
        if (!strcmp(w->res[i].name, name)) return w->res[i].id;
    return 0;
}

void ks_contract_sign(ks_world_t *w, int kind, int a, int b) {
    if (w->contract_count >= (int)(sizeof(w->contracts) / sizeof(w->contracts[0]))) return;
    ks_contract_t *c = &w->contracts[w->contract_count++];
    c->kind = kind; c->a = a; c->b = b; c->active = 1;
    snprintf(c->note, KSG_TEXT_MAX, "%s × %s", ks_contract_name(kind),
             w->units[b - 1].name);
    ks_log(w, "契约成立:%s(%s 立约, %s 签约)", ks_contract_name(kind),
           w->units[a - 1].name, w->units[b - 1].name);
}

/* 契约行为:检查该契约是否被违反;violate=1 表示有单位攻击自己的契约对象 */
void ks_contract_check_violation(ks_world_t *w, int idx, int attacker, int victim) {
    ks_contract_t *c = &w->contracts[idx];
    if (!c->active) return;
    int in_pair = (c->a == attacker && c->b == victim) ||
                  (c->a == victim && c->b == attacker);
    if (!in_pair) return;
    switch (c->kind) {
        case C_ALLIANCE: {
            /* 同盟契约:对契约内单位发起袭击→叛逆者,失去1令咒,始终暴露位置/令咒/魔力 */
            ks_unit_t *at = &w->units[attacker - 1];
            ks_log(w, "  ★ %s 违反[同盟契约]!成为[叛逆者]", at->name);
            if (at->cs > 0) {
                at->cs--;
                ks_log(w, "    失去1枚令咒!");
            }
            at->fate_known = 1;   /* 暴露位置(简化标记) */
            ks_log(w, "    暴露所处位置、令咒数量与自阵营魔力状态!");
            break;
        }
        case C_TRUCE: {
            /* 不战契约:违反者直到下一轮次结束无法行动且无法主动参与战斗 */
            ks_unit_t *at = &w->units[attacker - 1];
            ks_log(w, "  ★ %s 违反[不战契约]!受到剜心之痛", at->name);
            at->tired = 1;              /* 无法行动标记 */
            ks_log(w, "    直到下一轮次结束无法行动且无法参与战斗");
            break;
        }
        case C_FORCE: {
            /* 强制契约:违反者失去全部令咒+100魔力 */
            ks_unit_t *at = &w->units[attacker - 1];
            ks_log(w, "  ★ %s 违反[强制契约]!", at->name);
            at->cs = 0;
            ks_mp_pay(w, attacker, 100);
            ks_log(w, "    失去全部令咒,魔力-100(不足由从属代偿)");
            break;
        }
        case C_SLAVE:
            /* 奴役契约:纯主从关系,不惩罚(由立约人支配) */
            break;
        case C_HOLY:
        case C_MANA:
        case C_DUEL:
            /* 圣杯/魔力/决斗契约无袭击惩罚条款 */
            break;
        default:
            break;
    }
}

void ks_contract_break(ks_world_t *w, int idx) {
    if (idx < 0 || idx >= w->contract_count) return;
    w->contracts[idx].active = 0;
    ks_log(w, "契约破除:%s", w->contracts[idx].note);
}

int ks_is_friend(ks_world_t *w, int a, int b) {
    if (a < 1 || b < 1) return 0;
    const ks_unit_t *ua = &w->units[a - 1], *ub = &w->units[b - 1];
    if (ua->faction == ub->faction) return 1;
    /* 圣杯契约/同盟契约视为同阵营 */
    for (int i = 0; i < w->contract_count; i++) {
        ks_contract_t *c = &w->contracts[i];
        if (!c->active) continue;
        if ((c->a == a && c->b == b) || (c->a == b && c->b == a))
            if (c->kind == C_HOLY || c->kind == C_ALLIANCE || c->kind == C_SLAVE)
                return 1;
    }
    return 0;
}

int ks_unit_add_item(ks_world_t *w, int uid, int rid) { return ks_unit_add_res(w, uid, rid); }

void ks_world_print(ks_world_t *w) {
    ks_log(w, "+------- 空想圣杯模拟器 v1.0 -------+");
    ks_log(w, "| 轮次 %d / %s | 阵营 %d | 灵脉 %d |",
           w->round, w->day ? "昼" : "夜", w->unit_count / 2, w->leyline_count);
    ks_log(w, "+----------------------------------+");
    for (int i = 0; i < w->unit_count; i++) {
        ks_unit_t *u = &w->units[i];
        if (!u->alive) { ks_log(w, "  %s ✝ 退场", u->name); continue; }
        char tr[64] = "";
        for (int t = 0; t < 7; t++) {          /* 特性位:1<<0..1<<6 */
            if (u->traits & (1 << t)) { strcat(tr, ks_trait_name(1 << t)); strcat(tr, " "); }
        }
        ks_log(w, "  %s(Lv%d %s) 阵营[%s%s] 特性:%s", u->name, u->level,
               ks_utype_name(u->utype), ks_law_name(u->al_law), ks_moral_name(u->al_moral),
               tr[0] ? tr : "无");
        ks_log(w, "    筋力%d 耐久%d 敏捷%d 魔力%d 幸运%d 宝具%d | 魔力池 %d/%d FP%d 令咒%d",
               ks_unit_attr_value(w, u->id, A_STR), ks_unit_attr_value(w, u->id, A_END),
               ks_unit_attr_value(w, u->id, A_AGI), ks_unit_attr_value(w, u->id, A_MAG),
               ks_unit_attr_value(w, u->id, A_LUK), ks_unit_attr_value(w, u->id, A_NP),
               u->mp.cur, u->mp.cap, u->fp, u->cs);
    }
    for (int i = 0; i < w->leyline_count; i++) {
        ks_leyline_t *l = &w->leylines[i];
        ks_log(w, "  灵脉[%s] 魔力%d 人流量%d 主:%s 工房:%s%s", l->name, l->mana, l->flow,
               (l->owner > 0) ? w->units[l->owner - 1].name : "无主",
               l->has_workshop ? "有" : "无", l->has_bound ? " 固有结界" : "");
    }
}