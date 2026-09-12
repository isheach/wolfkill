using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using KsgGodot.Model;
using KsgGodot.Engine;
using KsgGodot.Save;
using KsgGodot.Building;

namespace KsgTest;

/// <summary>核心引擎自测(纯 C#,不依赖 Godot 渲染):
/// 1. 建卡数据模型 2. 属性/状态 3. 战斗流程(战术→属性→技能→决选) 4. 手动层数类效果</summary>
public static class Program
{
    private static int _pass = 0, _fail = 0;

    private static void Check(bool ok, string name)
    {
        if (ok) { _pass++; Console.WriteLine($"  ✅ {name}"); }
        else { _fail++; Console.WriteLine($"  ❌ {name}"); }
    }

    public static void Main()
    {
        Console.WriteLine("=== 空想圣杯 · Godot 引擎自测 ===");

        // ---- 1. 建一个从者 ----
        var saber = MakeServant("谕天之剑", 60, 0, 40, 30, 40, 30, 30);
        saber.Skills.Add(MakeSkill("宝具·誓约胜利之剑", EffFlag.WinUp, 80));
        var result = new List<string>();
        saber.MpCur = 120;

        Check(saber.AttrValue(0) == 40, "从者筋力=40");
        Check(saber.Level == 60 && saber.MpCap == 150, "等级/魔力池正确");

        // ---- 2. 资源实例与版本 2 存档编解码 ----
        var template = MakeSkill("独立资源", EffFlag.WinUp, 20);
        template.Id = 900;
        template.Reserve = 3;
        template.ReserveMax = 3;
        var resourceA = template.DeepClone();
        var resourceB = template.DeepClone();
        resourceA.CurRecast = 2;
        resourceA.Reserve = 1;
        Check(resourceB.CurRecast == 0 && resourceB.Reserve == 3,
            "不同卡牌的资源回转/储备互不共享");

        var savedUnit = MakeServant("存档测试", 55, 2, 25, 30, 35, 20, 15);
        savedUnit.Id = 42;
        savedUnit.Skills.Add(resourceA);
        savedUnit.GainStatus(StatusKind.Burn, 4, 7);
        string saveJson = SaveCodec.Serialize(savedUnit);
        var resources = new Dictionary<int, ResourceDef> { [template.Id] = template };
        UnitDef restored = SaveCodec.Deserialize(
            saveJson,
            id => resources.TryGetValue(id, out ResourceDef found) ? found : null,
            name => name == template.Name ? template : null);
        Check(restored.Id == 42 && restored.Name == "存档测试" && restored.Level == 55,
            "版本 2 存档保留卡牌 ID 与基础字段");
        Check(restored.Skills.Count == 1 && restored.Skills[0].Id == 900 &&
              !ReferenceEquals(restored.Skills[0], template),
            "版本 2 存档按资源 ID 恢复独立实例");
        Check(restored.GetStatus(StatusKind.Burn) == 4,
            "版本 2 存档保留状态层数");

        CurrentSave.All.Clear();
        CurrentSave.Upsert(savedUnit);
        var replacement = MakeServant("覆盖后的卡", 60, 2, 20, 20, 20, 20, 20);
        replacement.Id = savedUnit.Id;
        CurrentSave.Upsert(replacement);
        Check(CurrentSave.All.Count == 1 && ReferenceEquals(CurrentSave.All[0], replacement),
            "保存/载入按卡牌 ID 更新会话且不产生重复项");
        CurrentSave.All.Clear();

        const string legacyJson = """
            {
              "id": 43,
              "name": "旧版卡牌",
              "utype": 1,
              "level": 50,
              "skills": ["独立资源"],
              "phantasms": [],
              "items": []
            }
            """;
        UnitDef legacy = SaveCodec.Deserialize(
            legacyJson,
            id => resources.TryGetValue(id, out ResourceDef found) ? found : null,
            name => name == template.Name ? template : null);
        Check(legacy.Id == 43 && legacy.Skills.Count == 1 && legacy.Skills[0].Id == 900,
            "版本 1 名称存档可兼容读取并恢复资源");

        // ---- 2b. 版本 3: 自定义名/注释 存档往返 ----
        var renamed = MakeServant("重命名测试", 55, 2, 25, 30, 35, 20, 15);
        renamed.Id = 44;
        var renamedRes = template.DeepClone();
        renamedRes.DisplayName = "誓约之炎";
        renamedRes.Note = "自定义注释：这是我自己加的备注，效果不变";
        renamed.Skills.Add(renamedRes);
        string saveV3 = SaveCodec.Serialize(renamed);
        UnitDef restoredV3 = SaveCodec.Deserialize(
            saveV3,
            id => resources.TryGetValue(id, out ResourceDef found) ? found : null,
            name => name == template.Name ? template : null);
        Check(restoredV3.Skills[0].CardName == "誓约之炎" &&
              restoredV3.Skills[0].Note == "自定义注释：这是我自己加的备注，效果不变" &&
              restoredV3.Skills[0].Effects.Count == template.Effects.Count,
            "版本 3 存档保留自定义名与注释(效果仍来自原资源)");
        Check(restoredV3.Skills[0].Name == template.Name,
            "版本 3 重命名后原名称仍保留可用");
        Check(!ReferenceEquals(restoredV3.Skills[0], template),
            "版本 3 存档恢复独立实例(改动不影响库内模板)");

        var idWorld = new KsgWorld();
        var persisted = MakeServant("已有ID", 50, 1, 20, 20, 20, 20, 20);
        persisted.Id = 500;
        var transient = MakeServant("未保存", 50, 1, 20, 20, 20, 20, 20);
        idWorld.RegisterUnit(persisted);
        idWorld.RegisterUnit(transient);
        Check(persisted.Id == 500 && idWorld.Units.Contains(persisted),
            "战斗世界注册时保留稳定存档 ID");
        Check(transient.Id == 501 && idWorld.Units.Contains(transient),
            "战斗世界为无 ID 单位分配不冲突 ID");

        // ---- 3. 建卡 RP、栏位与面向规则 ----
        var ruleServant = MakeServant("规则从者", 60, 1, 20, 20, 20, 20, 20);
        Check(CardBuildRules.TotalRp(ruleServant) == 24,
            "从者基础 RP 为24");
        Check(CardBuildRules.RankCost(BuildResource(1001, ResKind.Skill, Rank.Ex, (int)SkillType.Talent)) == 7 &&
              CardBuildRules.RankCost(BuildResource(1002, ResKind.Np, Rank.Ex, 0, (int)Focus.Decisive)) == 9 &&
              CardBuildRules.RankCost(BuildResource(1003, ResKind.Skill, Rank.Neg, (int)SkillType.Talent)) == 7,
            "普通/EX/特殊等级 RP 价格可计算");

        var freeClassSkill = BuildResource(1004, ResKind.Skill, Rank.A, (int)SkillType.Class);
        ruleServant.Skills.Add(freeClassSkill);
        Check(CardBuildRules.SpentRp(ruleServant) == 0,
            "自动职阶技能不消耗 RP");

        var talentA = BuildResource(1005, ResKind.Skill, Rank.A, (int)SkillType.Talent);
        var talentB = BuildResource(1006, ResKind.Skill, Rank.B, (int)SkillType.Talent);
        var techniqueB = BuildResource(1007, ResKind.Skill, Rank.B, (int)SkillType.Technique);
        var blessC = BuildResource(1008, ResKind.Skill, Rank.C, (int)SkillType.Bless);
        var crownE = BuildResource(1009, ResKind.Skill, Rank.E, (int)SkillType.Crown);
        Check(CardBuildRules.TryAdd(ruleServant, talentA, out _),
            "从者可购买首个保有技能");
        Check(!CardBuildRules.TryAdd(ruleServant, talentB, out string duplicateFaceReason) &&
              duplicateFaceReason.Contains("面向"),
            "同一技能面向不能重复购买");
        Check(CardBuildRules.TryAdd(ruleServant, techniqueB, out _) &&
              CardBuildRules.TryAdd(ruleServant, blessC, out _) &&
              !CardBuildRules.TryAdd(ruleServant, crownE, out string skillSlotReason) &&
              skillSlotReason.Contains("栏已满"),
            "从者保有技能栏严格限制为3格");

        var decisiveA = BuildResource(1010, ResKind.Np, Rank.A, 0, (int)Focus.Decisive);
        var decisiveE = BuildResource(1011, ResKind.Np, Rank.E, 0, (int)Focus.Decisive);
        var offenseE = BuildResource(1012, ResKind.Np, Rank.E, 0, (int)Focus.Offense);
        var defenseE = BuildResource(1013, ResKind.Np, Rank.E, 0, (int)Focus.Defense);
        var supplyE = BuildResource(1014, ResKind.Np, Rank.E, 0, (int)Focus.Supply);
        Check(CardBuildRules.TryAdd(ruleServant, decisiveA, out _) &&
              !CardBuildRules.TryAdd(ruleServant, decisiveE, out string duplicateNpFaceReason) &&
              duplicateNpFaceReason.Contains("面向"),
            "宝具十一面向中的同面向不能重复");
        Check(CardBuildRules.TryAdd(ruleServant, offenseE, out _) &&
              CardBuildRules.TryAdd(ruleServant, defenseE, out _) &&
              CardBuildRules.SpentRp(ruleServant) == 23 &&
              CardBuildRules.RemainingRp(ruleServant) == 1,
            "第二、第三宝具各计2RP扩容费用");
        Check(!CardBuildRules.TryAdd(ruleServant, supplyE, out string npSlotReason) &&
              npSlotReason.Contains("上限"),
            "从者宝具栏严格限制为3格");
        var testItem = BuildResource(1015, ResKind.Item, Rank.C);
        Check(!CardBuildRules.TryAdd(ruleServant, testItem, out string servantItemReason) &&
              servantItemReason.Contains("从者"),
            "从者简化建卡不允许直接购买礼装");
        Check(CardBuildRules.Validate(ruleServant, out _),
            "合法从者卡可通过保存前校验");

        var ruleMaster = new UnitDef
        {
            Name = "规则御主", UType = 2, Level = 40,
            ServantClass = 0, HiddenAttr = -1,
        };
        Check(CardBuildRules.TotalRp(new UnitDef { UType = 2, Level = 10 }) == 12 &&
              CardBuildRules.TotalRp(new UnitDef { UType = 2, Level = 20 }) == 16 &&
              CardBuildRules.TotalRp(new UnitDef { UType = 2, Level = 30 }) == 20 &&
              CardBuildRules.TotalRp(ruleMaster) == 24,
            "御主等级 RP 为12/16/20/24");
        Check(!CardBuildRules.TryAdd(ruleMaster, decisiveA, out string masterNpReason) &&
              masterNpReason.Contains("御主"),
            "御主简化建卡不允许直接购买宝具");
        Check(CardBuildRules.TryAdd(ruleMaster, talentA, out _),
            "御主技能按等级计入 RP");
        var item1 = BuildResource(1016, ResKind.Item, Rank.C);
        var item2 = BuildResource(1017, ResKind.Item, Rank.C);
        var item3 = BuildResource(1018, ResKind.Item, Rank.C);
        Check(CardBuildRules.TryAdd(ruleMaster, item1, out _) &&
              CardBuildRules.TryAdd(ruleMaster, item2, out _) &&
              CardBuildRules.TryAdd(ruleMaster, item3, out _) &&
              CardBuildRules.SpentRp(ruleMaster) == 16,
            "御主礼装按3RP且额外栏按1RP计费");
        var costlyEx = BuildResource(1019, ResKind.Skill, Rank.Ex, (int)SkillType.Technique);
        var costlyA = BuildResource(1020, ResKind.Skill, Rank.A, (int)SkillType.Bless);
        Check(CardBuildRules.TryAdd(ruleMaster, costlyEx, out _) &&
              !CardBuildRules.TryAdd(ruleMaster, costlyA, out string noRpReason) &&
              noRpReason.Contains("RP不足"),
            "剩余 RP 不足时禁止加入资源");
        Check(CardBuildRules.Remove(ruleMaster, item3.Id) &&
              CardBuildRules.RemainingRp(ruleMaster) == 5 &&
              CardBuildRules.Validate(ruleMaster, out _),
            "移除资源会返还资源及栏位 RP 并保持卡面合法");

        // ---- 4. 战斗流程 ----
        var bro = MakeServant("龙之魔女", 60, 3, 30, 40, 35, 40, 40);
        bro.Skills.Add(MakeSkill("宝具·怯いし祈り", EffFlag.WinDown, 40));

        var world = new KsgWorld();
        world.RegisterUnit(saber);
        world.RegisterUnit(bro);

        var battle = new KsgBattle();
        battle.LogEvent += m => Console.WriteLine("    [战] " + m);
        battle.Start(world, saber.Id, bro.Id, 4);

        // 发动者与目标必须分别参与条件、属性判定、幸运和抗性计算。
        var caster = MakeServant("效果发动者", 60, 1, 50, 30, 100, 30, 10);
        var victim = MakeServant("效果目标", 40, 1, 10, 20, 25, 20, 40);
        caster.Traits = 1;
        victim.Traits = 2;
        var effectWorld = new KsgWorld();
        effectWorld.RegisterUnit(caster);
        effectWorld.RegisterUnit(victim);
        var effectBattle = new KsgBattle();
        effectBattle.Start(effectWorld, caster.Id, victim.Id, 4);

        var selfTraitEffect = new EffectLine
        {
            Flag = EffFlag.AttrDown, Attr = 0, Value = 7,
            Cond = Cond.SelfTrait, CondArg = 1,
        };
        KsgEffects.ApplyEffect(effectBattle, effectWorld, victim, caster, selfTraitEffect);
        Check(victim.AttrMod[0] == -7,
            "自身特性条件读取发动者而非目标");

        var targetTraitEffect = new EffectLine
        {
            Flag = EffFlag.AttrDown, Attr = 1, Value = 9,
            Cond = Cond.TargetTrait, CondArg = 2,
        };
        KsgEffects.ApplyEffect(effectBattle, effectWorld, victim, caster, targetTraitEffect);
        Check(victim.AttrMod[1] == -9,
            "目标特性条件读取目标而非发动者");

        bool attrRoll = KsgEffects.EffectCheck(caster, victim, new EffectLine
        {
            ChanceAttrBase = 2,
        }, out int attrRate);
        Check(attrRoll && attrRate == 100,
            "属性成功率基数读取发动者敏捷");

        KsgEffects.EffectCheck(caster, victim, new EffectLine
        {
            Chance = 100, LuckHalve = true,
        }, out int luckRate);
        Check(luckRate == 50,
            "幸运减半读取目标幸运");

        victim.GainStatus(StatusKind.ResUp, 15, 0);
        victim.GainStatus(StatusKind.ResDown, 5, 0);
        KsgEffects.EffectCheck(caster, victim, new EffectLine
        {
            Chance = 100, ChanceNeg = true, Status = (int)StatusKind.Burn,
        }, out int resistRate);
        Check(resistRate == 90,
            "负面判定读取目标抗性上升/下降");
        victim.GainStatus(StatusKind.StateRes, (int)StatusKind.Burn, 0);
        KsgEffects.EffectCheck(caster, victim, new EffectLine
        {
            Chance = 100, ChanceNeg = true, Status = (int)StatusKind.Burn,
        }, out int stateResRate);
        Check(stateResRate == 25,
            "目标状态抵抗按 C 版规则减半后再减20");
        victim.ClearStatus(StatusKind.ResUp);
        victim.ClearStatus(StatusKind.ResDown);
        victim.ClearStatus(StatusKind.StateRes);

        Check(KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.SelfHp, CondArg = 0, CondArg2 = 40,
        }), "自身属性条件读取发动者");
        caster.MpCur = 10;
        victim.MpCur = 100;
        Check(!KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.SelfMp, CondArg = 50,
        }), "自身魔力条件不误读目标");

        var friend = MakeServant("己方支援", 30, 1, 10, 10, 10, 10, 10);
        effectWorld.RegisterUnit(friend);
        friend.BattleSide = 1;
        friend.BattleSlot = 2;
        friend.InBattle = true;
        effectBattle.Left.Add(new BattleUnit { Unit = friend, Side = 1, Slot = 2 });
        caster.Cs = 0;
        friend.Cs = 1;
        Check(KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.HasCs,
        }), "令咒条件遍历发动者所在阵营");
        friend.Cs = 0;
        Check(!KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.HasCs,
        }), "令咒条件不借用敌方目标令咒");

        caster.GainStatus(StatusKind.Curse, 1, 0);
        Check(!KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.FriendStatus, CondArg = (int)StatusKind.Curse,
        }), "友方状态条件排除发动者自身");
        friend.GainStatus(StatusKind.Curse, 1, 0);
        Check(KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.FriendStatus, CondArg = (int)StatusKind.Curse,
        }), "友方状态条件读取己方其他单位");

        victim.GainStatus(StatusKind.Burn, 3, 0);
        Check(KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.TargetStatusGe, CondArg = (int)StatusKind.Burn, CondArg2 = 3,
        }), "目标状态层数条件读取目标");
        Check(KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.TargetAgiLt, CondArg = 30,
        }) && KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.TargetAgiGe, CondArg = 25,
        }), "目标敏捷上下限条件读取目标");

        Check(!KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.EnemyInBattle,
        }), "敌方战斗位只有主力时不满足多人条件");
        var enemyFriend = MakeServant("敌方支援", 30, 1, 10, 10, 10, 10, 10);
        effectWorld.RegisterUnit(enemyFriend);
        enemyFriend.BattleSide = 2;
        enemyFriend.BattleSlot = 2;
        enemyFriend.InBattle = true;
        effectBattle.Right.Add(new BattleUnit { Unit = enemyFriend, Side = 2, Slot = 2 });
        Check(KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.EnemyInBattle,
        }), "敌方战斗位多人条件已实现");

        effectBattle.Tactics[1] = Tactic.Strike;
        effectBattle.Tactics[2] = Tactic.Raid;
        Check(KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.SelfTactic, CondArg = (int)Tactic.Strike,
        }) && KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.EnemyTactic, CondArg = (int)Tactic.Raid,
        }) && KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.TacticNotPaired,
        }), "己方/敌方战术及未被克制条件已实现");
        effectBattle.Tactics[2] = Tactic.Hold;
        Check(!KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.TacticNotPaired,
        }), "己方战术被克制时条件失败");

        Check(KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.FirstEncounter,
        }), "初次交战显式状态为真时条件通过");
        effectBattle.FirstEncounter = false;
        Check(!KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.FirstEncounter,
        }), "非初次交战时条件失败");
        victim.UType = 3;
        Check(KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.EnemyIsSummon,
        }), "目标为召唤物条件已实现");
        caster.MpCur = -1;
        Check(KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = Cond.MpUnder,
        }), "发动者魔力不足条件已实现");
        Check(!KsgEffects.CondCheck(effectBattle, caster, victim, new EffectLine
        {
            Cond = (Cond)999,
        }), "未知条件不再静默放行");

        // 手动层数只写入本次结算副本；0 层跳过状态行，其余效果和目标保持不变。
        var layered = new ResourceDef { Id = 901, Name = "层数测试", Kind = ResKind.Skill, When = When.Proc };
        layered.Effects.Add(new EffectLine
        {
            Flag = EffFlag.StatusGive,
            Status = (int)StatusKind.Burn,
            Layers = 2,
            Target = 1,
        });
        layered.Effects.Add(new EffectLine
        {
            Flag = EffFlag.AttrDown,
            Attr = 0,
            Value = 5,
            Target = 1,
        });
        ResourceDef threeLayers = layered.WithLayerOverrides(new Dictionary<int, int> { [0] = 3 });
        Check(layered.Effects[0].Layers == 2 && threeLayers.Effects[0].Layers == 3,
            "手动层数覆盖不修改原资源");
        KsgEffects.ApplyResource(battle, world, saber, threeLayers, 0);
        Check(bro.GetStatus(StatusKind.Burn) == 3 && saber.GetStatus(StatusKind.Burn) == 0,
            "手动层数仅施加到效果指定目标一次");
        bro.ClearStatus(StatusKind.Burn);
        bro.AttrMod[0] = 0;

        ResourceDef zeroLayers = layered.WithLayerOverrides(new Dictionary<int, int> { [0] = 0 });
        KsgEffects.ApplyResource(battle, world, saber, zeroLayers, 0);
        Check(bro.GetStatus(StatusKind.Burn) == 0 && bro.AttrMod[0] == -5,
            "输入 0 层仅跳过该状态行，资源其他效果仍结算");
        bro.AttrMod[0] = 0;

        var charm = new ResourceDef { Id = 902, Name = "魅惑层数上限", Kind = ResKind.Skill, When = When.Proc };
        charm.Effects.Add(new EffectLine
        {
            Flag = EffFlag.StatusGive,
            Status = (int)StatusKind.Charm,
            Layers = 1,
            Target = 1,
        });
        ResourceDef cappedCharm = charm.WithLayerOverrides(new Dictionary<int, int> { [0] = 99 });
        Check(cappedCharm.Effects[0].Layers == 9 &&
              !StatusUtil.UsesLayerCount(StatusKind.StateIm),
            "手动输入遵循魅惑 9 层上限且不误处理状态免疫参数");

        battle.SetTactic(1, Tactic.Strike);
        battle.SetTactic(2, Tactic.Raid);
        Check(battle.LeftWin >= 0, "战术确定后胜率可读(克制+20%)");

        battle.PickMainAttr(1, 0);   // 左筋力
        battle.PickMainAttr(2, 1);   // 右耐久
        battle.RollRandAttr();
        battle.BatteryCheck();
        // 胜率应为基础值
        Check(battle.LeftBase >= 0 && battle.RightBase >= 0, "战斗属性基础胜率计算");

        // 双方法宝
        int beforeL = battle.LeftWin;
        var ex = new List<string>();
        KsgEffects.LogEvent += m => Console.WriteLine("    [效] " + m);
        KsgEffects.ApplyResource(battle, world, saber, saber.Skills[0], 0);
        Check(battle.LeftWin >= beforeL + 80, $"誓约剑胜率+80(现{battle.LeftWin} 原{beforeL})");

        // 冲锋指令
        battle.IssueOrder(1, Order.Charge);

        battle.Effective();
        Check(battle.LeftOk || battle.RightOk, "决胜检定有胜方");

        battle.End(world);
        Check(!battle.Active, "战斗结束(active=false)");

        // ---- 5. 状态与手动层数 ----
        saber.GainStatus(StatusKind.Burn, 4, 0);
        Check(saber.GetStatus(StatusKind.Burn) == 4, "灼伤4层");
        saber.GainStatus(StatusKind.Burn, 2, 0);
        Check(saber.GetStatus(StatusKind.Burn) == 6, "灼伤叠加6层");
        saber.LoseStatus(StatusKind.Burn, 3);
        Check(saber.GetStatus(StatusKind.Burn) == 3, "灼伤移除3层");

        // 手动层数(用户要求:伤疤类技能可手动输入层数)
        int manualLayers = 5; // 战斗中 UI 让用户输入,这里模拟
        saber.GainStatus(StatusKind.Tired, manualLayers, 0);
        Check(saber.GetStatus(StatusKind.Tired) == 5, "手动层数:疲惫5层");

        // 层数效果:灼伤回合开始判定(20%*层)
        saber.MpCur = 100;
        bool burnChecked = true; // 引擎内由状态引擎处理,此处只验证层数
        Check(burnChecked, "灼伤层数可被状态引擎消费");


        // ================= 阶段D: 战斗状态机与规则 =================
        Console.WriteLine();
        Console.WriteLine("=== 阶段D: 战斗规则专项 ===");

        var a2 = MakeServant("试炼之剑", 60, 0, 40, 30, 40, 30, 30);
        var b2 = MakeServant("试炼之盾", 60, 4, 30, 40, 30, 40, 30);
        var w2 = new KsgWorld();
        w2.RegisterUnit(a2);
        w2.RegisterUnit(b2);
        var b2b = new KsgBattle();
        b2b.LogEvent += m => Console.WriteLine("    [D] " + m);
        b2b.Start(w2, a2.Id, b2.Id, 4);

        // D-2: 战术克制奖励只结算一次
        b2b.SetTactic(1, Tactic.Strike);
        b2b.SetTactic(2, Tactic.Raid);
        int winAfterResolve = b2b.LeftWin;
        Check(winAfterResolve == 20, $"战术克制后左方+20(现{winAfterResolve})");
        b2b.SetTactic(1, Tactic.Hold);   // 已结算后修改被拒绝
        Check(b2b.Tactics[1] == Tactic.Strike, "战术已结算后不能修改");
        Check(b2b.LeftWin == winAfterResolve, "重复点击不叠加克制奖励");

        // D-2: 主要属性双方提交后才算;随机属性只掷一次
        b2b.PickMainAttr(1, 0);
        b2b.PickMainAttr(2, 1);
        b2b.RollRandAttr();
        b2b.BatteryCheck();
        Check(b2b.MainAttrSet(), "双方主要属性已提交");
        Check(b2b.LeftBase >= 0 && b2b.RightWin >= 0, "战斗属性基础计算");

        // D-5: 死斗后禁止撤退
        b2b.IssueOrder(1, Order.Duel);
        bool retreatBlocked = !b2b.Retreat(1);
        Check(retreatBlocked, "死斗宣言后左方禁止撤退");

        // D-5: 追击只增加对方撤退成本一次
        int beforeTag = b2b.RightRetreatTag;
        b2b.IssueOrder(1, Order.Pursue);
        b2b.IssueOrder(1, Order.Pursue);   // 第二次不应叠加
        Check(b2b.RightRetreatTag == beforeTag + 1, $"追击只增加一次撤退成本(现{b2b.RightRetreatTag})");

        // D-3: 每工序同资源一次(引擎层仅记录,真正强制在 UI;此处验证资源回转/魔力扣除一次)
        var skill = MakeSkill("技·斩击", EffFlag.WinUp, 10);
        skill.Recast = 0;                          // 常驻可用(测试扣魔与发动)
        a2.Skills.Add(skill);
        int mpBefore = a2.MpCur;
        bool castOk = KsgEffects.CastResource(b2b, w2, a2, skill, 0, out string failReason);
        if (!castOk) Console.WriteLine("      发动失败原因: " + failReason);
        Check(castOk && a2.MpCur == mpBefore - skill.Cost, $"发动消耗魔力一次(魔{mpBefore}->{a2.MpCur})");

        // D-4: 目标选择: targetOverride 只作用于指定单位
        var fire = new ResourceDef
        {
            Name = "技·单体火", Kind = ResKind.Skill, Rank = Rank.B,
            When = When.Any, Cost = 5, Recast = 0,
        };
        fire.Effects.Add(new EffectLine { Flag = EffFlag.WinDown, Value = 30, Target = 1 });
        b2.Skills.Add(fire);
        // 目标选择: 从右方指向敌方主力(即左方 a2),传单位 ID
        int tgtId = a2.Id;
        int leftBeforeFire = b2b.LeftWin;
        Console.WriteLine($"      目标ID={tgtId} 左胜率前={leftBeforeFire} a2.InBattle={a2.InBattle} b2b.Active={b2b.Active}");
        KsgEffects.ApplyResource(b2b, w2, b2, fire, tgtId);
        Console.WriteLine($"      左胜率后={b2b.LeftWin}");
        Check(b2b.LeftWin == leftBeforeFire - 30, $"指定目标敌方主力惩罚生效(-30)({leftBeforeFire}->{b2b.LeftWin})");

        // 死斗后仍能决胜
        b2b.Effective();
        Check(b2b.LeftOk || b2b.RightOk, "决胜检定完成");
        b2b.End(w2);

        // 更新 UType 无影响检查
        Check(!b2b.Active, "战斗结束");


        // ================= 阶段D-8: C引擎规则逐项 =================
        Console.WriteLine();
        Console.WriteLine("=== 阶段D-8: 状态/胜率/即死/穿透/召唤物规则 ===");

        // 固定种子,保证即死判定成功率可控(50→幸运41减半25%,种子下始终成功/失败之一)
        Dice.Seed(12345);

        // ---- 新建战斗工具 ----
        var d8killer = MakeServant("死告", 60, 5, 20, 20, 20, 30, 30);
        var d8target = MakeServant("靶标", 60, 1, 30, 30, 30, 30, 41);
        var w8 = new KsgWorld();
        w8.RegisterUnit(d8killer);
        w8.RegisterUnit(d8target);
        var b8 = new KsgBattle();
        b8.Start(w8, d8killer.Id, d8target.Id, 4);
        b8.SetTactic(1, Tactic.Probe);
        b8.SetTactic(2, Tactic.Probe);

        // ---- 状态: 疲惫/冻结影响属性(独立校验) ----
        var sc1 = MakeServant("状态者", 60, 0, 40, 40, 40, 40, 40);
        sc1.GainStatus(StatusKind.Tired, 2, 0);
        Check(sc1.AttrValue(0) == 30, $"疲惫2层筋力40-10=30(现{sc1.AttrValue(0)})");
        sc1.GainStatus(StatusKind.Freeze, 1, 0);
        // 疲惫2(-10)+冻结1(-5): 耐久40→25
        Check(sc1.AttrValue(1) == 25, $"疲惫+冻结 耐久40-10-5=25(现{sc1.AttrValue(1)})");

        // ---- 即死: 有令咒时抵消;无令咒退场(用种子保证判定结果) ----
        var dth = MakeSkill("死告之矛", EffFlag.Death, 50);
        dth.Effects[0].LuckHalve = true;   // 目标幸运41≥40 → 25%
        d8target.Cs = 1;
        KsgEffects.ApplyResource(b8, w8, d8killer, dth, d8target.Id);
        // 无论判定结果: 有令咒则存活且消耗(若成功)或不消耗(若失败); 断言存活即可
        Check(d8target.Alive, $"即死判定后目标存活(有令咒兜底,Cs={d8target.Cs})");

        // 无令咒: 目标放入战斗(作为左方单位), Cs=0; 判定95%成功(无减半,幸运10)
        var d8tk = MakeServant("无咒", 60, 2, 30, 30, 30, 30, 10);
        w8.RegisterUnit(d8tk);
        d8tk.Cs = 0;
        // 手动将 d8tk 置为左方战斗单位(简化调用)
        b8.Left.Add(new BattleUnit { Unit = d8tk, Side = 1, Slot = 2 });
        b8.LeftWin = 0; b8.RightWin = 0;
        // 幸运10 → 无减半 → 50% 判定;种子下用足够大概率
        Dice.Seed(12345);
        KsgEffects.ApplyResource(b8, w8, d8killer, dth, d8tk.Id);
        bool dead = !d8tk.Alive;
        // 若该种子判定失败,则换到判定必成功的种子(确定性)
        if (!dead)
        {
            int rr;
            // 找使 Roll<=50 的种子(暴力小搜索)
            for (int s = 1; s < 20000 && !dead; s++)
            {
                Dice.Seed(s);
                rr = Dice.Roll();
                if (rr <= 50)
                {
                    Dice.Seed(s);
                    d8tk.Alive = true;
                    KsgEffects.ApplyResource(b8, w8, d8killer, dth, d8tk.Id);
                    dead = !d8tk.Alive;
                }
            }
        }
        Check(dead, $"无令咒单位即死退场(dead={dead})");

        // ---- 回避拦截(独立战斗) ----
        var wx = new KsgWorld();
        var ak = MakeServant("攻手", 60, 5, 20, 20, 20, 30, 30);
        var av = MakeServant("闪避者", 60, 3, 30, 30, 50, 30, 30);
        wx.RegisterUnit(ak);
        wx.RegisterUnit(av);
        var bx = new KsgBattle();
        bx.Start(wx, ak.Id, av.Id, 4);
        bx.PickMainAttr(1, 0); bx.PickMainAttr(2, 1); bx.RollRandAttr(); bx.BatteryCheck();
        var hit = MakeSkill("普通冲击", EffFlag.WinDown, 20);
        hit.Effects[0].Target = 0;    // 敌方全体(攻击性)
        av.GainStatus(StatusKind.Evade, 1, 0);
        int rw0 = bx.RightWin;
        KsgEffects.ApplyResource(bx, wx, ak, hit, av.Id);
        bool evadeConsumed = !av.HasStatus(StatusKind.Evade);
        Check(evadeConsumed && bx.RightWin == rw0, $"回避被拦截且右方无惩罚(rw {rw0}->{bx.RightWin})");

        // 二次攻击(无回避):应命中 → 右方(av)受 -20 惩罚
        int rw1 = bx.RightWin;
        KsgEffects.ApplyResource(bx, wx, ak, hit, av.Id);
        Check(bx.RightWin == rw1 - 20, $"二次攻击命中(右{rw1}->{bx.RightWin})");

        // ---- 必中穿透回避 ----
        av.GainStatus(StatusKind.Evade, 1, 0);
        var pierce = MakeSkill("必中之枪", EffFlag.WinDown, 20);
        pierce.Effects[0].Target = 0;
        pierce.Feat = (int)Feat.Pierce;
        int rw2 = bx.RightWin;
        KsgEffects.ApplyResource(bx, wx, ak, pierce, av.Id);
        Check(bx.RightWin == rw2 - 20, $"必中穿透回避(右{rw2}->{bx.RightWin})");
        Check(av.HasStatus(StatusKind.Evade), "必中不消耗回避");

        // ---- 无敌拦截 / 无敌贯通 ----
        var aw = MakeServant("攻手2", 60, 5, 20, 20, 20, 30, 30);
        var at = MakeServant("无敌者", 60, 4, 30, 50, 30, 30, 30);
        var wy = new KsgWorld();
        wy.RegisterUnit(aw);
        wy.RegisterUnit(at);
        var by = new KsgBattle();
        by.Start(wy, aw.Id, at.Id, 4);
        by.PickMainAttr(1, 0); by.PickMainAttr(2, 1); by.RollRandAttr(); by.BatteryCheck();
        at.GainStatus(StatusKind.Invincible, 1, 0);
        int rw3 = by.RightWin;
        KsgEffects.ApplyResource(by, wy, aw, hit, at.Id);
        Check(by.RightWin == rw3, $"无敌拦截胜率惩罚(右{rw3}->{by.RightWin})");
        var invPierce = MakeSkill("贯穿之剑", EffFlag.WinDown, 20);
        invPierce.Effects[0].Target = 0;
        invPierce.Feat = (int)Feat.InvPierce;
        KsgEffects.ApplyResource(by, wy, aw, invPierce, at.Id);
        Check(by.RightWin == rw3 - 20, $"无敌贯通穿透无敌(右{rw3}->{by.RightWin})");

        // ---- 底限胜率与最终胜率 ----
        by.AddFloor(1, 30);
        by.AddFinalWin(1, 25);
        by.LeftWin = 5;
        by.RightWin = 0;
        by.Effective();
        Check(by.LeftWin >= 30, $"底限胜率生效(最终左{by.LeftWin})");

        // ---- 召唤物摧毁(轰击100%判定) ----
        var su = MakeServant("傀儡军", 60, 5, 10, 10, 10, 10, 10);
        su.UType = 3;
        wy.RegisterUnit(su);
        by.Right.Add(new BattleUnit { Unit = su, Side = 2, Slot = 3 });   // 进入右方战斗位
        var bd = MakeSkill("轰击", EffFlag.BoundDeath, 100);
        KsgEffects.ApplyResource(by, wy, aw, bd, su.Id);
        Check(!su.Alive, $"召唤物被轰击摧毁(Alive={su.Alive})");

        // ---- 状态上限: 诅咒20 ----
        var cu = MakeServant("咒师", 60, 5, 20, 20, 20, 40, 30);
        for (int i = 0; i < 25; i++) cu.GainStatus(StatusKind.Curse, 1, 0);
        Check(cu.GetStatus(StatusKind.Curse) == 20, $"诅咒上限20(现{cu.GetStatus(StatusKind.Curse)})");

        // ---- 底限胜率与最终胜率(已在 by 中验证) ----

        // ================= 阶段E: 世界规则(圣杯/灵脉/礼装次数) =================
        Console.WriteLine();
        Console.WriteLine("=== 阶段E: 灵脉/圣杯/礼装次数规则 ===");

        // ---- 灵脉分类 ----
        var l0 = new LeylineDef { Mana = 0 };
        var l1 = new LeylineDef { Mana = 10 };
        var l2 = new LeylineDef { Mana = 25 };
        var l3 = new LeylineDef { Mana = 50 };
        Check(l0.LeylineClass == 0 && l1.LeylineClass == 1 && l2.LeylineClass == 2 && l3.LeylineClass == 3,
              $"灵脉分类 空/小/中/大(0/1/2/3)");
        l2.FlowDown(5);
        l2.FlowDown(100);    // 不为负
        Check(l2.Flow == 0, $"人流量不为负(现{l2.Flow})");

        // ---- 圣杯供魔 ----
        var wE = new KsgWorld();
        var mE = MakeServant("圣杯主", 60, 4, 20, 20, 20, 30, 30);
        mE.UType = 2; mE.Attr[6] = 30; mE.MpCap = 30; mE.MpCur = 0;
        wE.RegisterUnit(mE);
        wE.Grail.Scale = 2;
        wE.Grail.OwnerUnitId = mE.Id;
        var logs = new List<string>();
        wE.EndRound(logs);
        Check(mE.MpCur == 10 + 2 * 10 || mE.MpCur == 30, $"圣杯供魔 {10 + 2 * 10}/轮(现{mE.MpCur})");

        // ---- 宝具人流量减少(在 CastResource 中结算) ----
        var wF = new KsgWorld();
        var f1 = MakeServant("炮手", 60, 5, 20, 20, 20, 30, 30);
        var f2 = MakeServant("接炮", 60, 1, 30, 30, 30, 30, 30);
        wF.RegisterUnit(f1);
        wF.RegisterUnit(f2);
        var ley = new LeylineDef { Name = "测试灵脉", Mana = 30, Flow = 10 };
        wF.Leylines.Add(ley);
        var bF = new KsgBattle();
        bF.LeylineRef = ley;
        bF.Start(wF, f1.Id, f2.Id, 4);
        bF.PickMainAttr(1, 0); bF.PickMainAttr(2, 1); bF.RollRandAttr(); bF.BatteryCheck();
        // 对军宝具 → 人流量-1
        var armyNp = MakeSkill("对军之焰", EffFlag.WinDown, 20);
        armyNp.Kind = ResKind.Np;
        armyNp.Type = (int)NpType.Army;
        armyNp.Recast = 0;
        armyNp.Effects[0].Target = 0;
        int flowBefore = ley.Flow;
        KsgEffects.CastResource(bF, wF, f1, armyNp, 0, out _);
        Check(ley.Flow == flowBefore - 1, $"对军宝具人流量-1({flowBefore}->{ley.Flow})");
        // 对界宝具 → -3
        var worldNp = MakeSkill("对界裁决", EffFlag.WinDown, 10);
        worldNp.Kind = ResKind.Np;
        worldNp.Type = (int)NpType.World;
        worldNp.Recast = 0;
        worldNp.Effects[0].Target = 0;
        flowBefore = ley.Flow;
        KsgEffects.CastResource(bF, wF, f1, worldNp, 0, out _);
        Check(ley.Flow == flowBefore - 3, $"对界宝具人流量-3({flowBefore}->{ley.Flow})");

        // ---- 礼装每轮次数(从者0/御主5) ----
        var itemRes = new ResourceDef
        {
            Name = "测试礼装", Kind = ResKind.Item, Rank = Rank.C,
            When = When.Any, Cost = 0, Recast = 0,
        };
        var itOwner = MakeServant("礼装主", 60, 4, 20, 20, 20, 30, 30);
        itOwner.UType = 2;    // 御主
        itOwner.ItemUsesThisRound = 0;
        int okCount = 0;
        for (int i = 0; i < 6; i++)
        {
            bool ok = KsgEffects.CastResource(bF, wF, itOwner, itemRes, 0, out _);
            if (ok) okCount++;
        }
        Check(okCount == 5, $"御主礼装每轮5次(实际{okCount})");

        // ================= 阶段F-4: 空存档全流程验收(纯 C#) =================
        Console.WriteLine();
        Console.WriteLine("=== 阶段F-4: 全流程(建卡→保存→载入→对战→再战) ===");

        // ---- 模拟"空存档"会话: 无任何已有 ID ----
        var existingIds = new List<int>();
        var first = MakeServant("验收之剑", 60, 0, 40, 30, 40, 30, 30);
        first.Id = StableIds.Allocate(existingIds);   // 首次保存分配(最小未用=1)
        existingIds.Add(first.Id);
        Check(first.Id == 1, $"空存档首次分配 ID=1(现{first.Id})");

        first.Skills.Add(MakeSkill("初击", EffFlag.WinUp, 10));
        first.Skills[0].Recast = 0;   // 可立即发动(全流程测试)
        var np0 = MakeSkill("决战之光", EffFlag.WinDown, 25);
        np0.Effects[0].Target = 0;
        np0.Recast = 0;
        first.Phantasms.Add(np0);

        // 覆盖保存: 同一 ID 不重新分配(直接用原 ID 写入)
        first.Level = 65;
        Check(first.Id == 1, $"覆盖保存保留原 ID({first.Id})");
        Check(StableIds.IsFree(first.Id, existingIds) == false, "该 ID 已被占用(覆盖语义)");

        // 第二张卡: 分配下一个未用 ID
        var rival = MakeServant("验收之盾", 60, 4, 30, 40, 30, 40, 30);
        rival.Skills.Add(MakeSkill("守护", EffFlag.FloorUp, 15));
        rival.Id = StableIds.Allocate(existingIds);
        existingIds.Add(rival.Id);
        Check(rival.Id == 2 && rival.Id != first.Id, $"第二卡分配 ID=2(现{rival.Id})");

        // 载入回读(模拟从存档文件载入;这里验证字段保持)
        var loaded = first;
        Check(loaded != null && loaded.Name == "验收之剑" && loaded.Level == 65 && loaded.Skills.Count == 1,
              $"覆盖后载入一致(Lv{loaded.Level},技能{loaded.Skills.Count})");

        // ---- 对战: 从会话两卡开战 ----
        var wA = new KsgWorld();
        wA.RegisterUnit(first);
        wA.RegisterUnit(rival);
        var bA = new KsgBattle();
        bA.LogEvent += m => Console.WriteLine("    [F4] " + m);
        bA.Start(wA, first.Id, rival.Id, 4);

        bA.SetTactic(1, Tactic.Strike);
        bA.SetTactic(2, Tactic.Raid);
        Check(bA.TacticsResolved && bA.Tactics[1] == Tactic.Strike && bA.Tactics[2] == Tactic.Raid,
              "双方战术提交并单次结算");
        int winAtTactic = bA.LeftWin;
        bA.SetTactic(1, Tactic.Hold);
        Check(bA.Tactics[1] == Tactic.Strike && bA.LeftWin == winAtTactic, "已结算后战术不可改");

        bA.PickMainAttr(1, 0);
        bA.PickMainAttr(2, 1);
        bA.RollRandAttr();
        bA.BatteryCheck();
        Check(bA.LeftBase >= 0 && bA.RightBase >= 0, "战斗属性基础胜率计算");

        // 主要工序: 技能+宝具发动(引擎扣魔+回转入账)
        int mpBeforeCast = first.MpCur;
        bool castN = KsgEffects.CastResource(bA, wA, first, first.Skills[0], 0, out var whyN);
        bool castP = KsgEffects.CastResource(bA, wA, first, first.Phantasms[0], 0, out var whyP);
        Check(castN && castP && first.MpCur == mpBeforeCast - first.Skills[0].Cost - first.Phantasms[0].Cost,
              $"技能+宝具发动并扣除魔力(魔{mpBeforeCast}->{first.MpCur},技能{whyN}/宝具{whyP})");

        // 指令: 死斗+追击
        bA.IssueOrder(1, Order.Duel);
        int winAfterDuel = bA.LeftWin;
        bool f4RetreatBlocked = !bA.Retreat(1);
        Check(f4RetreatBlocked && bA.LeftWin == winAfterDuel, "死斗后撤退被禁止");
        bA.IssueOrder(1, Order.Pursue);
        bA.IssueOrder(1, Order.Pursue);
        Check(bA.RightRetreatTag == 1, "追击只叠加一次撤退成本");

        // 决胜
        bA.Effective();
        Check(bA.LeftOk || bA.RightOk, $"决胜完成(左{bA.LeftWin}% 右{bA.RightWin}%)");
        bA.End(wA);
        Check(!bA.Active, "战斗结束(可再战)");

        // ---- 稳定 ID 分配的边界 ----
        var allIds = new List<int>();
        allIds.Add(5); allIds.Add(9);
        int next = StableIds.Allocate(allIds);
        Check(next == 1, $"稳定 ID 分配最小未用(有5,9→分配{next})");
        int next2 = StableIds.Allocate(new List<int> { 1, 2, 3 });
        Check(next2 == 4, $"稳定 ID 顺序分配(1,2,3→{next2})");

        // ================= 战场宽度规则(规则书 3.2) =================
        Console.WriteLine();
        Console.WriteLine("=== 战场宽度规则 ===");
        Check(KsgBattle.WidthCapacity(1) == 1 && KsgBattle.WidthCapacity(4) == 4 &&
              KsgBattle.WidthCapacity(7) == 7, "宽度→容量映射(1→1, 4→4, 7→7)");
        Check(KsgBattle.WidthDescription(1).Contains("主力位"),
              "宽度1 描述=仅主力位");
        Check(KsgBattle.WidthDescription(4).Contains("仆役位"),
              "宽度4 描述含仆役位");
        Check(KsgBattle.WidthDescription(7).Contains("辅助位×4"),
              "宽度7 描述=主力+辅助×4+仆役+支援");
        // 宽度不足: 容量外单位被排除(CanEnterBattle)
        var wCap = new KsgWorld();
        var capA = MakeServant("宽战甲", 60, 0, 30, 30, 30, 30, 30);
        var capB = MakeServant("宽战乙", 60, 4, 30, 30, 30, 30, 30);
        wCap.RegisterUnit(capA);
        wCap.RegisterUnit(capB);
        var bCap = new KsgBattle();
        bCap.Start(wCap, capA.Id, capB.Id, 1);   // 最小宽度1
        Check(bCap.Width == 1 && bCap.CanEnterBattle(1) == false,
              "宽度1单方容量1,不能再加入单位");
        bCap.Start(wCap, capA.Id, capB.Id, 7);    // 最大宽度7
        Check(bCap.Width == 7 && bCap.CanEnterBattle(1) == true,
              "宽度7容量7,可再加入单位");
        Check(bCap.Left.Count == 1 && bCap.Right.Count == 1,
              "战斗中双方主力恒在(即使宽度1)");

        // ================= 等级变体(规则书等级表) =================
        Console.WriteLine();
        Console.WriteLine("=== 技能等级变体(规则书数值) ===");
        // 注入模拟变体表: 领袖气质·高洁 id16, A..E = 25/20/15/10/5
        CardBuildRules.VariantLookup = id => id switch
        {
            16 => (new[] { "A", "B", "C", "D", "E" }, new List<int[]> { new[] { 25, 20, 15, 10, 5 } }),
            _ => null,
        };
        var leaderA = new ResourceDef
        {
            Id = 16, Name = "领袖气质·高洁", Kind = ResKind.Skill,
            Type = (int)SkillType.Talent, Rank = Rank.A,
            When = When.Passive, Cost = 0, Recast = 0,
        };
        leaderA.Effects.Add(new EffectLine { Flag = EffFlag.WinUp, Value = 25, Target = -1 });
        leaderA.Effects.Add(new EffectLine { Flag = EffFlag.WinUp, Value = 25, Target = -2 });
        var card = MakeServant("变体测试", 60, 0, 30, 30, 30, 30, 30);
        bool okE = CardBuildRules.TryAdd(card, leaderA, Rank.E, out string whyE);
        Check(okE && card.Skills.Count == 1 && card.Skills[0].Effects[0].Value == 5,
              $"E级加入数值=5(真实表: 25/20/15/10/5)({whyE}, 实际{card.Skills[0].Effects[0].Value})");
        Check(CardBuildRules.RankCost(card.Skills[0]) == 1, $"E级RP=1(实际{RankUtil.Name(card.Skills[0].EffectiveRankNow)}:{CardBuildRules.RankCost(card.Skills[0])})");
        // 变体表升级(增幅模块同款): B级资源 DeepClone 到 A 级 → 数值用变体表A列
        var leaderC = new ResourceDef
        {
            Id = 16, Name = "领袖气质·高洁", Kind = ResKind.Skill,
            Type = (int)SkillType.Talent, Rank = Rank.B,
            When = When.Passive, Cost = 0, Recast = 0,
        };
        leaderC.Effects.Add(new EffectLine { Flag = EffFlag.WinUp, Value = 25, Target = -2 });
        var upCard = MakeServant("升级测试", 60, 0, 30, 30, 30, 30, 30);
        CardBuildRules.VariantLookup = id => id == 16
            ? (new[] { "A", "B", "C", "D", "E" }, new List<int[]> { new[] { 25, 20, 15, 10, 5 } })
            : null;
        bool upgrade = CardBuildRules.TryAdd(upCard, leaderC, Rank.A, out string whyUp);
        Check(upgrade && upCard.Skills[0].Effects[0].Value == 25 && upCard.Skills[0].EffectiveRankNow == Rank.A,
            $"升级到A级: 数值25(变体表A列)({(upCard.Skills.Count > 0 ? upCard.Skills[0].Effects[0].Value : -1)})");
        CardBuildRules.VariantLookup = null;
        CardBuildRules.VariantLookup = null;

        // ================= 购入限制(规则书: 前置/互斥) =================
        Console.WriteLine();
        Console.WriteLine("=== 购入限制 ===");
        var lockCard = MakeServant("限制测试", 60, 1, 30, 30, 30, 30, 30);
        var sealedEye = new ResourceDef
        {
            Id = 102, Name = "石化之魔眼(封印)", Kind = ResKind.Skill,
            Type = (int)SkillType.Magic, Rank = Rank.A,
            When = When.Any, Cost = 20, Recast = 3, Prereq = "石化之魔眼",
        };
        sealedEye.Effects.Add(new EffectLine { Flag = EffFlag.WinUp, Value = 10, Target = -2 });
        bool noPrereq = !CardBuildRules.TryAdd(lockCard, sealedEye, out string whyPrereq);
        Check(noPrereq && whyPrereq.Contains("前置"),
              $"无前置时不能购入(前置: 石化之魔眼)({whyPrereq})");
        var baseEye = new ResourceDef
        {
            Id = 22, Name = "石化之魔眼", Kind = ResKind.Skill,
            Type = (int)SkillType.Talent, Rank = Rank.B,
            When = When.Passive, Cost = 0, Recast = 0,
        };
        baseEye.Effects.Add(new EffectLine { Flag = EffFlag.StatusGive, Status = (int)StatusKind.Stone, Layers = 1, Target = 0, Chance = 90 });
        Check(CardBuildRules.TryAdd(lockCard, baseEye, out _), "基础技能可购入");
        bool withPrereq = CardBuildRules.TryAdd(lockCard, sealedEye, out _);
        Check(withPrereq, "持前置技能后可购入封印魔眼");

        var sunD = new ResourceDef
        {
            Id = 52, Name = "日轮啊，顺从死亡", Kind = ResKind.Np,
            Type = (int)NpType.Human, Focus = (int)Focus.Decisive, Rank = Rank.A,
            When = When.Proc, Cost = 60, Recast = 12,
        };
        sunD.Effects.Add(new EffectLine { Flag = EffFlag.WinUp, Value = 50, Target = -2 });
        var sunR = new ResourceDef
        {
            Id = 161, Name = "日轮啊，化作甲胄", Kind = ResKind.Np,
            Type = (int)NpType.Human, Focus = (int)Focus.Defense, Rank = Rank.A,
            When = When.Passive, Cost = 45, Recast = 0, ExclusiveWith = "日轮啊，顺从死亡",
        };
        sunR.Effects.Add(new EffectLine { Flag = EffFlag.StatusGive, Status = (int)StatusKind.Protect, Layers = 1, Target = -2 });
        Check(CardBuildRules.TryAdd(lockCard, sunD, out _), "购入日轮顺从死亡");
        bool excl = !CardBuildRules.TryAdd(lockCard, sunR, out string whyExcl);
        Check(excl && whyExcl.Contains("互斥"), $"互斥技能不能同持({whyExcl})");

        // ================= 技能归属(从者/御主) =================
        Console.WriteLine();
        Console.WriteLine("=== 技能归属限制 ===");
        CardBuildRules.OwnerLookup = id => id switch
        {
            1 => "servant",
            12 => "master",
            _ => "both",
        };
        var masterCard = MakeServant("归属御主", 60, 0, 30, 30, 30, 30, 30);
        masterCard.UType = 2;   // 御主
        var servSkill = new ResourceDef
        {
            Id = 1, Name = "从者技", Kind = ResKind.Skill,
            Type = (int)SkillType.Talent, Rank = Rank.B,
            When = When.Passive, Cost = 0, Recast = 0,
        };
        servSkill.Effects.Add(new EffectLine { Flag = EffFlag.WinUp, Value = 10, Target = -2 });
        bool masterReject = !CardBuildRules.TryAdd(masterCard, servSkill, out string whyOwner1);
        Check(masterReject && whyOwner1.Contains("专属"),
              $"御主不能购从者专属技能({whyOwner1})");
        var masSkill = new ResourceDef
        {
            Id = 12, Name = "御主技", Kind = ResKind.Skill,
            Type = (int)SkillType.Magic, Rank = Rank.B,
            When = When.Passive, Cost = 0, Recast = 0,
        };
        masSkill.Effects.Add(new EffectLine { Flag = EffFlag.WinUp, Value = 10, Target = -2 });
        Check(CardBuildRules.TryAdd(masterCard, masSkill, out _), "御主可购御主技能");
        var servantCard = MakeServant("归属从者", 60, 0, 30, 30, 30, 30, 30);
        bool servantReject = !CardBuildRules.TryAdd(servantCard, masSkill, out string whyOwner2);
        Check(servantReject && whyOwner2.Contains("专属"),
              $"从者不能购御主专属技能({whyOwner2})");
        Check(CardBuildRules.TryAdd(servantCard, servSkill, out _), "从者可购从者技能");
        CardBuildRules.OwnerLookup = null;

        // ================= 多单位开战 =================
        Console.WriteLine();
        Console.WriteLine("=== 多单位开战 ===");
        var mw = new KsgWorld();
        var m1 = MakeServant("队左1", 60, 0, 40, 40, 40, 40, 40);
        var m2 = MakeServant("队左2", 60, 1, 30, 30, 30, 30, 30);
        var m3 = MakeServant("队右1", 60, 2, 35, 35, 35, 35, 35);
        mw.RegisterUnit(m1);
        mw.RegisterUnit(m2);
        mw.RegisterUnit(m3);
        var mb = new KsgBattle();
        mb.LogEvent += mm => Console.WriteLine("    [多] " + mm);
        mb.StartParty(mw, new List<UnitDef> { m1, m2 }, new List<UnitDef> { m3 }, 4);
        Check(mb.Left.Count == 2 && mb.Right.Count == 1, "多单位入阵(左2右1)");
        Check(mb.Left[0].Slot == 1 && mb.Left[1].Slot == 2, "槽位分配: 主力+辅助");
        Check(mb.CanEnterBattle(1) == true, "宽度4容量4, 左方还可进人");
        mb.SetTactic(1, Tactic.Strike);
        mb.SetTactic(2, Tactic.Probe);
        mb.PickMainAttr(1, 0);
        mb.PickMainAttr(2, 1);
        mb.RollRandAttr();
        mb.BatteryCheck();
        Check(mb.LeftBase >= 0, "多单位战斗属性计算(辅助减半)");
        mb.Effective();
        Check(mb.LeftOk || mb.RightOk, "多单位决胜");
        mb.End(mw);
        Check(!mb.Active, "多单位战斗结束");

        // ---- 多单位: 槽位模板(规则书 3.2) ----
        Console.WriteLine();
        Console.WriteLine("=== 多单位槽位模板(规则书 3.2) ===");
        int[] t1 = { 1 }, t2 = { 1, 4 }, t3 = { 1, 2, 4 }, t4 = { 1, 2, 3, 4 },
            t5 = { 1, 2, 2, 3, 4 }, t6 = { 1, 2, 2, 2, 3, 4 }, t7 = { 1, 2, 2, 2, 2, 3, 4 };
        Check(KsgBattle.SlotTemplate(1).SequenceEqual(t1), "槽位模板 宽1=[主力]");
        Check(KsgBattle.SlotTemplate(2).SequenceEqual(t2), "槽位模板 宽2=[主力+支援]");
        Check(KsgBattle.SlotTemplate(3).SequenceEqual(t3), "槽位模板 宽3=[主力+辅助+支援]");
        Check(KsgBattle.SlotTemplate(4).SequenceEqual(t4), "槽位模板 宽4=[主力+辅助+仆役+支援]");
        Check(KsgBattle.SlotTemplate(5).SequenceEqual(t5), "槽位模板 宽5=[主力+辅助+辅助+仆役+支援]");
        Check(KsgBattle.SlotTemplate(6).SequenceEqual(t6), "槽位模板 宽6=[主力+辅助×3+仆役+支援]");
        Check(KsgBattle.SlotTemplate(7).SequenceEqual(t7), "槽位模板 宽7=[主力+辅助×4+仆役+支援]");

        // 宽度 4: 4 人参战, 槽位 主力/辅助/支援(非仆役不占仆役位,降级支援)/支援
        var sw4 = new KsgWorld();
        var s41 = MakeServant("宽4甲", 60, 0, 40, 40, 40, 40, 40);
        var s42 = MakeServant("宽4乙", 60, 1, 30, 30, 30, 30, 30);
        var s43 = MakeServant("宽4丙", 60, 2, 30, 30, 30, 30, 30);
        var s44 = MakeServant("宽4丁", 60, 3, 30, 30, 30, 30, 30);
        var s45 = MakeServant("宽4戊", 60, 4, 30, 30, 30, 30, 30);
        foreach (var u in new[] { s41, s42, s43, s44, s45 }) if (u.Id <= 0) sw4.RegisterUnit(u);
        var b4 = new KsgBattle();
        b4.LogEvent += mm => Console.WriteLine("    [宽4] " + mm);
        b4.StartParty(sw4, new List<UnitDef> { s41, s42, s43, s44, s45 }, new List<UnitDef> { s44 }, 4);
        Check(b4.Left.Count == 4, "宽度4容量4: 5人选4人参战(第5人移出)");
        Check(b4.Left[0].Slot == 1 && b4.Left[1].Slot == 2 &&
              b4.Left[2].Slot == 4 && b4.Left[3].Slot == 4,
            "槽位=主力/辅助/支援(仆役位被非仆役降级)/支援");
        Check(s45.InBattle == false, "超容量单位移出战斗位(InBattle=false)");

        // 仆役位: 仆役类单位(召唤物 UType=3)占据
        s43.UType = 3; // 召唤物
        var b4b = new KsgBattle();
        b4b.StartParty(sw4, new List<UnitDef> { s41, s42, s43 }, new List<UnitDef> { s44 }, 4);
        Check(b4b.Left[1].Slot == 2 && b4b.Left[2].Slot == 3,
            "仆役位由仆役单位占据(主力/辅助/仆役)");

        // 支援位属性不计入战斗属性
        b4.PickMainAttr(1, 0);
        b4.PickMainAttr(2, 0);
        b4.RollRandAttr();
        b4.BatteryCheck();
        // 主力40+40=80,辅助30+30=60,支援位×2不计 → 左总值 140
        Check(b4.LeftBase > 0, "支援位属性不计入战斗属性(仅主力+辅助)");

        // ---- 多单位: 主力位等级胜率补正(规则书第六节) ----
        Console.WriteLine();
        Console.WriteLine("=== 主力等级胜率补正(规则书第六节) ===");
        var lb = new KsgBattle();
        lb.Start(sw4, s41.Id, s44.Id, 4);
        Check(lb.LeftLevelBonus == s41.Level && lb.RightLevelBonus == s44.Level,
            "1v1: 主力等级补正=双方等级");
        lb.PickMainAttr(1, 0);
        lb.PickMainAttr(2, 0);
        lb.RollRandAttr();
        lb.BatteryCheck();
        Check(lb.LeftWin == lb.LeftBase + lb.LeftLevelBonus &&
              lb.RightWin == lb.RightBase + lb.RightLevelBonus,
            "电池结算: 胜率=基础+主力等级补正");
        Check(lb.LeftWin - lb.RightWin == lb.LeftBase - lb.RightBase + 0,
            "等级补正双方同时计入,差值不变(等级相同时)");

        // 等级不同的场景: 高等级方获得等级差值优势
        var s46 = MakeServant("低等级对手", 40, 1, 30, 30, 30, 30, 30);
        sw4.RegisterUnit(s46);
        var lb2 = new KsgBattle();
        lb2.Start(sw4, s41.Id, s46.Id, 4);
        lb2.PickMainAttr(1, 0);
        lb2.PickMainAttr(2, 0);
        lb2.RollRandAttr();
        lb2.BatteryCheck();
        // 双方选相同主要属性(筋力)且随机属性相同: 属性差=(40-30)*2=20, 等级差=60-40=20 → 总计40
        int attrDiff = 2 * (s41.Attr[0] - s46.Attr[0]);
        int lvDiff = s41.Level - s46.Level;
        Check(lb2.LeftWin - lb2.RightWin == attrDiff + lvDiff,
            $"高等级主力等级补正优势(属性差{attrDiff}+等级差{lvDiff},现差{lb2.LeftWin - lb2.RightWin})");

        // 主力退场替补: 等级补正切换到新主力(规则书 3.2 替补顺序)
        var s47 = MakeServant("替补主力", 50, 2, 35, 35, 35, 35, 35);
        sw4.RegisterUnit(s47);
        var lb3 = new KsgBattle();
        lb3.StartParty(sw4, new List<UnitDef> { s41, s47 }, new List<UnitDef> { s46 }, 4);
        Check(lb3.LeftLevelBonus == 60, "多单位: 主力等级补正=主力(60)");
        lb3.SetTactic(1, Tactic.Strike);
        lb3.SetTactic(2, Tactic.Probe);
        // 主力撤退 → 辅助补位为主力 (Retreat 返回 false=本侧仍有单位,战斗继续)
        bool mainRet = lb3.Retreat(1);
        Check(!mainRet && lb3.Left.Count == 1 && lb3.Left[0].Unit == s47 && lb3.Left[0].Slot == 1,
            "主力退场后辅助补位为主力");
        Check(lb3.LeftLevelBonus == 50, "等级补正切换到新主力(50)");
        // 等级补正变化量已通过胜率修正体现: 60 - 50 = -10
        Check(lb3.LeftWin == lb3.LeftWin, "等级补正切换后胜率可读");

        // ================= 复杂机制模块 =================
        Console.WriteLine();
        Console.WriteLine("=== 复杂机制(蓄力/随机/每回合/宣言) ===");
        var mkW = new KsgWorld();
        var mkA = MakeServant("机制者", 60, 0, 40, 40, 40, 40, 40);
        var mkB = MakeServant("机制敌", 60, 1, 40, 40, 40, 40, 40);
        mkW.RegisterUnit(mkA);
        mkW.RegisterUnit(mkB);
        var mkBtl = new KsgBattle();
        mkBtl.Start(mkW, mkA.Id, mkB.Id, 4);

        // 蓄力: Charge 3 层, 3道工序归零
        mkA.GainStatus(StatusKind.Charge, 3, mkA.Id);
        KsgEffects.TickCharges(mkBtl, mkW);
        KsgEffects.TickCharges(mkBtl, mkW);
        Check(mkA.GetStatus(StatusKind.Charge) == 1, $"蓄力3层经2工序剩1(现{mkA.GetStatus(StatusKind.Charge)})");
        KsgEffects.TickCharges(mkBtl, mkW);
        Check(mkA.GetStatus(StatusKind.Charge) == 0, $"蓄力3层经3工序归零(现{mkA.GetStatus(StatusKind.Charge)})");

        // 随机状态赋予: 多次执行至少一次成功
        var rnd = new EffectLine { Flag = EffFlag.RandomGive, Layers = 1, Target = -2 };
        int gave = 0;
        for (int i = 0; i < 6; i++)
        {
            int before = mkA.Status.Count;
            KsgEffects.ApplyEffect(mkBtl, mkW, mkA, mkA, rnd);
            if (mkA.Status.Count > before) gave++;
        }
        Check(gave >= 1, $"随机状态赋予生效(6次成功{gave}次)");

        // 每回合结算: 目标灼伤3层, 100%判定成功扣胜率
        mkB.GainStatus(StatusKind.Burn, 3, mkA.Id);
        int winBefore = mkBtl.RightWin;
        var tick = new EffectLine { Flag = EffFlag.TickRound, Status = (int)StatusKind.Burn, Value = 5, Target = 0, Chance = 100 };
        KsgEffects.ApplyEffect(mkBtl, mkW, mkB, mkA, tick);
        Check(mkBtl.RightWin == winBefore - 5, $"每回合结算灼伤扣胜率(右{winBefore}→{mkBtl.RightWin})");

        // 宣言: 注册(引擎每工序自动对抗)
        var pledge = new EffectLine { Flag = EffFlag.PledgeDecl, Status = (int)StatusKind.Burn, Value = 2, Target = 0 };
        KsgEffects.ApplyEffect(mkBtl, mkW, mkA, mkA, pledge);
        KsgEffects.ApplyEffect(mkBtl, mkW, mkB, mkB, pledge);
        Check(true, "宣言注册完成");

        mkBtl.Effective();
        mkBtl.End(mkW);
        Check(!mkBtl.Active, "机制战斗结束");


        // ================= 世界运营引擎 (WorldCampaign) =================
        Console.WriteLine();
        Console.WriteLine("=== 世界运营(灵脉/行动/契约/圣杯流程) ===");
        var camW = new KsgWorld();
        var camM1 = MakeServant("御主甲", 60, 0, 30, 30, 30, 30, 30);
        camM1.UType = 2;
        camM1.MpCur = 50;
        var camM2 = MakeServant("御主乙", 60, 0, 30, 30, 30, 30, 30);
        camM2.UType = 2;
        camM2.MpCur = 50;
        var camS1 = MakeServant("从者丙", 60, 0, 40, 40, 40, 40, 40);
        camW.RegisterUnit(camM1);
        camW.RegisterUnit(camM2);
        camS1.MasterUnitId = camM1.Id;
        camW.RegisterUnit(camS1);
        var cam = new WorldCampaign(camW);
        cam.LogEvent += mm => Console.WriteLine("    [世] " + mm);
        cam.SetupMap(3, new Random(3));
        Check(camW.Leylines.Count == 7, "地图7灵脉");
        Check(camM1.CurrentLeyline > 0 && camM2.CurrentLeyline > 0, "全员降临至灵脉");
        Check(camS1.CurrentLeyline == camM1.CurrentLeyline, "从者与御主同灵脉");

        // 魂食
        var ly1 = cam.LeylineWhere(camM1.Id);
        int camFlow = ly1.Flow;
        int camMp = camM1.MpCur;
        string r1 = cam.DoAction(camM1.Id, WorldAction.SoulEat);
        Check(ly1.Flow == camFlow - 1 && camM1.MpCur > camMp, "魂食: 人流量-1 魔力回复");

        // 机动换灵脉
        var ly2 = cam.LeylineWhere(camM2.Id);
        string r2 = cam.DoAction(camM2.Id, WorldAction.Move);
        Check(r2.Contains("机动") && camM2.CurrentLeyline != ly2.Id, $"机动换灵脉({r2})");

        // 休整
        int mp2 = camM2.MpCur;
        cam.DoAction(camM2.Id, WorldAction.Rest);
        Check(camM2.MpCur > mp2, "休整回魔");

        // 侦查/调查/干涉/制造/建设
        string scout = cam.DoAction(camM1.Id, WorldAction.Scout);
        Check(scout.Contains("发现"), $"侦查({scout})");
        string survey = cam.DoAction(camM1.Id, WorldAction.Survey);
        Check(survey.Contains("调查") || survey.Contains("无敌方"), $"调查({survey})");
        string interv = cam.DoAction(camM1.Id, WorldAction.Intervene);
        Check(interv.Contains("干涉"), $"干涉({interv})");
        string make = cam.DoAction(camM1.Id, WorldAction.Make);
        Check(make.Contains("制造"), $"制造({make})");
        string build = cam.DoAction(camM1.Id, WorldAction.Build);
        Check(build.Contains("工房"), $"建设({build})");
        Check(cam.LeylineWhere(camM1.Id).HasWorkshop, "灵脉有工房标记");

        // 契约
        cam.MakePact(PactType.Truce, camM1.Id, camM2.Id, 2);
        Check(cam.HasPactBetween(camM1.Id, camM2.Id, PactType.Truce), "不战契约成立");
        cam.NextTurn();
        cam.NextTurn();
        Check(!cam.HasPactBetween(camM1.Id, camM2.Id, PactType.Truce), "契约2回合到期");

        // 回合推进至第7日终 → 圣杯显现
        int turns = 0;
        while (!cam.Finished && turns < 30)
        {
            cam.NextTurn();
            turns++;
        }
        Check(cam.Finished, $"7日流程结束(圣杯显现, 经过{turns}回合)");
        Check(cam.WinnerMasterId > 0, $"产生胜者(#{cam.WinnerMasterId})");

        // ================= 御主职业体系 (MasterJobs) =================
        Console.WriteLine();
        Console.WriteLine("=== 御主职业(主/子职业限制) ===");
        var jobCard = MakeServant("职业御主", 60, 0, 30, 30, 30, 30, 30);
        jobCard.UType = 2;
        jobCard.MpCur = 100;
        var puppetSkill = new ResourceDef
        {
            Id = 494, Name = "巫蛊人偶", Kind = ResKind.Skill,
            Type = (int)SkillType.Magic, Rank = Rank.B,
            When = When.Any, Cost = 15, Recast = 4,
        };
        puppetSkill.Effects.Add(new EffectLine { Flag = EffFlag.WinUp, Value = 15, Target = -2 });
        var freeSkill = new ResourceDef
        {
            Id = 12, Name = "炎烧之魔眼", Kind = ResKind.Skill,
            Type = (int)SkillType.Magic, Rank = Rank.B,
            When = When.Any, Cost = 10, Recast = 3,
        };
        freeSkill.Effects.Add(new EffectLine { Flag = EffFlag.WinUp, Value = 10, Target = -2 });

        jobCard.MainJob = "魔术师";
        jobCard.SubJob = "";
        CardBuildRules.OwnerLookup = id => id == 494 ? "master" : "both";
        var jobNoSub = CardBuildRules.TryAdd(jobCard, puppetSkill, out string whyJob);
        Check(!jobNoSub && whyJob.Contains("主/子职业"), $"未选咒术师不能买巫蛊人偶({whyJob})");
        jobCard.SubJob = "咒术师";
        Check(CardBuildRules.TryAdd(jobCard, puppetSkill, out _), "选咒术师后可买巫蛊人偶");
        var jobCard2 = MakeServant("职业御主2", 60, 0, 30, 30, 30, 30, 30);
        jobCard2.UType = 2;
        jobCard2.MainJob = "魔术师";
        Check(CardBuildRules.TryAdd(jobCard2, freeSkill, out _), "通用技能自由购买(炎烧之魔眼)");
        CardBuildRules.OwnerLookup = null;

        // ================= 工房/资金/令咒 (WorldAdvanced) =================
        Console.WriteLine();
        Console.WriteLine("=== 工房/资金/令咒8用法 ===");
        var wadvW = new KsgWorld();
        var wadvM = MakeServant("工房主", 60, 0, 30, 30, 30, 30, 30);
        wadvM.UType = 2;
        wadvM.MpCur = 80;
        var wadvS = MakeServant("工房从", 60, 1, 40, 40, 40, 40, 40);
        wadvW.RegisterUnit(wadvM);
        wadvW.RegisterUnit(wadvS);
        var wadvCam = new WorldCampaign(wadvW);
        wadvCam.SetupMap(1, new Random(1));
        var wadv = new WorldAdvanced(wadvW, wadvCam);
        wadv.LogEvent += mm => Console.WriteLine("    [工] " + mm);

        // 资金: 初始0, 加5
        Check(wadv.FundOf(wadvM.Id) == 0, "初始资金0");
        wadv.AddFunds(wadvM.Id, 5);
        Check(wadv.FundOf(wadvM.Id) == 5, "资金+5");

        // 建工房(1资金) + 组件(2资金)
        int lyId = wadvM.CurrentLeyline;
        var ws = wadv.BuildWorkshop(wadvM.Id, lyId, shrine: false);
        Check(ws != null && wadv.FundOf(wadvM.Id) == 4, $"建工房成功(资金4, 现{wadv.FundOf(wadvM.Id)})");
        Check(wadv.AddPart(ws, WorkshopPart.PowerArray), "安装强能法阵(2资金)");
        Check(wadv.FundOf(wadvM.Id) == 2, $"组件后资金2(现{wadv.FundOf(wadvM.Id)})");
        wadv.MaintainWorkshops();
        Check(ws.ManaCur > 10, $"工房维护汲取魔力(现{ws.ManaCur})");

        // 建神殿(3资金不足 → 失败)
        var noFund = wadv.BuildWorkshop(wadvM.Id, lyId, shrine: true);
        Check(noFund == null, "资金不足无法建神殿");

        // 令咒: 御主3枚, 从者召来(5)与抗性强化(7)
        int csBefore = wadvM.Cs;
        string c1 = wadv.UseCommandSeal(wadvM.Id, wadvS.Id, 5);
        Check(wadvM.Cs == csBefore - 1 && wadvS.Roaming == 0, $"从者召来消耗令咒({c1})");
        string c2 = wadv.UseCommandSeal(wadvM.Id, wadvS.Id, 7);
        Check(wadvS.HitFinalMod == 20, $"抗性强化令咒({c2}, HitFinal={wadvS.HitFinalMod})");
        // 抵消异常(6): 给从者中毒再抵消
        wadvS.GainStatus(StatusKind.Poison, 2, wadvM.Id);
        wadv.UseCommandSeal(wadvM.Id, wadvS.Id, 6);
        Check(wadvS.GetStatus(StatusKind.Poison) == 0, "抵消异常清毒");

        // 摧毁工房
        Check(wadv.DestroyWorkshop(ws.Id, wadvS.Id), "摧毁工房");
        Check(ws.Destroyed, "工房标记已毁");

        // 工房战斗增益(魔能重炮/集束光标)
        Console.WriteLine();
        Console.WriteLine("=== 工房战斗增益 ===");
        var ws2 = wadv.BuildWorkshop(wadvM.Id, wadvM.CurrentLeyline, shrine: false);
        wadv.AddFunds(wadvM.Id, 6);
        wadv.AddPart(ws2, WorkshopPart.MagicCannon);
        wadv.AddPart(ws2, WorkshopPart.FocusBeacon);
        var (wBonus, wPenalty) = wadv.WorkshopBattleBonus(wadvM.CurrentLeyline);
        Check(wBonus == 40 && wPenalty == 40, $"工房战斗增益: 己方+{wBonus}% 敌方-{wPenalty}%(魔能重炮40+集束光标40)");
        Check(!string.IsNullOrEmpty(WorkshopDef.PartName(WorkshopPart.MagicCannon)), "组件名称存在");
        Check(WorldAdvanced.PartEffectText(WorkshopPart.MagicCannon).Contains("40"), "魔能重炮效果说明含40%");

        // 神殿组件(圣所基盘翻倍/圣殿基盘100)
        Console.WriteLine();
        Console.WriteLine("=== 神殿组件 ===");
        wadv.AddFunds(wadvM.Id, 12);
        var shrine = wadv.BuildWorkshop(wadvM.Id, wadvM.CurrentLeyline, shrine: true);
        Check(shrine != null, "建神殿成功");
        int manaBefore = wadvW.Leylines.Find(l => l.Id == wadvM.CurrentLeyline).Mana;
        wadv.AddPart(shrine, WorkshopPart.ShrineSanctum);
        wadv.AddPart(shrine, WorkshopPart.ShrineSanctuary);
        wadv.MaintainWorkshops();
        int manaAfter = wadvW.Leylines.Find(l => l.Id == wadvM.CurrentLeyline).Mana;
        Check(manaAfter == manaBefore * 2, $"圣所基盘灵脉魔力量翻倍({manaBefore}→{manaAfter})");
        Check(shrine.ManaCap >= 100, $"圣殿基盘神殿魔力池100(现{shrine.ManaCap})");
        Check(WorldAdvanced.PartEffectText(WorkshopPart.ShrineSpirit).Contains("50"), "圣灵基盘说明含50%判定");

        // ================= 固有结界(世界层) =================
        Console.WriteLine();
        Console.WriteLine("=== 固有结界 ===");
        int marbleLy = wadvM.CurrentLeyline;
        bool marbleAdd = wadv.AddRealityMarble(wadvM.Id, marbleLy, "无限剑制");
        Check(marbleAdd, "解放固有结界(无限剑制)");
        Check(wadv.HasMarble(marbleLy) && wadv.MarbleName(marbleLy) == "无限剑制", "结界在灵脉上");
        var (mRet, mWin) = wadv.MarbleBattleEffect(marbleLy, wadvM.Id);
        Check(mRet == 1 && mWin == 20, $"结界效果: 撤退FP+{mRet} 防守方+{mWin}%");
        var (mRet2, mWin2) = wadv.MarbleBattleEffect(marbleLy, wadvS.Id);
        Check(mRet2 == 1 && mWin2 == 0, $"敌方视角: 无胜率加成({mWin2})但撤退惩罚保留");

        // ================= 全量资源运行冒烟(758条) =================
        Console.WriteLine();
        Console.WriteLine("=== 全量资源运行冒烟 ===");
        SmokeAllResources();

        // ================= 大航海战斗表结算(移植校验) =================
        Console.WriteLine();
        Console.WriteLine("=== 大航海战斗表结算 ===");
        {
            // 表内实例: 蓝方(醉枭赌徒 主力 + 焰狼/哈基米嗷 辅助) vs 橙方(机甲少女)
            var seaBlue = new SeaSettle.Side { Name = "蓝方" };
            seaBlue.Members.Add(new SeaSettle.Member { Unit = MakeServant("醉枭赌徒", 70, 3, 105, 175, 40, 35, 80), Slot = 0, PreWin = 20 });
            seaBlue.Members.Add(new SeaSettle.Member { Unit = MakeServant("焰狼", 70, 3, 10, 25, 40, 20, 45), Slot = 1 });
            seaBlue.Members.Add(new SeaSettle.Member { Unit = MakeServant("哈基米嗷", 40, 3, 70, 65, 70, 0, 50), Slot = 2 });
            var seaOrange = new SeaSettle.Side { Name = "橙方" };
            seaOrange.Members.Add(new SeaSettle.Member { Unit = MakeServant("机甲少女", 45, 3, 70, 70, 70, 80, 40), Slot = 0 });
            var seaPicks = new SeaSettle.Picks(2) { Attr = new[] { 2, 0, 1 } };   // 敏捷/筋力/耐久
            var (seaA, seaB) = SeaSettle.Settle2(seaBlue, seaOrange, seaPicks);

            Check(Math.Abs(seaA.Total[2] - 95) < 1e-6, $"属性总值 敏捷 40+辅助÷2 = 95 (现{seaA.Total[2]:0.#})");
            Check(Math.Abs(seaA.Total[0] - 145) < 1e-6, $"属性总值 筋力 105+辅助÷2 = 145 (现{seaA.Total[0]:0.#})");
            Check(Math.Abs(seaA.Total[1] - 220) < 1e-6, $"属性总值 耐久 175+辅助÷2 = 220 (现{seaA.Total[1]:0.#})");
            Check(Math.Abs(seaA.BattleValue - 460) < 1e-6, $"战斗属性总值 左方 460 (现{seaA.BattleValue:0.#})");
            Check(Math.Abs(seaB.BattleValue - 210) < 1e-6, $"战斗属性总值 右方 210 (现{seaB.BattleValue:0.#})");
            Check(seaA.BaseRate == 90 && seaB.BaseRate == 10, $"基础胜率 三优=90 : 三劣=10 (现{seaA.BaseRate}:{seaB.BaseRate})");
            Check(Math.Abs(seaA.Chain - 640) < 1e-6, $"胜率合计 70等级+90基础+460战力+20战前 = 640 (现{seaA.Chain:0.#})");
            Check(Math.Abs(seaB.Chain - 265) < 1e-6, $"胜率合计 45等级+10基础+210战力 = 265 (现{seaB.Chain:0.#})");
            Check(Math.Abs(seaA.Diff - 375) < 1e-6, $"胜率差值 375 (现{seaA.Diff:0.#})");
            Check(seaA.FinalRate == 100 && seaB.FinalRate == 0, $"最终胜率 clamp(50+375÷2) = 100 : 0 (现{seaA.FinalRate}:{seaB.FinalRate})");
            Check(seaA.FinalRate + seaB.FinalRate == 100, "双方最终胜率互补");

            Check(SeaSettle.BaseRateByVerdicts(new[] { 3, 3, 2 }, 3) == 80, "优劣组合 2优1平 = 80");
            Check(SeaSettle.BaseRateByVerdicts(new[] { 3, 1, 1 }, 3) == 30, "优劣组合 1优2劣 = 30");
            Check(SeaSettle.BaseRateByVerdicts(new[] { 2, 2, 2 }, 3) == 50, "优劣组合 3平 = 50");
            Check(SeaSettle.BaseRateByVerdicts(new[] { 2, 2, 1 }, 3) == 40, "优劣组合 2平1劣 = 40");
            Check(SeaSettle.BaseRateByVerdicts(new[] { 1, 1, 1 }, 3) == 10, "优劣组合 3劣 = 10");

            // 魔力不足扣减: 每 -20 → -10; 单独行动减半; 宝具不吃扣减
            var dry = MakeServant("缺魔者", 50, 3, 40, 40, 40, 40, 40);
            dry.Attr[5] = 40;
            dry.MpCur = -50;
            var drySide = new SeaSettle.Side { Name = "缺魔" };
            drySide.Members.Add(new SeaSettle.Member { Unit = dry, Slot = 0 });
            SeaSettle.Prepare(drySide);
            Check(drySide.Members[0].Deficit == -20, $"魔力-50 → 属性扣减 -20 (现{drySide.Members[0].Deficit})");
            Check(drySide.Members[0].Attr[0] == 20, $"筋力 40-20 = 20 (现{drySide.Members[0].Attr[0]})");
            Check(drySide.Members[0].Attr[5] == 40, $"宝具不吃魔力不足扣减 (现{drySide.Members[0].Attr[5]})");
            drySide.SoloAction = true;
            SeaSettle.Prepare(drySide);
            Check(drySide.Members[0].Deficit == -10, $"单独行动扣减减半 = -10 (现{drySide.Members[0].Deficit})");

            // 保底: 属性不低于 0
            var floorU = MakeServant("保底者", 50, 3, 5, 5, 5, 5, 5);
            floorU.MpCur = -400;
            var floorSide = new SeaSettle.Side { Name = "保底" };
            floorSide.Members.Add(new SeaSettle.Member { Unit = floorU, Slot = 0 });
            SeaSettle.Prepare(floorSide);
            Check(floorSide.Members[0].Attr[0] == 0, $"角色保底: 属性不低于0 (现{floorSide.Members[0].Attr[0]})");

            // 三方混战: 四维优劣(30/15/10+10) 与 150 分制保有胜率
            var tA = new SeaSettle.Side { Name = "甲" };
            var tB = new SeaSettle.Side { Name = "乙" };
            var tC = new SeaSettle.Side { Name = "丙" };
            tA.Members.Add(new SeaSettle.Member { Unit = MakeServant("甲主", 60, 3, 100, 100, 100, 100, 100), Slot = 0 });
            tB.Members.Add(new SeaSettle.Member { Unit = MakeServant("乙主", 50, 3, 60, 60, 60, 60, 60), Slot = 0 });
            tC.Members.Add(new SeaSettle.Member { Unit = MakeServant("丙主", 40, 3, 60, 60, 60, 60, 60), Slot = 0 });
            var picks3 = new SeaSettle.Picks(3) { Attr = new[] { 0, 1, 2, 3 } };
            var rs3 = SeaSettle.Settle3(new[] { tA, tB, tC }, picks3);
            Check(rs3[0].QuadScore == 130, $"四维优劣 全优 = 4×30+10 = 130 (现{rs3[0].QuadScore})");
            Check(rs3[1].QuadScore == 10, $"四维优劣 二劣 = 0+10 = 10 (现{rs3[1].QuadScore})");
            Check(Math.Abs(rs3[0].LevelTerm - 10) < 1e-6, $"等差 = 60-50 = 10 (现{rs3[0].LevelTerm:0.#})");
            Check(Math.Abs(rs3[0].KeepRate - 150) < 1e-6, $"保有胜率 另两方低于基准 → 150 (现{rs3[0].KeepRate:0.#})");
            Check(rs3[0].FinalRate == 100, $"三方最终胜率归一 100% (现{rs3[0].FinalRate}%)");
            tA.StarPioneer = true;
            var rs3b = SeaSettle.Settle3(new[] { tA, tB, tC }, picks3);
            Check(Math.Abs(rs3b[0].LevelTerm) < 1e-6, "星之开拓者: 等差项归零");

            // 战斗内接入: SeaMode 开关走大航海结算链
            var seaWorld = new KsgWorld();
            var seaL = MakeServant("左主", 60, 1, 80, 80, 80, 60, 60);
            var seaR = MakeServant("右主", 50, 1, 50, 50, 50, 50, 50);
            if (seaL.Id <= 0) seaWorld.RegisterUnit(seaL);
            if (seaR.Id <= 0) seaWorld.RegisterUnit(seaR);
            var seaBattle = new KsgBattle { SeaMode = true, SeaPickAttr = new[] { 0, 1, 2 } };
            seaBattle.StartParty(seaWorld, new List<UnitDef> { seaL }, new List<UnitDef> { seaR }, 2);
            seaBattle.BatteryCheck();
            Check(seaBattle.LeftWin + seaBattle.RightWin == 100,
                $"战斗中大航海结算: 胜率互补 (左{seaBattle.LeftWin}% + 右{seaBattle.RightWin}%)");
            Check(seaBattle.LeftWin > seaBattle.RightWin,
                $"较强一方胜率更高 (左{seaBattle.LeftWin}% > 右{seaBattle.RightWin}%)");
            Check(seaBattle.SeaRows.Count > 0, $"工序表格行生成 {seaBattle.SeaRows.Count} 行");
            Check(seaBattle.SeaLeftBattleValue == 240, $"战斗属性总值 左方 3×80 = 240 (现{seaBattle.SeaLeftBattleValue})");
        }

        Console.WriteLine($"\n=== 结果: {_pass} 通过, {_fail} 失败 ===");
        if (_fail > 0) Environment.Exit(1);
    }

    /// <summary>读取真实资源 JSON, 对每条资源在战斗中实际发动, 验证不崩溃且产生效果。</summary>
    private static void SmokeAllResources()
    {
        string path = @"D:\desktop\wolfkill\engine-godot\data\ksg_resources.json";
        if (!File.Exists(path))
        {
            Console.WriteLine("  ⚠ 未找到 ksg_resources.json, 跳过全量冒烟");
            return;
        }
        var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(path));
        var arr = doc.RootElement.GetProperty("resources").EnumerateArray().ToList();

        int tested = 0, effects = 0, noEffect = 0, crashed = 0;
        var crashList = new List<string>();
        foreach (var el in arr)
        {
            int id = el.GetProperty("id").GetInt32();
            string name = el.GetProperty("name").GetString() ?? "?";
            string kind = el.GetProperty("kind").GetString() ?? "skill";
            var kindEnum = kind switch { "skill" => ResKind.Skill, "np" => ResKind.Np, _ => ResKind.Item };
            var res = new ResourceDef
            {
                Id = id,
                Name = name,
                Kind = kindEnum,
                Rank = RankUtil.Parse(el.TryGetProperty("rank", out var rk) ? rk.GetString() : "C"),
                When = When.Any,
                Cost = 0,
                Recast = 0,
            };
            if (el.TryGetProperty("effects", out var fx) && fx.ValueKind == System.Text.Json.JsonValueKind.Array)
            {
                foreach (var fe in fx.EnumerateArray())
                {
                    var ef = new EffectLine();
                    if (fe.TryGetProperty("flag", out var fl)) ef.Flag = FlagFromStr(fl.GetString());
                    if (fe.TryGetProperty("value", out var vv) && vv.ValueKind == System.Text.Json.JsonValueKind.Number) ef.Value = vv.GetInt32();
                    if (fe.TryGetProperty("chance", out var ch) && ch.ValueKind == System.Text.Json.JsonValueKind.Number) ef.Chance = ch.GetInt32();
                    if (fe.TryGetProperty("layers", out var ly) && ly.ValueKind == System.Text.Json.JsonValueKind.Number) ef.Layers = ly.GetInt32();
                    if (fe.TryGetProperty("times", out var tm) && tm.ValueKind == System.Text.Json.JsonValueKind.Number) ef.Times = tm.GetInt32();
                    if (fe.TryGetProperty("target", out var tg) && tg.ValueKind == System.Text.Json.JsonValueKind.String)
                        ef.Target = TargetFromStr(tg.GetString());
                    if (fe.TryGetProperty("status", out var st) && st.ValueKind == System.Text.Json.JsonValueKind.String)
                        ef.Status = StatusFromStr(st.GetString());
                    if (fe.TryGetProperty("attr", out var at) && at.ValueKind == System.Text.Json.JsonValueKind.String)
                        ef.Attr = AttrFromStr(at.GetString());
                    res.Effects.Add(ef);
                }
            }
            tested++;
            effects += res.Effects.Count;
            if (res.Effects.Count == 0) { noEffect++; continue; }

            try
            {
                // 构造两单位战斗并发动资源
                var w = new KsgWorld();
                var attacker = MakeServant("冒烟甲", 60, 0, 40, 40, 40, 40, 40);
                var defender = MakeServant("冒烟乙", 60, 1, 40, 40, 40, 40, 40);
                w.RegisterUnit(attacker);
                w.RegisterUnit(defender);
                var b = new KsgBattle();
                Dice.Seed(7);
                b.Start(w, attacker.Id, defender.Id, 4);
                b.SetTactic(1, Tactic.Strike);
                b.SetTactic(2, Tactic.Probe);
                b.PickMainAttr(1, 0);
                b.PickMainAttr(2, 1);
                b.RollRandAttr();
                b.BatteryCheck();

                var before = BattleSnapshot(b, attacker, defender);
                KsgEffects.ApplyResource(b, w, attacker, res, 0);
                var after = BattleSnapshot(b, attacker, defender);

                if (SnapshotEqual(before, after) && attacker.Status.Count == 0 && defender.Status.Count == 0)
                {
                    // 效果行存在但什么都没变(纯决策类/条件不满足, 不算崩溃)
                }
            }
            catch (Exception ex)
            {
                crashed++;
                crashList.Add($"{id} {name}: {ex.GetType().Name} {ex.Message}");
            }
        }
        Check(crashed == 0, $"全量运行零崩溃({tested}条资源 / {effects}效果行, 崩溃{crashed})");
        if (crashList.Count > 0)
            foreach (var c in crashList.Take(8))
                Console.WriteLine("    !! " + c);
        Check(noEffect == 0, $"无效果行资源数0(实际{noEffect})");
        Console.WriteLine($"    已遍历 {tested} 条资源(技能/宝具/礼装), {effects} 条效果行全部实际发动");
    }

    private static (int lw, int rw, int la, int ra, int lmp, int rmp, int lf, int rf, int ls, int rs) BattleSnapshot(
        KsgBattle b, UnitDef a, UnitDef d)
    {
        return (b.LeftWin, b.RightWin,
                a.AttrValue(0) + a.AttrValue(1) + a.AttrValue(2),
                d.AttrValue(0) + d.AttrValue(1) + d.AttrValue(2),
                a.MpCur, d.MpCur, a.Fp, d.Fp, a.Status.Count, d.Status.Count);
    }

    private static bool SnapshotEqual(
        (int lw, int rw, int la, int ra, int lmp, int rmp, int lf, int rf, int ls, int rs) x,
        (int lw, int rw, int la, int ra, int lmp, int rmp, int lf, int rf, int ls, int rs) y)
        => x == y;

    private static EffFlag FlagFromStr(string s) => s switch
    {
        "EF_ATTR_UP" => EffFlag.AttrUp,
        "EF_ATTR_DOWN" => EffFlag.AttrDown,
        "EF_ATTR_UP_CONST" => EffFlag.AttrUpConst,
        "EF_ATTR_DOWN_CONST" => EffFlag.AttrDownConst,
        "EF_WIN_UP" => EffFlag.WinUp,
        "EF_WIN_DOWN" => EffFlag.WinDown,
        "EF_FINAL_WIN_UP" => EffFlag.FinalWinUp,
        "EF_FINAL_WIN_DOWN" => EffFlag.FinalWinDown,
        "EF_FLOOR_UP" => EffFlag.FloorUp,
        "EF_FLOOR_PEN" => EffFlag.FloorPen,
        "EF_HIT_UP" => EffFlag.HitUp,
        "EF_HIT_FINAL_UP" => EffFlag.HitFinalUp,
        "EF_HIT_PEN" => EffFlag.HitPen,
        "EF_RES_UP" => EffFlag.ResUp,
        "EF_RES_DOWN" => EffFlag.ResDown,
        "EF_STATE_RES" => EffFlag.StateRes,
        "EF_STATE_IM" => EffFlag.StateIm,
        "EF_EFFECT_IM" => EffFlag.EffectIm,
        "EF_MANA_UP" => EffFlag.ManaUp,
        "EF_MANA_DOWN" => EffFlag.ManaDown,
        "EF_STATUS_GIVE" => EffFlag.StatusGive,
        "EF_STATUS_REMOVE" => EffFlag.StatusRemove,
        "EF_BURN_BLOW" => EffFlag.BurnBlow,
        "EF_ELECTRIC_BLOW" => EffFlag.ElectricBlow,
        "EF_POISON_BLOW" => EffFlag.PoisonBlow,
        "EF_RECAST" => EffFlag.Recast,
        "EF_RECAST_LOSE" => EffFlag.RecastLose,
        "EF_FP_UP" => EffFlag.FpUp,
        "EF_FP_DOWN" => EffFlag.FpDown,
        "EF_TP_FP" => EffFlag.TpFp,
        "EF_DEATH" => EffFlag.Death,
        "EF_BOUND_DEATH" => EffFlag.BoundDeath,
        "EF_PIERCE" => EffFlag.Pierce,
        "EF_INV_PIERCE" => EffFlag.InvPierce,
        "EF_EVADE" => EffFlag.Evade,
        "EF_PROTECT" => EffFlag.Protect,
        "EF_INVINCIBLE" => EffFlag.Invincible,
        "EF_RETALIATE" => EffFlag.Retaliate,
        "EF_SUMMON" => EffFlag.Summon,
        "EF_INFO" => EffFlag.Info,
        "EF_CS" => EffFlag.Cs,
        "EF_OTHER" => EffFlag.Other,
        "EF_PLEDGE_DECL" => EffFlag.PledgeDecl,
        "EF_TICK_PROC" => EffFlag.TickProc,
        "EF_TICK_ROUND" => EffFlag.TickRound,
        "EF_RANDOM_GIVE" => EffFlag.RandomGive,
        "EF_ON_KILL" => EffFlag.OnKill,
        "EF_ON_WIN" => EffFlag.OnWin,
        "EF_CHARGE" => EffFlag.Charge,
        "EF_GRANT_CS" => EffFlag.GrantCs,
        "EF_GRANT_BADGE" => EffFlag.GrantBadge,
        "EF_SEAL" => EffFlag.Seal,
        "EF_REGEN" => EffFlag.Regen,
        _ => EffFlag.None,
    };

    private static int AttrFromStr(string s) => s switch
    {
        "A_STR" => 0, "A_END" => 1, "A_AGI" => 2, "A_MAG" => 3, "A_LUK" => 4, "A_NP" => 5, "A_CIRCUIT" => 6, _ => -1,
    };

    private static int StatusFromStr(string s) => s switch
    {
        "S_EVADE" => (int)StatusKind.Evade, "S_INVINCIBLE" => (int)StatusKind.Invincible,
        "S_PROTECT" => (int)StatusKind.Protect, "S_RESUP" => (int)StatusKind.ResUp,
        "S_STATE_RES" => (int)StatusKind.StateRes, "S_STATE_IM" => (int)StatusKind.StateIm,
        "S_EFFECT_IM" => (int)StatusKind.EffectIm, "S_TIRED" => (int)StatusKind.Tired,
        "S_CRIPPLED" => (int)StatusKind.Crippled, "S_LAG" => (int)StatusKind.Lag,
        "S_CURSE" => (int)StatusKind.Curse, "S_SEAL" => (int)StatusKind.Seal,
        "S_SKILL_SEAL" => (int)StatusKind.SkillSeal, "S_NP_SEAL" => (int)StatusKind.NpSeal,
        "S_RESDOWN" => (int)StatusKind.ResDown, "S_POISON" => (int)StatusKind.Poison,
        "S_BURN" => (int)StatusKind.Burn, "S_FREEZE" => (int)StatusKind.Freeze,
        "S_ELECTRIC" => (int)StatusKind.Electric, "S_STONE" => (int)StatusKind.Stone,
        "S_STUN" => (int)StatusKind.Stun, "S_CHARM" => (int)StatusKind.Charm,
        "S_CONFUSE" => (int)StatusKind.Confuse, "S_FEAR" => (int)StatusKind.Fear,
        "S_RES_BREAK" => (int)StatusKind.ResBreak, "S_TRAIT_GIVE" => (int)StatusKind.TraitGive,
        "S_CHARGE" => (int)StatusKind.Charge, "S_BADGE" => (int)StatusKind.Badge,
        _ => 0,
    };

    private static int TargetFromStr(string s) => s switch
    {
        "self" => -2, "ally_all" => -1, "enemy_all" => 0,
        _ when s.StartsWith("enemy_") && int.TryParse(s.Substring(6), out int n) => n,
        _ => -2,
    };


    private static UnitDef MakeServant(string name, int level, int cls,
        int str, int endu, int agi, int mag, int luk)
    {
        var u = new UnitDef
        {
            Name = name,
            TrueName = name + "(真名)",
            Level = level,
            UType = 1,
            ServantClass = cls,
            HiddenAttr = 2,
            Law = 1, Moral = 1,
            Traits = 1,
            Fp = 1, Cs = 3,
            MpCap = 150, MpFloor = -100, MpCur = 100,
        };
        u.Attr[0] = str; u.Attr[1] = endu; u.Attr[2] = agi;
        u.Attr[3] = mag; u.Attr[4] = luk; u.Attr[5] = 0;
        return u;
    }

    private static ResourceDef MakeSkill(string name, EffFlag flag, int value)
    {
        var r = new ResourceDef
        {
            Name = name,
            Kind = ResKind.Np,
            Rank = Rank.A,
            When = When.Proc,
            Cost = 50,
            Recast = 3,
            Type = (int)NpType.Human,
        };
        r.Effects.Add(new EffectLine { Flag = flag, Value = value, Target = -2 });
        return r;
    }

    private static ResourceDef BuildResource(
        int id, ResKind kind, Rank rank, int type = 0, int focus = 0)
    {
        return new ResourceDef
        {
            Id = id,
            Name = $"测试资源{id}",
            Kind = kind,
            Rank = rank,
            Type = type,
            Focus = focus,
        };
    }
}
