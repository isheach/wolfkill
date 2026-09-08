#ifndef KSG_H
#define KSG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <io.h>
#endif

#define KSG_NAME_MAX 64      /* 名称长度(UTF-8 中文名以字节计,32 不够) */
#define KSG_TEXT_MAX 512     /* 效果文本长度 */
#define KSG_SKILL_MAX 16     /* 每单位技能槽 */
#define KSG_PHANTASM_MAX 6   /* 每单位宝具槽 */
#define KSG_ITEM_MAX 16      /* 每单位礼装槽 */
#define KSG_POOL_MAX 6       /* 附带魔力池槽 */
#define KSG_STATUS_MAX 16    /* 每单位状态槽 */
#define KSG_EOF_MAX 16       /* 生效链槽 */
#define KSG_COLUMN_MAX 12    /* 每方战斗位列总数 */
#define KSG_ACTION_MAX 32    /* 行动注册上限 */
#define KSG_TRIGGER_MAX 32   /* 效果触发注册上限 */

/* ---------- 枚举 ---------- */

/* 时段 */
enum ks_time {
    KS_TIME_ROUND_START = 0,   /* 轮次开始时 */
    KS_TIME_DAY_START,         /* (昼)回合开始时 */
    KS_TIME_DAY_ACT,           /* (昼)回合行动提交时 */
    KS_TIME_DAY_RESOLVE,       /* (昼)回合行动执行时 */
    KS_TIME_DAY_END,           /* (昼)回合结束时 */
    KS_TIME_NIGHT_START,       /* (夜)回合开始时 */
    KS_TIME_NIGHT_ACT,         /* (夜)回合行动提交时 */
    KS_TIME_NIGHT_RESOLVE,     /* (夜)回合行动执行时 */
    KS_TIME_NIGHT_END,         /* (夜)回合结束时 */
    KS_TIME_ROUND_END,         /* 轮次结束时 */
    KS_TIME_BATTLE_START,      /* 战斗开始时 */
    KS_TIME_PROC_OPEN,         /* 初始工序 */
    KS_TIME_PROC_MAIN,         /* 主要工序 */
    KS_TIME_PROC_FINAL,        /* 最终工序 */
    KS_TIME_BATTLE_END,        /* 战斗结束时 */
    KS_TIME_ANY,               /* 随时 */
    KS_TIME_COUNT
};
const char *ks_time_name(int t);

/* 行动类型 */
enum ks_action {
    KS_ACT_NONE = 0,
    KS_ACT_MOVEMENT,       /* 机动 */
    KS_ACT_SOULFEED,       /* 魂食 */
    KS_ACT_INTERVENE,      /* 干涉 */
    KS_ACT_UNLOCK,         /* 解放(宝具) */
    KS_ACT_CRAFT,          /* 制造(礼装制作) */
    KS_ACT_BUILD,          /* 建设(工房建造) */
    KS_ACT_RECON,          /* 侦查(广泛/定向) */
    KS_ACT_INVESTIGATE,    /* 调查(情报调查/资料分析/真名猜测) */
    KS_ACT_REST,           /* 休整(补魔/自我调整) */
    KS_ACT_DEMOLISH,       /* 摧毁工房 */
    KS_ACT_COUNT
};
const char *ks_act_name(int a);

/* 从者属性 */
enum ks_attr { A_STR, A_END, A_AGI, A_MAG, A_LUK, A_NP, A_CIRCUIT, A_ATTR_COUNT };
const char *ks_attr_name(int a);

/* 战斗位 */
enum ks_slot {
    KS_SLOT_NONE = 0,
    KS_SLOT_MAIN = 1,   /* 主力位 */
    KS_SLOT_SUPPORT,    /* 辅助位 */
    KS_SLOT_SERVANT,    /* 仆役位 */
    KS_SLOT_REAR,       /* 支援位 */
};
const char *ks_slot_name(int s);

/* 阵营轴 */
enum ks_align { KS_LAW = 0, KS_MID, KS_CHAOS };
enum ks_moral { KS_GOOD = 0, KS_NEUT, KS_EVIL };

/* 单位类型 */
enum ks_utype {
    KS_U_SERVANT = 1,   /* 从者 */
    KS_U_MASTER,        /* 御主 */
    KS_U_SUMMON,        /* 召唤物 */
    KS_U_DOLL,          /* 人偶 */
    KS_U_FAMILIAR,      /* 使魔 */
};
const char *ks_utype_name(int u);

/* 资源类型(技能/宝具/礼装/科技造物) */
enum ks_restype {
    KS_R_NONE = 0,
    KS_R_SKILL = 1,     /* 技能(职阶/天赋/技艺/祝福/荣冠/兵器/魔术) */
    KS_R_PHANTASM,      /* 宝具 */
    KS_R_ITEM,          /* 礼装 */
    KS_R_TECH,          /* 科技造物 */
};
const char *ks_reskind_name(int r);

/* 技能类型(类型:职阶/天赋/…) */
enum ks_skilltype {
    KS_T_CLASS = 1, KS_T_TALENT, KS_T_TECHNIQUE, KS_T_BLESS, KS_T_CROWN, KS_T_WEAPON, KS_T_MAGIC,
};
const char *ks_skilltype_name(int t);

/* 宝具类型(类型:对人/对军/对城/对界/结界/…) */
enum ks_nptype {
    KS_NP_HUMAN = 1, KS_NP_ARMORY, KS_NP_CASTLE, KS_NP_WORLD, KS_NP_BOUND, KS_NP_MAKLESS,
};
const char *ks_nptype_name(int t);

/* 等级 */
enum ks_rank { KS_RANK_NEG = 0, KS_RANK_E = 1, KS_RANK_D, KS_RANK_C, KS_RANK_B, KS_RANK_A, KS_RANK_EX };
int ks_rank_value(int r);                 /* 数字数值 EX=6 */
int ks_rank_from_str(const char *s);
const char *ks_rank_name(int r);

/* 发动时机(资源内部) */
enum ks_when {
    KS_WHEN_PASSIVE = 0,   /* 常驻 */
    KS_WHEN_ACT,           /* 行动阶段 */
    KS_WHEN_BATTLE_START,  /* 战斗开始时 */
    KS_WHEN_PROC,          /* 指定工序 */
    KS_WHEN_ANY,           /* 随时 */
};
const char *ks_when_name(int w);

/* 资源特效(位掩码) */
enum ks_feat {
    KS_F_NONE = 0,
    KS_F_MAIN = 1 << 0,        /* [主力位] */
    KS_F_SUPPORT = 1 << 1,     /* [辅助位] */
    KS_F_SERVANT = 1 << 2,     /* [仆役位] */
    KS_F_REAR = 1 << 3,        /* [支援位] */
    KS_F_ASSIST = 1 << 4,      /* [支援]:主力/辅助/支援皆可 */
    KS_F_COUNTER = 1 << 5,     /* [反击] */
    KS_F_RIDE = 1 << 6,        /* [骑乘] */
    KS_F_BURST_READY = 1 << 7, /* [蓄力] */
    KS_F_PIERCE = 1 << 8,      /* [必中] */
    KS_F_INV_PIERCE = 1 << 9,  /* [无敌贯通] */
    KS_F_ENERGY = 1 << 10,     /* [爆发] */
    KS_F_UNIQUE = 1 << 11,     /* [唯一] */
};
const char *ks_feat_name(int f);

/* 状态 */
enum ks_status {
    S_NONE = 0,
    /* 强化 */
    S_EVADE,        /* 回避 */
    S_INVINCIBLE,   /* 无敌 */
    S_PROTECT,      /* 保护 */
    S_RESUP,        /* 抗性上升 */
    S_STATE_RES,    /* 状态抵抗 */
    S_STATE_IM,     /* 状态免疫 */
    S_EFFECT_IM,    /* 效果免疫 */
    /* 弱化 */
    S_TIRED,        /* 疲惫 */
    S_CRIPPLED,     /* 残废 */
    S_LAG,          /* 迟滞 */
    S_CURSE,        /* 诅咒 */
    S_SEAL,         /* 封印 */
    S_SKILL_SEAL,   /* 技能封印 */
    S_NP_SEAL,      /* 宝具封印 */
    S_RESDOWN,      /* 抗性下降 */
    /* 异常 */
    S_POISON,       /* 中毒 */
    S_BURN,         /* 灼伤 */
    S_FREEZE,       /* 冻结 */
    S_ELECTRIC,     /* 感电 */
    S_STONE,        /* 石化 */
    S_STUN,         /* 晕眩 */
    S_CHARM,        /* 魅惑 */
    S_CONFUSE,      /* 混乱 */
    S_FEAR,         /* 恐惧 */
    S_RES_BREAK,    /* 抗性破除 */
    S_TRAIT_GIVE,   /* 特性赋予 */
    S_CHARGE,       /* 蓄力层数(每工序-1) */
    S_BADGE,        /* 加符层数 */
    S_STATUS_COUNT
};
const char *ks_status_name(int s);
/* 状态是增强/弱化/异常 */
int ks_status_class(int s);

/* 契约种类 */
enum ks_contract_kind {
    C_NONE = 0,
    C_HOLY,     /* 圣杯契约 */
    C_ALLIANCE, /* 同盟契约 */
    C_TRUCE,    /* 不战契约 */
    C_MANA,     /* 魔力契约 */
    C_FORCE,    /* 强制契约 */
    C_SLAVE,    /* 奴役契约 */
    C_DUEL,     /* 决斗契约 */
};
const char *ks_contract_name(int c);

/* 战术 */
enum ks_tactic {
    T_NONE = 0,
    T_STRIKE,   /* 强击 */
    T_RAID,     /* 破袭 */
    T_PROBE,    /* 试探 */
    T_HOLD,     /* 扼守 */
};
const char *ks_tactic_name(int t);

/* 指令(战斗内命令) */
enum ks_order {
    O_NONE = 0,
    O_CHARGE,   /* 冲锋 */
    O_PURSUE,   /* 追击 */
    O_COVER,    /* 掩护 */
    O_DUEL,     /* 死斗 */
};
const char *ks_order_name(int o);

/* ---------- 基础容器 ---------- */

typedef struct ks_pool {
    int cap;       /* 上限 */
    int floor;     /* 下限 */
    int cur;       /* 当前 */
} ks_pool_t;

typedef struct ks_stat {
    int kind;      /* enum ks_status */
    int layers;    /* 层数,0 表示无层数状态 */
    int source;    /* 来源单位 id(0=无)、来源技能名散列 */
    int remain;    /* 剩余时点(内部用) */
} ks_stat_t;

typedef struct ks_effect {
    int kind;      /* enum ks_effect_flag */
    int value;     /* 数值(属性/胜率/魔力等) */
    int target;    /* 目标单位 id */
    int use_cs;    /* 是否触发令咒 */
    char note[KSG_TEXT_MAX];
} ks_effect_t;

/* 效果种类 */
enum ks_effect_flag {
    EF_NONE = 0,
    EF_ATTR_UP,          /* 属性补正 */
    EF_ATTR_DOWN,        /* 属性惩罚 */
    EF_ATTR_UP_CONST,    /* 属性常驻补正 */
    EF_ATTR_DOWN_CONST,  /* 属性常驻惩罚 */
    EF_WIN_UP,           /* 胜率补正 */
    EF_WIN_DOWN,         /* 胜率惩罚 */
    EF_FINAL_WIN_UP,     /* 最终胜率补正 */
    EF_FINAL_WIN_DOWN,   /* 最终胜率惩罚 */
    EF_FLOOR_UP,         /* 底限胜率 */
    EF_FLOOR_PEN,        /* 底限穿透 */
    EF_HIT_UP,           /* 判定基础成功率补正 */
    EF_HIT_FINAL_UP,     /* 判定最终成功率补正 */
    EF_HIT_PEN,          /* 判定成功率惩罚 */
    EF_RES_UP,           /* 抗性上升 */
    EF_RES_DOWN,         /* 抗性下降 */
    EF_STATE_RES,        /* 状态抵抗 */
    EF_STATE_IM,         /* 状态免疫 */
    EF_EFFECT_IM,        /* 效果免疫 */
    EF_MANA_UP,          /* 魔力供给 */
    EF_MANA_DOWN,        /* 魔力消耗 */
    EF_STATUS_GIVE,      /* 赋予状态 */
    EF_STATUS_REMOVE,    /* 移除状态 */
    EF_BURN_BLOW,        /* 爆燃 */
    EF_ELECTRIC_BLOW,    /* 激荡 */
    EF_POISON_BLOW,      /* 毒发 */
    EF_RECAST,           /* 获得回转 */
    EF_RECAST_LOSE,      /* 失去回转 */
    EF_FP_UP,            /* FP 增加 */
    EF_FP_DOWN,          /* FP 减少 */
    EF_TP_FP,            /* 临时FP */
    EF_DEATH,            /* 即死判定 */
    EF_BOUND_DEATH,      /* 轰击判定 */
    EF_PIERCE,           /* 附加[必中] */
    EF_INV_PIERCE,       /* 附加[无敌贯通] */
    EF_EVADE,            /* 附加[回避] */
    EF_PROTECT,          /* 附加[保护] */
    EF_INVINCIBLE,       /* 附加[无敌] */
    EF_RETALIATE,        /* [反击]触发 */
    EF_SUMMON,           /* 召唤物:value=等级,cond_arg=总属性,cond_arg2=特性位,status=仆役位需求 */
    EF_INFO,             /* 信息 */
    EF_CS,               /* 令咒相关 */
    EF_OTHER,
    /* 复杂机制模块 */
    EF_PLEDGE_DECL,      /* 宣言对抗:每工序双方暗宣言(status状态),相同失效/不同给敌方主力value层 */
    EF_TICK_PROC,        /* 每工序结算:status状态层数×chance%判定, 成功按value执行小效果 */
    EF_TICK_ROUND,       /* 每回合结算:同上(轮次结束) */
    EF_RANDOM_GIVE,      /* 随机状态赋予:从[灼伤感电冻结中毒诅咒魅惑混乱恐惧疲惫]随机一项, layers层 */
    EF_ON_KILL,          /* 击杀触发:击杀目标时给予击杀者(value胜率) */
    EF_ON_WIN,           /* 胜利触发:战斗胜利时给予自方(value胜率) */
    EF_CHARGE,           /* 蓄力:status=S_CHARGE layers层, 每工序-1, 归零结算 */
    EF_GRANT_CS,         /* 给予令咒 */
    EF_GRANT_BADGE,      /* 给予加符 */
    EF_SEAL,             /* 封印:layers层 */
    EF_REGEN,            /* 再生:value魔力/工序 */
};
const char *ks_eff_name(int e);

/* 效果条件 */
enum ks_cond {
    KC_NONE = 0,            /* 无条件 */
    KC_OWN_MAIN,            /* 自身处于主力位 */
    KC_OWN_SUPPORT,         /* 自身处于辅助位 */
    KC_OWN_REAR,            /* 自身处于支援位 */
    KC_TARGET_TRAIT,        /* 目标持有特性 */
    KC_TARGET_NOT_TRAIT,    /* 目标不持有特性 */
    KC_TARGET_STATUS_EQ,    /* 目标持有状态(kind 指定) */
    KC_TARGET_STATUS_GE,    /* 目标状态层数>=N */
    KC_STATUS_NOT,          /* 目标不持有状态 */
    KC_FRIEND_STATUS,       /* 己方其他单位持有状态 */
    KC_TARGET_LUCK_GE,      /* 目标幸运>=40 */
    KC_TARGET_LEVEL_GE,     /* 目标等级>=N */
    KC_LEVEL_DIFF,          /* 自身等级-目标等级>=N(负=低于) */
    KC_DAY,                 /* 当前为昼 */
    KC_NIGHT,               /* 当前为夜 */
    KC_FIRST_ENCOUNTER,     /* 初次与敌方主力交战 */
    KC_ENEMY_MAIN_MASTER,   /* 敌方主力位为御主 */
    KC_SELF_HP,             /* 自身属性>=N */
    KC_SELF_MP,             /* 自身魔力>=N */
    KC_FRIEND_IN_BATTLE,    /* 己方战斗位存在其他单位 */
    KC_ENEMY_IN_BATTLE,     /* 敌方战斗位存在其他单位 */
    KC_SELF_TRAIT,          /* 自身持有特性 */
    KC_ENEMY_TACTIC,        /* 敌方战术==N */
    KC_SELF_TACTIC,         /* 己方战术==N */
    KC_TACTIC_NOT_PAIRED,   /* 己方战术未被我方克制 */
    KC_ENEMY_IS_SERVANT,    /* 敌方主力位是从者 */
    KC_ENEMY_IS_SUMMON,     /* 敌方单位是召唤物 */
    KC_SELF_IS_MASTER,      /* 自身是御主 */
    KC_SELF_IS_SERVANT,     /* 自身是从者 */
    KC_HAS_CS,              /* 自阵营持有令咒 */
    KC_MP_UNDER,            /* 自身魔力<0(魔力不足) */
    KC_TARGET_AGI_LT,       /* 目标敏捷<N */
    KC_TARGET_AGI_GE,       /* 目标敏捷>=N */
};
const char *ks_cond_name(int c);

/* 效果行:一条可解析的最小效果指令 */
typedef struct ks_effectline {
    int flag;                  /* enum ks_effect_flag */
    int attr;                  /* 属性(用于 EF_ATTR_*) */
    int value;                 /* 数值 */
    int status;                /* 状态(用于 EF_STATUS_*) */
    int layers;
    int rank;                  /* 等级阈值 */
    int target;                /* 0=敌方全体 -1=己方全体 -2=施术者自身 正=敌方第N位 */
    int times;                 /* 重复次数(1=一次) */
    int cond;                  /* enum ks_cond 触发条件 */
    int cond_arg;              /* 条件参数(特性/状态/战术/数值) */
    int cond_arg2;             /* 条件第二参数 */
    int chance;                /* 判定成功率(0=必定生效) */
    int chance_attr_base;      /* 判定成功率=自身该属性(代替固定值) */
    int chance_neg;            /* 负面判定(受抗性) */
    int luck_halve;            /* 目标幸运>=40时判定成功率减半 */
    int cap;                   /* 总值上限(<=0 不限) */
    char desc[KSG_TEXT_MAX];   /* 原文摘录 */
} ks_effectline_t;

/* 宝具面向(建卡时每种至多购入1个) */
enum ks_focus {
    FC_NONE = 0,
    FC_DECISIVE = 1,   /* 决战 */
    FC_INSTAKILL,      /* 即死 */
    FC_SWORD,          /* 魔剑 */
    FC_DEFENSE,        /* 防御 */
    FC_OFFENSE,        /* 进攻 */
    FC_BUFF,           /* 增益 */
    FC_SUMMON,         /* 召唤 */
    FC_STATUS,         /* 状态 */
    FC_SUPPLY,         /* 补给 */
    FC_SPECIAL,        /* 特殊 */
    FC_ANTITRAIT,      /* 特攻 */
};
const char *ks_focus_name(int f);
int ks_focus_from_str(const char *s);

/* 资源(技能/宝具/礼装的统一模板) */
typedef struct ks_res {
    int id;
    char name[KSG_NAME_MAX];
    int kind;            /* enum ks_restype */
    int type;            /* enum ks_skilltype / ks_nptype */
    int focus;           /* 宝具面向 enum ks_focus;技能=0(用type) */
    int rank;            /* enum ks_rank */
    int when;            /* enum ks_when */
    int cost;            /* 魔力消耗 */
    int recast;          /* 回转(CD) */
    int cur_recast;      /* 已积累回转点数 */
    int feat;            /* 位特效(按位或) */
    int count_per_round; /* 每轮次数(礼装用) */
    int used_this_round; /* 本轮已用次数 */
    int reserve;         /* 储备 */
    int reserve_max;
    int unique;          /* [唯一] */
    int effect_count;
    ks_effectline_t effects[KSG_EOF_MAX];
    char text[KSG_TEXT_MAX]; /* 原文效果文本 */
} ks_res_t;

/* ---------- 单位 ---------- */

typedef struct ks_unit {
    int id;
    int faction;         /* 阵营 id */
    char name[KSG_NAME_MAX];
    char true_name[KSG_NAME_MAX];  /* 真名 */
    int utype;           /* enum ks_utype */
    int servant_class;   /* 从者职阶(0=无) */
    int hidden_attr;     /* 隐藏属性:0天 1地 2人 3星 4兽 */
    int level;
    int attr[A_ATTR_COUNT];          /* 基础属性 */
    int attr_mod[A_ATTR_COUNT];      /* 临时属性补正/惩罚(战斗/回合结束清除) */
    int attr_perm[A_ATTR_COUNT];     /* 常驻属性补正/惩罚 */
    int hit_mod;                     /* 判定基础成功率补正 */
    int hit_final_mod;               /* 判定最终成功率补正 */
    int floor_win;                   /* 底限胜率(战斗内) */
    int final_win;                   /* 最终胜率(战斗内) */
    int tp_fp;                       /* 临时FP(令咒) */
    int regain_cs;                   /* 伪臣令咒/追加令咒(演示) */
    ks_pool_t mp;               /* 魔力池 */
    ks_pool_t mp_extra[KSG_POOL_MAX];  /* 额外魔力池(礼装/工房等) */
    int fp;                      /* 脱离值 FP */
    int cs;                      /* 令咒数 */
    int al_law; int al_moral;    /* 人物阵营 */
    int traits;                  /* 特性位掩码(人型/神性/魔性/龙种/猛兽/魔兽/构装体) */
    int fate_known;              /* 真名是否被对手知晓(情报) */
    int roaming;                 /* 游荡状态 */
    int tired;                   /* 上层疲惫(战斗外也影响) */
    int acted;                   /* 本回合已行动 */
    int item_uses_this_round;    /* 本轮已用礼装次数(御主上限5/从者0) */
    int pledge_state;            /* 宣言状态(复杂机制: 每工序暗宣言) */
    int pledge_layers;           /* 宣言层数 */
    int battle_side;             /* 战斗中:1=左(我方)/2=右(敌方) */
    int battle_slot;             /* 战斗位 */
    int is_main;                 /* 是否主力位 */
    int in_battle;               /* 是否在战斗 */
    int alive;
    /* 持有资源 */
    int skill_count;
    int skill_ids[KSG_SKILL_MAX];
    int phantasm_count;
    int phantasm_ids[KSG_PHANTASM_MAX];
    int item_count;
    int item_ids[KSG_ITEM_MAX];
    /* 状态 */
    ks_stat_t status[KSG_STATUS_MAX];
} ks_unit_t;

/* 特性位 */
enum ks_trait {
    TR_HUMAN = 1 << 0,   /* 人型 */
    TR_DIVINITY = 1 << 1,/* 神性 */
    TR_DEMONIC = 1 << 2, /* 魔性 */
    TR_DRAGON = 1 << 3,  /* 龙种 */
    TR_BEAST = 1 << 4,   /* 猛兽 */
    TR_MONSTER = 1 << 5, /* 魔兽 */
    TR_GOLEM = 1 << 6,   /* 构装体 */
};
const char *ks_trait_name(int t);

/* ---------- 灵脉 ---------- */

typedef struct ks_leyline {
    int id;
    char name[KSG_NAME_MAX];
    int mana;        /* 魔力量 */
    int flow;        /* 人流量 */
    int owner;       /* 灵脉主单位 id,0=无主 */
    int has_workshop;/* 有无魔术工房 */
    int has_shrine;  /* 有无神殿 */
    int has_bound;   /* 有无固有结界 */
    /* 灵脉效果(简化) */
    int eff_win;     /* 灵脉胜率补正% */
    int eff_resup;   /* 灵脉抗性上升% */
    int extra_pool;  /* 额外魔力池(工房基盘等) */
} ks_leyline_t;

/* ---------- 工房组件 ---------- */

enum ks_component_kind {
    CMPT_BASE = 1, CMPT_AUX, CMPT_LIMITED, CMPT_SUPPORT,
};
const char *ks_cmpt_kind_name(int k);

typedef struct ks_component {
    char name[KSG_NAME_MAX];
    int kind;         /* enum ks_component_kind */
    int build;        /* 所需建设层数 */
    int hp;           /* 稳态 */
    int scale;        /* 规模 */
    int owner;        /* 持有者单位 id */
    const char *effect; /* 效果摘要(原文) */
} ks_component_t;

typedef struct ks_workshop {
    int leyline;         /* 所在灵脉 */
    int owner;           /* 工房主 */
    int components[8];   /* 组件索引(指向 cmpt, -1 空) */
    int build_prog[8];   /* 各组件建设进度(层) */
    int total_scale;
    int pool;            /* 工房魔力池 */
} ks_workshop_t;

/* ---------- 契约 ---------- */

typedef struct ks_contract {
    int kind;        /* enum ks_contract */
    int a;           /* 立约人 */
    int b;           /* 签约人 */
    int active;
    char note[KSG_TEXT_MAX];
} ks_contract_t;

/* ---------- 战斗 ---------- */

typedef struct ks_battle_unit {
    int uid;
    int slot;        /* 战斗位 */
    int is_side;     /* 1 左 / 2 右 */
} ks_battle_unit_t;

/* 参数(构造用) */
typedef struct ks_battle_cfg {
    int width;       /* 战场宽度 */
    int day;         /* 是否昼 */
    int leyline;
    int time_scale;  /* 0=正常 1=永昼 2=永夜 */
} ks_battle_cfg_t;

/* 战斗上下文 */
typedef struct ks_battle {
    int active;
    int leyline;
    int width;
    int phase;               /* 0=开始前 1=战斗开始时 2=初始 3=主要 4=最终 5=结束时 */
    int day;                 /* 昼=1 夜=0 */
    ks_battle_unit_t left[KSG_COLUMN_MAX];
    ks_battle_unit_t right[KSG_COLUMN_MAX];
    int left_n, right_n;
    int left_main, right_main;   /* 主力位 uid */
    int tactics[3];              /* 0 不用;1=左 2=右 的战术选择 */
    int main_attr[3];            /* 双方主力位选择的主要属性;0=未选 */
    int rand_attr;               /* 随机属性 */
    int left_win, right_win;     /* 当前胜率(含补正/惩罚) */
    int left_final_win, right_final_win; /* 最终胜率修正(直接加在决胜前) */
    int left_floor, right_floor; /* 底限胜率 */
    int left_base, right_base;   /* 战斗属性基础胜率 */
    int left_ok, right_ok;       /* 是否撤退/战败标记 */
    int left_retreat, right_retreat;   /* 撤退宣言 */
    int left_main_lost, right_main_lost; /* 主力位已脱离 */
    int char_orders[3];          /* 冲锋/追击/掩护/死斗(以左方视角) */
    int round_used;              /* 每轮次已用的圣杯魔法点(演示用) */
} ks_battle_t;

/* ---------- 世界 ---------- */

typedef struct ks_world {
    int round;              /* 轮次数(1 起) */
    int day;                /* 1=昼 0=夜 */
    int phase;              /* enum ks_time 当前时点 */
    int active_action;      /* 当前处理中的行动 */
    ks_leyline_t leylines[16];
    int leyline_count;
    ks_unit_t units[64];
    int unit_count;
    ks_workshop_t workshops[8];
    int workshop_count;
    ks_component_t cmpts[32];
    int cmpt_count;
    ks_contract_t contracts[32];
    int contract_count;
    ks_res_t res[1024];
    int res_count;
    ks_battle_t battle;
    int verbosity;
    unsigned int rng_state;
} ks_world_t;

/* 全局世界 */
extern ks_world_t g_w;

/* ---------- RNG ---------- */

void ks_rng_seed(ks_world_t *w, unsigned seed);
int ks_roll(ks_world_t *w);          /* 1..100 */
int ks_roll_bool(ks_world_t *w, int percent);

/* ---------- 日志 ---------- */

void ks_log(ks_world_t *w, const char *fmt, ...);
void ks_verbose(ks_world_t *w, const char *fmt, ...);

/* ---------- 单位 ---------- */

int ks_unit_new(ks_world_t *w, const char *name, const char *tname, int utype,
                int level, int faction);
void ks_unit_set_attr(ks_world_t *w, int uid, int attr, int base, int mod);
void ks_unit_set_mp(ks_world_t *w, int uid, int cap, int floor, int cur);
void ks_unit_add_extra_pool(ks_world_t *w, int uid, int cap, int cur);
int ks_unit_hp_attr(ks_world_t *w, int uid, int attr);            /* 基础 + 补正(含状态) */
int ks_unit_effective_attr(ks_world_t *w, int uid, int attr);     /* hp_attr 但吃状态惩罚 */
int ks_unit_gain_status(ks_world_t *w, int uid, int kind, int layers, int src);
int ks_unit_has_status(ks_world_t *w, int uid, int kind);
int ks_unit_state_res_check(ks_world_t *w, int uid, int kind);    /* 状态抵抗/免疫判定 */
int ks_unit_resist(ks_world_t *w, int uid, int base);             /* 抗性上升:负面判定最终成功率惩罚 */
void ks_apply_effect_line(ks_world_t *w, int uid, const ks_effectline_t *el, int src); /* 结算一条效果行 */
int ks_unit_add_res(ks_world_t *w, int uid, int id);              /* 持有资源 */

/* 状态引擎(ksg_status.c) */
int ks_status_layers_at(ks_world_t *w, int uid, int kind);
int ks_status_remove_layers(ks_world_t *w, int uid, int kind, int layers);
void ks_status_clear(ks_world_t *w, int uid, int kind);

/* 复杂机制模块 (ksg_effects.c) */
void ks_pledge_decl(ks_world_t *w, int src, int uid, const ks_effectline_t *el); /* 宣言对抗注册 */
void ks_tick_proc_mechanics(ks_world_t *w);   /* 每工序推进: 蓄力-1/宣言结算 */
int ks_status_immune(ks_world_t *w, int uid, int kind);
int ks_status_resist(ks_world_t *w, int uid, int kind);
void ks_status_round_start_tick(ks_world_t *w, int uid);
void ks_status_proc_start_tick(ks_world_t *w, int uid);
void ks_status_proc_end_tick(ks_world_t *w, int uid);
void ks_status_round_end_tick(ks_world_t *w, int uid);
void ks_status_battle_end_clean(ks_world_t *w, int uid);
void ks_status_print_unit(ks_world_t *w, int uid);
void ks_unit_tick(ks_world_t *w, int uid);                        /* 回合结束时点:状态衰减/魔力结算 */
int ks_unit_mp_sum(ks_world_t *w, int uid);                       /* 全部魔力池之和 */
void ks_unit_transfer_mp(ks_world_t *w, int from, int to, int n);
int ks_unit_attr_value(ks_world_t *w, int uid, int attr);         /* 含属性补正/惩罚,不含战斗位减半 */
int ks_unit_total_attr_value(ks_world_t *w, int uid, int attr, int slot); /* 按战斗位计入 */

/* 状态管理 */
void ks_status_tick(ks_world_t *w, int uid);        /* 回合结束衰减/清除 */
void ks_status_proc_tick(ks_world_t *w, int uid);   /* 工序结束时点衰减 */

/* ---------- 回转 ---------- */

void ks_recast_tick_round(ks_world_t *w, int uid);  /* 回合结束:回转+3 */
void ks_recast_tick_proc(ks_world_t *w, int uid);   /* 工序结束:回转+1 */
int ks_res_use(ks_world_t *w, int uid, int rid);    /* 尝试消耗/使用资源(魔耗/回转/计数),返回 0 失败 */
void ks_res_gain_recast(ks_world_t *w, int uid, int rid, int n);

/* ---------- 行动 ---------- */

typedef int (*ks_action_fn)(ks_world_t *w, int uid, const char *arg);

typedef struct ks_action_entry {
    int type;                 /* enum ks_action */
    int order;                /* 结算顺序(与 ks_act_order 一致) */
    const char *help;
    ks_action_fn fn;
} ks_action_entry_t;

int ks_action_register(ks_world_t *w, int type, ks_action_fn fn, const char *help);
void ks_action_do(ks_world_t *w, int uid, int type, const char *arg);
void ks_action_tick(ks_world_t *w);                 /* 按规则顺序执行本回合全部提交 */

/* 内置行动 */
int ks_act_intervene(ks_world_t *w, int uid, const char *arg);
int ks_act_recon(ks_world_t *w, int uid, const char *arg);
int ks_act_rest(ks_world_t *w, int uid, const char *arg);
int ks_act_soulfeed(ks_world_t *w, int uid, const char *arg);
int ks_act_movement(ks_world_t *w, int uid, const char *arg);
int ks_act_craft(ks_world_t *w, int uid, const char *arg);
int ks_act_build(ks_world_t *w, int uid, const char *arg);
int ks_act_investigate(ks_world_t *w, int uid, const char *arg);
int ks_act_unlock(ks_world_t *w, int uid, const char *arg);
int ks_act_demolish(ks_world_t *w, int uid, const char *arg);

/* ---------- 魔力 ---------- */

int ks_mp_pay(ks_world_t *w, int uid, int n);       /* 即时消耗,返回实际支付 */
void ks_mp_gain(ks_world_t *w, int uid, int n);     /* 即时供给 */
void ks_mp_settle(ks_world_t *w, int uid);          /* 魔力结算:移除溢出、判定魔力不足惩罚 */

/* ---------- 判定 ---------- */

typedef struct ks_check {
    int base;        /* 基础成功率 */
    int base_mod;    /* 基础补正 */
    int base_pen;    /* 基础惩罚 */
    int final_mod;   /* 最终补正 */
    int final_pen;   /* 最终惩罚 */
    int neg;         /* 是否为负面判定 */
    int status_kind; /* 状态判定(0=非状态) */
} ks_check_t;

int ks_check_final(ks_world_t *w, ks_check_t *c);
int ks_check_roaming_cap(ks_world_t *w, int uid, int rate);
int ks_check_roll(ks_world_t *w, int uid, ks_check_t *c, int *final_rate);
int ks_check_negative(ks_world_t *w, int uid, int target, ks_check_t *c);

/* ---------- 胜率/战斗 ---------- */

int ks_battle_start(ks_world_t *w, ks_battle_cfg_t *cfg);
int ks_battle_battery_check(ks_world_t *w);                 /* 计算战斗属性基础胜率 */
void ks_battle_tactics(ks_world_t *w, int side, int tac);    /* 选择战术 */
void ks_battle_attr_pick(ks_world_t *w, int side, int attr); /* 选择主要属性 */
void ks_battle_roll_rand_attr(ks_world_t *w);                /* 随机属性 */
void ks_battle_add_win(ks_world_t *w, int side, int n);      /* 侧面胜率修正(补/惩) */
int ks_battle_effective(ks_world_t *w);                      /* 计算最终胜率与决胜检定 */
void ks_battle_order(ks_world_t *w, int side, int order);    /* 冲锋/追击/掩护/死斗指令 */
void ks_battle_retreat(ks_world_t *w, int side);               /* 撤退(FP消耗→游荡) */
void ks_battle_chain_resolve(ks_world_t *w, int *uids, int *rids, int n); /* 结算链批量结算 */
void ks_charge_clear(void);
void ks_charge_begin(ks_world_t *w, int uid, int rid, int proc);
void ks_charge_tick(ks_world_t *w);
void ks_battle_cast_skill_ext(ks_world_t *w, int uid, int rid, int has_charge, int has_burst);
int ks_battle_unit_in_left(ks_world_t *w, int uid);
void ks_battle_cast_skill(ks_world_t *w, int uid, int rid);  /* 在战斗上下文中结算技能/宝具 */
int ks_battle_unit_castable(ks_world_t *w, int uid, int when, int *rids, int maxn); /* 列出可发动能力 */
int ks_battle_apply_effect(ks_world_t *w, ks_effect_t *e);   /* 在战斗上下文中应用效果 */
void ks_apply_effect_line(ks_world_t *w, int uid, const ks_effectline_t *el, int src);
void ks_apply_res_effects(ks_world_t *w, int src, int rid, int target_override);
void ks_battle_end(ks_world_t *w);
void ks_battle_tick(ks_world_t *w);                          /* 综合作战仿真一步 */
int ks_battle_demo(ks_world_t *w);                           /* 自动沙盘 */
void ks_battle_print(ks_world_t *w);

/* ---------- 世界 ---------- */

void ks_world_init(ks_world_t *w, unsigned seed);
void ks_world_round_start(ks_world_t *w);
void ks_world_round_end(ks_world_t *w);
void ks_world_day_night(ks_world_t *w);
int ks_world_advance(ks_world_t *w);      /* 推进到下一个时点 */
int ks_world_register_res(ks_world_t *w, ks_res_t *r);
int ks_world_find_res(ks_world_t *w, const char *name);
void ks_world_print(ks_world_t *w);
void ks_contract_sign(ks_world_t *w, int kind, int a, int b);
void ks_contract_check_violation(ks_world_t *w, int idx, int attacker, int victim);
void ks_contract_break(ks_world_t *w, int idx);
int ks_is_friend(ks_world_t *w, int a, int b);   /* 同阵营/自阵营判定 */
int ks_unit_add_item(ks_world_t *w, int uid, int rid);

/* ---------- 数据 ---------- */

void ks_data_build(ks_world_t *w);
void ks_db_load(ks_world_t *w);   /* 扩展资源库(ksg_db.c) */
void ks_data_make_unit_sheet(ks_world_t *w, int uid, const char *sheet);
void ks_data_print_unit(ks_world_t *w, int uid);
void ks_data_print_res(ks_world_t *w, int rid);

/* ---------- 交互 ---------- */

void ks_shell(ks_world_t *w);
void demo_simple(ks_world_t *w);   /* 预设演示(ksg_sim.c) */

/* ---------- 控制台 UTF-8 ---------- */

void ks_console_utf8(void);

/* ---------- UI(建卡/对战) ---------- */

int ks_ui_menu(const char *title, const char **items, int count);      /* 数字/方向键选择,返回索引 */
int ks_ui_ask_int(const char *prompt, int lo, int hi, int def);        /* 数字输入 */
void ks_ui_ask_str(const char *prompt, char *buf, int size);
void ks_ui_pause(const char *msg);
int ks_ui_create_servant(ks_world_t *w);   /* 交互建卡:从者,返回 uid 或 -1 */
int ks_ui_create_master(ks_world_t *w);    /* 交互建卡:御主,返回 uid 或 -1 */
int ks_ui_duel(ks_world_t *w);             /* 双人回合制对战(触发→结算),返回胜方阵营 */
void ks_ui_main(ks_world_t *w);            /* 主菜单:建卡/对战/演示 */

/* ---------- 内部工具 ---------- */

#ifndef strcasecmp
#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif
#endif

static inline int ks_clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline int ks_max(int a, int b) { return a > b ? a : b; }
static inline int ks_min(int a, int b) { return a < b ? a : b; }

#endif /* KSG_H */