/* ksg_status.c — 空想圣杯引擎:状态引擎
 * 依据《空想圣杯规则书》附录三词典,实现全部强化/弱化/异常状态的:
 *  - 回合开始/工序开始的自动判定与效果(中毒/灼伤/残废/晕眩等)
 *  - 属性影响(疲惫/冻结/石化)
 *  - 时点衰减(回合结束/工序结束/战斗结束)
 *  - 层数上限(诅咒20/感电20/魅惑9)与叠加
 *  - 状态免疫/抵抗对赋予流程的影响
 */
#include "ksg.h"

/* ---------------- 状态层数查询 ---------------- */

int ks_status_layers_at(ks_world_t *w, int uid, int kind) {
    ks_unit_t *u = &w->units[uid - 1];
    for (int i = 0; i < KSG_STATUS_MAX; i++)
        if (u->status[i].kind == kind) return u->status[i].layers;
    return 0;
}

/* 移除状态指定层(不足则整体移除);返回剩余层数 */
int ks_status_remove_layers(ks_world_t *w, int uid, int kind, int layers) {
    ks_unit_t *u = &w->units[uid - 1];
    for (int i = 0; i < KSG_STATUS_MAX; i++) {
        if (u->status[i].kind == kind) {
            if (layers <= 0) return u->status[i].layers;
            u->status[i].layers -= layers;
            if (u->status[i].layers <= 0) {
                u->status[i].kind = S_NONE;
                u->status[i].layers = 0;
                u->status[i].source = 0;
                return 0;
            }
            return u->status[i].layers;
        }
    }
    return 0;
}

/* 强制清除状态(全部层) */
void ks_status_clear(ks_world_t *w, int uid, int kind) {
    ks_unit_t *u = &w->units[uid - 1];
    for (int i = 0; i < KSG_STATUS_MAX; i++) {
        if (u->status[i].kind == kind) {
            u->status[i].kind = S_NONE;
            u->status[i].layers = 0;
            u->status[i].source = 0;
        }
    }
}

/* 是否持有对 kind 的[状态免疫] */
int ks_status_immune(ks_world_t *w, int uid, int kind) {
    ks_unit_t *u = &w->units[uid - 1];
    for (int i = 0; i < KSG_STATUS_MAX; i++) {
        ks_stat_t *st = &u->status[i];
        if (st->kind == S_STATE_IM) {
            /* layers 字段存免疫的状态种类 */
            if (st->layers == kind || st->layers == 0 || st->layers == -1) return 1;
        }
    }
    return 0;
}

/* 状态抵抗:返回对 kind 判定的最终成功率惩罚(0=无) */
int ks_status_resist(ks_world_t *w, int uid, int kind) {
    ks_unit_t *u = &w->units[uid - 1];
    for (int i = 0; i < KSG_STATUS_MAX; i++) {
        ks_stat_t *st = &u->status[i];
        if (st->kind == S_STATE_RES && st->layers == kind) return 30; /* 简化:统一-30% */
    }
    return 0;
}

/* ---------------- 回合开始时点 ---------------- */

/* 回合开始时:中毒/灼伤/残废/迟滞等状态的判定与效果 */
void ks_status_round_start_tick(ks_world_t *w, int uid) {
    ks_unit_t *u = &w->units[uid - 1];
    if (!u->alive) return;

    /* [残废]:回合开始 50% 即死判定,成功必须消耗令咒否则退场 */
    if (ks_unit_has_status(w, uid, S_CRIPPLED)) {
        ks_log(w, "  · %s [残废]回合开始即死判定 50%%", u->name);
        if (ks_roll_bool(w, 50)) {
            if (u->cs > 0) {
                u->cs--;
                ks_log(w, "    消耗1枚令咒抵消即死!");
            } else {
                u->alive = 0;
                ks_log(w, "    %s 残废发作,退场!", u->name);
            }
        } else {
            ks_log(w, "    判定失败,侥幸存活");
        }
    }

    /* [中毒]:20*层数% 负面判定,成功则随机筋力/耐久/敏捷-10常驻惩罚,层数-1 */
    int poison = ks_status_layers_at(w, uid, S_POISON);
    if (poison > 0) {
        int rate = 20 * poison;
        if (ks_unit_attr_value(w, uid, A_END) >= 20) rate /= 2;
        rate = ks_clamp(rate, 0, 100);
        ks_log(w, "  · %s [中毒]判定 %d%%", u->name, rate);
        if (ks_roll_bool(w, rate)) {
            int attr = A_STR + (ks_roll(w) % 3);   /* 筋力/耐久/敏捷 随机 */
            u->attr_perm[attr] -= 10;
            ks_log(w, "    中毒发作!%s -10常驻惩罚", ks_attr_name(attr));
        }
        ks_status_remove_layers(w, uid, S_POISON, 1);
    }

    /* [灼伤]:灼伤层数*20% 负面判定,成功则 筋力/耐久/敏捷/幸运 随机-10常驻惩罚,层数-1 */
    int burn = ks_status_layers_at(w, uid, S_BURN);
    if (burn > 0) {
        int rate = ks_clamp(burn * 20, 0, 100);
        ks_log(w, "  · %s [灼伤]判定 %d%%", u->name, rate);
        if (ks_roll_bool(w, rate)) {
            int attrs[4] = { A_STR, A_END, A_AGI, A_LUK };
            int attr = attrs[ks_roll(w) % 4];
            u->attr_perm[attr] -= 10;
            ks_log(w, "    灼伤发作!%s -10常驻惩罚", ks_attr_name(attr));
        }
        /* 与[冻结]同时存在时不衰减层数 */
        if (!ks_unit_has_status(w, uid, S_FREEZE))
            ks_status_remove_layers(w, uid, S_BURN, 1);
    }

    /* [冻结]:回合开始层数-1(战斗内每3层额外-5属性已在属性计算中处理) */
    if (ks_unit_has_status(w, uid, S_FREEZE))
        ks_status_remove_layers(w, uid, S_FREEZE, 1);

    /* [迟滞]:回合结束清除层数(规则:每工序-1,回合结束时清除) */
    if (ks_unit_has_status(w, uid, S_LAG))
        ks_status_clear(w, uid, S_LAG);

    /* [晕眩]:每工序开始时层数*20%负面判定,失败解除晕眩 */
    int stun = ks_status_layers_at(w, uid, S_STUN);
    if (stun > 0) {
        int rate = ks_clamp(stun * 20, 0, 100);
        if (!ks_roll_bool(w, rate)) {
            ks_log(w, "  · %s [晕眩]判定失败,状态解除", u->name);
            ks_status_clear(w, uid, S_STUN);
        } else {
            ks_status_remove_layers(w, uid, S_STUN, 1);
        }
    }
}

/* ---------------- 工序开始时点 ---------------- */

/* 工序开始时:中毒每工序判定,晕眩判定(战斗内) */
void ks_status_proc_start_tick(ks_world_t *w, int uid) {
    ks_unit_t *u = &w->units[uid - 1];
    if (!u->alive) return;

    /* [中毒]:每战斗工序开始时同样判定(回合开始时已判,此处战斗内再判) */
    int poison = ks_status_layers_at(w, uid, S_POISON);
    if (poison > 0 && u->in_battle) {
        int rate = 20 * poison;
        if (ks_unit_attr_value(w, uid, A_END) >= 20) rate /= 2;
        rate = ks_clamp(rate, 0, 100);
        if (ks_roll_bool(w, rate)) {
            int attr = A_STR + (ks_roll(w) % 3);
            u->attr_perm[attr] -= 10;
            ks_log(w, "  · %s [中毒]工序发作!%s -10", u->name, ks_attr_name(attr));
        }
        ks_status_remove_layers(w, uid, S_POISON, 1);
    }

    /* [晕眩]:工序开始时判定 */
    int stun = ks_status_layers_at(w, uid, S_STUN);
    if (stun > 0) {
        int rate = ks_clamp(stun * 20, 0, 100);
        if (!ks_roll_bool(w, rate)) {
            ks_log(w, "  · %s [晕眩]工序判定失败,解除", u->name);
            ks_status_clear(w, uid, S_STUN);
        } else {
            ks_status_remove_layers(w, uid, S_STUN, 1);
        }
    }
}

/* ---------------- 工序结束时点 ---------------- */

/* 工序结束:[迟滞]层数-1 */
void ks_status_proc_end_tick(ks_world_t *w, int uid) {
    ks_unit_t *u = &w->units[uid - 1];
    int lag = ks_status_layers_at(w, uid, S_LAG);
    if (lag > 0) ks_status_remove_layers(w, uid, S_LAG, 1);
}

/* ---------------- 回合结束时点 ---------------- */

/* 回合结束:感电魔耗、诅咒减层、疲惫减层、封印减层、迟滞清除、染毒惩罚回退 */
void ks_status_round_end_tick(ks_world_t *w, int uid) {
    ks_unit_t *u = &w->units[uid - 1];
    if (!u->alive) return;

    /* [感电]:轮次结束时 -5*层 魔力,然后清除 */
    int elec = ks_status_layers_at(w, uid, S_ELECTRIC);
    if (elec > 0) {
        ks_log(w, "  · %s [感电]结算:魔力-%d", u->name, 5 * elec);
        ks_mp_pay(w, uid, 5 * elec);
        ks_status_clear(w, uid, S_ELECTRIC);
    }

    /* [疲惫]:非游荡灵脉,回合开始或休整时层数-1(在回合结束时近似) */
    if (ks_unit_has_status(w, uid, S_TIRED) && !u->roaming)
        ks_status_remove_layers(w, uid, S_TIRED, 1);

    /* [诅咒]:(简化)层数每回合-1 */
    if (ks_unit_has_status(w, uid, S_CURSE))
        ks_status_remove_layers(w, uid, S_CURSE, 1);

    /* [封印]/[技能封印]/[宝具封印]:战斗结束或回合结束时层数-1 */
    if (ks_unit_has_status(w, uid, S_SEAL))
        ks_status_remove_layers(w, uid, S_SEAL, 1);
    if (ks_unit_has_status(w, uid, S_SKILL_SEAL))
        ks_status_remove_layers(w, uid, S_SKILL_SEAL, 1);
    if (ks_unit_has_status(w, uid, S_NP_SEAL))
        ks_status_remove_layers(w, uid, S_NP_SEAL, 1);

    /* [迟滞]回合结束清除 */
    if (ks_unit_has_status(w, uid, S_LAG))
        ks_status_clear(w, uid, S_LAG);

    /* [混乱]/[恐惧]:回合结束清除 */
    if (ks_unit_has_status(w, uid, S_CONFUSE))
        ks_status_clear(w, uid, S_CONFUSE);
    if (ks_unit_has_status(w, uid, S_FEAR))
        ks_status_clear(w, uid, S_FEAR);

    /* [抗性破除]:回合结束移除 */
    if (ks_unit_has_status(w, uid, S_RES_BREAK))
        ks_status_clear(w, uid, S_RES_BREAK);
}

/* ---------------- 战斗结束时点 ---------------- */

/* 战斗结束:回避/无敌等战斗状态清除 */
void ks_status_battle_end_clean(ks_world_t *w, int uid) {
    ks_unit_t *u = &w->units[uid - 1];
    for (int i = 0; i < KSG_STATUS_MAX; i++) {
        ks_stat_t *st = &u->status[i];
        if (st->kind == S_NONE) continue;
        switch (st->kind) {
            case S_EVADE:      /* 回避:生效或回合结束后失去 */
            case S_INVINCIBLE: /* 无敌:战斗/回合结束移除 */
            case S_PROTECT:    /* 保护:战斗结束/回合开始移除 */
            case S_LAG:
            case S_STUN:
            case S_CONFUSE:
            case S_FEAR:
                if (u->in_battle) {
                    st->kind = S_NONE; st->layers = 0; st->source = 0;
                }
                break;
            default:
                break;
        }
    }
    /* 战斗中获得的临时属性补正(非常驻)在战斗结束清除(规则:修正类效果战斗/回合结束移除) */
    /* attr_mod 保留,由调用方根据时点清除 */
}

/* ---------------- 查询:给玩家展示当前状态 ---------------- */

void ks_status_print_unit(ks_world_t *w, int uid) {
    ks_unit_t *u = &w->units[uid - 1];
    int any = 0;
    for (int i = 0; i < KSG_STATUS_MAX; i++) {
        ks_stat_t *st = &u->status[i];
        if (st->kind == S_NONE) continue;
        if (!any) printf("    状态: ");
        printf("%s%d ", ks_status_name(st->kind), st->layers);
        any = 1;
    }
    if (any) printf("\n");
}