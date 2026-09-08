/* ksg_sim.c — 空想圣杯引擎:交互式 Shell 与自动演示入口 */
#include "ksg.h"

/* 演示场景:两名从者+两名御主、两张灵脉,验证:建卡→行动→战斗→轮次结算 */
static void preset_world(ks_world_t *w);   /* 前置声明 */

void demo_simple(ks_world_t *w)
{
    ks_log(w, "═══ 空想圣杯 · 沙盘演示 ═══");
    preset_world(w);

    /* 引用预设单位(按创建顺序:1谕天之剑 2言峰 3灼岩 4时钟塔) */
    int s1 = 1, m1 = 2, s2 = 3, m2 = 4;

    /* 第二座工房:灵脉2(演示工房战斗效果) */
    ks_workshop_t *ws2 = &w->workshops[w->workshop_count++];
    ws2->leyline = 2;
    ws2->owner = m2;
    ws2->components[0] = 11;      /* 集束光标(索引11) */
    ws2->build_prog[0] = 99;
    ws2->total_scale = 4;
    ws2->pool = 0;
    w->leylines[1].has_workshop = 1;

    /* 宝具回转预设:开局前已完成回转(模拟前一轮次蓄力) */
    for (int i = 0; i < w->unit_count; i++) {
        ks_unit_t *u = &w->units[i];
        for (int j = 0; j < u->phantasm_count; j++) {
            ks_res_t *r = &w->res[u->phantasm_ids[j] - 1];
            r->cur_recast = r->recast; /* 已完成回转 */
        }
    }

    w->units[m1 - 1].battle_side = 0;
    /* 初始工房(演示)已在 preset_world 中建立 */

    ks_world_print(w);
    ks_log(w, "");

    /* —— 第一轮次:昼 —— */
    ks_world_round_start(w);
    ks_log(w, "▶ 行动阶段(昼):提交行动指令");
    ks_action_do(w, s1, KS_ACT_INTERVENE, "2");
    ks_action_do(w, m1, KS_ACT_CRAFT, NULL);
    ks_action_do(w, s2, KS_ACT_RECON, "broad");
    ks_action_do(w, m2, KS_ACT_INVESTIGATE, NULL);

    ks_log(w, "▶ 行动结算:干涉抵达目标灵脉 → 同灵脉单位进入[交流]");
    ks_log(w, "  (Saber 与 Lancer 于灵脉2遭遇,交流未达成共识 → [袭击])");
    ks_battle_cfg_t cfg = { .width = 4, .day = 1, .leyline = 2 };
    ks_battle_start(w, &cfg);
    int guard = 0;
    while (w->battle.active && guard++ < 12) ks_battle_tick(w);
    ks_log(w, "");

    ks_world_print(w);
    ks_log(w, "");

    /* —— 轮次结束(夜回合结算) —— */
    ks_world_round_end(w);
    ks_log(w, "▶ 行动阶段(夜):魔力恢复/状态衰减");
    ks_action_do(w, s1, KS_ACT_SOULFEED, "shelter");
    ks_action_do(w, m1, KS_ACT_REST, NULL);
    ks_action_do(w, s2, KS_ACT_REST, NULL);
    ks_world_round_end(w);

    ks_world_print(w);
    ks_log(w, "");
    ks_log(w, "═══ 演示完毕:输入 help 查看可用命令 ═══");
}

/* ---------- Shell ---------- */

/* 交互模式的世界预置:2 阵营、2 灵脉、契约与工房 */
static void preset_world(ks_world_t *w)
{
    /* 灵脉 */
    struct { const char *name; int mana, flow; } ley_des[] = {
        { "东京·圣巴托罗缪教堂", 40, 5 },
        { "东京·都立封印指定仓库", 20, 4 },
    };
    for (size_t i = 0; i < sizeof(ley_des) / sizeof(ley_des[0]); i++) {
        ks_leyline_t *l = &w->leylines[i];
        snprintf(l->name, KSG_NAME_MAX, "%s", ley_des[i].name);
        l->mana = ley_des[i].mana;
        l->flow = ley_des[i].flow;
        l->owner = 0;
        l->id = (int)i + 1;
        w->leyline_count++;
    }

    /* 阵营1 */
    int s1 = ks_unit_new(w, "谕天之剑", "亚瑟·潘德拉贡", KS_U_SERVANT, 60, 1);
    ks_data_make_unit_sheet(w, s1, "Saber");
    ks_unit_set_attr(w, s1, A_STR, 55, 0); ks_unit_set_attr(w, s1, A_END, 45, 0);
    ks_unit_set_attr(w, s1, A_AGI, 55, 0); ks_unit_set_attr(w, s1, A_MAG, 45, 0);
    ks_unit_set_attr(w, s1, A_LUK, 50, 0); ks_unit_set_attr(w, s1, A_NP, 50, 0);
    ks_unit_set_mp(w, s1, 150, -100, 120);
    w->units[s1 - 1].traits = TR_HUMAN;
    w->units[s1 - 1].al_law = KS_LAW; w->units[s1 - 1].al_moral = KS_GOOD;
    w->units[s1 - 1].fp = 1; w->units[s1 - 1].cs = 3;
    ks_unit_add_res(w, s1, ks_world_find_res(w, "对魔力"));
    ks_unit_add_res(w, s1, ks_world_find_res(w, "骑乘"));
    ks_unit_add_res(w, s1, ks_world_find_res(w, "领袖气质"));
    ks_unit_add_res(w, s1, ks_world_find_res(w, "直感"));
    ks_unit_add_res(w, s1, ks_world_find_res(w, "魔力放出"));
    ks_unit_add_res(w, s1, ks_world_find_res(w, "誓约胜利之剑"));
    ks_unit_add_res(w, s1, ks_world_find_res(w, "遗世独立的理想乡"));
    ks_unit_add_res(w, s1, ks_world_find_res(w, "王之军势·阿刻琉斯之壁垒"));

    int m1 = ks_unit_new(w, "言峰教会代行", "绮礼·言峰", KS_U_MASTER, 40, 1);
    ks_data_make_unit_sheet(w, m1, "master");
    ks_unit_set_attr(w, m1, A_STR, 15, 0); ks_unit_set_attr(w, m1, A_END, 15, 0);
    ks_unit_set_attr(w, m1, A_AGI, 10, 0); ks_unit_set_attr(w, m1, A_MAG, 25, 0);
    ks_unit_set_attr(w, m1, A_LUK, 10, 0); ks_unit_set_attr(w, m1, A_CIRCUIT, 40, 0);
    ks_unit_set_mp(w, m1, 40, -50, 40);
    ks_unit_add_res(w, m1, ks_world_find_res(w, "宝石魔术"));
    ks_unit_add_res(w, m1, ks_world_find_res(w, "调律魔术"));
    ks_unit_add_res(w, m1, ks_world_find_res(w, "魔力水晶"));
    ks_unit_add_res(w, m1, ks_world_find_res(w, "咒符"));
    w->units[m1 - 1].traits = TR_HUMAN;
    w->units[m1 - 1].al_law = KS_MID; w->units[m1 - 1].al_moral = KS_NEUT;
    w->units[m1 - 1].cs = 3;
    ks_contract_sign(w, C_HOLY, m1, s1);

    /* 阵营2 */
    int s2 = ks_unit_new(w, "灼岩旋律", "库·丘林", KS_U_SERVANT, 60, 2);
    ks_data_make_unit_sheet(w, s2, "Lancer");
    ks_unit_set_attr(w, s2, A_STR, 50, 0); ks_unit_set_attr(w, s2, A_END, 50, 0);
    ks_unit_set_attr(w, s2, A_AGI, 60, 0); ks_unit_set_attr(w, s2, A_MAG, 40, 0);
    ks_unit_set_attr(w, s2, A_LUK, 40, 0); ks_unit_set_attr(w, s2, A_NP, 45, 0);
    ks_unit_set_mp(w, s2, 150, -100, 110);
    w->units[s2 - 1].traits = TR_HUMAN;
    w->units[s2 - 1].al_law = KS_MID; w->units[s2 - 1].al_moral = KS_NEUT;
    w->units[s2 - 1].fp = 1; w->units[s2 - 1].cs = 3;
    ks_unit_add_res(w, s2, ks_world_find_res(w, "对魔力"));
    ks_unit_add_res(w, s2, ks_world_find_res(w, "心眼(伪)"));
    ks_unit_add_res(w, s2, ks_world_find_res(w, "战斗续行"));
    ks_unit_add_res(w, s2, ks_world_find_res(w, "刺穿死棘之枪"));
    ks_unit_add_res(w, s2, ks_world_find_res(w, "炽天覆七重圆环"));

    int m2 = ks_unit_new(w, "时钟塔资深", "巴泽特·弗拉加", KS_U_MASTER, 40, 2);
    ks_data_make_unit_sheet(w, m2, "master");
    ks_unit_set_attr(w, m2, A_STR, 20, 0); ks_unit_set_attr(w, m2, A_END, 15, 0);
    ks_unit_set_attr(w, m2, A_AGI, 10, 0); ks_unit_set_attr(w, m2, A_MAG, 20, 0);
    ks_unit_set_attr(w, m2, A_LUK, 10, 0); ks_unit_set_attr(w, m2, A_CIRCUIT, 35, 0);
    ks_unit_set_mp(w, m2, 35, -50, 35);
    ks_unit_add_res(w, m2, ks_world_find_res(w, "超越回路"));
    ks_unit_add_res(w, m2, ks_world_find_res(w, "黑键"));
    ks_unit_add_res(w, m2, ks_world_find_res(w, "魔力水晶"));
    w->units[m2 - 1].traits = TR_HUMAN;
    w->units[m2 - 1].al_law = KS_LAW; w->units[m2 - 1].al_moral = KS_GOOD;
    w->units[m2 - 1].cs = 3;
    ks_contract_sign(w, C_HOLY, m2, s2);

    w->leylines[0].owner = m1;
    w->leylines[1].owner = m2;

    ks_workshop_t *ws = &w->workshops[w->workshop_count++];
    ws->leyline = 1;
    ws->owner = m1;
    ws->components[0] = 3;       /* 魔能重炮(索引3=黑厄深阱?按cmpt顺序:0信息基盘 1资源基盘 2炼金基盘 3强能法阵 4医疗装机 5聚合圆盘 6黑厄深阱 7魔能重炮) */
    ws->components[1] = 7;       /* 魔能重炮 */
    ws->build_prog[0] = 99;      /* 已建成 */
    ws->build_prog[1] = 99;
    ws->total_scale = 10;
    ws->pool = 0;
    w->leylines[0].has_workshop = 1;
}

static void print_help(void)
{
    ks_log(&g_w, "可用命令:");
    ks_log(&g_w, "  units              列出全部单位卡面");
    ks_log(&g_w, "  unit <id>          查看单位详情");
    ks_log(&g_w, "  res                列出全部资源");
    ks_log(&g_w, "  res <id>           查看资源详情");
    ks_log(&g_w, "  ley                列出灵脉");
    ks_log(&g_w, "  act <unit> <行动>  执行行动: 机动/魂食/干涉/制造/建设/侦查/调查/休整/解放/摧毁");
    ks_log(&g_w, "  battle start       开始战斗(自动布阵)");
    ks_log(&g_w, "  battle step        推进一个战斗时点");
    ks_log(&g_w, "  battle print       打印战斗计算表");
    ks_log(&g_w, "  contract <k> <a> <b>  签订契约(1圣杯 2同盟 3不战 4魔力 5强制 6奴役 7决斗)");
    ks_log(&g_w, "  round end          结束轮次并结算");
    ks_log(&g_w, "  round start        开始下一轮次");
    ks_log(&g_w, "  demo               运行自动演示");
    ks_log(&g_w, "  seed <n>            设定随机种子");
    ks_log(&g_w, "  quit                退出");
}

static void cmd_units(void)
{
    for (int i = 0; i < g_w.unit_count; i++)
        ks_data_print_unit(&g_w, g_w.units[i].id);
}

static void cmd_res(void)
{
    for (int i = 0; i < g_w.res_count; i++) {
        ks_res_t *r = &g_w.res[i];
        printf("  %3d %s[%s] %s\n", r->id, r->name, ks_rank_name(r->rank),
               ks_reskind_name(r->kind));
    }
}

static void cmd_act(int uid, const char *rest)
{
    if (uid < 1 || uid > g_w.unit_count) { ks_log(&g_w, "单位不存在"); return; }
    char action[24], arg[64];
    action[0] = 0; arg[0] = 0;
    sscanf(rest, "%23s %63[^\n]", action, arg);
    if (!strcasecmp(action, "机动")) ks_action_do(&g_w, uid, KS_ACT_MOVEMENT, arg);
    else if (!strcasecmp(action, "魂食")) ks_action_do(&g_w, uid, KS_ACT_SOULFEED, arg);
    else if (!strcasecmp(action, "干涉")) ks_action_do(&g_w, uid, KS_ACT_INTERVENE, arg);
    else if (!strcasecmp(action, "解放")) ks_action_do(&g_w, uid, KS_ACT_UNLOCK, arg);
    else if (!strcasecmp(action, "制造")) ks_action_do(&g_w, uid, KS_ACT_CRAFT, arg);
    else if (!strcasecmp(action, "建设")) ks_action_do(&g_w, uid, KS_ACT_BUILD, arg);
    else if (!strcasecmp(action, "侦查")) ks_action_do(&g_w, uid, KS_ACT_RECON, arg);
    else if (!strcasecmp(action, "调查")) ks_action_do(&g_w, uid, KS_ACT_INVESTIGATE, arg);
    else if (!strcasecmp(action, "休整")) ks_action_do(&g_w, uid, KS_ACT_REST, arg);
    else if (!strcasecmp(action, "摧毁")) ks_action_do(&g_w, uid, KS_ACT_DEMOLISH, arg);
    else ks_log(&g_w, "未知行动 %s", action);
}

void ks_shell(ks_world_t *w)
{
    char line[256];
    ks_log(w, "");
    ks_log(w, "══════════ 空想圣杯 · 模拟器 ══════════");
    ks_log(w, "输入 help 查看命令列表; demo 运行自动演示");
    for (;;) {
        printf("\nksg> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        /* 去掉尾部换行 */
        line[strcspn(line, "\n")] = 0;
        char cmd[32], rest[192];
        cmd[0] = 0; rest[0] = 0;
        sscanf(line, "%31s %191[^\n]", cmd, rest);
        if (!strcasecmp(cmd, "quit") || !strcasecmp(cmd, "exit")) break;
        else if (!strcasecmp(cmd, "help")) print_help();
        else if (!strcasecmp(cmd, "units")) cmd_units();
        else if (!strcasecmp(cmd, "unit")) {
            int id = atoi(rest);
            if (id >= 1 && id <= w->unit_count) ks_data_print_unit(w, id);
        }
        else if (!strcasecmp(cmd, "res")) {
            if (rest[0]) ks_data_print_res(w, atoi(rest));
            else cmd_res();
        }
        else if (!strcasecmp(cmd, "ley")) {
            for (int i = 0; i < w->leyline_count; i++) {
                ks_leyline_t *l = &w->leylines[i];
                ks_log(w, "  %s 魔力%d 人流量%d 主:%s 工房:%s", l->name, l->mana, l->flow,
                       l->owner ? w->units[l->owner - 1].name : "无主",
                       l->has_workshop ? "有" : "无");
            }
        }
        else if (!strcasecmp(cmd, "act")) {
            int uid = atoi(rest);
            const char *act = rest;
            while (*act && *act != ' ') act++;
            while (*act == ' ') act++;
            cmd_act(uid, act);
        }
        else if (!strcasecmp(cmd, "contract")) {
            int k, a, b;
            if (sscanf(rest, "%d %d %d", &k, &a, &b) == 3 && k >= 1 && k <= 7)
                ks_contract_sign(w, k, a, b);
            else ks_log(w, "格式: contract <1-7> <立约人id> <签约人id>");
        }
        else if (!strcasecmp(cmd, "battle")) {
            if (!strcasecmp(rest, "start")) {
                ks_battle_cfg_t cfg = { .width = 4, .day = w->day, .leyline = 1 };
                ks_battle_start(w, &cfg);
            } else if (!strcasecmp(rest, "step")) {
                ks_battle_tick(w);
            } else if (!strcasecmp(rest, "print")) {
                ks_battle_print(w);
            } else ks_log(w, "battle start|step|print");
        }
        else if (!strcasecmp(cmd, "audit")) {
            /* 运行时审计:列出无效果行的资源 */
            int n0 = 0, n1 = 0, n2 = 0;
            for (int i = 0; i < w->res_count; i++) {
                ks_res_t *r = &w->res[i];
                if (r->effect_count == 0) { n0++; printf("  [无效果] %d %s[%s]\n", r->id, r->name, ks_rank_name(r->rank)); }
                else if (r->effect_count == 1) n1++;
                else n2++;
            }
            ks_log(w, "总资源 %d | 无效果行 %d | 1行效果 %d | 多行效果 %d",
                   w->res_count, n0, n1, n2);
        }
        else if (!strcasecmp(cmd, "roam")) {
            int id = atoi(rest);
            if (id >= 1 && id <= w->unit_count) {
                w->units[id - 1].roaming = 1;
                ks_log(w, "%s 进入[游荡状态]", w->units[id - 1].name);
            } else ks_log(w, "用法: roam <单位id>");
        }
        else if (!strcasecmp(cmd, "status")) {
            int id = atoi(rest);
            if (id >= 1 && id <= w->unit_count) ks_status_print_unit(w, id);
            else ks_log(w, "用法: status <单位id>");
        }
        else if (!strcasecmp(cmd, "give")) {
            int id; char rname[64] = "";
            if (sscanf(rest, "%d %63s", &id, rname) == 2) {
                int rid = ks_world_find_res(w, rname);
                if (id >= 1 && id <= w->unit_count && rid) {
                    ks_unit_add_res(w, id, rid);
                    ks_log(w, "%s 获得 %s", w->units[id - 1].name, rname);
                } else ks_log(w, "give <单位id> <资源名>");
            } else ks_log(w, "give <单位id> <资源名>");
        }
        else if (!strcasecmp(cmd, "round")) {
            if (!strcasecmp(rest, "start")) ks_world_round_start(w);
            else if (!strcasecmp(rest, "end")) ks_world_round_end(w);
            else ks_log(w, "round start|end");
        }
        else if (!strcasecmp(cmd, "seed")) {
            ks_rng_seed(w, (unsigned)atoi(rest));
            ks_log(w, "随机种子已设定");
        }
        else if (!strcasecmp(cmd, "world")) ks_world_print(w);
        else if (!strcasecmp(cmd, "demo")) demo_simple(w);
        else if (cmd[0]) ks_log(w, "未知命令: %s(输入 help)", cmd);
    }
}

int main(int argc, char **argv)
{
    ks_console_utf8();
    printf("空想圣杯引擎 (KsG) v1.1 — 五书重构版\n");
    printf("《空想圣杯规则书》《空想从者资源库》《空想御主资源库》\n");
    printf("《空想礼装资源书》《空想工房建造书》\n\n");

    ks_world_init(&g_w, (unsigned)time(NULL));
    g_w.verbosity = 1;

    if (argc > 1) {
        if (!strcasecmp(argv[1], "demo")) {
            demo_simple(&g_w);
            return 0;
        }
        if (!strcasecmp(argv[1], "shell")) {
            preset_world(&g_w);
            ks_shell(&g_w);
            return 0;
        }
        if (!strcasecmp(argv[1], "duel")) {
            demo_simple(&g_w);
            printf("\n预设单位已就绪(1~4),开始对战:\n");
            ks_ui_duel(&g_w);
            return 0;
        }
        if (!strcasecmp(argv[1], "quiet")) g_w.verbosity = 0;
    }
    preset_world(&g_w);
    ks_ui_main(&g_w);
    return 0;
}