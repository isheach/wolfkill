/* ksg_db.c — 空想圣杯引擎:扩展资源数据库
 * 依据《空想从者资源库》《空想御主资源库》原文效果,录入更多经典技能/宝具。
 * 效果行使用新扩展字段(cond/chance/luck_halve/cap/times)以贴近原文语义。
 */
#include "ksg.h"

/* 便捷构造:技能(带效果行) */
static int db_skill(ks_world_t *w, const char *name, int type, int rank,
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

/* 便捷构造:礼装/科技造物(与 ksg_data.c 的 def_item 签名一致) */
static int def_item_compat(ks_world_t *w, const char *name, int rank, int when,
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

/* 便捷构造:宝具 */
static int db_np(ks_world_t *w, const char *name, int type, int focus, int rank,
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

/* 效果行构造器(带条件/判定/上限) */
typedef struct {
    int flag, attr, value, status, layers, rank, target, times;
    int cond, cond_arg, cond_arg2;
    int chance, chance_attr_base, chance_neg, luck_halve, cap;
    const char *desc;
} E;

static void db_eff(ks_world_t *w, int rid, E e) {
    ks_res_t *r = &w->res[rid - 1];
    if (r->effect_count >= KSG_EOF_MAX) return;
    ks_effectline_t *el = &r->effects[r->effect_count++];
    memset(el, 0, sizeof(*el));
    el->chance_attr_base = -1;   /* 默认无属性判定 */
    el->flag = e.flag;
    el->attr = e.attr;
    el->value = e.value;
    el->status = e.status;
    el->layers = e.layers;
    el->rank = e.rank;
    el->target = e.target;
    el->times = e.times;
    el->cond = e.cond;
    el->cond_arg = e.cond_arg;
    el->cond_arg2 = e.cond_arg2;
    el->chance = e.chance;
    el->chance_attr_base = e.chance_attr_base;
    el->chance_neg = e.chance_neg;
    el->luck_halve = e.luck_halve;
    el->cap = e.cap;
    if (e.desc) snprintf(el->desc, KSG_TEXT_MAX, "%s", e.desc);
}

/* 便捷:礼装/科技造物 */
static int db_item(ks_world_t *w, const char *name, int rank, int when,
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

/* 便捷:科技造物 */
static int db_tech(ks_world_t *w, const char *name, int rank, int when,
                   int cost, int recast, int feat, int reserve)
{
    ks_res_t r;
    memset(&r, 0, sizeof(r));
    snprintf(r.name, KSG_NAME_MAX, "%s", name);
    r.kind = KS_R_TECH;
    r.type = KS_T_WEAPON;
    r.rank = rank;
    r.when = when;
    r.cost = cost;
    r.recast = recast;
    r.feat = feat;
    r.reserve = r.reserve_max = reserve;
    return ks_world_register_res(w, &r);
}

static void db_set_text(ks_world_t *w, int rid, const char *txt) {
    ks_res_t *r = &w->res[rid - 1];
    snprintf(r->text, KSG_TEXT_MAX, "%s", txt ? txt : "");
}

/* 便捷:最常见效果形态(chance_attr_base 默认-1=无属性判定) */
static E E_ATTR(int flag, int attr, int value, int target) {
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = flag; e.attr = attr; e.value = value; e.target = target; return e;
}
static E E_WIN(int flag, int value, int target) {
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = flag; e.value = value; e.target = target; return e;
}
static E E_STATUS(int status, int layers, int target) {
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_STATUS_GIVE; e.status = status; e.layers = layers; e.target = target; return e;
}
static E E_DEATH(int chance, int target, int luck_halve) {
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_DEATH; e.chance = chance; e.target = target; e.luck_halve = luck_halve; return e;
}
static E E_MANA(int flag, int value, int target) {
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = flag; e.value = value; e.target = target; return e;
}

/* 录入全部扩展资源(由 ks_data_build 调用) */
void ks_db_load(ks_world_t *w)
{
    int r;

    /* ============ 从者 · 保有技能(原文效果) ============ */

    /* 领袖气质A:常驻,己方战斗位除自身外+25%胜率;主力位时自身同样受用 */
    r = db_skill(w, "领袖气质·高洁", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 25, -1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 25, -2));
db_set_text(w, r, "己方战斗位除自身外全部单位[+25%]胜率补正;自身处于[主力位]时,自身同样受到[+25%](演示)。");

    /* 直感B:成为非自阵营效果对象时,效果等级下降2级(以判定成功率惩罚近似) */
    r = db_skill(w, "直感·心眼", KS_T_TALENT, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));   /* 简化:常驻小胜率 */
db_set_text(w, r, "成为非自阵营效果对象时,令其效果等级下降2级(演示:常驻[+5%]胜率近似判断力)。");

    /* 魔力放出A:常驻补正+15,胜率+5% */
    r = db_skill(w, "魔力放出·炎", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 15, 1, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 15, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
db_set_text(w, r, "随时发动,[筋力]+15常驻补正与[+5%]胜率补正(演示:以魔力强化武具)。");

    /* 战斗续行EX(简化A):状态免疫残废+角力补正 */
    r = db_skill(w, "战斗续行·不屈", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    {
        E e1001 = E_STATUS(S_STATE_IM, S_CRIPPLED, -2); e1001.chance_attr_base = -1; db_eff(w, r, e1001);
        db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 15, -2));
db_set_text(w, r, "[状态免疫:残废];[耐久]+15常驻补正(演示:不屈的斗志)。");
    }

    /* 黄金律B:回合开始判定40%→获得礼装(演示:获得魔力) */
    r = db_skill(w, "黄金律·聚敛", KS_T_BLESS, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    {
        E e1002 = E_MANA(EF_MANA_UP, 30, -2); e1002.chance = 40; db_eff(w, r, e1002);
db_set_text(w, r, "每回合开始40%判定,成功获得随机礼装(演示:成功时+30魔力近似财富)。");
    }

    /* 领袖气质·帝国:己方战术未被克制时+20%胜率 */
    r = db_skill(w, "皇帝特权", KS_T_CROWN, KS_RANK_A, KS_WHEN_ANY, 0, 9, 0);
    {
        E e = E_WIN(EF_WIN_UP, 20, -2); e.cond = KC_TACTIC_NOT_PAIRED;
        db_eff(w, r, e);
db_set_text(w, r, "己方[战术]未被克制时,给予自身[+20%]胜率补正(演示)。");
    }

    /* 石化之魔眼B:回合开始或干涉时,同灵脉单位90%石化判定 */
    r = db_skill(w, "石化之魔眼", KS_T_TALENT, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    {
        E e = E_STATUS(S_STONE, 1, 0);
        e.chance = 75; e.chance_neg = 1; e.luck_halve = 0;
        e.cond = KC_NONE; e.target = 0;
        db_eff(w, r, e);
db_set_text(w, r, "回合开始或干涉时,对同灵脉单位进行[75%]负面[石化]判定(目标幸运≥40成功率减半,演示)。");
    }

    /* 千里眼A:侦查判定+30%(以判定补正近似),夜战+10%胜率 */
    r = db_skill(w, "千里眼·鹰之瞳", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    {
        E e1003 = E_WIN(EF_WIN_UP, 10, -2); e1003.cond = KC_NIGHT; db_eff(w, r, e1003);
db_set_text(w, r, "自身发起的[侦查][情报调查]判定+30%;[夜间]战斗给予自身[+10%]胜率补正(演示)。");
    }

    /* ============ 从者 · 经典宝具(原文效果) ============ */

    /* 誓约胜利之剑A:蓄力解放,最终工序+80%胜率;追加令咒轰击+80% */
    r = db_np(w, "誓约胜利之剑·光炮", KS_NP_CASTLE, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_PROC, 80, 9, KS_F_MAIN | KS_F_BURST_READY);
    db_eff(w, r, E_WIN(EF_WIN_UP, 80, -2));
    {
        E e1004 = E_WIN(EF_WIN_UP, 80, -2); e1004.cond = KC_HAS_CS; db_eff(w, r, e1004);
    E e1005 = E_STATUS(S_CHARGE, 2, -2); e1005.flag = EF_CHARGE; db_eff(w, r, e1005);
    E e1006 = E_STATUS(S_CHARGE, 3, -2); e1006.flag = EF_CHARGE; db_eff(w, r, e1006); /* EF_CHARGE 蓄力 */
db_set_text(w, r, "[主力位][蓄力]最终工序解放,给予自身[+80%]胜率补正;追加宣言令咒可[轰击]再次[+80%](演示)。");
    }

    /* 刺穿死棘之枪B:对人,50%即死,目标幸运≥40成功率减半 */
    r = db_np(w, "刺穿死棘之枪·因果", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_B,
              KS_WHEN_PROC, 50, 3, KS_F_MAIN | KS_F_PIERCE);
    db_eff(w, r, E_DEATH(50, 0, 1));
db_set_text(w, r, "[主力位]解放,对敌方主力位发起[50%]即死判定,目标[幸运]≥40时成功率减半(演示)。");

    /* 天地乖离开辟之星A:对界,+90%胜率,结界无效;破坏阵地+60%敌罚 */
    r = db_np(w, "天地乖离·开天辟地", KS_NP_WORLD, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_PROC, 100, 9, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 90, -2));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 60, 0));
    {
        E e = E_STATUS(S_CHARGE, 2, -2); e.flag = EF_CHARGE; db_eff(w, r, e);   /* [蓄力]2工序 */
    }
db_set_text(w, r, "[对界][蓄力]解放,给予自身[+90%]胜率补正,并给予敌方全体[+60%]胜率惩罚(演示)。");

    /* 王之军势A:对军,召唤等级=自身的召唤物,生成固有结界(召唤5体) */
    r = db_np(w, "王之军势·阿刻琉斯之壁垒", KS_NP_ARMORY, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_ANY, 30, 18, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 40;             /* 等级 */
        e.cond_arg = 100;         /* 总属性 */
        e.cond_arg2 = TR_HUMAN;   /* 特性 */
        e.target = -2;
        e.desc = "王之军势:召唤等级40、总属性100的召唤物5体(演示为1体)";
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_UP, 20, -1));
db_set_text(w, r, "[对军]解放,召唤[5]体等级=自身、总属性100的召唤物至己方战斗位;己方全体[+20%]胜率(演示1体)。");
    }

    /* 无限剑制A:结界,蓄力后初始工序生成;投影反弹(演示:己方耐力) */
    r = db_np(w, "无限剑制·其身为剑", KS_NP_BOUND, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_BATTLE_START, 80, 18, KS_F_BURST_READY);
    {
        E e1007 = E_WIN(EF_WIN_UP, 25, -1); db_eff(w, r, e1007);
db_set_text(w, r, "[结界]战斗开始时展开[固有结界:无限剑制];己方战斗位全体[+25%]胜率(演示)。");
    }

    /* 妄想心音B:即死,90%判定,幸运≥40减半 */
    r = db_np(w, "妄想心音·交错而鸣", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_B,
              KS_WHEN_PROC, 60, 6, KS_F_MAIN);
    db_eff(w, r, E_DEATH(90, 0, 1));
db_set_text(w, r, "[主力位]解放,对敌方主力位发起[90%]即死判定,目标[幸运]≥40成功率减半(演示)。");

    /* 遗世独立的理想乡A:防御,反击支援,直到战斗结束[无敌],战斗结束回魔 */
    r = db_np(w, "遗世独立的理想乡·星之内海", KS_NP_BOUND, FC_DEFENSE, KS_RANK_A,
              KS_WHEN_ANY, 100, 12, KS_F_COUNTER | KS_F_ASSIST);
    {
        E e1008 = E_STATUS(S_INVINCIBLE, 1, -2); db_eff(w, r, e1008);
        E e1009 = E_MANA(EF_MANA_UP, 100, -2); db_eff(w, r, e1009);
db_set_text(w, r, "[反击][支援]解放,赋予自身[无敌](无视效果)与[+100]魔力(演示)。");
    }

    /* 炽天覆七重圆环B:防御,反击支援,记录并无效化负面效果(演示:抗性) */
    r = db_np(w, "炽天覆七重圆环·花之结界", KS_NP_BOUND, FC_DEFENSE, KS_RANK_B,
              KS_WHEN_ANY, 80, 15, KS_F_COUNTER | KS_F_ASSIST);
    {
        E e1010 = E_STATUS(S_RESUP, 40, -2); db_eff(w, r, e1010);
db_set_text(w, r, "[反击][支援]解放,记录目标攻击性效果并无效化;赋予自身[抗性上升:+40%](演示)。");
    }

    /* 炮击:对军,破坏阵地(演示:敌方-35%胜率) */
    r = db_np(w, "骑士的冲锋·热砂之枪", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_B,
              KS_WHEN_PROC, 60, 9, KS_F_MAIN | KS_F_ENERGY);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 35, 0));
db_set_text(w, r, "[主力位][爆发]主要工序解放,给予敌方战斗位全体[+35%]胜率惩罚(演示)。");

    /* ============ 御主技能(原文近似) ============ */

    /* 宝石魔术B:最终工序,摧毁魔力水晶(演示:魔耗换成胜率) */
    r = db_skill(w, "宝石魔术·魔力的结晶", KS_T_MAGIC, KS_RANK_B, KS_WHEN_PROC, 20, 6, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 0));
db_set_text(w, r, "[支援]最终工序发动,消耗魔力宝石,给予敌方战斗位任一单位[+30%]胜率惩罚(演示)。");

    /* 强筋锻骨B:体术师,抗性+15%基础,常驻三属性+5 */
    r = db_skill(w, "强筋锻骨·铁之躯", KS_T_BLESS, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    {
        E e1011 = E_STATUS(S_RESUP, 15, -2); db_eff(w, r, e1011);
        db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 5, -2));
        db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 5, -2));
        db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 5, -2));
db_set_text(w, r, "常驻:给予自身[抗性上升:+15%]与[筋力][耐久][敏捷]+5常驻补正(演示)。");
    }

    /* 神经衰弱B:咒术师,回合开始对情报目标施加诅咒3 */
    r = db_skill(w, "神经衰弱·扰心", KS_T_MAGIC, KS_RANK_B, KS_WHEN_ANY, 20, 6, 0);
    db_eff(w, r, E_STATUS(S_CURSE, 3, 0));
db_set_text(w, r, "回合开始时,对持有[情报调查]信息的敌方单位赋予[诅咒3](演示)。");

    /* 千里眼(现世视)C:任意战斗,情报(演示:终局+5%) */
    r = db_skill(w, "超越期待·千里眼(现世视)", KS_T_TECHNIQUE, KS_RANK_C, KS_WHEN_ANY, 20, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
db_set_text(w, r, "任意战斗发动,给予自身[+10%]胜率补正(演示:洞悉战局)。");

    /* ============ 礼装(原文效果) ============ */

    /* ================================================================
     * 《空想从者资源库》扩展录入(依据 SP1.17 原文)
     * 基础资源库·保有技能 / 基础资源库·宝具 / 12 扩充包代表条目
     * ================================================================ */

    /* ---- 基础资源库 · 保有技能 ---- */

    /* 军略A:常驻,己方[战术]未被克制时+25%胜率(对军宝具解放时亦+25%,简化) */
    r = db_skill(w, "军略", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 25, -2));
    {
        E e = E_WIN(EF_WIN_UP, 25, -2); e.cond = KC_TACTIC_NOT_PAIRED;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "己方[战术]未被克制时+25%胜率;双方解放[对军宝具]时+25%胜率。[主力位]");

    /* 透化A:常驻,[状态免疫:恐惧&魅惑&混乱];持有胜率惩罚时将其下降60% */
    r = db_skill(w, "透化", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_STATUS(S_STATE_IM, S_FEAR, -2));
    db_eff(w, r, E_STATUS(S_STATE_IM, S_CHARM, -2));
    db_eff(w, r, E_STATUS(S_STATE_IM, S_CONFUSE, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_set_text(w, r, "[主力位]状态免疫:恐惧&魅惑&混乱;自身胜率惩罚总数值下降60%(下限0,演示为+10%胜率近似)。");

    /* 精灵的加护A:常驻,战斗中[幸运]+50补正(无法选幸运作主要属性) */
    r = db_skill(w, "精灵的加护", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 50, -2));
    db_set_text(w, r, "战斗中[幸运]+50属性补正;自身无法选择[幸运]作为主要属性;劣势属性可按幸运基础属性%获胜率(至多+75%,简化)。");

    /* 怪力A:主要工序,[主力位]筋力+50属性补正 */
    r = db_skill(w, "怪力", KS_T_TALENT, KS_RANK_A, KS_WHEN_PROC, 20, 6, KS_F_MAIN);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 50, -2));
    db_set_text(w, r, "[主力位]主要工序发动:[筋力]+50属性补正;战斗属性存在复数[筋力]时翻倍(演示取一倍)。");

    /* 爱之黑痣(狂恋咒)A:常驻,回合开始/干涉时对同灵脉[异性]全体80%[魅惑]判定 */
    r = db_skill(w, "爱之黑痣(狂恋咒)", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    {
        E e = E_STATUS(S_CHARM, 1, 0);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    }
    db_set_text(w, r, "回合开始或[干涉]时,对同灵脉[异性]单位80%[魅惑]判定(出目达最终成功率/2、/10 追加层数,演示为1层);战斗位每存在来源于自身的[魅惑]单位+10%胜率。");

    /* 毒之食馔A:随时,敌方除[支援位]外全体50%[中毒2]判定 */
    r = db_skill(w, "毒之食馔", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 3, 0);
    {
        E e = E_STATUS(S_POISON, 2, 0);
        e.chance = 50; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "对敌方非[支援位]全体50%[中毒2]判定(出目≤最终成功率/2 追加2层;单位因[中毒]退场后判定+20%,简化);目标持有来源于自身的[魅惑]时成功率翻倍。");

    /* 卢恩符文B:轮次开始时发动,本轮次[侦查]/[情报调查]判定+50% */
    r = db_skill(w, "卢恩符文", KS_T_MAGIC, KS_RANK_B, KS_WHEN_ACT, 20, 6, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 50, -2));
    db_set_text(w, r, "轮次开始时宣言:本轮次自身[广泛侦查][定向侦查][情报调查]判定+50%成功率补正;或指定同灵脉单位本轮首次战斗[宣言属性+50]补正(演示为判定补正)。");

    /* ---- 基础资源库 · 宝具 ---- */

    /* 世人啊，冀以锁系神明A:对肃正(视为对界),最终工序+80%胜率,令咒轰击敌主力-100% */
    r = db_np(w, "世人啊，冀以锁系神明", KS_NP_WORLD, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_PROC, 100, 9, KS_F_MAIN | KS_F_BURST_READY);
    db_eff(w, r, E_WIN(EF_WIN_UP, 80, -2));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 100, 1));
    E e1012 = E_STATUS(S_CHARGE, 2, -2); e1012.flag = EF_CHARGE; db_eff(w, r, e1012);
    db_set_text(w, r, "[蓄力]最终工序+80%胜率并无效双方[结界]宝具;追加令咒[轰击]成功时敌主力-100%胜率(每枚令咒判定+30%,演示落地)。");

    /* 干将·莫邪B:随时,[支援]敌非支援单体三属性-15、-30%胜率 */
    r = db_np(w, "干将·莫邪", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_B,
              KS_WHEN_ANY, 20, 3, KS_F_ASSIST);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 15, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 15, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 15, 1));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 1));
    db_set_text(w, r, "[支援]敌非支援单体[筋力][耐久][敏捷]-15属性惩罚;未处魔力不足时再给予[-(当前魔力*2)%]胜率惩罚(至多-90%,演示取30%)。");

    /* 突穿死翔之枪A:对军,[必中]敌非支援全体三属性-30 */
    r = db_np(w, "突穿死翔之枪", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 60, 9, KS_F_PIERCE);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 30, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 30, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 30, 0));
    db_set_text(w, r, "[必中]主要工序解放:敌方战斗位非[支援位]全体除[宝具]外三属性[-30]属性惩罚(演示)。");

    /* 神威车轮A:常驻,[骑乘]自身技能宝具生效时给予敌方单位[感电1] */
    r = db_np(w, "神威车轮", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_PASSIVE, 30, 0, KS_F_RIDE);
    db_eff(w, r, E_STATUS(S_ELECTRIC, 1, 0));
    db_set_text(w, r, "[骑乘]自身发动技能或解放宝具时,受其效果影响的全部单位获得[感电1];主力位时己方全体技能宝具同样生效(演示)。");

    /* 无毁的湖光A:战斗开始时,[支援]全属性+20、自身判定+10% */
    r = db_np(w, "无毁的湖光", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_BATTLE_START, 60, 6, KS_F_ASSIST);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_MAG, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_LUK, 20, -2));
    db_eff(w, r, E_WIN(EF_HIT_UP, 10, -2));
    db_set_text(w, r, "[支援]战斗开始时解放:自身全属性+20属性补正,本场战斗自身发起的所有判定+10%基础成功率。");

    /* 天之锁A:对神(视为对人),[支援]指定目标全属性-5常驻;神性目标[宝具封印1] */
    r = db_np(w, "天之锁", KS_NP_HUMAN, FC_ANTITRAIT, KS_RANK_A,
              KS_WHEN_ANY, 30, 0, KS_F_ASSIST);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN_CONST, A_STR, 5, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN_CONST, A_END, 5, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN_CONST, A_AGI, 5, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN_CONST, A_MAG, 5, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN_CONST, A_LUK, 5, 1));
    {
        E e = E_STATUS(S_NP_SEAL, 1, 1);
        e.cond = KC_TARGET_TRAIT; e.cond_arg = TR_DIVINITY;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[支援]指定当前灵脉任一单位:全属性-5常驻惩罚;目标持[神性]时依其[神性]等级追加[宝具封印/技能封印/撤退FP+1](演示:宝具封印1)。[筋力]≥40可宣言无效化。");

    /* 必灭的黄蔷薇A:常驻,[主力位]敌方主力位[底限穿透:-20%] */
    r = db_np(w, "必灭的黄蔷薇", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PASSIVE, 20, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_FLOOR_PEN, 20, 1));
    db_eff(w, r, E_STATUS(S_NP_SEAL, 2, 1));
    db_set_text(w, r, "[主力位]常驻:给予敌方主力位[底限穿透:-20%];成功[冲锋]时目标[冲锋]属性-20;主要工序时劣势战斗属性-10(惩罚达3项则[宝具封印2],演示部分落地)。");

    /* 风王结界C:常驻,战斗开始时给予自身[回避:技能] */
    r = db_np(w, "风王结界", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_C,
              KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_EVADE, 0, -2));
    db_set_text(w, r, "使自身宝具不暴露效果(除资料分析/真名猜测);战斗开始时给予自身[回避:技能];敌方持有[名称包含[眼]]的技能宝具时本场失效。");

    /* ---- 扩充包 · 各池代表 ---- */

    /* 外典:日轮啊，顺从死亡A 战斗开始时蓄力,初始工序对敌单体70%[即死](神性+20%,幸运60减半) */
    r = db_np(w, "日轮啊，顺从死亡", KS_NP_HUMAN, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_BATTLE_START, 60, 12, KS_F_MAIN | KS_F_PIERCE);
    {
        E e = E_DEATH(70, 0, 1);
        e.cond = KC_NONE;
        db_eff(w, r, e);
    E e1013 = E_STATUS(S_CHARGE, 1, -2); e1013.flag = EF_CHARGE; db_eff(w, r, e1013);
    E e1014 = E_STATUS(S_CHARGE, 1, -2); e1014.flag = EF_CHARGE; db_eff(w, r, e1014); /* EF_CHARGE 蓄力 */
    }
    db_set_text(w, r, "[主力位][必中]战斗开始时解放宣言[蓄力];初始工序对敌战斗位任一单位70%[即死](目标持[神性]+20%最终成功率;幸运≥60减半;演示:幸运≥40减半)。");

    /* 京都:圈境A 常驻,自身初次成为技能目标时取消指定(演示:效果免疫) */
    r = db_skill(w, "圈境", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, -2));
    db_set_text(w, r, "自身不会因[广泛侦查]暴露灵脉单位信息;每场战斗初次成为技能目标时取消其指定(无效化且不返还消耗);A级起对宝具同样生效(演示为效果免疫)。");

    /* 京都:三千世界A 随时,[支援]敌非支援全体三属性-10(骑乘者翻倍) */
    r = db_np(w, "三千世界", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_ANY, 40, 1, KS_F_ASSIST);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 10, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 10, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 10, 0));
    db_set_text(w, r, "[支援]敌非[支援位]全体[筋力][耐久][敏捷]-10属性惩罚;持[骑乘]宝具者数值翻倍(演示不翻倍);非首次解放魔耗-30。");

    /* 苍银:邀至心荡神驰的黄金剧场A 战斗开始时赋予魔术结界(演示:属性+胜率加持) */
    r = db_np(w, "邀至心荡神驰的黄金剧场", KS_NP_BOUND, FC_DEFENSE, KS_RANK_A,
              KS_WHEN_BATTLE_START, 100, 12, KS_F_MAIN);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 20, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    db_set_text(w, r, "[主力位]战斗开始时解放,赋予当前灵脉[魔术结界:黄金剧场](每场限3次随机获六项效果之一:爆发/皇帝特权/属性+25/重骰随机属性/免疫魔力不足惩罚,演示取属性+20与+20%胜率)。");

    /* 北欧:伪·大神宣言A 主要工序,[支援]敌非支援全体[幸运]-50 */
    r = db_np(w, "伪·大神宣言", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 60, 12, KS_F_ASSIST);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_LUK, 50, 0));
    db_set_text(w, r, "[支援]敌方战斗位非[支援位]全体[幸运]-50属性惩罚(己方单位数多于敌方时每多一名+5,演示取基础值);自身处[支援位]解放后立即获得9回转。");

    /* 深池:魅惑的美声A 随时,对同灵脉任一单位80%[魅惑]判定 */
    r = db_skill(w, "魅惑的美声", KS_T_TALENT, KS_RANK_A, KS_WHEN_ANY, 20, 3, 0);
    {
        E e = E_STATUS(S_CHARM, 1, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "指定同灵脉除自身外任一单位,80%[魅惑]判定(异性时出目≤最终成功率/2、/10 追加层数,演示1层);判定上同时视为[类型:魔术]。");

    /* 军阵:军师的指挥A 常驻,己方战斗位全体+5%胜率(御主/从者翻倍) */
    r = db_skill(w, "军师的指挥", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -1));
    db_set_text(w, r, "己方战斗位全体单位+5%胜率补正;对除自身外至多5名[御主][从者]单位翻倍(演示取基础值);持有此补正的单位再受自身技能宝具影响时+5%(至多+25%)。");

    /* 军阵:军神五兵A 常驻,自身除[宝具]外全属性+25常驻补正 */
    r = db_np(w, "军神五兵", KS_NP_HUMAN, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_PASSIVE, 25, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 25, -2));
    db_set_text(w, r, "视为[类型:对人]:自身除[宝具]外全属性+25常驻补正;额外支付60魔力可视为[对军]对敌全体全属性-10(演示常驻部分)。");

    /* 赤幕:无辜的怪物A 常驻,战斗开始对初次相遇单位20%[恐惧];特性[魔性] */
    r = db_skill(w, "无辜的怪物", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    {
        E e = E_STATUS(S_FEAR, 1, 0);
        e.chance = 20; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, -2));
    }
    db_set_text(w, r, "建卡时自选效果(演示取2/4):战斗开始时对初次相遇单位20%[恐惧]判定;除[幸运]外任一属性+10常驻补正;[等级+20]与[特性赋予:魔性]以文本保留。");

    /* 赤幕:穿刺城寨B 行动阶段,敌全体80%全属性-5惩罚与[抗性下降:-10%] */
    r = db_np(w, "穿刺城寨", KS_NP_ARMORY, FC_DEFENSE, KS_RANK_B,
              KS_WHEN_ACT, 50, 12, 0);
    {
        E e = E_ATTR(EF_ATTR_DOWN, A_STR, 5, 0); e.chance = 80; e.chance_neg = 1; db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_DOWN, A_END, 5, 0); e.chance = 80; e.chance_neg = 1; db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_DOWN, A_AGI, 5, 0); e.chance = 80; e.chance_neg = 1; db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_DOWN, A_MAG, 5, 0); e.chance = 80; e.chance_neg = 1; db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_DOWN, A_LUK, 5, 0); e.chance = 80; e.chance_neg = 1; db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_RES_DOWN, 10, 0));
    }
    db_set_text(w, r, "仅当前灵脉持有者为自身时解放,赋予[魔术结界:拷问魔城]:异阵营单位[抗性下降:-10%];战斗开始时对敌全体80%成功率判定,成功给予全属性-5(曾撤退/魂食/杀御主者再判定并抗性下降+10%,演示)。");

    /* 物语:破坏工作A 随时,记录灵脉的战斗摧毁阵地工房,并给予中毒4 */
    r = db_skill(w, "破坏工作", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 40, 9, 0);
    {
        E e = E_STATUS(S_POISON, 4, 0);
        e.chance = 100;
        db_eff(w, r, e);
    E e1015 = E_STATUS(S_POISON, 4, 0); e1015.flag = EF_RANDOM_GIVE; db_eff(w, r, e1015); /* EF_RANDOM_GIVE 随机状态赋予 */
    }
    db_set_text(w, r, "仅可在其他阵营持[阵地][魔术工房]的灵脉发动,本轮次结束后记录;该灵脉战斗开始时摧毁[阵地][魔术工房]并限制敌方参战数;自身不在灵脉时给予随机4名御主[中毒4](演示)。");

    /* 辉拓:暴风雨的航海家A 常驻,[主力位]冲锋失败可再冲锋;战术未被克制时+胜率 */
    r = db_skill(w, "暴风雨的航海家", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -1));
    db_set_text(w, r, "[主力位]每场战斗限一次,自身[冲锋]失败时允许换属性再[冲锋];己方[战术]未被克制时:己方非支援位存在单位+20等级补正,每次发动技能/解放宝具后己方其他单位+5%胜率(至多4次,演示取基础胜率)。");

    /* 少女:吻魔B 随时,[补魔]时额外+30魔力供给 */
    r = db_skill(w, "吻魔", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_ANY, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 30, -2));
    db_set_text(w, r, "进行[补魔]时自身额外获得30魔力供给(每轮次一次);[交流]中可对同灵脉单位转移20魔力(持有技能信息时10,演示为补魔增量)。");

    /* 少女:多元重奏饱和炮击A 主要工序,蓄力后最终工序敌非支援全体-50%胜率 */
    r = db_np(w, "多元重奏饱和炮击", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 60, 12, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 50, 0));
    E e1016 = E_STATUS(S_CHARGE, 2, -2); e1016.flag = EF_CHARGE; db_eff(w, r, e1016);
    db_set_text(w, r, "解放方式1:额外支付40魔力,[蓄力];最终工序对敌非[支援位]全体-50%胜率惩罚,并立即获得6回转(演示)。方式2以自身属性与魔力为代价的回路供给以文本保留。");

    /* 战线:美之显现A 随时,交流中群体75%[魅惑]判定 */
    r = db_skill(w, "美之显现", KS_T_CROWN, KS_RANK_A, KS_WHEN_ANY, 10, 6, 0);
    {
        E e = E_STATUS(S_CHARM, 1, 0);
        e.chance = 75; e.chance_neg = 1;
        db_eff(w, r, e);
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -1));
    }
    db_set_text(w, r, "[交流]中对同灵脉除自身外任意数量单位75%[魅惑]判定(出目≤最终成功率/2、个位数≤/10 各追加+10%胜率,演示1层);本回合受魅惑单位处己方战斗位时+5%*层数胜率。");

    /* 战线:王之号炮A 随时,[支援]敌随机单位4次-15%胜率并追加随机状态 */
    r = db_np(w, "王之号炮", KS_NP_ARMORY, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_ANY, 80, 9, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 15, 0));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 15, 0));
    {
        E e = E_STATUS(S_ELECTRIC, 2, 0);
        db_eff(w, r, e);
    E e1017 = E_STATUS(S_CONFUSE, 2, 0); e1017.flag = EF_RANDOM_GIVE; db_eff(w, r, e1017);
    }
    db_set_text(w, r, "[支援]仅战斗中解放:下一工序开始时对敌非[支援位]随机单位重复4次-15%胜率惩罚,并随机附加[感电2/诅咒2/灼伤1/冻结1/疲惫1/中毒1]等一项状态(演示乘2行+感电);非[主力位]宝具可无视战斗位自肃解放后破弃。");

    /* 盈月:童子切安纲B 常驻,对[神性]-5%/对[魔性]-10%胜率惩罚 */
    r = db_np(w, "童子切安纲", KS_NP_HUMAN, FC_ANTITRAIT, KS_RANK_B,
              KS_WHEN_PASSIVE, 10, 0, 0);
    {
        E e = E_WIN(EF_WIN_DOWN, 5, 1); e.cond = KC_TARGET_TRAIT; e.cond_arg = TR_DIVINITY;
        db_eff(w, r, e);
        e = E_WIN(EF_WIN_DOWN, 10, 1); e.cond = KC_TARGET_TRAIT; e.cond_arg = TR_DEMONIC;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "对神秘(视为对人):敌方战斗位存在持[神性][魔性]单位时自身+20%胜率(演示文本);非[支援位]时始终给予持[神性]单位-5%、持[魔性]单位-10%胜率惩罚;首次受自身效果影响时惩罚翻倍并给等量[抗性下降]。");

    /* ================================================================
     * 《空想御主资源库》扩展录入(依据 SP1.17 原文)
     * ================================================================ */

    /* 生命秘术A:随时,对同灵脉魂食者追加60魔力供给(演示);或献祭回路换魔供 */
    r = db_skill(w, "生命秘术", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 6, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 60, -2));
    db_set_text(w, r, "发动方式1:消耗行动阶段指定[回路]不为0的御主,令其回路固定为0并给予自身[目标魔力池上限+回路*2]魔供;方式2:同灵脉单位[魂食]成功时人流量-1并给予+60魔供(演示60,视为[遮蔽魂食])。");

    /* 天人合一A:初始/主要工序,自身任一属性+15(同项至多+30) */
    r = db_skill(w, "天人合一", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PROC, 10, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 15, -2));
    db_set_text(w, r, "仅初始/主要工序发动:自身[筋力][耐久][敏捷]任一项+15属性补正(同项至多+30,演示筋力);一场战斗至多发动6次;[交流]中可令从者以90魔力使数值上限永久翻倍。");

    /* 活杀自在A:[反击],随机属性50+属性差%判定,成功无效化敌方技艺/兵器技能 */
    r = db_skill(w, "活杀自在", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 10, 1, KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, -2));
    db_set_text(w, r, "[反击]敌方战斗位存在等级不高于自身的[技艺/兵器]技能发动时,随机[筋力/耐久/敏捷]以50+属性差%判定,成功令该技能发动无效化且不返还消耗(演示为效果免疫);EX时不限等级。");

    /* 二天一流A:常驻,自身技能宝具对敌造成效果时给予-20%胜率惩罚 */
    r = db_skill(w, "二天一流", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 0));
    db_set_text(w, r, "自身发动技能对敌方单位造成效果时,给予受影响任一单位-20%胜率惩罚(每次发动一次;礼装/科技造物不触发);EX时追加下次判定+10%最终成功率或自身-10%最终惩罚。");

    /* 伪·直死之魔眼A:战斗开始时,[支援]敌方主力位[抗性下降:-25%] */
    r = db_skill(w, "伪·直死之魔眼", KS_T_TALENT, KS_RANK_A, KS_WHEN_BATTLE_START, 20, 9, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_RES_DOWN, 25, 1));
    db_set_text(w, r, "[支援]战斗开始时给予敌方主力位[抗性下降:-25%];自身对其发起负面判定时,自身[筋力][敏捷][耐久]每有一项高于目标,判定+10%最终成功率(演示抗性部分)。");

    /* 对胜利的确信A:常驻,战斗中始终+40%胜率补正 */
    r = db_skill(w, "对胜利的确信", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 40, -2));
    db_set_text(w, r, "战斗中始终+50%胜率补正(演示取40%);自阵营战斗胜利且自身未参战/处[支援位]时+50魔力供给;自阵营首次战败后数值减半。");

    /* 猩红收割A:随时,敌单体任一属性-30属性惩罚 */
    r = db_skill(w, "猩红收割", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 20, 1, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 30, 1));
    db_set_text(w, r, "给予敌方战斗位任一单位除[宝具]外任一属性-30属性惩罚(演示[耐久]);每次发动下次发动额外消耗10魔力(可叠加);回合结束未[魂食]且自阵营未击杀御主时自身全属性-10常驻惩罚。");

    /* 资源补给A:常驻,[支援][储备0/20],每2储备+5%胜率(演示10%) */
    r = db_skill(w, "资源补给", KS_T_WEAPON, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_set_text(w, r, "[支援][储备0/20]建卡时储备8;每持有2储备+5%胜率补正;轮次结束80%判定+1储备(出目≤最终成功率/2、/4 再+1);可发动将储备转化为魔力或工房材料(演示胜率部分)。");

    /* ================================================================
     * 《空想礼装资源书》扩展录入(依据 SP1.17 原文)
     * ================================================================ */

    /* 以太光纤:随时,[交流]中80%判定获取目标全部技能/宝具信息 */
    r = db_item(w, "以太光纤", KS_RANK_C, KS_WHEN_ANY, 10, 3, KS_F_ASSIST, 1, 0, 0);
    {
        E e = E_WIN(EF_INFO, 1, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[交流]中发动:对同灵脉任一单位80%负面判定,成功获得其全部[技能信息][宝具信息];出目<最终成功率/2时再以≤C级模板获取其一项[技艺/魔术]技能(回合结束时失去);目标为从者时追加50%判定,失败给予自身[晕眩5]。");

    /* 灰锭:随时,[支援][储备1/1]敌主力任一属性-5(魔性目标-20) */
    r = db_item(w, "灰锭", KS_RANK_C, KS_WHEN_ANY, 0, 0, KS_F_ASSIST, 1, 1, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 5, 1));
    db_set_text(w, r, "[储备1/1][支援]消耗储备:对方主力位除[宝具]外任一属性-5属性惩罚;目标持[魔性]时惩罚变为-20(演示取基础值)。");

    /* 抹大拉的圣骸布:初始工序,[唯一][支援]敌主力60%[迟滞1]判定 */
    r = db_item(w, "抹大拉的圣骸布", KS_RANK_B, KS_WHEN_PROC, 0, 3, KS_F_UNIQUE | KS_F_ASSIST, 1, 0, 1);
    {
        E e = E_STATUS(S_LAG, 1, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[唯一][支援]仅能指定[男性]单位:对方主力位以100-目标等级%成功率进行[迟滞]判定,成功给予[迟滞1](演示60%)。");

    /* 守护圣典:常驻,[唯一]自身[抗性上升:+10%](对魔性时+30%) */
    r = db_item(w, "守护圣典", KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, KS_F_UNIQUE, 1, 0, 1);
    db_eff(w, r, E_WIN(EF_RES_UP, 10, -2));
    db_set_text(w, r, "[唯一]常驻:[抗性上升:+10%];对方战斗位存在[魔性]单位且己方不存在时,此抗性上升变为+30%(演示取基础值)。");

    /* M1式加兰德射手步枪:主要工序,敌单体[100-敏捷]%判定成功三属性-20 */
    r = db_tech(w, "M1式加兰德射手步枪", KS_RANK_C, KS_WHEN_PROC, 0, 3, KS_F_ASSIST, 0);
    {
        E e = E_ATTR(EF_ATTR_DOWN, A_STR, 20, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[支援位]主要工序:对敌战斗位任一单位进行[100-目标敏捷]%负面判定(演示60%),成功给予[筋力/耐久/敏捷]任一项-20属性惩罚;目标为御主时战斗结束50-耐久/2%判定转化为常驻惩罚。");

    /* 卡利科M950A冲锋手枪:随时,[储备100/100]敌单体判定成功-10%胜率与[迟滞1] */
    r = db_tech(w, "卡利科M950A冲锋手枪", KS_RANK_C, KS_WHEN_ANY, 0, 3, KS_F_ASSIST, 100);
    {
        E e = E_WIN(EF_WIN_DOWN, 10, 1);
        e.chance = 40; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_STATUS(S_LAG, 1, 1);
        e.chance = 40; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[储备100/100]消耗任意储备:对敌非[支援位]单位以2*消耗-目标敏捷%判定(演示40%),成功给予-10%胜率惩罚与[迟滞1];目标为从者时判定成功率减半。");

    /* ================================================================
     * 全量录入 批次1:《空想从者资源库》基础资源库(职阶技能+保有技能+宝具)
     * ================================================================ */

    /* ---- 基础资源库 · 职阶技能(补全) ---- */

    /* 狂化EX:常驻,三属性+15常驻;自选1项追加效果(演示:负面效果触发补正+免疫) */
    r = db_skill(w, "狂化EX", KS_T_CLASS, KS_RANK_EX, KS_WHEN_PASSIVE, 15, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 15, -2));
    db_eff(w, r, E_STATUS(S_STATE_IM, S_FEAR, -2));
    db_eff(w, r, E_STATUS(S_STATE_IM, S_CHARM, -2));
    db_set_text(w, r, "[筋力][耐久][敏捷]+15常驻补正;建卡时自选一项追加(1不可撤退+令咒加成/2拒绝介入+敌单位加成/3受负面+5至多30/4未参战轮-20魔力/5知悉[恶]阵营并袭击时补正三倍,演示取免疫恐惧魅惑)。");

    /* 真名看破A:常驻,遭遇从者即知悉职阶与真名;A级真名已成功者-10%胜率 */
    r = db_skill(w, "真名看破", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 1));
    db_set_text(w, r, "遭遇任意[从者]时立即获悉其[职阶][真名];B级起[真名猜测]无法默认失败;A级起被成功[真名猜测]的单位处对立战斗位时判定成功率-10%、始终-10%胜率(演示)。");

    /* 神明裁决A:常驻,允许消耗令咒对从者造成令咒效果 */
    r = db_skill(w, "神明裁决", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 1));
    db_set_text(w, r, "允许消耗自身令咒,对所处灵脉任一[从者]造成令咒效果(无法强制命令);B级起每名从者限2次无消耗;A级起宣言将令咒效果变为[-30%]胜率惩罚(演示)。");

    /* 伪装工作A:常驻,记录他职阶并以E级模板获取其职阶技能 */
    r = db_skill(w, "伪装工作", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 5, -2));
    db_set_text(w, r, "建卡时记录任一本职阶外职阶(重复4次);每回合一次宣言将初始属性改为该职阶并以E级模板获取其职阶技能(回合结束失去);同职阶多记录可提升1级。演示取属性近似。");

    /* 复仇者A:常驻,受负面效果+5魔供;敌方每单位给予主要属性+25(至多100) */
    r = db_skill(w, "复仇者", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 25, -2));
    db_set_text(w, r, "每当自身受到负面效果时获得5魔力供给;战斗中敌方战斗位每存在一名单位,给予自身[主要属性]+25属性补正(至多+100,演示取筋力)。");

    /* 忘却补正A:常驻,自身作为袭击方+25%胜率;复仇标记机制(文本) */
    r = db_skill(w, "忘却补正", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 25, -2));
    db_set_text(w, r, "被袭击时可宣言袭击并视为[袭击方];作为[袭击方]参与战斗时+25%胜率;自身受到的[资料分析]默认失败;战败时给予敌方全体[复仇1]标记(每层+5主要属性,至多50,文本保留)。");

    /* 自我回复A:常驻,轮次结束时+50魔力供给 */
    r = db_skill(w, "自我回复", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 50, -2));
    db_set_text(w, r, "每轮次结束时给予自身50魔力供给(战败时改为一半数值)。");

    /* 己阵防御A:战斗开始时,己方战斗位至多5名单位获得[保护] */
    r = db_skill(w, "己阵防御", KS_T_CLASS, KS_RANK_A, KS_WHEN_BATTLE_START, 0, 6, 0);
    db_eff(w, r, E_WIN(EF_PROTECT, 0, -1));
    db_set_text(w, r, "战斗开始时给予己方战斗位除自身外至多5名单位[保护];记录经[保护]转移给自身的惩罚,减少至多40自身所受惩罚(演示保护给予)。");

    /* ---- 基础资源库 · 保有技能(补全) ---- */

    /* 藏知的司书A:随时,[支援]幸运%判定成功获取任一曾持有的技艺/魔术技能 */
    r = db_skill(w, "藏知的司书", KS_T_TALENT, KS_RANK_A, KS_WHEN_ANY, 0, 6, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_INFO, 1, -2));
    db_set_text(w, r, "[支援]宣言任一自身曾持有的[技艺/魔术]技能,以[幸运]%判定(宣言技能等级更高时成功率至多90%),成功则以先前模板获取该技能(回合结束时失去);EX时判定为幸运*2。演示为信息获取。");

    /* 宗和的心得A:常驻,[主力位]自身+20%胜率与+5%底限;敌方主力-10%底限穿透 */
    r = db_skill(w, "宗和的心得", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    db_eff(w, r, E_WIN(EF_FLOOR_UP, 5, -2));
    db_eff(w, r, E_WIN(EF_FLOOR_PEN, 10, 1));
    db_set_text(w, r, "[主力位]+20%胜率补正与+5%底限胜率;给予敌方主力位[-10%]底限穿透;敌方底限胜率≤0%时,敌方战斗位视为初次与自身敌对战斗(加符时底限穿透+10%,文本)。");

    /* 专精百般A:常驻,每轮次开始时获取任一等级低于自身的[技艺]技能 */
    r = db_skill(w, "专精百般", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 10, -2));
    db_set_text(w, r, "每轮次开始时获取任一等级低于自身的[技艺]技能(额外技能,轮次结束失去,回转转移至新技能);EX时可获取B级及以下[魔术/技艺]技能。演示取技艺补正近似。");

    /* 骑士的武略A:初始工序,自身属性+25;主要工序暗指定属性+50 */
    r = db_skill(w, "骑士的武略", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_BATTLE_START, 0, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 50, -2));
    db_set_text(w, r, "建卡时获得除[宝具]外任一属性+25额外基础属性(战斗开始/初始工序生效,不暴露);发动后暗指定一项属性,主要工序开始时+50属性补正(EX可再指定一项+20,演示取筋力与敏捷)。");

    /* 高速神言A:常驻,[反击]回转≤6的魔术技能优先结算并可额外发动一次 */
    r = db_skill(w, "高速神言", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_RECAST, 6, -2));
    db_set_text(w, r, "[反击]令自身[回转≤6]的[魔术]技能先于同级结算,且允许其在结算时机额外发动一次(每工序限一次、不产生额外回转;发动礼装无法生效)。演示为回转+6近似。");

    /* 重整旗鼓A:主要工序,[主力位]己方全部单位撤退所需FP-1 */
    r = db_skill(w, "重整旗鼓", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PROC, 0, 6, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_FP_UP, 1, -1));
    db_set_text(w, r, "[主力位]仅自身进行[机动]或[介入]时发动:本场战斗己方战斗位所有单位撤退所需FP-1;己方撤退后对当前灵脉进行一次不消耗行动力的[干涉](演示FP补正)。");

    /* 金羊毛C:随时,行动阶段+10魔力供给;或强化召唤物(文本) */
    r = db_skill(w, "金羊毛", KS_T_WEAPON, KS_RANK_C, KS_WHEN_ANY, 0, 3, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 10, -2));
    db_set_text(w, r, "方式1:行动阶段发动,给予自身+10魔力供给(同灵脉有单位参战的回合失效);方式2:[反击][支援]同灵脉单位解放[召唤]宝具时,令其召唤物+10等级、全属性+10并获[龙种][巨大]特性(需额外1仆役位,演示取方式1)。");

    /* 回归初始A:随时,[支援]恢复全部回转并清除自身补正惩罚 */
    r = db_skill(w, "回归初始", KS_T_BLESS, KS_RANK_A, KS_WHEN_ANY, 0, 999, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_RECAST, 999, -2));
    db_set_text(w, r, "[支援]令自身魔力池修正至0(按差值给魔供或消耗),恢复所有其他保有技能全部回转,清除自身全部补正与惩罚;此技能回转不受任何效果变化(演示回转+999)。");

    /* 避矢的加护A:常驻,[反击]等级低于自身的[兵器]技能效果无效化 */
    r = db_skill(w, "避矢的加护", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, -2));
    db_set_text(w, r, "[反击]仅建卡获取:成为等级小于自身的[兵器]技能目标且无[必中]时,令其效果无效且不返还消耗;等级不小于自身时以70%判定(演示为效果免疫);敌方礼装效果视为[兵器]技能。");

    /* 收藏家C:常驻,资料分析+20%;每件额外宝具+5%胜率 */
    r = db_skill(w, "收藏家", KS_T_CROWN, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 20, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    db_set_text(w, r, "自身发起的[资料分析]判定+20%成功率;每持有一件/曾经持有一件不同的额外宝具,自身+5%胜率;每场战斗一次自身解放额外宝具时+10%胜率(演示)。");

    /* 魔术(神代魔术)B:随时,最终工序敌主力-30%胜率;[支援]战斗开始己方抗性+30% */
    r = db_skill(w, "魔术(神代魔术)", KS_T_MAGIC, KS_RANK_B, KS_WHEN_ANY, 20, 6, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 1));
    db_eff(w, r, E_WIN(EF_RES_UP, 30, -1));
    E e1018 = E_STATUS(S_CHARGE, 3, -2); e1018.flag = EF_CHARGE; db_eff(w, r, e1018); /* EF_CHARGE 蓄力 */
    db_set_text(w, r, "依等级存在多种发动方式(A轮次开始无消耗机动/B行动阶段给御主三属性+20常驻/C最终工序敌主力-30%胜率(宣言+20魔力变-60%)/D战斗开始己方抗性+30%/E蓄力后敌全体-20%,演示取C与D)。");

    /* 石化之魔眼(封印)A:随时,[支援]替换为同等级石化之魔眼,下次判定+30% */
    r = db_skill(w, "石化之魔眼(封印)", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 3, KS_F_ASSIST);
    {
        E e = E_STATUS(S_STONE, 1, 1);
        e.chance = 75; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[支援]将此技能替换为同等级[石化之魔眼]并令其下一次判定+30%成功率(目标持有技能信息时减半),回合结束时还原;判定上视为存在[封印](演示石化判定75%)。");

    /* 精神污染A:常驻,状态抵抗魅惑/恐惧/混乱;自身魅惑系判定+50% */
    r = db_skill(w, "精神污染", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_STATUS(S_STATE_RES, S_FEAR, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CHARM, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CONFUSE, -2));
    db_eff(w, r, E_WIN(EF_HIT_UP, 50, -2));
    db_set_text(w, r, "给予自身[状态抵抗:魅惑&恐惧&混乱];不持有等级不小于自身[对魔力]的单位对自身[真名猜测]默认失败;自身发起的[魅惑/恐惧/混乱]判定+50%成功率;EX时改为状态免疫(演示)。");

    /* ---- 基础资源库 · 宝具(补全) ---- */

    /* 誓约胜利之剑·MorganA:随时,[主力位]解放+40%胜率;轰击判定成功额外+50% */
    r = db_np(w, "誓约胜利之剑·Morgan", KS_NP_CASTLE, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_ANY, 80, 1, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 40, -2));
    {
        E e = E_WIN(EF_WIN_UP, 50, -2); e.cond = KC_HAS_CS;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[主力位]解放时+40%胜率;宣言消耗至少一枚令咒或额外支付120魔力进行[轰击](70%判定),成功额外+50%胜率;加符时宣言移除加符并支付40魔力视为令咒[轰击](演示含令咒条件行)。");

    /* 禁忌狂宴B:战斗开始时解放,生成固有结界;结界内技能魔耗-10但[中毒2];工序内可对敌全体晕眩判定 */
    r = db_np(w, "禁忌狂宴", KS_NP_HUMAN, FC_DEFENSE, KS_RANK_B,
              KS_WHEN_BATTLE_START, 60, 9, 0);
    {
        E e = E_STATUS(S_POISON, 2, 0);
        e.chance = 100;
        db_eff(w, r, e);
        e = E_STATUS(S_STUN, 2, 0);
        e.chance = 25; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "战斗开始时解放,生成[固有结界:禁忌岛屿](宽5,魔量=灵脉魔量):除自身外单位技能发动魔耗-10,但发动时给予自身[中毒2](持同等级[对魔力]可免除);每工序限一次消耗40魔力对敌全体25%[晕眩]判定(每异常+25%,演示晕眩并简化中毒赋予)。");

    /* 射杀百头A:随时,指定属性给敌惩罚(演示以对城方式:敌全体三属性-50) */
    r = db_np(w, "射杀百头", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_ANY, 40, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 50, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 50, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 50, 0));
    db_set_text(w, r, "三种解放方式:1)[对人]指定[筋力/耐久/敏捷]任一属性,敌非支援单体-50惩罚;2)B级起[对军]额外40魔力,敌非支援全体-50;3)A级起[对城]额外80魔力[蓄力],最终工序敌全体三属性-50(敌方主力持[巨大]时改九次全属性-5)。演示取对城最终效果。");

    /* 遥远的蹂躏制霸A:主要工序,[蓄力]最终工序敌非支援全体敏捷差判定,属性惩罚+感电激荡 */
    r = db_np(w, "遥远的蹂躏制霸", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 70, 9, KS_F_BURST_READY);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 5, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 5, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 5, 0));
    {
        E e = E_STATUS(S_ELECTRIC, 3, 0);
        e.chance = 30; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "主要工序解放宣言[蓄力];最终工序对敌非[支援位]全体各6次[100+等级差-目标敏捷]%负面判定,每次成功给三属性-5惩罚;随后以[战斗位感电层数*10+等级差]%判定给予[感电3]并[激荡](演示取5点惩罚与感电判定);[胜利]后自身+10等级补正。");

    /* 屠戮不死之刃A:随时,敌主力敏捷差判定成功[耐久]-40(战斗结束转一半常驻) */
    r = db_np(w, "屠戮不死之刃", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_ANY, 40, 3, 0);
    {
        E e = E_ATTR(EF_ATTR_DOWN, A_END, 40, 1);
        e.chance = 40; e.chance_neg = 1;
        db_eff(w, r, e);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN_CONST, A_AGI, 20, 1));
    }
    db_set_text(w, r, "指定敌方[主力位]:40+与目标敏捷差*2%负面判定(无视[抗性上升]),成功给予[耐久]-40属性惩罚,战斗结束时转为一半常驻惩罚(此宝具被封印/破弃/自身退场前持续);EX时初次遭遇判定翻倍并追加[筋力][敏捷]-20常驻。");

    /* 骑士不死于徒手A:常驻,[主力位]夺取对人宝具;礼装/科技造物视为[对人]宝具 */
    r = db_np(w, "骑士不死于徒手", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, -2));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 5, 0));
    db_set_text(w, r, "[主力位]其他单位以自身为目标解放[对人]宝具时,立即获取那件宝具并无视时机/回转解放(解放后失去);同灵脉单位许可时可解放其已知[对人]宝具;支付20魔力可无需礼装发动次数发动礼装;自身[科技造物]生效时给予目标-5%胜率。演示取武具化属性近似。");

    /* 必胜黄金之剑B:常驻/战斗开始时,[主力位]战斗中每次宝具解放+20等级(至多60) */
    r = db_np(w, "必胜黄金之剑", KS_NP_HUMAN, FC_BUFF, KS_RANK_B,
              KS_WHEN_PASSIVE, 10, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_set_text(w, r, "[主力位]战斗中每有一次宝具解放,给予自身+20等级补正(至多60,战斗结束移除并获等级差/10回转);战斗开始时支付40魔力解放:本场战斗始终+双方主力位等级差/2%胜率(演示取胜率简化);[秩序善]且有条件时可宣言[爆发]使敌主力底限胜率固定为0。");

    /* 不毁的极圣A:战斗开始时,[主力位]+20等级与任一项属性+50 */
    r = db_np(w, "不毁的极圣", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_BATTLE_START, 60, 9, KS_F_MAIN);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 50, -2));
    db_set_text(w, r, "[主力位]战斗开始时解放:自身+20等级补正,任一项属性+50或任两项+25(战斗结束移除;属性补正≤30时立即获6回转);特定条件下可将自身魔力耗至下限换取底限胜率(每30魔力+5%,文本)。");

    /* 集结于圣旗之下怒吼吧B:常驻,三属性+20常驻;死斗+20%胜率;不可撤退 */
    r = db_np(w, "集结于圣旗之下怒吼吧", KS_NP_HUMAN, FC_BUFF, KS_RANK_B,
              KS_WHEN_PASSIVE, 30, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 20, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_set_text(w, r, "[筋力][耐久][敏捷]+20常驻补正;自身无法撤退;宣言[死斗]时额外+20%胜率(演示计10%);主力位时己方其他单位全属性+10(演示);战斗位每存在一名自阵营单位+5%胜率。");

    /* 骑英之缰绳A:随时,[骑乘]蓄力后敌非支援全体-40%胜率;或强化召唤物回避 */
    r = db_np(w, "骑英之缰绳", KS_NP_ARMORY, FC_BUFF, KS_RANK_A,
              KS_WHEN_ANY, 60, 9, KS_F_RIDE);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 0));
    E e1019 = E_STATUS(S_CHARGE, 1, -2); e1019.flag = EF_CHARGE; db_eff(w, r, e1019);
    db_set_text(w, r, "[骑乘]方式1:主要工序解放[蓄力],蓄力期间自身与指定召唤物获[回避],最终工序敌非[支援位]全体-40%胜率;方式2:随时解放,己方召唤物全属性+20与[回避](辅助位翻倍)。演示取方式1。");

    /* 不识爱的悲哀之龙啊A:随时,召唤等级70/总属性280/[巨大]的龙种召唤物(3仆役位) */
    r = db_np(w, "不识爱的悲哀之龙啊", KS_NP_ARMORY, FC_SUMMON, KS_RANK_A,
              KS_WHEN_ANY, 60, 9, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 70; e.cond_arg = 280; e.cond_arg2 = TR_DRAGON;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "不识爱的悲哀之龙啊:等级70 总属性280(3仆役位)";
        db_eff(w, r, e);
    }
    db_set_text(w, r, "召唤等级70、总属性280、[巨大][独特]的召唤物(需3仆役位;建卡决定[特性赋予:神性/魔性/龙种/猛兽/魔兽/构装体]其一,演示取龙种);其处灵脉时若灵脉无主/自身为主,回合结束摧毁[阵地][工房][神殿];其处于战斗位时给予敌非支援全体[-(等级/2)%]胜率惩罚(文本)。");

    /* 妄想幻象A:随时,召唤等级=自身的召唤物(全属性5,演示40/30) */
    r = db_np(w, "妄想幻象", KS_NP_HUMAN, FC_SUMMON, KS_RANK_A,
              KS_WHEN_ANY, 20, 6, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 40; e.cond_arg = 30; e.cond_arg2 = TR_HUMAN;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "妄想幻象:等级=自身(演示40) 全属性5";
        db_eff(w, r, e);
    }
    db_set_text(w, r, "召唤等级=自身等级、除[宝具]外全属性5的[召唤物](保有本职职阶技能但仅E级模板生效;礼装次数至多2);可将自身该属性数值的常驻惩罚转移为召唤物常驻补正;至多同时5体;与御主存在[圣杯契约](演示40级单体)。");

    /* 螺湮城教本A:行动阶段,召唤等级60/总属性140;敌主力-等级/2%胜率惩罚 */
    r = db_np(w, "螺湮城教本", KS_NP_ARMORY, FC_SUMMON, KS_RANK_A,
              KS_WHEN_ACT, 40, 6, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 60; e.cond_arg = 140; e.cond_arg2 = TR_GOLEM;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "螺湮城教本:等级60 总属性140(聚合)";
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 1));
    }
    db_set_text(w, r, "行动阶段解放:召唤等级60、总属性140、[聚合]的召唤物(特性赋予自选,演示构装体);其存在时敌方主力位受到[-(等级/2)%]胜率惩罚(不可叠加,演示-30%);[聚合]后获[巨大]并占用+1战斗位(分裂/加符联动文本)。");

    /* 自我封印·暗黑神殿A:随时,[反击][支援]宣言技能[封印3] */
    r = db_np(w, "自我封印·暗黑神殿", KS_NP_HUMAN, FC_STATUS, KS_RANK_A,
              KS_WHEN_ANY, 20, 6, KS_F_COUNTER | KS_F_ASSIST);
    {
        E e = E_STATUS(S_SKILL_SEAL, 3, 1);
        e.chance = 90; e.chance_neg = 1;
        db_eff(w, r, e);
    db_eff(w, r, E_STATUS(S_SKILL_SEAL, 1, 1));
    }
    db_set_text(w, r, "[反击][支援]方式1:宣言同灵脉单位任一等级不高于此宝具的非[职阶]已知技能,给予[封印3],目标三属性合计≤45时再转移30魔力;方式2:给予[封印1]并立即获3回转;方式3:夜间对持情报单位[交流],轮次结束时90%判定转移至多30魔力(演示技能封印)。");

    /* 破魔的红蔷薇A:常驻,敌方主力常驻宝具效果无效化,非对城/对界宝具补正为0 */
    r = db_np(w, "破魔的红蔷薇", KS_NP_HUMAN, FC_STATUS, KS_RANK_A,
              KS_WHEN_PASSIVE, 20, 0, 0);
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, 1));
    db_set_text(w, r, "令敌方主力位等级低于此宝具的常驻宝具对其造成的影响无效化;令敌方主力位非[对城/对界]宝具的属性补正与胜率补正始终为0(溢出部分支付等量魔力);行动阶段可宣言破弃此宝具(演示效果免疫)。");

    /* 他者封印·鲜血神殿A:行动阶段,赋予结界:人流量-1并+60魔力供给 */
    r = db_np(w, "他者封印·鲜血神殿", KS_NP_ARMORY, FC_SUPPLY, KS_RANK_A,
              KS_WHEN_ACT, 60, 9, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 60, -2));
    db_set_text(w, r, "仅当自身为当前灵脉持有者且人流量≠0时解放:赋予[魔术结界:鲜血神殿](灵脉改[永夜]、魔力量变0、遮蔽魂食信息隐藏);自身处该灵脉时每回合结束人流量-1并+60魔供(已存在结界时人流量归0并+120,演示60);B级起视作成功[遮蔽魂食]。");

    /* 风王铁锤C:最终工序,[主力位]按胜率差给自身补正并立即决胜 */
    r = db_np(w, "风王铁锤", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_C,
              KS_WHEN_PROC, 20, 6, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 50, -2));
    db_eff(w, r, E_WIN(EF_FLOOR_PEN, 20, 1));
    db_set_text(w, r, "[主力位]最终工序解放:根据优势方与劣势方最终胜率差值给予自身[100-差值]%胜率补正,随后给予自身[-20%]底限穿透并立即进行决胜检定(与[风王结界C]共用宝具栏位,演示取50%补正与底限穿透)。");

    /* 不为一己之荣光B:常驻,Saber/Lancer/Archer专用,多从者卡面切换 */
    r = db_np(w, "不为一己之荣光", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_B,
              KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, -2));
    db_set_text(w, r, "仅[Saber][Lancer][Archer]可持有:建卡时额外持有多张从者卡面(真名/建卡资源各自独立,共用魔力池);每轮次开始可切换当前卡面(常驻效果保留在对应卡面);持C级以上[狂化]时此宝具[封印]。演示取属性近似。");

    /* 王之财宝A:战斗开始时,[主力位]蓄力记录,可随时支付30魔力获取低等级宝具解放 */
    r = db_np(w, "王之财宝", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_A,
              KS_WHEN_BATTLE_START, 30, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    E e1020 = E_STATUS(S_CHARGE, 2, -2); e1020.flag = EF_CHARGE; db_eff(w, r, e1020);
    E e1021 = E_STATUS(S_CHARGE, 3, -2); e1021.flag = EF_CHARGE; db_eff(w, r, e1021); /* EF_CHARGE 蓄力 */
    db_set_text(w, r, "[主力位]战斗开始时解放宣言[蓄力];蓄力期间支付30魔力,从[进攻&防御&增益&状态&即死]面向中获取等级低于自身的宝具立即解放(解放后/战斗结束失去;无法获取[结界]与[常驻]宝具);转回自身保有宝具或战斗结束时结束蓄力并失去记录回转(演示胜率近似)。");

    /* 十二试炼B:常驻,[主力位]等级不高于自身的技能宝具效果无效并记录试炼 */
    r = db_np(w, "十二试炼", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_B,
              KS_WHEN_PASSIVE, 20, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, -2));
    db_set_text(w, r, "[主力位]成为来源不为自身且等级不高于此宝具的技能/宝具效果对象时,若自身全属性总和不低于目标,令其效果无效(不返还消耗)并记录[试炼1];战败决胜时可支付各FP撤退后重骰(等于自身底限胜率);每记录[试炼1]产生10魔力消耗,达[试炼12]时破除(演示效果免疫)。");

    /* 万符必应破戒C:随时,[反击][支援]无效化[魔术]技能发动并返还魔耗 */
    r = db_np(w, "万符必应破戒", KS_NP_HUMAN, FC_ANTITRAIT, KS_RANK_C,
              KS_WHEN_ANY, 20, 0, KS_F_COUNTER | KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, -2));
    db_set_text(w, r, "[反击][支援]方式1:当前灵脉任一单位发动[魔术]技能时,令其无效且返还魔力消耗;方式2:令技能带来的仆役单位退场;方式3:摧毁技能带来的[魔术结界];方式4:解除同灵脉单位的任一[契约](演示效果免疫)。");

    /* ================================================================
     * 全量录入 批次3:《空想从者资源库》外典扩充包
     * ================================================================ */

    /* 追逐的美学C:战斗开始时,目标属性+15与自身属性+15;主要属性含补正属性时+25%胜率 */
    r = db_skill(w, "追逐的美学", KS_T_TALENT, KS_RANK_C, KS_WHEN_BATTLE_START, 0, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 15, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 15, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 25, -2));
    db_set_text(w, r, "战斗开始时:指定敌方任一单位,给予其一项高于自身的属性+15,之后自身低于目标的一项属性+15(同一项时自身该项额外+15);[主要属性]存在补正属性时自身+25%胜率(演示)。");

    /* 不屈的意志A:常驻,[状态免疫:恐惧];受负面效果时+25%胜率(至多+75) */
    r = db_skill(w, "不屈的意志", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_STATUS(S_STATE_IM, S_FEAR, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    db_set_text(w, r, "给予自身[状态免疫:恐惧];每次受到负面效果影响时+25%胜率补正(至多+75%;持[混乱]或受[魅惑]胜率惩罚时失去);EX时受≥40数值惩罚额外+15%(演示取20)。");

    /* 理性蒸发A:常驻,工序开始时80%判定成功则+25%胜率 */
    r = db_skill(w, "理性蒸发", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 25, -2));
    db_set_text(w, r, "每工序开始随机展示一项自身技能宝具并进行80%判定,成功时+25%胜率补正(判定出目≤5时向同灵脉展露自身真名;演示为无条件胜率)。");

    /* 流电学B:常驻,自身产生魔力消耗时给予施术者与目标[感电1] */
    r = db_skill(w, "流电学", KS_T_TALENT, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_STATUS(S_ELECTRIC, 1, 0));
    db_set_text(w, r, "自身产生魔力消耗时给予自身[感电1];若魔耗因技能/宝具产生,给予其施术者与目标[感电1](每回合对同一单位至多4次;以技艺A模板获取时改为自身感电换10魔供,演示敌全体)。");

    /* 深渊的邪视B:随时,80%[恐惧]判定;对方每持[恐惧]一名+5%胜率 */
    r = db_skill(w, "深渊的邪视", KS_T_TALENT, KS_RANK_B, KS_WHEN_ANY, 0, 6, 0);
    {
        E e = E_STATUS(S_FEAR, 1, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    }
    db_set_text(w, r, "对敌方战斗位任一非[构装体]单位进行80%[恐惧]判定;双方持[魔性]时可多指定一名目标;双方战斗位每存在一名持[恐惧]单位自身+5%胜率;对该技能仅持[人型]的目标判定+50%(演示)。");

    /* 雾夜的凶杀A:常驻/战斗开始时,袭击方+5%胜率,夜回合翻倍 */
    r = db_skill(w, "雾夜的凶杀", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 1, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    {
        E e = E_WIN(EF_WIN_UP, 5, -2); e.cond = KC_NIGHT;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "自身作为[袭击方]时+5%胜率(与敌方主力初次同场时翻倍,夜回合再翻倍,演示计夜+5%);昼回合发动需[幸运/2]%判定;干涉时立即获得回转。");

    /* 野兽的逻辑B:常驻,战斗行动限制规则(仅能经[兽化]获取) */
    r = db_skill(w, "野兽的逻辑", KS_T_TALENT, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "战斗中:1)作为袭击方必须以杀死敌人为目的;2)己方最终胜率≠0%时无法在主要/最终工序撤退;3)最终胜率为0%时工序结束即撤退;4)存在可发动的技能宝具则必须发动。仅能通过职阶技能[兽化]获取(演示占位)。");

    /* 自我进化A:常驻,受惩罚时成长(演示取抗性与耐久补正) */
    r = db_skill(w, "自我进化", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 20, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 5, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 5, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    db_set_text(w, r, "战斗中随自身受罚成长:受胜率惩罚时胜率补正+5%、受属性惩罚时对应属性+5常驻、受成功负面判定时抗性上升+5%,各至多叠加6次(演示取单层);全满后可将常驻补正转化任意属性。");

    /* 越过阿卡迪亚A:随时,[反击][支援]成为技能/宝具目标时40%判定给予自身[回避] */
    r = db_skill(w, "越过阿卡迪亚", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 0, 3, KS_F_COUNTER | KS_F_ASSIST);
    {
        E e = E_WIN(EF_EVADE, 0, -2);
        e.chance = 40; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[反击][支援]成为非特殊等级技能或等级低于自身的宝具的效果对象时,以40+敏捷差%判定,成功时仅对此次效果给予自身[回避](无效化宝具效果则失去6回转);等级高于自身的技能判定上限60%,级差3时固定0%。");

    /* 魔力放出(炎)A:随时,敌单体属性-15与[灼伤1];自身50%失败则自身[灼伤1] */
    r = db_skill(w, "魔力放出(炎)", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 15, 1, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 15, 1));
    {
        E e = E_STATUS(S_BURN, 1, 1);
        e.chance = 50; e.chance_neg = 1;
        db_eff(w, r, e);
    E e1022 = E_STATUS(S_BURN, 1, -2); e1022.chance = 50; db_eff(w, r, e1022);
    }
    db_set_text(w, r, "对自身进行50%判定,失败给予自身[灼伤1];战斗中发动时给予敌方战斗位任一单位[筋力/耐久/敏捷]任一项-15属性惩罚与[灼伤1](演示);EX时额外支付40魔力可指定第二名目标。");

    /* 自我保存B:常驻,战斗结束等级魔耗减半(演示+10魔力近似) */
    r = db_skill(w, "自我保存", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 10, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_set_text(w, r, "战斗结束时产生的等级魔耗数值减半(向下取整);C级起游荡回合的判定默认失败;B级起游荡回合仍获回转;A级起战败游荡+10%胜率(至多2次,演示魔力近似)。");

    /* 过载B:主要工序,指定目标,激荡联动胜率惩罚 */
    r = db_skill(w, "过载", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_PROC, 20, 9, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 1));
    db_set_text(w, r, "主要工序指定自身与敌方非[支援位]任一单位:本场战斗中自身对目标造成[激荡]时自身同样[激荡],但因此自损给予目标同等的胜率惩罚(至多-80%,演示取40)。");

    /* 剑之凯旋B:主要工序,战斗属性含[筋力]时自身[筋力]+20 */
    r = db_skill(w, "剑之凯旋", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_PROC, 0, 6, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 20, -2));
    db_set_text(w, r, "[战斗属性]中存在[筋力]时才能发动,给予自身[筋力]+20属性补正;本场战斗胜利后补正数值永久+20(每次胜利成长,演示)。");

    /* 外科手术E:行动阶段,解除任一异常状态或常驻惩罚 */
    r = db_skill(w, "外科手术", KS_T_TECHNIQUE, KS_RANK_E, KS_WHEN_ACT, 0, 9, 0);
    db_eff(w, r, E_WIN(EF_STATUS_REMOVE, S_POISON, -2));
    db_eff(w, r, E_WIN(EF_STATUS_REMOVE, S_BURN, -2));
    db_eff(w, r, E_WIN(EF_STATUS_REMOVE, S_CURSE, -2));
    db_set_text(w, r, "消耗行动阶段发动:解除当前灵脉上目标除[特性赋予]外任一[异常状态],或消除[宝具]外任一属性受到的常驻惩罚(演示可解除中毒/灼伤/诅咒)。");

    /* 无冠之武艺-(视为A):常驻,全属性+10补正与+50%胜率(情报知悉时失效) */
    r = db_skill(w, "无冠之武艺", KS_T_TECHNIQUE, KS_RANK_NEG, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 10, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 50, -2));
    db_set_text(w, r, "战斗中始终给予自身全属性+10属性补正与+50%胜率补正;敌方战斗位存在持有自身[荣冠]技能信息或知悉自身[真名]的单位时,此胜率补正无效化(演示)。");

    /* 女神的护佑A:常驻,特性[神性];+20等级与三属性+20常驻 */
    r = db_skill(w, "女神的护佑", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 20, -2));
    db_set_text(w, r, "持有此技能时始终给予自身[特性赋予:神性](不因无效化失去);始终给予+20等级补正与[筋力][耐久][敏捷]+20常驻补正(等级补正文本)。");

    /* 神授的智慧A:行动阶段,将自身技艺/魔术技能以C级模板给予目标 */
    r = db_skill(w, "神授的智慧", KS_T_BLESS, KS_RANK_A, KS_WHEN_ACT, 30, 9, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_MAG, 20, -2));
    db_set_text(w, r, "行动阶段指定当前灵脉任一单位,将自身任一[技艺/魔术]保有技能以C级模板给予目标;加符时改为宣言技能以≤B级赋予自身(回转满后失去),演示取魔力补正近似。");

    /* 启示A:常驻,回合结束90%判定获情报;同侧+40%胜率/对立侧目标-40%(演示) */
    r = db_skill(w, "启示", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 20, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    db_set_text(w, r, "回合结束时90%判定成功可从[位置/外貌/阵营/灵脉]信息中选择获悉一项;成功后的下个回合,与指定单位同侧时+40%胜率,对立侧时给予其-40%胜率惩罚(演示取20)。");

    /* 平稳的无花果C:常驻,[主力位]己方除自身外单位撤退FP-1 */
    r = db_skill(w, "平稳的无花果", KS_T_WEAPON, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_FP_UP, 1, -1));
    db_set_text(w, r, "[主力位]己方战斗位除自身外的自阵营单位撤退消耗FP-1(同类效果最后计算,不降至0);自阵营不持有令咒时撤退无需FP;EX时己方全部单位受用,自身退场时受用单位+10%抗性上升(演示)。");

    /* 黄金苹果-(视为C):随时,战斗中撤退FP-1或敌方FP+1;效果转移规则(文本) */
    r = db_skill(w, "黄金苹果", KS_T_WEAPON, KS_RANK_NEG, KS_WHEN_ANY, 20, 6, 0);
    db_eff(w, r, E_WIN(EF_FP_UP, 1, -1));
    db_set_text(w, r, "方式1:行动阶段发动,对任一灵脉进行不消耗行动力的[机动](不暴露灵脉,可宣言任选灵脉通报);方式2:战斗中自阵营单位本工序撤退FP-1,或敌方全体FP+1,之后将本技能给予本场战斗最终胜利者。");

    /* 卷烟之狮子A:初始工序,[主力位]宣言属性,每项对应战斗属性+20%胜率 */
    r = db_skill(w, "卷烟之狮子", KS_T_CROWN, KS_RANK_A, KS_WHEN_BATTLE_START, 10, 6, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    db_set_text(w, r, "[主力位]初始工序宣言[4-自身战败次数]项属性,本场战斗每有一项战斗属性为指定属性,自身+20%胜率(己方战斗位存在非自阵营单位时+5%);EX时每项优势额外+10%(演示)。");

    /* 狩猎卡莱顿的野猪A:随时,战斗开始时赋予自身[回避:技能] */
    r = db_skill(w, "狩猎卡莱顿的野猪", KS_T_CROWN, KS_RANK_A, KS_WHEN_ANY, 0, 1, 0);
    db_eff(w, r, E_WIN(EF_EVADE, 0, -2));
    db_set_text(w, r, "方式1:战斗开始时赋予自身[回避:技能];方式2:行动阶段宣言一名曾同场的单位,知悉其灵脉的[灵脉信息][单位信息](回转1/4/7,演示回避)。");

    /* 贫者的见识A:常驻,战斗内可资料分析;持分析信息的从者每名+30%胜率(演示) */
    r = db_skill(w, "贫者的见识", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 20, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 15, -2));
    db_set_text(w, r, "自身可在战斗中进行[资料分析](显示具体数值/宝具信息/特攻列表);敌方战斗位每存在一名持自身[资料分析]信息的从者,+30%胜率(成功分析过+10%、初次遭遇翻倍,演示取15);");

    /* 受虐之荣光B:常驻,[反击]受负面判定成功时耐久+10常驻与抗性+10% */
    r = db_skill(w, "受虐之荣光", KS_T_CROWN, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, KS_F_COUNTER);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 10, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 10, -2));
    db_set_text(w, r, "[反击]每当自身受到来源不为自身的负面判定且判定成功时,给予自身[耐久]+10常驻补正与[抗性上升:+10%](可叠加,演示单层);自身施加的异常状态之负面判定视来源为状态本身。");

    /* 屠龙A:常驻,[主力位]+40%胜率;对龙种目标额外+10%与抗性上升 */
    r = db_skill(w, "屠龙", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 40, -2));
    {
        E e = E_WIN(EF_WIN_UP, 10, 1);
        e.cond = KC_TARGET_TRAIT; e.cond_arg = TR_DRAGON;
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_RES_UP, 10, -2));
    }
    db_set_text(w, r, "[主力位]+40%胜率补正;自身受到[龙种]技能判定成功率减半且不因[龙种]获[恐惧];敌方战斗位存在[龙种]单位时+10%胜率与[抗性上升:+10%](每额外龙种+5%,演示)。");

    /* 永生的奉献C:常驻,[耐久]+40常驻,无法选其为主属性;己方胜率补正(文本) */
    r = db_skill(w, "永生的奉献", KS_T_CROWN, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 40, -2));
    db_set_text(w, r, "始终给予自身[耐久]+40常驻补正,但自身无法将[耐久]作为[主要属性];自身[抗性上升]恒为0%,按失去数值给予己方其他单位[+失去值/2%]胜率(仆役减半、人偶再减半,演示耐久部分);EX时改为给予等量抗性上升。");

    /* 护国的鬼将C:行动阶段,结界断壁残垣;结界内自身属性+15 */
    r = db_skill(w, "护国的鬼将", KS_T_CROWN, KS_RANK_C, KS_WHEN_ACT, 0, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 15, -2));
    db_set_text(w, r, "消耗行动阶段赋予当前灵脉[魔术结界:断壁残垣][结阵1/3]:该灵脉无法建立[阵地][工房][神殿](已存在则摧毁);[结阵3]时结界设立者恒为灵脉持有者,战斗中自身全属性+15(演示)。");

    /* 艺术审美A:常驻,回合开始/干涉时90%判定获取宝具真名;轮次结束+魔力 */
    r = db_skill(w, "艺术审美", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 5, -2));
    db_set_text(w, r, "回合开始或[干涉]时对同灵脉所有其他从者90%判定(每单位每回合一次),成功获得其随机一件宝具真名并记[审美1];轮次结束时+[审美数*5]魔力供给(演示5);此技能不暴露效果。");

    /* 圣人A:常驻,特性[神性];状态抵抗魅惑/恐惧/混乱;建卡自选效果(演示抵抗) */
    r = db_skill(w, "圣人", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_STATUS(S_STATE_RES, S_FEAR, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CHARM, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CONFUSE, -2));
    db_set_text(w, r, "始终给予自身[特性赋予:神性]与[状态抵抗:魅惑&恐惧&混乱];建卡时自选一项(1己方祝福技能效果等级+3/2[诅咒免疫+抗性15+清除常驻惩罚]/3[魅力气质]技能+1级并同侧+20%胜率/4记录4项概念武装可制作,演示取状态抵抗)。");

    /* 魔力附加A:常驻,魔力结算溢出转化为[附魔];消耗附魔减少魔耗并+25%胜率 */
    r = db_skill(w, "魔力附加", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 25, -2));
    db_set_text(w, r, "魔力结算时,将因超出上限移除的魔力转化为等量[附魔]标记(轮次结束清除至20);发动保有技能时可宣言消耗20层[附魔],令其本次同时视为[魔术]技能并减少20魔耗,随后+25%胜率(演示胜率)。");

    /* 使魔(鸽)D:常驻,建卡获得3个使魔(演示占位) */
    r = db_skill(w, "使魔(鸽)", KS_T_MAGIC, KS_RANK_D, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时给予持有者3个任意[使魔];除非此技能失效,其使魔不被无足量[对魔力]的单位破坏;使魔被破坏后下回合开始时于持有者灵脉重生(不占用面向,演示占位)。");

    /* 数秘术B:随时,交流中强化御主(人偶化);战斗中50%判定转移人偶控制权 */
    r = db_skill(w, "数秘术", KS_T_MAGIC, KS_RANK_B, KS_WHEN_ANY, 20, 6, 0);
    {
        E e = E_STATUS(S_CHARM, 2, 1);
        e.chance = 50; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "方式1:[交流]中对同灵脉御主(需同意)给予+10等级、全属性+20常驻补正与[特性赋予:构装体](无法叠加);方式2:战斗中令持[人偶]特性单位退出战斗位并以50%判定尝试夺取控制权(非仆役单位默认失败,演示魅惑近似)。");

    /* 虚无生者的叹息A:常驻,回合开始/干涉时同灵脉全体80%[恐惧]判定 */
    r = db_skill(w, "虚无生者的叹息", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    {
        E e = E_STATUS(S_FEAR, 1, 0);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 0));
    E e1023 = E_WIN(EF_WIN_DOWN, 10, 0); e1023.flag = EF_TICK_ROUND; e1023.status = S_FEAR; e1023.chance = 80; db_eff(w, r, e1023);
    E e2; memset(&e2, 0, sizeof(e2)); e2.chance_attr_base = -1; e2.flag = EF_TICK_ROUND; e2.status = S_FEAR; e2.value = 10; e2.chance = 0; e2.target = 0; db_eff(w, r, e2); /* EF_TICK_ROUND 每回合结算 */
    }
    db_set_text(w, r, "回合开始或[干涉]时,对同灵脉除自身外所有非[构装体]单位进行80%[恐惧]判定(每单位每回合一次);受此成功判定的单位本回合处对立战斗位时给予其-10%胜率惩罚;EX时持[恐惧]者无法对自身发动技能(演示)。");

    /* ---- 外典扩充包 · 宝具 ---- */

    /* 幻想大剑·天魔失坠A:最终工序,[主力位]敌非支援全体-40%胜率;龙种/记录特性额外-30% */
    r = db_np(w, "幻想大剑·天魔失坠", KS_NP_ARMORY, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_PROC, 70, 9, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 0));
    {
        E e = E_WIN(EF_WIN_DOWN, 30, 0);
        e.cond = KC_TARGET_TRAIT; e.cond_arg = TR_DRAGON;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[主力位]最终工序:敌非[支援位]全体-40%胜率惩罚,目标持[记录特性(神性/魔性)]或[龙种]时额外-30%;追加令咒[轰击](70%判定)再-40%并同样对龙种-30%;展露真名可+10%轰击成功率(演示)。");

    /* 解体圣母B:随时,[支援]满足条件时给予-25%胜率;条件满时100%[即死] */
    r = db_np(w, "解体圣母", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_B,
              KS_WHEN_ANY, 50, 13, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 25, 1));
    {
        E e = E_DEATH(50, 1, 1);
        e.cond = KC_NIGHT;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[支援]仅能指定敌非[支援位]单位:每满足一项条件(夜回合/目标为女性/初次同场或持[圣母]),立即获4回转,战斗内则给予-25%胜率;三条件全满时对目标100%[即死](幸运≥40减半,演示夜条件即死50%)。");

    /* 天蝎一射A:随时,仅[夜]回合解放,30+等级差%[即死]判定 */
    r = db_np(w, "天蝎一射", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_A,
              KS_WHEN_ANY, 80, 6, 0);
    {
        E e = E_DEATH(30, 1, 1);
        e.cond = KC_NIGHT;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "仅当前灵脉处于[夜]回合时解放(战斗中解放可不展露宝具信息);行动阶段可指定持[情报调查][资料分析]信息的目标:30+等级差%[即死]判定(幸运≥20减半);战斗中暗宣言目标,不持此宝具信息时解放具[必中](演示夜限定)。");

    /* 日轮啊，化作甲胄A:常驻,自身[抗性上升:+45%];属性/胜率惩罚视为0(演示抗性) */
    r = db_np(w, "日轮啊，化作甲胄", KS_NP_HUMAN, FC_DEFENSE, KS_RANK_A,
              KS_WHEN_PASSIVE, 45, 0, 0);
    db_eff(w, r, E_WIN(EF_RES_UP, 45, -2));
    db_set_text(w, r, "给予自身[抗性上升:+45%];令自身受到的属性惩罚与胜率惩罚始终为0(溢出总值>75时支付等量魔力,无法支付则本场不再影响,演示抗性部分);可额外4RP获取:战斗开始破弃此宝具并立即解放[日轮啊，顺从死亡EX]。");

    /* 包围苍天的小世界A:[反击][支援]己方单位成为效果对象时,己方全体[保护]+自身[无敌] */
    r = db_np(w, "包围苍天的小世界", KS_NP_BOUND, FC_DEFENSE, KS_RANK_A,
              KS_WHEN_ANY, 100, 9, KS_F_COUNTER | KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_PROTECT, 0, -1));
    db_eff(w, r, E_WIN(EF_INVINCIBLE, 0, -2));
    db_set_text(w, r, "[反击][支援]己方战斗位任一单位成为等级不大于此宝具的技能/宝具效果对象时解放:给予己方除自身外全部单位[保护],并给予自身[无敌];任一[保护]被触发后清除全部[保护]并立即失去9回转(演示)。");

    /* 勇者的不凋花B:常驻,[抗性上升:+30%];受到的[即死]判定默认失败 */
    r = db_np(w, "勇者的不凋花", KS_NP_HUMAN, FC_DEFENSE, KS_RANK_B,
              KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_RES_UP, 30, -2));
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, -2));
    db_set_text(w, r, "给予自身[抗性上升:+30%];成为负面效果对象时其效果等级仅对自身下降1级(B级以下来源持[神性]则无效化);自身受到的[即死]判定默认失败(每次使魔耗+10,递减);高等级[神性]单位效果下本回合无效(演示)。");

    /* 虚荣的空中庭园A:行动阶段,结界+召唤龙种人偶;结界内自身全属性+10与抗性+20% */
    r = db_np(w, "虚荣的空中庭园", KS_NP_WORLD, FC_DEFENSE, KS_RANK_A,
              KS_WHEN_ACT, 30, 3, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 20; e.cond_arg = 60; e.cond_arg2 = TR_DRAGON;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "虚荣的空中庭园:召唤龙种人偶(等级20 全属性10)";
        db_eff(w, r, e);
        db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, -2));
        db_eff(w, r, E_WIN(EF_RES_UP, 20, -2));
    }
    db_set_text(w, r, "方式1:行动阶段赋予[魔术结界:空中庭园](结阵1/6,结阵6时转为[额外灵脉:空中庭园]宽7/魔量60);方式2(B):宣言灵脉干涉;方式3(A):战斗开始时召唤任意数量[龙种][人偶](等级20/全属性10),结界内自身全属性+10与[抗性上升:+20%](演示取A)。");

    /* 恶龙之血铠B:常驻,获得[龙种];对自己效果等级下降(演示效果免疫) */
    r = db_np(w, "恶龙之血铠", KS_NP_HUMAN, FC_DEFENSE, KS_RANK_B,
              KS_WHEN_PASSIVE, 30, 0, 0);
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, -2));
    db_set_text(w, r, "建卡时破除[对魔力]并+1RP,获得[特性赋予:龙种](文本);成为敌方技能/宝具效果对象时,那个效果仅对自身下降3级(低于E级则无效化且不返还消耗);敌方持[屠龙]时此宝具对其无效;无法对[无敌贯通]效果生效(演示免疫)。");

    /* 非世间所存之幻马A:随时,[骑乘][反击]成为效果对象时赋予自身[回避] */
    r = db_np(w, "非世间所存之幻马", KS_NP_ARMORY, FC_DEFENSE, KS_RANK_A,
              KS_WHEN_ANY, 30, 18, KS_F_RIDE | KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_EVADE, 0, -2));
    db_set_text(w, r, "[骑乘][反击]自身成为不大于此宝具等级的技能/宝具效果对象时解放:赋予自身[回避];此[回避]无效化技能效果时立即获得18回转;EX时对任意等级生效(演示)。");

    /* 诉状箭书B:主要工序,宣言性别,敌非支援对应性别单位全属性-20 */
    r = db_np(w, "诉状箭书", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_B,
              KS_WHEN_PROC, 60, 9, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 20, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 20, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 20, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_MAG, 20, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_LUK, 20, 0));
    db_set_text(w, r, "主要工序解放,宣言[男性/女性]之一:敌非[支援位]对应性别单位全属性-20(仅一名目标时-25);解放时可不宣言性别改为随机(属性惩罚-30);EX时敌全部单位-30(演示按B级)。");

    /* 疾风怒涛的不死战车A:常驻,[骑乘]机动无需消耗行动;首场战斗敌主力-20%胜率 */
    r = db_np(w, "疾风怒涛的不死战车", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PASSIVE, 0, 0, KS_F_RIDE);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
    db_set_text(w, r, "[骑乘]宣言[机动]时允许给予当前灵脉单位不消耗行动的[介入]指令;回合开始宣言支付10*次数的魔力对任一灵脉进行不消耗行动的[机动];到达灵脉的首场战斗开始,敌主力-20%胜率(至多-100%,演示按首次)。");

    /* 极刑王A:主要工序,[无敌贯通]敌非支援全体以120%进行属性惩罚判定 */
    r = db_np(w, "极刑王", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 60, 6, KS_F_INV_PIERCE);
    {
        E e = E_ATTR(EF_ATTR_DOWN, A_STR, 5, 0);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_DOWN, A_END, 5, 0);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_DOWN, A_AGI, 5, 0);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "主要工序解放,宣言[筋力/耐久/敏捷]任一属性:敌非[支援位]全体进行十次120%负面判定,每次成功给予指定属性-5(本场未受过成功负面判定的目标成功率减半,演示单次80%);EX时不再减半。");

    /* 磔刑之雷树B:随时,消耗自身魔力池至下限给予[感电]并[激荡] */
    r = db_np(w, "磔刑之雷树", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_B,
              KS_WHEN_ANY, 20, 9, 0);
    db_eff(w, r, E_STATUS(S_ELECTRIC, 4, 0));
    db_eff(w, r, E_WIN(EF_ELECTRIC_BLOW, 0, 0));
    db_set_text(w, r, "方式1[对人]:令自身任一魔力池消耗至下限,给予敌单体[魔耗/10]层[感电](至多8层)并[激荡],立即获6回转;方式2[对军]:给予敌非[支援位]全体[魔耗/20]层[感电](至多6层)并[激荡](演示4层)。");

    /* 灿然辉耀的王剑B:常驻,自身四属性+20常驻;己方全体+5%胜率 */
    r = db_np(w, "灿然辉耀的王剑", KS_NP_HUMAN, FC_BUFF, KS_RANK_B,
              KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 20, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -1));
    db_set_text(w, r, "给予自身除[幸运][宝具]外全属性+20常驻补正;给予己方战斗位全部单位+5%胜率补正(自身处[主力位]时此胜率补正仅对自身翻倍,演示)。");

    /* 伤兽的咆吼A:常驻/主要工序,受罚记录[伤痕];宣告破弃给予敌主力惩罚(演示属性近似) */
    r = db_np(w, "伤兽的咆吼", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 15, -2));
    db_set_text(w, r, "战斗中每次受到来源不为自身的负面效果影响记录[伤痕1];战斗开始时每层[伤痕]给自身除[宝具][幸运]外全属性+5(至多+60);主要工序可宣言破弃此宝具,给予敌主力[-(伤痕数*25)%]胜率惩罚(演示属性近似)。");

    /* 王冠·睿智之光A:随时,宣言击杀后召唤[魔性]召唤物(演示) */
    r = db_np(w, "王冠·睿智之光", KS_NP_ARMORY, FC_SUMMON, KS_RANK_A,
              KS_WHEN_ANY, 30, 18, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 10; e.cond_arg = 60; e.cond_arg2 = TR_DEMONIC;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "王冠·睿智之光:召唤(等级10 全属性10 魔性)";
        db_eff(w, r, e);
    }
    db_set_text(w, r, "宣言同时击杀一名御主或从者后发动:召唤等级10、全属性10、[特性赋予:魔性]的召唤物(若击杀御主/从者而发动则同时视为该单位,存活4回合;否则2回合消失);回合结束判定成功时赋予目标灵脉[固有结界:人间乐园](每个结界令召唤物翻倍,文本)。");

    /* 梵天啊，覆盖大地A:初始工序,[主力位]三属性合计不高于自身则[晕眩3]/[灼伤3] */
    r = db_np(w, "梵天啊，覆盖大地", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 80, 9, KS_F_MAIN);
    db_eff(w, r, E_STATUS(S_STUN, 3, 0));
    db_eff(w, r, E_STATUS(S_BURN, 3, 1));
    db_eff(w, r, E_STATUS(S_LAG, 1, 0));
    db_set_text(w, r, "[主力位]方式1:指定敌非[支援位]全体,自身[筋力][耐久][敏捷]合计值不高于目标时给予[晕眩3](持≤此等级的[防御]宝具者额外[迟滞1]);方式2:指定敌主力,合计值不高于自身时给予[灼伤3]并获6回转(演示)。");

    /* 彗星跑法A:常驻,[敏捷]+50常驻;攻击判定成功后给予[迟滞] */
    r = db_np(w, "彗星跑法", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_PASSIVE, 25, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 50, -2));
    {
        E e = E_STATUS(S_LAG, 1, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "自身不受到己方[骑乘]宝具效果影响时生效:[敏捷]+50常驻补正;战斗位上以外单位发动技能/解放宝具使自身成为效果对象时,60%[迟滞]判定(成功则那个效果受[迟滞]影响);可宣言使己方[骑乘]宝具本场对自身无效(演示)。");

    /* 骄慢王之美酒B:随时,赋予灵脉中毒效果(演示方式3:自身魔术技能发动时给予全场中毒) */
    r = db_np(w, "骄慢王之美酒", KS_NP_ARMORY, FC_STATUS, KS_RANK_B,
              KS_WHEN_ANY, 20, 3, 0);
    db_eff(w, r, E_STATUS(S_POISON, 4, 0));
    E e2; memset(&e2, 0, sizeof(e2)); e2.chance_attr_base = -1; e2.flag = EF_TICK_ROUND; e2.status = S_POISON; e2.value = 10; e2.chance = 0; e2.target = 0; db_eff(w, r, e2); /* EF_TICK_ROUND 每回合结算 */
    db_set_text(w, r, "方式1/2:赋予当前灵脉灵脉效果(回合开始或干涉时对除自身外全场做100-耐久%中毒判定;获取灵脉供魔者中毒1);方式3:[反击]自身发动[魔术]技能时额外支付20魔力宣言,给予同灵脉除自身外全部单位[中毒4](灵脉效果每层仅对同一单位每回合一次,演示方式3)。");

    /* 唤起恐慌之魔笛B:初始工序,敌非支援全体90%[恐惧]判定;每赋予恐惧+10%胜率 */
    r = db_np(w, "唤起恐慌之魔笛", KS_NP_ARMORY, FC_STATUS, KS_RANK_B,
              KS_WHEN_PROC, 60, 6, 0);
    {
        E e = E_STATUS(S_FEAR, 1, 0);
        e.chance = 90; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_eff(w, r, E_STATUS(S_CONFUSE, 1, 0));
    }
    db_set_text(w, r, "初始工序:对战斗位除自身外所有非[支援位]非[构装体]单位进行90%[恐惧]判定;本场战斗每当自身给予任一单位[恐惧/混乱],自身+10%胜率;同时使3体以上获得[恐惧]时,清除之并给予全场[混乱1](演示)。");

    /* 一碰就倒！A:随时,取消蓄力/给迟滞;攻击蓄力单位+20%胜率(演示) */
    r = db_np(w, "一碰就倒！", KS_NP_HUMAN, FC_STATUS, KS_RANK_A,
              KS_WHEN_ANY, 20, 0, 0);
    db_eff(w, r, E_STATUS(S_LAG, 1, 1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    db_set_text(w, r, "每回合可解放至多5次:1)因[冲锋]获胜率时给予被冲锋目标[迟滞1](每名单位每战斗一次);2)指定非[支援位]单位令其所有[蓄力]效果无效化;3)任一单位[蓄力]时解放,自身+20%胜率(演示)。");

    /* 开演之刻已至，此处应有雷鸣般的喝彩B:随时,[交流]中解放,结界封印目标职阶技能 */
    r = db_np(w, "开演之刻已至，此处应有雷鸣般的喝彩", KS_NP_HUMAN, FC_STATUS, KS_RANK_B,
              KS_WHEN_ANY, 60, 12, 0);
    {
        E e = E_STATUS(S_SKILL_SEAL, 3, 1);
        e.chance = 90; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "仅[交流]中解放:生成[魔术结界:舞台],给予目标本职[职阶技能][封印](目标判定时可重骰一次);目标与自身不同灵脉时结界摧毁;C级起额外给予与目标职阶属性相同的属性惩罚,加符时对常驻保有技能[封印](演示技能封印)。");

    /* 暗黑雾都B:随时,结界:敌从者敏捷-20、御主中毒3(演示) */
    r = db_np(w, "暗黑雾都", KS_NP_HUMAN, FC_STATUS, KS_RANK_B,
              KS_WHEN_ANY, 50, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 20, 0));
    db_eff(w, r, E_STATUS(S_POISON, 3, 1));
    db_eff(w, r, E_STATUS(S_POISON, 2, 1));
    db_set_text(w, r, "赋予[魔术结界:暗黑雾都](灵脉改[永夜]):灵脉单位无法发动≤此宝具等级的[魔术]技能;战斗开始时战斗位上除自身外的从者/仆役[敏捷]-20、御主[中毒3];回合结束御主再[中毒2](演示);持高等级[直感]或本回合发动过[魔术]技能者不受影响。");

    /* 少女的贞节B:常驻,轮次结束时清除全场[感电]并转化为魔力(演示) */
    r = db_np(w, "少女的贞节", KS_NP_HUMAN, FC_SUPPLY, KS_RANK_B,
              KS_WHEN_PASSIVE, 15, 0, 0);
    db_eff(w, r, E_WIN(EF_STATUS_REMOVE, S_ELECTRIC, -1));
    db_eff(w, r, E_MANA(EF_MANA_UP, 40, -2));
    db_set_text(w, r, "此宝具持有上限150的额外魔力池;轮次结束时清除当前灵脉全部单位的[感电]层数,并给予此宝具魔力池[清除层数*10]魔力供给(每次至多80);此魔力池无法接受其他来源的魔力(演示清除与补充)。");

    /* 隐藏不贞的头盔C:常驻,自身宝具不暴露;资料分析/真名猜测默认失败(演示) */
    r = db_np(w, "隐藏不贞的头盔", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_C,
              KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "此宝具生效时给予自身[宝具封印];自身持有的保有技能/宝具不会暴露效果,对自身进行的[资料分析]与[真名猜测]默认失败;可随时支付20魔力令此宝具失效至轮次结束(演示占位)。");

    /* 驰骋天际星之枪尖B:随时,[交流]中解放,宣言袭击并生成斗技场结界 */
    r = db_np(w, "驰骋天际星之枪尖", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_B,
              KS_WHEN_ANY, 30, 3, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 1));
    db_set_text(w, r, "仅[交流]中解放,指定同灵脉任一非[女性]单位立即宣言[袭击],赋予[魔术结界:流星斗技场](双方战斗位仅能各存在1单位,[祝福/魔术]技能效果无效化,战斗结束摧毁);EX时冲锋判定给予目标惩罚(演示胜率惩罚近似)。");

    /* 神罚的野猪B:随时,解放后变为常驻:获得[狂化A]与[猛兽]特性,+20%胜率 */
    r = db_np(w, "神罚的野猪", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_B,
              KS_WHEN_ANY, 20, 999, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    E e1024 = E_WIN(EF_WIN_UP, 20, -2); e1024.flag = EF_ON_WIN; db_eff(w, r, e1024);
    E e1025 = E_WIN(EF_WIN_UP, 20, -2); e1025.flag = EF_ON_WIN; db_eff(w, r, e1025); /* EF_ON_WIN 胜利触发 */
    db_set_text(w, r, "无法在战斗工序内解放;解放后变为[常驻/魔耗0]:破除[对魔力],获得[狂化A]与[特性赋予:猛兽];作为袭击方+20%胜率,每次[战斗胜利]+20%;回合开始或干涉时50%判定失败则袭击同灵脉随机单位;Berserker建卡时立即解放并获得[兽化B](演示)。");

    /* 鲜血的传承A:随时,解放后全属性+20常驻,获得[狂化EX]与[魔性](演示) */
    r = db_np(w, "鲜血的传承", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_A,
              KS_WHEN_ANY, 20, 999, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 20, -2));
    db_set_text(w, r, "解放后变为[常驻]:给予自身[30-自阵营令咒数*10]全属性常驻补正(演示20),无效化自身全部技能宝具效果(可按RP购入[死徒]技能/[变化]);获得[狂化EX]与[特性赋予:魔性];昼回合[死徒]技能无效且无法[机动];[幸运]至多10;Berserker建卡立即解放(演示)。");

    /* 魔术万能攻略书C:常驻,[对魔力]提升至A级;可无效化工房/结界并免疫魔术类宝具效果 */
    r = db_np(w, "魔术万能攻略书", KS_NP_HUMAN, FC_ANTITRAIT, KS_RANK_C,
              KS_WHEN_PASSIVE, 15, 0, 0);
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, -2));
    db_set_text(w, r, "持有此宝具时若持有[对魔力]则提升至A级,否则以C级模板获取;可随时支付30魔力指定[工房/阵地/人偶/结界/固有结界]令其效果无效化;即将受视为[魔术]技能的宝具效果影响时,支付60魔力令其对自身无效化(演示免疫)。");

    /* 红莲之圣女C:最终工序,满足条件时解放,自焚换取敌主力[灼伤]并[爆燃] */
    r = db_np(w, "红莲之圣女", KS_NP_HUMAN, FC_ANTITRAIT, KS_RANK_C,
              KS_WHEN_PROC, 20, 999, 0);
    db_eff(w, r, E_STATUS(S_BURN, 5, 1));
    db_set_text(w, r, "需满足至少一项条件(己方最终胜率0%/敌方存在[恶]阵营/敌方存在[魂食]者/敌方存在[对人类威胁]或[隐藏属性:兽]):解放时自身魔力耗至下限且魔力池上限降为0,每20魔力或20上限给予敌主力[灼伤1],再按符合条件数*2层[灼伤]并[爆燃];解放过的战斗结束后自身退场(演示5层灼伤)。");

    /* ================================================================
     * 全量录入 批次4:《空想从者资源库》京都扩充包
     * ================================================================ */

    /* 老练A:常驻,破除职阶技能换额外技艺栏;抗性+20与状态抵抗;技艺技能本场等级+1 */
    r = db_skill(w, "老练", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_RES_UP, 20, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_FEAR, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CHARM, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CONFUSE, -2));
    db_set_text(w, r, "建卡时破除自身职阶技能(不返还RP),获得额外技能栏并无视面向购入一个[技艺]技能;始终[抗性上升:+20%][状态抵抗:魅惑&恐惧&混乱],自身不会因任何效果被强制行动;战斗开始宣言一项[技艺]技能令其本场效果等级+1(无模板则不受降级)。");

    /* 神性A:常驻,特性[神性];全属性+5常驻;抗性+15;效果等级下降(演示) */
    r = db_skill(w, "神性", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 20, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 5, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 15, -2));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 5, 0));
    db_set_text(w, r, "依等级叠加:成为技能效果对象时其效果仅对自身下降1级(同型效果冲突时+10%抗性);B级起自身判定+10%终成功/其他单位对自身判定-10%;C级起技能宝具目标-5%胜率(至多-15%);D级起全属性+5常驻;E级起[特性赋予:神性]与[抗性上升:+15%](演示按A级)。");

    /* 病弱A:常驻,耐久分配上限-50;首位解放宝具[必中];负面判定+25%(演示) */
    r = db_skill(w, "病弱", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_PIERCE, 0, -2));
    db_eff(w, r, E_WIN(EF_HIT_FINAL_UP, 25, -2));
    db_set_text(w, r, "建卡时[耐久]分配上限-50,其他属性+30;每场战斗首个解放的宝具仅在那次解放获得[必中];自身负面判定与负面状态判定+25%最终成功率;自身执行行动/发动技能宝具时须5%判定,成功则本次失败(不产生消耗,演示)。");

    /* 高千穗的白色大蛇A:常驻,[反击]成为弱化目标时获得[状态免疫:弱化]与[龙种] */
    r = db_skill(w, "高千穗的白色大蛇", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, -2));
    db_set_text(w, r, "[反击]自身成为任一[弱化状态]效果目标时(此技能无回转):获得[状态免疫:弱化状态]与[特性赋予:龙种](失去1/3/5回转),在来源结算完成或工序结束时失去(演示效果免疫)。");

    /* 蝮蛇之眼B:战斗开始时,每场一次暗宣言对抗,不同则首个宝具负面判定+30% */
    r = db_skill(w, "蝮蛇之眼", KS_T_TALENT, KS_RANK_B, KS_WHEN_BATTLE_START, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_FINAL_UP, 30, -2));
    db_eff(w, r, E_STATUS(S_CONFUSE, 1, 1));
    db_set_text(w, r, "每场战斗限一次:指定敌方主力位,双方暗宣言工序;宣言相同则给予其随机低等级技能[封印](无条件则[混乱1]);宣言不同则自身在宣言工序解放的首个宝具负面判定+30%成功率(无法生效时改为自身属性补正,演示)。");

    /* 大蛇之咒B:常驻/战斗开始时,诅咒层数转胜率;发动时转移诅咒(演示) */
    r = db_skill(w, "大蛇之咒", KS_T_TALENT, KS_RANK_B, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    {
        E e = E_STATUS(S_CURSE, 4, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
    E e1026 = E_STATUS(S_CURSE, 1, -2); e1026.cond = KC_NIGHT; db_eff(w, r, e1026);
    }
    db_set_text(w, r, "建卡时获得[特性赋予:猛兽/魔性];自身[诅咒]至少1层且不被转移,每持[诅咒1]胜率补正+5%;夜回合开始自身[诅咒1];发动时10*诅咒层数%负面判定,成功给自身[恐惧][抗性下降:-10%],失败给予敌非[支援位]任一单位[诅咒4]与[-5*诅咒层数%]胜率惩罚(至多-50%,演示)。");

    /* 绝招A:随时,[主力位]下一个技能/宝具获[无敌贯通]并选取强化效果 */
    r = db_skill(w, "绝招", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 0, 12, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_INV_PIERCE, 0, -2));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 1));
    db_eff(w, r, E_WIN(EF_FLOOR_PEN, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 40, 1));
    db_set_text(w, r, "[主力位]发动时默认优先结算:自身下一个发动的技能/解放的宝具获得[无敌贯通],并自选一项(首次生效时判定+40%/目标-30%胜率与[底限穿透:-10%]/目标[耐久]-40,演示全给);EX时不因任何效果无法发动并额外[必中]。");

    /* 阴阳交汇B:常驻,[主力位]双方基础胜率按对抗情况互罚(演示) */
    r = db_skill(w, "阴阳交汇", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 15, 1));
    db_set_text(w, r, "[主力位]始终给予自身[-战斗属性对抗带给己方的基础胜率%]胜率惩罚,并给予敌方主力位等量惩罚(演示15);可随时宣言将给予自身的惩罚转化为等量胜率补正;此技能带来的胜率修正无法被移除/削减。");

    /* 人斩A:常驻,敌方主力仅持[人型]时给予-40%胜率 */
    r = db_skill(w, "人斩", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    {
        E e = E_WIN(EF_WIN_DOWN, 40, 1);
        e.cond = KC_TARGET_TRAIT; e.cond_arg = TR_HUMAN;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "敌方主力位仅持有[人型]特性时,给予其-40%胜率惩罚并令自身对其判定+20%最终成功率;目标同时持有其他特性时效果减半;不持有[人型]时无效(演示条件行)。");

    /* 拔刀无二A:常驻,战斗开始获[纳刀];首次解放宝具负面判定+20%并逐工序增长 */
    r = db_skill(w, "拔刀无二", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_FINAL_UP, 20, -2));
    db_set_text(w, r, "战斗开始时给予自身[纳刀];持有[纳刀]时,自身战斗中首次解放宝具,其首次生效的负面判定+20%成功率,此补正每工序结束时+20%(每场战斗开始重置);仅能通过[拔刀·神威]购入。");

    /* 天下布武A:常驻,按敌方高等级从者特性获得增益(演示取龙种/神性分支) */
    r = db_skill(w, "天下布武", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 20, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 10, -2));
    db_set_text(w, r, "战斗中按敌方等级高于自身的从者特性获得效果(取最高等级一方):[神性]全属性+10与抗性+10%(按神性技能等级再+20/15/10/5);[魔性]状态抵抗+判定+20%;[龙种]免疫疲惫/残废/抗性下降与三属性+20;[隐藏属性:地]等级/2%胜率(演示取龙种与神性)。");

    /* 维新的英雄A:常驻,敌方主力等级高于自身时全属性+15(神性翻倍) */
    r = db_skill(w, "维新的英雄", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 15, -2));
    db_set_text(w, r, "己方战斗位无[神性]单位且敌方主力位等级高于自身时,给予自身除[宝具]外全属性+15属性补正(敌方主力持[神性]时翻倍);自身处[辅助位]时己方主力位同样受用。");

    /* 恶魔的证明C:常驻,获得特性[魔性];即死/令咒死亡后可于游荡灵脉复活 */
    r = db_skill(w, "恶魔的证明", KS_T_BLESS, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "获得[特性赋予:魔性](文本);因[即死]或令咒效果死亡的下回合开始,清除自身全部状态/标记并以魔力50于游荡灵脉[复活](契约与仆役继承,礼装不继承;破除所有等级高于此技能的技能宝具);任意单位对自身[真名]正确的[真名猜测]时自身退场。");

    /* 鬼武藏的遗嘱B:常驻,发动[技艺]技能时记录属性+5常驻补正(至多30) */
    r = db_skill(w, "鬼武藏的遗嘱", KS_T_BLESS, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 5, -2));
    db_set_text(w, r, "建卡时记录自身除[宝具]外4项属性;战斗中发动[技艺]技能时给予全部记录属性+5常驻补正并失去6回转(回转满前无法再次生效,至多+30);自身退场时将常驻补正给予同灵脉任一单位(演示按B级)。");

    /* 天逆矛A:随时,30%负面判定成功给目标属性-25与胜率-25% */
    r = db_skill(w, "天逆矛", KS_T_WEAPON, KS_RANK_A, KS_WHEN_ANY, 15, 1, 0);
    {
        E e = E_ATTR(EF_ATTR_DOWN, A_STR, 25, 1);
        e.chance = 30; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_WIN(EF_WIN_DOWN, 25, 1);
        e.chance = 30; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "对敌方战斗位任一单位进行30%负面判定,成功时令其除[宝具]外一项属性-25并承受-25%胜率惩罚(演示)。");

    /* 魔王A:常驻,[主力位][反击]30%判定令自身技能/宝具效果再生效一次 */
    r = db_skill(w, "魔王", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN | KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_RECAST, 6, -2));
    db_set_text(w, r, "[主力位][反击]自身发动技能或宝具时自动生效:30%判定成功时,再次支付一份发动条件,令其效果再次生效一次(视为同一次发动;不作用于[额外技能][额外宝具])。演示为回转+6近似。");

    /* 血染的蛮勇A:常驻,额外属性惩罚-5(或改胜率惩罚);自身抗性下降(演示) */
    r = db_skill(w, "血染的蛮勇", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 5, 1));
    db_eff(w, r, E_STATUS(S_TIRED, 1, -2));
    db_set_text(w, r, "战斗中,每当自身以其他效果给予任意单位属性惩罚时,额外给予对应属性-5(多属性同罚时改为-5%胜率),每次生效自身[抗性下降:-5%](常时存在);B级起按抗性下降/2%胜率补正(至多10%);A级起战斗开始三属性+10常驻并于战斗结束转为[疲惫1](演示)。");

    /* 击剑矫捷如鹰隼A:战斗开始时,[主力位]敌方技艺/兵器技能发动时+25%胜率 */
    r = db_skill(w, "击剑矫捷如鹰隼", KS_T_CROWN, KS_RANK_A, KS_WHEN_BATTLE_START, 0, 3, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 25, -2));
    db_set_text(w, r, "[主力位]战斗开始时暗指定[技艺/兵器]一项:敌方主力每发动该项技能(或其效果生效),自身+25%胜率;自身每发动一次该项技能,自身+5%胜率(演示)。");

    /* 维新之龙A:随时,[反击]敌方技能生效时记录并以B级模板获取 */
    r = db_skill(w, "维新之龙", KS_T_CROWN, KS_RANK_A, KS_WHEN_ANY, 0, 3, KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "[反击]敌方战斗位任一技能生效时允许发动:记录那个技能,随后以B级模板获取一项记录技能(战斗结束时移除);获取面向与自身重复且自身等级≥B时替换原技能,此后可再宣言提升该技能等级/魔耗/回转1级(演示占位)。");

    /* 无敌之剑A:常驻,魔剑宝具不因[蓄力]延迟,随时宣言解放 */
    r = db_skill(w, "无敌之剑", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "自身[面向:魔剑]的宝具不会因[蓄力]延迟生效,允许无视发动时机随时宣言解放并立即结算;解放[魔剑]宝具时,免疫自身持有的[异常/弱化状态]带来的效果(演示占位)。");

    /* 人斩彦斋A:常驻,负面判定+20%;首次属性惩罚数值提升一半 */
    r = db_skill(w, "人斩彦斋", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 20, -2));
    db_set_text(w, r, "建卡时破除[气息遮蔽]并+1RP;自身发起的负面判定+20%成功率;战斗内首次对敌主力造成属性惩罚时其数值提升一半(至多+50,对3项属性时至多+30,对5项时+20);判定上视为[技艺]技能。");

    /* 船中八策A:随时,自身魔力<0时发动;给予目标胜率/常驻/抗性/魔力(演示两项) */
    r = db_skill(w, "船中八策", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 3, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 25, 1));
    db_eff(w, r, E_MANA(EF_MANA_UP, 45, 1));
    db_set_text(w, r, "自身魔力池小于0时才能发动,指定同灵脉任一非仆役单位,自选一项:1)+25%胜率;2)两项属性+15常驻(回合结束移除);3)[抗性上升:+25%];4)+45魔力供给(演示1与4);对自身以外目标发动立即获2回转,一回合不可指定相同目标。");

    /* ---- 京都扩充包 · 宝具 ---- */

    /* 了结剑-(视为A):随时,[主力位]记录技艺技能并复制发动;决胜时每获取+20%胜率 */
    r = db_np(w, "了结剑", KS_NP_HUMAN, FC_SWORD, KS_RANK_NEG,
              KS_WHEN_ANY, 10, 3, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    db_set_text(w, r, "[主力位]建卡时记录一项≤A级[技艺]技能(占用面向);解放时可宣言记录的技能以原持有者模板发动(魔耗回转正常);60%判定成功立即获3回转;决胜检定时,本场每通过此宝具获取过一项技能+20%胜率(每场首次,演示)。");

    /* 无形-(视为A):随时,[主力位][必中]80%混乱判定;60%判定成功耐久-20 */
    r = db_np(w, "无形", KS_NP_HUMAN, FC_SWORD, KS_RANK_NEG,
              KS_WHEN_ANY, 10, 3, KS_F_MAIN | KS_F_PIERCE);
    {
        E e = E_STATUS(S_CONFUSE, 2, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_DOWN, A_END, 20, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[主力位][必中]解放时对敌主力80%负面判定(目标持[心眼]/[直感]减半),成功给予[混乱2];追加支付10魔力:60%负面判定(目标每持[混乱]+20%,敏捷≥40减半),成功清除其[混乱]并给予自身[+20%*清除层数]胜率与目标[耐久]-20(不持[抗性上升]或最终成功率≥100%时翻倍);无法被[反击]。");

    /* 无二打-(视为A):初始工序,[主力位][无敌贯通]以敏捷%判定,成功耐久-30(演示) */
    r = db_np(w, "无二打", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_NEG,
              KS_WHEN_PROC, 20, 6, KS_F_MAIN | KS_F_INV_PIERCE);
    {
        E e = E_ATTR(EF_ATTR_DOWN, A_END, 30, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[主力位][无敌贯通]初始工序:对敌主力以[自身基础敏捷]%负面判定(目标敏捷≥40减半),成功给予[耐久]-[自身初始敏捷/2]属性惩罚;目标耐久因此降为0时立即结算[残废](演示60%/-30)。");

    /* 百花缭乱·我爱称A:随时,[主力位][无敌贯通]按目标三属性惩罚总值进行即死判定 */
    r = db_np(w, "百花缭乱·我爱称", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_A,
              KS_WHEN_ANY, 50, 18, KS_F_MAIN | KS_F_INV_PIERCE);
    db_eff(w, r, E_DEATH(50, 1, 1));
    db_set_text(w, r, "[主力位][无敌贯通]指定敌非[支援位]任一单位,按目标[筋力][耐久][敏捷]属性惩罚总值:≥50时30%[即死](失败给三属性-20并获12回转);≥100时50%[即死](失败给三属性-10并获9回转);≥150时90%[即死](幸运≥60减半,演示50%档)。");

    /* 通灵·伊吹大明神缘起A:初始工序,诅咒层数强化判定与即死 */
    r = db_np(w, "通灵·伊吹大明神缘起", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_A,
              KS_WHEN_PROC, 60, 6, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 15, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 15, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 15, 1));
    {
        E e = E_DEATH(30, 1, 1);
        e.cond = KC_TARGET_STATUS_GE; e.cond_arg = S_CURSE; e.cond_arg2 = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "初始工序指定敌非[支援位]任一单位,以[60+20*自身[诅咒]层数]%负面判定,成功给予[筋力][耐力][敏捷]-15;目标持[诅咒]时额外发起[5*自身诅咒层数]%[即死]判定(不受自身成功率补正影响,演示30%行)。");

    /* 誓言的羽织B:常驻,自身全属性+10常驻;持有的低等级宝具获得[爆发] */
    r = db_np(w, "誓言的羽织", KS_NP_HUMAN, FC_BUFF, KS_RANK_B,
              KS_WHEN_PASSIVE, 20, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 10, -2));
    db_set_text(w, r, "给予自身全属性+10常驻补正;自身持有的不高于此宝具等级的宝具获得[爆发](每场战斗首次[爆发]解放后失效至回合结束);EX时对所有宝具生效并在宣言爆发时全属性+10(演示)。");

    /* 与你同征绵津见之原A:战斗开始时,己方主力[抗性上升:+20%](演示) */
    r = db_np(w, "与你同征绵津见之原", KS_NP_ARMORY, FC_BUFF, KS_RANK_A,
              KS_WHEN_BATTLE_START, 70, 18, 0);
    db_eff(w, r, E_WIN(EF_RES_UP, 20, -1));
    db_set_text(w, r, "战斗开始时:将[神性]以C级模板赋予自身(已持有则其魔耗/效果/等级+1;无[神性]技能时无需支付RP);给予己方主力位[抗性上升:+20%]与+25等级补正(己方仅两体自阵营时翻倍;己方主力非自身时此宝具立即获12回转,演示抗性)。");

    /* 百段C:战斗开始时,[骑乘]给予自身[回避:技能]与[耐久][敏捷]+20 */
    r = db_np(w, "百段", KS_NP_HUMAN, FC_BUFF, KS_RANK_C,
              KS_WHEN_BATTLE_START, 20, 6, KS_F_RIDE);
    db_eff(w, r, E_WIN(EF_EVADE, 0, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 20, -2));
    db_set_text(w, r, "[骑乘]战斗开始时给予自身[回避:技能];给予自身[耐久][敏捷]+20属性补正;EX时[回避:技能]改为[回避](演示)。");

    /* 诚之旗A:随时,召唤至多5个等级40/全属性20的召唤物(演示1体) */
    r = db_np(w, "诚之旗", KS_NP_ARMORY, FC_SUMMON, KS_RANK_A,
              KS_WHEN_ANY, 20, 3, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 40; e.cond_arg = 120; e.cond_arg2 = TR_HUMAN;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "诚之旗:召唤(等级40 全属性20,演示1体/至多5体)";
        db_eff(w, r, e);
    }
    db_set_text(w, r, "召唤至多5个等级40、全属性20的[召唤物](判定上同时视为[从者],可执行除[魂食]外的从者行动,始终与持有者同灵脉);自身处[主力位]时召唤物全属性+10;有空余战斗位时召唤物可立即参战;B级起可随同阵营离开灵脉,A级起自由行动(演示1体)。");

    /* 如翱翔天际之龙C:常驻,[骑乘]召唤龙种召唤物,存在时敌主力-等级/2%胜率 */
    r = db_np(w, "如翱翔天际之龙", KS_NP_ARMORY, FC_SUMMON, KS_RANK_C,
              KS_WHEN_PASSIVE, 10, 0, KS_F_RIDE);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 40; e.cond_arg = 100; e.cond_arg2 = TR_DRAGON;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "如翱翔天际之龙:召唤(等级40 总属性100 龙种)";
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
    }
    db_set_text(w, r, "[骑乘]降临即召唤等级40、总属性100、[独特]的[龙种]召唤物(宝具属性20);其存在于战斗位时敌方主力位受到[-(等级/2)%]胜率惩罚(演示-20%);不因[轰击]退场;战斗开始支付20魔力可使其等级/全属性+20、抗性+20%、体型[巨大]并占用2战斗位(召唤物退场则破弃此宝具)。");

    /* 人类无骨C:随时,给予目标[抗性下降:-50%]与[-15%]底限穿透 */
    r = db_np(w, "人类无骨", KS_NP_HUMAN, FC_STATUS, KS_RANK_C,
              KS_WHEN_ANY, 40, 6, 0);
    db_eff(w, r, E_WIN(EF_RES_DOWN, 50, 1));
    db_eff(w, r, E_WIN(EF_FLOOR_PEN, 15, 1));
    db_set_text(w, r, "给予同灵脉任一单位[抗性下降:-50%]与[-15%]底限穿透;目标存在[抗性上升]或不持[人型]特性时,此[抗性下降]减半(演示)。");

    /* 热力学第二定律的否定A:随时/常驻,魔性赋予与魔耗免除(演示占位) */
    r = db_np(w, "热力学第二定律的否定", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_A,
              KS_WHEN_ANY, 0, 6, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "回合开始或[干涉]时支付30魔力宣言,给予同灵脉任一单位[特性赋予:魔性](仅向已持/曾持[魔性]者公示);本场存在持[魔性]单位时,自身产生魔力消耗可记录并免除(每回合限5次);任何知晓此宝具的[从者]可支付30魔力消除任意单位的[魔性]赋予(演示占位)。");

    /* 第六天魔王波旬A:战斗开始时,[主力位]摧毁阵地工房并生成焦灼地狱结界 */
    r = db_np(w, "第六天魔王波旬", KS_NP_HUMAN, FC_ANTITRAIT, KS_RANK_A,
              KS_WHEN_BATTLE_START, 30, 3, KS_F_MAIN);
    db_eff(w, r, E_STATUS(S_BURN, 1, 0));
    {
        E e = E_STATUS(S_FEAR, 1, 0);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[主力位]解放后摧毁当前灵脉全部[阵地][工房][神殿],生成[固有结界:焦灼地狱](宽5/永昼/初始工序无法撤退,结阵1/6);回合开始/干涉/解放宝具时,对无足量[神性]的同灵脉单位依[神性]等级给予魔耗翻倍/魔力池上限0/灼伤等(演示灼伤与恐惧);结界存在时自身视为[魔性]。");

    /* ================================================================
     * 全量录入 批次7:《空想从者资源库》深池扩充包
     * ================================================================ */

    /* 女难之相A:常驻,回合开始/干涉时给予同灵脉[女性]单位[魅惑1];敌方每女性+5%胜率 */
    r = db_skill(w, "女难之相", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_STATUS(S_CHARM, 1, 0));
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    db_set_text(w, r, "回合开始或[干涉]时,给予当前灵脉除自身外全部[女性]单位[魅惑1](每单位每回合一次,至多令[魅惑]达5层,持[状态抵抗:魅惑]者上限-2);战斗中敌方战斗位每存在一名[女性]单位,自身+5%胜率(演示)。");

    /* 沉着冷静A:常驻,状态抵抗魅惑/恐惧/混乱;同阵营行动判定+20% */
    r = db_skill(w, "沉着冷静", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_STATUS(S_STATE_RES, S_FEAR, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CHARM, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CONFUSE, -2));
    db_eff(w, r, E_WIN(EF_HIT_UP, 20, -1));
    db_set_text(w, r, "给予自身[状态抵抗:魅惑&恐惧&混乱];未参与战斗的轮次中自身[等级魔耗]-20;同灵脉同阵营单位发起行动判定时+20%基础成功率(自身改为全部判定+20%);EX时战斗开始暗宣言战术,初始工序可变更为之(演示)。");

    /* 梦幻的魅力A:常驻,魅惑/恐惧/混乱判定+30%;给予混乱类状态时敌方-10%胜率 */
    r = db_skill(w, "梦幻的魅力", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 30, -2));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 0));
    db_set_text(w, r, "自身发起的[魅惑&恐惧&混乱]判定+30%成功率;给予任意单位这些状态时,该单位本回合处敌方战斗位则-10%胜率、处己方战斗位则+10%(每单位每回合一次);可随时解除自身施加的这些状态;己方持更高级[气质/魅力]技能时本场失效(演示)。");

    /* 影乡的武练B:常驻,每次[战斗胜利]全属性+5常驻(单属性至多+40) */
    r = db_skill(w, "影乡的武练", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_PASSIVE, 5, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 5, -2));
    db_set_text(w, r, "每次[战斗胜利]给予自身全属性+5常驻补正(累计达初始[宝具]属性时再+5;单属性至多+40);加符时获得补正的同时额外+5等级补正(演示)。");

    /* 布利里安德罗的嘶鸣A:常驻,骑乘等级提升;自身[敏捷]+30常驻与+20等级补正 */
    r = db_skill(w, "布利里安德罗的嘶鸣", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 20, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 30, -2));
    db_set_text(w, r, "建卡时将职阶技能[骑乘]等级+3(否则获得[骑乘C]);不持[骑乘]宝具时始终给予任一宝具[骑乘]效果;始终给予+20等级补正与[敏捷]+30常驻补正;战斗中己方[战术]总是被克制(演示敏捷)。");

    /* 暗夜的武练A:常驻,筋力敏捷+20;夜回合翻倍 */
    r = db_skill(w, "暗夜的武练", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 20, -2));
    {
        E e = E_ATTR(EF_ATTR_UP_CONST, A_STR, 20, -2); e.cond = KC_NIGHT; db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_UP_CONST, A_AGI, 20, -2); e.cond = KC_NIGHT; db_eff(w, r, e);
        e = E_WIN(EF_HIT_UP, 20, -2); e.cond = KC_NIGHT; db_eff(w, r, e);
    E e1027 = E_WIN(EF_WIN_UP, 5, -2); e1027.cond = KC_NIGHT; db_eff(w, r, e1027);
    }
    db_set_text(w, r, "始终给予自身[筋力][敏捷]+20常驻补正;当前灵脉处于[夜]回合时常驻补正翻倍(演示夜+20行)并令自身负面判定+20%基础成功率;Assassin职阶不占面向;EX时夜回合每次负面判定成功+5%胜率。");

    /* 战斗加速(影)A:随时,给予自身[敏捷]+50常驻补正(战斗结束失去) */
    r = db_skill(w, "战斗加速(影)", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 0, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 50, -2));
    db_set_text(w, r, "方式1:自身[敏捷]+50常驻补正(战斗结束失去);方式2:同数值但工序结束失去且默认最先结算;A级时每场一次可指定敌方[支援位]单位视为处[辅助位];EX时夜回合额外[回避:技能](演示方式1)。");

    /* 精灵的狂躁A:战斗开始时,敌非支援全体90%[恐惧]判定,成功给予筋力敏捷-30 */
    r = db_skill(w, "精灵的狂躁", KS_T_BLESS, KS_RANK_A, KS_WHEN_BATTLE_START, 20, 0, 0);
    {
        E e = E_STATUS(S_FEAR, 1, 0);
        e.chance = 90; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_DOWN, A_STR, 30, 0);
        e.chance = 90; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_DOWN, A_AGI, 30, 0);
        e.chance = 90; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "对敌方战斗位除[支援位]外全体进行90%[恐惧]判定,成功时给予目标[筋力][敏捷]-30属性惩罚(判定成功率>100%时惩罚提升至40);每场战斗仅能发动一次;加符时对[支援位]也生效(演示)。");

    /* 英雄制作A:随时,御主条件触发强化:等级+5与全属性+10常驻 */
    r = db_skill(w, "英雄制作", KS_T_BLESS, KS_RANK_A, KS_WHEN_ANY, 0, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 10, 1));
    db_set_text(w, r, "当前灵脉御主参战/胜利/被指定/负面判定成功/魔力结算<-20/令咒0时立即发动(每条件一次):给予目标[王者1]与+5等级、全属性+10常驻;王者4层时赋予自身任一技能,5层转移宝具,6层目标视为[从者];至多生效5次后破弃(演示属性部分)。");

    /* 赤枝的继承者B:战斗开始时,任一属性+40;无条件时给状态抵抗 */
    r = db_skill(w, "赤枝的继承者", KS_T_BLESS, KS_RANK_B, KS_WHEN_BATTLE_START, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 40, -2));
    db_set_text(w, r, "每场战斗限一次:给予自身[筋力][耐久][敏捷]任一项+40属性补正(演示筋力);己方战斗位除自身外无持[赤枝的骑士]者时给予[状态抵抗:异常状态](战斗结束失去);判定上名称视为[赤枝的骑士];仅能通过[赤枝的骑士]获取。");

    /* 圣者的数字C:常驻,昼回合:[抗性上升:+10%]与[筋力]+40属性补正 */
    r = db_skill(w, "圣者的数字", KS_T_BLESS, KS_RANK_C, KS_WHEN_PASSIVE, 20, 0, 0);
    {
        E e = E_WIN(EF_RES_UP, 10, -2); e.cond = KC_DAY; db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_UP, A_STR, 40, -2); e.cond = KC_DAY; db_eff(w, r, e);
    }
    db_set_text(w, r, "当前为[昼]回合时,始终给予自身[抗性上升:+10%],以及[筋力]属性等同于[分配耐久数值]的属性补正(至多+60,演示40)。");

    /* 九伟人之铠A:常驻,+20等级与[抗性上升:+30%];己方低等级单位+10%胜率 */
    r = db_skill(w, "九伟人之铠", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_RES_UP, 30, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -1));
    db_set_text(w, r, "给予自身+20等级补正与[抗性上升:+30%];始终给予己方战斗位除自身外等级低于自身的单位+10%胜率补正(总值至多+60%);加符时成为技能/宝具目标时其效果对自身下降1级(演示)。");

    /* 妊娠之花A:常驻,女性单位[妊娠]标记机制(演示占位) */
    r = db_skill(w, "妊娠之花", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "每回合一次宣言,指定同灵脉任一[女性]非[仆役]单位(需同意)给予[妊娠]标记:自身无法对其袭击;其战斗中[筋力][耐久][敏捷][魔力]-10;经三个轮次后标记失去并产生等级/属性同其初始的[召唤物](不产生等级魔耗、不因[轰击]退场,演示占位)。");

    /* 猛犬杀手A:常驻,+30%胜率;敌方存在猛兽/魔兽时自身敏捷幸运+20 */
    r = db_skill(w, "猛犬杀手", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 30, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_LUK, 20, -2));
    db_set_text(w, r, "给予自身+30%胜率补正;敌方存在持[猛兽][魔兽]特性单位时自身立即知悉,随后本场战斗自身[敏捷][幸运]+20属性补正,己方单位[冲锋][死斗]获得的胜率补正翻倍(演示)。");

    /* 赤枝的骑士B:战斗开始时,任一属性+40;死亡时转移[赤枝的继承者] */
    r = db_skill(w, "赤枝的骑士", KS_T_CROWN, KS_RANK_B, KS_WHEN_BATTLE_START, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 40, -2));
    db_set_text(w, r, "每场战斗限一次:给予自身[筋力][耐久][敏捷]任一项+40属性补正(演示耐久);己方战斗位存在其他持[赤枝的骑士]者时给予自身[状态抵抗:异常状态];自身死亡时指定同灵脉任一单位给予其[赤枝的继承者](演示)。");

    /* 魔境的智慧A:随时,[支援]每回合100%判定获取等级低于此技能的保有技能(演示) */
    r = db_skill(w, "魔境的智慧", KS_T_CROWN, KS_RANK_A, KS_WHEN_ANY, 30, 0, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "[支援]每回合一次宣言后以100%判定,成功获取等级低于此技能的非[天赋]非[常驻]从者保有技能(满足条件可立即发动;每轮次首次外成功率30%);无法获取面向重复/特殊等级技能;获取的技能为额外技能回合结束失去,判定上视为[魔术](演示占位)。");

    /* 守护的誓约A:随时,[反击]指定御主给予[保护],自身[抗性上升:+30%] */
    r = db_skill(w, "守护的誓约", KS_T_CROWN, KS_RANK_A, KS_WHEN_ANY, 10, 3, KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_PROTECT, 0, 1));
    db_eff(w, r, E_WIN(EF_RES_UP, 30, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    db_set_text(w, r, "[反击]指定当前灵脉任一御主(需同意)赋予其[保护],并赋予自身[抗性上升:+30%];自身因[保护]转移受负面时+5%胜率、负面判定失败时+10%(总值不超过[抗性上升]);敌方无[保护]时自身+5%底限胜率;敌方对本回合目标解放过≥此等级的[即死]宝具时+20%最终胜率(演示)。");

    /* 顷刻一击A:初始工序,暗宣言属性,主要工序按等级差给属性补正(至多40)(演示) */
    r = db_skill(w, "顷刻一击", KS_T_CROWN, KS_RANK_A, KS_WHEN_BATTLE_START, 40, 6, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 40, -2));
    db_set_text(w, r, "本场战斗赋予自身[抗性破除:负面效果],暗宣言3项属性;主要工序时若等级低于目标,宣言属性+等级差补正(高于目标则减半,至多+40);魔力<0时可宣言支付40魔力使补正翻倍并立即对自身20%[即死](演示)。");

    /* 新娘的守护者C:常驻,给予[新娘]标记;同灵脉时目标[抗性上升:+20%] */
    r = db_skill(w, "新娘的守护者", KS_T_CROWN, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_RES_UP, 20, 1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, 1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    db_set_text(w, r, "可给予初次遭遇的非仆役单位[新娘]标记(不可袭击其);与[新娘]同灵脉时给予其[抗性上升:+20%](演示);两位[新娘]同灵脉时可举办[婚礼](双方移除标记,+20%胜率);移除6个标记后自身获得[新娘]并按曾持有数量+5%胜率。");

    /* 光之地平线A:战斗开始时,[主力位]始终[龙种];四属性+10常驻(战斗结束失去) */
    r = db_skill(w, "光之地平线", KS_T_CROWN, KS_RANK_A, KS_WHEN_BATTLE_START, 0, 12, KS_F_MAIN);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 10, -2));
    db_set_text(w, r, "[主力位]建卡时可购入[阵地制作];持有此技能时自身始终持[龙种]特性;发动时给予自身[筋力][耐久][敏捷][魔力]+10常驻补正(战斗结束失去);不持[决战]宝具时替换为[无人知晓的无垢搏动]且本场等级+1(演示四属性)。");

    /* 幻术A:随时,120%[混乱]判定;已有混乱的目标改给胜率惩罚(演示) */
    r = db_skill(w, "幻术", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 6, 0);
    {
        E e = E_STATUS(S_CONFUSE, 1, 1);
        e.chance = 100; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "对同灵脉任一单位发起120%负面判定,成功给予[混乱1](演示100%);目标已持[混乱]时改为[-(最终成功率/4)%]胜率惩罚;也可指定自身(本轮回避免侦查暴露);B级起可指定任意数量目标且成功率减半;EX时赋予灵脉[魔术结界:幻术领域](进入者50%混乱判定)。");

    /* ---- 深池扩充包 · 宝具 ---- */

    /* 死亡满溢的魔境之门A:战斗开始时,[主力位]给[祝福]标记,未持者负面判定(即死) */
    r = db_np(w, "死亡满溢的魔境之门", KS_NP_ARMORY, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_BATTLE_START, 60, 18, KS_F_MAIN);
    db_eff(w, r, E_DEATH(50, 0, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_LUK, 35, -2));
    db_eff(w, r, E_STATUS(S_PROTECT, 1, -2));
    db_set_text(w, r, "[主力位]本场战斗初始工序无法撤退;给予自身与指定单位[祝福]标记;未持[祝福]的非[支援位]单位进行[105-目标幸运]%负面判定(视为[即死]):成功令其产生60魔力消耗,出目<5时其不付令咒即退场;追加令咒可额外保护1体并给[祝福]者[幸运]+35(演示50%档,判断幸运减半)。");

    /* 通往死亡满溢的魔境之门A:战斗开始时,[支援]给[祝福]:幸运+30,胜率惩罚-20% */
    r = db_np(w, "通往死亡满溢的魔境之门", KS_NP_ARMORY, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_BATTLE_START, 60, 18, KS_F_ASSIST);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_LUK, 30, 1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, 1));
    db_set_text(w, r, "[支援]指定当前灵脉任一单位给予[祝福](已持祝福则其补正永久+10):持有者[幸运]+30属性补正,其所持胜率惩罚总值始终减少20%;此宝具解放的战斗中补正+50且首次[即死]判定受[-(幸运补正/2)%]成功率惩罚;可随时清除祝福并每名获6回转;加符时[底限胜率:+10%](演示)。");

    /* 轮转胜利之剑A:最终工序,[主力位]敌非支援全体-40%胜率并[灼伤1] */
    r = db_np(w, "轮转胜利之剑", KS_NP_ARMORY, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_PROC, 80, 9, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 0));
    db_eff(w, r, E_STATUS(S_BURN, 1, 0));
    db_set_text(w, r, "[主力位]解放时给予敌非[支援位]全体-40%胜率惩罚并给予其[灼伤1](昼回合时对敌主力惩罚+20%);追加令咒[轰击]成功再-40%并[灼伤1](每令咒+30%成功率,演示)。");

    /* 无人知晓的无垢搏动A:主要工序,[主力位]蓄力后敌全体-40%胜率(演示) */
    r = db_np(w, "无人知晓的无垢搏动", KS_NP_WORLD, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_PROC, 100, 9, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 0));
    E e1028 = E_STATUS(S_CHARGE, 1, -2); e1028.flag = EF_CHARGE; db_eff(w, r, e1028);
    db_set_text(w, r, "[主力位]主要工序解放宣言[蓄力];最终工序给予敌方全部单位-40%胜率惩罚;追加令咒[轰击]成功令惩罚+40%(最终成功率>100%时对敌主力追加溢出值惩罚);EX时可宣言全属性+20后立即退场。仅能经[光之地平线]获取(演示)。");

    /* 贯穿死翔之枪A:随时,[主力位][必中]冲锋联动或蓄力即死(演示) */
    r = db_np(w, "贯穿死翔之枪", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_A,
              KS_WHEN_ANY, 40, 12, KS_F_MAIN | KS_F_PIERCE);
    {
        E e = E_DEATH(40, 1, 1);
        e.cond = KC_NONE;
        db_eff(w, r, e);
    E e1029 = E_STATUS(S_CHARGE, 1, -2); e1029.flag = EF_CHARGE; db_eff(w, r, e1029);
    }
    db_set_text(w, r, "[主力位][必中]方式1:自身宣言/成为[冲锋]目标时解放,给予敌主力任一属性-20并使其无法在初始工序撤退,随后以方式2无视时机回转立即解放;方式2:初始工序[蓄力],主要工序对敌主力40%[即死](幸运≥40减半,失败时立即获3回转);EX时受属性惩罚者[抗性下降:-20%](演示)。");

    /* 闪耀的终天一箭A:初始工序,[主力位][必中]蓄力后属性-20并有条件即死(演示) */
    r = db_np(w, "闪耀的终天一箭", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_A,
              KS_WHEN_PROC, 60, 6, KS_F_MAIN | KS_F_PIERCE);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 20, 1));
    {
        E e = E_DEATH(40, 1, 1);
        e.cond = KC_NONE;
        db_eff(w, r, e);
    E e1030 = E_STATUS(S_CHARGE, 1, -2); e1030.flag = EF_CHARGE; db_eff(w, r, e1030);
    }
    db_set_text(w, r, "[主力位][必中]解放时[蓄力];主要工序给予敌主力任一属性-20;持其[资料分析]/[情报调查]信息时追加[0%]即死判定(目标[敏捷][魔力][幸运]每项<40则+25%成功率,演示40%档);不受等级≤此宝具的[防御]宝具影响。");

    /* 死亡将为明日的希望A:最终工序,敌主力50%[即死];胜利后自身+30%胜率 */
    r = db_np(w, "死亡将为明日的希望", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_A,
              KS_WHEN_PROC, 60, 9, 0);
    db_eff(w, r, E_DEATH(50, 1, 1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 30, -2));
    E e1031 = E_WIN(EF_WIN_UP, 30, -2); e1031.flag = EF_ON_WIN; db_eff(w, r, e1031);
    E e1032 = E_WIN(EF_WIN_UP, 30, -2); e1032.flag = EF_ON_WIN; db_eff(w, r, e1032); /* EF_ON_WIN 胜利触发:自身 */
    E e1033 = E_WIN(EF_WIN_UP, 10, -1); e1033.flag = EF_ON_WIN; db_eff(w, r, e1033); /* EF_ON_WIN 胜利触发:己方全体 */
    db_set_text(w, r, "最终工序:对敌方[主力位]50%[即死](目标为[恶]阵营时+[10+初始成功率/2]%;目标持[混乱/恐惧/魅惑]状态抵抗或免疫时成功率减半);其因此退场时己方视为[战斗胜利];己方胜利后自身本轮次+30%胜率且己方其他单位+10%(演示)。");

    /* 绝世的幻剑A:常驻/随时,[主力位][反击]支付20魔力令自身惩罚始终为0(演示) */
    r = db_np(w, "绝世的幻剑", KS_NP_HUMAN, FC_DEFENSE, KS_RANK_A,
              KS_WHEN_ANY, 10, 0, KS_F_MAIN | KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, -2));
    db_set_text(w, r, "[主力位][反击]给予自身+30等级补正;成为任一效果对象时支付40魔力:本场战斗中自身受到的属性惩罚与胜率惩罚始终为0(超出总值>75时支付等量魔力;使一项保有技能仅本场条件/回转/效果等级+1);每场战斗仅解放一次;仅能经[不带剑的誓言]获取(演示免疫)。");

    /* 斩断死辉之刃A:最终工序,[无敌贯通]敌主力[-15%]底限穿透 */
    r = db_np(w, "斩断死辉之刃", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 60, 3, KS_F_INV_PIERCE);
    db_eff(w, r, E_WIN(EF_FLOOR_PEN, 15, 1));
    db_set_text(w, r, "[无敌贯通]给予敌方主力位[-15%]底限穿透;其不存在[底限胜率]时额外-30%胜率(己方每有一项战斗属性[优势]再追加一次,演示)。");

    /* 无败紫鞴草A:常驻/随时,状态抵抗;敌方技能发动时-10%胜率(演示) */
    r = db_np(w, "无败紫鞴草", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PASSIVE, 35, 0, 0);
    db_eff(w, r, E_STATUS(S_STATE_RES, S_FEAR, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CHARM, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CONFUSE, -2));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 0));
    db_set_text(w, r, "战斗中给予自身[状态抵抗:魅惑&恐惧&混乱];敌方战斗位每有技能发动,给予发动者-10%胜率;自身每发动一次技能也给予敌主力-10%(至多5次);惩罚总值达30%时可宣言蓄力,宣言工序开始时对受影响单位再-35%(神性单位额外-15%)(演示)。");

    /* 贯穿之朱枪A:主要工序,[主力位]敌主力[幸运]-50属性惩罚 */
    r = db_np(w, "贯穿之朱枪", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 60, 9, KS_F_MAIN);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_LUK, 50, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_LUK, 50, 0));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 25, 1));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 25, 0));
    db_set_text(w, r, "[主力位]方式1[对人]:给予敌主力[幸运]-50属性惩罚并令其无法在主要工序撤退([幸运]为战斗属性且[优势]时额外-25%胜率),立即获3回转;方式2[对军]:额外支付30魔力,敌非[支援位]全体[幸运]-50(优势时各-25%胜率)(演示)。");

    /* 遗失的圣剑A:主要工序,[主力位]敌主力[耐久]-40;最终工序按战斗属性差给惩罚 */
    r = db_np(w, "遗失的圣剑", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 60, 9, KS_F_MAIN);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 40, 1));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 1));
    db_set_text(w, r, "[主力位]解放时给予敌主力[耐久]-40属性惩罚;最终工序按[目标战斗属性合计-自身战斗属性合计]%给予胜率惩罚(正数则转为目标胜率补正,至多-60%);极端劣势时可将魔力耗至下限换取战斗属性惩罚(仅能经[不带剑的誓言]获取)。");

    /* 缚锁全断·过重湖光A:宝具解放时,[主力位][反击]与[无毁的湖光]联动:敌全体-50%胜率 */
    r = db_np(w, "缚锁全断·过重湖光", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_ANY, 20, 9, KS_F_MAIN | KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 50, 0));
    db_eff(w, r, E_WIN(EF_RES_DOWN, 10, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 20, 1));
    db_eff(w, r, E_STATUS(S_TIRED, 2, -2));
    db_set_text(w, r, "[主力位][反击]购入仅3RP,建卡时必须与同等级[无毁的湖光]共用栏位;其解放时可选:1)对敌非[支援位]任一单位以50+敏捷差%判定,成功[耐久]-20并-60%胜率;2)敌非[支援位]全体-50%胜率与[抗性下降:-10%](演示);解放过的战斗结束自身[疲惫2]。");

    /* 尚未知晓的无垢湖光A:随时,敌非支援任一单位-35%胜率 */
    r = db_np(w, "尚未知晓的无垢湖光", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_ANY, 20, 1, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 35, 1));
    db_set_text(w, r, "给予敌方战斗位非[支援位]任一单位-35%胜率惩罚(演示);仅能通过[光之地平线]的效果获取。");

    /* 黑刃C:随时,80%判定成功-20%胜率 */
    r = db_np(w, "黑刃", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_C,
              KS_WHEN_ANY, 20, 1, 0);
    {
        E e = E_WIN(EF_WIN_DOWN, 20, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "解放时对目标进行80%判定,成功给予-20%胜率惩罚;除首次解放外魔力消耗为0;加符时判定成功额外给敌非[支援位]全体-5%胜率(演示)。");

    /* 噬碎死牙之兽A:常驻,[筋力][耐久]+30常驻补正 */
    r = db_np(w, "噬碎死牙之兽", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_PASSIVE, 20, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 30, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 30, -2));
    db_set_text(w, r, "给予自身[筋力][耐久]+30常驻补正;自身未解放宝具的战斗中额外+40属性补正(演示)。");

    /* 永世隔绝的理想乡A:随时,[支援位][反击]己方主力等级补正与条件强化(演示) */
    r = db_np(w, "永世隔绝的理想乡", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_ANY, 60, 15, KS_F_SUPPORT | KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_WIN_UP, 30, -1));
    db_set_text(w, r, "[支援位][反击]本场战斗中始终给予己方主力位+30等级补正;依序叠加:其不持令咒时下次负面判定默认失败;其为御主时免疫属性惩罚;己方仅自阵营时免疫胜率惩罚;等级≥70时+90%胜率;属性补正总值≥150时全属性+30(条件未达成则停止并每项获3回转,演示胜率)。");

    /* 紧握其剑，银之臂A:常驻/战斗开始时,[专注]机制:属性补正与底限穿透(演示) */
    r = db_np(w, "紧握其剑，银之臂", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 15, -2));
    db_eff(w, r, E_WIN(EF_FLOOR_PEN, 20, 1));
    db_set_text(w, r, "战斗中按[专注]层数给予三属性+5/层与+15%/层胜率补正(至多+60%);战斗开始支付60魔力解放:始终给予自身[-20%]底限穿透(演示),自身给予战斗属性补正时记录[专注1](至多12层);持魅惑/恐惧/混乱或解放其他宝具时清除记录;战斗结束按专注层数获回转并减半。");

    /* 无想躯体C:随时,[反击]回避(方式1);蓄力期间敌全体-10三属性(方式2,演示) */
    r = db_np(w, "无想躯体", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_C,
              KS_WHEN_ANY, 40, 6, 0);
    db_eff(w, r, E_WIN(EF_EVADE, 0, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 10, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 10, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 10, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 20, -2));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 5, 0));
    db_set_text(w, r, "方式1[反击]:成为技能/宝具效果对象时解放,敏捷+20常驻并判定成功获[回避];方式2:解放[蓄力](可与他人共存),蓄力期间自身敏捷+20,每次技能宝具造成效果时敌非[支援位]全体-5%胜率,每工序结束敌非[支援位]全体三属性-10(演示);蓄力结束时自身三属性-20并可能结算[残废]。");

    /* 终结破晓之蛇啊，降临于此A:随时,[骑乘]召唤猛兽神性召唤物并灼伤攻击(演示) */
    r = db_np(w, "终结破晓之蛇啊，降临于此", KS_NP_ARMORY, FC_SUMMON, KS_RANK_A,
              KS_WHEN_ANY, 70, 9, KS_F_RIDE);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 60; e.cond_arg = 240; e.cond_arg2 = TR_BEAST;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "终结破晓之蛇:召唤(等级60 总属性240 猛兽)";
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
        db_eff(w, r, E_STATUS(S_BURN, 1, 0));
    db_eff(w, r, E_STATUS(S_POISON, 1, -2));
    }
    db_set_text(w, r, "[骑乘]解放时给予自身[中毒1],召唤等级60、总属性240、[独特][猛兽][神性]的召唤物(战斗位时敌主力-[等级/2]%胜率,演示-20%);解放方式2令召唤物对敌非[支援位]全体-30%胜率并以80%判定给予[灼伤1];成功的[中毒]结算后基础[耐久]为0时自身[即死](不持[骑乘]则无法[冲锋])。");

    /* 灼烧殆尽的炎笼A:初始工序,敌非支援90%[灼伤]判定;每灼伤单位+40%胜率(演示) */
    r = db_np(w, "灼烧殆尽的炎笼", KS_NP_ARMORY, FC_STATUS, KS_RANK_A,
              KS_WHEN_PROC, 60, 9, 0);
    {
        E e = E_STATUS(S_BURN, 1, 0);
        e.chance = 90; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    db_eff(w, r, E_STATUS(S_BURN, 2, 0));
    }
    db_set_text(w, r, "对敌非[支援位]全体进行90%[灼伤]判定,成功给予[灼伤1](已持[灼伤]者额外[灼伤2]);敌方战斗位每存在一名持[灼伤]的单位,自身+40%胜率补正(演示20)。");

    /* 不带剑的誓言A:常驻,[主力位]探索[遗失的圣剑];可宣言破弃换取[绝世的幻剑] */
    r = db_np(w, "不带剑的誓言", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_A,
              KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "仅建卡获取;持其他[剑]字段宝具或[不毁的极圣]时无效;降临时将[遗失的圣剑]暗置于随机灵脉(抵达时可知悉),可执行[行动-探索]将其获取;自阵营无令咒时战斗开始宣言破弃,以同等级模板获取[绝世的幻剑](无异常状态时给予加符);可将礼装/宝具视为[不毁的极圣E]解放(付20魔力,属性+15,演示占位)。");

    /* 一闪而逝，银之臂A:最终工序,[主力位]按[专注]层数给敌主力/全体胜率惩罚(演示) */
    r = db_np(w, "一闪而逝，银之臂", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_A,
              KS_WHEN_PROC, 60, 12, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 1));
    db_eff(w, r, E_STATUS(S_TIRED, 2, -2));
    db_set_text(w, r, "[主力位]仅[紧握其剑，银之臂]解放效果生效期间可解放;方式1[对人]:敌主力-15*专注层数%胜率(己方每项战斗属性[劣势]再-5*专注层数%);方式2[对军]:敌非[支援位]全体-(30+5*专注层数)%胜率(非主力减半,演示30);解放过的战斗结束自身[疲惫2](与[紧握其剑]共用栏位)。");

    /* 这手掬起的诸多性命啊C:常驻/随时,[状态免疫:中毒];魔池解诅咒异常(演示) */
    r = db_np(w, "这手掬起的诸多性命啊", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_C,
              KS_WHEN_ANY, 0, 6, 0);
    db_eff(w, r, E_STATUS(S_STATE_IM, S_POISON, -2));
    db_set_text(w, r, "给予此宝具[60]魔力池(不可直接转移);给予自身[状态免疫:中毒];获得[灵脉供魔]时可宣言转入此魔池;解放时消耗魔池魔力自选:1)等量移除目标属性惩罚(至多40);2)每20魔力移除目标20常驻惩罚;3)每10魔力清除目标1层带层数异常(至多3层,演示免疫)。");

    /* ================================================================
     * 全量录入 批次8:《空想从者资源库》军阵扩充包
     * ================================================================ */

    /* 宇宙反应器A:常驻,+40%胜率(己方有[恶]阵营时-10%;敌方有[恶]时翻倍,演示) */
    r = db_skill(w, "宇宙反应器", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 40, -2));
    db_set_text(w, r, "始终给予自身+40%胜率补正;己方战斗位存在[恶]阵营单位时此补正-10%(自阵营有[恶]时等级-1);敌方战斗位存在[恶]阵营单位时补正翻倍;己方无[魂食]而敌方有[魂食]时,自身判定+10%最终成功率、受负面判定-10%(演示基础值)。");

    /* 龙种A:常驻,特性[龙种];初始工序敌主力75%[恐惧]判定并-40%胜率 */
    r = db_skill(w, "龙种", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 20, 0, 0);
    {
        E e = E_STATUS(S_FEAR, 1, 1);
        e.chance = 75; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_WIN(EF_WIN_DOWN, 40, 1);
        e.chance = 75; e.chance_neg = 1;
        db_eff(w, r, e);
    E e1034 = E_WIN(EF_WIN_DOWN, 40, 1); e1034.flag = EF_TICK_PROC; e1034.status = S_FEAR; e1034.chance = 75; db_eff(w, r, e1034);
    }
    db_set_text(w, r, "始终给予自身[特性赋予:龙种](不因无效化失去);初始工序开始时令敌方主力位75%[恐惧]判定,成功给予[恐惧]与-40%胜率惩罚(敌方存在更高等级[龙种]技能时惩罚减半);EX时恐惧成功后敌方主力所有[优势]战斗属性视为[均势]。");

    /* 天性的肉体A:常驻,状态免疫:特性赋予;战斗中[筋力]补正(演示) */
    r = db_skill(w, "天性的肉体", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 40, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN_CONST, A_STR, 20, -2));
    db_set_text(w, r, "建卡时[筋力]分配上限+20;给予自身[状态免疫:特性赋予];战斗外[筋力]-20常驻惩罚,战斗中[筋力]+[分配筋力-60]属性补正(演示40);判定上自身三属性合计始终<270;EX时可将任一[常驻]保有技能变为[天赋]。");

    /* 龙之心A:常驻/随时,特性[龙种];轮次结束+20魔供;发动时[抗性上升:+15%] */
    r = db_skill(w, "龙之心", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 20, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 15, -2));
    db_set_text(w, r, "始终给予自身[特性赋予:龙种];每轮次结束时+20魔力供给;支付15魔力发动:给予自身[抗性上升:+15%](受成功负面判定或回合结束时移除);发动过此技能的战斗内自身[轰击]判定按发动次数+5%成功率(演示)。");

    /* 谷间深渊A:轮次开始时,回朔至四天前的灵脉/储备/标记状态 */
    r = db_skill(w, "谷间深渊", KS_T_TALENT, KS_RANK_A, KS_WHEN_ACT, 0, 24, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "轮次开始时发动:对四天前自身所处灵脉进行一次不消耗行动的[干涉];自身[储备]技能/礼装、给予他人的[标记]与异常状态,层数全部回溯为四天前数值;清除时获得[清除数*20]魔力供给(演示占位)。");

    /* 雾夜的散步者A:常驻,避免广泛侦查暴露;夜回合获得同等级[气息遮蔽] */
    r = db_skill(w, "雾夜的散步者", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "自身不会因其他单位出目>5的[广泛侦查]暴露所处灵脉的[单位信息];当前灵脉处于[夜]回合时,以相同等级模板给予自身[气息遮蔽](演示占位)。");

    /* 气息感知A:常驻,[侦查]判定+50%;低等级[气息遮蔽]不阻碍自身侦查 */
    r = db_skill(w, "气息感知", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 50, -2));
    db_set_text(w, r, "自身发起的[侦查]判定+50%成功率,且等级不高于此技能的[气息遮蔽]不影响自身[侦查];加符时敌方[气息遮蔽]胜率补正减半(自身主力位则无效化);EX时[侦查]出目固定1且任意隐藏信息效果对自身无效(演示)。");

    /* 鉴识眼A:常驻,情报调查+20%;持信息目标可给予属性+10常驻(演示) */
    r = db_skill(w, "鉴识眼", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, 1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_set_text(w, r, "使自身发起的[情报调查]+20%成功率,并允许随时对所处灵脉的从者进行[资料分析];自身持信息的目标处当前灵脉时,消耗行动阶段给予其任一属性+10常驻补正(至多4次,满时+10%胜率);EX时[资料分析]+20%且可对御主[情报调查](演示)。");

    /* 绝冻的魅力A:常驻,[主力位]己方全体+5%胜率并免疫冻结属性惩罚 */
    r = db_skill(w, "绝冻的魅力", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -1));
    db_eff(w, r, E_MANA(EF_MANA_UP, 5, -2));
    db_set_text(w, r, "[主力位]给予己方战斗位所有单位+5%胜率补正并使其免疫[冻结]带来的属性惩罚(持[冻结]者补正提升至15%);B级起自身持[冻结]时给敌全体-5%胜率;A级起按自身[冻结]层数给[魔力]+5/层(演示)。");

    /* 兽性的豪腕A:战斗开始时,[主力位]以分配[筋力]/2给予筋力常驻补正(至多30) */
    r = db_skill(w, "兽性的豪腕", KS_T_TALENT, KS_RANK_A, KS_WHEN_BATTLE_START, 0, 6, KS_F_MAIN);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 30, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_set_text(w, r, "[主力位]建卡时[筋力]分配上限+20;发动时给予自身[筋力]+[分配筋力/2]常驻补正(至多+30,回合结束时失去);主要工序时[筋力]为战斗属性且[优势]时额外+10%胜率(敌方存在[魔兽/猛兽]则翻倍);与人宝具解放联动(演示)。");

    /* 魔力放出(冰)A:随时,敌单体属性-15并120%[冻结]判定 */
    r = db_skill(w, "魔力放出(冰)", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 15, 1, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 15, 1));
    {
        E e = E_STATUS(S_FREEZE, 1, 1);
        e.chance = 100; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "给予敌方非[支援位]一名单位[筋力/耐久/敏捷]任一项-15属性惩罚,并对其发起120%[冻结]判定(目标耐久≥40减半,演示100%),成功给予[冻结1]。");

    /* 鲜花战争A:随时,[交流]中指定单位宣言袭击;胜利方全体+40魔供(演示) */
    r = db_skill(w, "鲜花战争", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 0, 9, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 20, -1));
    db_set_text(w, r, "仅[交流]中解放:指定当前灵脉任一单位立即宣言[袭击](双方战斗位至多3名,自身战斗工序内无法撤退);本场战斗胜利方全体+40魔力供给(非主力减半,演示20);己方胜利时自身全属性+5常驻(至多+40,自阵营击杀御主也+5,翻倍条件)。");

    /* 魔力放出(风)A:随时,[反击]敌单体[筋力][敏捷]-15;冲锋联动(演示) */
    r = db_skill(w, "魔力放出(风)", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 15, 1, KS_F_COUNTER);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 15, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 15, 1));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 0));
    db_set_text(w, r, "[反击]指定敌方战斗位任一单位,给予其[筋力][敏捷]-15属性惩罚;战斗位任一单位发起[冲锋]时允许[反击]发动:敌方目标[冲锋]判定减半(无判定则取消冲锋),己方目标改为等量补正并[冲锋]+50%(EX时首效给敌全体-10%胜率,演示)。");

    /* 圣杯的宠爱A:常驻,[幸运]+40常驻补正;获得魔供时下次判定+5% */
    r = db_skill(w, "圣杯的宠爱", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 40, -2));
    db_set_text(w, r, "降临时通告全局;自身[幸运]+40常驻补正;自身获得魔力供给时,下一次自身发起的判定+5%基础成功率(至多叠5次);存活单位无同等级[圣杯的宠爱]时,圣杯降临变更为当前灵脉;EX时圣杯供魔期间轮次结束额外+圣杯供魔/2(演示)。");

    /* 军师的忠言A:行动阶段,给予同灵脉单位唯一最低属性+20常驻 */
    r = db_skill(w, "军师的忠言", KS_T_BLESS, KS_RANK_A, KS_WHEN_ACT, 5, 9, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 20, 1));
    db_set_text(w, r, "给予一名同灵脉单位唯一最低的属性+20常驻补正(若其最低属性变化,可给予其等级≤此技能的已知技能宝具一枚加符并自身等级-1);以此法补正无法叠加,目标参与一场[完整战斗]后失去(演示耐久)。");

    /* 神的护佑A:常驻,特性[神性];+30等级;受负面判定按等级差减成功率 */
    r = db_skill(w, "神的护佑", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_RES_UP, 15, -2));
    db_set_text(w, r, "给予自身[特性赋予:神性];始终+30等级补正;成为他方负面判定对象时,那个判定受[-(等级差/2)%]最终成功率惩罚(至多-30%);轮次结束时若为灵脉主且非游荡,+[等级补正/2]魔供;战斗胜利时+等级/2魔供(演示抗性近似)。");

    /* 湖之加护A:常驻,战斗结束魔力<0时补足至0(演示) */
    r = db_skill(w, "湖之加护", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 40, -2));
    db_set_text(w, r, "自身参与的战斗结束时,若魔力低于0,补足至0并使此技能失去6回转(至多+50且不超过自身等级;回转满前无法再次生效);取得上限魔供时可展露真名额外获得等量补足(演示40)。");

    /* 千貌A:随时,改变外貌/性别/特性,职阶变化为Caster/Assassin/Berserker */
    r = db_skill(w, "千貌", KS_T_BLESS, KS_RANK_A, KS_WHEN_ANY, 0, 3, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "改变自身[外貌描述][性别][保有特性];使自身职阶变为[Caster/Assassin/Berserker]之一并改对应职阶属性(无法变为当前职阶),原职阶职阶技能[封印],以E级模板获取新职阶职阶技能与任一从者技能;变职后对自身非原职阶的[真名猜测]默认失败(演示占位)。");

    /* 幽弋A:常驻,夜回合可瞬移;成为技能目标时夜回合无效化其效果 */
    r = db_skill(w, "幽弋", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    {
        E e = E_WIN(EF_EFFECT_IM, 1, -2);
        e.cond = KC_NIGHT;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "1)每回合限一次,夜回合时指定任一其他处于[夜]的灵脉立即进入;2)战斗中撤退时夜回合可立即进入他处[夜]灵脉;3)一回合一次,在[夜]灵脉成为任一技能目标时,使那个技能对自身即将产生的效果无效化(演示夜条件免疫)。");

    /* 月之湖A:常驻,降临支付80魔赋予灵脉[月之湖]:己方抗性+20、战斗时自身全属性+20 */
    r = db_skill(w, "月之湖", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_RES_UP, 20, -1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 20, -2));
    db_set_text(w, r, "降临时通告全局并支付80魔力,赋予降临灵脉[月之湖](魔力量+25,灵脉上自阵营单位[抗性上升:+20%];全场唯一,该灵脉无法圣杯降临);[月之湖]灵脉战斗时自身除[宝具]外全属性+20(演示);判定上视为[神殿](规模0/10)。");

    /* 月女神的压力C:常驻,抗性破除:疲惫;全属性+10常驻;始终[疲惫1] */
    r = db_skill(w, "月女神的压力", KS_T_BLESS, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 10, -2));
    db_eff(w, r, E_STATUS(S_TIRED, 1, -2));
    db_set_text(w, r, "自身始终具有[抗性破除:疲惫];始终给予除[宝具]外全属性+10常驻补正;此技能生效期间始终[疲惫1];每参与两场战斗或战斗胜利获得一枚加符(加符+5补正,4枚时EX);三属性合计>270时[疲惫]带来的[即死]判定默认失败,<270且疲惫≥5时每回合结束结算[疲惫]即死(演示)。");

    /* 太平要术A:随时,敌主力灼伤/感电/冻结各30%判定 */
    r = db_skill(w, "太平要术", KS_T_WEAPON, KS_RANK_A, KS_WHEN_ANY, 25, 3, 0);
    {
        E e = E_STATUS(S_BURN, 3, 1); e.chance = 30; e.chance_neg = 1; db_eff(w, r, e);
        e = E_STATUS(S_ELECTRIC, 3, 1); e.chance = 30; e.chance_neg = 1; db_eff(w, r, e);
        e = E_STATUS(S_FREEZE, 3, 1); e.chance = 30; e.chance_neg = 1; db_eff(w, r, e);
    }
    db_set_text(w, r, "指定敌方主力位:依次进行30%[灼伤]/[感电]/[冻结]判定,各成功给予[灼伤3]/[感电3]/[冻结3](原文为1D5/4/3层,演示取3);自身参战战斗中这些状态首次赋予战斗位单位时,此技能立即获1回转(EX时2)。");

    /* 大江之鬼闹A:常驻,特性[魔性];+10等级与三属性+30常驻补正 */
    r = db_skill(w, "大江之鬼闹", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 20, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 30, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 30, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 30, -2));
    db_set_text(w, r, "始终给予自身[特性赋予:魔性](不因无效化失去);始终给予+10等级补正与[筋力][耐久][敏捷]+30常驻补正;进行[魂食]的回合内[状态免疫:弱化状态];进行[恶性/无限制魂食]时三属性额外+20;2天未进行恶性魂食时补正暂时无效化(演示)。");

    /* 救世的航海家A:常驻,[主力位]己方全体+5%胜率与[抗性上升:+15%] */
    r = db_skill(w, "救世的航海家", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -1));
    db_eff(w, r, E_WIN(EF_RES_UP, 15, -1));
    db_set_text(w, r, "[主力位]自身发起成功的[冲锋]判定后,本场战斗己方战斗属性均不为[劣势];给予己方战斗位全部单位+5%胜率与[抗性上升:+15%];B级起己方单位首次负面判定失败时+10%胜率;EX时己方+[抗性上升/2]%胜率(演示)。");

    /* 替罪羊E:随时,善/恶宣言,宣言相同且为[善]时替身承受效果 */
    r = db_skill(w, "替罪羊", KS_T_MAGIC, KS_RANK_E, KS_WHEN_ANY, 20, 6, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN_CONST, A_LUK, 40, -2));
    db_set_text(w, r, "仅战斗外发动:指定除自身外当前灵脉任一非仆役单位,双方各暗宣言[善/恶];均为[善]时,本回合自身下一次成为技能/宝具效果对象时改由目标承受;宣言不同时自身[幸运]-40常驻惩罚;目标宣言与其阵营不同时其产生40魔力消耗(演示占位)。");

    /* 影灯笼A:常驻,夜回合隐藏宝具信息;夜回合结束+30魔力供给 */
    r = db_skill(w, "影灯笼", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, 0);
    {
        E e1035 = E_MANA(EF_MANA_UP, 30, -2); e1035.cond = KC_NIGHT; db_eff(w, r, e1035);
    }
    db_set_text(w, r, "当前灵脉处于[夜]回合时隐藏自身宝具信息,并在战斗开始与初始工序将自身属性在战斗计算表上隐藏;所处灵脉为[夜]时,回合结束自身+30魔力供给(未参与战斗的轮次免除此技能魔耗);演示夜条件魔供。");

    /* ---- 军阵扩充包 · 宝具 ---- */

    /* 约柜A:随时,生成[魔术结界:因许之地];结界被摧毁时全场[即死]判定 */
    r = db_np(w, "约柜", KS_NP_BOUND, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_ANY, 100, 9, 0);
    {
        E e = E_DEATH(40, 0, 1);
        e.chance = 60; e.cond = KC_NONE;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "方式1:行动阶段解放,赋予当前灵脉[魔术结界:因许之地](自身视为其灵脉主并始终知悉信息);方式2:存在结界时对自身40%[即死]并转移结界至当前灵脉;结界被摧毁时灵脉全体产生40魔力消耗,随后60%[即死]判定(幸运≥40减半,演示40%档);灵脉[人流量]显示为0,有单位[魂食]时结界摧毁。");

    /* 长坂坡一骑破阵A:随时,介入时解放,30%[即死]并全属性+20(演示) */
    r = db_np(w, "长坂坡一骑破阵", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_A,
              KS_WHEN_ANY, 60, 18, 0);
    db_eff(w, r, E_DEATH(30, 1, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 20, -2));
    db_set_text(w, r, "方式1:宣言[介入]时解放,到达后赋予自身[无敌贯通],对同灵脉其他[从者]30%[即死](幸运≥20减半);方式2:自阵营御主被袭击时解放,立即[机动]并给予状态免疫;加入战斗后自身全属性+20属性补正(敌方每名从者再+5),给自阵营御主[保护](演示)。");

    /* 花开堪折直须折A:初始工序,[支援][反击]按满足条件数进行即死判定 */
    r = db_np(w, "花开堪折直须折", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_A,
              KS_WHEN_PROC, 60, 12, KS_F_ASSIST | KS_F_COUNTER);
    db_eff(w, r, E_DEATH(60, 1, 1));
    db_set_text(w, r, "[支援][反击]建卡时记录5项条件(仅自阵营/御主可退位/回路补给/受即死者回避/敏捷最高者迟滞/人流量0),解放时依序执行满足的选项并移除;对敌主力进行[移除选项数*20%]的[即死]判定(幸运≥40减半,演示60%档)。");

    /* 不归之匕B:战斗开始时,[主力位]最终工序60+敏捷差%[即死]判定 */
    r = db_np(w, "不归之匕", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_B,
              KS_WHEN_BATTLE_START, 20, 9, KS_F_MAIN);
    db_eff(w, r, E_DEATH(60, 1, 1));
    db_set_text(w, r, "[主力位]仅建卡获取;解放时不暴露宝具效果,本场战斗自身无法在战斗工序内撤退;最终工序对敌方[主力位]进行[60+敏捷差]%[即死]判定(目标持此宝具效果信息时减半),成功则不付令咒立即退场(演示)。");

    /* 怀抱你的希望之星A:随时,[反击][支援位]己方单位成为敌方效果对象时给予[回避] */
    r = db_np(w, "怀抱你的希望之星", KS_NP_ARMORY, FC_DEFENSE, KS_RANK_A,
              KS_WHEN_ANY, 70, 12, KS_F_COUNTER | KS_F_SUPPORT);
    db_eff(w, r, E_WIN(EF_EVADE, 0, -1));
    db_set_text(w, r, "[反击][支援位]己方战斗位除自身外任一单位成为不来源己方战斗位的技能/宝具效果对象时解放:该单位首次遭对象时,若来源等级不高于此宝具,给予其[回避];其已持[回避]/[无敌]时,改令其撤退FP-1且不产生等级魔耗;判定上视为[结界]宝具(演示)。");

    /* 我将根绝一切毒物，一切害物A:随时,[反击]清除己方指定异常状态 */
    r = db_np(w, "我将根绝一切毒物，一切害物", KS_NP_ARMORY, FC_DEFENSE, KS_RANK_A,
              KS_WHEN_ANY, 70, 9, KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_STATUS_REMOVE, S_POISON, -1));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 0));
    db_set_text(w, r, "[反击]宣言一项[异常状态]解放:清除己方战斗位全部单位的该[异常状态],受效单位解放的下一个宝具,对敌方非[支援位]目标额外造成-40%胜率惩罚;每清除一名单位的异常,此宝具获3回转(演示中毒清除)。");

    /* 万象之伪誊抄A:初始工序,蓄力记录双方技能宝具给予自身的惩罚,最终工序返还(演示) */
    r = db_np(w, "万象之伪誊抄", KS_NP_HUMAN, FC_DEFENSE, KS_RANK_A,
              KS_WHEN_PROC, 20, 18, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 0));
    E e1036 = E_STATUS(S_CHARGE, 2, -2); e1036.flag = EF_CHARGE; db_eff(w, r, e1036);
    db_set_text(w, r, "初始工序宣言[蓄力]:己方战斗位仅为自阵营时给予御主[保护];记录蓄力期间双方技能宝具给予自身的属性/胜率惩罚;最终工序给予敌非[支援位]全体[记录总值%]胜率惩罚(记录属性惩罚>60或胜率惩罚>60%时支付溢出等量魔力;战斗提前结束则获9回转,演示40)。");

    /* 余晖，灾祸的血之城堡A:战斗开始时,[主力位]固有结界:全场[冻结1],自身抗性+20 */
    r = db_np(w, "余晖，灾祸的血之城堡", KS_NP_ARMORY, FC_DEFENSE, KS_RANK_A,
              KS_WHEN_BATTLE_START, 80, 9, KS_F_MAIN);
    db_eff(w, r, E_STATUS(S_FREEZE, 1, 0));
    db_eff(w, r, E_WIN(EF_RES_UP, 20, -2));
    db_eff(w, r, E_STATUS(S_FREEZE, 3, 0));
    db_eff(w, r, E_STATUS(S_FREEZE, 1, -2));
    db_set_text(w, r, "[主力位]解放生成[固有结界:冰之城堡](宽6/初始工序无法撤退;结界外非自阵营单位的技能宝具对结界内无效);结界存在时始终给予当前灵脉所有单位[冻结1],给予自身[抗性上升:+20%];战斗开始可指定一方战斗位给[冻结3];每工序可支付20魔力给自己与敌非[支援位][冻结1](演示)。");

    /* 石兵八阵A:战斗开始时,[支援]魔术结界:按战术触发晕眩/胜率等效果(演示) */
    r = db_np(w, "石兵八阵", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_BATTLE_START, 70, 8, KS_F_ASSIST);
    {
        E e = E_STATUS(S_STUN, 3, 0);
        e.chance = 50; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_UP, 30, -2));
    db_eff(w, r, E_STATUS(S_CONFUSE, 1, 0));
    }
    db_set_text(w, r, "[支援]赋予[魔术结界:石兵八阵](覆盖既有结界,一场[完整战斗]后摧毁):己方宣言[扼守]时对敌非[支援位]全体50%[晕眩3]判定(全部成功则改[混乱1]);[强击]时自身+敌单位数*25%胜率(演示30);[破袭]时敌撤退FP+1;[试探]时获7回转;克制关系成立时效果增强。");

    /* 混元一阵A:战斗开始时,[支援]魔术结界:最终工序宣言属性-40惩罚 */
    r = db_np(w, "混元一阵", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_BATTLE_START, 70, 9, KS_F_ASSIST);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 40, 1));
    db_set_text(w, r, "[支援]赋予[魔术结界:混元一阵]:己方非[支援位]存在至少2名单位时,1)任意工序开始可重宣言[战术](每场一次);2)主要工序指定敌非[支援位]任一单位宣言[负面效果]给予[抗性破除];3)最终工序指定敌非[支援位]任一单位宣言[战斗属性]给予-40属性惩罚(演示3)。");

    /* 月女神的爱箭恋矢A:随时,[支援]敌单体-25%胜率并清除异常转魅惑 */
    r = db_np(w, "月女神的爱箭恋矢", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_ANY, 50, 3, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 25, 1));
    db_eff(w, r, E_STATUS(S_CHARM, 3, 1));
    db_set_text(w, r, "[支援]给予敌方战斗位任一单位-25%胜率惩罚(为[异性]时翻倍),本场剩余工序其低等级宝具无法获得回转;目标有处于回转的宝具时失去3回转;目标与自身战斗位种类相同/处[支援位]时清除其至多3项[异常状态]并给予等量[魅惑](演示);EX时追加[-(自身魅惑层数*5)%]惩罚。");

    /* 苍天已死，黄天当立A:战斗开始时,[主力位]使魔无效+支援混乱+三篇宣言(灼伤/冻结/感电)属性惩罚 */
    r = db_np(w, "苍天已死，黄天当立", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_BATTLE_START, 60, 6, KS_F_MAIN);
    db_eff(w, r, E_STATUS(S_CONFUSE, 1, 0));      /* 敌方全体混乱1 */
    {
        E e = E_STATUS(S_CONFUSE, 1, -1);          /* 己方全体混乱1(演示:规则书为支援位) */
        db_eff(w, r, e);
        E e1037 = E_ATTR(EF_ATTR_DOWN, A_STR, 15, 0); db_eff(w, r, e1037);   /* 敌方非支援全部 筋力-15 */
        E e1038 = E_ATTR(EF_ATTR_DOWN, A_END, 15, 0); db_eff(w, r, e1038);   /* 耐久-15 */
        E e1039 = E_ATTR(EF_ATTR_DOWN, A_AGI, 15, 0); db_eff(w, r, e1039);   /* 敏捷-15 */
        E e1040 = E_ATTR(EF_ATTR_DOWN, A_MAG, 15, 0); db_eff(w, r, e1040);   /* 魔力-15 */
        E e1041 = E_ATTR(EF_ATTR_DOWN, A_LUK, 15, 0); db_eff(w, r, e1041);   /* 幸运-15 */
        E e1042 = E_ATTR(EF_ATTR_DOWN, A_NP, 15, 0);  db_eff(w, r, e1042);   /* 宝具-15 */
        E e1043 = E_STATUS(S_BURN, 1, 0);             db_eff(w, r, e1043);   /* 宣言[灼伤]:敌方获得灼伤1(演示) */
        E e1044 = E_STATUS(S_ELECTRIC, 1, 0);         db_eff(w, r, e1044);   /* 宣言[感电]:敌方获得感电1(演示) */
        E e1045 = E_STATUS(S_BURN, 0, -2); e1045.flag = EF_PLEDGE_DECL; e1045.value = 3; e1045.status = S_BURN; db_eff(w, r, e1045);
    }
    db_set_text(w, r, "[主力位]解放后使当前灵脉全部[使魔]本场无效化,无效他处获取本灵脉[灵脉信息][单位信息]的效果,给予双方[支援位]单位[混乱1]。每个战斗工序开始时,自身与敌方主力位各可暗宣言[灼伤][冻结][感电]之一;宣言相同则本工序此宝具效果失效,宣言不同则给予目标该状态[3/2/1]层。宣言[灼伤]:敌方非[支援位]全部单位[筋力][耐久]-15/10/5并立即结算[灼伤];[冻结]:[敏捷][魔力]-15/10/5,战斗胜利时[抗性下降:-15/10/5%];[感电]:[幸运][宝具]-15/10/5,最终工序对受罚单位[激荡](演示:六属性-15+双方支援混乱+灼伤/感电各1)。");

    /* 兰陵王入阵曲A:随时,[骑乘]冲锋后恐惧判定;己方+10%胜率,敌三属性-20 */
    r = db_np(w, "兰陵王入阵曲", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_ANY, 60, 9, KS_F_RIDE);
    {
        E e = E_STATUS(S_FEAR, 1, 0);
        e.chance = 70; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_UP, 10, -1));
        db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 20, 0));
        db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 20, 0));
        db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 20, 0));
    }
    db_set_text(w, r, "[骑乘]仅宣言[冲锋]时解放;冲锋结算后对敌全体进行[40%]恐惧判定(本场成功[冲锋]过则翻倍,演示70%),成功给[恐惧](全部成功则改[混乱1]);本回合己方战斗位全部单位+10%胜率,敌全体[筋力][耐久][敏捷]-20惩罚;敌持[恐惧]/[混乱]时其再-5(演示)。");

    /* 恶雾将与伦敦的破晓一同毁灭消逝A:随时,特性[魔性];全属性+5并按人流量/恐惧增长(演示) */
    r = db_np(w, "恶雾将与伦敦的破晓一同毁灭消逝", KS_NP_ARMORY, FC_BUFF, KS_RANK_A,
              KS_WHEN_ANY, 40, 13, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_MAG, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_LUK, 5, -2));
    db_set_text(w, r, "建卡时本职[狂化]始终[封印];战斗中解放:给予自身[特性赋予:魔性],始终全属性+5属性补正,当前灵脉每1人流量/每名持[恐惧]的非仆役单位再+5(人流量部分至多+35);敌方主力仅[人型]时其[随机属性]始终[劣势];有此[封印]狂化等级时再给四属性+25(演示)。");

    /* 枪手们啊，挑战风车吧A:战斗开始时,[支援]己方主力御主按等级差获得属性补正 */
    r = db_np(w, "枪手们啊，挑战风车吧", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_BATTLE_START, 40, 6, KS_F_ASSIST);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 20, -1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 20, -1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 20, -1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_MAG, 20, -1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_LUK, 20, -1));
    db_set_text(w, r, "[支援]仅以[御主]为目标:给予己方[主力位]御主全属性[+等级差]属性补正(己方主力初始等级不低于敌方时减半,演示20),并将自身任一技能以同等级模板赋予目标;效果回合结束失去;解放后此宝具等级-1(低于E破弃);加符时己方主力+等级差%胜率。");

    /* 战神的军带A:常驻,视为对军:自身全属性+20属性补正(回转不为0时无效) */
    r = db_np(w, "战神的军带", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_PASSIVE, 20, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_MAG, 20, -2));
    db_set_text(w, r, "三种效果:1)[对人]持[神性]技能则等级+1,否则赋予[神性E];2)[对军]给予自身除[幸运][宝具]外全属性+20属性补正(回转不为0时无效,演示);3)[对城]战斗开始宣言失去9回转,给予任一低等级宝具加符并使其本场视为[对城]解放。");

    /* 月女神的纯洁之爱A:战斗开始时,[主力位]给予自身[疲惫4]并三属性+25;随机属性+疲惫*5 */
    r = db_np(w, "月女神的纯洁之爱", KS_NP_ARMORY, FC_BUFF, KS_RANK_A,
              KS_WHEN_BATTLE_START, 60, 9, KS_F_MAIN);
    db_eff(w, r, E_STATUS(S_TIRED, 4, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 25, -2));
    db_set_text(w, r, "[主力位]解放时给予自身[疲惫4],本场战斗自身免疫[疲惫]带来的常驻惩罚;始终给予[筋力][耐久][敏捷]+25属性补正与[随机属性]+5*疲惫层数补正;三属性合计高于敌主力时额外[无敌贯通]与[必中];EX时+10*疲惫层数%胜率。仅能经[月女神的压力]获取(演示)。");

    /* 炎门之守护者A:随时,[主力位]召唤自身等级的召唤物至仆役位(演示) */
    r = db_np(w, "炎门之守护者", KS_NP_ARMORY, FC_SUMMON, KS_RANK_A,
              KS_WHEN_ANY, 70, 9, KS_F_MAIN);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 40; e.cond_arg = 200; e.cond_arg2 = TR_HUMAN;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "炎门之守护者:召唤(等级=自身演示40 全属性50)";
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[主力位]解放此宝具的战斗中自身无法撤退,召唤物战斗结束退场;方式1:战斗开始时移除双方[支援位](其上单位进入[辅助位]),召唤等级=自身、全属性50的召唤物至仆役位(至多60级),允许非[主力位]单位宣言[死斗](胜率减半);方式2:[反击]己方单位成为目标时召唤[耐久]型召唤物给予其[保护](演示)。");

    /* 天之公牛A:随时,召唤神性魔兽[巨大]召唤物(2战斗位) */
    r = db_np(w, "天之公牛", KS_NP_ARMORY, FC_SUMMON, KS_RANK_A,
              KS_WHEN_ANY, 70, 12, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 40; e.cond_arg = 160; e.cond_arg2 = TR_BEAST;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "天之公牛:召唤(等级40 总属性160 神性魔兽)";
        db_eff(w, r, e);
    }
    db_set_text(w, r, "召唤等级40、总属性160、[神性][魔兽][独特]的召唤物(战斗开始时解放且有空位可入仆役位);其处于战斗位时,同灵脉全部从者全属性[-(5*占用战斗位数)](宝具属性减半);支付40魔力使其全属性+5常驻、等级+10并额外占用1战斗位(2位时[巨大],4位时[超巨大],至多5位,演示1体)。");

    /* 诺亚方舟A:常驻/随时,[主力位][骑乘]额外灵脉:召唤物与敌方远程胜率惩罚(演示) */
    r = db_np(w, "诺亚方舟", KS_NP_WORLD, FC_SUMMON, KS_RANK_A,
              KS_WHEN_ANY, 30, 3, KS_F_MAIN | KS_F_RIDE);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 60; e.cond_arg = 180; e.cond_arg2 = TR_HUMAN;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "诺亚方舟:召唤(等级60 全属性30)";
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 0));
    }
    db_set_text(w, r, "[主力位][骑乘]降临时生成[额外灵脉:诺亚方舟](宽7,魔量=人流量*5,无法[魂食],灵脉上单位[抗性上升:+10%]);建卡时[暴风雨的航海家]变为[救世的航海家];灵脉主时:回合开始宣言灵脉[干涉]/行动阶段转移人流量/主要工序支付80魔力给予敌全部不在此灵脉的单位-40%胜率(非主力减半,演示);战斗中可按主力许可召唤等级60全属性30的召唤物。");

    /* 转身火生三昧A:战斗开始时,特性[龙种];按灼伤单位胜率补正(演示) */
    r = db_np(w, "转身火生三昧", KS_NP_HUMAN, FC_STATUS, KS_RANK_A,
              KS_WHEN_BATTLE_START, 40, 9, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    db_set_text(w, r, "发动后给予自身[特性赋予:龙种],本场战斗给予自身[双方持[灼伤]的其他单位补正合计值%]胜率补正(至多[灼伤单位数*35%],每单位至多70%,演示20);解放时自身灼伤层数为全场唯一最高时,令咒宣言给全场[灼伤1]并[状态免疫:异常状态]至本回合结束;战斗结束转移全场[灼伤]并+5*层数魔供。");

    /* 到来吧，冥途啊，到来吧A:战斗开始时,[主力位]固有结界:全场50%[中毒2]判定 */
    r = db_np(w, "到来吧，冥途啊，到来吧", KS_NP_WORLD, FC_STATUS, KS_RANK_A,
              KS_WHEN_BATTLE_START, 100, 12, KS_F_MAIN);
    {
        E e = E_STATUS(S_POISON, 2, 0);
        e.chance = 50; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_STATUS(S_STATE_IM, S_POISON, -1));
    E e2; memset(&e2, 0, sizeof(e2)); e2.chance_attr_base = -1; e2.flag = EF_TICK_PROC; e2.status = S_POISON; e2.value = 10; e2.chance = 0; e2.target = 0; db_eff(w, r, e2); /* EF_TICK_PROC 每工序结算 */
    }
    db_set_text(w, r, "[主力位]仅己方战斗位存在自阵营御主时解放:生成[固有结界:冥途](宽5/初始工序无法撤退;结界内退场者判定+20%);给予自阵营御主[状态免疫:中毒];每工序开始对同灵脉除自身外所有单位50%[中毒2]判定(演示);结界内从者退场时按持毒单位数+5%胜率;EX时结界获得人流量并每1人流量+5%胜率。");

    /* 掎角一阵C:战斗开始时,[无敌贯通]蓄力后敌非支援-胜率并无法撤退(演示) */
    r = db_np(w, "掎角一阵", KS_NP_ARMORY, FC_SPECIAL, KS_RANK_C,
              KS_WHEN_BATTLE_START, 30, 9, KS_F_INV_PIERCE);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 0));
    E e1046 = E_STATUS(S_CHARGE, 2, -2); e1046.flag = EF_CHARGE; db_eff(w, r, e1046);
    db_set_text(w, r, "[无敌贯通]战斗开始解放宣言[蓄力];蓄力期间己方非[支援位]单位多于敌方时,敌方非[支援位]全体撤退FP+1;最终工序开始时给予敌方非[支援位]全体[-(10*己方非支援位单位数)%]胜率惩罚并令其无法在最终工序撤退(演示20)。");

    /* 剑、饥馑、死、兽A:随时,[主力位]固有结界内毒发结算/夺召唤物控制权等(演示) */
    r = db_np(w, "剑、饥馑、死、兽", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_A,
              KS_WHEN_ANY, 30, 3, KS_F_MAIN);
    db_eff(w, r, E_STATUS(S_POISON, 1, 1));
    db_set_text(w, r, "[主力位]仅建卡获取;建卡记录一件[对人]宝具;仅当前灵脉存在来源于自身的[固有结界]且敌方主力持[人型]时可解放,从以下四效果选一:1)获取记录的宝具立即解放;2)结界内任一单位产生40魔力消耗;3)结界内全部持[中毒]单位立即结算[中毒](退场者全场[中毒2]);4)夺取敌方[猛兽/魔兽/龙种]召唤物控制权;四效果同回合全生效则灵脉人流量归0(演示中毒)。");

    /* 现为裁决之刻，上告汝之名来A:随时,[主力位]敌非主力至多3名退场,每名-5%胜率 */
    r = db_np(w, "现为裁决之刻，上告汝之名来", KS_NP_ARMORY, FC_SPECIAL, KS_RANK_A,
              KS_WHEN_ANY, 70, 12, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 15, 1));
    db_set_text(w, r, "[主力位]仅己方战斗位只有自身且本场未对战斗外单位[冲锋]时可解放:指定敌方除[主力位]外至多3名单位退出战斗位(不算撤退);每退出一名给予敌方主力位-5%胜率惩罚(目标为恶阵营或进行过魂食再各+5,演示15)。");

    /* 无铭胜利之剑A:初始工序,替换[对魔力]为[气息遮蔽];敌方视为初次遭遇 */
    r = db_np(w, "无铭胜利之剑", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_A,
              KS_WHEN_PROC, 20, 9, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "解放时,本场战斗中将自身[对魔力]替换为同等级的[气息遮蔽];敌方战斗位不存在知悉自身[真名]的单位时,对方战斗位单位本场视为初次与自身处于敌对战斗位战斗(演示占位)。");

    /* 汝即是龙C:战斗开始时,[主力位]赋予敌主力[龙种];敌方每龙种+10%胜率 */
    r = db_np(w, "汝即是龙", KS_NP_ARMORY, FC_ANTITRAIT, KS_RANK_C,
              KS_WHEN_BATTLE_START, 30, 9, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_set_text(w, r, "[主力位]本场战斗中赋予敌方[主力位][龙种]特性(文本);敌方战斗位每存在一名持[龙种]特性的单位,自身+10%胜率补正并令自身发起的全部判定+10%成功率(仆役减半);敌方主力宣言撤退时FP+1(演示)。");

    /* ================================================================
     * 全量录入 批次9:《空想从者资源库》赤幕扩充包
     * ================================================================ */

    /* 当代不吉A:常驻,任一保有技能视为[兵器];同等级单位[幸运]-10 */
    r = db_skill(w, "当代不吉", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_LUK, 10, 0));
    db_set_text(w, r, "建卡时令自身任一保有技能同时视为[类型:兵器]与[面向:兵器];始终给予与自身等级相同的所有同灵脉单位[幸运]-10属性惩罚(演示)。");

    /* 龙之吐息A(天赋):随时,特性[龙种];敌非支援全体-10%胜率惩罚 */
    r = db_skill(w, "龙之吐息", KS_T_TALENT, KS_RANK_A, KS_WHEN_ANY, 20, 3, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 0));
    E e1047 = E_STATUS(S_CHARGE, 1, -2); e1047.flag = EF_CHARGE; db_eff(w, r, e1047);
    db_set_text(w, r, "始终给予自身[特性赋予:龙种](不因无效化失去);发动时给予敌方非[支援位]全体-10%胜率惩罚(非主力减半);支付10魔力宣言[蓄力],下一工序开始时结算并再生效一次(至多宣言5次);加符时额外给受效单位[灼伤1];EX时改战斗开始并蓄力后-30%(演示)。");

    /* 龙之吐息(炎)A:随时,特性[龙种];对敌非支援全体5次40%[灼伤]判定 */
    r = db_skill(w, "龙之吐息(炎)", KS_T_TALENT, KS_RANK_A, KS_WHEN_ANY, 20, 3, 0);
    {
        E e = E_STATUS(S_BURN, 1, 0);
        e.chance = 40; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "始终给予自身[特性赋予:龙种];发动时对敌非[支援位]全体进行5次40%[灼伤]判定(仅一名目标时+20%),每次成功给予[灼伤1](演示单次);加符时敌方仅主力位存在则判定+30%;EX时战斗开始宣言工序蓄力,给予[灼伤2]且每工序敌主力+1层。");

    /* 吸血A(天赋):常驻/随时,魂食回合三属性+10;40%判定转移目标魔力 */
    r = db_skill(w, "吸血", KS_T_TALENT, KS_RANK_A, KS_WHEN_ANY, 10, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 10, -2));
    E e = E_ATTR(EF_ATTR_DOWN_CONST, A_MAG, 10, 1); e.chance = 40; e.chance_neg = 1; db_eff(w, r, e);
    e = E_STATUS(S_CHARM, 1, 1); e.chance = 40; e.chance_neg = 1; db_eff(w, r, e);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_MAG, 10, -2));
    db_set_text(w, r, "通过[魂食]获得魔供的回合内,给予自身除[幸运][宝具]外全属性+10常驻补正;3回合未获魂食则转为-10常驻惩罚并每回合+5;指定同灵脉任一单位40%负面判定(魂食回合+20%),成功转移其魔力并给[魅力]对应效果:目标[魔力]-10常驻、[魅惑1],自身[魔力]+10,目标魔力为0时[疲惫1];可同时宣言[袭击](演示)。");

    /* 嗜虐体质A:常驻,免疫疲惫属性惩罚;受异常/即死判定默认失败(演示) */
    r = db_skill(w, "嗜虐体质", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, -2));
    db_set_text(w, r, "自身免疫[疲惫带来的属性惩罚],每持[疲惫1]给自身[抗性下降:-15%];每消耗1FP给自身除[宝具]外全属性+5常驻;始终给予[+抗性下降总值%]胜率补正(至多基础耐久/2%);自身受到[异常/即死]判定时默认失败(每失败一次抗性下降+15%,演示免疫)。");

    /* 女帝的魅力A:常驻,[反击]敌方战斗位全体-10%胜率 */
    r = db_skill(w, "女帝的魅力", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 0));
    db_set_text(w, r, "[反击]仅[女性]单位允许持有;给予敌方战斗位全部单位-10%胜率惩罚;每场一次,成为等级≤此技能的技能效果对象时,令其即将造成的效果无效化并使那个技能额外失去一次回转;己方持更高等级[气质/魅力]技能时本场无效(演示)。");

    /* 信仰的加护A:常驻,[抗性上升:+25%];成功负面判定后+胜率(演示) */
    r = db_skill(w, "信仰的加护", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_RES_UP, 25, -2));
    db_set_text(w, r, "给予自身[抗性上升:+25%];战斗中发起成功负面判定或受到失败负面判定(来源等级不高于此技能)时,自身+10%胜率补正(非魅惑/恐惧/混乱判定减半;胜率补正至多不超过自身[抗性上升]数值);敌方存在[魔性]单位时此[抗性上升]翻倍(演示)。");

    /* 千里眼·业之瞳A:常驻,[资料分析]+30%;对持[灼伤]单位解放宝具获[无敌贯通] */
    r = db_skill(w, "千里眼·业之瞳", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 30, -2));
    db_set_text(w, r, "自身发起的[资料分析]+30%成功率;处于战斗灵脉时知悉同灵脉单位是否持[灼伤];战斗中自身对持[灼伤]的单位解放的宝具仅在那次解放获得[无敌贯通](演示);EX时任意等级生效且持[无敌贯通]的技能宝具生效时+10%胜率。");

    /* 金刚之体A:常驻,[状态免疫:灼伤]与[抗性上升:+10%];记录并免去所受惩罚 */
    r = db_skill(w, "金刚之体", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_STATUS(S_STATE_IM, S_BURN, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 10, -2));
    db_set_text(w, r, "给予自身[状态免疫:灼伤]与[抗性上升:+10%];即将受到胜率/属性惩罚时记录并免去(多项属性惩罚双倍计算;记录上限=基础[耐久],至多120,溢出正常生效);记录达上限后本回合此技能无效化,无效化期间始终受[-基础耐久/2%]胜率惩罚(至多-60%,演示)。");

    /* 魔力放出(光)A:随时,自身[魔力][幸运]+15常驻补正(回合结束失去) */
    r = db_skill(w, "魔力放出(光)", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 15, 1, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 15, -2));
    db_set_text(w, r, "给予自身[魔力][幸运]任一项+15常驻补正([昼]回合可额外支付15魔力令两项同时生效,演示两项);敌方主力位为[恶]阵营时本场自身胜率补正额外+5%;回合结束时失去(演示)。");

    /* 拷问技术A:常驻,自身成功负面判定后,目标受到的负面判定+10% */
    r = db_skill(w, "拷问技术", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 10, 0));
    db_set_text(w, r, "当自身对一名单位发起成功的负面判定后,其受到的负面判定始终+10%基础成功率(每工序每技能宝具至多触发3次);该成功率补正轮次结束时减半,未受自身成功负面判定的轮次结束时移除(演示)。");

    /* 神速A:常驻,[主力位]冲锋固定敏捷;每工序自身敏捷+10且敌主力45%[迟滞]判定 */
    r = db_skill(w, "神速", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, KS_F_MAIN);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 10, -2));
    {
        E e = E_STATUS(S_LAG, 1, 1);
        e.chance = 45; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[主力位]自身发起[冲锋]判定时允许将[敏捷]固定为判定属性;每个工序开始时自身[敏捷]+10属性补正,并令敌方主力位进行45%[迟滞]判定(成功给予[迟滞1]);目标已持[迟滞]或处[蓄力]时改为自身[敏捷]+10(至多+30);敌主力敏捷更高或持更高等级[技艺]技能时本场失效(演示)。");

    /* 圣索菲亚的祈祷C:常驻,轮次开始时+30魔供;同阵营单位[魔力][幸运]+15 */
    r = db_skill(w, "圣索菲亚的祈祷", KS_T_BLESS, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 30, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 15, -1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 15, -1));
    db_set_text(w, r, "轮次开始时(轮次数为7的倍数或不大于灵脉人流量):给予作为[灵脉主]的自身+30魔力供给;给予当前灵脉所有无[魔性]的同阵营单位[魔力][幸运]+15常驻补正(轮次结束失去);EX时对[魔性]单位同样生效且在敌方有[魔性]时翻倍(演示)。");

    /* 沉溺恋情潸然泪下C:常驻,首遇目标给予自身[魅惑1];全属性+10常驻 */
    r = db_skill(w, "沉溺恋情潸然泪下", KS_T_BLESS, KS_RANK_C, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_STATUS(S_CHARM, 1, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, -2));
    db_set_text(w, r, "建卡时记录[异性/同性]之一;给予自身[抗性破除:魅惑],造成[魅惑]的效果中[抗性下降:-20%];回合开始或[干涉]时,同灵脉存在首次遭遇的记录类型单位(演示魅惑1)则其给予自身[魅惑1](至多9层);始终给予自身全属性+10常驻补正;持自身来源[魅惑]且同灵脉时每2层全属性+5(此补正生效时无法撤退)。");

    /* 火力支援(炮)A:随时,[支援]敌非支援全体[灼伤1]与-5%胜率 */
    r = db_skill(w, "火力支援(炮)", KS_T_WEAPON, KS_RANK_A, KS_WHEN_ANY, 30, 2, KS_F_ASSIST);
    db_eff(w, r, E_STATUS(S_BURN, 1, 0));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 5, 0));
    db_set_text(w, r, "[支援]无法在[辅助位]发动;给予敌方非[支援位]全体[灼伤1];当前灵脉每存在归属于自身的[结阵1],再给敌非[支援位]全体-5%胜率(演示);自身处[支援位]时,解放的下个宝具首次造成效果时额外给敌-15%胜率。");

    /* 恸哭外装A:常驻,战斗开始[诅咒1];战斗中[筋力][敏捷]+20 */
    r = db_skill(w, "恸哭外装", KS_T_WEAPON, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_STATUS(S_CURSE, 1, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 20, -2));
    db_eff(w, r, E_STATUS(S_CONFUSE, 1, 1));   /* 宣言:移除自身诅咒2并给敌方任一单位混乱1(演示固定) */
    db_set_text(w, r, "战斗开始时给予自身[诅咒1];战斗中给予自身[筋力][敏捷]+20属性补正,自身发起的负面判定获得等同自身[诅咒]效果的成功率补正;每工序一次,宣言移除自身[诅咒2]并给予敌非[支援位]任一单位[混乱1](演示)。");

    /* 非常大权A:常驻,给予双方战斗位除自身外全部非仆役单位全属性-20 */
    r = db_skill(w, "非常大权", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 15, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 20, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 20, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 20, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_MAG, 20, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_LUK, 20, 0));
    db_set_text(w, r, "战斗中始终给予除自身外双方战斗位全部非[仆役]单位全属性-20属性惩罚([宝具]属性惩罚减半);持有[领袖气质EX/A/B]的单位不受影响;判定上名称视为[皇帝特权](演示)。");

    /* 终焉特权C:随时,[支援]成功真名猜测/资料分析的轮次内获取E级保有技能(演示) */
    r = db_skill(w, "终焉特权", KS_T_CROWN, KS_RANK_C, KS_WHEN_ANY, 20, 0, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "[支援]仅在他人对自身成功的[真名猜测][资料分析]轮次内可发动:获取E级任一非[常驻]从者保有技能并满足条件可发动(回合结束失去);任意单位对自身[真名猜测][资料分析]时自身立即知悉其结果;自身退场时给予同灵脉低等级单位[+等级差]等级补正并以C级模板给予[皇帝特权](演示占位)。");

    /* 王途踏破A:常驻,建卡记录特质;主力位时+记录特质数*15%胜率(演示2项) */
    r = db_skill(w, "王途踏破", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 30, -2));
    db_set_text(w, r, "建卡时记录5项特质(无法魂食/必须解放宝具/知悉恶阵营/契约与真言/冲锋死斗限制/战术限制;违背即移除记录);自身处[主力位]时始终给予[+记录特质数*15%]胜率补正(演示2项记录30%);EX时对魂食/混沌恶袭击可暂停回转并在战斗结束时恢复。");

    /* 可能性之光A:常驻,[主力位]隐藏属性星/人时抗性+30;每劣势属性+15%胜率 */
    r = db_skill(w, "可能性之光", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_RES_UP, 30, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 15, -2));
    db_set_text(w, r, "[主力位]若自身持[隐藏属性:星/人],给予自身[抗性上升:+30%];己方其他单位不持[天/星/兽]时,每有一项战斗中处于[劣势]的属性,自身+15%胜率;敌方战斗位每存在一个持[神性]特性的单位,自身知悉并额外+10%胜率(演示)。");

    /* 骥足百般A:随时,[支援]以记录类型技能替换[骑乘] */
    r = db_skill(w, "骥足百般", KS_T_CROWN, KS_RANK_A, KS_WHEN_ANY, 0, 0, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "[支援]建卡时从[技艺/兵器/魔术]记录3种类型;随时发动:将[骑乘]技能替换为同等级的任一非[常驻]从者保有技能(须为记录类型;产生对应魔耗与回转);替换的技能视为保有技能,回合结束时与[骑乘]交换回去(演示占位)。");

    /* 圣骑士帝A:常驻,[主力位]己方非支援从者+10%胜率与抗性;敌方[魔性]单位-30% */
    r = db_skill(w, "圣骑士帝", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -1));
    db_eff(w, r, E_WIN(EF_RES_UP, 10, -1));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 0));
    db_set_text(w, r, "[主力位]建卡时可无视面向购入[王途踏破];给予己方非[支援位]所有[从者]单位+10%胜率补正与[抗性上升:+10%];敌方战斗位存在持[魔性]的单位时给予其-30%胜率惩罚;持[神性]单位对自身发起负面判定时,仅在那次判定给予自身[抗性上升:+30%](演示)。");

    /* 燎原之火A:常驻/随时,诅咒流转:发动时40%判定给敌[疲惫2] */
    r = db_skill(w, "燎原之火", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 3, 0);
    db_eff(w, r, E_STATUS(S_CURSE, 1, -2));
    {
        E e = E_STATUS(S_TIRED, 2, 0);
        e.chance = 40; e.chance_neg = 1;
        db_eff(w, r, e);
        E e1048 = E_STATUS(S_CONFUSE, 1, 0);   /* 出目≤最终成功率/2追加混乱1(演示固定) */
        e2.chance = 20; e2.chance_neg = 1;
        db_eff(w, r, e2);
    }
    db_set_text(w, r, "自身持有的[诅咒]触发时不失去层数;每轮次开始时与战斗开始时各给予自身[诅咒1];发动时消耗任意层数[诅咒],对敌方战斗位全体进行[40+20*消耗层数]%负面判定(演示40%):成功给予[疲惫2](已持[疲惫]改[抗性下降:-20%]);出目≤最终成功率/2时追加[混乱1],≤/10时追加[恐惧]。");

    /* ---- 赤幕扩充包 · 宝具 ---- */

    /* 誓之所祈乃三重高墙A:行动阶段,[储备3/3]结界:魔力/幸运/宝具+25与回避 */
    r = db_np(w, "誓之所祈乃三重高墙", KS_NP_ARMORY, FC_DEFENSE, KS_RANK_A,
              KS_WHEN_ACT, 70, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_MAG, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_LUK, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_NP, 25, -2));
    db_eff(w, r, E_WIN(EF_EVADE, 0, -2));
    db_set_text(w, r, "[储备3/3]消耗行动阶段解放,赋予[魔术结界:崇高余晖][结阵1/3](储备-1):该灵脉人流量不因宝具解放减少;战斗开始时给予结界设立者[魔力][幸运][宝具]+25属性补正与[回避:技能](主力位时己方全体非仆役受用;每1人流量使补正-5);结界内每1人流量给灵脉+10魔力量;设立者离开灵脉时结界摧毁(演示)。");

    /* 展示王勇，遍历巡世的十二辉剑A:主要工序,自身+10%胜率并按影响单位重复(演示) */
    r = db_np(w, "展示王勇，遍历巡世的十二辉剑", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_PROC, 60, 1, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 0));
    db_set_text(w, r, "仅自身未解放过更高等级宝具的战斗中解放:给予自身+10%胜率补正;选择双方战斗位本场受自身技能宝具影响的单位,给予其+10%胜率(敌方改等量惩罚),每选一名此宝具失去1回转;按己方单位数量可重复(总值至多100%);EX时+20%*影响单位数量胜率(演示)。");

    /* 高歌凯旋之虹弓A:战斗开始时,[主力位]蓄力后敌非支援-40%胜率(非主力减半) */
    r = db_np(w, "高歌凯旋之虹弓", KS_NP_ARMORY, FC_BUFF, KS_RANK_A,
              KS_WHEN_BATTLE_START, 80, 9, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 0));
    E e1049 = E_STATUS(S_CHARGE, 1, -2); e1049.flag = EF_CHARGE; db_eff(w, r, e1049);
    E e1050 = E_STATUS(S_CHARGE, 2, -2); e1050.flag = EF_CHARGE; db_eff(w, r, e1050); /* EF_CHARGE 蓄力 */
    db_set_text(w, r, "[主力位]战斗开始解放[蓄力];主要工序开始时给予敌非[支援位]全体-40%胜率惩罚(非主力减半;敌主力等级>50时额外[-等级/2%]);解放此宝具的战斗胜利时算作额外一次[战斗胜利](无战斗来源);EX时胜利后至下轮结束自身全属性+20常驻(自身主力位时己方非支援单位半值,演示)。");

    /* 为狮子圆环奏响十字A:常驻,召唤持职阶技能的召唤物(演示按40级) */
    r = db_np(w, "为狮子圆环奏响十字", KS_NP_ARMORY, FC_SUMMON, KS_RANK_A,
              KS_WHEN_PASSIVE, 10, 0, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 40; e.cond_arg = 60; e.cond_arg2 = TR_HUMAN;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "为狮子圆环奏响十字:召唤(等级40 职阶技能召唤物)";
        db_eff(w, r, e);
    }
    db_set_text(w, r, "获得等级40、全属性0、持有任一职阶全部A/B/C级职阶技能的[召唤物](执行仆役规则,判定上视为[从者],等级魔耗翻倍);与宝具持有者御主存在[圣杯契约],[轰击]/[即死]退场时可消耗令咒免除;降临灵脉每10魔力量使召唤物全属性+5常驻;每个职阶技能仅可获取一次(演示)。");

    /* 吞噬吾心吧，月光A:战斗开始时,夜回合限定:同灵脉低等级单位[恐惧]与全属性-10 */
    r = db_np(w, "吞噬吾心吧，月光", KS_NP_ARMORY, FC_STATUS, KS_RANK_A,
              KS_WHEN_BATTLE_START, 70, 24, KS_F_MAIN);
    {
        E e1051 = E_STATUS(S_FEAR, 1, 0); e1051.cond = KC_NIGHT; e1051.chance = 100; db_eff(w, r, e1051);
        e = E_ATTR(EF_ATTR_DOWN, A_STR, 10, 0); e.cond = KC_NIGHT; db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_DOWN, A_END, 10, 0); e.cond = KC_NIGHT; db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_DOWN, A_AGI, 10, 0); e.cond = KC_NIGHT; db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_DOWN, A_MAG, 10, 0); e.cond = KC_NIGHT; db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_DOWN, A_LUK, 10, 0); e.cond = KC_NIGHT; db_eff(w, r, e);
    }
    db_set_text(w, r, "[主力位]仅当前灵脉处于[夜]回合时解放,本回合内始终:给予同灵脉全部等级低于自身的单位[恐惧];持来源于自身的[恐惧]的单位全属性-10属性惩罚,且其等级低于此宝具的非[常驻]宝具[封印];按受罚单位数量给予自身全属性+5/体;进行过[魂食]的回合解放立即获12回转(演示夜条件)。");

    /* 告密罗织经A:随时,[支援]罗织记录转诅咒;决胜时按中毒层数惩罚(演示) */
    r = db_np(w, "告密罗织经", KS_NP_HUMAN, FC_STATUS, KS_RANK_A,
              KS_WHEN_ANY, 40, 12, KS_F_ASSIST);
    db_eff(w, r, E_STATUS(S_CURSE, 3, 1));
    db_set_text(w, r, "[支援]仅建卡获取;仅宣言[袭击]或被袭击时可发动;方式1:夜回合行动阶段指定持[情报调查]/[资料分析]信息的单位记录[罗织3](不展露信息)并获6回转;方式2:战斗中消耗[罗织]给予等量[诅咒](演示);宣言后可将双方战斗位一半[诅咒]转给指定目标并化[中毒],决胜时敌主力[-(中毒层数*10)%]胜率(至多-120%)。");

    /* 至高之神啊，请垂怜于我B:战斗开始时,[支援]蓄力期间每工序敌三属性-20与抗性下降 */
    r = db_np(w, "至高之神啊，请垂怜于我", KS_NP_ARMORY, FC_STATUS, KS_RANK_B,
              KS_WHEN_BATTLE_START, 60, 9, KS_F_ASSIST);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 20, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 20, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 20, 1));
    db_eff(w, r, E_WIN(EF_RES_DOWN, 10, 1));
    db_eff(w, r, E_STATUS(S_CURSE, 2, -2));
    db_set_text(w, r, "[支援]解放时[蓄力];蓄力期间每工序开始时支付20魔力,给予同灵脉除自身外全部单位[筋力][耐久][敏捷]任一项-20属性惩罚与[抗性下降:-10%](演示三项);战斗结束时给予自身[诅咒2]。");

    /* 幻想铁处女C:初始工序,对双方[女性]单位25%判定并转移魔力(演示) */
    r = db_np(w, "幻想铁处女", KS_NP_HUMAN, FC_SUPPLY, KS_RANK_C,
              KS_WHEN_PROC, 20, 6, 0);
    {
        E e = E_MANA(EF_MANA_UP, 60, -2);
        e.chance = 25; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "对双方战斗位全部[女性]单位进行25%负面判定(自身[筋力]高于目标或目标持[魅惑/恐惧/混乱]时+50%):成功时从目标魔力池转移60魔力到自身魔力池;魔力超过上限时消耗至上限并按每10魔力给予敌主力-5%胜率惩罚;敌主力为[女性]且不持[抗性上升/无效]时可将自身胜率惩罚转移为同值胜率补正(演示)。");

    /* 为时已晚的号角A:常驻,战败时额外卡面从者降临(演示占位) */
    r = db_np(w, "为时已晚的号角", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_A,
              KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时额外持有仅限[Saber]职阶、等级50的从者卡面(真名/建卡资源独立,除职阶技能外不与原卡面选取相同技能宝具);当自身在决胜检定中战败时(自身不持FP则立即退场),额外卡面从者降临至御主所处灵脉(独立魔力池,不保有任何契约;降临于战败灵脉时原御主不会被俘虏,演示占位)。");

    /* 为时已晚的号角(毁)-:常驻,机动限制 */
    r = db_np(w, "为时已晚的号角(毁)-", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_NEG,
              KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "若自身持有[骑乘],自身执行的[机动]低于其他[机动]结算;若不持有[骑乘],自身无法执行[机动];建卡时不消耗RP但占用宝具栏位(演示占位)。");

    /* ================================================================
     * 全量录入 批次10:《空想从者资源库》物语扩充包
     * ================================================================ */

    /* 腹语术A:常驻,[状态免疫:技能封印];允许技能无视时机暗宣言蓄力 */
    r = db_skill(w, "腹语术", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_STATUS(S_STATE_IM, S_SKILL_SEAL, -2));
    db_set_text(w, r, "给予自身[状态免疫:技能封印];允许令自身技能无视[发动时机]暗宣言发动并赋予[蓄力](下次序生效并优先结算);B级起蓄力经过2个完整工序时技能额外生效一次;A级起对宝具同样生效(演示)。");

    /* 阴谋制作A:行动阶段,赋予灵脉[阴谋]:敌方[抗性下降](演示) */
    r = db_skill(w, "阴谋制作", KS_T_CLASS, KS_RANK_A, KS_WHEN_ACT, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_RES_DOWN, 10, 0));
    db_set_text(w, r, "建卡时破除[阵地制作]并+1RP;赋予当前灵脉[阴谋][结阵1](失去3回转;可支付10魔力立即获3回转):在生效或被摧毁前不暴露存在;本灵脉战斗时给予所有非自阵营单位[抗性下降:-5%*结阵数](演示10);[阴谋]同时视为[阵地]。");

    /* 阵地制作(工作室)A:行动阶段,[工作室]结界:礼装制作+10% */
    r = db_skill(w, "阵地制作(工作室)", KS_T_CLASS, KS_RANK_A, KS_WHEN_ACT, 20, 3, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 10, -2));
    db_set_text(w, r, "仅持有[阵地制作]的单位允许购入;建卡时破除[阵地制作]并+1RP;发动时赋予[工作室][结阵1](可付20魔力获3回转):自身[礼装制作]判定+10%*结阵数成功率;成功制作[基础礼装]时再以5%*结阵数判定,成功则强化该礼装(效果翻倍或判定+25%);工作室在自身离开或灵脉战斗时摧毁(演示)。");

    /* 享乐主义A:常驻,混沌限定;+25%胜率;每次魂食永久+5% */
    r = db_skill(w, "享乐主义", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 25, -2));
    db_set_text(w, r, "仅建卡获取且阵营必须为[混沌];给予自身[抗性破除:魅惑&混乱];始终+25%胜率补正[每次[魂食]永久+5%(成功魂食+10%,恶性+10%,无限制+20%),总量至多+100%];EX时每持1层不来源自阵营的[魅惑]+5%胜率(演示基础值)。");

    /* 淑女服饰之爱A:常驻,建卡获4件基础礼装;每回合一次强化礼装翻倍 */
    r = db_skill(w, "淑女服饰之爱", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时获取至多4个[基础礼装];每回合一次支付20魔力,强化自身任一[基础礼装]使其效果翻倍(含成功率判定时改+25%成功率,无法叠加);EX时改获取[限定礼装][辅助礼装]各1(演示占位)。");

    /* 邪智的魅力A:常驻,己方战斗位[恶]阵营单位+15%胜率 */
    r = db_skill(w, "邪智的魅力", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 15, -1));
    db_set_text(w, r, "发生[干涉]时暗指定两名非自阵营单位(不暴露效果);始终给予己方战斗位除自身外全部[恶]阵营单位+15%胜率补正;若本回合暗指定单位对另一单位宣言过袭击,此补正对己方全体生效;补正总值≤90%时主力位自身等级视为最高受补正单位等级+10;己方持更高级[气质/魅力]技能时本场无效(演示)。");

    /* 与此同时A:常驻,按记录职阶情报获得强化(演示取Saber/体术师/执法者) */
    r = db_skill(w, "与此同时", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 10, -2));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 5, 0));
    db_set_text(w, r, "记录5项非自身持有的职阶/主职业;对记录职阶从者成功[真名猜测]或持有记录主职业御主完整情报时,按对应职阶获得效果(Saber三属性+10/Lancer敏捷+10可冲锋/Assassin负面判定+10%/魔术师可礼装制作/执法者敌全体-5%胜率等,演示Saber与执法者);每项效果仅获取一次,首次获取后此技能魔耗+10。");

    /* 打开的是梦想之门A:战斗开始时,[主力位]三属性+20与+15%胜率 */
    r = db_skill(w, "打开的是梦想之门", KS_T_TALENT, KS_RANK_A, KS_WHEN_BATTLE_START, 0, 6, KS_F_MAIN);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 20, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 15, -2));
    db_set_text(w, r, "[主力位]发动时给予自身[状态抵抗:恐惧],[筋力][耐久][敏捷]+20属性补正,以及[+自身等级/4%]胜率补正(等级低于敌方主力时翻倍,至多初始等级/2%,演示15);本场战斗工序内自身无法撤退;持[合上的是现实之帷幕]时变为常驻;EX时可将属性补正总值转化为[幸运]并按幸运/2获胜率。");

    /* 骰子的选择C:随时,随机切换自身阵营并获效果(演示混沌善分支) */
    r = db_skill(w, "骰子的选择", KS_T_TALENT, KS_RANK_C, KS_WHEN_ANY, 20, 9, 0);
    db_eff(w, r, E_WIN(EF_RES_UP, 15, -2));
    db_eff(w, r, E_WIN(EF_HIT_UP, 10, -2));
    db_set_text(w, r, "仅建卡获取且阵营必须为[混沌];发动时随机改变阵营为六种之一并获取对应效果([守序善]袭击恶阵营+40%胜率/[中立善]辅助支援位加入时己方+25%/[混沌善]判定+10%最终成功与状态抵抗(演示)/[守序恶]目标抗性下降/[中立恶]指定灵脉属性+40/[混沌恶]负面判定+10%),EX时二次相同阵营再获C级效果。");

    /* 荒事舞A:常驻,三属性+15属性补正;判定上视为三属性合计不小于270 */
    r = db_skill(w, "荒事舞", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 15, -2));
    db_set_text(w, r, "战斗中给予自身[筋力][耐久][敏捷]+15属性补正;此补正生效时判定上自身始终视为三属性合计值不小于270的单位;自处主力位时,每有一项优势的[筋力/耐久/敏捷]战斗属性给予敌主力-10%胜率(非己方主要属性时翻倍);敌方主力为[魔性]时持续给其[敌方主要属性]-30惩罚(演示)。");

    /* 数学性思考A:常驻,允许将等级不高于自身的技能宝具结算时机延后 */
    r = db_skill(w, "数学性思考", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "自身发动技能或解放宝具时,若其等级不高于此技能,允许将其结算时机延后至该技能结算时机之后的本次结算链内任意时机(演示占位)。");

    /* 进化的伊丽莎白A:常驻,特性[龙种];回合结束40%判定给自身[诅咒2] */
    r = db_skill(w, "进化的伊丽莎白", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    {
        E e = E_STATUS(S_CURSE, 2, -2);
        e.chance = 40;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "给予自身[特性赋予:龙种](不因无效化失去);回合结束时进行[40+龙种同灵脉数*30]%判定,成功给予自身[诅咒1](至多6层,演示2);此诅咒无法被移除/转移;诅咒达上限时将[不完全·鲜血行星]替换为[向着完整的血之行星](演示)。");

    /* 合上的是现实之帷幕E:常驻,Saber/Lancer/Archer/Rider限定,初始属性替代 */
    r = db_skill(w, "合上的是现实之帷幕", KS_T_BLESS, KS_RANK_E, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "仅[Saber][Lancer][Archer][Rider]允许持有;建卡时不保有可分配属性,将自身除[幸运][宝具]外全部初始属性变为[100-初始等级];自身战败后至下轮结束,自身除[幸运][宝具]外全部属性至多5(持[打开的是梦想之门]时其效果同时无效化)(演示占位)。");

    /* 白雪公主B:常驻,抗性破除:中毒&魅惑;获得[中毒]时给[回避:技能] */
    r = db_skill(w, "白雪公主", KS_T_BLESS, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_STATUS(S_STATE_RES, S_POISON, -2));
    db_eff(w, r, E_WIN(EF_EVADE, 0, -2));
    db_set_text(w, r, "给予自身[抗性破除:中毒&魅惑];自身获得[中毒]时给予自身[回避:技能](每轮次至多3次;中毒归零时清除回避);持回避时不会因[中毒]退场且回避不因回合结束失去;判定上视为[魔术]技能(演示)。");

    /* 魔弹射手A:随时,[支援][储备7/7]记录效果任选(演示胜率/抗性两项) */
    r = db_skill(w, "魔弹射手", KS_T_BLESS, KS_RANK_A, KS_WHEN_ANY, 0, 3, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 1));
    db_eff(w, r, E_WIN(EF_RES_DOWN, 25, 1));
    db_set_text(w, r, "[支援][储备7/7]建卡时记录5项效果(各属性-50/胜率-40%/抗性下降-25%等,演示最后两项),每次发动消耗储备1并指定同灵脉任一单位造成一项记录效果(每项每回合一次);储备归零时对效果对象发起100%[即死]判定后恢复全部储备;EX时记录全部效果并回转1(演示)。");

    /* 勇者大原则A:常驻,按顺序获得多项效果(演示必中/无敌贯通与底限胜率) */
    r = db_skill(w, "勇者大原则", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_PIERCE, 0, -2));
    db_eff(w, r, E_WIN(EF_INV_PIERCE, 0, -2));
    db_eff(w, r, E_WIN(EF_FLOOR_UP, 15, -2));
    db_set_text(w, r, "建卡时自身阵营不能为[恶],按顺序获取7项效果:1)自身所有宝具视为[必中][无敌贯通](演示),撤退至少耗1令咒;2)三属性+15但禁用[扼守][试探];3)礼装次数+5限他人礼装;4)战斗胜利+40魔供/未参战轮-20魔力;5)[宿敌]标记机制;6)首次[即死]默认失败;7)同阵营单位退场获加符;条件满足时[底限胜率:+15%](演示)。");

    /* 计算尺武器A:随时,以同模板获取[兵器]技能(演示占位) */
    r = db_skill(w, "计算尺武器", KS_T_WEAPON, KS_RANK_A, KS_WHEN_ANY, 20, 6, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "仅建卡获取:发动时将任一非[常驻]的[兵器]技能以与此技能相同的模板赋予自身(回合结束或[封印]时失去;可以此技能时机发动并正常产生魔耗与回转);加符时允许以低等级模板发动或回转变2(演示占位)。");

    /* 无辜的世界C:常驻,建卡自选五项之一(演示[瘟疫]分支) */
    r = db_skill(w, "无辜的世界", KS_T_CROWN, KS_RANK_C, KS_WHEN_PASSIVE, 20, 0, 0);
    {
        E e = E_STATUS(S_POISON, 2, 0);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    }
    db_set_text(w, r, "仅建卡获取且限定Saber/Lancer/Archer/Rider;从五项效果选一:[征服]成为灵脉主获魔供与等级[瘟疫]免疫中毒并记录灵脉放毒(演示)/[战争]指定方胜率+20/[饥荒]魔力池上限200并按魔力差获胜率/[死亡]人流量减少换胜率/[天启]逐项获取;EX时获效果6(演示瘟疫)。");

    /* 鞍马奇才A:常驻,受其外属性补正时给予其他属性补正或胜率(演示) */
    r = db_skill(w, "鞍马奇才", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 15, -2));
    db_set_text(w, r, "战斗中自身受到不来源于此技能的属性补正时,给予自身除[宝具]外任一非补正属性+20属性补正(同时多属性受补正时改+20%胜率,演示胜率);一次效果只触发一次;触发达3次后数值降至5;EX时获得补正时额外+5%胜率(演示)。");

    /* 游侠骑士大冒险A:常驻,轮次开始指定灵脉机动;[幻想]层换胜率 */
    r = db_skill(w, "游侠骑士大冒险", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    db_set_text(w, r, "每轮次开始知悉非游荡灵脉上的从者并必须指定其一进行[机动](慢于魂食结算且不消耗行动阶段);到达时灵脉有异阵营单位给[幻想1](无则[现实1]);目标灵脉战斗胜利给[幻想3](失败给[现实3]);每持[幻想1]+5%胜率(演示);幻想高于现实时每现实1给抗性+5%(至多20%);战斗中仅能对主力/辅助位宣言冲锋。");

    /* 统领星球之物B:战斗开始时,指定己方单位给予[灼伤5]与+20%胜率 */
    r = db_skill(w, "统领星球之物", KS_T_CROWN, KS_RANK_B, KS_WHEN_BATTLE_START, 0, 9, 0);
    db_eff(w, r, E_STATUS(S_BURN, 5, 1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, 1));
    E e1052 = E_WIN(EF_WIN_DOWN, 10, -2); e1052.flag = EF_TICK_PROC; e1052.status = S_BURN; e1052.chance = 100; db_eff(w, r, e1052);
    db_set_text(w, r, "战斗开始时指定己方战斗位任一单位,给予其[灼伤5]与+20%胜率补正;该目标每个战斗工序开始时立即结算一次[灼伤](结算成功再+10%胜率);若指定单位为自身,自身任意属性因灼伤惩罚归0时立即结算[残废](演示)。");

    /* 绯红勇者传说A:随时,[主力位]指定回转型宝具换随机效果(演示筋力+80) */
    r = db_skill(w, "绯红勇者传说", KS_T_CROWN, KS_RANK_A, KS_WHEN_ANY, 20, 12, KS_F_MAIN);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 80, -2));
    db_set_text(w, r, "[主力位]仅在自身持有未处于回转的非常驻宝具时可发动:指定其一立即失去等同解放的回转(不能指定回转0),随后从五项效果中随机生效:1)[筋力]+80常驻(回合结束失去,演示);2)己方全体[无敌]至工序结束;3)清除己方40属性惩罚;4)己方全体+20%胜率(自身翻倍);5)自身判定+30%;EX时指定宝具回转减半(至少1)。");

    /* 堕天之魔A:常驻,特性[魔性];三属性+20常驻;敌方[人型]单位+5%胜率(演示) */
    r = db_skill(w, "堕天之魔", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 20, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 20, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_set_text(w, r, "始终给予自身[特性赋予:魔性]与[筋力][耐久][敏捷]+20常驻补正;与仅持[人型]单位同灵脉时须立即袭击(袭击战斗中+20%胜率);敌方战斗位每存在一名[人型]单位自身+5%胜率(仅[人型]者再+5%,演示10);进行[恶性/无限制魂食]的回合补正翻倍;2天未进行则全部补正无效化。");

    /* 人偶神乐A:随时,[辅助位]给予敌主力-30%胜率惩罚 */
    r = db_skill(w, "人偶神乐", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 1, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 1));
    db_set_text(w, r, "[辅助位]给予敌方主力位-30%胜率惩罚;可宣言支付20魔力令惩罚翻倍并与己方主力位交换战斗位(失去1回转);己方战斗位存在[人偶]单位时本场获得[反击]:人偶成为效果对象时可与其交换战斗位并替代承受(演示)。");

    /* 玻璃灰姑娘C:常驻,未被真名猜测前敌方视为初次遭遇;夜回合防真名展露 */
    r = db_skill(w, "玻璃灰姑娘", KS_T_MAGIC, KS_RANK_C, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "若自身未曾被敌战斗位单位进行[真名]正确的[真名猜测],敌战斗位不持足量[对魔力]的单位视为初次与自身敌对战斗(御主持有时改为未被成功[情报调查]);仅[夜]回合:自身即将[向同灵脉展露真名]时防止之,随后给予自身[气息遮蔽C]并通告全局(夜回合结束时失去,演示占位)。");

    /* 星之笼A:主要工序,己方[幸运]+20与敌方判定惩罚(演示) */
    r = db_skill(w, "星之笼", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PROC, 20, 9, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_LUK, 20, -1));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 15, 0));
    E e1053 = E_STATUS(S_CHARGE, 1, -2); e1053.flag = EF_CHARGE; db_eff(w, r, e1053);
    db_set_text(w, r, "仅建卡获取;方式1[支援]:己方战斗位全体[幸运]+20属性补正,受补正者的判定+5%最终成功率/受到负面判定-5%,按影响单位数给敌主力10*单位数魔力供给,此技能获6回转;方式2:敌全体+20魔力并令其判定-20%(演示15)/负面判定+20%;方式3[主力位]蓄力后依次产生效果1与2(演示)。");

    /* 一夜羽织B:行动阶段,仅夜回合;赋予[羽织作坊]结界可无行动礼装制作 */
    r = db_skill(w, "一夜羽织", KS_T_MAGIC, KS_RANK_B, KS_WHEN_ACT, 0, 12, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "仅[夜]回合可发动并生效:赋予当前灵脉[魔术结界:羽织作坊],设立者可进行[基础礼装]制作且不消耗行动(基础成功率=自身[魔力]基础属性%);结界内设立者无法[补魔],每次[礼装制作]消耗20*已制作次数魔力;结界在设立者离开灵脉时摧毁,摧毁时此技能至少保有6回转(演示占位)。");

    /* 感染A:随时,[支援]60%[恐惧]判定并转移魔力(演示) */
    r = db_skill(w, "感染", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 3, KS_F_ASSIST);
    {
        E e = E_STATUS(S_FEAR, 1, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_MANA(EF_MANA_UP, 20, -2));
    }
    db_set_text(w, r, "[支援]指定当前灵脉除自身外任一非[构装体]单位进行60%[恐惧]判定(同灵脉每名持[异常/弱化状态]者+10%):成功给予[恐惧],出目≤最终成功率/2时从目标魔力池转移20魔力给自身(个位数≤/10再转移一次);自身机动/干涉/介入时原灵脉1人流量转移至目标灵脉(演示)。");

    /* 同行随从A:常驻,额外从者卡面同行(演示占位) */
    r = db_skill(w, "同行随从", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 20, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时获取等级等同自身的额外从者卡面(分配属性点数一半,除[幸运]外上限40;共用主卡面属性点,不持RP;轮次/战斗结束产生等级魔耗),给予其任一自身不持有的≤A级保有技能;与自身同灵脉同行、共用魔力池、独立行动但判定成功率减半,撤退无需FP(自身退场时其退场);卡面退场后此技能破弃(演示占位)。");

    /* ---- 物语扩充包 · 宝具 ---- */

    /* 不完全·鲜血行星B:随时,按诅咒层数+5%胜率(演示) */
    r = db_np(w, "不完全·鲜血行星", KS_NP_WORLD, FC_DECISIVE, KS_RANK_B,
              KS_WHEN_ANY, 20, 6, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    E e1054 = E_STATUS(S_CHARGE, 1, -2); e1054.flag = EF_CHARGE; db_eff(w, r, e1054);
    db_set_text(w, r, "仅建卡获取;方式1:宣言任一自身未持有的[魔女]字段宝具以E级获取(不触发建卡效果,回合结束失去);方式2[主力位]:主要工序额外支付80魔力[蓄力],最终工序按战斗位每1层[诅咒]给自身+5%胜率(每单位至多50%,演示20),随后清除双方战斗位2层诅咒;以此解放时战斗结束回转永久+3。");

    /* 向着完整的血之行星A:主要工序,[主力位]蓄力后敌全体[诅咒5]并+胜率 */
    r = db_np(w, "向着完整的血之行星", KS_NP_WORLD, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_PROC, 100, 9, KS_F_MAIN);
    db_eff(w, r, E_STATUS(S_CURSE, 5, 0));
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    E e1055 = E_STATUS(S_CHARGE, 1, -2); e1055.flag = EF_CHARGE; db_eff(w, r, e1055);
    db_set_text(w, r, "[主力位]获取时无视面向以E级获取2件[魔女]字段宝具;主要工序解放[蓄力];最终工序给予敌战斗位全体[诅咒5],并给予自身[+战斗位诅咒层数*5%]胜率(每单位至多50%,演示20;敌主力持[龙种]时额外+30%);战斗结束诅咒降至1层;追加令咒[轰击](敌主力龙种+20%)成功时按目标诅咒层数*5%给胜率惩罚(至多30%)。仅经[进化的伊丽莎白]获取。");

    /* 祈祷之弓C:常驻/随时,杉毒联动与毒发;蓄力后按杉毒层数惩罚(演示) */
    r = db_np(w, "祈祷之弓", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_C,
              KS_WHEN_PROC, 40, 9, 0);
    {
        E e = E_STATUS(S_POISON, 3, 1);
        e.chance = 100;
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_DOWN, 25, 1));
    E e1056 = E_STATUS(S_CHARGE, 1, -2); e1056.flag = EF_CHARGE; db_eff(w, r, e1056);
    }
    db_set_text(w, r, "自身赋予任意目标[中毒]时额外给予等量[杉毒](每次至多3层);方式1[支援]:指定敌方任一单位,若其持来源于自身的[中毒]则令其[毒发];方式2[主力位]:战斗开始[蓄力],主要工序移除敌主力至多6层[杉毒],每1层给予-10%胜率(至多-60%,演示25)并移除杉毒,随后展露自身真名(演示)。");

    /* 吾之枪自当献予心爱的公主C:随时,[主力位]敌主力-30%胜率(巨大时耐久-30) */
    r = db_np(w, "吾之枪自当献予心爱的公主", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_C,
              KS_WHEN_ANY, 40, 3, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 30, 1));
    db_eff(w, r, E_STATUS(S_TIRED, 2, -2));   /* 目标巨大时自身疲惫2(演示固定) */
    db_set_text(w, r, "[主力位]给予敌方主力位-30%胜率惩罚;目标持[巨大/超巨大]时再给予[耐久]-30并给予自身[疲惫2](目标战败则此属性惩罚转常驻);若当前工序为最终工序,宣言令惩罚翻倍后失去3回转(翻倍时战斗结束自身额外[疲惫2],演示)。");

    /* 鲜血极致魔女C:主要工序,[支援]存在自身结界时敌非支援-15%与[诅咒1] */
    r = db_np(w, "鲜血极致魔女", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_C,
              KS_WHEN_PROC, 20, 3, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 15, 0));
    db_eff(w, r, E_STATUS(S_CURSE, 1, 0));
    db_set_text(w, r, "[支援]每当持有者发动/解放赋予[魔术结界]且等级不大于此宝具的技能宝具时,此宝具获1枚加符;仅当前灵脉存在归属于自身的[魔术结界]时可解放,对敌非[支援位]全体造成-15%胜率惩罚与[诅咒1](演示)。");

    /* 鲜血魔女C:战斗开始时,[支援]结界[监狱城堡]:发动魔术技能时全场诅咒(演示) */
    r = db_np(w, "鲜血魔女", KS_NP_HUMAN, FC_DEFENSE, KS_RANK_C,
              KS_WHEN_BATTLE_START, 40, 9, KS_F_ASSIST);
    db_eff(w, r, E_STATUS(S_CURSE, 1, 0));
    db_set_text(w, r, "[支援]解放时赋予[魔术结界:监狱城堡](亦可在行动阶段解放):战斗中自身低等级宝具在判定中视为[魔术]技能;每战斗工序一次,自身发动[魔术]技能时给予同灵脉除自身外所有单位[诅咒1](演示);持[诅咒]且无足量[对魔力]的单位发动技能/解放宝具时额外消耗10魔力并-5%胜率(离开灵脉解除)。");

    /* 天衣无缝·鹤恩惜别歌A:常驻,破弃自身换取[仙女羽衣]给御主(演示占位) */
    r = db_np(w, "天衣无缝·鹤恩惜别歌", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "允许随时破弃至多7个各不相同的礼装进行宣言,破弃此宝具并将[天衣无缝·仙女羽衣]给予当前灵脉任一御主单位;EX时破弃时自身立即退场,仙女羽衣+10%底限胜率与[幸运]+20(演示占位)。");

    /* 天衣无缝·仙女羽衣-:常驻,每破弃礼装[幸运]+10与抗性+5% */
    r = db_np(w, "天衣无缝·仙女羽衣", KS_NP_HUMAN, FC_BUFF, KS_RANK_NEG,
              KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 10, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 5, -2));
    db_set_text(w, r, "经[天衣无缝·鹤恩惜别歌]破弃的每个礼装,给予自身[幸运]+10常驻补正与[抗性上升:+5%];破弃的礼装受[淑女服饰之爱]强化时额外[幸运]+5(非[基础礼装]再+5,演示)。");

    /* 童话荆棘雪魔女C:常驻,给予[童话]标记:己方+15%胜率/敌方-15%胜率 */
    r = db_np(w, "童话荆棘雪魔女", KS_NP_HUMAN, FC_BUFF, KS_RANK_C,
              KS_WHEN_PASSIVE, 20, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 15, -1));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 15, 0));
    db_set_text(w, r, "战斗位存在持[魔性/龙种/构装体/巨大]的非仆役单位时给予其[童话]标记并获加符;持[童话]标记的单位处于持有者己方战斗位时+15%胜率,对立战斗位时-15%胜率(演示);加符无原本效果,达C++++时可破弃全部加符给予任一单位[特性赋予](演示)。");

    /* 为你撰写的故事C:行动阶段,给予御主[主人公]标记并强化(演示) */
    r = db_np(w, "为你撰写的故事", KS_NP_HUMAN, FC_BUFF, KS_RANK_C,
              KS_WHEN_ACT, 20, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 5, 1));
    db_set_text(w, r, "消耗行动阶段指定同灵脉一名[御主]给予[主人公1](同一目标至多4层);首获标记时记录其3项属性,每层[主人公]+5等级与记录属性+5常驻(演示筋力);2层时给予[特性赋予:神性/魔性/龙种/魔兽];4层时使其从资源库获取任一≤C级技能视为[保有技能];对新目标使用时移除先前目标标记(演示)。");

    /* 永久机关·少女帝国C:常驻,固有结界召唤与无名之森(演示召唤) */
    r = db_np(w, "永久机关·少女帝国", KS_NP_BOUND, FC_SUMMON, KS_RANK_C,
              KS_WHEN_PASSIVE, 30, 0, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 40; e.cond_arg = 120; e.cond_arg2 = TR_HUMAN;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "永久机关·少女帝国:召唤(等级40 全属性20)";
        db_eff(w, r, e);
    }
    db_set_text(w, r, "自身所处灵脉始终存在[固有结界:永久机关·少女帝国](不替代灵脉,他处固有结界存在时无效化):袭击发生时支付等级魔耗可召唤等级40、全属性20的召唤物(1~2体,战斗位时敌主力-等级/2%胜率);可随时支付80魔力赋予[固有结界:无名之森](其内部其余单位发动技能时按[阵地制作]等级30%+判定,成功令其发动失败并返还魔耗与回转,演示召唤)。");

    /* 终焉的三首怪兽，再临A:随时,召唤[超巨大]召唤物;其在场时90%判定惩罚敌方 */
    r = db_np(w, "终焉的三首怪兽，再临", KS_NP_WORLD, FC_SUMMON, KS_RANK_A,
              KS_WHEN_ANY, 70, 12, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 40; e.cond_arg = 120; e.cond_arg2 = TR_BEAST;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "终焉的三首怪兽:召唤(等级=自身演示40 总属性120 超巨大)";
        db_eff(w, r, e);
        e = E_WIN(EF_WIN_DOWN, 15, 0);
        e.chance = 90; e.chance_neg = 1;
        db_eff(w, r, e);
    E e1057 = E_WIN(EF_WIN_DOWN, 15, 0); e1057.flag = EF_TICK_PROC; e1057.status = S_POISON; e1057.chance = 90; db_eff(w, r, e1057);
    E e1058 = E_STATUS(S_BURN, 1, 0); e1058.flag = EF_RANDOM_GIVE; db_eff(w, r, e1058);
    }
    db_set_text(w, r, "仅建卡获取:召唤等级=自身、总属性120、[超巨大][独特]的召唤物(需3仆役位参战;建卡决定特性赋予);其处于战斗位时每工序开始对敌全体进行90%负面判定(演示),成功给予-15%胜率并随机产生[判定-10%/灼伤1/诅咒2]之一;EX时判定成功三效果全部生效。");

    /* 雀返C:随时,60%判定给目标属性-10与[技能封印1] */
    r = db_np(w, "雀返", KS_NP_HUMAN, FC_STATUS, KS_RANK_C,
              KS_WHEN_ANY, 20, 3, 0);
    {
        E e = E_ATTR(EF_ATTR_DOWN, A_STR, 10, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_STATUS(S_SKILL_SEAL, 1, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "仅建卡获取且无法在战斗开始时解放;指定敌非[支援位]任一单位60%负面判定,成功给予其任一属性-10与[技能封印1];解放时己方最终胜率为0可宣言[爆发](改为至多6次40%判定,每次成功属性-10,3次成功即结束并结算原本效果)(演示)。");

    /* 仿药·无用的冥府悲叹B:随时,目标异常-3层并移除常驻惩罚;首次即死默认失败 */
    r = db_np(w, "仿药·无用的冥府悲叹", KS_NP_HUMAN, FC_STATUS, KS_RANK_B,
              KS_WHEN_ANY, 50, 9, 0);
    db_eff(w, r, E_WIN(EF_STATUS_REMOVE, S_POISON, 1));
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, 1));
    db_set_text(w, r, "仅建卡获取;解放时指定同灵脉任一单位:其所有具有层数的异常状态下降3层(演示中毒),并移除全部属性受到的常驻惩罚;之后其本回合内受到的首次[即死]判定默认失败(演示效果免疫)。");

    /* 鲜血龙卷魔女B:初始工序,[主力位]4次60%判定给底限穿透与灼伤,随后爆燃 */
    r = db_np(w, "鲜血龙卷魔女", KS_NP_HUMAN, FC_STATUS, KS_RANK_B,
              KS_WHEN_PROC, 20, 3, KS_F_MAIN);
    {
        E e = E_WIN(EF_FLOOR_PEN, 5, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_STATUS(S_BURN, 1, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_BURN_BLOW, 0, 1));
    }
    db_set_text(w, r, "[主力位]对敌方非[支援位]任一单位发起4次60%负面判定,每次成功给予[底限穿透:-5%]与[灼伤1](演示);全部判定完成后令目标[爆燃]。");

    /* 钢铁天空魔女B:常驻,[主力位]记录技能获储备,额外消耗储备给胜率惩罚(演示) */
    r = db_np(w, "钢铁天空魔女", KS_NP_HUMAN, FC_SUPPLY, KS_RANK_B,
              KS_WHEN_PASSIVE, 10, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 15, 1));
    db_set_text(w, r, "[主力位]建卡时记录自身任一[兵器/魔术]保有技能并赋予[储备0/6]与[储备4](回合结束+1);自身发动持[储备]的技能或礼装时,宣言额外消耗记录技能至多4层储备,对目标额外造成[-15*消耗层数%]胜率惩罚(目标处己方战斗位时改等量补正;多名目标时仅其中一名生效;演示15);EX时每工序一次可免去储备消耗并产生10魔力消耗。");

    /* 献给某人的故事C:随时,仅在自身固有结界中解放:移除惩罚并重新开始工序(演示占位) */
    r = db_np(w, "献给某人的故事", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_C,
              KS_WHEN_ANY, 40, 9, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "仅当前灵脉存在来源于自身的[固有结界]时允许解放:移除自身与自身召唤物本工序内受到的任何胜率/属性惩罚并产生等同总值的魔力消耗;本工序有召唤物退场时按其等级产生魔耗后以工序开始状态重新召唤,随后重新开始本工序(不改变魔力/回转/令咒;演示占位)。");

    /* 直面此凄惨而温柔之现实C:最终工序,[辅助位]按等级差给予敌主力胜率惩罚(演示) */
    r = db_np(w, "直面此凄惨而温柔之现实", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_C,
              KS_WHEN_PROC, 20, 12, KS_F_SUPPORT);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
    db_set_text(w, r, "[辅助位]持有者始终具有对应[宝具]属性(仆役单位时改其所有者);降临时将此宝具给予自阵营御主或来源于自身的从者;解放时给予敌方[主力位][自身与目标等级差%]胜率惩罚(演示20);本场剩余时间内自身与目标等级差视为0;EX时决胜检定给予己方主力[100-己方最终胜率%]胜率补正。");

    /* 无貌之王(五月之王)B:常驻,[气息遮蔽]等级+1(无则获取D级);轮次开始自选效果(演示) */
    r = db_np(w, "无貌之王(五月之王)", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_B,
              KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_EVADE, 0, -2));
    db_set_text(w, r, "仅建卡获取;若自身持有[气息遮蔽]则其等级+1(至多A),否则以D级模板获取;每轮次一次支付20魔力自选一项(至轮次结束):1)无足量[对魔力]者对自身[资料分析][真名猜测]默认失败;2)自身在参战战斗中始终视为[袭击方]并在战斗开始获得[回避:技能](演示);3)袭击战斗中与敌方主力视为初次同场(被成功资料分析/真名猜测时对目标无效)。");

    /* 十王判决·葛笼纪行A:初始工序,[主力位][必中]按达成条件给惩罚并蓄力后属性惩罚 */
    r = db_np(w, "十王判决·葛笼纪行", KS_NP_HUMAN, FC_ANTITRAIT, KS_RANK_A,
              KS_WHEN_PROC, 60, 3, KS_F_MAIN | KS_F_PIERCE);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
    db_eff(w, r, E_WIN(EF_RES_DOWN, 10, 1));
    {
        E e = E_ATTR(EF_ATTR_DOWN, A_END, 30, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[主力位][必中]指定敌方战斗位任一单位使其无法在初始工序撤退;目标每达成一项条件([混沌]阵营/[恶]阵营/进行过[魂食]/进行过[恶性或无限制魂食])给予其-5%胜率惩罚与[抗性下降:-10%](演示20/10);全部达成时额外[抗性破除:负面效果];[蓄力]后最终工序80%判定(无条件达成者减半),成功给予任一属性[-(30+20*达成条件)]惩罚(演示30)。");

    /* ================================================================
     * 全量录入 批次11:《空想从者资源库》辉拓扩充包
     * ================================================================ */

    /* 专心一意A:常驻,连续相同行动时补魔+40/判定+40%(演示) */
    r = db_skill(w, "专心一意", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 40, -2));
    db_eff(w, r, E_WIN(EF_HIT_UP, 40, -2));
    db_set_text(w, r, "自身连续进行相同行动时按行动类型获得效果:1)[补魔]:给予自身40魔力供给(演示);2)需判定行动:判定+40%成功率;3)[机动]/[干涉]:该行动优先结算;4)[工房建设]:100%判定成功后额外给予任一构件[建设1];EX时判定行动改双判取优、机动恒为最高优先(演示)。");

    /* 大量生产A:随时,召唤等级20/全属性10的召唤物(至多5体演示1体) */
    r = db_skill(w, "大量生产", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 20, 3, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 20; e.cond_arg = 60; e.cond_arg2 = TR_GOLEM;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "大量生产:召唤(等级20 全属性10)";
        db_eff(w, r, e);
    }
    db_set_text(w, r, "发动时产生1名等级20、除[宝具]外全属性10的[召唤物](演示1体,至多5体);此召唤物允许进行[基础礼装]的[礼装制作](判定成功率补正减半)。");

    /* 蒸汽装置输出提升B:主要工序,消耗蒸汽强化属性或宝具惩罚(演示属性) */
    r = db_skill(w, "蒸汽装置输出提升", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_PROC, 20, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 50, -2));
    db_set_text(w, r, "方式1:消耗至多5层[蒸汽],自身[筋力/耐久/敏捷]任一项+10*消耗层数属性补正(该项为0时数值提升一半,演示50);方式2:消耗至多5层蒸汽,自身解放宝具下次造成效果时额外[-5*消耗层数%]胜率惩罚(仅一名目标时翻倍);仅能通过[机械铠甲]效果获取(演示)。");

    /* 概念改良A:随时,指定礼装:对敌-5%胜率/对己+5%胜率(演示) */
    r = db_skill(w, "概念改良", KS_T_BLESS, KS_RANK_A, KS_WHEN_ANY, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 5, 1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -1));
    db_set_text(w, r, "发动时指定自身持有的任一礼装赋予效果:1)其效果对敌方单位造成影响时额外-5%胜率(不可叠加);2)其效果对己方战斗位单位造成影响时额外+5%胜率(不可叠加);一回合至多发动4次;B级起可消耗3次发动次数给予同灵脉单位[兵器]技能[改良]标记(判定+20%,演示)。");

    /* 特斯拉线圈A:常驻,自身感电≥3时转移给敌主力-20%胜率与[感电3] */
    r = db_skill(w, "特斯拉线圈", KS_T_WEAPON, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_STATUS(S_ELECTRIC, 3, 1));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
    db_set_text(w, r, "自身持有的[感电]层数达到至少3层时,若敌方[主力位]具[感电]且层数小于自身,移除自身3层[感电]并给予敌方[主力位]-20%胜率惩罚和[感电3](符合条件即持续触发,演示);EX时每工序开始转移同灵脉其他全部单位的[感电1]给自身。");

    /* 机械铠甲A:常驻,[筋力][耐久]+50常驻与[抗性上升:+10%];[敏捷]-40常驻 */
    r = db_skill(w, "机械铠甲", KS_T_WEAPON, KS_RANK_A, KS_WHEN_PASSIVE, 20, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 50, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 50, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN_CONST, A_AGI, 40, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 10, -2));
    db_set_text(w, r, "建卡时将[过载]改为同等级[蒸汽装置输出提升];始终给予[筋力][耐久]+50常驻补正(演示)、[敏捷]-40常驻惩罚与[抗性上升:+10%];每工序限一次消耗至多2层[蒸汽]对敌主力[-(5*层数)%]胜率惩罚;EX时不会因[敏捷]为0获得[残废];仅能经[绚烂的灰烬世界]获取。");

    /* 百章之星A:常驻,[章节]成长:单位发动技能宝具+5%胜率(演示) */
    r = db_skill(w, "百章之星", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    db_set_text(w, r, "建卡时给予[章节1];在自身已知的战斗中,单位发动技能/解放宝具时记录[章节1](自身参战时额外+1);章节达1时参战战斗中单位发动技能宝具给自身+5%胜率(每单位一次,演示);达33时全属性+10常驻,67时抗性+30%,100时判定+30%。");

    /* 星之开拓者EX:常驻,特定角色自写资源(文本占位) */
    r = db_skill(w, "星之开拓者EX", KS_T_CROWN, KS_RANK_EX, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "此技能为特定角色才可能持有的自写资源,在确认选角通过的情况下,将以角色事迹的画风为核心标准进行设计(非熟练GM不建议启用,演示占位)。");

    /* ---- 辉拓扩充包 · 宝具 ---- */

    /* 吾将远征，鹦鹉螺的大冲角A:常驻,[主力位][骑乘]仅能对敌主力冲锋;冲锋失败时-40%胜率 */
    r = db_np(w, "吾将远征，鹦鹉螺的大冲角", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PASSIVE, 30, 0, KS_F_MAIN | KS_F_RIDE);
    db_eff(w, r, E_STATUS(S_STATE_RES, S_FEAR, -2));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 1));
    db_set_text(w, r, "[主力位][骑乘]给予自身[状态抵抗:恐惧];自身仅能对敌方[主力位]进行[冲锋];冲锋失败时给予敌主力[40+目标等级/2%]胜率惩罚(演示40),冲锋成功且目标等级更高时[-25%](主要工序每项劣势战斗属性再+25%);冲锋目标三属性合计≥270或持[巨大]时本场自身[无敌贯通]。");

    /* 人类神话·雷电降临A:随时,感电转移/移除;决胜前按感电层数+胜率并[激荡](演示) */
    r = db_np(w, "人类神话·雷电降临", KS_NP_CASTLE, FC_STATUS, KS_RANK_A,
              KS_WHEN_ANY, 80, 12, 0);
    db_eff(w, r, E_STATUS(S_ELECTRIC, 3, 1));
    db_eff(w, r, E_WIN(EF_ELECTRIC_BLOW, 0, 0));
    E e1059 = E_STATUS(S_CHARGE, 2, -2); e1059.flag = EF_CHARGE; db_eff(w, r, e1059);
    db_set_text(w, r, "方式1:转移战斗位任意单位的[感电]给自身(至多转移4层)并获11回转;方式2:移除自身4层[感电]后给予敌任一单位[感电3](演示)并获11回转;方式3:额外支付80魔力[蓄力];决胜检定前每存在2层[感电]自身+5%胜率,随后对战斗位全部单位造成[激荡]。");

    /* 绚烂的灰烬世界A:常驻,蒸汽机制:魔耗可用蒸汽削减,战斗开始强化机械铠甲(演示) */
    r = db_np(w, "绚烂的灰烬世界", KS_NP_ARMORY, FC_SPECIAL, KS_RANK_A,
              KS_WHEN_PASSIVE, 30, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, -2));
    db_set_text(w, r, "仅建卡获取;降临时给予自身[蒸汽8](轮次结束+3,至多20);自身产生魔力消耗时消耗1层[蒸汽]削减10魔耗;回合开始可宣言消耗行动阶段换取[蒸汽3];战斗开始宣言消耗至多4层蒸汽令[机械铠甲]常驻补正+5*层数;解除时[机械铠甲][蒸汽装置输出提升]同时无效化;EX时加符+蒸汽/上限40(演示)。");

    /* 万能之人C:技能/宝具发动时,[支援][反击]宣言异常给予目标[抗性破除](演示) */
    r = db_np(w, "万能之人", KS_NP_HUMAN, FC_ANTITRAIT, KS_RANK_C,
              KS_WHEN_ANY, 20, 6, KS_F_ASSIST | KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_RES_DOWN, 15, 1));
    db_set_text(w, r, "[支援][反击]任一技能/宝具发动时,指定其一目标并宣言任一[弱化/异常状态],给予目标该类型[抗性破除](持续至战斗结束;仅能同时存在一项;仅战斗内生效;目标持更高等级[防御]宝具时无效);EX时常驻化并可对多目标生效,给予对应状态后额外+1层(演示)。");

    /* ================================================================
     * 全量录入 批次12:《空想从者资源库》少女扩充包
     * ================================================================ */

    /* 妖精骑士A:常驻,对人负面判定+20%;负面判定成功时+5%胜率 */
    r = db_skill(w, "妖精骑士", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_FINAL_UP, 20, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    db_set_text(w, r, "自身技能和宝具对[隐藏属性:人]或仅持[人型]的单位发起负面判定时+20%最终成功率;自身发起的负面判定成功时+5%胜率补正(一次技能发动/宝具解放内仅获取一次,演示)。");

    /* 祭神的巫女A:常驻,特性[神性];给予己方[混沌][善]阵营单位+30%胜率 */
    r = db_skill(w, "祭神的巫女", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 30, -1));
    db_set_text(w, r, "始终给予自身[特性赋予:神性](不因无效化失去);自身处于战斗位时,始终给予己方战斗位[混沌][善]阵营单位+30%胜率补正(同时满足两种阵营只获得一次),战斗结束时受效单位+10魔力供给(演示)。");

    /* 钢之信仰A:常驻,状态抵抗魅惑/恐惧;战斗位时[抗性上升:+20%] */
    r = db_skill(w, "钢之信仰", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_STATUS(S_STATE_RES, S_FEAR, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CHARM, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 20, -2));
    db_set_text(w, r, "给予自身[状态抵抗:魅惑&恐惧],受到失败的[魅惑&恐惧]判定时+5%胜率;处于战斗位时再获[抗性上升:+20%](主力位时翻倍);C级起改为[状态免疫:魅惑&恐惧];EX时不再给抗性,改为自身受所有负面判定基础成功率减半(至多-60%)与[耐久]+30常驻(演示)。");

    /* 梦魔之畔A:随时,80%[混乱]判定并转移20魔力(演示) */
    r = db_skill(w, "梦魔之畔", KS_T_TALENT, KS_RANK_A, KS_WHEN_ANY, 20, 6, 0);
    {
        E e = E_STATUS(S_CONFUSE, 1, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_MANA(EF_MANA_UP, 20, -2));
    }
    db_set_text(w, r, "建卡时使自身保有[魔性];指定同灵脉任一单位进行80%[混乱]判定(演示),成功给予[混乱1]并从其魔力池转移20魔力给自身;判定上同时视为[魔术]技能且具有[魔眼]字段(演示)。");

    /* 千里眼(千里眼·现世视)C:随时,展露技能监视任一灵脉(演示占位) */
    r = db_skill(w, "千里眼(千里眼·现世视)", KS_T_TALENT, KS_RANK_C, KS_WHEN_ANY, 20, 0, 0);
    db_eff(w, r, E_WIN(EF_INFO, 1, -2));
    db_set_text(w, r, "指定任一灵脉,将此技能展露给该灵脉所有单位并暴露自身存在,自当下起获悉该灵脉上发生的一切直至回合结束(同一时间只能对一个灵脉生效);EX时仅展露给等级>60或持[神性A]的从者,并允许对灵脉单位发动[魔眼]/[魔术]保有技能(演示)。");

    /* 秀丽的容貌C:常驻,未知悉此技能者视为[异性];魅惑联动(演示) */
    r = db_skill(w, "秀丽的容貌", KS_T_TALENT, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_STATUS(S_CHARM, 1, 0));
    db_set_text(w, r, "此技能生效时不暴露效果;未知悉此技能信息的单位,在自身技能宝具判定中始终视为[异性];持来源于自身[魅惑]的单位即使知悉此技能同样受此影响;EX时回合开始/干涉时给予同灵脉所有[异性]单位[魅惑1](每单位每回合一次,演示)。");

    /* 受肉精灵A:常驻,所处灵脉获得额外魔力池(演示+40) */
    r = db_skill(w, "受肉精灵", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 40, -2));
    db_set_text(w, r, "所处灵脉获得[灵脉魔力量*2]的额外魔力池(上限至多40,初始满值,轮次结束恢复等同灵脉魔力量),允许持有者将其中的魔力转移给自身;已转移的魔力池不因离开灵脉重置;其他持有此技能者无法获取其中魔力;EX时上限60(演示40)。");

    /* 盛开于夏夜之花D:常驻,每名持自身魅惑的单位+5%胜率;参战时己方80%魅惑判定 */
    r = db_skill(w, "盛开于夏夜之花", KS_T_TALENT, KS_RANK_D, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    {
        E e = E_STATUS(S_CHARM, 1, -1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "战斗位上每存在一名持来源于自身的[魅惑]的单位,自身+5%胜率(演示);自身参与战斗时对己方战斗位除自身外全部单位进行80%[魅惑]判定(成功给[魅惑1];非主力位时成功率减半);判定上视为[领袖气质];己方持更高等级[气质/魅力]技能时本场判定默认失败。");

    /* 自我暗示A:常驻,状态抵抗魅惑/恐惧/混乱;回合开始宣言性别获属性或判定强化 */
    r = db_skill(w, "自我暗示", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 5, 0, 0);
    db_eff(w, r, E_STATUS(S_STATE_RES, S_FEAR, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CHARM, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CONFUSE, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 25, -2));
    db_set_text(w, r, "给予自身[状态抵抗:恐惧&魅惑&混乱];回合开始时指定[男性/女性]使自身在低等级技能宝具判定中视为对应性别([男性]:任两项属性+25(演示筋力耐久)/[女性]:负面判定+25%);A级起可改变自身[性别];EX时+10%抗性并可在任意等级判定中变更性别与特性。");

    /* 米可诺斯☆米可科尔C:随时,指定单位[弱化/异常状态]提升或减少1层(演示) */
    r = db_skill(w, "米可诺斯☆米可科尔", KS_T_BLESS, KS_RANK_C, KS_WHEN_ANY, 20, 9, 0);
    db_eff(w, r, E_WIN(EF_STATUS_REMOVE, S_POISON, 1));
    db_set_text(w, r, "指定同灵脉任一单位,将其持有的所有[弱化/异常状态]提升1层或减少1层(演示减少中毒;无层数状态不受影响;判定上视为同一次赋予);EX时可额外支付一次发动条件使效果额外生效一次。");

    /* 予其手以光B:随时,[支援]己方主力位等级更低时给予其全属性+10常驻 */
    r = db_skill(w, "予其手以光", KS_T_BLESS, KS_RANK_B, KS_WHEN_ANY, 20, 9, KS_F_ASSIST);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, -1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 10, -1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 10, -1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 10, -1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 10, -1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -1));
    db_set_text(w, r, "[支援]仅建卡获取;指定不为自身的己方[主力位]单位,目标等级低于敌方主力位时给予其全属性+10常驻补正(非御主单位仅能获得一次,多次获取后数值减半);主要工序开始,持此补正的单位处主力位时,己方每有一项劣势战斗属性+10%胜率(目标为御主时翻倍,演示)。");

    /* 愉快型魔术礼装A:常驻/随时,记录技能赋予自身并可按时机强化(演示属性+10) */
    r = db_skill(w, "愉快型魔术礼装", KS_T_WEAPON, KS_RANK_A, KS_WHEN_ANY, 20, 6, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 10, -2));
    db_set_text(w, r, "建卡时以对应等级记录自身不持有面向的任一从者保有技能(仅A级以下);支付20魔力发动:将记录技能赋予自身(正常产生回转与魔耗;常驻技能立即支付两倍魔耗;效果无效化时失去;不具建卡效果);持记录期间可支付20魔力:1)初始工序任意属性+10(同属性至多+20);2)随时移除最多10属性惩罚(每场3次);3)[反击]低等级技能宝具对自身效果降1级。判定上视为[魔术](演示)。");

    /* 英灵猎手(死神)A:常驻,特性[神性];敌主力为从者时+10%胜率并获其真名 */
    r = db_skill(w, "英灵猎手(死神)", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_set_text(w, r, "始终给予自身[特性赋予:神性]并知悉同灵脉从者的等级;敌方[主力位]为从者时始终+10%胜率补正(与更高等级从者战斗胜出再+等级差%,每名从者仅获取一次,至多叠4次);非自阵营从者每次发动技能宝具时,以[目标等级-自身等级]%判定,成功获取其真名(A级起同时获取核心人设文本,演示)。");

    /* 手杖魔术A:随时,散射全体-20%胜率/炮射单体-25%(演示两种) */
    r = db_skill(w, "手杖魔术", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 1, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 0));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 25, 1));
    E e1060 = E_STATUS(S_CHARGE, 1, -2); e1060.flag = EF_CHARGE; db_eff(w, r, e1060);
    db_set_text(w, r, "方式1[散射]:敌非[支援位]全体-20%胜率(等级判定始终低于此技能1级;同一战斗每发动过依次再-5%);方式2[炮射]:敌非[支援位]任一单位-25%胜率(此技能等级判定与[魔力]属性相同,魔力低于目标时无效);方式3[斩击]:以魔力-敏捷%判定成功-30%(每次-30%成功率惩罚);方式4[狙射]:初始工序蓄力,对目标按魔力-敏捷%判定给-35%(演示1与2)。");

    /* 米可诺斯之锤B:随时,记录本工序惩罚并反射给敌主力(演示40%) */
    r = db_skill(w, "米可诺斯之锤", KS_T_MAGIC, KS_RANK_B, KS_WHEN_ANY, 20, 6, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 1));
    db_set_text(w, r, "战斗工序结束时记录本工序受到胜率/属性惩罚的数值(每工序结束前移除后重新记录);发动时给予敌方[主力位][-记录数值%]胜率惩罚并清除记录(至多-75%,演示40)。");

    /* 巫邪灵媒A:随时,给予自身[诅咒2];兵器技能/宝具生效时敌-10%胜率(魔性-20%) */
    r = db_skill(w, "巫邪灵媒", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 6, 0);
    db_eff(w, r, E_STATUS(S_CURSE, 2, -2));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 0));
    db_set_text(w, r, "给予自身[诅咒2];本场战斗中自身发动[兵器]保有技能或解放任意宝具时,若敌非[支援位]任意单位受到其效果影响,给予那个单位-10%胜率惩罚(该单位持[魔性]时数值+10,演示)。");

    /* 天体轨道调整B:行动阶段,赋予结界:昼夜调整与灵脉魔量翻倍(演示占位) */
    r = db_skill(w, "天体轨道调整", KS_T_MAGIC, KS_RANK_B, KS_WHEN_ACT, 20, 12, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "消耗行动阶段赋予当前灵脉[魔术结界:天体调整](可随时支付20魔力变更所有[昼夜规律:正常]的灵脉的昼夜规律,全局通报,不处同一灵脉也可生效);使此灵脉魔力量翻倍(至多+30);结界与持有者不处同灵脉时依然存在;同一来源仅能存在一个(演示占位)。");

    /* ---- 少女扩充包 · 宝具 ---- */

    /* 天遡鉾A:随时,60%[即死]判定(魔性/从者目标+20%) */
    r = db_np(w, "天遡鉾", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_A,
              KS_WHEN_ANY, 60, 9, 0);
    db_eff(w, r, E_DEATH(60, 1, 1));
    db_set_text(w, r, "指定敌非[支援位]任一单位,60%[即死]判定(目标持[魔性]或为[从者]时各+20%;幸运≥40减半,演示);仅在[对魔力]判定中同时视为[魔术]技能;EX时不受非EX等级[对魔力]影响。");

    /* 百合花散剑之舞踏C:随时,异性目标[魅惑1]联动属性惩罚与胜率惩罚(演示) */
    r = db_np(w, "百合花散剑之舞踏", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_C,
              KS_WHEN_ANY, 20, 3, 0);
    {
        E e = E_STATUS(S_CHARM, 1, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_WIN(EF_WIN_DOWN, 10, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 10, 1));
    }
    db_set_text(w, r, "指定敌非[支援位]一名单位,目标为[异性]时给予其[魅惑1];魅惑成功给予后,以[80+10*目标魅惑层数]%判定,成功给予其[筋力][耐久][敏捷]-10属性惩罚(演示80%档与胜率惩罚);这之后给予[-(10*其来源于自身魅惑层数)%]胜率惩罚;所有效果视为[魅惑]效果。");

    /* 百合花开豪华绚烂C:初始工序,敌方[异性]全体[魅惑1]并三属性-15(演示) */
    r = db_np(w, "百合花开豪华绚烂", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_C,
              KS_WHEN_PROC, 40, 6, 0);
    db_eff(w, r, E_STATUS(S_CHARM, 1, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 15, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 15, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 15, 0));
    db_set_text(w, r, "给予敌方战斗位全部[异性]单位[魅惑1],随后给予受效单位[筋力][耐久][敏捷]-15属性惩罚(目标不持来源于自身的[魅惑]时失去);再以[20*魅惑层数-目标三属性之和]%判定,成功打断目标本工序所有未结算的发动/解放;所有效果视为[魅惑](演示);允许与[百合花散剑之舞踏]共用栏位。");

    /* 于彼招手的理想乡A:战斗开始时,[支援]己方战术不被克制,全体+10%胜率与抗性 */
    r = db_np(w, "于彼招手的理想乡", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_BATTLE_START, 60, 12, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -1));
    db_eff(w, r, E_WIN(EF_RES_UP, 10, -1));
    db_eff(w, r, E_WIN(EF_EVADE, 0, -1));
    db_set_text(w, r, "[支援]仅建卡获取;本场战斗令己方除[试探]外的[战术]不会被克制;主要工序时:1)己方主力+等级差%胜率(等级更高减半,御主翻倍);2)己方全体+10%胜率(每优势战斗属性对己方主力追加一次,演示);3)己方全体[抗性上升:+10%]且判定+10%;4)给予己方主力[回避:技能](演示);自身处主力位时改对己方其他单位生效。");

    /* 祝贺跃祭A:战斗开始时,[主力位]蓄力期间每工序70%判定给[诅咒2][中毒2]与-10%胜率 */
    r = db_np(w, "祝贺跃祭", KS_NP_WORLD, FC_STATUS, KS_RANK_A,
              KS_WHEN_BATTLE_START, 100, 12, KS_F_MAIN);
    {
        E e = E_STATUS(S_CURSE, 2, 0);
        e.chance = 70; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_STATUS(S_POISON, 2, 0);
        e.chance = 70; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 0));
    E e1061 = E_STATUS(S_CHARGE, 2, -2); e1061.flag = EF_CHARGE; db_eff(w, r, e1061);
    E e1062 = E_WIN(EF_WIN_DOWN, 10, 0); e1062.flag = EF_TICK_PROC; e1062.status = S_CURSE; e1062.chance = 70; db_eff(w, r, e1062);
    }
    db_set_text(w, r, "[主力位]战斗开始解放[蓄力];每工序开始,若此宝具存在[蓄力],同灵脉除自身外所有单位进行70%负面判定(演示),成功给予[诅咒2][中毒2]并予以-10%胜率惩罚(对单名单位判定成功率>100%时惩罚+10%);此宝具发起的判定触发[诅咒]效果时不减少其层数;蓄力在战斗结束时移除。");

    /* 极星天球壳循环B:战斗开始时,[主力位]固有结界:首耗免去,昼免下次魔耗/夜技能宝具+1回转 */
    r = db_np(w, "极星天球壳循环", KS_NP_ARMORY, FC_SPECIAL, KS_RANK_B,
              KS_WHEN_BATTLE_START, 80, 12, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_RECAST, 1, -2));
    db_set_text(w, r, "[主力位]解放赋予[固有结界:宇宙蛋](宽7/初始工序无法撤退);自此结界存在起持有者首次魔力消耗免去;每工序开始按灵脉[昼][夜]:昼免去自身下一次魔力消耗,夜使自身所有技能宝具获得1回转(回转1者无效;演示回转)。");

    /* 圣剑遥远梦之遗痕A:随时,消耗[拟似圣剑]获取记录宝具或提升等级(演示占位) */
    r = db_np(w, "圣剑遥远梦之遗痕", KS_NP_ARMORY, FC_SPECIAL, KS_RANK_A,
              KS_WHEN_ANY, 20, 3, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    E e1063 = E_STATUS(S_CHARGE, 1, -2); e1063.flag = EF_CHARGE; db_eff(w, r, e1063);
    db_set_text(w, r, "仅建卡获取;建卡时从[誓约胜利之剑][轮转胜利之剑][无毁的湖光][灿然辉耀的王剑]记录4项;解放时消耗[拟似圣剑]:1)消耗2件并付20魔力将记录宝具以C级模板获取(视为额外宝具,仅一件,不能轰击,不回合结束失去);2)已持记录宝具时消耗2件令其等级+1(至多B);行动阶段可将此宝具转移给非仆役单位;加符时主要工序蓄力,最终工序按破弃数量给敌-10*数量%胜率(演示占位)。");

    /* 孵化希望夏之水镜A:随时,结界制作[拟似圣剑]并轮次结束免费制作(演示占位) */
    r = db_np(w, "孵化希望夏之水镜", KS_NP_ARMORY, FC_SPECIAL, KS_RANK_A,
              KS_WHEN_ANY, 60, 6, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 5, 0));
    db_set_text(w, r, "仅建卡获取,可与[圣剑遥远梦之遗痕]共占栏位;行动阶段赋予[魔术结界:希望之湖·多兹玛利](视为特殊[魔术工房],魔池0/60,可建工房,稳态60):轮次结束获得等同灵脉魔量的魔力;可进行[礼装制作](仅能制作[拟似圣剑]);持有者处于灵脉时轮次结束免费制作判定,成功获得2件[拟似圣剑](加符+1件);加符2枚以上时发动[拟似圣剑]可付20魔力给敌非支援全体-5%胜率(演示占位)。");

    /* 拟似圣剑C:随时,[支援]消耗自身并给予敌非支援任一单位-10%胜率 */
    r = db_item(w, "拟似圣剑", KS_RANK_C, KS_WHEN_ANY, 0, 0, KS_F_ASSIST, 1, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 1));
    db_set_text(w, r, "[支援]指定敌方战斗位非[支援位]任一单位,消耗此礼装并给予其-10%胜率惩罚;发动时判定上视为C级的[对人]宝具;发动无需消耗[礼装发动次数],但每场战斗中一名单位至多发动3次(仆役单位1次,演示)。");

    /* ================================================================
     * 全量录入 批次13:《空想从者资源库》战线 + 盈月扩充包
     * ================================================================ */

    /* 怪力(魔)A:主要工序,筋力+50常驻补正(回合结束移除) */
    r = db_skill(w, "怪力(魔)", KS_T_TALENT, KS_RANK_A, KS_WHEN_PROC, 20, 6, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 50, -2));
    db_set_text(w, r, "建卡时给予自身[特性赋予:魔性]或[特性赋予:魔兽];仅[战斗属性]存在[筋力]时可发动:给予自身[筋力]+50常驻补正(回合结束时移除);EX时自身[筋力]高于敌方主力时其无法改变[随机属性]。");

    /* 魔力放出(宝石)A:行动阶段/随时,[魔弹宝石]蓄能后给敌单体-30%胜率 */
    r = db_skill(w, "魔力放出(宝石)", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 20, 1, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 1));
    db_set_text(w, r, "降临时与每轮次开始时给予自身1个[魔弹宝石](允许仅制作[魔弹宝石]的[礼装制作]);方式1:行动阶段指定任一[魔弹宝石]给予[蓄能]标记;方式2:战斗中消耗[蓄能]宝石,给予敌非[支援位]任一单位-30%胜率惩罚(演示)。");

    /* 天狗的兵法A:常驻,[军略]等级+2(无则给[军略D]);冲锋时目标-10%胜率 */
    r = db_skill(w, "天狗的兵法", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 1));
    db_set_text(w, r, "若持有[军略]则其等级+2,否则以[军略D]赋予自身;若持有[对魔力]则等级+1,否则[抗性上升:+10%];战斗中允许自身[冲锋],冲锋判定时给目标-10%胜率(判定成功翻倍);己方主力位时给敌主力-10%(战术克制时翻倍);自身技能宝具对敌方单位造成效果时-5%(每单位一次,演示)。");

    /* 殿军的心得A:常驻,+15%底限胜率;契约御主无令咒时全属性+10 */
    r = db_skill(w, "殿军的心得", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_FLOOR_UP, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_MAG, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_LUK, 10, -2));
    db_set_text(w, r, "给予自身+15%底限胜率(己方宣言[扼守]且生效时转化为等量最终胜率);与自身有[圣杯契约]的御主不持令咒时,给予自身除[宝具]外全属性+10属性补正(主力位时己方全体受用);A级时本场圣杯战争限一次,主力位战败后允许己方其他单位不消耗FP撤退(演示)。");

    /* 战士的咆哮A:常驻,战斗开始时(无疲惫且非支援位)三属性+20 */
    r = db_skill(w, "战士的咆哮", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 20, -2));
    db_set_text(w, r, "建卡时可移除初始FP并+1RP;战斗开始时,若自身不持[疲惫]且不处[支援位],给予自身[筋力][耐久][敏捷]+20属性补正;己方非[支援位]单位不少于2名时,同时给予己方其他所有单位三属性+5;己方持更低等级[战士的咆哮]者本场效果等级提升至与此技能相同(演示)。");

    /* 昏暗密林之颚A:常驻,自身发起的所有判定+15%基础成功率 */
    r = db_skill(w, "昏暗密林之颚", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 15, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    db_set_text(w, r, "自身发起的所有判定+15%基础成功率;自身发起[轰击]判定时+20%胜率补正(轰击成功翻倍);环境系统尚未正式制作,相关环境设定暂不启用(演示)。");

    /* 变转之魔(神)B:常驻,特性[魔性];除[宝具]外全属性+20常驻 */
    r = db_skill(w, "变转之魔(神)", KS_T_BLESS, KS_RANK_B, KS_WHEN_PASSIVE, 20, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 20, -2));
    db_set_text(w, r, "建卡时记录除[宝具]外任一属性;给予自身[特性赋予:魔性];始终给予除[宝具]外全属性+20常驻补正;始终令自身持有的[神性]技能效果无效化并按等级给予记录属性[+50]常驻补正(判定上视为不持[神性]技能);加符时敌方有[神性]者再给全属性+10(演示)。");

    /* 铁之傅C:常驻,自阵营御主[耐久]+20常驻与[抗性上升:+10%] */
    r = db_skill(w, "铁之傅", KS_T_BLESS, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 20, 1));
    db_eff(w, r, E_WIN(EF_RES_UP, 10, 1));
    db_set_text(w, r, "始终给予自阵营御主[耐久]+20常驻补正与[抗性上升:+10%];每回合一次,自阵营御主成为负面判定/负面效果的目标时,令自身成为那个效果的效果对象(即使不与御主同处一灵脉同样生效,演示)。");

    /* 豹之加护A:常驻,[反击]状态抵抗恐惧/混乱;属性惩罚提升(演示) */
    r = db_skill(w, "豹之加护", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 15, 0, KS_F_COUNTER);
    db_eff(w, r, E_STATUS(S_STATE_RES, S_FEAR, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CONFUSE, -2));
    db_set_text(w, r, "[反击]给予自身[状态抵抗:恐惧&混乱];己方[战术]未被克制时:自身造成属性惩罚时其数值+5(每场至多+40);自身被等级≤此技能的技能指定为目标时,60%判定成功给予[回避:技能](每场一次);EX时受来源非自阵营的成功负面判定时[抗性上升:+10%]。");

    /* 变容A:常驻,属性重分配上限提高并可重分配(演示敏捷补正) */
    r = db_skill(w, "变容", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 15, -2));
    db_set_text(w, r, "建卡时将全部属性分配上限改为110并破除自身[对魔力](不返还RP);每轮次开始允许重新进行[属性分配];职阶持[对魔力]时,持20未分配属性点则赋予[对魔力E](每再20点等级+1;职阶不持对魔力则需求提升一半);加符时分配上限+20(演示敏捷化)。");

    /* 魔力放出(槛)A:随时,额外魔池与胜率惩罚转换(演示) */
    r = db_skill(w, "魔力放出(槛)", KS_T_WEAPON, KS_RANK_A, KS_WHEN_ANY, 0, 1, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 30, -2));
    db_set_text(w, r, "方式1:行动阶段宣言,若当前灵脉不持额外魔力池,给予其[60]额外魔力池并将自身魔力转移其中(该池仅能经此技能获得魔力);方式2:战斗中消耗该池任意数值魔力,给予敌非[支援位]任一单位[-(消耗魔力)%]胜率惩罚(演示30)。");

    /* 仁王立姿A:常驻,[主力位][抗性上升:+30%]并吸引指定效果 */
    r = db_skill(w, "仁王立姿", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_RES_UP, 30, -2));
    db_set_text(w, r, "[主力位]给予自身[抗性上升:+30%];战斗中敌战斗位单位发动技能/解放宝具时,若其必须指定自身所处战斗位上的单位,将其指定单位改为自身;战斗外同阵营单位与自身同灵脉时,任何需指定目标的效果将其指定改为自身(演示)。");

    /* 晚钟C:回合开始时,指定目标:其受到[即死]判定时-5%胜率(演示) */
    r = db_skill(w, "晚钟", KS_T_CROWN, KS_RANK_C, KS_WHEN_ACT, 20, 9, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 5, 1));
    db_set_text(w, r, "发动时指定任一持[外貌描述信息]的单位,获悉其灵脉并向该灵脉全体展露此技能;本回合内目标受到[即死]判定时-5%胜率惩罚(即死成功时惩罚+5%,可多次触发但提升不叠加);同一时间仅能对一名单位生效;连续两次指定同一单位时自身对其灵脉的[机动][干涉]优先结算;EX时效果常时存在且始终知悉目标位置与行动(演示)。");

    /* 怨灵降伏A:随时,50%负面判定(魔性翻倍),成功技能封印 */
    r = db_skill(w, "怨灵降伏", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 6, 0);
    {
        E e = E_STATUS(S_SKILL_SEAL, 1, 1);
        e.chance = 50; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "指定同灵脉任一单位,50%负面判定(目标持[魔性]时成功率翻倍),成功给予目标[魔术]技能[技能封印](演示);判定出目≤最终成功率/10时将以此方式封印的技能赋予自身(额外技能,回合结束失去)。");

    /* ---- 战线扩充包 · 宝具 ---- */

    /* 炽焰，亦焚尽神明A:初始工序[对人]/主要工序[对城],[主力位]灼伤与封印或蓄力后-40%胜率 */
    r = db_np(w, "炽焰，亦焚尽神明", KS_NP_HUMAN, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_PROC, 80, 9, KS_F_MAIN);
    db_eff(w, r, E_STATUS(S_BURN, 1, 1));
    db_eff(w, r, E_STATUS(S_NP_SEAL, 1, 1));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 0));
    db_set_text(w, r, "[主力位]方式1[对人]仅初始工序解放(固定80魔力):给予同灵脉所有单位[灼伤1],指定任一受灼伤单位给予其所有宝具[宝具封印](目标不持灼伤或本工序结束时移除);方式2[对城]:蓄力后最终工序给敌全体-40%胜率(每额外蓄力一工序+10%,支援位减半);令咒追加[轰击]成功时此宝具获[无敌贯通]并额外-30%。");

    /* 已然遥远的理想之城B:随时,[反击]己方单位免去惩罚并+15%胜率(演示) */
    r = db_np(w, "已然遥远的理想之城", KS_NP_ARMORY, FC_DEFENSE, KS_RANK_B,
              KS_WHEN_ANY, 40, 12, KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, -1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 15, -1));
    db_set_text(w, r, "[反击]仅建卡获取(至多4枚加符);己方战斗位任一单位成为敌方技能/宝具效果对象时解放:己方除自身外所有单位至本场战斗结束:受到的胜率/属性惩罚免去(总值高于90时自身产生溢出魔耗);其他负面效果无效化(自身每产生20魔力消耗;无效[即死]额外40魔力并获加符;来源等级更高者再+20;无敌贯通时消耗翻倍);自身不处非[支援位]战斗位时失效;加符:++时己方其他单位+15%胜率(演示)。");

    /* 死告天使C:常驻,[主力位]自身即死判定成功率减半;每工序20%[即死]判定敌主力 */
    r = db_np(w, "死告天使", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_C,
              KS_WHEN_PASSIVE, 20, 0, KS_F_MAIN);
    db_eff(w, r, E_DEATH(20, 1, 0));
    db_set_text(w, r, "[主力位]自身受到的[即死]判定基础成功率始终减半;战斗开始及每工序开始时对敌主力发起20%[即死]判定(无法获得其他来源成功率补正;本场圣杯战争中目标每受过一次[即死]判定+5%,本场战斗中每次+20%):成功时若处[战斗开始时]目标不付令咒即退场,否则给予其除[宝具]外任一属性-30惩罚(演示)。");

    /* 山脉震撼明星之薪A:主要工序,[支援]摧毁工房并全场三属性-20 */
    r = db_np(w, "山脉震撼明星之薪", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 80, 6, KS_F_ASSIST);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 20, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 20, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 20, 0));
    db_set_text(w, r, "[支援]仅建卡获取且仅[魔力量]不为0的灵脉可解放:摧毁灵脉[阵地][工房][神殿][魔术结界],本场战斗始终给予自身外双方战斗位全部单位[筋力][耐久][敏捷]-20属性惩罚(每记录一次灵脉再-20);记录:自身灵脉有其他单位任意属性因惩罚变为0(不消耗/不回转);战斗结束每记录一次灵脉魔力量-5(演示)。");

    /* 太阳历石(豹)A:常驻/战斗开始时,神殿建设与猛兽召唤(演示召唤与胜率) */
    r = db_np(w, "太阳历石(豹)", KS_NP_ARMORY, FC_SUMMON, KS_RANK_A,
              KS_WHEN_BATTLE_START, 20, 12, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 60; e.cond_arg = 240; e.cond_arg2 = TR_BEAST;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "太阳历石(豹):召唤(等级60 总属性240 猛兽)";
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    }
    db_set_text(w, r, "必须持有[神性A/B/C]才能获取;允许自身建设[神殿]:神殿所在灵脉每1人流量给自身+5%胜率(演示);其他灵脉机动/干涉至此灵脉时转移原灵脉1人流量;战斗开始解放时召唤至多4个等级60、总属性240、[猛兽]的召唤物至己方战斗位;EX时召唤数量不再受限且等级=初始等级(判定上视为[从者],等级魔耗翻倍)。");

    /* 太阳历石(风)A:战斗开始时,敌非支援全体[灼伤3];随后全场[灼伤1] */
    r = db_np(w, "太阳历石(风)", KS_NP_ARMORY, FC_STATUS, KS_RANK_A,
              KS_WHEN_BATTLE_START, 80, 9, 0);
    db_eff(w, r, E_STATUS(S_BURN, 3, 0));
    db_eff(w, r, E_STATUS(S_BURN, 1, 0));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 50, 1));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 0));
    db_set_text(w, r, "必须持有[神性A/B/C]才能获取;方式1:战斗开始时给予敌非[支援位]全体[灼伤3],随后给予同灵脉除自身外全部单位[灼伤1](演示);方式2:初始工序解放,使自身下一个解放的[对城]宝具首效时额外给敌主力-50%胜率,随后双方战斗位除自身外全体-20%胜率;EX时每场一次可无视回转解放方式2并额外[灼伤1]。");

    /* 遮那王流离谭A:随时,5种解放方式(演示方式2与5) */
    r = db_np(w, "遮那王流离谭", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_A,
              KS_WHEN_ANY, 20, 3, 0);
    {
        E e = E_WIN(EF_WIN_DOWN, 40, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 0));
    }
    db_set_text(w, r, "建卡时给予此宝具3项解放方式:1)战斗开始[对军]转移灵脉单位并失去6回转;2)主要工序以[50+敏捷差]%判定,成功-40%胜率(演示60%档);3)[反击]成为非[对城/对界]宝具目标时指定单位给予[保护];4)敌方FP+1并给予自身[回避:技能];5)最终工序[对军]-10%胜率([魔性]目标-20%并50%恐惧判定,演示);加符时每枚额外给予一项解放方式。");

    /* 灵峰踏抱冥府之鞴A:随时,[主力位]冥界:异阵营-30%胜率,同阵营+20% */
    r = db_np(w, "灵峰踏抱冥府之鞴", KS_NP_ARMORY, FC_SPECIAL, KS_RANK_A,
              KS_WHEN_ANY, 80, 12, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 0));
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -1));
    db_eff(w, r, E_STATUS(S_TIRED, 1, 0));
    db_set_text(w, r, "[主力位]仅建卡获取;降临时将游荡灵脉改变为[冥界]并全局通报;行动阶段/[交流]中解放:给予当前灵脉所有非同阵营单位-30%胜率惩罚(战斗外转等量魔力消耗),仅本回合将当前灵脉变为[冥界](宽7/魔量20/人流量0):自身始终视为灵脉主,同阵营单位+20%胜率(演示);回合结束异阵营单位[疲惫1](逐回合+1);冥界无法建工房但可建神殿。");

    /* 五百罗汉补陀落渡A:战斗开始时,蓄力后80%判定给[晕眩3][诅咒3](魔性者追加) */
    r = db_np(w, "五百罗汉补陀落渡", KS_NP_ARMORY, FC_ANTITRAIT, KS_RANK_A,
              KS_WHEN_BATTLE_START, 80, 12, 0);
    {
        E e = E_STATUS(S_STUN, 3, 0);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_STATUS(S_CURSE, 3, 0);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_DEATH(10, 0, 1);
        e.chance = 10;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "解放时[蓄力];初始工序开始时对同灵脉所有单位进行80%判定(演示),成功给予[晕眩3][诅咒3];判定成功时目标持[魔性]则追加一轮上述效果并对其10%[即死]判定(幸运≥40减半,演示)。");

    /* ---- 盈月扩充包 ---- */

    /* 魔性鬼神A:随时,特性[神性][魔性];除[幸运][宝具]外全属性+25与抗性+10% */
    r = db_skill(w, "魔性鬼神", KS_T_TALENT, KS_RANK_A, KS_WHEN_ANY, 20, 12, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_MAG, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_NP, 25, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 10, -2));
    db_set_text(w, r, "仅建卡获取;建卡时可无视职阶限制购入职阶技能[狂化];始终给予自身[特性赋予:神性][特性赋予:魔性];发动时令自身除[幸运][宝具]外全部属性+25属性补正(演示含宝具属性)与[抗性上升:+10%]。");

    /* 魔力放出(水)A:随时,[支援]己方目标属性+15与+5%胜率,或敌方-15(演示) */
    r = db_skill(w, "魔力放出(水)", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 15, 1, KS_F_ASSIST);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 15, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 15, 1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -1));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 5, 1));
    db_set_text(w, r, "[支援]发动时自选:1)己方非[支援位]任一单位[筋力/耐久/敏捷]任一项+15属性补正与+5%胜率(演示筋力);2)敌方非[支援位]任一单位任一项-15属性惩罚与-5%胜率;加符时每场首次发动以A级模板额外生效一次;EX时对己方全体半值生效或对敌全体生效(非主力惩罚减半)。");

    /* 烈士的兵法B:常驻,己方可宣言[死斗];解放[对军]宝具时+20%胜率 */
    r = db_skill(w, "烈士的兵法", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    db_set_text(w, r, "本场战斗允许己方非[支援位]全部单位宣言[死斗](以此宣言的[死斗]仅提供+5%胜率);己方[战术]未被克制时,每当己方解放[对军]宝具,给予自身+20%胜率补正(演示);EX时决胜检定每存在宣言过[死斗]的非仆役单位+10%胜率(有克制关系时再+一次,至多不超过双方最终胜率差值)。");

    /* 魔力放出(迅雷)A:随时,敌单体-15属性惩罚与[感电1] */
    r = db_skill(w, "魔力放出(迅雷)", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 15, 1, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 15, 1));
    db_eff(w, r, E_STATUS(S_ELECTRIC, 1, 1));
    db_set_text(w, r, "给予敌非[支援位]任一单位[筋力/耐久/敏捷]任一项-15属性惩罚与[感电1](演示筋力与感电);自身持[感电]时可消耗1层减少10发动魔耗;EX时额外支付40魔力改对敌非[支援位]全体生效。");

    /* 神魔屠戮A:常驻,[主力位]敌方单位首次受自身技能宝具影响-20%胜率 */
    r = db_skill(w, "神魔屠戮", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 0));
    db_eff(w, r, E_WIN(EF_RES_DOWN, 20, 1));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
    db_set_text(w, r, "[主力位]战斗中敌方战斗位每名单位首次受到自身技能宝具影响时,给予其-20%胜率惩罚;受影响单位的[神性]持有者同时受到[抗性下降:-20%](演示),持[魔性]者再受-20%胜率;自身不受敌方战斗位等级不高于此技能的[神性]技能影响;EX时持[神性]或[魔性]的目标两项效果同时生效(演示)。");

    /* 红玉之书A:常驻,记录[魔术]御主技能并以C/D/E级模板获取(演示占位) */
    r = db_skill(w, "红玉之书", KS_T_WEAPON, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "通过任意方式作为[保有技能]获取时总是触发建卡效果;建卡时记录任一名称具有[魔术]且非[常驻]的御主技能;每回合一次宣言,将该类技能以[C/D/E]级模板获取(记录技能等级提升至与此技能相同);获取的技能视为保有技能,回合结束失去,获全部回转前无法再次获取(演示占位)。");

    /* 勇往直前·俱利伽罗岭A:战斗开始时,[主力位]固有结界:召唤与灼伤打击 */
    r = db_np(w, "勇往直前·俱利伽罗岭", KS_NP_ARMORY, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_BATTLE_START, 80, 9, KS_F_MAIN);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 40; e.cond_arg = 60; e.cond_arg2 = TR_HUMAN;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "勇往直前·俱利伽罗岭:召唤(等级40 全属性10)";
        db_eff(w, r, e);
        db_eff(w, r, E_STATUS(S_BURN, 1, 0));
    }
    db_set_text(w, r, "[主力位]赋予[固有结界:俱利伽罗岭](宽7/永夜/初始工序无法撤退);每工序开始召唤至多2个等级40、全属性10的召唤物(受敌方技能宝具影响即退场);每工序结束可宣言敌方任一单位,召唤物各以70%判定成功给[灼伤1]后退场;最终工序按召唤物数给敌[灼伤1]并令其退场;令咒判定成功时敌全体三属性-30并[爆燃](无令咒时判定成功不造成属性惩罚,演示)。");

    /* 奥义·夜樱B:初始工序,80%[混乱2]判定;随后按混乱层数偶数耐久-40(演示) */
    r = db_np(w, "奥义·夜樱", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_B,
              KS_WHEN_PROC, 20, 0, 0);
    {
        E e = E_STATUS(S_CONFUSE, 2, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_DOWN, A_END, 40, 1);
        e.chance = 40; e.chance_neg = 1;
        db_eff(w, r, e);
    db_eff(w, r, E_STATUS(S_CONFUSE, 1, 0));
    }
    db_set_text(w, r, "每场战斗仅能解放一次;指定敌方[主力位]80%负面判定,成功给予[混乱2](演示);无论成败,随后立即以[目标混乱层数*20%]判定(演示40%),成功给予[耐久]-40并移除目标[混乱];本工序结束时目标持[残废]则立即结算一次;额外生效时仅给判定+40%;加符时目标残废结算成功则敌全体[混乱1]。");

    /* 秘剑·比翼闪耀-(视为A):随时,[主力位][必中]6次敏捷%判定属性-5并+胜率 */
    r = db_np(w, "秘剑·比翼闪耀", KS_NP_HUMAN, FC_SWORD, KS_RANK_NEG,
              KS_WHEN_ANY, 10, 3, KS_F_MAIN | KS_F_PIERCE);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 5, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 5, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 5, 1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_set_text(w, r, "[主力位][必中]对目标发起6次[自身基础敏捷]%负面判定(目标敏捷≥40减半;每次失败+20%成功率补正),判定成功给予[筋力][耐久][敏捷]-5属性惩罚(演示各5);每判定成功一次给予自身+10%胜率补正(每场首次生效,演示10)。");

    /* 绝技·八岐怒涛B:随时,[主力位]敌单体8次-5%胜率(演示40) */
    r = db_np(w, "绝技·八岐怒涛", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_B,
              KS_WHEN_ANY, 20, 9, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 1));
    db_set_text(w, r, "[主力位]仅建卡获取;方式1[对人]:指定敌非[支援位]任一单位,给予其8次-5%胜率惩罚(宣言可将胜率惩罚改为[筋力/耐久/敏捷]任一项属性惩罚,演示总值40);方式2:额外支付60魔力,给予敌主力8次-5%胜率,再宣言属性给予其他敌非[支援位]单位8次-5属性惩罚。");

    /* 悲叹的圣母D:随时,敌单体-20%胜率与[抗性下降:-20%] */
    r = db_np(w, "悲叹的圣母", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_D,
              KS_WHEN_ANY, 20, 9, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
    db_eff(w, r, E_WIN(EF_RES_DOWN, 20, 1));
    db_set_text(w, r, "方式1[对人]:敌非[支援位]任一单位-20%胜率惩罚与[抗性下降:-20%](演示);方式2[对军](支付40魔力):敌非[支援位]全体-10%胜率与[抗性下降:-10%](对敌主力翻倍)。");

    /* 旭将军B:随时,[主力位]敌主力[灼伤3]并[爆燃];魔性者额外-20%胜率 */
    r = db_np(w, "旭将军", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_B,
              KS_WHEN_ANY, 50, 9, KS_F_MAIN);
    db_eff(w, r, E_STATUS(S_BURN, 3, 1));
    db_eff(w, r, E_WIN(EF_BURN_BLOW, 0, 1));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
    db_set_text(w, r, "[主力位]记录敌主力本场受到的[灼伤]层数;解放时给予其[灼伤3](目标持[魔性]额外-20%胜率,演示);若目标本场未曾[爆燃]则使其[爆燃](演示);已[爆燃]且灵脉存在[固有结界]时摧毁结界并按记录层数*5%判定(无视抗性),成功给[爆燃]相关属性等量惩罚;最终工序解放时延迟至决胜检定前。");

    /* 化身·伊吹大明神缘起A:随时,[主力位]三属性+60常驻与神魔体征(演示) */
    r = db_np(w, "化身·伊吹大明神缘起", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_ANY, 80, 12, KS_F_MAIN);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 60, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 60, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 60, -2));
    db_set_text(w, r, "[主力位]解放时给予自身[筋力][耐久][敏捷]+60常驻补正(演示);补正生效期间始终给予[特性赋予:神性][魔性][魔兽][巨大];可战斗外宣言使效果无效化;一回合经历两场战斗后无法主动无效化并必须袭击当前灵脉单位(以杀死为目标;行动为魂食时改为无限制魂食);[爆发]时不再额外回转且额外魔耗减半(补正减半);效果回合结束失去。");

    /* 赤壁战祸·地狱摇篮A:战斗开始时,[支援]结界[赤壁]:初始工序任一方全体[灼伤1] */
    r = db_np(w, "赤壁战祸·地狱摇篮", KS_NP_ARMORY, FC_STATUS, KS_RANK_A,
              KS_WHEN_BATTLE_START, 80, 9, KS_F_ASSIST);
    db_eff(w, r, E_STATUS(S_BURN, 1, 0));
    db_set_text(w, r, "[支援]解放时赋予[魔术结界:赤壁](判定上视为固有结界):双方战斗位单位无法冲锋战斗外单位;初始工序开始时给予任一方战斗位全部单位[灼伤1](演示);每工序2次可支付20魔力给予任一方非[支援位]任一单位[灼伤1](额外再付60魔力使其[爆燃]);此宝具判定上视为[结界]宝具。");

    /* 堕天·灼热异邦B:随时,双方主力[灼伤5]并立即结算(演示) */
    r = db_np(w, "堕天·灼热异邦", KS_NP_HUMAN, FC_STATUS, KS_RANK_B,
              KS_WHEN_ANY, 20, 6, 0);
    db_eff(w, r, E_STATUS(S_BURN, 5, 1));
    db_eff(w, r, E_STATUS(S_BURN, 5, -2));
    db_eff(w, r, E_WIN(EF_BURN_BLOW, 0, 1));
    db_eff(w, r, E_STATUS(S_BURN, 3, 0));
    db_eff(w, r, E_STATUS(S_BURN, 3, -1));
    db_set_text(w, r, "仅建卡获取;始终给予自身[抗性破除:灼伤];给予自身与敌方[主力位][灼伤5](自身主力位时双方非[支援位][主力位]全部单位另[灼伤3];自身辅助位时己方主力再[灼伤3]);之后使所有受[灼伤]单位立即结算一次[灼伤];[爆发]时持续结算至无灼伤(自身因此[残废]则立即结算,演示)。");

    /* 水神C:常驻,记录宝具封印并获取[魔力放出(水)]C级模板(演示) */
    r = db_np(w, "水神", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_C,
              KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 15, -2));
    db_set_text(w, r, "仅建卡获取;建卡时记录自身持有的任一其他宝具,始终给予其[封印](封印期间不暴露宝具信息);持有此宝具时将[魔力放出(水)]以C级模板赋予自身(已持有则获加符,无法加符时每回合首次发动以A级模板额外生效一次);封印存在时可随时支付20魔力无视时机回转发动一次[魔力放出(水)](不产生消耗;至本回合结束封印无效化,演示)。");

    /* ================================================================
     * 全量录入 批次14:《空想御主资源库》基础资源包(魔术师/体术师/刽子手/剑术师/武士)
     * ================================================================ */

    /* 增殖的源A:常驻,魔力池上限+60(演示) */
    r = db_skill(w, "增殖的源", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_CIRCUIT, 60, -2));
    db_set_text(w, r, "自身魔力池上限+60;允许随时摧毁一个礼装并给予自身10魔力供给;EX时战斗开始时宣言使魔力池上限无效化,随后给予[魔力]+溢出的属性补正并移除溢出(演示)。");

    /* 反击屏障A:常驻,成为低等级技能对象时其效果无效并-20魔力 */
    r = db_skill(w, "反击屏障", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, -2));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 25, 1));
    db_set_text(w, r, "此技能未生效时不展露技能信息;每回合一次,战斗中成为敌方战斗位技能的效果对象时(技能等级低于此技能),自身产生20魔力消耗并使那个技能对自身效果无效化;对触发单位造成-25%胜率惩罚(演示);EX时对任意等级技能生效。");

    /* 强力召唤A:常驻,自阵营[从者]全属性+10与魔力池上限+60(演示) */
    r = db_skill(w, "强力召唤", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 10, 1));
    db_set_text(w, r, "建卡时使自阵营[从者]获得全属性+10常驻补正与魔力池上限+60(演示属性部分);此效果不会因自阵营[从者]变动而改变效果对象。");

    /* 移动要塞A:常驻,工房特殊组件[移动要塞](演示占位) */
    r = db_skill(w, "移动要塞", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时获得工房特殊组件[移动要塞](稳态80/规模1/20,需建设4完成):可作为基础组件直接在要塞上建设工房;赋予灵脉[结阵]时可转而赋予要塞(结阵等量占用规模);工房主可在行动阶段支付20魔力令要塞对任一灵脉[干涉];受轰击时要塞与基础组件不会被摧毁(演示占位)。");

    /* 机械革命A:行动阶段,强化人偶或指定单位(演示胜率) */
    r = db_skill(w, "机械革命", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ACT, 10, 6, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_CIRCUIT, 5, -2));
    db_eff(w, r, E_STATUS(S_ELECTRIC, 1, -2));
    db_set_text(w, r, "必须当前灵脉存在自阵营[魔术工房]才能发动;指定具有[人偶]特性的单位:[战斗人偶]+10等级与全属性+5常驻;[后勤人偶]+10%行动判定成功率并强化其[协助];[魔力人偶]+10魔力池上限与回路+5;其他单位+10%胜率(演示);受效单位随后[感电1];对一名单位至多生效5次。");

    /* 义体黎明A:行动阶段,指定御主任一属性+15常驻(至多+75) */
    r = db_skill(w, "义体黎明", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ACT, 20, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 15, 1));
    db_set_text(w, r, "必须当前灵脉存在自阵营[魔术工房]才能发动;消耗行动阶段指定当前灵脉任一[御主]单位(需同意),给予目标[特性赋予:人偶&构装体]及任一不存在补正的属性+15常驻补正(该属性为0时额外+15;每单位总值至多75,演示耐久);");

    /* 终极巨像A:常驻,魔像核心与聚合机制(演示占位) */
    r = db_skill(w, "终极巨像", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时召唤等级10、全属性10、[构装体][人偶]特性的[魔像核心];战斗外宣言令核心与任意数量人偶[聚合]成占用2战斗位的[魔像人偶](除核心外单位属性减半;参与单位不多于3时允许自身替代核心主体);魔像人偶处辅助位时始终给予敌主力[-自身等级%]胜率惩罚;聚合解除时全部参与人偶[损毁];战斗结束产生10*聚合数魔力消耗(演示占位)。");

    /* 自我原型E:行动阶段,以人偶数据记录并复活(演示占位) */
    r = db_skill(w, "自我原型", KS_T_MAGIC, KS_RANK_E, KS_WHEN_ACT, 20, 24, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "发动后令当前灵脉除自身外任一人偶退场并记录灵脉;自身死亡的下回合开始时可在任一记录灵脉[复活]并获得[人偶]特性(属性与该人偶相同,魔力恢复至上限;继承仆役单位与工房所有权,不继承令咒/契约/礼装;再死不产生圣杯规模;演示占位)。");

    /* 闲人免进A:行动阶段,结界:人流量-5,机动/干涉者混乱判定 */
    r = db_skill(w, "闲人免进", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ACT, 20, 3, 0);
    {
        E e = E_STATUS(S_CONFUSE, 1, 0);
        e.chance = 100; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "赋予当前灵脉[魔术结界:闲人免进]:令灵脉人流量-5;对此灵脉机动或干涉的单位进行[140-目标魔力]%[混乱]判定,成功给予[混乱1](演示100%档);[交流]中发动需灵脉持有者允许(无持有者时自动成功);EX时交流发动无需允许并可即时判定(成功率减半)。");

    /* 罗生三相A:行动阶段,结界:令低等级技能发动失败(演示占位) */
    r = db_skill(w, "罗生三相", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ACT, 20, 6, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "消耗行动阶段赋予[魔术结界:罗生三相][结阵1]:结界设置者位于灵脉时可随时令双方战斗位等级不高于此技能的技能发动失败(不返还发动消耗,每次失去结阵1);结界至多[结阵3];EX时对任意等级技能生效(演示占位)。");

    /* 众星拱辰A:常驻,魔量最多灵脉:袭击方从者-30%胜率(演示) */
    r = db_skill(w, "众星拱辰", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 0));
    db_set_text(w, r, "自身处于[魔力量]最多的灵脉且灵脉存在结阵满的[魔术结界]时:自阵营作为[被袭击方]参战时,战斗开始给予宣言[袭击]的阵营从者-30%胜率惩罚(演示);灵脉持工房时轮次结束后向异阵营从者展示并转移其10魔力。");

    /* 阵式混合A:轮次开始时,令自身任一结界与他结界共存(演示占位) */
    r = db_skill(w, "阵式混合", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ACT, 20, 9, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "指定当前灵脉任一归属于自身的[魔术结界],给予其[阵式混合]标记:该结界将与非同名的其他[魔术结界]共存(演示占位)。");

    /* 改天换日A:回合开始时,令自身设置的结界转移至自身灵脉(演示占位) */
    r = db_skill(w, "改天换日", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ACT, 20, 3, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "令自身设置的任一[魔术结界]转移至自身所处灵脉;无法选择已成为过此技能目标的结界(演示占位)。");

    /* 护身结界C:技能发动时,获得职业礼装[护身结界]并赋予结界效果(演示占位) */
    r = db_skill(w, "护身结界", KS_T_MAGIC, KS_RANK_C, KS_WHEN_ANY, 20, 9, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "持有此技能时允许制作职业礼装[护身结界];自身赋予灵脉[魔术结界]的技能发动时发动此技能,将那个结界效果赋予自身持有的[护身结界](不持结阵,可触发消耗结阵的效果,需特定结阵层数者无法触发;演示占位)。");

    /* 巫蛊人偶A:行动阶段,职业礼装[巫蛊人偶]记录真名(演示占位) */
    r = db_skill(w, "巫蛊人偶", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ACT, 20, 6, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时获得职业礼装[巫蛊人偶];消耗行动阶段发动,令[巫蛊人偶]记录任一御主真名(再次发动替换记录);生祭:使[巫蛊人偶]发起的判定+20%成功率(演示占位)。");

    /* 枯萎之手A:随时,[交流]中给予[枯萎]:目标技能宝具魔耗翻倍(演示) */
    r = db_skill(w, "枯萎之手", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 3, 0);
    db_eff(w, r, E_MANA(EF_MANA_DOWN, 20, 1));
    db_set_text(w, r, "[交流]中才能发动,指定当前灵脉任一单位给予[枯萎]:目标本轮次接下来技能宝具产生的魔力消耗翻倍(每单位每轮次一次,额外魔耗至多100,演示20);生祭:获得[支援]并可从敌方魔力池扣除发动魔耗;EX时可在发动当下再次发动(演示)。");

    /* 法力汲取A:战斗开始时,120-目标等级%判定转移40魔力 */
    r = db_skill(w, "法力汲取", KS_T_MAGIC, KS_RANK_A, KS_WHEN_BATTLE_START, 20, 12, 0);
    {
        E e = E_MANA(EF_MANA_UP, 40, -2);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "指定敌方战斗位任一单位,以其[120-目标等级]%负面判定(演示80%),成功时从目标魔力池转移40魔力给自身;生祭:自身技能效果导致同灵脉单位产生魔力消耗时此技能获3回转(魔力转移不视为补给/消耗)。");

    /* 恶意中伤A:回合开始时,指定灵脉:干涉者产生30魔力消耗(演示) */
    r = db_skill(w, "恶意中伤", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ACT, 20, 6, 0);
    db_eff(w, r, E_MANA(EF_MANA_DOWN, 30, 0));
    db_set_text(w, r, "指定任一灵脉并向其全体展露此技能:本回合每个单位对该灵脉[干涉]时产生30魔力消耗(演示),该灵脉上的单位对其他灵脉[干涉]时同效;生祭:不展示给目标灵脉单位;EX时对[机动]同样生效。");

    /* 刻印献祭A:回合开始时,指定御主给予[诅咒6];主力从者+5%胜率 */
    r = db_skill(w, "刻印献祭", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ACT, 20, 12, 0);
    db_eff(w, r, E_STATUS(S_CURSE, 6, 1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN_CONST, A_END, 20, 1));
    db_set_text(w, r, "指定当前灵脉任一未曾被此技能指定的[御主]单位(需允许;俘虏无需允许):给予其[耐久]-20常驻惩罚与[诅咒6](演示);当前灵脉每存在1层来源于此技能的[诅咒],始终给予主力位自阵营从者+5%胜率补正(夜回合翻倍,至多+120%,演示);此技能给你的诅咒存在时自身所有技能持[生祭]。");

    /* 祟神使役A:回合开始时,全体负面判定+20%(演示) */
    r = db_skill(w, "祟神使役", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ACT, 20, 6, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 20, 0));
    db_set_text(w, r, "从三项效果选择一项生效:1)本回合所有单位受到的[负面判定]+20%成功率(演示);2)所有单位发起的[负面判定]-20%最终成功率;3)所有单位的行动判定-20%最终成功率;同名相同效果取最大值,不可叠加;生祭:回转改3;EX时自身[抗性下降:-20%]并令同灵脉单位所有判定-20%。");

    /* 分割思考A:常驻,判定双判取优(演示+10%基础成功率) */
    r = db_skill(w, "分割思考", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 10, -2));
    db_set_text(w, r, "自身或自身持有的魔术工房发起判定时,允许改为进行两次判定并选择一项作为结果(每轮次至多5次,战斗中失效);EX时使那次判定+20%基础成功率(演示)。");

    /* 自动装械A:常驻,工房辅助组件[自动装械]:每回合两次礼装制作判定(演示占位) */
    r = db_skill(w, "自动装械", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "允许建造工房辅助组件[自动装械](稳态40/规模+2):存在此组件的魔术工房每回合开始进行2次50%[礼装制作]判定,成功时获得任一[基础礼装](演示占位)。");

    /* 炼成总机A:常驻,工房限定组件:成功制作时追加制作(演示占位) */
    r = db_skill(w, "炼成总机", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "允许建造工房限定组件[炼成总机](稳态60/规模+3):同灵脉产生成功的[基础礼装]制作判定时,工房进行30%追加判定,成功额外获得相同[基础礼装](每回合至多5次,演示占位)。");

    /* 活体熔炉A:行动阶段,礼装次数+5与[回路]+5常驻(演示) */
    r = db_skill(w, "活体熔炉", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ACT, 20, 12, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_CIRCUIT, 5, -2));
    db_set_text(w, r, "仅当前灵脉存在自阵营[魔术工房]时可发动:给予自身除[宝具]外全属性-5常驻惩罚(或献祭一名御主),随后令自身每轮次[发动礼装次数上限]+5并给予[回路]+5常驻补正与等量魔力池上限(演示回路)。");

    /* 机动工造A:随时,移动工房基座(演示占位) */
    r = db_skill(w, "机动工造", KS_T_WEAPON, KS_RANK_A, KS_WHEN_ANY, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "方式1:消耗行动阶段赋予自身[机动工造](稳态40/规模1/7):可作为基础组件直接在上面建设魔术工房(不视为工房,不受轰击/覆盖灵脉影响;游荡灵脉时效果无效);方式2:当前灵脉任意单位[工房建造]时可在[机动工造]上进行(需同意)(演示占位)。");

    /* 周期推演A:行动阶段,以9件基础礼装换辅助/限定礼装(演示占位) */
    r = db_skill(w, "周期推演", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ACT, 20, 3, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "破弃自身持有的9件[基础礼装]并宣言[辅助礼装][限定礼装]各一件,随后获取宣言礼装(视为礼装制作;无法重复获取[唯一]的同一礼装,演示占位)。");

    /* 身若惊鸿A:随时,[支援]80+敏捷差%判定取消目标蓄力(演示) */
    r = db_skill(w, "身若惊鸿", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 10, 6, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "[支援]指定敌方战斗位任一单位,以[80+敏捷差]%判定(演示),成功时令其处于[蓄力]的技能宝具无效化;发动时若自身处[支援位]则进入[辅助位](无辅助位则无法在支援位发动);EX时回合开始宣言可对任一灵脉进行不消耗行动的[机动]。");

    /* 先声夺人A:战斗开始时,[支援]80+敏捷差%判定成功给[迟滞1] */
    r = db_skill(w, "先声夺人", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_BATTLE_START, 10, 6, KS_F_ASSIST);
    {
        E e = E_STATUS(S_LAG, 1, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[支援]指定敌方战斗位任一单位,以[80+敏捷差]%负面判定(演示80%),成功给予[迟滞1];目标本场未发动过技能宝具时出目固定为1。");

    /* 明镜止水A:常驻,双方[技艺]技能发动时+10魔力供给(演示) */
    r = db_skill(w, "明镜止水", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 10, -2));
    db_set_text(w, r, "建卡时自选一项:1)[主力位]双方战斗位每有一个[技艺]技能发动,+10魔力供给(低等级再+10);2)己方每有一个[技艺]技能发动,+15魔力(每场至多5次);3)己方每有[技艺]技能发动+5魔力(低等级再+5),战斗胜利时额外获得本场累计供给(演示)。");

    /* 破境还元A:战斗开始时,[主力位]获取心眼真/直感/透化并以三属性+10(演示) */
    r = db_skill(w, "破境还元", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_BATTLE_START, 0, 3, KS_F_MAIN);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 10, -2));
    db_set_text(w, r, "[主力位]从[心眼(真)][直感][透化]中选择一项以C级模板获取(视为保有技能,不回合结束失去);持全部三项时令自身[筋力][耐久][敏捷]+10常驻补正(演示);自身参与的任一战斗战败时失去由此技能获取的全部技能。");

    /* 周天行B:常驻,魔力池上限与轮次结束魔力供给(演示) */
    r = db_skill(w, "周天行", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 15, -2));
    db_set_text(w, r, "始终给予[+三属性基础属性之和/4]魔力池上限补正;轮次结束时给予等同此补正的魔力供给(持异常/弱化状态时减半,演示15);以5为单位舍去;A级模板获取时改为[天赋]且轮次结束给所处灵脉[魔力量]的魔力供给。");

    /* 六合八荒A:最终工序,按三属性合计差判定,-60%胜率(演示) */
    r = db_skill(w, "六合八荒", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PROC, 10, 9, 0);
    {
        E e = E_WIN(EF_WIN_DOWN, 60, 1);
        e.chance = 40; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "指定敌方[主力位],以[自身三属性合计-目标三属性合计]%负面判定(演示40%档),成功立即给予-60%胜率惩罚;圣杯战争限一次交流中可从者支付90魔力令此胜率惩罚翻倍。");

    /* 错骨缠龙A:主要工序,给予自身宣言属性差补正(演示50) */
    r = db_skill(w, "错骨缠龙", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PROC, 10, 6, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 50, -2));
    db_set_text(w, r, "宣言敌方战斗位任一单位和除[宝具]外任一项属性,给予自身宣言属性[+宣言属性差值]的属性补正(至多+70,演示50)。");

    /* 一指金刚A:初始工序,敌主力[耐久]惩罚至多50 */
    r = db_skill(w, "一指金刚", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PROC, 0, 12, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 50, 1));
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
    db_set_text(w, r, "给予敌方[主力位][耐久]-[目标耐久]属性惩罚(至多-50);本场战斗中目标[耐久]作为[战斗属性]时始终视为[劣势](已劣势则改-20%胜率);目标耐久为0时额外[抗性下降:-20%](演示)。");

    /* 不二法门A:常驻,以D级模板获取全部体术师/武术家职业技能(演示占位) */
    r = db_skill(w, "不二法门", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "将全部体术师主职业技能和武术家子职业技能以D级模板获取(无法获取[常驻]技能;获取的技能为额外技能,发动后失去;不占技能栏;演示占位)。");

    /* 迟到的正义A:初始工序,80%判定成功敌-40%胜率(人型翻倍) */
    r = db_skill(w, "迟到的正义", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PROC, 10, 3, 0);
    {
        E e = E_WIN(EF_WIN_DOWN, 40, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 10, -2));
    }
    db_set_text(w, r, "发动时选择一项效果(不持对应标记则成功率减半):[行刑者]对同灵脉任一单位80%负面判定(演示),成功-40%胜率(仅[人型]目标翻倍);[解脱者]对敌战斗位任一单位80%负面判定,成功-40%胜率(给予自身[耐久]-10常驻惩罚可翻倍);EX时惩罚+20%。");

    /* 信念的枷锁A:随时,[交流]中负面判定压制目标(演示) */
    r = db_skill(w, "信念的枷锁", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 10, 6, 0);
    {
        E e = E_WIN(EF_WIN_DOWN, 20, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "仅[交流]中发动且不持对应标记时成功率减半:[行刑者]对同灵脉任一单位100%负面判定,成功令其本次干涉无法宣言袭击(进入战斗位则产生20魔力消耗并-20%胜率,人型翻倍);[解脱者]100%判定成功令目标立即袭击自身且无法支援位参战(进入战斗位时-40%胜率,演示);EX时目标的自阵营单位-10%。");

    /* 义务的终结A:初始工序,70%判定成功给技能封印(演示) */
    r = db_skill(w, "义务的终结", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PROC, 10, 6, 0);
    {
        E e = E_STATUS(S_SKILL_SEAL, 1, 1);
        e.chance = 70; e.chance_neg = 1;
        db_eff(w, r, e);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 10, -2));
    }
    db_set_text(w, r, "[行刑者]对同灵脉任一单位70%负面判定(仅[人型]目标+50%),成功宣言任一非特殊等级[技艺/魔术]技能,目标持有时给予[封印1](重复封印+1层);[解脱者]70%判定(可自罚筋力/敏捷-10获得+50%)成功时以对应模板获取目标技能(临时技能,演示)。");

    /* 死亡与新生A:常驻,[行刑者]:自身技能对人型生效时胜率永久+10%(演示) */
    r = db_skill(w, "死亡与新生", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 30, -2));
    db_set_text(w, r, "按自身标记产生效果:[行刑者]给予主力位自阵营从者+0%起始胜率补正(至多+80%;每当自身技能对[人型]单位生效永久+10%,仅[人型]者翻倍);[解脱者]给予自身胜率补正(至多+100%;自身每存在-10常驻惩罚+20%;敌方仅一体时翻倍,演示30);EX时两项效果同时生效(演示按A级)。");

    /* 内心的正义A:随时,给予目标抗性上升/下降与等值胜率修正(演示) */
    r = db_skill(w, "内心的正义", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 10, 6, 0);
    db_eff(w, r, E_WIN(EF_RES_UP, 25, 1));
    db_eff(w, r, E_WIN(EF_RES_DOWN, 25, 1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 10, -2));
    db_set_text(w, r, "[行刑者]给予同灵脉任一单位[抗性上升:+25%]或[抗性下降:-25%],以及等同的胜率补正或惩罚(仅[人型]目标翻倍);[解脱者]转移目标至多4层[弱化/异常状态]给自身(每层+10%胜率,至多+50%,可自罚敏捷-10使抗性与胜率翻倍);不持标记时胜率数值减半(演示)。");

    /* 放逐C:随时,清除标记并+50魔力供给 */
    r = db_skill(w, "放逐", KS_T_TECHNIQUE, KS_RANK_C, KS_WHEN_ANY, 0, 6, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 50, -2));
    db_set_text(w, r, "自身拥有[行刑者]或[解脱者]标记时才能发动:清除自身[行刑者][解脱者]标记并重置自身技能对战斗胜负的判定,随后给予自身+50魔力供给;自身下一次参与战斗结束时按胜负重新给予标记(演示)。");

    /* 适情录C:随时,[剑势]切换并-10%胜率(演示) */
    r = db_skill(w, "适情录", KS_T_TECHNIQUE, KS_RANK_C, KS_WHEN_ANY, 10, 1, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 1));
    db_set_text(w, r, "此技能不能在最终工序内发动:给予敌方战斗位任一单位-10%胜率惩罚,清除通过此技能获取的[剑势]并从[辘轳势/重梅势/长生势]中选择一项获取(一场战斗多次发动时无法选择已持剑势;已持全部剑势时不再清除);初始工序开始时此技能立即获1回转(演示)。");

    /* 双飞燕C:战斗开始时,赋予记录剑势;敌方暗指定技能发动时-10%胜率 */
    r = db_skill(w, "双飞燕", KS_T_TECHNIQUE, KS_RANK_C, KS_WHEN_BATTLE_START, 10, 3, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 0));
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
    db_set_text(w, r, "赋予自身此技能持有记录的[剑势](发动此技能获取剑势时每有一项已持剑势+10%胜率);暗指定一项[技艺/兵器]技能:敌方战斗位每有一次暗指定技能发动,给予其-10%胜率惩罚(演示)并从三种剑势中选择获取并记录。");

    /* 双倒扑A:初始工序,按剑势给对应属性-30(演示辘轳势) */
    r = db_skill(w, "双倒扑", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PROC, 10, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 30, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 30, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 30, 1));
    db_set_text(w, r, "按自身持有的[剑势]产生效果:[辘轳势]敌任一单位[筋力]-30(非主力翻倍,演示);[重梅势][敏捷]-30;[长生势][耐久]-30(均可对任一单位且非主力翻倍)。");

    /* 接不归A:技能/宝具发动时,按剑势反击强化的神技(演示长生势) */
    r = db_skill(w, "接不归", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 10, 6, 0);
    db_eff(w, r, E_WIN(EF_RES_UP, 30, 1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 15, 1));
    db_set_text(w, r, "按[剑势]进行三种[反击]:[辘轳势]自身回转3且低等级技能发动时立即再次发动(不产生回转);[重梅势]成为非自身来源技能对象时以敏捷%判定,成功无效化那个技能(常驻技能则本场始终对自身无效);[长生势]己方主力成为目标时以耐久%判定(演示成功),给予己方主力[抗性上升:+30%]与[+抗性/2%]胜率补正(演示)。");

    /* 相思断A:主要工序,按剑势给属性惩罚或重骰随机属性(演示) */
    r = db_skill(w, "相思断", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PROC, 10, 3, 0);
    {
        E e = E_ATTR(EF_ATTR_DOWN, A_STR, 40, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "按[剑势]产生效果:[辘轳势]对敌主力以[50+筋力差]%负面判定(演示60%),成功给予除[回路][宝具]外任一属性-40;[重梅势]以[50+敏捷差]%判定,成功使目标任一低等级[常驻]技能效果无效;[长生势]以[50+耐久差]%判定成功时宣言属性并重骰随机属性(至多重复3次)。");

    /* 回龙征A:最终工序,按剑势给胜率惩罚或敏捷%即死(演示) */
    r = db_skill(w, "回龙征", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PROC, 10, 9, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 25, 1));
    db_set_text(w, r, "按[剑势]:[辘轳势]令敌主力以[自身筋力]%负面判定,成功时己方每有一项优势战斗属性给予-25%胜率(演示);[重梅势]以[自身敏捷]%判定,成功后对敌方任一单位以基础敏捷%[即死]判定(目标敏捷≥20减半);[长生势]以[自身耐久]%判定成功削减己方单位至多40胜率/属性惩罚。");

    /* 魔剑传承A:常驻,自资源库获取[魔剑]宝具(演示占位) */
    r = db_skill(w, "魔剑传承", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时额外支付2RP扩容,从从者资源库获取一件[面向:魔剑]的宝具(不产生首次解放效果,判定上视为A级[技艺]技能并移除[主力位]词条);受效果等级下降时每降1级其判定-20%成功率;作为额外技能获取时等级恒-2(演示占位)。");

    /* 一刀流A:随时,[反击]自身技能造成效果时敌-50%胜率(演示) */
    r = db_skill(w, "一刀流", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 0, 0, KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 50, 1));
    db_set_text(w, r, "[反击]自身发动技能时允许发动:本次技能造成效果时额外给予受效敌非[支援位]单位-50%胜率惩罚(演示;对多名单位生效时仅一名全额,其余-10%);一回合仅能生效一次;EX时额外[抗性下降:-10%](不持抗性上升者翻倍)。");

    /* 光芒一闪A:随时,以敏捷%判定给[耐久]-40与-10%胜率(演示) */
    r = db_skill(w, "光芒一闪", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 0, 9, 0);
    {
        E e = E_ATTR(EF_ATTR_DOWN, A_END, 40, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_WIN(EF_WIN_DOWN, 10, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
    E e1064 = E_STATUS(S_STUN, 1, 1); e1064.chance = 50; e1064.chance_neg = 1; db_eff(w, r, e1064);
    }
    db_set_text(w, r, "建卡时须持[一刀流];对敌方[主力位]以[自身敏捷]%负面判定(目标敏捷≥40减半,演示60%),成功给予[耐久]-40属性惩罚与-10%胜率,出目≤最终成功率/2时额外给予[晕眩1]。");

    /* 二河白道A:随时,敌非支援全体-30%胜率与[耐久]-10 */
    r = db_skill(w, "二河白道", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 0, 9, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 10, 0));
    db_set_text(w, r, "建卡时须持[二天一流];给予敌非[支援位]全体-30%胜率惩罚(非主力减半,演示全额)与[耐久]-10属性惩罚;有目标[耐久]因此归0时取消其[蓄力]效果。");

    /* ---- 御主·通用技能/魔眼/后勤/战斗 ---- */

    /* 迁延之魔眼A:初始工序,[支援]给予[迟滞3](演示) */
    r = db_skill(w, "迁延之魔眼", KS_T_TALENT, KS_RANK_A, KS_WHEN_PROC, 10, 9, KS_F_ASSIST);
    db_eff(w, r, E_STATUS(S_LAG, 3, 1));
    db_set_text(w, r, "[支援]给予敌方战斗位上一名单位[迟滞1],之后允许额外给予[迟滞2]并对自身造成一次[即死]效果(不付令咒即退场,演示3层)。");

    /* 歪曲之魔眼A:随时,[支援]属性-25并使蓄力无效化(演示) */
    r = db_skill(w, "歪曲之魔眼", KS_T_TALENT, KS_RANK_A, KS_WHEN_ANY, 20, 6, KS_F_ASSIST);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 25, 1));
    db_set_text(w, r, "[支援]给予战斗位除[支援位]外一名单位一项属性-25惩罚(演示筋力);目标该属性为所有属性中最低时,以[75-目标此属性]%判定(非首次目标-25%),成功令其处于[蓄力]的技能宝具无效化。");

    /* 往视之魔眼A:随时,[支援]知悉目标行动记录并获取技能效果信息 */
    r = db_skill(w, "往视之魔眼", KS_T_TALENT, KS_RANK_A, KS_WHEN_ANY, 20, 6, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_INFO, 1, 1));
    db_set_text(w, r, "[支援]指定同灵脉任一单位,知悉其2回合内的行动记录,并获取其曾经发动过的技能或宝具的效果信息(演示)。");

    /* 预见之魔眼A:随时,[支援]宣言限制目标下工序的技能发动(演示占位) */
    r = db_skill(w, "预见之魔眼", KS_T_TALENT, KS_RANK_A, KS_WHEN_ANY, 20, 6, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "[支援]方式1:指定战斗位任一单位,其暗宣言任意非[常驻]技能/宝具,自身知悉其效果;下一工序中目标必须发动/解放未暗宣言的技能宝具(对等级高于自身或特殊等级无效);方式2(A级):宣言目标任意多个非特殊等级且等级≤自身的技能宝具,下一工序其必须发动/解放(演示占位)。");

    /* 泡影之魔眼B:常驻,重复上一工序发动的技能效果(演示占位) */
    r = db_skill(w, "泡影之魔眼", KS_T_TALENT, KS_RANK_B, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "自身发起的行动判定-20%成功率;每工序一次宣言支付20魔力,指定自身上一工序发动过的等级≤自身的非[魔眼]技能,对相同目标造成那个技能的效果(仅对敌方战斗位单位生效,演示占位)。");

    /* 远视之魔眼C:常驻,允许[侦查];行动判定+10% */
    r = db_skill(w, "远视之魔眼", KS_T_MAGIC, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 10, -2));
    db_set_text(w, r, "允许自身进行[侦查]行动并令自身行动判定+10%成功率;自身不必处于战斗灵脉也能知悉其他灵脉战斗[战斗计算表]的内容;不与从者同灵脉也可使用令咒全部效果;EX时可作为[支援位]参战(无需同意,只能有空位;无胜率修正,撤退不耗FP)。");

    /* 生命之魔眼C:初始工序,40+等级差%[即死]判定(演示) */
    r = db_skill(w, "生命之魔眼", KS_T_MAGIC, KS_RANK_C, KS_WHEN_PROC, 20, 18, 0);
    db_eff(w, r, E_DEATH(40, 1, 1));
    db_set_text(w, r, "指定战斗位非[支援位]任一单位,令其进行[40+等级差]%[即死]判定(幸运≥20减半;非首次目标-25%,演示);成功时目标不付令咒即退场;EX时目标改任一战位单位。");

    /* 封印保存C:常驻,两个额外魔眼栏;发动魔眼技能+25%胜率(演示) */
    r = db_skill(w, "封印保存", KS_T_TECHNIQUE, KS_RANK_C, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN_CONST, A_MAG, 5, -2));
    db_set_text(w, r, "建卡时获得两个额外技能栏(仅用于额外购入两个[魔眼]技能,购入后变[技艺/兵器]类型);战斗中发动[魔眼]保有技能时按技能等级给予胜率补正(A+30%/B+25%/C+20%,演示25)与[幸运]+魔力属性补正,战斗结束每次生效后[耐久][魔力]-5常驻惩罚;EX时+20%胜率且未发动魔眼的轮次免魔耗。");

    /* 贫民窟侦探A:常驻,持情报调查信息的敌单位-25%胜率(演示) */
    r = db_skill(w, "贫民窟侦探", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 25, 1));
    db_set_text(w, r, "降临时获得随机5名御主的[基本资料]与降临灵脉;第一轮次开始获得5个灵脉全部单位的[外貌描述]与[能力面板];始终对敌战斗位自身已持[情报调查]信息的单位造成-25%胜率惩罚(演示);EX时每轮次开始随机获取一名御主的[情报调查]信息。");

    /* 灵体加工魔术A:随时,魂食/击杀御主转化为礼装获取(演示占位) */
    r = db_skill(w, "灵体加工魔术", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "仅建卡获取:1)自阵营单位通过[魂食]获得魔供时,消耗其中20魔力获得任一[基础礼装];2)自阵营击杀[御主]时获得任一[辅助/限定礼装];3)成功[恶性/无限制魂食]时消耗60魔力获[辅助/限定礼装];4)A级时[无限制魂食]时同样获取;一回合至多5次且仅限同灵脉(获取礼装视为制作,演示占位)。");

    /* 努力的结晶A:常驻,额外技能栏与他主职业技能(演示占位) */
    r = db_skill(w, "努力的结晶", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时额外持有一个技能栏,并选择除[异能者]外的任一其他主职业:获得其职业特性并将其一项主职业技能以B级模板获取(演示占位)。");

    /* 卢恩制作B:常驻,仅能制作职业礼装[卢恩符文] */
    r = db_skill(w, "卢恩制作", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "允许自身进行[礼装制作],以此方式进行的[礼装制作]只能制作职业礼装[卢恩符文](演示占位)。");

    /* 二重召唤咒文B:常驻,召唤Rider/Caster/Assassin/Berserker时赋予[二重召唤](演示占位) */
    r = db_skill(w, "二重召唤咒文", KS_T_MAGIC, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "仅建卡获取(与[狂化咒文]互斥):召唤从者时,若自阵营从者职阶为[Rider/Caster/Assassin/Berserker],赋予其[二重召唤]并触发建卡效果(视为保有技能);其他职阶或已持[二重召唤]则给予其+40魔力池上限与任一属性+20常驻补正(演示占位)。");

    /* 金钱的魅力C:常驻,降临时免费工房建造或科技造物(演示占位) */
    r = db_skill(w, "金钱的魅力", KS_T_TECHNIQUE, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时自选:1)降临时无需[工房组件],立即对降临灵脉进行3次不消耗行动的[工房建造](不触发[建造使魔],无视行动限制);2)获取1件[科技造物]并额外获取1件[弹药包](占用礼装栏但无需扩容)(演示占位)。");

    /* 狂化咒文C:常驻,召唤Berserker强化,否则给予[狂化](演示占位) */
    r = db_skill(w, "狂化咒文", KS_T_MAGIC, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "仅建卡获取(与[二重召唤咒文]互斥);始终知悉[Berserker]职阶从者的圣遗物与梦境;召唤从者时魔力池上限-50;召唤Berserker时给予其一项属性+20常驻补正(重复2次,已补正者减半);非Berserker则令其获取[狂化C]或[狂化EX]并触发建卡效果(演示占位)。");

    /* 兽化魔术A:常驻/随时,特性[猛兽],三属性+25与抗性+10%(演示) */
    r = db_skill(w, "兽化魔术", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 25, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 10, -2));
    db_set_text(w, r, "发动时本回合给予自身[特性赋予:猛兽]并自选一项常驻效果:1)三属性+25属性补正与[抗性上升:+10%](演示);2)允许[广泛侦查](70%判定但必定暴露灵脉);3)宣言一项属性获得等同[魔力]的补正(至多+50);同一时间仅一项生效,可再次发动切换;敏捷达80时允许[机动]。");

    /* 万华之杖A:常驻,获取职业礼装[愉快型魔术礼装](演示占位) */
    r = db_skill(w, "万华之杖", KS_T_WEAPON, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "仅建卡获取,且仅限主职业[魔术师]的单位:建卡时获取职业礼装[愉快型魔术礼装](演示占位)。");

    /* 魔术师的优雅C:常驻,[辅助位]按魔术技能造成的惩罚获得胜率补正(演示) */
    r = db_skill(w, "魔术师的优雅", KS_T_CROWN, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 30, -2));
    db_set_text(w, r, "[辅助位]仅建卡获取;自身存在[弱化/异常状态]时效果无效化;给予自身[+自身[魔术]技能和礼装造成的胜率/属性惩罚之和/2%]胜率补正(至多+100%,演示30);EX时不因弱化/异常状态失效。");

    /* 魔术师杀手C:常驻,敌方魔术技能更多者-20%胜率与抗性下降(演示) */
    r = db_skill(w, "魔术师杀手", KS_T_CROWN, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
    db_eff(w, r, E_WIN(EF_RES_DOWN, 5, 1));
    db_set_text(w, r, "给予敌方战斗位持[魔术]技能数大于自身的御主单位-20%胜率惩罚与[抗性下降:-5%](演示);自身非支援位时额外[抗性上升:+10%];持目标[情报调查][资料分析]信息时效果翻倍;持[Caster]职阶特性的从者也触发。");

    /* 龙告令咒C:常驻,失去一枚令咒并获[龙告令咒]礼装与[龙种]特性(演示占位) */
    r = db_skill(w, "龙告令咒", KS_T_BLESS, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时失去一枚令咒并获得职业礼装[龙告令咒];持有此技能时给予自身[特性赋予:龙种];EX时允许随时消耗一枚令咒制作[龙告令咒](演示占位)。");

    /* 魔弹装填C:常驻/随时,[魔弹宝石]/[魔力水晶]变职业礼装[魔力弹药];发动时敌-20%胜率 */
    r = db_skill(w, "魔弹装填", KS_T_MAGIC, KS_RANK_C, KS_WHEN_ANY, 0, 1, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 0));
    db_set_text(w, r, "允许指示自身持有的任一[魔弹宝石]/[魔力水晶]进行[礼装制作],判定成功时仅为指定礼装变为职业礼装[魔力弹药];发动时消耗一件[魔力弹药],给予敌非[支援位]全体-20%胜率惩罚(非主力-5%,演示)。");

    /* ---- 通用子职业:卢恩使 ---- */

    /* 流转之水(拉格斯)A:随时,交换两项基础属性(演示占位) */
    r = db_skill(w, "流转之水(拉格斯)", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 3, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "方式1:交换自身任两项属性的基础属性,并可将其中一项的属性补正转移至另一项(至多30);方式2:换回上次交换的属性并移除这两项属性的惩罚(至多50);两种方式交替(初始为方式1);EX时回转2(演示占位)。");

    /* 活力之火(肯纳兹)A:随时,三属性+30常驻补正(回合结束失去);无视惩罚时抗性+10% */
    r = db_skill(w, "活力之火(肯纳兹)", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 30, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 30, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 30, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 10, -2));
    db_eff(w, r, E_STATUS(S_BURN, 4, -2));
    db_set_text(w, r, "方式1:给予自身[筋力][耐久][敏捷]+30常驻补正(回合结束失去,演示);不持属性惩罚时额外[抗性上升:+10%];方式2:给予自身[灼伤4]后给予己方全体三属性+30属性补正并失去3回转;EX时额外支付20魔力使两种效果同时对自身生效。");

    /* 停滞之冰(伊莎)A:随时,冻结联动(演示方式2) */
    r = db_skill(w, "停滞之冰(伊莎)", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 9, 0);
    {
        E e = E_STATUS(S_FREEZE, 4, 1);
        e.chance = 85; e.chance_neg = 1;
        db_eff(w, r, e);
    db_eff(w, r, E_STATUS(S_FREEZE, 1, 1));
    }
    db_set_text(w, r, "方式1:指定战斗位非[支援位]任一单位宣言属性,本场战斗中目标对应属性始终与其当前数值相等(该属性本场补正>90时无效);方式2:给予敌主力[冻结1],指定其一属性以[110-指定属性]%负面判定(演示85%),成功给予[冻结4]。");

    /* 共荣之生灵(美娜兹)A:随时,同行动单位判定+25%;男女平等+胜率,混沌秩序平等+抗性(演示) */
    r = db_skill(w, "共荣之生灵(美娜兹)", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 6, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 25, -1));
    db_eff(w, r, E_WIN(EF_RES_UP, 25, -1));
    db_eff(w, r, E_WIN(EF_WIN_UP, 25, -1));
    db_set_text(w, r, "方式1:回合开始发动,本回合同灵脉进行相同行动的单位及[协助]者,行动需判定则+25%成功率(补魔则+35魔供);方式2:战斗开始发动,一方[男性][女性]数量相等则该方+25%胜率;一方[混沌][秩序]数量相等给该方[抗性上升:+25%](演示)。");

    /* 丰饶之天使(英格瓦兹)A:随时,移除异常并+魔供;或属性常驻补正(演示) */
    r = db_skill(w, "丰饶之天使(英格瓦兹)", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 6, 0);
    db_eff(w, r, E_WIN(EF_STATUS_REMOVE, S_POISON, 4));
    db_eff(w, r, E_MANA(EF_MANA_UP, 20, 1));
    db_set_text(w, r, "建卡时给予[成长1];方式1:指定同灵脉任一单位,按[成长]层数移除其异常状态(一次至多4层,演示),生效时给目标[移除层数*10]魔力供给并获移除层数*2回转(指定非自身时[成长+1]);方式2:给予自身2项属性[成长层数*5]常驻补正(轮次结束失去)。");

    /* 觉醒之白昼(达格斯)A:随时,觉醒4可免除退场或发动作战宣言(演示占位) */
    r = db_skill(w, "觉醒之白昼(达格斯)", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 999, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "每当自身参战、战斗胜利或[机动],记录[觉醒1];方式1:持[觉醒4]且因[即死]外原因退场时,移除觉醒免除退场,清除全部状态、魔力恢复上限、其他技能获全部回转,随后立即行动一次;方式2:持[觉醒4]夜回合宣言:下一轮次所有非仆役单位不产生等级魔耗,战斗胜利可立即行动一次(演示占位)。");

    /* 魔眼激发C:随时,令下一个魔眼技能获得[爆发] */
    r = db_skill(w, "魔眼激发", KS_T_MAGIC, KS_RANK_C, KS_WHEN_ANY, 20, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "每场战斗限一次生效:令自身下一个发动的[面向:魔眼]技能获得[爆发](演示占位)。");

    /* ---- 月姬扩充包:异能者/死徒/退魔师/混血/传承保菌者 ---- */

    /* 意识扩张A:常驻,允许[侦查];自身判定+25%成功率 */
    r = db_skill(w, "意识扩张", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 25, -2));
    db_set_text(w, r, "允许持有者进行[侦查]行动;自身发起的判定+25%成功率;EX时发生战斗时知悉战斗灵脉,本回合进行过[广泛侦查]时还获战斗计算表内容(演示)。");

    /* 弱点看破A:战斗开始时,[支援]宣言负面效果:敌主力受该效果判定+25%最终成功率 */
    r = db_skill(w, "弱点看破", KS_T_TALENT, KS_RANK_A, KS_WHEN_BATTLE_START, 0, 6, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_HIT_FINAL_UP, 25, 0));
    db_set_text(w, r, "[支援]宣言除[即死]外的任一[负面效果]:本场战斗中敌方[主力位]受到造成宣言效果的负面判定时,那次判定+25%最终成功率(目标数值最低的属性为[战斗属性]时再+10%,演示)。");

    /* 镜中形影A:常驻,回合开始90%判定获额外行动(演示) */
    r = db_skill(w, "镜中形影", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 20, -2));
    {
        E e = E_STATUS(S_STUN, 3, -2); e.chance = 10; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "每回合开始对自身进行90%判定:成功时本回合自身获得一次额外行动;失败时给予自身[晕眩3](演示判定补正近似)。");

    /* 继理血戒(全形态)A:战斗开始时,六选一(演示[剑]/[城]分支) */
    r = db_skill(w, "继理血戒(全形态)", KS_T_CROWN, KS_RANK_A, KS_WHEN_BATTLE_START, 20, 6, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 25, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 20, 1));
    db_eff(w, r, E_STATUS(S_FREEZE, 4, 1));
    db_set_text(w, r, "允许[恶性魂食][无限制魂食];建卡从六项效果自选:[剑]对敌主力[140-敏捷]%判定成功[耐久]-20与[抗性下降:-10%](魔性目标常驻化,演示);[城]敌全体-25%胜率且初始工序无法撤退(演示);[森林]随行结界;[热能]灼伤/冻结4层;[兽王之巢]聚合复活;[蔷薇之魔眼]状态赋予。");

    /* 暴走冲动A:常驻,[无限制魂食];四属性+30常驻(夜回合额外+10) */
    r = db_skill(w, "暴走冲动", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 30, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 30, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 30, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 30, -2));
    db_set_text(w, r, "允许[无限制魂食];给予自身+10等级补正与[筋力][耐久][敏捷][魔力]+30常驻补正([夜]回合与无限制魂食轮次内额外+10);[夜]回合开始80%无来源负面判定,成功则本回合必须至少一次[无限制魂食](无法进行则无效);无法与[反转冲动]同持(演示)。");

    /* 渴血恶灵A:常驻,魂食/击杀御主时全属性+10常驻(不可叠加) */
    r = db_skill(w, "渴血恶灵", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 10, -2));
    db_set_text(w, r, "自阵营击杀御主或[魂食]时,给予自身除[宝具]外全属性+10常驻补正与+10等级补正(无法叠加);首次[魂食]成功补正永久+10,首次[恶性魂食]成功再+10;4天未魂食失去补正;EX时首次无限制魂食+20(演示)。");

    /* 触觉延伸A:回合结束时,夜回合召唤[魔性]召唤物(演示) */
    r = db_skill(w, "触觉延伸", KS_T_TALENT, KS_RANK_A, KS_WHEN_ACT, 30, 6, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 10; e.cond_arg = 60; e.cond_arg2 = TR_DEMONIC;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "触觉延伸:召唤(等级10 全属性10 魔性)";
        db_eff(w, r, e);
    }
    db_set_text(w, r, "仅[夜]回合可发动:召唤等级10、全属性10、[魔性]特性的[召唤物](仅允许[干涉]且仅夜回合行动);同方战斗位时召唤物全属性+10(魂食回合翻倍);未魂食的夜回合召唤物必定随机干涉;魂食获魔供时可无视时机回转额外发动1次;至多同时5体;EX时基础属性+5(演示)。");

    /* 死亡初拥A:随时,[交流]中指定目标魔性化(演示占位) */
    r = db_skill(w, "死亡初拥", KS_T_TALENT, KS_RANK_A, KS_WHEN_ANY, 30, 12, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "仅[交流]中发动:指定当前灵脉除自身外任一持魔力池且非[构装体]单位,下回合开始以[自身基础魔力]%负面判定,成功给予[特性赋予:魔性]并使其随机获得死徒职业D级技能;宣言自罚全属性-10时(需目标允许)改为立即获得任一项死徒A级技能;每名单位仅能生效一次(演示占位)。");

    /* 退魔净眼A:常驻,知悉特性并获魔眼栏位(演示) */
    r = db_skill(w, "退魔净眼", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 10, -2));
    db_set_text(w, r, "允许知悉当前灵脉任意单位是否持有除[人型]外的特性及是否持[魔性];获得一个额外技能栏(仅能购入等级≤自身的[魔眼]技能);判定上具有[魔眼]字段;EX时知悉全部特性,栏位闲置时自身全部判定+20%(演示)。");

    /* 退魔冲动A:常驻,三属性+20常驻;持魔性者对立时全属性+25(演示) */
    r = db_skill(w, "退魔冲动", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_MAG, 25, -2));
    db_set_text(w, r, "始终知悉当前灵脉单位是否持[魔性];始终给予[筋力][耐久][敏捷]+20常驻补正;与持[魔性]单位对立战斗位时额外给予除[幸运][宝具]外全属性+25属性补正(演示魔力);干涉时首遇持[魔性]非同阵营单位须立即袭击;同样视为[反转冲动]技能。");

    /* 此身双灵A:常驻,回合开始自选判定强化(演示+40%基础成功率) */
    r = db_skill(w, "此身双灵", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 40, -2));
    db_set_text(w, r, "每回合开始时选择一项:1)本回合限一次,自身执行行动时宣言令本次行动判定+40%基础成功率(判定上性别视为[男性]);2)本回合限一次,自身发起非行动判定时宣言令其+40%基础成功率(性别视为[女性]);EX时两项时常生效且回合开始指定一项数值翻倍(演示)。");

    /* 七夜暗杀术A:常驻,属性转移到敏捷;被指定时敏捷/2-30%回避(演示) */
    r = db_skill(w, "七夜暗杀术", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 50, -2));
    db_set_text(w, r, "允许随时宣言给予自身[筋力][耐久]任意数值属性惩罚,并给予[敏捷]等同总值补正(至多+50,演示);被任一技能指定时以[敏捷/2-30]%判定,成功给予[回避:技能];EX时敌主力仅[人型]且无[魔性]时补正上限+30。");

    /* 祟神使役(退魔)A:回合开始时,75%判定令遮蔽魂食/侦查/礼装制作受惩罚(演示) */
    r = db_skill(w, "祟神使役(退魔)", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ACT, 0, 3, 0);
    db_eff(w, r, E_WIN(EF_HIT_PEN, 25, 0));
    db_set_text(w, r, "对自身进行75%判定(演示惩罚效果),成功时自选:1)本回合所有单位的[遮蔽魂食]受[-自身灵脉魔力量%]最终成功率惩罚;2)[广泛侦查]同效;3)[礼装制作]同效(至多-30%,不可叠加);EX时支付40魔力宣言情报单位,令其本回合所有判定受同值惩罚并立即失去15回转。");

    /* 净空感应A:常驻,魔力池上限+40;休整时魔力*2%判定给目标补正(演示) */
    r = db_skill(w, "净空感应", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_CIRCUIT, 40, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 20, 1));
    db_set_text(w, r, "给予自身+40魔力池上限(演示);[休整]时以[自身魔力*2]%判定,成功给予目标除[宝具]外任一属性+20常驻补正并额外+20魔力(不可叠加,可再次休整重新获取;演示);EX时可将魔力供给改为+10魔力池上限。");

    /* 破知化物A:常驻,魔力池上限+等级补正;对人型低等级者+10等级(演示) */
    r = db_skill(w, "破知化物", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_CIRCUIT, 20, -2));
    db_set_text(w, r, "给予自身魔力池上限+[自身等级补正](演示20);自身对持[人型]特性单位造成惩罚时,目标等级低于自身则+10等级补正(仅人型翻倍,至多+50);EX时可宣言至本轮结束魔力池上限降为0,获得[+减少值/2%]胜率补正(轮次结束失去12回转,演示)。");

    /* 禁缚术式A:随时,魔性者默认成功,给技能封印(演示) */
    r = db_skill(w, "禁缚术式", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 6, 0);
    {
        E e = E_STATUS(S_SKILL_SEAL, 1, 1);
        e.chance = 40; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "方式1:指定同灵脉一名[御主](无法机动且无从者同宣言退出干涉者不可主动退出),其下次技能发动时40%判定失败(不返还消耗);方式2:指定敌战斗位任一单位40%负面判定(持[魔性]默认成功),成功给予其任一已知技能[封印1](演示)。");

    /* 反转冲动A:常驻,[恶性魂食];回合开始失败判定给全属性+5(演示) */
    r = db_skill(w, "反转冲动", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 20, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 15, -2));
    db_set_text(w, r, "允许[恶性魂食];回合开始120%判定失败时给予除[宝具]外全属性+5常驻补正(逐次-10%成功率,至多+50)并立即进行[魂食](灵脉无法魂食则随机干涉并展示);回合开始/战斗中可消耗20魔力宣言立即发起判定并默认失败;魂食获魔供时立即触发判定;无法与[暴走冲动]同持(演示)。");

    /* 槛发A:随时,[支援]每工序开始选择灼伤/胜率惩罚/FP惩罚(演示) */
    r = db_skill(w, "槛发", KS_T_TALENT, KS_RANK_A, KS_WHEN_ANY, 20, 2, KS_F_ASSIST);
    {
        E e = E_STATUS(S_BURN, 2, 1);
        e.chance = 75; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_WIN(EF_WIN_DOWN, 30, 1);
        e.chance = 75; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[支援]发动后本场战斗每个战斗工序开始时自选:1)指定敌非[支援位]任一单位75%[灼伤]判定(演示),成功给[灼伤2];2)75%负面判定成功-30%胜率(演示);3)75%负面判定成功令目标本工序撤退FP+1;每项每场对一名单位仅生效一次;此技能判定受[行动]判定成功率补正影响。");

    /* 约束之神秘A:常驻,额外宝具栏购入从者宝具(演示占位) */
    r = db_skill(w, "约束之神秘", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时额外持有一个消耗3RP扩容的宝具栏,从从者资源库[即死/进攻/防御/增益/状态/决战]面向选择低于此技能等级的宝具获取(视为购入,适用从者购入规则,解放时正常计算消耗与回转);EX时额外2RP扩容允许获取A级宝具(演示占位)。");

    /* 流转之传承A:常驻,额外技能栏购入从者技能(演示占位) */
    r = db_skill(w, "流转之传承", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时额外持有一个消耗3RP扩容的技能栏,从从者资源库选择低于此技能等级且非[职阶]的技能获取(视为购入,适用从者购入规则);EX时额外2RP扩容允许A级技能(演示占位)。");

    /* 鞘中之锋刃A:随时,[反击]提高指定技能等级(演示占位) */
    r = db_skill(w, "鞘中之锋刃", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 3, KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "[反击]指定自身持有的任一技能或宝具,令其等级、发动条件和效果上升1级(回合结束失去);每个未发动此技能的回合结束其等级上升+1(发动后重置;宝具等级至高A级;演示占位)。");

    /* 鲜血之结末A:随时,给予[回路]数值的[宝具]属性补正并赋予加符(演示) */
    r = db_skill(w, "鲜血之结末", KS_T_TALENT, KS_RANK_A, KS_WHEN_ANY, 0, 999, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_NP, 30, -2));
    db_set_text(w, r, "本回合内始终给予自身等同[回路]属性数值的[宝具]属性补正(演示30),并赋予自身除此技能外任一不持加符的技能或宝具以加符;发动的轮次内自身[回路]属性始终为0。");

    /* 因子之烙印C:常驻,按宝具等级给等级补正;解放宝具时+魔力供给(演示) */
    r = db_skill(w, "因子之烙印", KS_T_TALENT, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 25, -2));
    db_set_text(w, r, "自身始终具有与持有宝具等级对应的[宝具]基础属性,并始终给予[等同宝具基础属性]的等级补正;战斗中宣言解放宝具时获得[自身宝具属性/2]的魔力供给(一次结算链内多次生效时除首次外减半,演示25)。");

    /* 王之现世身A:常驻,自选效果组合(演示魔力/抗性/等级三项) */
    r = db_skill(w, "王之现世身", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 10, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_CIRCUIT, 5, -2));
    db_set_text(w, r, "建卡时自选5项:轮次结束+10魔供(演示)/[抗性上升:+10%](演示)/任两项属性+5常驻/[回路]+5常驻/+10等级与[龙种/神性/魔性]/消耗3RP获取[封印礼装LogosReAct];EX时判定上同时视为[从者]单位(不产生等级魔耗,演示)。");

    /* ---- 月姬扩充包:代行者/驱魔师/修道士/圣堂骑士 ---- */

    /* 忏悔祷文A:随时,[反击]行动失败时取消重行动判定+50%(演示) */
    r = db_skill(w, "忏悔祷文", KS_T_BLESS, KS_RANK_A, KS_WHEN_ANY, 0, 3, KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_HIT_UP, 50, -2));
    db_set_text(w, r, "[反击]方式1:自阵营任一单位行动/行动判定失败时取消其行动并令其立即再次行动(本次判定+50%基础成功率,演示);方式2:自阵营单位技能宝具发动失败或其判定失败导致效果无效时,返还回转并允许再次发动(本次判定+25%最终成功率)。");

    /* 神祝仪式A:轮次开始时,同阵营单位+10等级与魔力幸运+25(演示) */
    r = db_skill(w, "神祝仪式", KS_T_BLESS, KS_RANK_A, KS_WHEN_ACT, 50, 6, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_MAG, 25, -1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_LUK, 25, -1));
    db_set_text(w, r, "给予当前灵脉上所有同阵营单位+10等级补正与[魔力][幸运]+25属性补正(敌方主力持[魔性]时数值翻倍;对持[魔性]单位无效;轮次结束失去);EX时对[魔性]单位同样生效但不翻倍(演示)。");

    /* 坚定信仰A:随时,[反击][支援]15%判定无效化技艺/魔术效果(演示) */
    r = db_skill(w, "坚定信仰", KS_T_BLESS, KS_RANK_A, KS_WHEN_ANY, 0, 1, KS_F_COUNTER | KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_EFFECT_IM, 1, -2));
    db_set_text(w, r, "[反击][支援]仅[技艺/魔术]技能或持[魔性]单位的技能指定自身时可发动:对自身进行15%负面判定,失败时使那个技能对自身产生的效果无效化(演示免疫);EX时改为回转0并立即给予自身[抗性上升:+20%](不可叠加)。");

    /* 对灵作战A:常驻,累计消耗100魔力给[魔力]+10常驻(演示) */
    r = db_skill(w, "对灵作战", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 10, -2));
    db_set_text(w, r, "自身每累计消耗100魔力,给予自身[魔力]+10常驻补正(此技能支付魔力时计数翻倍,至多+40);敌方主力持[魔性]时,自身发起判定或成为判定目标时,可支付至多25魔力令判定+支付魔力%基础成功率或惩罚;EX时常驻补正上限60(演示)。");

    /* 黑键精通A:常驻,持[黑键]时+25%胜率(演示) */
    r = db_skill(w, "黑键精通", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 25, -2));
    db_set_text(w, r, "自身持有[黑键]时始终+25%胜率补正(演示);发动过[黑键]的战斗中额外+25%([黑键]对[魔性]单位造成胜率惩罚时翻倍);EX时[黑键]获[反击]并可:1)成为[魔性]单位效果对象时[抗性上升:+15%];2)[黑键]指定自身移除-15%胜率惩罚;3)[黑键]仅指定[魔性]目标时额外[抗性下降:-5%]。");

    /* 铁甲作用A:常驻,发动[黑键]时额外-10%胜率(魔性翻倍) */
    r = db_skill(w, "铁甲作用", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 1));
    db_set_text(w, r, "自身发动[黑键]时,给予其指定的单位-10%胜率惩罚(目标持[魔性]时翻倍,演示)。");

    /* 灵慑喝止A:初始工序,60%[晕眩3]判定(魔性翻倍) */
    r = db_skill(w, "灵慑喝止", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PROC, 20, 3, 0);
    {
        E e = E_STATUS(S_STUN, 3, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_STATUS(S_STUN, 2, 1); e.cond = KC_TARGET_TRAIT; e.cond_arg = TR_DEMONIC;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "指定敌方战斗位任一单位,60%[晕眩]判定(演示),成功给予[晕眩3];目标持[魔性]时成功率翻倍并额外给[晕眩2];灵脉存在[驱魔结界]时可指定其为目标改对同灵脉其他单位判定(成功率减半);EX时目标已[晕眩]则结算并给[晕眩1]。");

    /* 驱魔禁制A:战斗开始时,50%负面判定成功[技能封印1](魔性出目1) */
    r = db_skill(w, "驱魔禁制", KS_T_MAGIC, KS_RANK_A, KS_WHEN_BATTLE_START, 0, 12, 0);
    {
        E e = E_STATUS(S_SKILL_SEAL, 1, 1);
        e.chance = 50; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "指定对方战斗位任一单位,50%负面判定(演示;指定从者时成功率减半),成功给予[技能封印1];目标持[魔性]时出目固定1且封印存在期间无法初始工序撤退;目标抗性上升≥25%时无效(魔性翻倍);灵脉有[驱魔结界]时可指定其为目标,魔性单位进入灵脉时即触发(成功率减半)。");

    /* 以儆效尤A:常驻,击杀魔性者/魔性在场胜利时全属性+10(至多+50) */
    r = db_skill(w, "以儆效尤", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 10, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_LUK, 10, -2));
    db_set_text(w, r, "始终知晓同灵脉单位是否持[魔性];自身击杀[魔性]单位,或敌方战斗位存在[魔性]单位的[战斗胜利]时,给予自身全属性+10常驻补正(至多+50,演示);本灵脉每存在一名[魔性]单位,给予自身除[宝具]外5项属性+5属性补正(同场者翻倍);在本灵脉击杀过[魔性]单位时常驻补正翻倍。");

    /* 告死预言B:战斗开始时,敌任一单位[魔力][幸运]任一-30(胜利转常驻) */
    r = db_skill(w, "告死预言", KS_T_BLESS, KS_RANK_B, KS_WHEN_BATTLE_START, 20, 6, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_MAG, 30, 1));
    db_set_text(w, r, "给予敌方场上任一单位[魔力][幸运]任一项-30属性惩罚(演示魔力;目标持[魔性]时翻倍);若[本场战斗胜利],令此属性惩罚转化为常驻惩罚。");

    /* 第四福音B:常驻,每场第3个[魔术]技能视为[祝福]并额外生效一次(演示) */
    r = db_skill(w, "第四福音", KS_T_BLESS, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时无需扩容;自身每场战斗发动的第3个[魔术]技能在本场战斗中视为[祝福]技能,并在发动并生效时额外生效一次(无法对[魔眼]技能生效;发动顺序按结算链决定;演示占位)。");

    /* 殉节历典A:技能/宝具发动时,[支援][反击]给予同灵脉单位[保护] */
    r = db_skill(w, "殉节历典", KS_T_BLESS, KS_RANK_A, KS_WHEN_ANY, 0, 6, KS_F_ASSIST | KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_PROTECT, 0, 1));
    db_set_text(w, r, "[支援][反击]自身不处战斗位时非反击发动无法指定战斗位单位;发动时给予同灵脉任一单位[保护](任意工序开始时失去);同灵脉单位成为低等级技能/宝具对象时可发动同等效果;[保护]转移宝具效果后此技能效果无效化;EX时允许对任意等级技能与≤A级宝具[反击](演示)。");

    /* 被虐灵媒A:常驻,休整对象+60魔供;灵脉有魔性者时全属性-5与抗性+15%(演示) */
    r = db_skill(w, "被虐灵媒", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 60, 1));
    db_eff(w, r, E_WIN(EF_RES_UP, 15, -2));
    db_set_text(w, r, "总是能获悉同灵脉单位是否持[魔性];与自身进行[休整]的单位获得60魔力供给(已获过则改[抗性上升:+30%]至下轮结束);目标持非自身来源[魔性]时清除之并额外给自身40魔力;灵脉存在[魔性]单位时给予自身除[宝具]外全属性-5常驻惩罚与[抗性上升:+15%](离开灵脉失去,演示)。");

    /* 治疗灵媒A:常驻,休整对象获等同[魔力]的魔供并移除常驻惩罚(演示) */
    r = db_skill(w, "治疗灵媒", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 50, 1));
    db_eff(w, r, E_WIN(EF_STATUS_REMOVE, S_POISON, 3));
    db_set_text(w, r, "与自身进行[休整]的单位获得等同[自身魔力]的魔力供给(至多60,演示50;已获过则改为移除其全部常驻惩罚);随后令目标任一[异常/弱化状态]失去至多3层(演示);EX时魔力供给翻倍。");

    /* 接续灵媒E:随时,视同传讯使魔干涉交流或契约之书使用(演示占位) */
    r = db_skill(w, "接续灵媒", KS_T_BLESS, KS_RANK_E, KS_WHEN_ANY, 0, 3, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "方式1:指定任一自阵营或自身持[情报调查][资料分析]信息的单位,视为已令一体传讯使魔对目标所处灵脉执行[干涉-交流];方式2:视同一次[契约之书]的使用(立约人只能是自己持情报的单位或自阵营单位;演示占位)。");

    /* 律戒原典A:常驻,[抗性上升:+40%]与+胜率;敌方有魔性者时己方再+10%(演示) */
    r = db_skill(w, "律戒原典", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_RES_UP, 40, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 10, -1));
    db_set_text(w, r, "给予自身[抗性上升:+40%],并令自身获得[+持有抗性上升/2%]胜率补正(演示20);敌方战斗位存在[魔性]单位时,额外给予己方战斗位全部单位[抗性上升:+10%](演示)。");

    /* 天声启示A:常驻,轮次数为2的倍数时敌方-20%胜率(演示) */
    r = db_skill(w, "天声启示", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 0));
    db_set_text(w, r, "当前轮次数为2的倍数时:1)知悉所有持[魔性]单位的[位置信息],本轮次自阵营单位处对立战斗位时,敌方全体-20%胜率(魔性者/主力位各再-20%,演示);2)知悉所有[魂食]者的位置,己方全体+20%胜率(主力位再+20%);对持[神性]且无[魔性]的敌方无效,对己方[魔性]单位的补正无效。");

    /* 自我鞭策A:回合开始时,90%判定成功获额外行动(演示) */
    r = db_skill(w, "自我鞭策", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ACT, 0, 3, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 20, -2));
    db_set_text(w, r, "进行90%判定:成功时本回合自身获得一次额外行动;失败时本回合自身无法行动;EX时判定视为行动判定,每成功一次+10%行动判定成功率(失败不再无法行动,改为清除该成功率补正,演示)。");

    /* 圣母圣咏A:常驻/行动阶段,结界效果或自身强化切换(演示方式2) */
    r = db_skill(w, "圣母圣咏", KS_T_WEAPON, KS_RANK_A, KS_WHEN_ACT, 0, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 30, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 20, -2));
    db_set_text(w, r, "消耗行动阶段在两种效果间切换(初始为1):1)魔耗0:给予当前灵脉单位[抗性上升:+25%](持[魔性]者改等量[抗性下降]);2)给自身+10等级、[魔力]+30常驻与[抗性上升:+20%](敌方战斗位存在[魔性]单位时数值翻倍,演示)。");

    /* 正式外典A:初始工序,80%负面判定成功给[耐久][幸运]-10常驻(魔性翻倍) */
    r = db_skill(w, "正式外典", KS_T_WEAPON, KS_RANK_A, KS_WHEN_PROC, 0, 6, 0);
    {
        E e = E_ATTR(EF_ATTR_DOWN, A_END, 10, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_ATTR(EF_ATTR_DOWN, A_LUK, 10, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "对敌方战斗位任一单位80%负面判定(幸运≥20减半;目标持[魔性]+50%,演示),成功给予[耐久][幸运]-10常驻惩罚(持[魔性]者翻倍)。");

    /* 否决圣典A:随时,令敌单位产生50魔力消耗(演示) */
    r = db_skill(w, "否决圣典", KS_T_BLESS, KS_RANK_A, KS_WHEN_ANY, 0, 6, 0);
    db_eff(w, r, E_MANA(EF_MANA_DOWN, 50, 1));
    {
        E e = E_ATTR(EF_ATTR_DOWN_CONST, A_MAG, 20, 1); e.cond = KC_TARGET_TRAIT; e.cond_arg = TR_DEMONIC;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "指定敌方战斗位任一单位,令其立即产生50魔力消耗(演示);目标持[魔性]时额外给予[魔力]-20常驻惩罚。");

    /* 火葬式典A:常驻,发动礼装时100%灼伤判定(魔性目标-10%胜率) */
    r = db_skill(w, "火葬式典", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    {
        E e = E_STATUS(S_BURN, 1, 0);
        e.chance = 100; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 0));
    }
    db_set_text(w, r, "每当自身发动礼装时,对敌非[支援位]全体进行100%[灼伤]判定(演示),成功赋予[灼伤1];目标持[魔性]时额外-10%胜率惩罚;最终工序时令敌方全部[魔性]单位[爆燃]。");

    /* ---- 赝作扩充包:执法者/军部/权贵 ---- */

    /* 执法搭档A:常驻,额外执法者御主卡面(演示占位) */
    r = db_skill(w, "执法搭档", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时额外持有一张等级20、保有3RP、40属性点、主职业[执法者]的御主卡面(视为[御主]单位,独立行动,无[回路]属性);不同卡面间无法持有相同技能;此技能同样视为[信任的伙伴](演示占位)。");

    /* 钓鱼执法A:回合开始时,指定灵脉机动/魂食通告并给[介入];战斗自阵营+40%胜率(演示) */
    r = db_skill(w, "钓鱼执法", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ACT, 0, 12, 0);
    db_eff(w, r, E_WIN(EF_WIN_UP, 40, -1));
    db_set_text(w, r, "指定任一灵脉,选择[机动]或[魂食]行动并在对应时机全局通告,给予所有可[机动]者[介入]指令;自阵营单位经此[介入]并在目标灵脉战斗时,给予战斗位自阵营单位+40%胜率补正(非主力减半,演示)。");

    /* 双枪绝技B:常驻,一个工序内可发动两件[手枪]科技造物 */
    r = db_skill(w, "双枪绝技", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "允许此技能持有者在一个战斗工序中发动两件模板名称带有[手枪]的[科技造物](演示占位)。");

    /* 卫星天眼C:常驻,指定单位始终知悉其灵脉信息(演示占位) */
    r = db_skill(w, "卫星天眼", KS_T_WEAPON, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "随时宣言指定任一持[外貌描述信息]的单位,自宣言起始终获取那个单位所处灵脉,包括行动、技能宝具发动及交流在内的所有信息;同一时间仅能对一名单位生效(演示占位)。");

    /* 枪械精通A:常驻,科技造物负面判定+25%,发动时+5%胜率(演示) */
    r = db_skill(w, "枪械精通", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_HIT_UP, 25, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    db_set_text(w, r, "使自身发动[科技造物]时,其带来的负面判定+25%成功率(演示);发动[科技造物]时给予自身+5%胜率补正(对敌主力造成效果时翻倍,至多+30%,演示)。");

    /* 战斗动员A:战斗开始时,召唤至多4个等级20/三属性30的召唤物(演示) */
    r = db_skill(w, "战斗动员", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_BATTLE_START, 0, 9, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 20; e.cond_arg = 90; e.cond_arg2 = TR_HUMAN;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "战斗动员:召唤(等级20 三属性30)";
        db_eff(w, r, e);
    }
    db_set_text(w, r, "战斗开始时立即召唤至多4个等级20、[筋力][耐久][敏捷]三项属性30的[召唤物](有空余战斗位可立即加入己方战斗位);此召唤物不产生魔力消耗,回合结束时立即退场(演示1体)。");

    /* 武装配给A:常驻,建卡指定科技造物获取;后续轮次再获取(演示占位) */
    r = db_skill(w, "武装配给", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时指定任一[科技造物]并获取(无需占用礼装栏);C级起第1/3/5轮次开始时指定又一[科技造物]获取;B级起每次获取额外给2件[弹药包];EX时第2/4轮次开始给予[弹药包]并获取指定造物(演示占位)。");

    /* 后勤支援B:随时,[支援]令科技造物恢复储备或回转(演示) */
    r = db_skill(w, "后勤支援", KS_T_WEAPON, KS_RANK_B, KS_WHEN_ANY, 0, 0, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_RECAST, 9, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 40, -1));
    db_set_text(w, r, "[支援]每轮次仅能发动一次:1)指定自阵营单位持有的任一[科技造物]获得全部[储备];2)指定任一[科技造物]获得全部回转(演示);3)行动阶段宣言失去18回转,令自阵营全部[科技造物]获全部储备与回转,并至下轮结束始终+40%胜率(演示)。");

    /* 区域封锁C:回合开始时,指定灵脉御主无法干涉并-10%胜率(演示) */
    r = db_skill(w, "区域封锁", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ACT, 0, 6, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 0));
    db_set_text(w, r, "指定任一灵脉并向其展露此技能:目标灵脉的[御主]单位本回合内,若无[从者]与其同灵脉宣言[干涉]则无法[干涉];参与战斗且处于此技能持有者对立战斗位时-10%胜率惩罚(演示);EX时目标灵脉单位无法[退出干涉]。");

    /* 迅速行军C:回合开始时,自阵营单位立即进行一次干涉(演示占位) */
    r = db_skill(w, "迅速行军", KS_T_TECHNIQUE, KS_RANK_C, KS_WHEN_ACT, 0, 9, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "指定任一灵脉,令当前灵脉上任意数量的自阵营单位立即对目标灵脉仅限一次不消耗行动阶段的[干涉](由于是回合开始时进行的干涉,优先于通常[机动],演示占位)。");

    /* 资金调取A:行动阶段,+1资金并可判定+2(演示占位) */
    r = db_skill(w, "资金调取", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ACT, 0, 6, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    {
        E e = E_STATUS(S_SKILL_SEAL, 2, -2); e.chance = 10; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "发动时获得[资金1];随后进行90%判定,成功再获[资金2];判定失败时随机给予自身除此技能外任一保有技能[封印2](演示占位)。");

    /* 资金周转A:常驻,建卡资金10并可转移给权贵(演示占位) */
    r = db_skill(w, "资金周转", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时获得[资金10];允许将自身持有的[资金]转移给同灵脉任一[权贵]职业的单位(演示占位)。");

    /* 炒股A:随时,资金4换判定获益(演示) */
    r = db_skill(w, "炒股", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 0, 3, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "消耗4资金进行80%判定:成功获得6资金(出目≤最终成功率/2再+1,≤5再+1);失败获3资金(出目>95无收益);可消耗行动阶段双倍资金产生双倍效果(演示占位)。");

    /* 雇佣兵小队A:轮次开始时,消耗资金4召唤雇佣兵部队(演示) */
    r = db_skill(w, "雇佣兵小队", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ACT, 0, 18, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 30; e.cond_arg = 120; e.cond_arg2 = TR_HUMAN;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "雇佣兵小队:召唤(等级30 全属性20)";
        db_eff(w, r, e);
    }
    db_set_text(w, r, "消耗[资金4]指定任一灵脉,昼回合结束通报全局;夜回合开始时在当前灵脉产生1个等级30、全属性20的[召唤物]与3个等级20、全属性10的[召唤物],并令其立即向指定灵脉进行[干涉];召唤物礼装发动次数0、不产生魔力消耗、轮次结束退场;随时消耗资金4使此技能获9回转(演示1体)。");

    /* 强力保安A:常驻,降临时召唤等级40的人型召唤物(演示) */
    r = db_skill(w, "强力保安", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 40; e.cond_arg = 180; e.cond_arg2 = TR_HUMAN;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "强力保安:召唤(等级40 全属性30 人型)";
        db_eff(w, r, e);
    }
    db_set_text(w, r, "建卡时可消耗[资金6]使此技能带来的召唤物获得除[宝具][魔力]外全属性+20常驻补正;降临时召唤一个等级40、除[宝具][魔力]外全属性30、持[人型]特性的[召唤物](不产生魔耗,仅自身处[战斗位]时可加入战斗);此技能[封印]时召唤物无法参战且属性变为0(演示)。");

    /* ================================================================
     * 全量录入 批次15:《空想礼装资源书》补全
     * ================================================================ */

    /* 群星魔杖:常驻,无法持令咒;令咒转化为职介卡(演示占位) */
    r = db_item(w, "群星魔杖", KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, KS_F_UNIQUE, 1, 0, 1);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "自身无法持有令咒;建卡时自身全部令咒自动转化为3张职介卡,选择其中1张获取,余下2张随机投放到不同灵脉上(演示占位)。");

    /* 超越魔剑:常驻,令咒转化并在战斗胜利时获取职介卡(演示占位) */
    r = db_item(w, "超越魔剑", KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, KS_F_UNIQUE, 1, 0, 1);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "自身无法持有令咒;建卡时自身全部令咒自动转化为3张职介卡并选择其中1张获取,每次战斗胜利时从余下职介卡中再选择1张获取(演示占位)。");

    /* 职介卡:常驻,持有16RP从者卡面可随时获取其技能宝具(演示占位) */
    r = db_item(w, "职介卡", KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, KS_F_UNIQUE, 1, 0, 1);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时获取一张具有16RP点的从者卡面;每回合一次在[交流]中或战斗开始时宣言,获取此卡面的技能宝具并给予自身对应[初始属性]的常驻补正(魔耗回转正常产生,[常驻]技能立即产生一次魔耗);此礼装生效期间自身原本技能宝具无效化;所有效果战斗/回合结束失去;不可摧毁,被俘虏时掉落(演示占位)。");

    /* 封印礼装·LogosReAct:随时,[十三封印]联动礼装(演示) */
    r = db_item(w, "封印礼装·LogosReAct", KS_RANK_C, KS_WHEN_ANY, 10, 9, KS_F_UNIQUE | KS_F_COUNTER, 1, 0, 1);
    db_eff(w, r, E_MANA(EF_MANA_DOWN, 5, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 5, -2));
    db_set_text(w, r, "建卡时若持有[誓约胜利之剑]或[闪耀于终焉之枪],令其获取EX效果(解放魔耗按达成条件数)并获取宝具[十三封印];持有此礼装时始终给予自身[宝具封印](封印期间宝具不暴露信息);方式1:每工序限一次令敌非[支援位]任一单位产生5魔力消耗(演示)并给自身[筋力/耐久/敏捷]任一+5属性补正(魔性目标数值10);方式2:[反击]移除封印立即解放对应宝具;十三封印达成7条以上时誓约胜利之剑/闪耀终焉之枪获加符后破弃此礼装。");

    /* 卢恩符文(礼装):常驻,宣言D级卢恩使技能并获得其效果(演示占位) */
    r = db_item(w, "卢恩符文", KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0, 1, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "此礼装被制作时,宣言任一D级的[卢恩使]职业的技能,将此技能的发动时机与效果变为与宣言技能相同;每个效果的卢恩符文仅能同时存在一个(演示占位)。");

    /* 护身结界(礼装):常驻,获得赋予此礼装的魔术结界效果(演示占位) */
    r = db_item(w, "护身结界", KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, KS_F_UNIQUE, 1, 0, 1);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "获得赋予此礼装的[魔术结界]的效果(不获结阵,可触发消耗结阵的效果;结界效果仅在该礼装持有者自身成为效果对象时生效,生效后破弃此礼装);赋予[闲人免进]时改为对非自阵营单位的混乱判定;此礼装总是最后结算(演示占位)。");

    /* 愉快型魔术礼装(礼装):常驻/随时,记录技能赋予(演示占位) */
    r = db_item(w, "愉快型魔术礼装", KS_RANK_C, KS_WHEN_ANY, 20, 6, KS_F_ASSIST, 1, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时以对应等级记录自身不持有面向的任一从者保有技能(仅A级以下);支付20魔力发动将记录技能赋予自身(常驻技能立即支付两倍魔耗;无效化时失去);作为礼装获取时效果为C级;持记录期间支付20魔力可:初始工序任意属性+10(至多20)/移除至多10属性惩罚(每场3次)/[反击]低等级技能宝具对自身效果降1级;判定上同时视为[魔术](演示占位)。");

    /* 魔力弹药:随时,令任一礼装/科技造物获得全部储备(演示) */
    r = db_item(w, "魔力弹药", KS_RANK_C, KS_WHEN_ANY, 0, 0, 0, 1, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "发动时令自身持有的任一[礼装][科技造物]获得全部[储备](对[唯一]礼装效果降低至50%),之后破弃此礼装;提供的[储备]在本回合结束时失去(演示占位)。");

    /* 调律器:常驻,[调律魔术]联动:回转>9者+3回转,异常弱化-1层(演示) */
    r = db_item(w, "调律器", KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, KS_F_UNIQUE, 1, 0, 1);
    db_eff(w, r, E_WIN(EF_RECAST, 3, -2));
    db_eff(w, r, E_WIN(EF_STATUS_REMOVE, S_POISON, 1));
    db_set_text(w, r, "回合结束时,所有在本回合内受到过自身[调律魔术]效果的单位,其当前回转>9的技能和宝具获得3回转(演示),持有的全部[异常/弱化状态]减少1层(无层数状态不受影响);每个回合每名单位仅能受到一次此礼装带来的效果。");

    /* 竞争者TheContender单发手枪:随时,[储备1/1]120-敏捷%判定成功-20%胜率 */
    r = db_tech(w, "竞争者TheContender单发手枪", KS_RANK_C, KS_WHEN_ANY, 0, 9, KS_F_ASSIST, 1);
    {
        E e = E_WIN(EF_WIN_DOWN, 20, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[储备1/1]指定敌方战斗位非[支援位]任一单位,以[120-目标敏捷]%负面判定(演示80%),成功给予-20%胜率惩罚(自身主力位时翻倍);受罚目标不持[抗性上升]时给予[抗性下降:-10%],持[抗性上升]则其本工序无效化。");

    /* 巫蛊人偶(礼装):常驻,轮次开始对记录御主40%即死判定(演示) */
    r = db_item(w, "巫蛊人偶(礼装)", KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, KS_F_UNIQUE, 1, 0, 1);
    {
        E e = E_DEATH(40, 1, 1);
        e.chance = 40; e.cond = KC_NONE;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[唯一]每轮次开始时,令被记录的御主单位受到40%[即死]判定并向其暴露自身所处灵脉(幸运≥20减半,演示);成功时其不付令咒即退场(可自罚三属性-10代替);目标无[人型]特性时此判定-20%;此即死判定同时视为[疲惫&中毒&诅咒&残废]判定;持有者与目标同阵营战斗战败时破弃此礼装。");

    /* 龙告令咒(礼装):轮次开始/战斗开始,龙种强化(演示) */
    r = db_item(w, "龙告令咒(礼装)", KS_RANK_C, KS_WHEN_BATTLE_START, 20, 0, KS_F_UNIQUE, 1, 0, 1);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 5, -2));
    db_set_text(w, r, "消耗此礼装:给予自身除[宝具]外全属性+5常驻补正并获[龙种E](已持[龙种]则等级/魔耗/效果+1);本回合级别修正至70并视为[从者],将[屠龙A][幻想大剑·天魔失坠A][恶龙之血铠B+]赋予自身(额外技能,回合结束失去);此礼装本身同样视为令咒;仅持[龙告令咒]技能的单位可发动(演示)。");

    /* ================================================================
     * 缺口补录(全量比对发现)
     * ================================================================ */

    /* 投影魔术A(从者版):随时,[支援]记录基础礼装并强化属性(演示) */
    r = db_skill(w, "投影魔术(从者)", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 0, KS_F_ASSIST);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 25, -2));
    db_set_text(w, r, "[支援]建卡时记录5件[基础礼装](允许消耗3件记录换取一件[科技造物]记录);给予自身[每轮次礼装发动次数]+5;发动时:1)获取任一记录礼装(视为[制作],轮次结束摧毁;每回合至多5次);2)指定自身持有的以此获取的礼装令其不摧毁;3)指定双方战斗位任一单位,令自身[筋力][耐久][敏捷]尽量提升至目标基础属性(至多+25,目标离开战斗位前持续,演示);EX时无法获取礼装但+20回路。");

    /* 黄金鹿与暴风夜A:战斗开始时,[主力位][骑乘]召唤构装体并持续灼伤压制(演示) */
    r = db_np(w, "黄金鹿与暴风夜", KS_NP_ARMORY, FC_SUMMON, KS_RANK_A,
              KS_WHEN_BATTLE_START, 80, 12, KS_F_MAIN | KS_F_RIDE);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 40; e.cond_arg = 120; e.cond_arg2 = TR_GOLEM;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "黄金鹿与暴风夜:召唤(等级40 全属性20 构装体)";
        db_eff(w, r, e);
        db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 0));
        db_eff(w, r, E_STATUS(S_BURN, 1, 0));
    }
    db_set_text(w, r, "[主力位][骑乘]解放时召唤任意数量等级40、全属性20、[构装体]的[召唤物](演示1体);其存在于战斗位时,每工序开始对敌非[支援位]全体造成[-(5*召唤物数量)%]胜率惩罚(演示20)并进行[40+10*数量]%负面判定,成功给[灼伤2](演示灼伤1);召唤物战斗结束或其耐久归0时消灭。");

    /* 剜穿鏖杀之枪A:初始工序,[主力位]蓄力后敌非支援50%[即死]与三属性-20 */
    r = db_np(w, "剜穿鏖杀之枪", KS_NP_ARMORY, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_PROC, 60, 12, KS_F_MAIN);
    db_eff(w, r, E_DEATH(50, 0, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 20, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 20, 0));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 20, 0));
    db_set_text(w, r, "[主力位]初始工序解放宣言[蓄力];主要工序对敌非[支援位]全部单位各进行50%[即死]判定(幸运≥40减半,演示),成功不付令咒即退场;随后给予敌非[支援位]全体[筋力][耐久][敏捷]-20属性惩罚(演示);解放当下自身[耐久]-20,战斗结束持[残废]则立即结算;EX时惩罚翻倍且追加最终成功率减半的即死判定。");

    /* 信任的伙伴A:常驻,额外御主卡面同行(演示占位) */
    r = db_skill(w, "信任的伙伴", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时额外持有一张[初始等级]与自身相同、保有40属性点、不持RP但能使用自身RP点、[回路]至多为自身[回路]/2的御主卡面;卡面视为[御主]单位,独立行动,与自身保有[魔力契约];不同卡面间无法在建卡时持有相同技能(演示占位)。");

    /* 魔术的馈赠A:常驻,建卡免费获取辅助/限定礼装或6件基础礼装(演示占位) */
    r = db_skill(w, "魔术的馈赠", KS_T_WEAPON, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "建卡时从以下效果选择一项:1)从[辅助礼装]与[限定礼装]中各选择1个获取;2)从[基础礼装]中任意选择6个获取(可重复获取同一礼装);此技能获得的礼装不占用礼装栏(演示占位)。");

    /* 洗礼咏唱A:随时,[支援]6次[50+魔力差]%判定,成功给属性-5惩罚 */
    r = db_skill(w, "洗礼咏唱", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 1, KS_F_ASSIST);
    {
        E e = E_ATTR(EF_ATTR_DOWN, A_STR, 5, 1);
        e.chance = 50; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[支援]仅主要/最终工序可发动:令敌方战斗位任一单位进行6次[50+魔力差]%负面判定(目标持[魔性]+25%,演示50%),判定成功给予其除[宝具]外任一属性-5属性惩罚(目标持[魔性]特性时翻倍)。");

    /* 破却宣言A:技能发动时,[反击]60%判定无效化目标发动(演示) */
    r = db_skill(w, "破却宣言", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 3, KS_F_COUNTER);
    {
        E e = E_WIN(EF_EFFECT_IM, 1, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[反击]敌方战斗位任一单位发动技能或礼装时对其发动,令目标进行60%负面判定(目标持[魔性]+50%,演示),成功令其本次发动带来的效果无效化;此技能使礼装效果无效化时立即获3回转。");

    /* 抵近射击A:随时,[反击]冲锋联动使科技造物无视战斗位发动(演示) */
    r = db_skill(w, "抵近射击", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 0, 1, KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
    db_set_text(w, r, "[反击]仅战斗位上可发动;自身[冲锋]或被[冲锋]时发动:令自身持的任一[科技造物]无视所处战斗位发动,必须指定冲锋目标(判定成功率提升一半,效果数值翻倍);目标受到了以此发动的效果时其本次[冲锋]判定默认失败;主力位时主要工序可额外发动一次(仅指定敌主力);[死斗]战斗中可在决胜前额外发动(演示胜率惩罚)。");

    /* ================================================================
     * 全量录入 批次5:《空想从者资源库》苍银扩充包
     * ================================================================ */

    /* 健硕A:常驻,状态抵抗:中毒;耐久不低于基础时抗性+20 */
    r = db_skill(w, "健硕", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_STATUS(S_STATE_RES, S_POISON, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 20, -2));
    db_set_text(w, r, "建卡时[耐久]分配上限+50;给予自身[状态抵抗:中毒];若自身[耐久]不低于[基础耐久],额外[抗性上升:+20%](EX时状态抵抗改状态免疫,演示)。");

    /* 弓矢制作A:行动阶段,令[箭矢-]获得[储备1] */
    r = db_skill(w, "弓矢制作", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ACT, 5, 0, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "令自身保有的[箭矢-]技能获得[储备1](同一轮次至多发动5次,重复发动消耗行动回合);必须与[箭矢-]一同获取并占用同一技能栏位;EX时重复发动不再消耗行动回合(演示占位)。");

    /* 变化A:随时,变形;不持技能信息者[资料分析]默认失败 */
    r = db_skill(w, "变化", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 10, 1, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "仅战斗外发动:自由改变[性别](A)/与其他单位视为同单位(B)/改变[特性][体型](C)/改变[能力面板](D)/改变立绘外貌(E);不持此技能信息者对变化中的自身[资料分析]默认失败;可随时不耗魔力取消(演示占位)。");

    /* 恐慌之声A:随时,敌非构装全体90%[恐惧]判定(溢出时追加晕眩) */
    r = db_skill(w, "恐慌之声", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 10, 6, 0);
    {
        E e = E_STATUS(S_FEAR, 1, 0);
        e.chance = 90; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_STATUS(S_STUN, 2, 0);
        e.chance = 30; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "令敌方战斗位全部非[构装体]单位进行90%[恐惧]判定;最终成功率>100%时,对溢出部分的对应目标额外进行[超出值]%[晕眩]判定,成功给予[晕眩2](演示30%)。");

    /* 自我改造A:行动阶段(灵脉主限定),魔力*2%判定成功两项属性+5常驻 */
    r = db_skill(w, "自我改造", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ACT, 20, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 5, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 5, -2));
    db_set_text(w, r, "自身为当前灵脉持有者时才能发动:消耗行动阶段,以[自身魔力*2]%判定,成功给予自身除[宝具]外两项属性+5常驻补正(总值至多+80,达上限时+30等级补正;判定上视为[魔术]技能,演示)。");

    /* 太阳神的加护A:常驻,昼回合发动:魔力幸运+20常驻 */
    r = db_skill(w, "太阳神的加护", KS_T_BLESS, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, 0);
    {
        E e1065 = E_ATTR(EF_ATTR_UP_CONST, A_MAG, 20, -2); e1065.cond = KC_DAY; db_eff(w, r, e1065);
        e = E_ATTR(EF_ATTR_UP_CONST, A_LUK, 20, -2); e.cond = KC_DAY; db_eff(w, r, e);
    }
    db_set_text(w, r, "仅当前灵脉处于[昼]回合时生效:始终给予自身+20等级补正与[魔力][幸运]+20常驻补正(战斗内自身处[主力位]时己方全部单位同受);[战斗胜利]时可使自身常驻技能宝具魔耗变为0或永久-15(演示昼条件)。");

    /* 英雄的伴娘C:随时,[支援][反击]己方单位发起判定时+15%基础成功率 */
    r = db_skill(w, "英雄的伴娘", KS_T_BLESS, KS_RANK_C, KS_WHEN_ANY, 0, 3, KS_F_ASSIST | KS_F_COUNTER);
    db_eff(w, r, E_WIN(EF_HIT_UP, 15, -1));
    db_set_text(w, r, "[支援][反击]当前灵脉除自身外任一单位发起判定时,使该判定+15%基础成功率补正(同一判定无法受多次此技能补正);此技能仅能在回合结束时获得回转补充(演示)。");

    /* 箭矢-(视为C):初始工序,[储备10/10]敌单体属性-10 */
    r = db_skill(w, "箭矢-", KS_T_WEAPON, KS_RANK_NEG, KS_WHEN_PROC, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 10, 1));
    db_set_text(w, r, "[储备10/10]初始工序:给予敌方战斗位任一单位除[宝具]外一项属性-10(储备-1);效果同时视为礼装[魔法箭矢]但不计入礼装使用次数;同一工序多次发动视为同一次;必须与[弓矢制作]一同获取(演示按10储备)。");

    /* 投掷(短刀)B:随时,[支援]以150-目标敏捷%判定给予[中毒2] */
    r = db_skill(w, "投掷(短刀)", KS_T_WEAPON, KS_RANK_B, KS_WHEN_ANY, 0, 3, KS_F_ASSIST);
    {
        E e = E_STATUS(S_POISON, 2, 1);
        e.chance = 90; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[支援]令当前灵脉任一单位以[150-目标敏捷]%成功率进行[中毒]判定,成功给予[中毒2](演示90%);EX时给予[蚀毒],目标[中毒]不再因[耐久]减半。");

    /* 陨铁之鞴(原初之火)A:常驻/随时,[反击]与[魔力放出(炎)]联动,灼伤判定与-5%胜率 */
    r = db_skill(w, "陨铁之鞴(原初之火)", KS_T_WEAPON, KS_RANK_A, KS_WHEN_ANY, 10, 0, KS_F_COUNTER);
    {
        E e = E_STATUS(S_BURN, 1, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
        e = E_WIN(EF_WIN_DOWN, 5, 1);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "不持有[魔力放出(炎)]时以同等级模板获取之(此技能魔耗+10并失去发动效果);持有[魔力放出(炎)]时发动其可再发动此技能:对目标60%[灼伤]判定(每工序+20%、战斗结束重置),成功给予[灼伤1]与-5%胜率;或对敌非[支援位]全体进行灼伤判定;自阵营无令咒时成功再给属性-5(演示)。");

    /* 巨兽猎手A:常驻,[主力位]敌方存在巨大/猛兽等时+30%胜率与抗性、判定+30% */
    r = db_skill(w, "巨兽猎手", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_UP, 30, -2));
    db_eff(w, r, E_WIN(EF_RES_UP, 30, -2));
    db_eff(w, r, E_WIN(EF_HIT_UP, 30, -2));
    db_set_text(w, r, "[主力位]敌方战斗位任意单位持[巨大/超巨大/猛兽]特性或三属性合计≥270时:自身+30%胜率补正与[抗性上升:+30%],发起的所有判定+30%基础成功率;敌方每解放一件[对人]宝具,自身全属性+10(演示)。");

    /* 静谧的舞蹈A:战斗开始时,敌方战斗位全体[中毒2] */
    r = db_skill(w, "静谧的舞蹈", KS_T_CROWN, KS_RANK_A, KS_WHEN_BATTLE_START, 10, 6, 0);
    db_eff(w, r, E_STATUS(S_POISON, 2, 0));
    db_set_text(w, r, "战斗开始时赋予敌方战斗位全体[中毒2];因此获[中毒]的单位本场受到负面判定时成功率+30%;目标持任何来源的[魅惑]时清除并给予相同层数(演示)。");

    /* 元素精灵A:随时,[储备0/10]异常赋予联动(冻结/灼伤/感电),且敌方技能对自身效果降3级 */
    r = db_skill(w, "元素精灵", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 1, 0);
    db_eff(w, r, E_STATUS(S_BURN, 1, 1));
    db_eff(w, r, E_STATUS(S_ELECTRIC, 1, 1));
    db_eff(w, r, E_STATUS(S_FREEZE, 1, 1));
    db_set_text(w, r, "[储备0/10]每持[储备1]使魔力池上限+10(仅用于此技能);方式1:行动阶段发动储备+1;方式2:[反击]自身技能宝具将给予异常时额外给予冻结/灼伤/感电各1(演示);方式3:[反击]成为敌方保有技能目标时,其效果仅对自身下降3级。");

    /* 贤者之石A:随时,解除目标异常2层;或使技能效果等级上升2(演示) */
    r = db_skill(w, "贤者之石", KS_T_MAGIC, KS_RANK_A, KS_WHEN_ANY, 20, 0, 0);
    db_eff(w, r, E_WIN(EF_STATUS_REMOVE, S_POISON, 1));
    db_set_text(w, r, "每次发动后此技能下降一级(E级时无法发动);方式1:消耗行动阶段额外支付20魔力使等级不降反升;方式2:[支援]指定同灵脉单位,任一项[异常状态]减少2层(无层数则移除,演示);B级起技能发动时其效果等级上升2级;A级起使目标在本次负面判定中[抗性上升:+30%]。");

    /* ---- 苍银扩充包 · 宝具 ---- */

    /* 光辉复合大神殿A:战斗开始时,[主力位]固有结界;每工序敌全体-40%胜率 */
    r = db_np(w, "光辉复合大神殿", KS_NP_BOUND, FC_DEFENSE, KS_RANK_A,
              KS_WHEN_BATTLE_START, 100, 12, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 0));
    {
        E e = E_STATUS(S_NP_SEAL, 1, 0);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[主力位]解放生成[固有结界:光辉神殿](宽6/永昼/初始工序无法撤退);每工序开始限一次消耗80魔力,给予敌全体-40%胜率惩罚;结界内敌方低于此宝具等级的非[常驻]宝具[封印](演示);结界可独立为额外灵脉移动;EX时获得魔量60并改为己方+30%抗性。");

    /* 暗夜太阳船A:常驻,[骑乘]每工序开始对敌非支援全体90%[灼伤2]判定 */
    r = db_np(w, "暗夜太阳船", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PASSIVE, 30, 0, KS_F_RIDE);
    {
        E e = E_STATUS(S_BURN, 2, 0);
        e.chance = 90; e.chance_neg = 1;
        db_eff(w, r, e);
    E e1066 = E_WIN(EF_WIN_DOWN, 10, 0); e1066.flag = EF_TICK_PROC; e1066.status = S_BURN; e1066.chance = 90; db_eff(w, r, e1066);
    }
    db_set_text(w, r, "[骑乘]每个工序开始时,令敌方战斗位除[支援位]外全体进行90%[灼伤]判定,成功给予[灼伤2]并立即结算一次[灼伤]效果(演示)。");

    /* 元素使的魔剑A:初始工序,敌魔力池-35;或转移目标等级魔耗(演示) */
    r = db_np(w, "元素使的魔剑", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 30, 9, 0);
    db_eff(w, r, E_MANA(EF_MANA_DOWN, 35, 0));
    db_set_text(w, r, "方式1[对人]:指定敌非[支援位]任一单位,将其等级战斗魔耗转移至现在结算并立即结算其魔力不足惩罚,此宝具获6回转(其后按宣言属性惩罚总值双方等量受罚);方式2[对军]:额外支付60魔力,敌持魔力池的非[支援位]单位-35魔力消耗(演示方式2)。");

    /* 只身孤影的冥府之旅A:主要工序,[主力位]孤影机制,蓄力后敌非支援-40%胜率(演示) */
    r = db_np(w, "只身孤影的冥府之旅", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 20, 9, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 0));
    E e1067 = E_STATUS(S_CHARGE, 2, -2); e1067.flag = EF_CHARGE; db_eff(w, r, e1067);
    db_set_text(w, r, "[主力位]仅己方战斗位只有自身时解放:[蓄力];蓄力期间按敌方单位数/劣势属性数/魔力不足记录[孤影];最终工序时[孤影≤9]给予敌非[支援位]全体[-(孤影数*20)%]胜率惩罚,孤影>6时对自身[即死];战斗胜利时移除全部孤影并按数获回转(演示2孤影)。");

    /* 童女讴歌的荣华帝政A:最终工序,[主力位]敌非支援全体-20%胜率,兵器技能免回转发动 */
    r = db_np(w, "童女讴歌的荣华帝政", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 40, 6, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 0));
    db_set_text(w, r, "[主力位]最终工序:给予敌非[支援位]全体-20%胜率惩罚;自身等级≤此宝具的任一[兵器]技能无视回转立即发动一次(回转延迟至战斗结束产生);回转与[星驰终幕的蔷薇]共享(演示)。");

    /* 星驰终幕的蔷薇A:最终工序,[主力位]按灼伤层数给予敌主力胜率惩罚(演示) */
    r = db_np(w, "星驰终幕的蔷薇", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 40, 6, KS_F_MAIN);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
    db_set_text(w, r, "[主力位]方式1:按战斗位全体单位持有的[灼伤]层数,给予敌主力[-(5%*灼伤层数)]胜率惩罚(自身灼伤层数翻倍计算,演示20);方式2:按记录属性惩罚总值给予等量胜率惩罚;结算时机为决胜检定前且一场一次;黄金剧场被摧毁时给予全场[灼伤3];仅能经[黄金剧场]获取。");

    /* 王律键A:随时,敌单体-5%胜率重复5次;初始工序开始时获1回转 */
    r = db_np(w, "王律键", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_ANY, 30, 1, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 5, 1));
    db_set_text(w, r, "解放时对敌非[支援位]任一单位造成-5%胜率惩罚,重复5次(演示按行叠加);初始工序开始时立即获得1回转;EX时改为对敌全体生效。");

    /* 热砂狮身兽A:随时,召唤巨大召唤物(2仆役位),其存在时每工序给予敌主力灼伤 */
    r = db_np(w, "热砂狮身兽", KS_NP_ARMORY, FC_SUMMON, KS_RANK_A,
              KS_WHEN_ANY, 60, 12, 0);
    {
        E e; memset(&e, 0, sizeof(e));
        e.chance_attr_base = -1;
        e.flag = EF_SUMMON;
        e.value = 70; e.cond_arg = 280; e.cond_arg2 = TR_BEAST;
        e.status = KS_SLOT_SERVANT;
        e.target = -2;
        e.desc = "热砂狮身兽:召唤(等级70 总属性280 巨大)";
        db_eff(w, r, e);
        db_eff(w, r, E_STATUS(S_BURN, 1, 1));
    }
    db_set_text(w, r, "召唤等级70、总属性280、[巨大]的召唤物(需2仆役位参战);其存在于战斗位时,每工序开始给予敌方主力位等同己方[召唤物]数量的[灼伤]层数(不可叠加,战斗开始时解放则本场无效;演示1层)。");

    /* 妄想毒身C:常驻,[状态免疫:中毒];回合开始/干涉时80%[中毒1]判定 */
    r = db_np(w, "妄想毒身", KS_NP_HUMAN, FC_STATUS, KS_RANK_C,
              KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_STATUS(S_STATE_IM, S_POISON, -2));
    {
        E e = E_STATUS(S_POISON, 1, 1);
        e.chance = 80; e.chance_neg = 1;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "给予自身[状态免疫:中毒];回合开始或[干涉]时指定同灵脉任一单位进行80%[中毒]判定(目标敏捷≥20减半,最终成功率>100%额外中毒1);目标持来源于自身的[魅惑]时可移除魅惑并令判定+30%/层;目标不持[中毒免疫]则给予自身[魅惑2](演示)。");

    /* 隐秘的罪之游戏C:随时,Assassin/Berserker专用,交换[气息遮蔽]/[狂化] */
    r = db_np(w, "隐秘的罪之游戏", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_C,
              KS_WHEN_ANY, 20, 6, 0);
    db_eff(w, r, E_WIN(EF_OTHER, 0, -2));
    db_set_text(w, r, "仅[Assassin][Berserker]持有;解放时将[气息遮蔽]/[狂化]替换为同等级的[狂化]/[气息遮蔽](含建卡效果);解放时魔力<0则补足至0;EX时可记录保有技能并在解放时交换(演示占位)。");

    /* 直至死亡拆散两人A:战斗开始时,[无敌贯通]给予胜者[英雄];解放时按胜利数全属性惩罚 */
    r = db_np(w, "直至死亡拆散两人", KS_NP_HUMAN, FC_ANTITRAIT, KS_RANK_A,
              KS_WHEN_BATTLE_START, 0, 6, KS_F_INV_PIERCE);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_MAG, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_LUK, 10, 1));
    db_set_text(w, r, "[无敌贯通]任意从者[战斗胜利]时(无[英雄]标记者时)必定给予其[英雄]标记;敌方战斗位存在[英雄]时解放,按其[战斗胜利]数给予全属性[-10*胜利数]属性惩罚(演示1胜);B级对[守序/善良]目标额外-10;A级目标胜利达3时双方各进行50%[即死]。");

    /* 十三封印·Seal·Thirteen-(视为A):常驻,誓约胜利之剑/闪耀终焉之枪的条件解放(演示) */
    r = db_np(w, "十三封印·Seal·Thirteen", KS_NP_HUMAN, FC_ANTITRAIT, KS_RANK_NEG,
              KS_WHEN_PASSIVE, 20, 0, 0);
    db_eff(w, r, E_WIN(EF_FINAL_WIN_UP, 30, -2));
    db_set_text(w, r, "仅建卡获取:自身[誓约胜利之剑][闪耀于终焉之枪]无法经令咒宣言解放;解放宣言时判定13条条件(令咒0/被袭击/等级差/无魂食/真言/无精灵字段/孤身/恶阵营/敌方魂食/人类威胁/真名展露/同盟契约/混沌非善):达成6项以上时轰击无需令咒,7项以上轰击默认成功,13项全达成时无需蓄力且+30%最终胜率(演示最终胜率)。");

    /* ================================================================
     * 全量录入 批次6:《空想从者资源库》北欧扩充包
     * ================================================================ */

    /* 龙种改造A:常驻,始终[特性赋予:龙种];回路(演示属性)补正+50 */
    r = db_skill(w, "龙种改造", KS_T_TALENT, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 50, -2));
    db_set_text(w, r, "建卡时可购入[龙之心];始终给予自身[特性赋予:龙种](不因无效化失去);不持有[回路]时给予[回路]+[基础筋力]常驻补正(至多+50,演示魔力);允许视为[御主]参与[休整];EX时失去灵脉/圣杯供魔但不强制退场(特性文本)。");

    /* 命运纺织A:行动阶段,魔力补给/解除异常/消除常驻惩罚(演示) */
    r = db_skill(w, "命运纺织", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ACT, 0, 9, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 40, 1));
    db_eff(w, r, E_WIN(EF_STATUS_REMOVE, S_CURSE, 1));
    db_set_text(w, r, "指定当前灵脉任一单位后宣言:1)目标魔力<0时给予40魔力补给;2)移除目标任一[异常状态]至多3层(无层数无效);3)消除目标3项属性受到的常驻惩罚(演示1与2);指定自身时立即获6回转。");

    /* 复仇计划(狂奔)A:随时,战败后给予[仇敌]与[复仇];对仇敌按层数惩罚(演示) */
    r = db_skill(w, "复仇计划(狂奔)", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 0, 3, 0);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 1));
    db_set_text(w, r, "1)自身战败时给予战胜方[仇敌][复仇1];2)战斗开始时给予持[仇敌]者[抗性下降:-复仇数*10%]并清除[复仇](演示胜率惩罚近似);3)主要工序给予持[仇敌]者全属性[-复仇数*5];回合开始持[仇敌]者各[复仇1](至多5层)。");

    /* 雏鸟礼装B:战斗开始时,给予自身[回避:技能] */
    r = db_skill(w, "雏鸟礼装", KS_T_BLESS, KS_RANK_B, KS_WHEN_BATTLE_START, 10, 3, 0);
    db_eff(w, r, E_WIN(EF_EVADE, 0, -2));
    db_set_text(w, r, "战斗开始时给予自身[回避:技能];若此[回避]无效化了一个A级或特殊等级技能的效果,则立即破除此技能(演示)。");

    /* 杀戮报偿A:战斗开始时,[支援]按阵营对立给予全场[抗性下降](演示) */
    r = db_skill(w, "杀戮报偿", KS_T_BLESS, KS_RANK_A, KS_WHEN_BATTLE_START, 25, 9, KS_F_ASSIST);
    db_eff(w, r, E_WIN(EF_RES_DOWN, 10, 0));
    db_set_text(w, r, "[支援]双方战斗位每存在1对[守序/混乱]或[善良/邪恶]阵营单位,给予除自身外全部单位[抗性下降:-10%](单体至多-50%);主要工序开始若双方未发动过含[负面判定]的技能宝具,移除该抗性下降并改给等值胜率惩罚(演示)。");

    /* 睿智的结晶A:战斗开始时,宣言非职阶/天赋/祝福/荣冠技能,敌方主力无法发动并常驻无效 */
    r = db_skill(w, "睿智的结晶", KS_T_CROWN, KS_RANK_A, KS_WHEN_BATTLE_START, 0, 3, 0);
    db_eff(w, r, E_STATUS(S_SKILL_SEAL, 1, 1));
    db_set_text(w, r, "战斗开始时宣言一项非[职阶&天赋&祝福&荣冠]技能:本场战斗敌方[主力位]无法发动等级不高于此技能的宣言技能,其已产生的常驻效果无效化(演示技能封印);同一时间仅能对一个技能生效。");

    /* 狂战士A:战斗开始时,状态抵抗;三属性+25补正;御主失令咒+10%胜率(演示) */
    r = db_skill(w, "狂战士", KS_T_CROWN, KS_RANK_A, KS_WHEN_BATTLE_START, 20, 0, 0);
    db_eff(w, r, E_STATUS(S_STATE_RES, S_FEAR, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CHARM, -2));
    db_eff(w, r, E_STATUS(S_STATE_RES, S_CONFUSE, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 25, -2));
    db_set_text(w, r, "每场战斗限一次发动:给予自身[状态抵抗:魅惑&恐惧&混乱]与[筋力][耐久][敏捷]+25属性补正;本场自阵营御主失去令咒时自身+10%胜率(可叠加);本场战斗工序内自身无法撤退。");

    /* 高贵少女之爱C:常驻,魔力池上限+60;累计消耗100魔力则[宝具]属性+10(演示) */
    r = db_skill(w, "高贵少女之爱", KS_T_CROWN, KS_RANK_C, KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_MANA(EF_MANA_UP, 60, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_NP, 10, -2));
    db_set_text(w, r, "给予自身魔力池上限+60;除等级魔耗外,每累计产生100魔力消耗,自身[宝具]属性+10常驻补正(至多+30;魔力转移不计入,演示)。");

    /* 虚数美术A:常驻,回合开始/干涉时60%[诅咒1]判定;轮次结束按诅咒+魔供(演示) */
    r = db_skill(w, "虚数美术", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    {
        E e = E_STATUS(S_CURSE, 1, 0);
        e.chance = 60; e.chance_neg = 1;
        db_eff(w, r, e);
        db_eff(w, r, E_MANA(EF_MANA_UP, 5, -2));
    }
    db_set_text(w, r, "回合开始或[干涉]时,对同灵脉所有非仆役单位进行[60+5*自身诅咒层数]%[诅咒]判定;战斗开始时将自身效果给予的[诅咒]转移给自身并可按诅咒层数%给恐惧;轮次结束时每持[诅咒1]自身+5魔力供给(演示)。");

    /* 支援咒术C:随时,[支援]目标全属性-10与[抗性下降:-20%](对魔力减半) */
    r = db_skill(w, "支援咒术", KS_T_MAGIC, KS_RANK_C, KS_WHEN_ANY, 20, 6, KS_F_ASSIST);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_AGI, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_MAG, 10, 1));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_LUK, 10, 1));
    db_eff(w, r, E_WIN(EF_RES_DOWN, 20, 1));
    db_set_text(w, r, "[支援]指定战斗位任一单位,给予其全属性-10属性惩罚与[抗性下降:-20%](目标持[对魔力]时效果减半);效果同时视为[疲惫&中毒&诅咒&残废]带来的效果(演示)。");

    /* 黄房子A:常驻,建卡诅咒6;自身诅咒改为对负面判定成功率惩罚(演示) */
    r = db_skill(w, "黄房子", KS_T_MAGIC, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
    db_eff(w, r, E_STATUS(S_CURSE, 6, -2));
    db_eff(w, r, E_WIN(EF_HIT_PEN, 25, -2));
    db_set_text(w, r, "建卡时给予自身[诅咒6];自身[诅咒]无法被转移,改为:自身受不来源于自身的负面判定时,那个判定-5%基础成功率(至多-40%,战斗位时对己方其他单位半值);战斗结束失去1层诅咒并使其他参战单位[诅咒1](演示)。");

    /* ---- 北欧扩充包 · 宝具 ---- */

    /* 终末幻想·少女降临A:初始工序,[蓄力]最终工序对敌从者/仆役40%[即死](魔性/魔兽+20%) */
    r = db_np(w, "终末幻想·少女降临", KS_NP_ARMORY, FC_DECISIVE, KS_RANK_A,
              KS_WHEN_PROC, 70, 18, KS_F_MAIN);
    {
        E e = E_DEATH(40, 0, 1);
        e.cond = KC_NONE;
        db_eff(w, r, e);
    E e1068 = E_STATUS(S_CHARGE, 2, -2); e1068.flag = EF_CHARGE; db_eff(w, r, e1068);
    }
    db_set_text(w, r, "[主力位]初始工序解放宣言[蓄力];最终工序对敌非[支援位]全部[从者][仆役]单位40%[即死](目标持[魔性]/[魔兽]+20%,幸运≥40减半);之后无效化当前灵脉[蓄力]中的[魔术]技能并摧毁全部[魔术结界];己方有其他非仆役单位时对失败目标追加[即死];[战斗胜利]时获12回转(演示)。");

    /* 流离魔剑·圣妃失坠A:初始工序,[主力位]对敌主力70%[即死];全部失败可重复宣言(演示) */
    r = db_np(w, "流离魔剑·圣妃失坠", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_A,
              KS_WHEN_PROC, 60, 9, KS_F_MAIN);
    {
        E e = E_DEATH(70, 1, 1);
        e.cond = KC_NONE;
        db_eff(w, r, e);
    }
    db_set_text(w, r, "[主力位]给予敌方主力位70%[即死]判定(幸运≥40减半);判定失败时可宣言追加:再次对敌主力70%[即死]并随后对自身70%[即死];若本次解放对目标全部失败可重复宣言(已成功过则对自身判定默认成功)。");

    /* 天鹅礼装A:常驻,始终[回避:技能];可赋予同灵脉单位[雏鸟礼装B] */
    r = db_np(w, "天鹅礼装", KS_NP_HUMAN, FC_DEFENSE, KS_RANK_A,
              KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_WIN(EF_EVADE, 0, -2));
    db_set_text(w, r, "允许当前灵脉持[雏鸟礼装B]的单位执行[机动]/[介入];此宝具生效时始终给予自身[回避:技能](其无效化A级/特殊等级技能时此宝具[封印1]);每回合限一次宣言,指定同灵脉任一单位获得[雏鸟礼装B](离开自身灵脉时失去)。");

    /* 铁锤蛇溃A:初始工序,目标属性-10并记[击溃1];层数达倍数时追加9次(演示) */
    r = db_np(w, "铁锤蛇溃", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 20, 3, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_STR, 10, 1));
    db_set_text(w, r, "初始工序:给予战斗位任一单位任一项属性-10并按层数记[击溃1];自身[击溃]层数变为2/3/4/5的倍数时,此宝具属性惩罚额外追加9次(至多-110);属性惩罚超出目标属性时,溢出部分变为等量胜率惩罚(演示)。");

    /* 源流斗争A:最终工序,[主力位][必中][无敌贯通]按宣言属性差给予敌军主力惩罚(演示) */
    r = db_np(w, "源流斗争", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_A,
              KS_WHEN_PROC, 60, 12, KS_F_MAIN | KS_F_PIERCE | KS_F_INV_PIERCE);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 60, 1));
    db_set_text(w, r, "[主力位][必中][无敌贯通]最终工序:宣言一项属性,给予敌方主力位[-(宣言属性差)%]胜率惩罚(双方战斗位仅主力时翻倍;至多-120%,演示60)。");

    /* 听吾之声，众位灼热的复仇之神啊B:回合开始时,下回合首场战斗开始双方全场灼伤4 */
    r = db_np(w, "听吾之声，众位灼热的复仇之神啊", KS_NP_ARMORY, FC_OFFENSE, KS_RANK_B,
              KS_WHEN_ANY, 50, 12, 0);
    db_eff(w, r, E_STATUS(S_BURN, 4, 0));
    {
        E e = E_WIN(EF_BURN_BLOW, 0, 0);
        db_eff(w, r, e);
    }
    db_set_text(w, r, "解放后至下个回合结束前,当前灵脉首场战斗开始时给予双方战斗位除自身外全部单位[灼伤4];该战斗最终工序开始再给全场[灼伤4]并令除自身外全部非[支援位]单位[爆燃];不持此宝具信息者额外[灼伤1];与[同灵脉袭击]同时宣言时灼伤层数翻倍(演示)。");

    /* 破灭之黎明A:常驻,主要工序开始时全场[灼伤1];决胜时按灼伤层数+胜率(演示) */
    r = db_np(w, "破灭之黎明", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_PASSIVE, 10, 0, 0);
    db_eff(w, r, E_STATUS(S_BURN, 1, 0));
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
    db_set_text(w, r, "自身受到[爆燃]时保留一半[灼伤]层数;主要工序开始时给予双方非[支援位]全体[灼伤1];决胜检定时按[战斗位灼伤合计层数*20%]给予自身胜率补正(至多+140%);战斗结束胜利时转移全部灼伤至自身(演示)。");

    /* 赤原猎犬A:初始工序,[主力位]随机属性+65补正并防止被更改 */
    r = db_np(w, "赤原猎犬", KS_NP_HUMAN, FC_BUFF, KS_RANK_A,
              KS_WHEN_PROC, 20, 9, KS_F_MAIN);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 65, -2));
    db_set_text(w, r, "[主力位]若自身[筋力]大于敌方主力位,本场战斗的[随机属性]无法被敌方更改,并给予自身[随机属性]+65属性补正(仅影响战斗属性的随机属性,演示敏捷)。");

    /* 嗜血兽斧C:常驻,三属性+30常驻;魂食/击杀御主后+15与等级上升(演示) */
    r = db_np(w, "嗜血兽斧", KS_NP_HUMAN, FC_BUFF, KS_RANK_C,
              KS_WHEN_PASSIVE, 15, 0, 0);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 30, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 30, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 30, -2));
    db_set_text(w, r, "给予自身[筋力][耐久][敏捷]+30常驻补正;每次[魂食]成功或击杀御主后补正永久+15且等级上升1级(魔耗+5;将超A时改为加符,至多4枚后停;3天未[魂食]或击杀御主则破弃)。");

    /* 星月夜A:战斗开始时,[主力位]100%诅咒判定后生成固有结界;按诅咒+胜率(演示) */
    r = db_np(w, "星月夜", KS_NP_HUMAN, FC_SPECIAL, KS_RANK_A,
              KS_WHEN_BATTLE_START, 60, 6, KS_F_MAIN);
    db_eff(w, r, E_STATUS(S_CURSE, 2, -2));
    db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
    db_set_text(w, r, "[主力位]解放时若处于[昼]回合回转永久+6;对自身100%[诅咒]判定:失败则获全部回转,成功给予[诅咒2]并生成[固有结界:星月夜](宽5/永夜/初始工序无法撤退);结界内自身每持2层[诅咒]+5%胜率(演示);每工序可支付40魔力按双方数值差获得属性补正。");

    /* 血染的加冕仪式B:战斗开始时,[主力位]给予自身[狂化C](已持有则+1级)并削减自身属性惩罚 */
    r = db_np(w, "血染的加冕仪式", KS_NP_HUMAN, FC_ANTITRAIT, KS_RANK_B,
              KS_WHEN_BATTLE_START, 60, 9, KS_F_MAIN);
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_STR, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_END, 20, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP, A_AGI, 20, -2));
    db_set_text(w, r, "[主力位]不会因[狂化]无法解放:解放后获得[狂化C](已持有则等级+1,不高于此宝具;触发建卡效果);敌方主力仅[人型]时自身[狂化]属性补正翻倍;记录来源于[对人]宝具的非[常驻]属性惩罚并削减自身全部属性惩罚(至多对应属性基础值,战斗结束转为常驻惩罚至下轮结束,演示狂化近似)。");

/* ================= 兼容原版名称完整实装(批次1) =================
 * 以下条目以 sim.c/demo 与建卡查找的"原名"创建,效果行按 PDF 原文拆解。
 * 旧 ksg_data.c 中同名"仅文本"条目将在后续批次删除。 */

/* ---- 职阶技能 ---- */

/* 对魔力A-D(职阶):负面判定-10%;魔术技能未生效时+10%胜率 */
r = db_skill(w, "对魔力", KS_T_CLASS, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_HIT_PEN; e.value = 10; e.target = -2;
    db_eff(w, r, e);
    db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
}
db_set_text(w, r, "令自身受到的、来源于技能和宝具的负面判定受到[-10%]的基础成功率惩罚;若[类型:魔术]技能未能对自身造成效果,给予自身[+10%]胜率补正;允许宣言令[类型:魔术]技能效果下降4/3/2/1/-级(演示为固定-10%惩罚+10%胜率)。");

/* 骑乘A-D(职阶):对[骑乘]效果持有者+胜率;冲锋允许 */
r = db_skill(w, "骑乘", KS_T_CLASS, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, KS_F_RIDE);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_WIN_UP; e.value = 30; e.target = -2;
    db_eff(w, r, e);
}
db_set_text(w, r, "自身受到己方战斗位上具有[骑乘]的技能宝具效果影响时,本场战斗内获得[+30%]胜率补正(无冲锋时额外+15%,演示取基础30%);允许自身进行[冲锋]([骑乘]特效)。");

/* ---- 保有技能 ---- */

/* 直感A-D(天赋):成为非自阵营效果对象时,令效果等级下降2级(以抗性+判定惩罚近似) */
r = db_skill(w, "直感", KS_T_TALENT, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_RES_UP; e.value = 20; e.target = -2;
    db_eff(w, r, e);
}
db_set_text(w, r, "每当自身成为不来源于自阵营和己方战斗位的技能或宝具的效果对象时,令其对自身即将造成的效果下降[2]级(每轮至多4次);以[抗性上升:+20%]近似表达。");

/* 心眼(伪)(天赋):每项劣势战斗属性+15%胜率;主力位+15%底限胜率 */
r = db_skill(w, "心眼(伪)", KS_T_TALENT, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, KS_F_MAIN);
db_eff(w, r, E_WIN(EF_WIN_UP, 15, -2));
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_FLOOR_UP; e.value = 15; e.target = -2;
    db_eff(w, r, e);
}
db_set_text(w, r, "战斗属性表每有一项劣势属性给予自身[+15%]胜率补正;主力位时给予自身[+15%]底限胜率(演示为常驻15%胜率+主力位底限15%)。");

/* 战斗续行(技艺):状态免疫残废;三属性+15常驻;即死豁免一次(以底限+免疫近似) */
r = db_skill(w, "战斗续行", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
{
    E e = E_STATUS(S_STATE_IM, S_CRIPPLED, -2); e.chance_attr_base = -1;
    db_eff(w, r, e);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 15, -2));
}
db_set_text(w, r, "给予自身[状态免疫:残废];[筋力][耐久][敏捷]+15常驻补正;受到[即死]效果未支付令咒时,允许以三属性-30常驻惩罚豁免退场(演示为常驻免疫+三属性+15)。");

/* 领袖气质(天赋):己方战斗位其他单位+20%胜率(仆役至多10%);主力位自身亦受用 */
r = db_skill(w, "领袖气质", KS_T_TALENT, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
db_eff(w, r, E_WIN(EF_WIN_UP, 20, -1));
db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
db_set_text(w, r, "始终给予己方战斗位除自身外全部单位[+20%]胜率补正(仆役与不同阵营至多+10%);主力位时自身同样受到+20%。");

/* 魔力放出(技艺):随时发动,三属性之一+15常驻补正与+5%胜率 */
r = db_skill(w, "魔力放出", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_ANY, 10, 1, 0);
db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 15, -2));
db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
db_set_text(w, r, "发动时给予[筋力][耐久][敏捷]中一项[+15]常驻补正(回合结束失去)与[+5%]胜率补正。EX:可额外支付40魔力给予敌方主力位[-15%]胜率惩罚(演示取B级)。");

/* ---- 宝具 ---- */

/* 誓约胜利之剑A-D(决战/对城):蓄力,最终工序+80%胜率;令咒轰击+80% */
r = db_np(w, "誓约胜利之剑", KS_NP_CASTLE, FC_DECISIVE, KS_RANK_A,
          KS_WHEN_PROC, 80, 9, KS_F_MAIN | KS_F_BURST_READY);
db_eff(w, r, E_WIN(EF_WIN_UP, 80, -2));
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_WIN_UP; e.value = 80; e.target = -2; e.cond = KC_HAS_CS;
    db_eff(w, r, e);
    E e1069 = E_STATUS(S_CHARGE, 2, -2); e1069.flag = EF_CHARGE; db_eff(w, r, e1069);
}
db_set_text(w, r, "[主力位][蓄力]最终工序时给予自身[+本次解放魔耗%]胜率补正;允许自阵营御主追加宣言消耗令咒进行[轰击]判定(成功再+80%,演示以有无令咒作为条件)。");

/* 刺穿死棘之枪A-E(即死/对人):50%即死,目标幸运≥40成功率减半 */
r = db_np(w, "刺穿死棘之枪", KS_NP_HUMAN, FC_INSTAKILL, KS_RANK_B,
          KS_WHEN_PROC, 50, 3, KS_F_MAIN | KS_F_PIERCE);
db_eff(w, r, E_DEATH(50, 0, 1));
db_set_text(w, r, "[主力位][必中]对敌方战斗位非支援位任一单位进行一次成功率=[50%]的[即死]判定;目标[幸运]≥40时成功率减半;成功且不消耗令咒则立即退场。");

/* 遗世独立的理想乡(防御/结界):反击支援,直到战斗结束[无敌];战斗结束回魔 */
r = db_np(w, "遗世独立的理想乡", KS_NP_BOUND, FC_DEFENSE, KS_RANK_A,
          KS_WHEN_ANY, 100, 12, KS_F_COUNTER | KS_F_ASSIST);
{
    E e = E_STATUS(S_INVINCIBLE, 1, -2); e.chance_attr_base = -1;
    db_eff(w, r, e);
    db_eff(w, r, E_MANA(EF_MANA_UP, 100, -2));
}
db_set_text(w, r, "[反击][支援]仅在成为等级不高于自身的任一宝具效果对象时解放;直到战斗结束给予自身[无敌];战斗结束时获得魔力补给直至魔力池为[0]。");

/* 炽天覆七重圆环(防御/结界):反击支援,记录负面效果并逐次判定无效化(以抗性+回避近似) */
r = db_np(w, "炽天覆七重圆环", KS_NP_BOUND, FC_DEFENSE, KS_RANK_B,
          KS_WHEN_ANY, 80, 15, KS_F_COUNTER | KS_F_ASSIST);
{
    E e = E_STATUS(S_RESUP, 40, -2); e.chance_attr_base = -1;
    db_eff(w, r, e);
    E e1070 = E_STATUS(S_EVADE, 1, -2); e1070.chance_attr_base = -1;; db_eff(w, r, e1070);
}
db_set_text(w, r, "[反击][支援]任一宝具解放时可解放,记录属性惩罚/胜率惩罚等效果并逐次判定无效化(演示以[抗性上升:+40%]与[回避]近似);至多进行6/5/4/3/2次判定。");


/* ---- 批次1D:御主/礼装 同名完整版 ---- */

/* 宝石魔术B-D(魔术/最终工序):[支援]摧毁魔力水晶/魔弹宝石,敌方主力位大额胜率惩罚 */
r = db_skill(w, "宝石魔术", KS_T_MAGIC, KS_RANK_B, KS_WHEN_PROC, 20, 6, KS_F_ASSIST);
db_eff(w, r, E_WIN(EF_WIN_DOWN, 160, 1));
db_set_text(w, r, "[支援]最终工序:摧毁[15/12/9/6/3]个[魔力水晶]或[魔弹宝石]发动,给予敌方[主力位]以[-160%]胜率惩罚(演示取B级160,EX时+30%)。");

/* 超越回路B(魔术/常驻):轮次结束非游荡时+25魔力供给(工房存在时翻倍) */
r = db_skill(w, "超越回路", KS_T_MAGIC, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_MANA_UP; e.value = 25; e.target = -2;
    e.cond = KC_MP_UNDER;  /* 利用MP<0条件的反向:非游荡简化再加;此处直接抚平 */
    db_eff(w, r, e);
}
db_set_text(w, r, "轮次结束时,若自身不处于游荡状态,获得[+25]魔力供给(所处灵脉存在归属于自身的[魔术工房]时翻倍);溢出魔力结算后令下次[类型:魔术]技能效果等级上升1级(演示为常驻+25魔力供给)。");

/* 调律魔术C(魔术/随时):[支援位]指定同灵脉单位:魔耗-10 / 魔术技能优先发动 / 供给魔力 */
r = db_skill(w, "调律魔术", KS_T_MAGIC, KS_RANK_C, KS_WHEN_ANY, 0, 0, KS_F_REAR);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_MANA_UP; e.value = 10; e.target = -2;
    db_eff(w, r, e);
}
db_set_text(w, r, "[支援位]随时发动:指定同灵脉单位——令其下一次自身产生的魔力消耗-10 / 令其下一个[类型:魔术]技能先于等级不高于此技能的技能发动 / 回合结束时获得[10]魔力供给(演示取第三种)。");

/* 咒符(基础礼装):[反击][支援]同灵脉单位发动负面判定时,基础成功率+10% */
r = def_item_compat(w, "咒符", KS_RANK_C, KS_WHEN_ANY, 0, 0, KS_F_COUNTER | KS_F_ASSIST, 1, 0, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_NONE; e.target = -2;   /* 占位(礼装本身含成功率修正,以文本呈现) */
    e.desc = "同灵脉任一单位发动负面判定时可发动,令判定基础成功率获得[+10%]的补正";
    db_eff(w, r, e);
}
db_set_text(w, r, "[反击][支援]同灵脉任一单位发动负面判定时可以发动,令判定基础成功率获得[+10%]的补正(对同一负面判定不可叠加)。");

/* 魔力水晶(基础礼装):[支援]自阵营/同阵营单位+10魔力补充 */
r = def_item_compat(w, "魔力水晶", KS_RANK_C, KS_WHEN_ANY, 0, 0, KS_F_ASSIST, 5, 0, 0);
db_eff(w, r, E_MANA(EF_MANA_UP, 10, -1));
db_set_text(w, r, "[支援]给予自阵营或同阵营任一单位[+10]的魔力补充(每轮5次)。");

/* 黑键(概念武装):[支援]消耗1储备:敌方非支援位-5%胜率;对[魔性]单位额外-10% */
r = def_item_compat(w, "黑键", KS_RANK_C, KS_WHEN_ANY, 0, 0, KS_F_ASSIST, 1, 1, 0);
db_eff(w, r, E_WIN(EF_WIN_DOWN, 5, 1));
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_WIN_DOWN; e.value = 10; e.target = 1;
    e.cond = KC_TARGET_TRAIT; e.cond_arg = TR_DEMONIC;
    db_eff(w, r, e);
}
db_set_text(w, r, "[储备1/1][支援]消耗[储备1]给予敌方战斗位非[支援位]任一单位[-5%]胜率惩罚;对持有[魔性]特性的单位额外[-10%]。");


/* ============ 批次2A:职阶技能 同名完整版 ============ */

/* 单独行动A(Archer职阶):侦查+40%;魔力池下限-100;无契约时仍存活 */
r = db_skill(w, "单独行动", KS_T_CLASS, KS_RANK_A, KS_WHEN_PASSIVE, 0, 0, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_HIT_UP; e.value = 40; e.target = -2;
    db_eff(w, r, e);
    E e2; memset(&e2, 0, sizeof(e2));
    e2.chance_attr_base = -1;
    e2.flag = EF_FINAL_WIN_UP; e2.value = 5; e2.target = -2;
    db_eff(w, r, e2);
}
db_set_text(w, r, "自身发起的[侦查]判定获得[+40%]成功率补正,定向侦查不暴露自身所处灵脉;魔力池下限额外-100,魔力不足属性惩罚减半;御主死亡无契约时不强制退场(演示为侦查+40%与+5%最终胜率)。");

/* 阵地制作C(魔术师/从者职阶):行动阶段发动,赋予灵脉[阵地][结阵1];可工房建造 */
r = db_skill(w, "阵地制作", KS_T_CLASS, KS_RANK_C, KS_WHEN_ACT, 20, 3, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_ATTR_UP_CONST; e.attr = A_MAG; e.value = 10; e.target = -2;
    db_eff(w, r, e);
}
db_set_text(w, r, "消耗行动阶段发动,赋予当前灵脉[阵地][结阵1](额外魔力池+20/结阵,演示为自身[魔力]+10常驻近似);D级起可不消耗组件进行[工房建造],A级可建设[神殿]。");

/* 道具制作C:建卡记录礼装;允许[礼装制作];礼装次数+9 */
r = db_skill(w, "道具制作", KS_T_CLASS, KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, 0);
db_eff(w, r, E_MANA(EF_MANA_UP, 10, -2));
db_set_text(w, r, "建卡时记录辅助/限定礼装;允许进行[礼装制作](仅基础礼装与记录礼装);每轮次[发动礼装次数上限]+9(演示:每轮+10魔力近似制作辅助)。");

/* 气息遮蔽A(Assassin职阶):广泛侦查不暴露;定向侦查不暴露;胜率补正 */


/* 心眼(真)B(技艺):[主力位]初始工序持宝具情报时+15%底限;主要工序可重选战术 */
r = db_skill(w, "心眼(真)", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_ANY, 0, 5, KS_F_MAIN);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_FLOOR_UP; e.value = 15; e.target = -2;
    db_eff(w, r, e);
}
db_set_text(w, r, "[主力位]初始工序发动:若持有敌方主力位[宝具情报],给予自身[+15%]底限胜率;主要工序发动:重新选择己方[战术](演示:发动时+15%底限)。");

/* 千里眼B(天赋):侦查/情报调查+20%成功率 */
r = db_skill(w, "千里眼", KS_T_TALENT, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_HIT_UP; e.value = 20; e.target = -2;
    db_eff(w, r, e);
}
db_set_text(w, r, "自身发起的[侦查]判定和[情报调查]判定获得[+20%]基础成功率补正(演示为判定成功率+20);不必处于战斗灵脉也能知悉其他灵脉[战斗计算表]内容。");

/* 黄金律B(祝福):每回合开始40%判定获得随机礼装(演示:40%判定+30魔力) */
r = db_skill(w, "黄金律", KS_T_BLESS, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_MANA_UP; e.value = 30; e.target = -2; e.chance = 40;
    db_eff(w, r, e);
}
db_set_text(w, r, "每回合开始进行一次40%判定,成功时获得随机礼装(演示:成功时+30魔力作为礼装近似);[礼装制作]判定属性可改为[幸运]。");

/* 无穷的武练A(荣冠):三属性+25常驻;状态抵抗混乱恐惧魅惑 */
r = db_skill(w, "无穷的武练", KS_T_CROWN, KS_RANK_A, KS_WHEN_PASSIVE, 10, 0, 0);
{
    E e = E_STATUS(S_STATE_IM, S_CONFUSE, -2); e.chance_attr_base = -1;
    db_eff(w, r, e);
    E e1071 = E_STATUS(S_STATE_IM, S_FEAR, -2); e1071.chance_attr_base = -1;; db_eff(w, r, e1071);
    E e1072 = E_STATUS(S_STATE_IM, S_CHARM, -2); e1072.chance_attr_base = -1;; db_eff(w, r, e1072);
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 25, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 25, -2));
}
db_set_text(w, r, "给予自身[筋力][耐久][敏捷]三项属性[+25]常驻补正,[状态抵抗:混乱&恐惧&魅惑](演示以状态免疫表达);B级起免疫战斗内[魔力不足]退场。");


/* ============ 批次2C:宝具 同名完整版 ============ */

/* 王之军势A(决战/对军):生成固有结界+召唤等级=自身等级的召唤物5体;召唤物在场时敌方主力-等级/2%胜率 */
r = db_np(w, "王之军势", KS_NP_ARMORY, FC_DECISIVE, KS_RANK_A,
          KS_WHEN_ANY, 30, 18, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_SUMMON; e.value = 40; e.cond_arg = 100; e.cond_arg2 = TR_HUMAN;
    e.target = -2;
    e.desc = "王之军势:召唤等级40、总属性100的召唤物至己方战斗位(演示1体)";
    db_eff(w, r, e);
    db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
}
db_set_text(w, r, "[交流中]指定目标宣言袭击并生成[固有结界:灼砂大地](宽度7/永昼);召唤5/4/3体等级=[自身等级]的召唤物至己方战斗位;召唤物存在于战斗位时给予敌方主力位[-召唤物等级/2%]胜率惩罚(演示:召唤1体+自身+20%胜率)。");

/* 无限剑制A(决战/结界):持有投影魔术时战斗开始生成固有结界;敌方-25%胜率近似 */
r = db_np(w, "无限剑制", KS_NP_BOUND, FC_DECISIVE, KS_RANK_A,
          KS_WHEN_BATTLE_START, 80, 18, 0);
db_eff(w, r, E_WIN(EF_WIN_DOWN, 25, 0));
db_set_text(w, r, "建卡时按[投影魔术]等级-40魔力池下限;仅持有[投影魔术]时生效;初始工序生成[固有结界:无限剑制](宽度7/永昼);双方初始工序无法撤退(演示:战斗开始时敌方-25%胜率)。");

/* 秘剑·燕返(魔剑/对人):3次敏捷/2%即死级负面判定,3次全中即死 */
r = db_np(w, "秘剑·燕返", KS_NP_HUMAN, FC_SWORD, KS_RANK_A,
          KS_WHEN_ANY, 10, 3, KS_F_MAIN);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_DEATH; e.value = 60; e.target = 1; e.luck_halve = 0;
    e.chance_attr_base = A_AGI; e.chance = 0;   /* 由敏捷决定,简化为60% */
    e.chance = 60;
    db_eff(w, r, e);
    E e1073 = E_ATTR(EF_ATTR_DOWN, A_END, 30, 1); e1073.chance_attr_base = -1;; db_eff(w, r, e1073);
    db_eff(w, r, E_STATUS(S_LAG, 1, -2));
}
db_set_text(w, r, "[主力位]解放后给予自身[迟滞1];令敌方主力位受到[3]次成功率=[敏捷/2%]的负面判定(目标敏捷≥40减半;演示固定60%),每次成功给予三属性之一[-30]属性惩罚;3次全部成功时追加[即死]效果(演示:即死判定60%+耐久-30)。");

/* 无明三段突(魔剑/对人):3次80%负面判定(视为即死),3次成功即死 */
r = db_np(w, "无明三段突", KS_NP_HUMAN, FC_SWORD, KS_RANK_A,
          KS_WHEN_ANY, 10, 3, KS_F_MAIN);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_DEATH; e.chance = 80; e.value = 80; e.target = 1;
    e.times = 3; e.luck_halve = 0;
    db_eff(w, r, e);
}
db_set_text(w, r, "[主力位]令敌方主力位受到三次80%的负面判定(视为即死,目标敏捷≥40减半);每有一项成功给予三属性之一[-20]属性惩罚;判定成功数达3时追加[即死],不消耗令咒即退场(演示:3次80%即死判定)。");

/* 拔刀·神威(魔剑/对人):按工序不同:[初始]60%即死/[主要]80%→属性-60/[最终]160%判定属性惩罚 */
r = db_np(w, "拔刀·神威", KS_NP_HUMAN, FC_SWORD, KS_RANK_A,
          KS_WHEN_ANY, 10, 3, KS_F_MAIN);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_DEATH; e.value = 60; e.target = 1; e.chance = 60;
    db_eff(w, r, e);
    E e1074 = E_ATTR(EF_ATTR_DOWN, A_END, 60, 1); e1074.chance_attr_base = -1; e1074.chance = 80;; db_eff(w, r, e1074);
}
db_set_text(w, r, "[主力位][初始工序]对敌方主力位60%即死判定(敏捷≥40减半);[主要工序]80%负面判定成功给予宣言属性[-60]惩罚;[最终工序]160%判定给予[溢出成功率]属性惩罚(演示:60%即死+80%耐久-60)。");

/* 流星一条A(决战/对军):自身魔力耗至下限,敌方[魔耗/2%]胜率惩罚 */
r = db_np(w, "流星一条", KS_NP_ARMORY, FC_DECISIVE, KS_RANK_A,
          KS_WHEN_PROC, 30, 18, KS_F_MAIN);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_MANA_DOWN; e.value = 60; e.target = -2;
    db_eff(w, r, e);
    db_eff(w, r, E_WIN(EF_WIN_DOWN, 30, 0));
    E e1075 = E_STATUS(S_CHARGE, 2, -2); e1075.flag = EF_CHARGE; db_eff(w, r, e1075);
}
db_set_text(w, r, "[主力位][蓄力]最终工序摧毁固有结界并令自身魔力耗至下限,给予敌方全体[-魔耗/2%]胜率惩罚;追加令咒可[轰击](演示:自身-60魔力+敌方-30%胜率)。");

/* 终结剑B(决战/对界):战斗开始蓄力,激流成长;最终工序同灵脉其他单位-45%胜率+毁工房 */
r = db_np(w, "终结剑", KS_NP_WORLD, FC_DECISIVE, KS_RANK_B,
          KS_WHEN_BATTLE_START, 90, 9, KS_F_MAIN);
db_eff(w, r, E_WIN(EF_WIN_DOWN, 45, 0));
    E e1076 = E_STATUS(S_CHARGE, 2, -2); e1076.flag = EF_CHARGE; db_eff(w, r, e1076);
db_set_text(w, r, "[主力位][视为对界]战斗开始宣言解放并[蓄力](不可打断);[激流]随轮次成长(至多7层);最终工序若激流≥3,令本灵脉人流量归零并给予同灵脉其他所有单位[-45%]胜率惩罚且摧毁工房/结界(演示:敌方-45%胜率)。");

/* 闪耀于终焉之枪B(决战/对城):蓄力,最终工序敌方非支援位[魔耗/2%]惩罚 */
r = db_np(w, "闪耀于终焉之枪", KS_NP_CASTLE, FC_DECISIVE, KS_RANK_B,
          KS_WHEN_PROC, 80, 9, KS_F_INV_PIERCE);
db_eff(w, r, E_WIN(EF_WIN_DOWN, 40, 0));
    E e1077 = E_STATUS(S_CHARGE, 2, -2); e1077.flag = EF_CHARGE; db_eff(w, r, e1077);
db_set_text(w, r, "[无敌贯通][蓄力]最终工序给予敌方战斗位非支援位全部单位[-魔耗/2%]胜率惩罚(演示-40%);追加令咒可[轰击](+30%);两枚加符时可在知晓战斗的灵脉远程解放。");

/* 鹤翼三连B(进攻/对人):干将莫邪获得必中并立即解放2次 */
r = db_np(w, "鹤翼三连", KS_NP_HUMAN, FC_OFFENSE, KS_RANK_B,
          KS_WHEN_ANY, 20, 9, 0);
db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
db_set_text(w, r, "[与干将·莫邪一同购入]解放时赋予[干将·莫邪][必中]并立即解放2次(每次-20%胜率惩罚,回转延后至战斗结束);自身在辅助位时可与己方主力位交换位置(演示:2次-20%胜率)。");


/* ============ 批次2D:剩余技能 同名完整版 ============ */

/* 兽化B(职阶):破除狂化获得特性猛兽/魔兽;全属性+25常驻 */
r = db_skill(w, "兽化", KS_T_CLASS, KS_RANK_B, KS_WHEN_PASSIVE, 15, 0, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_ATTR_UP_CONST; e.attr = A_STR; e.value = 25; e.target = -2;
    db_eff(w, r, e);
    E e2; memset(&e2, 0, sizeof(e2));
    e2.chance_attr_base = -1;
    e2.flag = EF_ATTR_UP_CONST; e2.attr = A_END; e2.value = 25; e2.target = -2;
    db_eff(w, r, e2);
    E e3; memset(&e3, 0, sizeof(e3));
    e3.chance_attr_base = -1;
    e3.flag = EF_ATTR_UP_CONST; e3.attr = A_AGI; e3.value = 25; e3.target = -2;
    db_eff(w, r, e3);
}
db_set_text(w, r, "建卡时破除[狂化]+1RP;持有此技能时始终持有[猛兽/魔兽]特性;给予自身除宝具外全属性[+25]常驻补正(演示:三属性+25)。");

/* 缩地B(技艺):[主力位]指定敌方取消[蓄力];[反击]被指定时回避技能 */
r = db_skill(w, "缩地", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_ANY, 0, 3, KS_F_MAIN);
db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
db_set_text(w, r, "[主力位]指定敌方战斗位任一单位,发起[敏捷差/2%]判定,成功取消其[蓄力];[反击]成为技能对象时判定成功给予自身[回避:技能](演示:发动时+5%胜率)。");

/* 中国武术A(技艺):三属性+15常驻;发动时记录属性+10 */
r = db_skill(w, "中国武术", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_PASSIVE, 0, 3, 0);
db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 15, -2));
db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 15, -2));
db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 15, -2));
db_set_text(w, r, "建卡时记录[筋力][耐久][敏捷]任两项;给予三属性[+15]常驻补正;发动时记录属性[+10]属性补正(加符时战斗属性记录属性+10且优势翻倍)。");

/* 混血B(职阶):记录神性/魔性/龙种一项,获得特性与对应效果 */
r = db_skill(w, "混血", KS_T_CLASS, KS_RANK_B, KS_WHEN_PASSIVE, 10, 0, 0);
db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 25, -2));
db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 25, -2));
db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 25, -2));
db_set_text(w, r, "建卡时记录[神性/魔性/龙种]其中一项并获得特性;[龙种]:三属性+25常驻+抗性+10%;[神性]:抗性+30%+判定+10%;[魔性]:负面判定+20%+轮次结束魔供(演示:三属性+25)。");

/* 机关制作B(职阶):破除道具制作+1RP;召唤[武者人偶](演示:召唤构装体召唤物) */
r = db_skill(w, "机关制作", KS_T_CLASS, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_SUMMON; e.value = 40; e.cond_arg = 50; e.cond_arg2 = TR_GOLEM;
    e.target = -2;
    e.desc = "机关制作:召唤[武者人偶](构装体)";
    db_eff(w, r, e);
}
db_set_text(w, r, "建卡时破除[道具制作]+1RP;从属性记录10项;召唤等级等同自身的[武者人偶](构装体特性,演示:召唤等级40构装体);可消耗行动+20魔力进行[机关改造]属性+10。");

/* 鬼神之显B(职阶):神性+魔性特性;三属性+25常驻;抗性+15% */
r = db_skill(w, "鬼神之显", KS_T_CLASS, KS_RANK_B, KS_WHEN_PASSIVE, 15, 0, 0);
db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 25, -2));
db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 25, -2));
db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_MAG, 25, -2));
db_set_text(w, r, "始终持有[神性][魔性]特性;[筋力][耐久][魔力]+25常驻与[抗性上升:+15%](演示:三属性+25)。");

/* 投射魔术C(魔术):负面判定[魔力*2-敏捷]%,成功给-30%胜率惩罚 */
r = db_skill(w, "投射魔术", KS_T_MAGIC, KS_RANK_C, KS_WHEN_ANY, 20, 3, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_DEATH; e.value = 50; e.target = 1;
    e.chance_attr_base = A_MAG; e.chance = 50; e.chance_neg = 1;
    db_eff(w, r, e);
    E e1078 = E_STATUS(S_CHARGE, 2, -2); e1078.flag = EF_CHARGE; db_eff(w, r, e1078);
    E e1079 = E_STATUS(S_CHARGE, 2, -2); e1079.flag = EF_CHARGE; db_eff(w, r, e1079);
}
db_set_text(w, r, "指定敌方非支援位单位,发起[魔力*2-敏捷%]负面判定,成功给予[-30%]胜率惩罚(目标魔力更高则默认失败);战斗开始时可选[蓄力]在任意工序结束宣言造成胜率惩罚(演示:50%即死级判定+胜率惩罚)。");

/* 机巧军势B(魔术/常驻):允许[人偶制作],产生人偶单位(演示:召唤构装体) */
r = db_skill(w, "机巧军势", KS_T_MAGIC, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_SUMMON; e.value = 10; e.cond_arg = 20; e.cond_arg2 = TR_GOLEM;
    e.target = -2;
    e.desc = "机巧军势:召唤[战斗人偶](构装体)";
    db_eff(w, r, e);
}
db_set_text(w, r, "允许[人偶制作]:消耗行动阶段宣言[战斗/后勤/魔力人偶],以[魔力*2%]成功判定产生人偶(演示:召唤等级10构装体);至多同时存在3个。");

/* 七重守护B(魔术/行动阶段):赋予[魔术结界:七重守护][结阵1](演示:自身抗性+15%) */
r = db_skill(w, "七重守护", KS_T_MAGIC, KS_RANK_B, KS_WHEN_ACT, 20, 0, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_RES_UP; e.value = 15; e.target = -2;
    db_eff(w, r, e);
}
db_set_text(w, r, "行动阶段发动:赋予当前灵脉[魔术结界:七重守护][结阵1](至多7);结界内来自灵脉外等级不高于此技能的效果无效化(消耗结阵层数,演示:自身抗性+15%)。");

/* 神经衰弱B(魔术/回合开始):诅咒判定成功给诅咒3(目标至多6层) */
r = db_skill(w, "神经衰弱", KS_T_MAGIC, KS_RANK_B, KS_WHEN_ANY, 20, 6, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_STATUS_GIVE; e.status = S_CURSE; e.layers = 3; e.target = 0;
    e.chance = 100; e.chance_neg = 1;
    db_eff(w, r, e);
}
db_set_text(w, r, "回合开始:对持有[情报调查]信息的御主发起[100%]诅咒判定,成功给予[诅咒3](至多6层);[生祭]:诅咒单位行动判定-20%(演示:100%诅咒3)。");

/* 强筋锻骨B(天赋/常驻):抗性+15%;三属性+5常驻;耐久≥40翻倍 */
r = db_skill(w, "强筋锻骨", KS_T_TALENT, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_STR, 5, -2));
db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_END, 5, -2));
db_eff(w, r, E_ATTR(EF_ATTR_UP_CONST, A_AGI, 5, -2));
db_set_text(w, r, "给予[抗性上升:+15%]与[筋力][耐久][敏捷]+5常驻补正;自身耐久≥40时抗性翻倍(+30%,演示:三属性+5)。");

/* 毒蛇一艺B(技艺):对首次遭遇的主力位负面判定+30%;主要工序敏捷差判定→耐久-30 */
r = db_skill(w, "毒蛇一艺", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_PROC, 0, 0, KS_F_MAIN);
db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 30, 1));
db_set_text(w, r, "仅非支援位生效;与敌方主力位初次同场战斗时其负面判定+30%最终成功率;主要工序开始对目标[20+敏捷差%]判定,成功给其[耐久-30]属性惩罚(演示:耐久-30+自身胜利+10%)。");

/* 无限切C(技艺):给予敌方主力位-10%胜率并重选[剑势] */
r = db_skill(w, "无限切", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_ANY, 10, 1, 0);
db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 1));
db_set_text(w, r, "给予敌方战斗位任一单位[-10%]胜率惩罚,并清除/重选[剑势];不能于最终工序发动;初始工序开始时此技能立刻获得1回转(演示:敌方-10%胜率)。");


/* ============ 批次3A:御主技能 同名完整版 ============ */

/* 原初的卢恩B(魔术):记录卢恩符文效果,发动时按效果(演示:不同效果以胜率/属性表达) */
r = db_skill(w, "原初的卢恩", KS_T_MAGIC, KS_RANK_B, KS_WHEN_ANY, 20, 3, 0);
db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
db_set_text(w, r, "记录16种卢恩符文效果;发动时宣言对应效果(演示:发动时+10%胜率;按符文类别可转换为属性/状态/魔力等效果)。");

/* 投射魔术(御主/魔术):负面判定[魔力*2-敏捷]%,成功-30%胜率(第二条定义,与从者版区分) */
r = db_skill(w, "投射魔术", KS_T_MAGIC, KS_RANK_C, KS_WHEN_ANY, 20, 3, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_DEATH; e.value = 50; e.target = 1;
    e.chance_attr_base = A_MAG; e.chance = 50; e.chance_neg = 1;
    db_eff(w, r, e);
}
db_set_text(w, r, "指定敌方非支援位单位发动,发起[魔力*2-敏捷%]负面判定,成功给予[-30%]胜率惩罚(目标魔力更高则默认失败);战斗开始时可选[蓄力](演示:50%判定)。");

/* 潜能激发B(天赋):75%判定令一项技能获全部回转,失败给疲惫1+回合结束再疲惫1 */


/* 继理血戒B(荣冠):允许恶性/无限制魂食;建卡选择剑/城/森林/热能等效果(演示:自身+15%胜率) */
r = db_skill(w, "继理血戒", KS_T_CROWN, KS_RANK_B, KS_WHEN_BATTLE_START, 20, 6, 0);
db_eff(w, r, E_WIN(EF_WIN_UP, 15, -2));
    db_eff(w, r, E_ATTR(EF_ATTR_DOWN, A_END, 20, 1));
db_set_text(w, r, "允许进行[恶性魂食][无限制魂食];建卡时从[剑][城][森林][热能操纵][兽王之巢][蔷薇之魔眼]选择一项效果(演示:战斗开始时+15%胜率;[剑]对主力位耐久-20+抗性下降)。");

/* 祝福秘迹B(祝福):[支援]指定单位下一次发动魔耗-40并给予抗性+20% */
r = db_skill(w, "祝福秘迹", KS_T_BLESS, KS_RANK_B, KS_WHEN_ANY, 0, 9, KS_F_ASSIST);
db_eff(w, r, E_MANA(EF_MANA_UP, 40, -1));
    {
        E e2; memset(&e2, 0, sizeof(e2));
        e2.chance_attr_base = -1;
        e2.flag = EF_RES_UP; e2.value = 20; e2.target = -1;
        db_eff(w, r, e2);
    }
    db_set_text(w, r, "[支援]指定战斗位任一单位:令其下一次发动技能/解放宝具产生的魔力消耗下降[-40],并给予其[抗性上升:+20%](目标持[魔性]时抗性无效;EX获[反击])。");

/* 资料调取B(技艺):降临获取情报;敌方战斗位每名被调查单位+20%胜率 */
r = db_skill(w, "资料调取", KS_T_TECHNIQUE, KS_RANK_B, KS_WHEN_PASSIVE, 0, 0, 0);
db_eff(w, r, E_WIN(EF_WIN_UP, 20, -2));
db_set_text(w, r, "降临时获悉随机5名御主外貌信息并立即进行5次不消耗行动的50%[情报调查];敌方战斗位每存在一名自身持有[情报调查]信息的单位,给予自身[+20%]胜率补正(演示:常驻+20%胜率)。");

/* 火力支援B(兵器):[支援]战斗开始时指定一方,主要工序开始时-20%胜率与人流量-1 */
r = db_skill(w, "火力支援", KS_T_WEAPON, KS_RANK_B, KS_WHEN_ANY, 0, 12, KS_F_ASSIST);
db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 0));
db_set_text(w, r, "[支援]任一已知战斗开始时,指定其中一方战斗位;主要工序开始时给予指定方全部单位[-20%]胜率惩罚,并导致目标灵脉人流量[-1](演示:敌方-20%胜率)。");

/* 金钱的力量A(技艺):消耗资金获取礼装/工房建造(演示:获得魔力近似资金) */
r = db_skill(w, "金钱的力量", KS_T_TECHNIQUE, KS_RANK_A, KS_WHEN_ANY, 0, 0, 0);
db_eff(w, r, E_MANA(EF_MANA_UP, 20, -2));
db_set_text(w, r, "消耗[资金]获取非职业礼装/科技造物(下轮次结束获得),或消耗[资金2]对当前灵脉进行不消耗行动的[工房建造](演示:发动时+20魔力近似资金收益)。");


/* ============ 批次3B:基础礼装/科技造物/黑键 同名完整版 ============ */

/* 替身札纸(基础礼装):[反击][支援]负面判定时-10%基础成功率 */
r = def_item_compat(w, "替身札纸", KS_RANK_C, KS_WHEN_ANY, 0, 0, KS_F_COUNTER | KS_F_ASSIST, 1, 0, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_HIT_PEN; e.value = 10; e.target = -1;
    e.desc = "同灵脉任一单位遭到负面判定时可以发动,令判定基础成功率受到-10%的惩罚";
    db_eff(w, r, e);
}
db_set_text(w, r, "[反击][支援]同灵脉任一单位遭到负面判定时可以发动,令判定基础成功率受到[-10%]的惩罚(对同一负面判定不可叠加)。");

/* 契约之书(基础礼装):宣言进行契约 */
r = def_item_compat(w, "契约之书", KS_RANK_C, KS_WHEN_ANY, 0, 0, 0, 1, 0, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_CS; e.value = 1; e.target = -2;
    e.desc = "行动阶段或交流中宣言,进行[圣杯/同盟/不战/魔力/强制/奴役/决斗]契约";
    db_eff(w, r, e);
}
db_set_text(w, r, "行动阶段或交流中进行宣言,可进行[圣杯契约][同盟契约][不战契约][魔力契约][强制契约][奴役契约][决斗契约]任一(演示:令咒+1近似契约效力)。");

/* 侦查使魔(基础礼装):指定灵脉干涉/介入战斗知悉战况(演示:+5%胜率近似情报) */
r = def_item_compat(w, "侦查使魔", KS_RANK_C, KS_WHEN_ANY, 0, 3, 0, 1, 0, 0);
db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
db_set_text(w, r, "指定任一灵脉发起[干涉](无法交流);任意战斗发生时介入目标灵脉获知双方战斗情况(含属性表/胜率/技能宝具;演示:生效时+5%胜率近似情报优势)。");

/* 传讯使魔(基础礼装):指定灵脉干涉-交流 */
r = def_item_compat(w, "传讯使魔", KS_RANK_C, KS_WHEN_ANY, 0, 3, 0, 1, 0, 0);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_INFO; e.value = 0; e.target = -2;
    e.desc = "指定灵脉发起[干涉-交流]";
    db_eff(w, r, e);
}
db_set_text(w, r, "指定任一灵脉,使[传讯使魔]对其发起一次[干涉-交流];目标灵脉战斗时若未停止干涉则其被破坏(演示:给予情报信息)。");

/* 建筑使魔(基础礼装):回合结束50%判定视为一次工房建造 */
r = def_item_compat(w, "建筑使魔", KS_RANK_C, KS_WHEN_ANY, 0, 3, 0, 1, 0, 0);
db_eff(w, r, E_WIN(EF_WIN_UP, 0, -2));
db_set_text(w, r, "回合结束时进行50%判定,成功视为一次[工房建造](该判定不受持有者成功率补正;复数持有额外+25%成功率)。");

/* 水银剑AZOTH剑(辅助礼装):独立魔力池20;**即死判定不实现**,以供给+惩罚近似 */
r = def_item_compat(w, "水银剑(AZOTH剑)", KS_RANK_C, KS_WHEN_ANY, 0, 6, KS_F_UNIQUE | KS_F_ASSIST, 1, 0, 1);
db_eff(w, r, E_MANA(EF_MANA_UP, 20, -2));
db_set_text(w, r, "[唯一]持有独立额外魔力池(上限20);储存满后可对同灵脉御主进行20%[即死]判定(敏捷≥20减半;演示:持有即+20魔力)。");

/* 宝石吊坠(辅助礼装):额外魔力池上限50,初始60 */
r = def_item_compat(w, "宝石吊坠", KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, KS_F_UNIQUE, 1, 0, 1);
db_eff(w, r, E_MANA(EF_MANA_UP, 60, -2));
db_set_text(w, r, "[唯一]持有额外魔力池(上限50),建卡时其魔力变为60(溢出部分演示为+60魔力供给)。");

/* 红宝石手杖(辅助礼装):额外魔力池上限100,轮次结束+10 */
r = def_item_compat(w, "红宝石手杖", KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, KS_F_UNIQUE, 1, 0, 1);
db_eff(w, r, E_MANA(EF_MANA_UP, 10, -2));
db_set_text(w, r, "[唯一]额外魔力池上限100(初始0),每轮次结束+10魔力补给;获取[灵脉供魔]时可转移至多10进入此池(演示:轮次结束+10)。");

/* 白鹳骑士(限定礼装):[储备10][支援]敌方-10%胜率(非主力减半);反击无效基础礼装 */
r = def_item_compat(w, "白鹳骑士", KS_RANK_C, KS_WHEN_ANY, 10, 0, KS_F_UNIQUE | KS_F_ASSIST, 1, 10, 1);
db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 1));
db_set_text(w, r, "[储备10/10][唯一][支援]可重复发动:给予任一单位[-10%]胜率惩罚(非主力减半);[反击]令敌方发动的任一基础礼装效果无效化(不返还消耗;演示:敌方-10%胜率)。");

/* 月灵髓液(限定礼装):[唯一][支援]常驻抗性+10%;发动敌方主力-15%或自身抗性+10% */
r = def_item_compat(w, "月灵髓液", KS_RANK_C, KS_WHEN_ANY, 20, 3, KS_F_UNIQUE | KS_F_ASSIST, 1, 0, 1);
db_eff(w, r, E_WIN(EF_WIN_DOWN, 15, 1));
db_set_text(w, r, "[唯一][支援]常驻:每轮次礼装发动次数不为0时自己[抗性上升:+10%];发动:敌方[主力位]-15%胜率惩罚或自身抗性+10%;初遇敌方主力时可无视回转再发一次(演示:发动时敌方-15%)。");

/* 伪臣之书(限定礼装):消耗令咒制作,视为圣杯契约(演示:令咒+1近似) */
r = def_item_compat(w, "伪臣之书", KS_RANK_C, KS_WHEN_ANY, 0, 0, KS_F_UNIQUE | KS_F_ASSIST, 1, 0, 1);
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_CS; e.value = 1; e.target = -2;
    e.desc = "消耗1令咒与行动阶段制作;视为与指定从者存在[圣杯契约],持有[伪臣令咒]";
    db_eff(w, r, e);
}
db_set_text(w, r, "[唯一]行动阶段消耗1枚令咒与行动阶段制作;持有此礼装视为与指定从者存在[圣杯契约],可对其魔力供应并拥有御主权利;同时视为[伪臣令咒](演示:令咒+1近似)。");

/* 圣马丁的圣骸布(概念武装):[储备3]即死15%判定无效化;残废判定默认失败 */
r = def_item_compat(w, "圣马丁的圣骸布", KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, KS_F_UNIQUE, 1, 3, 1);
db_eff(w, r, E_MANA(EF_MANA_UP, 10, -2));
db_set_text(w, r, "[储备3/3][唯一]受到[即死]影响时消耗[储备1]进行15%判定,成功令其无效(可再消耗重试);成为[残废]判定目标时默认失败(演示:持有+10魔力近似圣骸布防护)。");

/* 黑键(概念武装):消耗1储备:敌方非支援位-5%胜率;对魔性-10% */
r = def_item_compat(w, "黑键", KS_RANK_C, KS_WHEN_ANY, 0, 0, KS_F_ASSIST, 1, 1, 0);
db_eff(w, r, E_WIN(EF_WIN_DOWN, 5, 1));
{
    E e; memset(&e, 0, sizeof(e));
    e.chance_attr_base = -1;
    e.flag = EF_WIN_DOWN; e.value = 10; e.target = 1;
    e.cond = KC_TARGET_TRAIT; e.cond_arg = TR_DEMONIC;
    db_eff(w, r, e);
}
db_set_text(w, r, "[储备1/1][支援]消耗[储备1]给予敌方战斗位非[支援位]任一单位[-5%]胜率惩罚;对持有[魔性]特性的单位额外[-10%]。");

/* 雅马哈V-MAX1200(科技造物):允许机动/介入;协助机动;参战时+10%胜率 */
r = def_item_compat(w, "雅马哈V-MAX1200大魔鬼", KS_RANK_C, KS_WHEN_PASSIVE, 0, 0, KS_F_RIDE, 0, 0, 0);
db_eff(w, r, E_WIN(EF_WIN_UP, 10, -2));
db_set_text(w, r, "[骑乘]允许[机动][介入];执行机动时允许至多1名单位协助;自身[机动][介入]到达目标灵脉并参战时+10%胜率;持有[骑乘]技能时无视前置条件触发其效果(演示:常驻+10%胜率)。");

/* 马克沁重机枪(科技造物):[支援][爆发]初始工序敌方全体100-敏捷负面判定-10%胜率(从者减半) */
r = def_item_compat(w, "马克沁重机枪", KS_RANK_C, KS_WHEN_PROC, 0, 3, KS_F_ASSIST | KS_F_ENERGY, 0, 0, 0);
db_eff(w, r, E_WIN(EF_WIN_DOWN, 10, 0));
db_set_text(w, r, "[支援][爆发]初始工序:对敌方非支援位全体进行[100-敏捷%]负面判定,成功给予[-10%]胜率惩罚(对[从者]减半);可额外宣言至多3次(演示:敌方全体-10%胜率)。");

/* 铁拳火箭筒(科技造物):[储备3][支援]最终工序对敌方主力60%负面判定,成功[-100-耐久]%胜率惩罚 */
r = def_item_compat(w, "铁拳火箭筒", KS_RANK_C, KS_WHEN_PROC, 0, 9, KS_F_ASSIST, 0, 3, 0);
db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
db_set_text(w, r, "[储备3/3][支援]最终工序:消耗[储备1]对敌方[主力位]进行附加[优势]补正的60%负面判定,成功给予[-100-耐久]%胜率惩罚(从者判定减半;演示:敌方主力-20%胜率)。");

/* TT-33军用制式手枪(科技造物):[储备8][支援]消耗储备:20*储备%判定-20%胜率 */
r = def_item_compat(w, "TT-33军用制式手枪", KS_RANK_C, KS_WHEN_ANY, 0, 0, KS_F_ASSIST, 0, 8, 0);
db_eff(w, r, E_WIN(EF_WIN_DOWN, 20, 1));
db_set_text(w, r, "[储备8/8][支援]消耗任意数量储备,对敌方非支援位任一单位进行[20*消耗储备%]负面判定,成功给予[-20%]胜率惩罚(目标耐久≥60减半;演示:敌方单体-20%胜率)。");

/* 弹药包(科技造物):[储备5][支援]消耗储备给其他科技造物增加储备(演示:自身胜率+5%近似) */
r = def_item_compat(w, "弹药包", KS_RANK_C, KS_WHEN_ANY, 0, 0, KS_F_ASSIST, 0, 5, 0);
db_eff(w, r, E_WIN(EF_WIN_UP, 5, -2));
db_set_text(w, r, "[储备5/5][支援]发动时消耗任意储备,令自身持有的任一[科技造物]获得[20*消耗%]储备(不足1舍去;对[储备1/1]仅需消耗1;演示:自身+5%胜率近似装填)。");
}
