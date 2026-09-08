using System;
using System.Collections.Generic;
using KsgGodot.Model;

namespace KsgGodot.Engine;

/// <summary>行动类型(规则书 3.1~3.11)</summary>
public enum WorldAction
{
    None, Move,     // 机动(3.1): 移动+暴露
    SoulEat,        // 魂食(3.2): 人流量-1 换魔力
    Intervene,      // 干涉(3.3): 对灵脉干预
    Make,           // 制造(3.4): 礼装制作等
    Build,          // 建设(3.5): 工房/神殿
    Scout,          // 侦查(3.6): 广泛/定向侦查
    Survey,         // 调查(3.7): 情报调查/真名猜测
    Rest,           // 休整(3.8): 恢复
    Intercede,      // 介入(3.9): 战斗介入
    Destroy,        // 摧毁工房(3.10)
    Exit,           // 退出干涉(3.11)
}

/// <summary>契约类型(第二章)</summary>
public enum PactType
{
    None, Grail, Alliance, Truce, Mana, Force, Servitude, Duel,
}

/// <summary>一个契约(两方单位间)</summary>
public class PactDef
{
    public PactType Type;
    public int AUnitId, BUnitId;   // 缔约双方
    public int TurnsLeft;          // 剩余回合(0=永久)
    public string Desc = "";
}

/// <summary>世界战役(完整圣杯战争流程): 灵脉地图/行动/契约/轮次/圣杯</summary>
public class WorldCampaign
{
    public const int MaxDays = 7;
    public const int MaxMasters = 6;

    public KsgWorld W;
    public int Day = 1;                       // 当前战斗日(1..7)
    public bool Phase = true;                 // true=昼 false=夜
    public int ActionSlot = -1;               // 当前待行动席位(-1=行动阶段未开始)
    public List<PactDef> Pacts = new();
    public int WinnerMasterId;                // 胜利者(圣杯愿望实现)
    public bool Finished;

    public event Action<string> LogEvent;
    private void Log(string s) => LogEvent?.Invoke(s);

    public WorldCampaign(KsgWorld w)
    {
        W = w;
    }

    // ---------- 灵脉地图 ----------
    public static readonly string[] LeylineNames = {
        "冬木·柳洞寺", "冬木·教会", "冬木·大桥", "穗群原学园",
        "冬木港", "圆藏山", "爱因兹贝伦城",
    };

    public void SetupMap(int masterCount, Random rng, Dictionary<int, int> assignedLeylines = null)
    {
        W.Leylines.Clear();
        for (int i = 0; i < 7; i++)
        {
            // 经典分布: 大小中循环
            int mana = new[] { 40, 15, 25, 20, 35, 30, 45 }[i % 7];
            var ly = new LeylineDef
            {
                Id = i + 1,
                Name = LeylineNames[i],
                Mana = mana,
                Flow = 8 + (i % 3) * 2,
            };
            W.Leylines.Add(ly);
        }
        // 全员降临: 优先使用指定出生地(出场设置阶段), 否则随机
        var masters = W.Units.FindAll(u => u.IsMaster && u.Alive);
        int n = Math.Min(masters.Count, MaxMasters);
        for (int i = 0; i < n && masters.Count > 0; i++)
        {
            var m = masters[i];
            LeylineDef ly;
            if (assignedLeylines != null && assignedLeylines.TryGetValue(m.Id, out int lid))
                ly = W.Leylines.Find(x => x.Id == lid) ?? W.Leylines[rng.Next(W.Leylines.Count)];
            else
                ly = W.Leylines[rng.Next(W.Leylines.Count)];
            m.CurrentLeyline = ly.Id;
            m.Roaming = 1;
            Log($"  · {m.Name} 降临至「{ly.Name}」");
        }
        // 与御主配对的从者同灵脉
        foreach (var s in W.Units.FindAll(u => u.IsServant && u.Alive))
        {
            var master = FindPairedMaster(s);
            if (master != null) s.CurrentLeyline = master.CurrentLeyline;
        }
        Day = 1; Phase = true; Finished = false; WinnerMasterId = 0;
    }

    public UnitDef FindPairedMaster(UnitDef servant)
    {
        // 简化: 记录在 UnitDef.MasterUnitId(建卡时设定); 未设则按创建顺序最近御主
        if (servant.MasterUnitId > 0)
            return W.Units.Find(u => u.Id == servant.MasterUnitId);
        return W.Units.Find(u => u.IsMaster && u.Alive);
    }

    public LeylineDef LeylineWhere(int unitId)
    {
        var u = W.Units.Find(x => x.Id == unitId);
        if (u == null || u.CurrentLeyline <= 0) return null;
        return W.Leylines.Find(l => l.Id == u.CurrentLeyline);
    }

    // ---------- 行动 ----------
    /// <summary>执行一次行动(返回日志)。</summary>
    public string DoAction(int unitId, WorldAction act)
    {
        var u = W.Units.Find(x => x.Id == unitId);
        if (u == null || !u.Alive) return "单位不存在";
        var ly = LeylineWhere(unitId);
        switch (act)
        {
            case WorldAction.SoulEat:
                return DoSoulEat(u, ly);
            case WorldAction.Rest:
                return DoRest(u);
            case WorldAction.Move:
                return DoMove(u, ly);
            case WorldAction.Scout:
                return DoScout(u, ly);
            case WorldAction.Survey:
                return DoSurvey(u, ly);
            case WorldAction.Intervene:
                return DoIntervene(u, ly);
            case WorldAction.Make:
                return DoMake(u, ly);
            case WorldAction.Build:
                return DoBuild(u, ly);
            default:
                return $"{u.Name} 行动[{act}]执行(规则细则见手册)";
        }
    }

    private string DoScout(UnitDef u, LeylineDef ly)
    {
        if (ly == null) return $"{u.Name} 不在任何灵脉";
        int info = W.Units.FindAll(x => x.CurrentLeyline == ly.Id && x.Id != u.Id && x.Alive).Count;
        Log($"  · {u.Name} [广泛侦查]「{ly.Name}」: 发现 {info} 名其他单位, 获得其灵脉信息");
        return $"{u.Name} 侦查「{ly.Name}」: 发现{info}名单位";
    }

    private string DoSurvey(UnitDef u, LeylineDef ly)
    {
        if (ly == null) return $"{u.Name} 不在任何灵脉";
        var enemies = W.Units.FindAll(x => x.CurrentLeyline == ly.Id && x.Id != u.Id && x.Alive && x.IsServant);
        if (enemies.Count == 0) return $"{u.Name} 调查: 本灵脉无敌方从者";
        var target = enemies[Math.Abs(u.Id) % enemies.Count];
        Log($"  · {u.Name} [情报调查] {target.Name}({target.TrueName}) 真名暴露");
        return $"{u.Name} 调查: 获悉 {target.TrueName} 的真名";
    }

    private string DoIntervene(UnitDef u, LeylineDef ly)
    {
        if (ly == null) return $"{u.Name} 不在任何灵脉";
        if (ly.Flow > 0)
        {
            ly.FlowDown(1);
            Log($"  · {u.Name} [干涉]「{ly.Name}」人流量{ly.Flow + 1}→{ly.Flow}");
            return $"{u.Name} 干涉「{ly.Name}」: 人流量-1";
        }
        return $"{u.Name} 干涉: 人流量已为0";
    }

    private string DoMake(UnitDef u, LeylineDef ly)
    {
        bool canMake = u.Skills.Exists(s => s.CardName.Contains("制作")) || u.Skills.Exists(s => s.CardName.Contains("礼装"));
        if (!canMake && !u.IsMaster) return $"{u.Name} 无制作技能(需[道具制作]/[礼装制作])";
        u.Items.Add(new ResourceDef { Id = -1, Name = "简易礼装", Kind = ResKind.Item, When = When.Any });
        Log($"  · {u.Name} [制造] 获得简易礼装");
        return $"{u.Name} 制造: 获得简易礼装";
    }

    private string DoBuild(UnitDef u, LeylineDef ly)
    {
        if (ly == null) return $"{u.Name} 不在任何灵脉";
        if (ly.HasWorkshop || ly.HasShrine)
            return ly.HasWorkshop ? $"{u.Name} 建设: 「{ly.Name}」已有工房" : $"{u.Name} 建设: 「{ly.Name}」已有神殿";
        if (u.IsServant) return $"{u.Name} 无法建设(御主职能)";
        ly.HasWorkshop = true;
        Log($"  · {u.Name} [建设] 在「{ly.Name}」建立工房");
        return $"{u.Name} 在「{ly.Name}」建立工房";
    }

    private string DoSoulEat(UnitDef u, LeylineDef ly)
    {
        if (ly == null) return $"{u.Name} 不在任何灵脉";
        if (ly.Flow <= 0) return $"{u.Name} 魂食失败: 「{ly.Name}」人流量为0";
        ly.FlowDown(1);
        int gain = u.IsMaster ? 40 : 60;
        u.MpCur = Math.Min(u.MpCur + gain, u.MpCap);
        Log($"  · {u.Name} [魂食]!{ly.Name} 人流量{ly.Flow + 1}→{ly.Flow}, 魔力+{gain}");
        return $"{u.Name} 魂食成功: 人流量-1, 魔力+{gain}";
    }

    private string DoRest(UnitDef u)
    {
        int heal = u.IsMaster ? 20 : 30;
        u.MpCur = Math.Min(u.MpCur + heal, u.MpCap);
        Log($"  · {u.Name} [休整] 魔力+{heal}");
        return $"{u.Name} 休整: 魔力+{heal}";
    }

    private string DoMove(UnitDef u, LeylineDef ly)
    {
        if (ly == null) return $"{u.Name} 不在任何灵脉";
        // 机动: 移动到下一灵脉(演示: 随机相邻)
        var others = W.Leylines.FindAll(l => l.Id != ly.Id);
        if (others.Count == 0) return $"{u.Name} 无处可去";
        var target = others[(int)(u.Id * 7) % others.Count];
        u.CurrentLeyline = target.Id;
        u.Roaming = 1;
        Log($"  · {u.Name} [机动] {ly.Name} → {target.Name}(暴露)");
        return $"{u.Name} 机动至「{target.Name}」";
    }

    // ---------- 契约 ----------
    public PactDef MakePact(PactType type, int aUnitId, int bUnitId, int turns)
    {
        var p = new PactDef { Type = type, AUnitId = aUnitId, BUnitId = bUnitId, TurnsLeft = turns };
        Pacts.Add(p);
        Log($"  · 契约成立: {UnitName(aUnitId)} × {UnitName(bUnitId)} [{type}]");
        return p;
    }

    public bool HasPactBetween(int aUnitId, int bUnitId, PactType type)
        => Pacts.Exists(p => p.Type == type &&
            ((p.AUnitId == aUnitId && p.BUnitId == bUnitId) ||
             (p.AUnitId == bUnitId && p.BUnitId == aUnitId)));

    private string UnitName(int id)
    {
        var u = W.Units.Find(x => x.Id == id);
        return u != null ? u.Name : $"#{id}";
    }

    // ---------- 回合推进 ----------
    /// <summary>进入下一回合(昼→夜→昼…; 每2回合为1日, 第14回合终=7日终)。</summary>
    public void NextTurn()
    {
        if (Finished) return;
        Phase = !Phase;
        if (Phase) Day++;          // 新昼 = 新一日
        // 契约回合递减
        foreach (var p in Pacts)
            if (p.TurnsLeft > 0) p.TurnsLeft--;
        Pacts.RemoveAll(p => p.TurnsLeft == 0);
        W.Day = Phase;
        Log($"════ 第{Day}日·{(Phase ? "昼" : "夜")} ════");
        if (Day > MaxDays)
        {
            Log("※ 第7日终: 圣杯显现!");
            FinishGame();
        }
        // 每新回合让行动槽复位
        ActionSlot = -1;
    }

    /// <summary>圣杯战争结束: 统计胜者。</summary>
    public void FinishGame()
    {
        if (Finished) return;
        Finished = true;
        // 活着且有契约的主(简化: 最后存活且未退场者胜)
        var candidates = W.Units.FindAll(u => u.IsMaster && u.Alive && !u.Retreated);
        if (candidates.Count > 0)
        {
            var winner = candidates[0];
            WinnerMasterId = winner.Id;
            Log($"━━━━ 圣杯显现! 胜者: {winner.Name} 实现了愿望 ━━━━");
        }
        else
        {
            Log("━━━━ 全员退场, 圣杯战争无胜者 ━━━━");
        }
    }

    /// <summary>下一行动席位(返回 -1 表示行动阶段结束)。</summary>
    public int NextActionSlot()
    {
        var masters = W.Units.FindAll(u => u.IsMaster && u.Alive && !u.Retreated);
        if (masters.Count == 0) return -1;
        // 轮转: 从 ActionSlot+1 开始找下一个
        int start = ActionSlot;
        for (int k = 1; k <= masters.Count; k++)
        {
            int idx = (start + k) % masters.Count;
            if (masters[idx].Acted == 0)
            {
                ActionSlot = idx;
                return masters[idx].Id;
            }
        }
        return -1;
    }

    /// <summary>回合开始结算: 供魔/工房/灵脉效果。</summary>
    public void BeginTurnSettlement()
    {
        foreach (var u in W.Units)
        {
            if (!u.Alive || u.Retreated) continue;
            // 灵脉供魔(所在灵脉魔力量/10)
            var ly = LeylineWhere(u.Id);
            if (ly != null && ly.Mana > 0)
            {
                int supply = Math.Max(1, ly.Mana / 10);
                u.MpCur = Math.Min(u.MpCur + supply, u.MpCap);
                Log($"  · {u.Name} 灵脉供魔 +{supply}");
            }
            u.Acted = 0;
        }
    }
}