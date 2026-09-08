using System;
using System.Collections.Generic;
using KsgGodot.Model;

namespace KsgGodot.Engine;

/// <summary>工房组件类型(工房建造书)</summary>
public enum WorkshopPart
{
    None,
    InfoBase,      // 信息基盘(基本)
    ResBase,       // 资源基盘(基本)
    AlchemyBase,   // 炼金基盘(基本)
    PowerArray,    // 强能法阵(辅助)
    MedicalUnit,   // 医疗装机(辅助)
    ConvergeDisc,  // 聚合圆盘(辅助)
    DarkPit,       // 黑厄深阱(限定)
    MagicCannon,   // 魔能重炮(限定)
    SanctionOrg,   // 制裁机关(限定)
    AmplifyMod,    // 增幅模块(限定)
    TransPortal,   // 传输阵式(支援)
    FocusBeacon,   // 集束光标(支援)
    ArtifControl,  // 术控主脑(支援)
    // 神殿组件(工房书 3.x)
    ShrineSanctum,     // 圣所基盘: 灵脉魔力量翻倍
    ShrineSanctuary,   // 圣殿基盘: 神殿额外魔力池100
    ShrineSpirit,      // 圣灵基盘: 圣杯于神殿降临
}

/// <summary>工房(或神殿)</summary>
public class WorkshopDef
{
    public int Id;
    public string Name = "";
    public int OwnerUnitId;
    public int LeylineId;
    public bool IsShrine;                       // true=神殿 false=工房
    public int ManaCap = 40, ManaCur = 10;
    public List<WorkshopPart> Parts = new();
    public bool Destroyed;

    public static string PartName(WorkshopPart p) => p switch
    {
        WorkshopPart.InfoBase => "信息基盘",
        WorkshopPart.ResBase => "资源基盘",
        WorkshopPart.AlchemyBase => "炼金基盘",
        WorkshopPart.PowerArray => "强能法阵",
        WorkshopPart.MedicalUnit => "医疗装机",
        WorkshopPart.ConvergeDisc => "聚合圆盘",
        WorkshopPart.DarkPit => "黑厄深阱",
        WorkshopPart.MagicCannon => "魔能重炮",
        WorkshopPart.SanctionOrg => "制裁机关",
        WorkshopPart.AmplifyMod => "增幅模块",
        WorkshopPart.TransPortal => "传输阵式",
        WorkshopPart.FocusBeacon => "集束光标",
        WorkshopPart.ArtifControl => "术控主脑",
        WorkshopPart.ShrineSanctum => "圣所基盘",
        WorkshopPart.ShrineSanctuary => "圣殿基盘",
        WorkshopPart.ShrineSpirit => "圣灵基盘",
        _ => "无",
    };

    public static int PartCost(WorkshopPart p) => p switch
    {
        WorkshopPart.InfoBase or WorkshopPart.ResBase or WorkshopPart.AlchemyBase => 1,
        WorkshopPart.PowerArray or WorkshopPart.MedicalUnit or WorkshopPart.ConvergeDisc => 2,
        WorkshopPart.DarkPit or WorkshopPart.MagicCannon or WorkshopPart.SanctionOrg => 3,
        WorkshopPart.AmplifyMod or WorkshopPart.TransPortal or WorkshopPart.FocusBeacon or WorkshopPart.ArtifControl => 2,
        WorkshopPart.ShrineSanctum or WorkshopPart.ShrineSanctuary or WorkshopPart.ShrineSpirit => 3,
        _ => 0,
    };
}

/// <summary>高级世界机制: 工房/资金/令咒8用法 (A4/A5/A6)</summary>
public class WorldAdvanced
{
    public KsgWorld W;
    public WorldCampaign C;
    public int NextWorkshopId = 1;
    public List<WorkshopDef> Workshops = new();
    public Dictionary<int, int> Funds = new();     // 御主资金

    public event Action<string> LogEvent;
    private void Log(string s) => LogEvent?.Invoke(s);

    public WorldAdvanced(KsgWorld w, WorldCampaign c)
    {
        W = w;
        C = c;
    }

    // ---------- 资金 / 储备 (A5) ----------
    public int FundOf(int unitId) => Funds.TryGetValue(unitId, out int f) ? f : 0;

    public int AddFunds(int unitId, int n)
    {
        int v = FundOf(unitId) + n;
        Funds[unitId] = v;
        return v;
    }

    /// <summary>支付资金(不足返回false)。</summary>
    public bool SpendFunds(int unitId, int n)
    {
        if (FundOf(unitId) < n) return false;
        Funds[unitId] = FundOf(unitId) - n;
        return true;
    }

    // ---------- 工房 (A4) ----------
    /// <summary>建造工房/神殿: 需要资金与灵脉。返回工房对象(失败null)。</summary>
    public WorkshopDef BuildWorkshop(int unitId, int leylineId, bool shrine)
    {
        var u = W.Units.Find(x => x.Id == unitId);
        if (u == null || !u.Alive) { Log("建造失败: 单位无效"); return null; }
        var ly = W.Leylines.Find(x => x.Id == leylineId);
        if (ly == null) { Log("建造失败: 灵脉不存在"); return null; }
        int cost = shrine ? 3 : 1;
        if (!SpendFunds(unitId, cost))
        {
            Log($"  · {u.Name} 资金不足(需{cost}资金, 现{FundOf(unitId)})");
            return null;
        }
        var ws = new WorkshopDef
        {
            Id = NextWorkshopId++,
            Name = shrine ? $"{ly.Name}·神殿" : $"{ly.Name}·工房",
            OwnerUnitId = unitId,
            LeylineId = leylineId,
            IsShrine = shrine,
        };
        Workshops.Add(ws);
        ly.HasWorkshop = !shrine;
        ly.HasShrine = shrine;
        Log($"  · {u.Name} 在「{ly.Name}」建造{(shrine ? "神殿" : "工房")}(花费{cost}资金)");
        return ws;
    }

    /// <summary>加组件(花费资金)。</summary>
    public bool AddPart(WorkshopDef ws, WorkshopPart part)
    {
        if (ws == null || ws.Destroyed) return false;
        if (ws.Parts.Contains(part)) { Log("  · 已安装同组件"); return false; }
        int cost = WorkshopDef.PartCost(part);
        if (!SpendFunds(ws.OwnerUnitId, cost))
        {
            Log($"  · 资金不足(需{cost})");
            return false;
        }
        ws.Parts.Add(part);
        Log($"  · {ws.Name} 安装组件[{WorkshopDef.PartName(part)}](花费{cost}资金)");
        return true;
    }

    /// <summary>工房/神殿回合维护: 工房魔力池从灵脉汲取; 神殿组件效果应用。</summary>
    public void MaintainWorkshops()
    {
        foreach (var ws in Workshops)
        {
            if (ws.Destroyed) continue;
            var ly = W.Leylines.Find(x => x.Id == ws.LeylineId);
            if (ly == null) continue;
            // 神殿组件: 圣所基盘=灵脉魔力量翻倍; 圣殿基盘=魔力池100
            if (ws.IsShrine)
            {
                if (ws.Parts.Contains(WorkshopPart.ShrineSanctum))
                    ly.Mana *= 2;
                if (ws.Parts.Contains(WorkshopPart.ShrineSanctuary))
                    ws.ManaCap = Math.Max(ws.ManaCap, 100);
            }
            // 工房汲取
            if (ly.Mana > 0 && ws.ManaCur < ws.ManaCap)
            {
                int gain = Math.Min(ws.ManaCap - ws.ManaCur, Math.Max(1, ly.Mana / 10));
                ws.ManaCur += gain;
            }
        }
    }

    /// <summary>摧毁工房(袭击成功等触发)。</summary>
    public bool DestroyWorkshop(int workshopId, int destroyerId)
    {
        var ws = Workshops.Find(x => x.Id == workshopId);
        if (ws == null) return false;
        ws.Destroyed = true;
        var ly = W.Leylines.Find(x => x.Id == ws.LeylineId);
        if (ly != null) { ly.HasWorkshop = false; ly.HasShrine = false; }
        Log($"  · {UnitName(destroyerId)} 摧毁了「{ws.Name}」");
        return true;
    }

    private string UnitName(int id) => W.Units.Find(x => x.Id == id)?.Name ?? $"#{id}";

    // ---------- 令咒8用法 (A6) ----------
    public record CsEffect(int Cost, string Name);

    public static readonly CsEffect[] CsUses = {
        new(1, "强制命令"), new(1, "属性补正"), new(1, "战况修正"),
        new(1, "胜率补正"), new(1, "用于撤退"), new(1, "从者召来"),
        new(1, "抵消即死与异常"), new(1, "抗性强化"),
    };

    /// <summary>消耗令咒执行一种用法。返回结果日志。</summary>
    public string UseCommandSeal(int masterId, int targetServantId, int usage)
    {
        var m = W.Units.Find(x => x.Id == masterId);
        var s = W.Units.Find(x => x.Id == targetServantId);
        if (m == null || !m.IsMaster) return "无效御主";
        if (s == null || !s.IsServant) return "无效从者";
        if (m.Cs <= 0) return $"{m.Name} 没有剩余令咒";
        if (usage < 0 || usage >= CsUses.Length) return "无效用法";

        m.Cs--;
        string use = CsUses[usage].Name;
        Log($"  · {m.Name} 消耗1枚令咒 → 对{s.Name}[{use}]");
        switch (usage)
        {
            case 0: // 强制命令: 强制指令
                Log($"    令{s.Name} 执行强制指令(强制某种行动)");
                break;
            case 1: // 属性补正: +20等级补正
                s.AttrMod[0] += 20; s.AttrMod[1] += 20; s.AttrMod[2] += 20;
                break;
            case 2: // 战况修正: 战斗属性+30
                break;
            case 3: // 胜率补正: +50%胜率(战斗内)
                break;
            case 4: // 撤退: 从者撤退豁免(战斗外免疫)
                s.TpFp += 1;
                break;
            case 5: // 从者召来: 无论距离召来
                s.Roaming = 0;
                Log($"    {s.Name} 被召来至御主身边");
                break;
            case 6: // 抵消即死与异常
                s.ClearStatus(StatusKind.Poison);
                s.ClearStatus(StatusKind.Burn);
                s.ClearStatus(StatusKind.Curse);
                s.ClearStatus(StatusKind.Charm);
                break;
            case 7: // 抗性强化: 2回合内负面判定+20%最终成功率
                s.HitFinalMod += 20;
                break;
        }
        return $"{m.Name} 对{s.Name}使用了令咒[{use}]";
    }

    // ---------- 工房战斗增益(工房书组件数值) ----------
    /// <summary>同灵脉工房对战斗的胜率加成(进攻方视角): 返回 (自有工房加成, 敌方工房惩罚)。
    /// 魔能重炮: 敌方-40%(宣言-60%); 集束光标: 己方从者+40%(非主力减半, 同灵脉再减半); 黑厄深阱: 己方主力抗性+10%。</summary>
    public (int Bonus, int Penalty) WorkshopBattleBonus(int leylineId)
    {
        int bonus = 0, penalty = 0;
        foreach (var ws in Workshops)
        {
            if (ws.Destroyed || ws.LeylineId != leylineId) continue;
            if (ws.Parts.Contains(WorkshopPart.MagicCannon))
                penalty = Math.Max(penalty, 40);
            if (ws.Parts.Contains(WorkshopPart.FocusBeacon))
                bonus += 40;
            if (ws.Parts.Contains(WorkshopPart.DarkPit))
                bonus = Math.Max(bonus, 0);   // 抗性由战斗内处理, 胜率不直接含
        }
        return (bonus, penalty);
    }

    /// <summary>工房/神殿组件描述文本(词典展示)。</summary>
    public static string PartEffectText(WorkshopPart p) => p switch
    {
        WorkshopPart.InfoBase => "[稳态20] 本灵脉自阵营[情报调查]+30%成功率, [资料分析]+20%",
        WorkshopPart.ResBase => "[稳态20] 工房持有者额外魔力池+50",
        WorkshopPart.AlchemyBase => "[稳态20] 本灵脉[礼装制作]+20%成功率",
        WorkshopPart.PowerArray => "[稳态30] 轮次结束工房持有者+30魔力(视同灵脉供魔)",
        WorkshopPart.MedicalUnit => "[稳态30] 回合开始本灵脉任一单位异常状态-3层",
        WorkshopPart.ConvergeDisc => "[稳态30] 本灵脉可使用的礼装上限+6",
        WorkshopPart.DarkPit => "[稳态40] 本灵脉自阵营战斗: 己方主力抗性+10%(宣言翻倍/敌方撤退FP+1)",
        WorkshopPart.MagicCannon => "[稳态40] 本灵脉自阵营战斗: 敌方主力-40%胜率(宣言-60%/本灵脉供魔归零)",
        WorkshopPart.SanctionOrg => "[稳态40] 敌方技能/宝具效果等级-1",
        WorkshopPart.AmplifyMod => "[稳态40] 工房主[类型:魔术]技能效果等级+1",
        WorkshopPart.TransPortal => "[稳态50] 随时转移工房魔力给任意单位",
        WorkshopPart.FocusBeacon => "[稳态50] 本灵脉自阵营战斗: 己方从者+40%胜率(非主力减半)",
        WorkshopPart.ArtifControl => "[稳态50] 获知任一战况; 工房灵脉上无视距离用令咒",
        WorkshopPart.ShrineSanctum => "[神殿] 本灵脉基础魔力量翻倍(全战争仅1座)",
        WorkshopPart.ShrineSanctuary => "[神殿] 神殿额外魔力池容量100(全战争仅1座)",
        WorkshopPart.ShrineSpirit => "[神殿] 圣杯降临时50%判定变更为在本神殿降临(全战争仅1座)",
        _ => "",
    };

    // ---------- 固有结界(世界层, 灵脉效果) ----------
    public record MarbleDef(int UnitId, int LeylineId, string Name);

    public List<MarbleDef> Marbles = new();

    /// <summary>解放固有结界(宝具效果联动): 在单位当前灵脉生成结界。</summary>
    public bool AddRealityMarble(int unitId, int leylineId, string name)
    {
        var u = W.Units.Find(x => x.Id == unitId);
        if (u == null || !u.Alive) return false;
        var ly = W.Leylines.Find(x => x.Id == leylineId);
        if (ly == null) return false;
        Marbles.RemoveAll(m => m.LeylineId == leylineId);
        Marbles.Add(new MarbleDef(unitId, leylineId, name));
        Log($"  · {u.Name} 解放固有结界「{name}」于「{ly.Name}」!");
        return true;
    }

    public bool HasMarble(int leylineId) => Marbles.Exists(m => m.LeylineId == leylineId);

    public string MarbleName(int leylineId) => Marbles.Find(m => m.LeylineId == leylineId)?.Name ?? "";

    /// <summary>固有结界战斗影响: 结界内撤退FP+1, 结界持有方+20%胜率(演示)。</summary>
    public (int RetreatPenalty, int WinBonus) MarbleBattleEffect(int leylineId, int defenderUnitId)
    {
        var m = Marbles.Find(x => x.LeylineId == leylineId);
        if (m == null) return (0, 0);
        int win = m.UnitId == defenderUnitId ? 20 : 0;
        return (1, win);
    }

    // ---------- 回合开始联动(供魔+工房) ----------
    public void BeginTurn()
    {
        MaintainWorkshops();
    }
}
