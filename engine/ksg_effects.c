/* ksg_effects.c — 空想圣杯引擎:效果结算引擎
 * 依据《空想圣杯规则书》附录三词典、第3-5章,实现:
 *  - 效果条件检查(位置/特性/状态/等级/时段/情报等)
 *  - 目标解析(敌方全体/己方全体/自身/指定)
 *  - 判定(基础成功率+属性差/幸运减半/负面判定抗性)
 *  - 各类效果的完整结算(属性/胜率/最终胜率/底限/魔力/状态/爆燃/激荡/毒发/
 *    即死/轰击/回转/FP/必中/无敌贯通/回避/保护/无敌/反击)
 */
#include "ksg.h"

/* ---------------- 条件名称 ---------------- */

const char *ks_cond_name(int c) {
    switch (c) {
        case KC_NONE: return "无条件";
        case KC_OWN_MAIN: return "[主力位]";
        case KC_OWN_SUPPORT: return "[辅助位]";
        case KC_OWN_REAR: return "[支援位]";
        case KC_TARGET_TRAIT: return "目标持有特性";
        case KC_TARGET_NOT_TRAIT: return "目标不持有特性";
        case KC_TARGET_STATUS_EQ: return "目标持有状态";
        case KC_TARGET_STATUS_GE: return "目标状态层数≥N";
        case KC_STATUS_NOT: return "目标不持有状态";
        case KC_FRIEND_STATUS: return "己方其他单位持有状态";
        case KC_TARGET_LUCK_GE: return "目标幸运≥40";
        case KC_TARGET_LEVEL_GE: return "目标等级≥N";
        case KC_LEVEL_DIFF: return "等级差(自身-目标)";
        case KC_DAY: return "昼回合";
        case KC_NIGHT: return "夜回合";
        case KC_FIRST_ENCOUNTER: return "初次与敌方主力交战";
        case KC_ENEMY_MAIN_MASTER: return "敌方主力位为御主";
        case KC_SELF_HP: return "自身属性≥N";
        case KC_SELF_MP: return "自身魔力≥N";
        case KC_FRIEND_IN_BATTLE: return "己方战斗位有其他单位";
        case KC_ENEMY_IN_BATTLE: return "敌方战斗位有其他单位";
        case KC_SELF_TRAIT: return "自身持有特性";
        case KC_ENEMY_TACTIC: return "敌方战术==N";
        case KC_SELF_TACTIC: return "己方战术==N";
        case KC_TACTIC_NOT_PAIRED: return "己方战术未被克制";
        case KC_ENEMY_IS_SERVANT: return "敌方主力位是从者";
        case KC_ENEMY_IS_SUMMON: return "敌方单位是召唤物";
        case KC_SELF_IS_MASTER: return "自身是御主";
        case KC_SELF_IS_SERVANT: return "自身是从者";
        case KC_HAS_CS: return "自阵营持有令咒";
        case KC_MP_UNDER: return "自身魔力不足";
        case KC_TARGET_AGI_LT: return "目标敏捷<N";
        case KC_TARGET_AGI_GE: return "目标敏捷≥N";
    }
    return "条件";
}

/* ---------------- 条件检查 ---------------- */

static int ks_trait_holds(ks_unit_t *u, int trait) { return (u->traits & trait) ? 1 : 0; }

static int ks_unit_status_layers(ks_world_t *w, int uid, int kind) {
    ks_unit_t *u = &w->units[uid - 1];
    for (int i = 0; i < KSG_STATUS_MAX; i++)
        if (u->status[i].kind == kind) return u->status[i].layers;
    return 0;
}

/* 敌方战斗位主帅(与自身对位) */
static int ks_enemy_main(ks_world_t *w, int uid) {
    ks_battle_t *b = &w->battle;
    if (!b->active) return 0;
    ks_unit_t *u = &w->units[uid - 1];
    int my_side = u->battle_side;
    if (my_side == 1) return b->right_main;
    if (my_side == 2) return b->left_main;
    return 0;
}

/* 检查一条效果行的触发条件;cond_arg 是参数 */
static int ks_cond_check(ks_world_t *w, int uid, int tgt, const ks_effectline_t *el) {
    ks_unit_t *u = &w->units[uid - 1];
    ks_unit_t *t = (tgt > 0) ? &w->units[tgt - 1] : NULL;
    ks_battle_t *b = &w->battle;
    int c = el->cond;
    switch (c) {
        case KC_NONE: return 1;
        case KC_OWN_MAIN: return (u->battle_slot == KS_SLOT_MAIN);
        case KC_OWN_SUPPORT: return (u->battle_slot == KS_SLOT_SUPPORT);
        case KC_OWN_REAR: return (u->battle_slot == KS_SLOT_REAR);
        case KC_TARGET_TRAIT: return t ? ks_trait_holds(t, el->cond_arg) : 0;
        case KC_TARGET_NOT_TRAIT: return t ? !ks_trait_holds(t, el->cond_arg) : 0;
        case KC_TARGET_STATUS_EQ: return t ? ks_unit_status_layers(w, tgt, el->cond_arg) > 0 : 0;
        case KC_TARGET_STATUS_GE: return t ? ks_unit_status_layers(w, tgt, el->cond_arg) >= el->cond_arg2 : 0;
        case KC_STATUS_NOT: return t ? ks_unit_status_layers(w, tgt, el->cond_arg) == 0 : 0;
        case KC_FRIEND_STATUS: {
            if (!b->active) return 0;
            int side = (u->battle_side == 1) ? 1 : 2;
            int n = (side == 1) ? b->left_n : b->right_n;
            for (int i = 0; i < n; i++) {
                int fid = (side == 1) ? b->left[i].uid : b->right[i].uid;
                if (fid == uid) continue;
                if (ks_unit_status_layers(w, fid, el->cond_arg) > 0) return 1;
            }
            return 0;
        }
        case KC_TARGET_LUCK_GE:
            return t ? (ks_unit_attr_value(w, tgt, A_LUK) >= (el->cond_arg ? el->cond_arg : 40)) : 0;
        case KC_TARGET_LEVEL_GE: return t ? (t->level >= el->cond_arg) : 0;
        case KC_LEVEL_DIFF:
            return t ? ((u->level - t->level) >= el->cond_arg) : 0;
        case KC_DAY: return w->day == 1;
        case KC_NIGHT: return w->day == 0;
        case KC_FIRST_ENCOUNTER: {
            /* 简化:以 battle.round_used==0 且双方主力初次交手近似 */
            if (!b->active) return 0;
            return b->round_used == 0;
        }
        case KC_ENEMY_MAIN_MASTER: {
            int em = ks_enemy_main(w, uid);
            return em ? (w->units[em - 1].utype == KS_U_MASTER) : 0;
        }
        case KC_SELF_HP:
            return (ks_unit_attr_value(w, uid, el->cond_arg) >= el->cond_arg2);
        case KC_SELF_MP: return (u->mp.cur >= el->cond_arg);
        case KC_FRIEND_IN_BATTLE: {
            if (!b->active) return 0;
            int side = (u->battle_side == 1) ? 1 : 2;
            int n = (side == 1) ? b->left_n : b->right_n;
            return n > 1;
        }
        case KC_ENEMY_IN_BATTLE: {
            if (!b->active) return 0;
            int side = (u->battle_side == 1) ? 2 : 1;
            int n = (side == 1) ? b->left_n : b->right_n;
            return n > 1;
        }
        case KC_SELF_TRAIT: return ks_trait_holds(u, el->cond_arg);
        case KC_ENEMY_TACTIC: {
            if (!b->active) return 0;
            int es = (u->battle_side == 1) ? 2 : 1;
            return b->tactics[es] == el->cond_arg;
        }
        case KC_SELF_TACTIC: {
            if (!b->active) return 0;
            int side = (u->battle_side == 1) ? 1 : 2;
            return b->tactics[side] == el->cond_arg;
        }
        case KC_TACTIC_NOT_PAIRED: {
            if (!b->active || !b->tactics[1] || !b->tactics[2]) return 1;
            int my = (u->battle_side == 1) ? 1 : 2;
            int en = (my == 1) ? 2 : 1;
            /* 己方战术未被敌方克制 = 不处于被克关系 */
            static const int next[] = { 0, T_RAID, T_PROBE, T_HOLD, T_STRIKE };
            return next[b->tactics[en]] != b->tactics[my];
        }
        case KC_ENEMY_IS_SERVANT: {
            int em = ks_enemy_main(w, uid);
            return em ? (w->units[em - 1].utype == KS_U_SERVANT) : 0;
        }
        case KC_ENEMY_IS_SUMMON:
            return t ? (t->utype == KS_U_SUMMON) : 0;
        case KC_SELF_IS_MASTER: return (u->utype == KS_U_MASTER);
        case KC_SELF_IS_SERVANT: return (u->utype == KS_U_SERVANT);
        case KC_HAS_CS: {
            if (!b->active) return 0;
            int side = (u->battle_side == 1) ? 1 : 2;
            int n = (side == 1) ? b->left_n : b->right_n;
            for (int i = 0; i < n; i++) {
                int fid = (side == 1) ? b->left[i].uid : b->right[i].uid;
                if (w->units[fid - 1].cs > 0) return 1;
            }
            return 0;
        }
        case KC_MP_UNDER: return (u->mp.cur < 0);
        case KC_TARGET_AGI_LT:
            return t ? (ks_unit_attr_value(w, tgt, A_AGI) < el->cond_arg) : 0;
        case KC_TARGET_AGI_GE:
            return t ? (ks_unit_attr_value(w, tgt, A_AGI) >= el->cond_arg) : 0;
    }
    return 0;
}

/* ---------------- 目标解析 ---------------- */

/* 解析效果行的目标集合,写入 out[];返回数量。
 * target: 0=敌方全体 -1=己方全体 -2=自身 正数=敌方第N位 */
static int ks_effect_targets(ks_world_t *w, int uid, const ks_effectline_t *el, int *out, int max) {
    int n = 0;
    ks_battle_t *b = &w->battle;
    ks_unit_t *u = &w->units[uid - 1];
    int my_side = (u->battle_side == 1) ? 1 : (u->battle_side == 2) ? 2 : 0;

    if (el->target == -2) { out[n++] = uid; return n; }
    if (!b->active || !my_side) { out[n++] = uid; return n; }   /* 战外作用于自身 */

    int en = (my_side == 1) ? 2 : 1;
    int my_n = (my_side == 1) ? b->left_n : b->right_n;
    int en_n = (en == 1) ? b->left_n : b->right_n;

    if (el->target == 0) {
        for (int i = 0; i < en_n && n < max; i++)
            out[n++] = (en == 1) ? b->left[i].uid : b->right[i].uid;
    } else if (el->target == -1) {
        for (int i = 0; i < my_n && n < max; i++)
            out[n++] = (my_side == 1) ? b->left[i].uid : b->right[i].uid;
    } else {
        int idx = el->target - 1;
        if (idx >= 0 && idx < en_n)
            out[n++] = (en == 1) ? b->left[idx].uid : b->right[idx].uid;
    }
    return n;
}

/* ---------------- 判定 ---------------- */

/* 效果效果的判定:返回 1=成功。
 * 成功率来源优先级:chance_attr_base(属性) > chance(固定值)
 * luck_halve:目标幸运>=40 时成功率减半
 * chance_neg:负面判定,受目标抗性上升/下降修正 */
static int ks_effect_check(ks_world_t *w, int uid, int tgt, const ks_effectline_t *el, int *rate_out) {
    int rate;
    if (el->chance > 0) {
        rate = el->chance;
    } else if (el->chance_attr_base >= 0 && el->chance_attr_base < A_ATTR_COUNT) {
        rate = ks_unit_attr_value(w, uid, el->chance_attr_base);
    } else {
        if (rate_out) *rate_out = 0;
        return 1;   /* 无判定则必定生效 */
    }
    if (el->luck_halve && tgt > 0 &&
        ks_unit_attr_value(w, tgt, A_LUK) >= 40)
        rate /= 2;
    if (el->chance_neg && tgt > 0) {
        ks_unit_t *t = &w->units[tgt - 1];
        for (int i = 0; i < KSG_STATUS_MAX; i++) {
            if (t->status[i].kind == S_RESUP) rate -= t->status[i].layers;
            if (t->status[i].kind == S_RESDOWN) rate += t->status[i].layers;
        }
        /* 状态抵抗 */
        for (int i = 0; i < KSG_STATUS_MAX; i++) {
            if (t->status[i].kind == S_STATE_RES && t->status[i].layers == el->status) {
                rate = rate / 2 - 20;
            }
            if (t->status[i].kind == S_STATE_IM && t->status[i].layers == el->status)
                rate = -1;   /* 免疫:必败 */
        }
    }
    rate = ks_clamp(rate, 0, 100);
    if (rate_out) *rate_out = rate;
    return (rate == 0) ? 0 : ks_roll_bool(w, rate);
}

/* ---------------- 状态辅助 ---------------- */

/* 移除指定状态若干层(层数不足则移除整个状态) */
static void ks_unit_lose_status(ks_world_t *w, int uid, int kind, int layers) {
    ks_unit_t *u = &w->units[uid - 1];
    if (layers <= 0) return;
    for (int i = 0; i < KSG_STATUS_MAX; i++) {
        if (u->status[i].kind == kind) {
            u->status[i].layers -= layers;
            if (u->status[i].layers <= 0) { u->status[i].kind = S_NONE; u->status[i].layers = 0; }
            return;
        }
    }
}

static int ks_status_layers(ks_world_t *w, int uid, int kind) {
    ks_unit_t *u = &w->units[uid - 1];
    for (int i = 0; i < KSG_STATUS_MAX; i++)
        if (u->status[i].kind == kind) return u->status[i].layers;
    return 0;
}

/* ---------------- 汇率/轰炸 ---------------- */

/* 对目标执行"轰击"判定(规则:判定成功→触发生效;对召唤物/人偶直接退场) */
static void ks_do_bound_death(ks_world_t *w, int src, int tgt, int rate) {
    ks_unit_t *t = &w->units[tgt - 1];
    ks_battle_t *b = &w->battle;
    /* 先检查[无敌]/[回避] */
    for (int i = 0; i < KSG_STATUS_MAX; i++) {
        ks_stat_t *st = &t->status[i];
        if (st->kind == S_EVADE) {
            ks_log(w, "  · %s 以[回避]无效化轰击!", t->name);
            st->kind = S_NONE; st->layers = 0;
            return;
        }
        if (st->kind == S_INVINCIBLE) {
            ks_log(w, "  · %s 以[无敌]无效化轰击!", t->name);
            return;
        }
    }
    if (ks_roll_bool(w, rate)) {
        if (t->utype == KS_U_SUMMON || t->utype == KS_U_DOLL) {
            if (t->utype == KS_U_DOLL) {
                ks_unit_gain_status(w, tgt, S_NONE, 0, src);   /* 占位 */
                for (int i = 0; i < KSG_STATUS_MAX; i++)
                    if (t->status[i].kind == S_NONE) {
                        /* 人偶获得[损毁] */
                        t->status[i].kind = S_STUN;   /* 复用槽位,以"损毁"语义处理 */
                        t->status[i].layers = 1;
                        t->status[i].source = src;
                        break;
                    }
                ks_log(w, "  · 轰击成功!%s(人偶) 被[损毁],离开战斗位", t->name);
                if (t->in_battle) { t->battle_slot = KS_SLOT_NONE; t->in_battle = 0; }
            } else {
                ks_log(w, "  · 轰击成功!%s(召唤物) 退场", t->name);
                t->alive = 0;
            }
        } else {
            ks_log(w, "  · 轰击判定成功!%s 受到轰击影响", t->name);
        }
    } else {
        ks_verbose(w, "  · %s 轰击判定失败(%d%%)", t->name, rate);
    }
}

/* ---------------- 主结算函数(导出) ---------------- */

void ks_apply_effect_line(ks_world_t *w, int uid, const ks_effectline_t *el, int src) {
    ks_unit_t *u = &w->units[uid - 1];
    if (!u->alive) return;
    /* 条件检查 */
    if (!ks_cond_check(w, uid, uid, el)) return;
    /* 判定 */
    int rate = 0;
    if (!ks_effect_check(w, src ? src : uid, uid, el, &rate)) {
        ks_verbose(w, "  · %s 效果判定失败(%d%%)", u->name, rate);
        return;
    }
    /* 次数循环 */
    int times = el->times > 0 ? el->times : 1;
    for (int t = 0; t < times; t++) {
        switch (el->flag) {
            case EF_ATTR_UP:
            case EF_ATTR_DOWN: {
                int v = (el->flag == EF_ATTR_UP) ? el->value : -el->value;
                if (el->attr >= 0 && el->attr < A_ATTR_COUNT)
                    u->attr_mod[el->attr] += v;
                ks_verbose(w, "  · %s %s %+d", u->name, ks_attr_name(el->attr), v);
                break;
            }
            case EF_ATTR_UP_CONST:
            case EF_ATTR_DOWN_CONST: {
                int v = (el->flag == EF_ATTR_UP_CONST) ? el->value : -el->value;
                if (el->attr >= 0 && el->attr < A_ATTR_COUNT)
                    u->attr_perm[el->attr] += v;
                ks_verbose(w, "  · %s %s(常驻) %+d", u->name, ks_attr_name(el->attr), v);
                break;
            }
            case EF_WIN_UP:
            case EF_WIN_DOWN: {
                if (w->battle.active && u->in_battle) {
                    int side = (u->battle_side == 1) ? 1 : 2;
                    int v = (el->flag == EF_WIN_UP) ? el->value : -el->value;
                    ks_battle_add_win(w, side, v);
                } else {
                    ks_verbose(w, "  · (战斗外)胜率修正 %+d%% 对 %s", el->value, u->name);
                }
                break;
            }
            case EF_FINAL_WIN_UP:
            case EF_FINAL_WIN_DOWN: {
                if (w->battle.active && u->in_battle) {
                    int side = (u->battle_side == 1) ? 1 : 2;
                    int v = (el->flag == EF_FINAL_WIN_UP) ? el->value : -el->value;
                    if (side == 1) w->battle.left_final_win += v;
                    else w->battle.right_final_win += v;
                    ks_log(w, "  · [最终胜率修正] %s %+d%%", u->name, v);
                }
                break;
            }
            case EF_FLOOR_UP:
            case EF_FLOOR_PEN: {
                if (w->battle.active && u->in_battle) {
                    int side = (u->battle_side == 1) ? 1 : 2;
                    int v = (el->flag == EF_FLOOR_UP) ? el->value : -el->value;
                    if (side == 1) w->battle.left_floor += v;
                    else w->battle.right_floor += v;
                    ks_log(w, "  · [底限胜率] %s %+d%%", u->name, v);
                }
                break;
            }
            case EF_HIT_UP: u->hit_mod += el->value; break;
            case EF_HIT_FINAL_UP: u->hit_final_mod += el->value; break;
            case EF_HIT_PEN: u->hit_mod -= el->value; break;
            case EF_RES_UP:
                ks_unit_gain_status(w, uid, S_RESUP, el->value, src);
                break;
            case EF_RES_DOWN:
                ks_unit_gain_status(w, uid, S_RESDOWN, el->value, src);
                break;
            case EF_STATE_RES:
                ks_unit_gain_status(w, uid, S_STATE_RES, el->status, src);
                break;
            case EF_STATE_IM:
                ks_unit_gain_status(w, uid, S_STATE_IM, el->status, src);
                break;
            case EF_EFFECT_IM:
                ks_unit_gain_status(w, uid, S_EFFECT_IM, el->status, src);
                break;
            case EF_MANA_UP:
                ks_mp_gain(w, uid, el->value);
                break;
            case EF_MANA_DOWN:
                ks_mp_pay(w, uid, el->value);
                break;
            case EF_STATUS_GIVE:
                ks_unit_gain_status(w, uid, el->status, el->layers, src);
                ks_verbose(w, "  · %s 获得 %s%d", u->name, ks_status_name(el->status), el->layers);
                break;
            case EF_STATUS_REMOVE:
                ks_unit_lose_status(w, uid, el->status, el->layers ? el->layers : 1);
                break;
            case EF_BURN_BLOW: {
                int layers = ks_status_layers(w, uid, S_BURN);
                if (layers > 0) {
                    for (int a = 0; a < A_NP; a++) u->attr_mod[a] -= 5 * layers;
                    ks_unit_lose_status(w, uid, S_BURN, layers);
                    ks_log(w, "  · %s [爆燃]!清除灼伤%d层,除宝具外全属性-%d",
                           u->name, layers, 5 * layers);
                }
                break;
            }
            case EF_ELECTRIC_BLOW: {
                int layers = ks_status_layers(w, uid, S_ELECTRIC);
                if (layers > 0) {
                    if (w->battle.active && u->in_battle) {
                        int side = (u->battle_side == 1) ? 1 : 2;
                        ks_battle_add_win(w, side, -10 * layers);
                    }
                    ks_unit_lose_status(w, uid, S_ELECTRIC, layers);
                    ks_log(w, "  · %s [激荡]!清除感电%d层,胜率-%d%%",
                           u->name, layers, 10 * layers);
                }
                break;
            }
            case EF_POISON_BLOW: {
                int layers = ks_status_layers(w, uid, S_POISON);
                if (layers > 0) {
                    int rate2 = ks_clamp(10 * layers - ks_unit_attr_value(w, uid, A_END) / 2, 0, 100);
                    if (ks_unit_attr_value(w, uid, A_END) >= 20) rate2 /= 2;
                    ks_log(w, "  · %s [毒发]!中毒%d层,即死判定 %d%%", u->name, layers, rate2);
                    if (ks_roll_bool(w, rate2)) {
                        if (u->cs > 0) { u->cs--; ks_log(w, "    消耗1令咒抵消!"); }
                        else { u->alive = 0; ks_log(w, "    %s 毒发身亡!", u->name); }
                    }
                    int remain = (layers + 1) / 2;
                    ks_unit_lose_status(w, uid, S_POISON, layers - remain);
                }
                break;
            }
            case EF_RECAST:
                if (el->value > 0) ks_res_gain_recast(w, uid, el->value, el->value);
                break;
            case EF_RECAST_LOSE:
                ks_res_gain_recast(w, uid, el->value > 0 ? el->value : uid, -el->value);
                break;
            case EF_FP_UP: u->fp += el->value; break;
            case EF_FP_DOWN: u->fp -= el->value; if (u->fp < 0) u->fp = 0; break;
            case EF_TP_FP: u->tp_fp += el->value; break;
            case EF_DEATH: {
                int r2 = el->value;
                if (el->luck_halve && ks_unit_attr_value(w, uid, A_LUK) >= 40) r2 /= 2;
                if (el->chance_neg) {
                    for (int i = 0; i < KSG_STATUS_MAX; i++)
                        if (u->status[i].kind == S_RESUP) r2 -= u->status[i].layers;
                }
                r2 = ks_clamp(r2, 0, 100);
                ks_log(w, "  · %s 受到[即死]判定 %d%%", u->name, r2);
                if (ks_roll_bool(w, r2)) {
                    if (u->cs > 0) { u->cs--; ks_log(w, "    以1枚令咒抵消!"); }
                    else { u->alive = 0; ks_log(w, "    %s 即死,退场!", u->name); }
                }
                break;
            }
            case EF_BOUND_DEATH:
                ks_do_bound_death(w, src, uid, ks_clamp(el->value, 0, 100));
                break;
            case EF_PIERCE:
                ks_unit_gain_status(w, uid, S_EFFECT_IM, S_EVADE, src); /* 以效果免疫标记表示"必中定向" */
                break;
            case EF_INV_PIERCE:
                ks_unit_gain_status(w, uid, S_EFFECT_IM, S_INVINCIBLE, src);
                break;
            case EF_EVADE:
                ks_unit_gain_status(w, uid, S_EVADE, 1, src);
                break;
            case EF_PROTECT:
                ks_unit_gain_status(w, uid, S_PROTECT, 1, src);
                break;
            case EF_INVINCIBLE:
                ks_unit_gain_status(w, uid, S_INVINCIBLE, 1, src);
                break;
            case EF_SUMMON: {
                /* 召唤物:无魔力池,由施术者持有,入仆役位(有富余时) */
                int lv = el->value;
                int total = el->cond_arg;
                if (lv <= 0) break;
                if (w->unit_count >= 64) { ks_log(w, "  · (单位槽已满,无法召唤)"); break; }
                char sname[KSG_NAME_MAX];
                snprintf(sname, sizeof(sname), "%s的召唤物", w->units[src - 1].name);
                int sid = ks_unit_new(w, sname, "召唤物", KS_U_SUMMON, lv, u->faction);
                ks_unit_t *su = &w->units[sid - 1];
                /* 总属性均分到非宝具属性 */
                int per = total / 5;
                for (int a = 0; a < 5; a++) su->attr[a] = per;
                su->attr[A_NP] = 0;
                if (el->cond_arg2) su->traits = el->cond_arg2;
                su->mp.cap = 0; su->mp.floor = 0; su->mp.cur = 0;
                su->fp = 0; su->cs = 0;
                su->alive = 1;
                /* 战斗中有空余仆役位则入战 */
                if (w->battle.active && u->in_battle) {
                    int side = (u->battle_side == 1) ? 1 : 2;
                    ks_battle_t *b = &w->battle;
                    int n = (side == 1) ? b->left_n : b->right_n;
                    if (n < KSG_COLUMN_MAX - 1 && n < b->width) {
                        su->in_battle = 1;
                        su->battle_side = side;
                        su->battle_slot = KS_SLOT_SERVANT;
                        if (side == 1) b->left[b->left_n++].uid = sid;
                        else b->right[b->right_n++].uid = sid;
                        ks_log(w, "  · 召唤物[%s] Lv%d 加入%s仆役位", sname, lv,
                               side == 1 ? "左方" : "右方");
                    } else {
                        ks_log(w, "  · 召唤物[%s] 生成(战斗位已满,待机)", sname);
                    }
                } else {
                    ks_log(w, "  · 召唤物[%s] Lv%d 生成", sname, lv);
                }
                break;
            }
            case EF_RETALIATE:
                /* 反击:在结算链中由战斗系统先结算本效果 */
                break;
            case EF_CS:
                if (u->cs > 0) u->cs += el->value;
                break;
            case EF_INFO:
                ks_verbose(w, "  · %s 获得情报:%s", u->name, el->desc);
                break;
            /* ---- 复杂机制模块 ---- */
            case EF_PLEDGE_DECL:
                /* 宣言对抗:每工序暗宣言,同/不同判定由战斗系统在工序推进时调用本效果 */
                ks_pledge_decl(w, src, uid, el);
                break;
            case EF_TICK_PROC:
            case EF_TICK_ROUND: {
                /* 每工序/回合状态结算:目标持状态status层数, 按chance(缺省=层*10)%判定, 成功给value胜率惩罚 */
                int layers = (el->status > 0 && el->status < S_STATUS_COUNT)
                    ? ks_unit_status_layers(w, uid, el->status) : 0;
                if (layers <= 0) break;
                int rate = (el->chance > 0) ? el->chance : layers * 10;
                if (rate > 100) rate = 100;
                int r = ks_roll(w) <= rate;
                if (r)
                    ks_battle_add_win(w, (u->battle_side == 1) ? 1 : 2,
                                      (el->value < 0) ? el->value : -el->value);
                ks_verbose(w, "  · %s %s结算(%s%d层):判定%d%%%s",
                           u->name,
                           (el->flag == EF_TICK_PROC) ? "每工序" : "每回合",
                           ks_status_name(el->status), layers, rate,
                           r ? "成功" : "失败");
                break;
            }
            case EF_RANDOM_GIVE: {
                static const int pool[] = {S_BURN, S_ELECTRIC, S_FREEZE, S_POISON, S_CURSE,
                                           S_CHARM, S_CONFUSE, S_FEAR, S_TIRED};
                int pick = pool[(ks_roll(w) % (int)(sizeof(pool)/sizeof(pool[0])))];
                int layers = (el->layers > 0) ? el->layers : 1;
                ks_unit_gain_status(w, uid, pick, layers, src);
                ks_verbose(w, "  · 随机状态:%s 获得 %s%d", u->name, ks_status_name(pick), layers);
                break;
            }
            case EF_ON_KILL:
                /* 击杀触发:由战斗系统在单位退场时对击杀者结算本效果(近似=击杀者胜率+value) */
                ks_verbose(w, "  · %s 击杀触发:胜率+%d(结算时)", u->name, el->value);
                break;
            case EF_ON_WIN:
                /* 胜利触发:战斗结束时胜方结算 */
                ks_verbose(w, "  · %s 胜利触发:胜率+%d(战斗结束时)", u->name, el->value);
                break;
            case EF_CHARGE:
                ks_unit_gain_status(w, uid, S_CHARGE, (el->layers > 0) ? el->layers : el->value, src);
                ks_verbose(w, "  · %s [蓄力] %d", u->name, (el->layers > 0) ? el->layers : el->value);
                break;
            case EF_GRANT_CS:
                u->cs += el->value;
                ks_log(w, "  · %s 获得令咒+%d(现%d)", u->name, el->value, u->cs);
                break;
            case EF_GRANT_BADGE:
                ks_unit_gain_status(w, uid, S_BADGE, (el->value > 0) ? el->value : 1, src);
                break;
            case EF_REGEN:
                if (u->mp.cur + el->value <= u->mp.cap) u->mp.cur += el->value;
                ks_verbose(w, "  · %s 再生:魔力+%d", u->name, el->value);
                break;
            default:
                break;
        }
    }
}

/* 宣言对抗实现: 若源单位为目标, 记录宣言状态; 战斗系统工序推进时比较 */
void ks_pledge_decl(ks_world_t *w, int src, int uid, const ks_effectline_t *el) {
    if (!w || el->status <= 0 || el->status >= S_STATUS_COUNT) return;
    ks_unit_t *u = &w->units[uid - 1];
    if (u->battle_side == 0) return;
    /* 存到单位临时槽: 宣言状态与层数 */
    u->pledge_state = el->status;
    u->pledge_layers = (el->value > 0) ? el->value : 1;
    ks_verbose(w, "  · %s 宣言:%s(%d层)", u->name, ks_status_name(el->status), u->pledge_layers);
}

/* 每工序推进: 蓄力-1 与宣言对抗 + tick 结算 */
void ks_tick_proc_mechanics(ks_world_t *w) {
    if (!w || !w->battle.active) return;
    ks_battle_t *b = &w->battle;
    for (int side = 1; side <= 2; side++) {
        int n = (side == 1) ? b->left_n : b->right_n;
        for (int i = 0; i < n; i++) {
            int suid = (side == 1) ? b->left[i].uid : b->right[i].uid;
            if (suid <= 0 || suid > w->unit_count) continue;
            ks_unit_t *u = &w->units[suid - 1];
            /* 蓄力层数 -1 */
            int ch = ks_unit_status_layers(w, suid, S_CHARGE);
            if (ch > 0) {
                if (ch <= 1) {
                    ks_status_clear(w, suid, S_CHARGE);
                    ks_log(w, "  · %s 蓄力完成,触发结算", u->name);
                } else {
                    ks_status_remove_layers(w, suid, S_CHARGE, 1);
                }
            }
            /* 毒发/灼伤等每工序结算(若有TICK_PROC注册) */
        }
    }
}

/* ---------------- 资源全效果结算 ---------------- */

/* 该效果是否为[攻击性/负面]效果(值得被[回避]/[无敌]拦截) */
static int ks_eff_is_offensive(int flag) {
    switch (flag) {
        case EF_ATTR_DOWN:
        case EF_ATTR_DOWN_CONST:
        case EF_WIN_DOWN:
        case EF_FINAL_WIN_DOWN:
        case EF_FLOOR_PEN:
        case EF_HIT_PEN:
        case EF_RES_DOWN:
        case EF_MANA_DOWN:
        case EF_STATUS_GIVE:
        case EF_BURN_BLOW:
        case EF_ELECTRIC_BLOW:
        case EF_POISON_BLOW:
        case EF_DEATH:
        case EF_BOUND_DEATH:
        case EF_FP_DOWN:
        case EF_RECAST_LOSE:
            return 1;
        default:
            return 0;
    }
}

/* 目标是否带[无敌]/[回避]拦截本次效果;返回 1=拦截 */
static int ks_effect_blocked(ks_world_t *w, int tgt, int src, const ks_effectline_t *el, ks_res_t *r) {
    ks_unit_t *t = &w->units[tgt - 1];
    if (src == tgt) return 0;                      /* 自身效果不拦截 */
    if (!ks_eff_is_offensive(el->flag)) return 0;
    /* [无敌]:不受来源自身外的任意效果影响;[无敌贯通]可穿透 */
    if (ks_unit_has_status(w, tgt, S_INVINCIBLE) && !(r->feat & KS_F_INV_PIERCE)) {
        ks_log(w, "  · %s 以[无敌]拦截了 %s 的%s!", t->name,
               w->units[src - 1].name, r->name);
        return 1;
    }
    /* [回避]:下一个来源不为自身的技能/宝具效果无效化;[必中]穿透 */
    if (ks_unit_has_status(w, tgt, S_EVADE) && !(r->feat & KS_F_PIERCE)) {
        ks_log(w, "  · %s 以[回避]闪避了 %s 的%s!", t->name,
               w->units[src - 1].name, r->name);
        ks_status_remove_layers(w, tgt, S_EVADE, 1);   /* 回避被消耗 */
        return 1;
    }
    return 0;
}

/* 结算一个资源的全部效果行:按目标解析将效果施加到每个目标 */
void ks_apply_res_effects(ks_world_t *w, int src, int rid, int target_override) {
    ks_res_t *r = &w->res[rid - 1];
    if (!r->id) return;
    ks_unit_t *u = &w->units[src - 1];
    for (int i = 0; i < r->effect_count; i++) {
        const ks_effectline_t *el = &r->effects[i];
        if (el->flag == EF_NONE) continue;
        int targets[64], n;
        if (target_override != 0) {
            /* 指定目标 */
            targets[0] = target_override;
            n = 1;
        } else {
            n = ks_effect_targets(w, src, el, targets, 64);
        }
        for (int k = 0; k < n; k++) {
            /* 回避/无敌拦截(带[必中]/[无敌贯通]的资源可穿透) */
            if (ks_effect_blocked(w, targets[k], src, el, r)) continue;
            ks_apply_effect_line(w, targets[k], el, src);
        }
    }
}