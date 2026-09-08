/* ksg_battle.c — 空想圣杯引擎:战斗系统(战斗位/工序/结算链/胜率/战术/指令/决胜/撤退) */
#include "ksg.h"

/* ---------- 战术克制:强击>破袭>试探>扼守>强击 ---------- */

static int ks_tactic_counter(int a, int b) {
    /* 返回 1:a 克制 b;0:无;-1:b 克制 a */
    if (a == T_NONE || b == T_NONE || a == b) return 0;
    /* 环: 强击克破袭 破袭克试探 试探克扼守 扼守克强击 */
    static const int next[] = { 0, T_RAID, T_PROBE, T_HOLD, T_STRIKE };
    if (next[a] == b) return 1;
    if (next[b] == a) return -1;
    return 0;
}

/* ---------- 战斗位容量 ---------- */

static int ks_width_capacity(int width, int *main, int *support, int *servant, int *rear) {
    /* 规则书 3.2:宽度对应结构 */
    *main = 1; *support = 0; *servant = 0; *rear = 0;
    switch (width) {
        case 1: break;                       /* 主力 */
        case 2: *rear = 1; break;            /* 主力+支援 */
        case 3: *support = 1; *rear = 1; break;
        case 4: *support = 1; *servant = 1; *rear = 1; break;
        case 5: *support = 2; *servant = 1; *rear = 1; break;
        case 6: *support = 3; *servant = 1; *rear = 1; break;
        case 7: *support = 4; *servant = 1; *rear = 1; break;
        default: *support = width - 3; *rear = 1; break;
    }
    return *main + *support + *servant + *rear;
}

int ks_battle_unit_in_left(ks_world_t *w, int uid) {
    for (int i = 0; i < w->battle.left_n; i++)
        if (w->battle.left[i].uid == uid) return 1;
    for (int i = 0; i < w->battle.right_n; i++)
        if (w->battle.right[i].uid == uid) return 0;
    return 1;
}

static void ks_battle_clear(ks_world_t *w) {
    ks_battle_t *b = &w->battle;
    memset(b, 0, sizeof(*b));
    b->width = 4;
    for (int i = 0; i < w->unit_count; i++) {
        w->units[i].battle_side = 0;
        w->units[i].battle_slot = KS_SLOT_NONE;
        w->units[i].in_battle = 0;
    }
}

int ks_battle_start(ks_world_t *w, ks_battle_cfg_t *cfg) {
    ks_battle_t *b = &w->battle;
    if (b->active) { ks_log(w, "战斗已在进行中"); return 0; }
    ks_battle_clear(w);
    b->active = 1;
    b->width = cfg ? cfg->width : 4;
    b->day = cfg ? cfg->day : 1;
    b->leyline = cfg ? cfg->leyline : 1;
    b->phase = 1; /* 战斗开始时 */

    /* 找两方单位:左侧 1 阵营 vs 右侧 2 阵营 */
    int lc = 0, rc = 0;
    for (int i = 0; i < w->unit_count && (lc < KSG_COLUMN_MAX); i++) {
        ks_unit_t *u = &w->units[i];
        if (!u->alive || u->utype == KS_U_FAMILIAR) continue;
        if (u->faction == 1) {
            b->left[lc].uid = u->id;
            u->battle_side = 1;
            u->in_battle = 1;
            /* 简易分配:第一个是主力,其余支持 */
            u->battle_slot = (lc == 0) ? KS_SLOT_MAIN : KS_SLOT_SUPPORT;
            lc++;
        }
    }
    for (int i = 0; i < w->unit_count && rc < KSG_COLUMN_MAX; i++) {
        ks_unit_t *u = &w->units[i];
        if (!u->alive || u->utype == KS_U_FAMILIAR) continue;
        if (u->faction == 2) {
            b->right[rc].uid = u->id;
            u->battle_side = 2;
            u->in_battle = 1;
            u->battle_slot = (rc == 0) ? KS_SLOT_MAIN : KS_SLOT_SUPPORT;
            rc++;
        }
    }
    b->left_n = lc; b->right_n = rc;
    b->left_main = lc ? b->left[0].uid : 0;
    b->right_main = rc ? b->right[0].uid : 0;

    ks_log(w, "");
    ks_log(w, "════════ 战斗开始 ════════");
    ks_log(w, "战场:灵脉%d 宽度%d %s", b->leyline, b->width, b->day ? "[昼]" : "[夜]");
    ks_log(w, "左方:主力 %s(共%d体) | 右方:主力 %s(共%d体)",
           b->left_main ? w->units[b->left_main - 1].name : "-", lc,
           b->right_main ? w->units[b->right_main - 1].name : "-", rc);
    /* 4.1.3 魔力结算:移除高于上限的魔力 */
    for (int i = 0; i < w->unit_count; i++)
        if (w->units[i].in_battle) ks_mp_settle(w, w->units[i].id);

    /* 工房组件战场生效(规则:工房书 2.5 限定/支援组件) */
    for (int j = 0; j < w->workshop_count; j++) {
        ks_workshop_t *ws = &w->workshops[j];
        if (ws->leyline != b->leyline) continue;
        for (int c = 0; c < 8; c++) {
            int ci = ws->components[c];
            if (ci < 0) continue;
            if (ws->build_prog[c] < w->cmpts[ci].build) continue;  /* 未完成无效 */
            const char *cn = w->cmpts[ci].name;
            if (!strcmp(cn, "魔能重炮")) {
                /* 工房主所在阵营获得对敌 -40% 胜率惩罚 */
                if (w->units[ws->owner - 1].battle_side == 1) {
                    ks_log(w, "  🏰 工房[魔能重炮]!右方主力位 -40%%胜率惩罚");
                    ks_battle_add_win(w, 2, -40);
                } else if (w->units[ws->owner - 1].battle_side == 2) {
                    ks_log(w, "  🏰 工房[魔能重炮]!左方主力位 -40%%胜率惩罚");
                    ks_battle_add_win(w, 1, -40);
                }
            }
            if (!strcmp(cn, "黑厄深阱")) {
                ks_log(w, "  🏰 工房[黑厄深阱]!己方主力位 [抗性上升:+10%%]");
                ks_unit_gain_status(w, b->left_main, S_RESUP, 10, 0);
                ks_unit_gain_status(w, b->right_main, S_RESUP, 10, 0);
            }
            if (!strcmp(cn, "集束光标")) {
                ks_log(w, "  🏰 工房[集束光标]!自阵营从者 +40%%胜率补正(非主力减半)");
            }
            if (!strcmp(cn, "强能法阵")) {
                ks_log(w, "  🏰 工房[强能法阵]!工房主 +30魔力补给");
                ks_mp_gain(w, ws->owner, 30);
            }
            if (!strcmp(cn, "医疗装机")) {
                for (int i = 0; i < w->unit_count; i++)
                    if (w->units[i].in_battle)
                        ks_status_remove_layers(w, w->units[i].id, S_BURN, 3);
                ks_log(w, "  🏰 工房[医疗装机]!参战单位异常状态-3层");
            }
        }
    }
    return 1;
}

/* ---------- 战斗属性/基础胜率 ---------- */

int ks_battle_battery_check(ks_world_t *w) {
    ks_battle_t *b = &w->battle;
    if (!b->active || !b->main_attr[1] || !b->main_attr[2]) return 0;
    if (!b->rand_attr) return 0;

    /* 战斗属性:己方主要属性 + 随机属性(双方各自的主要属性可不同) */
    int lv = 0, rv = 0;
    /* 左方:主要属性=main_attr[1], 随机=rand_attr */
    for (int i = 0; i < b->left_n; i++) {
        ks_unit_t *u = &w->units[b->left[i].uid - 1];
        int v1 = ks_unit_attr_value(w, u->id, b->main_attr[1]);
        int v2 = ks_unit_attr_value(w, u->id, b->rand_attr);
        switch (u->battle_slot) {
            case KS_SLOT_MAIN: lv += v1 + v2; break;
            case KS_SLOT_SUPPORT: case KS_SLOT_SERVANT: lv += (v1 + v2) / 2; break;
            default: break; /* 支援位不计入战斗属性 */
        }
    }
    /* 右方:主要属性=main_attr[2], 随机=rand_attr */
    for (int i = 0; i < b->right_n; i++) {
        ks_unit_t *u = &w->units[b->right[i].uid - 1];
        int v1 = ks_unit_attr_value(w, u->id, b->main_attr[2]);
        int v2 = ks_unit_attr_value(w, u->id, b->rand_attr);
        switch (u->battle_slot) {
            case KS_SLOT_MAIN: rv += v1 + v2; break;
            case KS_SLOT_SUPPORT: case KS_SLOT_SERVANT: rv += (v1 + v2) / 2; break;
            default: break;
        }
    }
    /* 规则:每1点战斗属性=1%胜率;基础胜率仅由差值产生 */
    if (lv > rv) { b->left_base = lv - rv; b->right_base = 0; }
    else         { b->left_base = 0; b->right_base = rv - lv; }
    b->left_win = b->left_base;
    b->right_win = b->right_base;
    ks_verbose(w, "  [战斗属性] 左方总值%d 右方总值%d → 基础胜率 左%d%% 右%d%%",
               lv, rv, b->left_base, b->right_base);
    return 1;
}

void ks_battle_tactics(ks_world_t *w, int side, int tac) {
    ks_battle_t *b = &w->battle;
    if (!b->active) return;
    if (side < 1 || side > 2) return;
    if (tac < T_STRIKE || tac > T_HOLD) return;
    b->tactics[side] = tac;
    ks_log(w, "  %s 选择战术:%s", side == 1 ? "左方" : "右方", ks_tactic_name(tac));
    /* 克制立即结算(6.2) */
    int c = ks_tactic_counter(b->tactics[1], b->tactics[2]);
    if (c > 0) {
        ks_log(w, "  [战术克制] 左方%s 克制 右方%s! 左方主力位 +20%%胜率", 
               ks_tactic_name(b->tactics[1]), ks_tactic_name(b->tactics[2]));
        b->left_win += 20;
    } else if (c < 0) {
        ks_log(w, "  [战术克制] 右方%s 克制 左方%s! 右方主力位 +20%%胜率",
               ks_tactic_name(b->tactics[2]), ks_tactic_name(b->tactics[1]));
        b->right_win += 20;
    }
}

void ks_battle_attr_pick(ks_world_t *w, int side, int attr) {
    ks_battle_t *b = &w->battle;
    if (!b->active) return;
    if (attr < A_STR || attr > A_NP) return;
    b->main_attr[side] = attr;
    ks_log(w, "  %s ▸ 主要属性:%s(%d)", side == 1 ? "左方" : "右方",
           ks_attr_name(attr), attr);
}

void ks_battle_roll_rand_attr(ks_world_t *w) {
    ks_battle_t *b = &w->battle;
    if (!b->active) return;
    /* 规则:由GM随机抽取一项随机属性(六项中) */
    int attr = A_STR + (ks_roll(w) % 6);
    b->rand_attr = attr;
    ks_log(w, "  ✦ 随机属性:%s(%d)", ks_attr_name(attr), attr);
}

void ks_battle_add_win(ks_world_t *w, int side, int n) {
    ks_battle_t *b = &w->battle;
    if (!b->active) return;
    if (side == 1) b->left_win += n;
    else if (side == 2) b->right_win += n;
    ks_log(w, "  [胜率修正] %s %+d%%(现 %d%% vs %d%%)",
           side == 1 ? "左方" : "右方", n, b->left_win, b->right_win);
}

void ks_battle_order(ks_world_t *w, int side, int order) {
    ks_battle_t *b = &w->battle;
    if (!b->active) return;
    ks_unit_t *u = &w->units[(side == 1 ? b->left_main : b->right_main) - 1];
    ks_unit_t *e = &w->units[(side == 1 ? b->right_main : b->left_main) - 1];
    switch (order) {
        case O_CHARGE: {
            /* 规则 6.1.1:若目标不在战斗位→强制进入;若在战斗位→属性差+50%判定 */
            ks_log(w, "  ⚔ %s 宣言[冲锋]!", u->name);
            if (!e->in_battle) {
                /* 目标不在战斗位:若敌方战斗位有空位则强制进入 */
                int en = (side == 1) ? 2 : 1;
                int cap = ks_width_capacity(b->width, &(int){0}, &(int){0}, &(int){0}, &(int){0});
                int n = (en == 1) ? b->left_n : b->right_n;
                /* 宽度计算(简化):每方可容纳 = 宽度(不含支援位?此处简化为宽度) */
                if (n < b->width) {
                    ks_log(w, "     · 目标未在战斗位,强制进入敌方战斗位!");
                    e->in_battle = 1;
                    e->battle_side = (side == 1) ? 2 : 1;
                    e->battle_slot = KS_SLOT_SUPPORT;
                    /* 加入敌方战斗列表 */
                    if (en == 1) b->left[b->left_n++].uid = e->id;
                    else b->right[b->right_n++].uid = e->id;
                } else {
                    ks_log(w, "     · 敌方战斗位已满,冲锋无效");
                }
                return;
            }
            /* 冲锋判定:从筋力/耐久/敏捷/幸运随机选一项,属性差+50% */
            int at = A_STR + (ks_roll(w) % 4);
            int diff = ks_unit_attr_value(w, u->id, at) - ks_unit_attr_value(w, e->id, at);
            int rate = ks_clamp(50 + diff, 0, 100);
            ks_log(w, "     · 判定属性:%s 差=%d 成功率=%d%%", ks_attr_name(at), diff, rate);
            if (ks_roll_bool(w, rate)) {
                ks_log(w, "     · 冲锋成功!+20%%胜率补正");
                ks_battle_add_win(w, side, 20);
            } else {
                ks_log(w, "     · 冲锋失败!-10%%底限穿透");
                if (side == 1) b->left_floor -= 10;
                else b->right_floor -= 10;
            }
            break;
        }
        case O_PURSUE:
            /* 规则 6.1.2:敌方战斗位单位在战斗工序内宣言撤退时,己方主力位宣言[追击] */
            ks_log(w, "  → %s 宣言[追击]! 目标%s撤退消耗FP+1", u->name, e->name);
            /* 记录追击令目标 FP+1(在撤退结算时执行) */
            if (side == 1) b->right_retreat += 1;
            else b->left_retreat += 1;
            /* 目标可撤销撤退宣言(跳过:演示自动保持) */
            break;
        case O_COVER:
            /* 规则 6.1.3:替代非主力位成为冲锋/追击目标,-20%胜率 */
            ks_log(w, "  🛡 %s 宣言[掩护]! 替代目标承受冲锋/追击,-20%%胜率", u->name);
            ks_battle_add_win(w, side, -20);
            break;
        case O_DUEL:
            /* 规则 6.1.4:最终工序,+20%胜率,最终工序内无法撤退 */
            ks_log(w, "  ☠ %s 宣言[死斗]! +20%%胜率, 最终工序内无法撤退", u->name);
            ks_battle_add_win(w, side, 20);
            break;
        default: break;
    }
}

/* 撤退:按规则 7.1 消耗FP(初始0/主要1/最终2/结束时3,被追击+1),撤退后进入游荡 */
void ks_battle_retreat(ks_world_t *w, int side) {
    ks_battle_t *b = &w->battle;
    if (!b->active) return;
    int main = (side == 1) ? b->left_main : b->right_main;
    if (!main) return;
    ks_unit_t *u = &w->units[main - 1];
    /* 基础FP消耗按时点 */
    int cost = 0;
    switch (b->phase) {
        case 2: cost = 0; break;   /* 初始工序 0FP */
        case 3: cost = 1; break;   /* 主要工序 1FP */
        case 4: cost = 2; break;   /* 最终工序 2FP */
        case 5: cost = 3; break;   /* 战斗结束时 3FP */
        default: cost = 1; break;
    }
    /* 追击+1 */
    int pursue = (side == 1) ? b->left_retreat : b->right_retreat;
    cost += pursue;
    int avail = u->fp + u->tp_fp;   /* 可用FP含临时FP(令咒) */
    if (avail < cost) {
        ks_log(w, "  ✖ %s 撤退失败!FP不足(需%d,持有%d)", u->name, cost, avail);
        return;
    }
    /* 支付:先临时FP后保有FP */
    int pay = cost;
    if (u->tp_fp >= pay) { u->tp_fp -= pay; pay = 0; }
    else { pay -= u->tp_fp; u->tp_fp = 0; u->fp -= pay; }
    ks_log(w, "  ↩ %s 宣言撤退!消耗FP%d(剩余%d),进入游荡状态", u->name, cost, u->fp);
    /* 从战斗位移除 */
    int n = (side == 1) ? b->left_n : b->right_n;
    for (int i = 0; i < n; i++) {
        int uid = (side == 1) ? b->left[i].uid : b->right[i].uid;
        if (uid == main) {
            if (side == 1) {
                for (int j = i; j < b->left_n - 1; j++) b->left[j] = b->left[j + 1];
                b->left_n--;
            } else {
                for (int j = i; j < b->right_n - 1; j++) b->right[j] = b->right[j + 1];
                b->right_n--;
            }
            break;
        }
    }
    u->in_battle = 0;
    u->battle_side = 0;
    u->battle_slot = KS_SLOT_NONE;
    u->roaming = 1;
    (void)n;
    /* 若主力位撤退:按规则 3.1.4 辅助→仆役→支援 顺序补位 */
    if (side == 1 && b->left_n > 0) { b->left_main = b->left[0].uid; w->units[b->left_main - 1].battle_slot = KS_SLOT_MAIN; }
    if (side == 2 && b->right_n > 0) { b->right_main = b->right[0].uid; w->units[b->right_main - 1].battle_slot = KS_SLOT_MAIN; }
    if ((side == 1 && b->left_n == 0) || (side == 2 && b->right_n == 0)) {
        ks_log(w, "  □ 一方战斗位已空,战斗立即结束");
        if (side == 1) { b->right_ok = 1; b->left_ok = 0; }
        else { b->left_ok = 1; b->right_ok = 0; }
    }
}

/* ---------- 技能/宝具解放在战斗中的结算 ---------- */

/* 是否可发动:检查时机/回转/魔耗 */
static int res_castable_at(ks_world_t *w, ks_res_t *r, int when)
{
    if (r->kind != KS_R_SKILL && r->kind != KS_R_PHANTASM) return 0;
    if (r->when == KS_WHEN_PASSIVE) return 0;          /* 常驻自动生效 */
    /* 回转判定:可发动类能力必须在冷却完毕后才能被提交 */
    if (r->recast > 0 && r->cur_recast < r->recast) return 0;
    if (r->when == KS_WHEN_ACT && when != KS_TIME_DAY_ACT) return 0;
    if (r->when == KS_WHEN_BATTLE_START && when != KS_TIME_BATTLE_START) return 0;
    if (r->when == KS_WHEN_PROC && when != KS_TIME_PROC_OPEN && when != KS_TIME_PROC_MAIN
        && when != KS_TIME_PROC_FINAL) return 0;
    if (r->when == KS_WHEN_ANY) return 1;
    return 1;
}

/* 列出单位当前时机可发动的能力,返回数量 */
int ks_battle_unit_castable(ks_world_t *w, int uid, int when, int *rids, int maxn)
{
    ks_unit_t *u = &w->units[uid - 1];
    int n = 0;
    for (int i = 0; i < u->skill_count && n < maxn; i++) {
        int rid = u->skill_ids[i];
        ks_res_t *r = &w->res[rid - 1];
        if (res_castable_at(w, r, when)) rids[n++] = rid;
    }
    for (int i = 0; i < u->phantasm_count && n < maxn; i++) {
        int rid = u->phantasm_ids[i];
        ks_res_t *r = &w->res[rid - 1];
        if (res_castable_at(w, r, when)) rids[n++] = rid;
    }
    return n;
}

void ks_battle_cast_skill(ks_world_t *w, int uid, int rid) {
    ks_res_t *r = &w->res[rid - 1];
    ks_unit_t *u = &w->units[uid - 1];
    if (!r->id) return;
    if (r->kind == KS_R_PHANTASM && u->phantasm_count <= 0) return;
    if (!ks_res_use(w, uid, rid)) {
        ks_log(w, "  %s 无法发动 %s(回转/魔耗)", u->name, r->name);
        return;
    }
    ks_log(w, "  ✦ %s 发动 %s[%s] 魔耗%d", u->name, r->name, ks_rank_name(r->rank), r->cost);
    /* 结算效果链(统一结算引擎:目标解析/条件/判定/上限) */
    ks_apply_res_effects(w, uid, rid, 0);
    ks_recast_tick_proc(w, uid); /* 工序+1回转(结算后) */
}

/* ---------- 结算链引擎(规则书 5.2) ---------- */

/* 等级链数值:EX=7 A=6 B=5 C=4 D=3 E=2 -=1 */
static int ks_rank_order(int r) {
    switch (r) {
        case KS_RANK_EX: return 7;
        case KS_RANK_A: return 6;
        case KS_RANK_B: return 5;
        case KS_RANK_C: return 4;
        case KS_RANK_D: return 3;
        case KS_RANK_E: return 2;
        default: return 1;
    }
}

/* 时机链:常驻>指定工序>随时 */
static int ks_when_order(int w) {
    switch (w) {
        case KS_WHEN_PASSIVE: return 3;
        case KS_WHEN_PROC: return 2;
        case KS_WHEN_BATTLE_START: return 2;
        case KS_WHEN_ACT: return 1;
        default: return 1;
    }
}

/* 一次"提交"的条目 */
typedef struct {
    int uid;
    int rid;
    ks_res_t *r;
} ks_commit_t;

/* 按结算链排序:能力链(宝具>技能>礼装) → 等级链(EX>A>...) → 时机链 */
static int ks_commit_cmp(const void *pa, const void *pb) {
    const ks_commit_t *a = (const ks_commit_t *)pa;
    const ks_commit_t *b = (const ks_commit_t *)pb;
    /* 能力链 */
    int ka = (a->r->kind == KS_R_PHANTASM) ? 3 : (a->r->kind == KS_R_SKILL) ? 2 : 1;
    int kb = (b->r->kind == KS_R_PHANTASM) ? 3 : (b->r->kind == KS_R_SKILL) ? 2 : 1;
    if (ka != kb) return kb - ka;
    /* 类型链(技能:职阶>天赋>技艺>祝福>荣冠>兵器>魔术;宝具:对人魔剑>对人>对军>对城>结界>对界) */
    int ta = a->r->type, tb = b->r->type;
    if (ta != tb) return ta - tb;
    /* 等级链 */
    int ra = ks_rank_order(a->r->rank), rb = ks_rank_order(b->r->rank);
    if (ra != rb) return rb - ra;
    /* 时机链 */
    int wa = ks_when_order(a->r->when), wb = ks_when_order(b->r->when);
    return wb - wa;
}

/* 提交列表并一次性按结算链结算 */
void ks_battle_chain_resolve(ks_world_t *w, int *uids, int *rids, int n) {
    if (n <= 0) return;
    ks_commit_t cm[64];
    int m = 0;
    for (int i = 0; i < n && m < 64; i++) {
        ks_res_t *r = &w->res[rids[i] - 1];
        if (!r->id) continue;
        cm[m].uid = uids[i];
        cm[m].rid = rids[i];
        cm[m].r = r;
        m++;
    }
    qsort(cm, m, sizeof(ks_commit_t), ks_commit_cmp);
    ks_log(w, "  ── 结算链: 能力(宝具→技能→礼装) → 类型 → 等级(EX→A→…) → 时机 ──");
    for (int i = 0; i < m; i++) {
        ks_log(w, "  [结算顺序%d] %s(%s %s)", i + 1,
               w->units[cm[i].uid - 1].name, cm[i].r->name, ks_rank_name(cm[i].r->rank));
        ks_battle_cast_skill(w, cm[i].uid, cm[i].rid);
    }
}

/* ---------- 蓄力/爆发特效(规则书附录三) ---------- */

/* 记录单位当前的蓄力条目 */
typedef struct {
    int uid;
    int rid;
    int proc;      /* 需要经过的工序数 */
    int used;      /* 已用 */
} ks_charge_t;

#define KS_CHARGE_MAX 16
static ks_charge_t g_charges[KS_CHARGE_MAX];
static int g_charges_n = 0;

void ks_charge_clear(void) { g_charges_n = 0; }

/* 宣言[蓄力]:该能力延迟 proc 个工序后自动结算 */
void ks_charge_begin(ks_world_t *w, int uid, int rid, int proc) {
    if (g_charges_n >= KS_CHARGE_MAX) return;
    ks_res_t *r = &w->res[rid - 1];
    ks_log(w, "  ⏳ %s 宣言[蓄力]%s(经过%d个工序后生效)",
           w->units[uid - 1].name, r->name, proc);
    g_charges[g_charges_n].uid = uid;
    g_charges[g_charges_n].rid = rid;
    g_charges[g_charges_n].proc = proc;
    g_charges[g_charges_n].used = 0;
    g_charges_n++;
}

/* 每工序结束:蓄力计数推进,满足时自动结算 */
void ks_charge_tick(ks_world_t *w) {
    for (int i = 0; i < g_charges_n; i++) {
        ks_charge_t *c = &g_charges[i];
        if (c->used) continue;
        ks_res_t *r = &w->res[c->rid - 1];
        if (!r->id) continue;
        if (c->proc <= 0) continue;
        c->used++;
        if (c->used >= c->proc) {
            ks_log(w, "  ── [蓄力完成] %s 自动解放 %s! ──",
                   w->units[c->uid - 1].name, r->name);
            c->used = 1; /* 标记已结算 */
            ks_battle_cast_skill(w, c->uid, c->rid);
        }
    }
}

/* 结算一个资源(支持[蓄力]/[爆发]语义) */
void ks_battle_cast_skill_ext(ks_world_t *w, int uid, int rid, int has_charge, int has_burst) {
    ks_res_t *r = &w->res[rid - 1];
    if (has_charge) {
        ks_charge_begin(w, uid, rid, 1);   /* 经过1个工序 */
        return;
    }
    if (has_burst) {
        /* [爆发]:额外支付一份发动条件(魔耗)后效果再次生效一次 */
        ks_unit_t *u = &w->units[uid - 1];
        if (u->mp.cur >= r->cost) {
            ks_log(w, "  ✦✦ %s 宣言[爆发]%s! 额外支付魔耗%d,效果再次生效",
                   u->name, r->name, r->cost);
            ks_mp_pay(w, uid, r->cost);
            ks_battle_cast_skill(w, uid, rid);
            /* 爆发后已结算一次,再结算一次原文效果 */
            ks_apply_res_effects(w, uid, rid, 0);
            uid = uid; /* 无操作 */
        } else {
            ks_log(w, "  ✖ %s [爆发]失败:魔力不足", u->name);
        }
        return;
    }
    ks_battle_cast_skill(w, uid, rid);
}

/* ---------- 最终胜率/决胜检定 ---------- */

int ks_battle_effective(ks_world_t *w) {
    ks_battle_t *b = &w->battle;
    if (!b->active) return 0;
    /* 底限胜率:最终胜率的下限(取较大值) */
    int lw = ks_clamp(b->left_win, 0, 100);
    int rw = ks_clamp(b->right_win, 0, 100);
    if (b->left_floor > 0 && lw < b->left_floor) lw = b->left_floor;
    if (b->right_floor > 0 && rw < b->right_floor) rw = b->right_floor;
    /* 最终胜率直接修正 */
    lw = ks_clamp(lw + b->left_final_win, 0, 100);
    rw = ks_clamp(rw + b->right_final_win, 0, 100);
    b->left_win = lw;
    b->right_win = rw;

    /* 按规则书 6.3.1:最终胜率总值按100计,劣势方按其最终胜率占用1~100的区间 */
    ks_log(w, "  【最终胜率】左方 %d%% : 右方 %d%%", lw, rw);
    int r = ks_roll(w);
    int l_win;
    if (lw == rw) l_win = (r <= 50);
    else if (lw > rw) l_win = (r > rw ? 1 : 0);   /* 左优势:掷出超过右方区间即左胜 */
    else l_win = (r > lw ? 0 : 1);                /* 右优势:掷出不超过左方区间即左胜 */
    ks_log(w, "  【决胜检定】骰 %d → %s", r, l_win ? "左方胜!" : "右方胜!");
    int lw2 = ks_clamp(b->left_win, 0, 100);
    int rw2 = ks_clamp(b->right_win, 0, 100);
    b->left_ok = l_win;
    b->right_ok = !l_win;
    return l_win;
}

/* ---------- 撤退 ---------- */

void ks_battle_end(ks_world_t *w) {
    ks_battle_t *b = &w->battle;
    if (!b->active) return;
    ks_log(w, "════════ 战斗结束 ════════");
    /* 结算等级魔耗 */
    for (int i = 0; i < w->unit_count; i++) {
        ks_unit_t *u = &w->units[i];
        if (!u->in_battle || !u->alive) continue;
        int cost = u->level / 2;
        ks_mp_pay(w, u->id, cost);
        ks_mp_settle(w, u->id);
        u->in_battle = 0;
        u->battle_side = 0;
        u->battle_slot = KS_SLOT_NONE;
    }
    /* 俘虏:战败方玩家单位成为胜方主力位的俘虏;召唤物/人偶直接摧毁(词典) */
    if (b->left_ok) {
        for (int i = 0; i < b->right_n; i++) {
            ks_unit_t *u = &w->units[b->right[i].uid - 1];
            if (u->utype == KS_U_SUMMON || u->utype == KS_U_DOLL) {
                ks_log(w, "  %s(召唤物) 战败,被摧毁", u->name);
                u->alive = 0;
                continue;
            }
            ks_log(w, "  %s 成为俘虏(归左方主力位 %s)",
                   u->name, w->units[b->left_main - 1].name);
            u->roaming = 1;
            u->alive = 1; /* 俘虏存活 */
            u->in_battle = 0;
        }
    } else if (b->right_ok) {
        for (int i = 0; i < b->left_n; i++) {
            ks_unit_t *u = &w->units[b->left[i].uid - 1];
            if (u->utype == KS_U_SUMMON || u->utype == KS_U_DOLL) {
                ks_log(w, "  %s(召唤物) 战败,被摧毁", u->name);
                u->alive = 0;
                continue;
            }
            ks_log(w, "  %s 成为俘虏(归右方主力位 %s)",
                   u->name, w->units[b->right_main - 1].name);
            u->roaming = 1;
            u->in_battle = 0;
        }
    } else {
        ks_log(w, "  双方未分胜负(无FP消耗/伤亡)");
    }
    memset(b, 0, sizeof(*b));
}

/* ---------- 战斗推进(push one step) ---------- */

void ks_battle_tick(ks_world_t *w) {
    ks_battle_t *b = &w->battle;
    if (!b->active) return;
    switch (b->phase) {
        case 1: /* 战斗开始时:展露/指令/战术 */
            ks_log(w, " — 战斗开始时 —");
            if (!b->tactics[1]) ks_battle_tactics(w, 1, T_STRIKE + (ks_roll(w) % 4));
            if (!b->tactics[2]) ks_battle_tactics(w, 2, T_STRIKE + (ks_roll(w) % 4));
            b->phase = 2;
            break;
        case 2: /* 初始工序:选属性 */
            ks_log(w, " — 初始工序 —");
            for (int i = 0; i < w->unit_count; i++)
                if (w->units[i].in_battle) ks_status_proc_start_tick(w, w->units[i].id);
            if (!b->main_attr[1]) ks_battle_attr_pick(w, 1, A_STR + (ks_roll(w) % 6));
            if (!b->main_attr[2]) ks_battle_attr_pick(w, 2, A_STR + (ks_roll(w) % 6));
            if (!b->rand_attr) ks_battle_roll_rand_attr(w);
            ks_battle_battery_check(w);
            b->phase = 3;
            break;
        case 3: /* 主要工序 */
            ks_log(w, " — 主要工序 —");
            {
                /* 双方各发动技能/宝具:选择可用的进攻类宝具(排除[反击]防御类) */
                int sides[] = { 1, 2 };
                for (int s = 0; s < 2; s++) {
                    int side = sides[s];
                    int mid = (side == 1) ? b->left_main : b->right_main;
                    ks_unit_t *u = &w->units[mid - 1];
                    /* 找第一个可用、非[反击]的宝具 */
                    int found = 0;
                    for (int i = 0; i < u->phantasm_count && !found; i++) {
                        ks_res_t *r = &w->res[u->phantasm_ids[i] - 1];
                        if (!(r->feat & KS_F_COUNTER) && r->cur_recast >= r->recast)
                            found = u->phantasm_ids[i];
                    }
                    if (found) ks_battle_cast_skill(w, mid, found);
                    else ks_log(w, "  %s 无可用宝具,进行普通攻击", u->name);
                }
                /* 指令:冲锋 */
                if (ks_roll(w) <= 40) ks_battle_order(w, 1, O_CHARGE);
                if (ks_roll(w) <= 30) ks_battle_order(w, 2, O_CHARGE);
                for (int i = 0; i < w->unit_count; i++)
                    if (w->units[i].in_battle) { ks_status_proc_tick(w, w->units[i].id); ks_recast_tick_proc(w, w->units[i].id); }
                ks_charge_tick(w);   /* 蓄力推进 */
                /* 主要工序开始时:状态判定(中毒/晕眩等) */
                for (int i = 0; i < w->unit_count; i++)
                    if (w->units[i].in_battle) ks_status_proc_start_tick(w, w->units[i].id);
                b->phase = 4;
                break;
            }
        case 4: /* 最终工序 */
            ks_log(w, " — 最终工序 —");
            for (int i = 0; i < w->unit_count; i++)
                if (w->units[i].in_battle) ks_status_proc_start_tick(w, w->units[i].id);
            if (ks_roll(w) <= 20) ks_battle_order(w, 1, O_DUEL);
            if (ks_roll(w) <= 15) ks_battle_order(w, 2, O_DUEL);
            /* 主力位各自解放本场首个可用的[决战]宝具(带蓄力/大消耗,排除[反击]类) */
            for (int s = 1; s <= 2; s++) {
                int mid = (s == 1) ? b->left_main : b->right_main;
                ks_unit_t *u = &w->units[mid - 1];
                int best = 0, best_cost = 0;
                for (int i = 0; i < u->phantasm_count; i++) {
                    ks_res_t *r = &w->res[u->phantasm_ids[i] - 1];
                    int ok = (r->kind == KS_R_PHANTASM && r->when != KS_WHEN_ACT &&
                              !(r->feat & KS_F_COUNTER) &&
                              r->recast > 0 && r->cur_recast >= r->recast);
                    if (ok && r->cost > best_cost) { best = u->phantasm_ids[i]; best_cost = r->cost; }
                }
                if (best) ks_battle_cast_skill(w, mid, best);
            }
            b->phase = 5;
            break;
        case 5: /* 结束时 */
            ks_battle_effective(w);
            ks_battle_end(w);
            break;
        default:
            break;
    }
}

/* ---------- 打印战斗计算表 ---------- */

void ks_battle_print(ks_world_t *w) {
    ks_battle_t *b = &w->battle;
    if (!b->active) { ks_log(w, "(无进行中的战斗)"); return; }
    ks_log(w, "┌──── 战斗计算表(%s 灵脉%d 宽度%d) ────┐", b->day ? "昼" : "夜", b->leyline, b->width);
    ks_log(w, "│ 左方(阵营1) 战术:%s 主要:%s 随机:%s", 
           ks_tactic_name(b->tactics[1]),
           b->main_attr[1] ? ks_attr_name(b->main_attr[1]) : "-",
           b->rand_attr ? ks_attr_name(b->rand_attr) : "-");
    for (int i = 0; i < b->left_n; i++) {
        ks_unit_t *u = &w->units[b->left[i].uid - 1];
        ks_log(w, "│   %s [%s] 筋%d 耐%d 敏%d 魔%d 幸%d 宝%d",
               u->name, ks_slot_name(u->battle_slot),
               ks_unit_attr_value(w, u->id, A_STR), ks_unit_attr_value(w, u->id, A_END),
               ks_unit_attr_value(w, u->id, A_AGI), ks_unit_attr_value(w, u->id, A_MAG),
               ks_unit_attr_value(w, u->id, A_LUK), ks_unit_attr_value(w, u->id, A_NP));
    }
    ks_log(w, "│ 右方(阵营2) 战术:%s 主要:%s", 
           ks_tactic_name(b->tactics[2]),
           b->main_attr[2] ? ks_attr_name(b->main_attr[2]) : "-");
    for (int i = 0; i < b->right_n; i++) {
        ks_unit_t *u = &w->units[b->right[i].uid - 1];
        ks_log(w, "│   %s [%s] 筋%d 耐%d 敏%d 魔%d 幸%d 宝%d",
               u->name, ks_slot_name(u->battle_slot),
               ks_unit_attr_value(w, u->id, A_STR), ks_unit_attr_value(w, u->id, A_END),
               ks_unit_attr_value(w, u->id, A_AGI), ks_unit_attr_value(w, u->id, A_MAG),
               ks_unit_attr_value(w, u->id, A_LUK), ks_unit_attr_value(w, u->id, A_NP));
    }
    ks_log(w, "│ 胜率:左 %d%% | 右 %d%%", b->left_win, b->right_win);
    ks_log(w, "└────────────────────────────────────┘");
}

/* ---------- 自动沙盘演示 ---------- */

int ks_battle_demo(ks_world_t *w) {
    ks_log(w, "");
    ks_log(w, "========== 沙盘演示:谕天之剑(左) × 灼岩旋律(右) ==========");
    /* 建卡 */
    ks_data_make_unit_sheet(w, 1, "saber");
    ks_data_make_unit_sheet(w, 2, "master");
    ks_data_make_unit_sheet(w, 4, "lancer");
    ks_data_make_unit_sheet(w, 5, "master");
    /* 灵脉 */
    w->leylines[0].owner = 1;
    w->leylines[1].owner = 4;

    ks_log(w, ""); ks_log(w, "【第一回合:昼】");
    ks_log(w, "行动阶段开始…");
    ks_action_do(w, 1, KS_ACT_INTERVENE, "2");  /* Saber 干涉灵脉2 */
    ks_action_do(w, 4, KS_ACT_INTERVENE, "1");  /* Lancer 干涉灵脉1 */
    ks_action_do(w, 2, KS_ACT_CRAFT, NULL);
    ks_action_do(w, 5, KS_ACT_RECON, "broad");
    ks_log(w, "干涉结算:双方同时抵达目标灵脉,进入事前遭遇?");

    /* 战斗 */
    ks_battle_start(w, &(ks_battle_cfg_t){ .width = 4, .day = 1, .leyline = 1 });
    for (int i = 0; i < 10; i++) {
        ks_battle_tick(w);
        if (!w->battle.active) break;
    }
    ks_battle_print(w);

    /* 轮次结算 */
    ks_world_round_end(w);
    ks_log(w, "");
    ks_log(w, "【第一回合:夜】行动:");
    ks_action_do(w, 1, KS_ACT_SOULFEED, "shelter");
    ks_action_do(w, 4, KS_ACT_MOVEMENT, "2");
    ks_world_round_end(w);

    /* 第二回合:结算战局 */
    ks_log(w, ""); ks_log(w, "【第二回合:昼】若前次战斗未分胜负则再战");
    if (!w->battle.active) {
        ks_battle_start(w, &(ks_battle_cfg_t){ .width = 4, .day = 1, .leyline = 1 });
        for (int i = 0; i < 10; i++) {
            ks_battle_tick(w);
            if (!w->battle.active) break;
        }
    }
    ks_world_print(w);
    ks_log(w, "");
    ks_log(w, "========== 沙盘演示结束 ==========");
    return 0;
}