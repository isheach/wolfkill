using System;
using System.Collections.Generic;
using KsgGodot.Model;

namespace KsgGodot.Engine;

/// <summary>战斗单位(位)</summary>
public class BattleUnit
{
    public UnitDef Unit;
    public int Side;     // 1左 2右
    public int Slot;     // 1主力 2辅助 3仆役 4支援
    public int Uid => Unit.Id;
}

/// <summary>战术(强击>破袭>试探>扼守)</summary>
public enum Tactic { None = 0, Strike, Raid, Probe, Hold }

public static class TacticUtil
{
    public static string Name(Tactic t) => t switch
    {
        Tactic.Strike => "强击",
        Tactic.Raid => "破袭",
        Tactic.Probe => "试探",
        Tactic.Hold => "扼守",
        _ => "战术",
    };

    /// <summary>1=a克制b 0=无 -1=b克制a</summary>
    public static int Counter(Tactic a, Tactic b)
    {
        if (a == Tactic.None || b == Tactic.None || a == b) return 0;
        int[] next = { 0, (int)Tactic.Raid, (int)Tactic.Probe, (int)Tactic.Hold, (int)Tactic.Strike };
        if (next[(int)a] == (int)b) return 1;
        if (next[(int)b] == (int)a) return -1;
        return 0;
    }
}

/// <summary>指令</summary>
public enum Order { None = 0, Charge, Pursue, Cover, Duel }

/// <summary>战斗阶段: 1战斗开始 2初始 3主要 4最终 5结束</summary>
public class KsgBattle
{
    public bool Active;
    public int Leyline = 1;
    public LeylineDef LeylineRef;   // 当前战场灵脉(供人流量结算)
    public bool Day = true;
    public int Phase = 1;
    public int Width = 4;
    // 近似 C 版的“初次与敌方主力交战”状态；后续接入跨战斗历史时由上层覆盖。
    public bool FirstEncounter = true;

    /// <summary>战场宽度(规则书 3.2): 1~7 对应一方战斗位组成。</summary>
    public static string WidthDescription(int width)
    {
        string slots = width switch
        {
            1 => "主力位",
            2 => "主力位+支援位",
            3 => "主力位+辅助位+支援位",
            4 => "主力位+辅助位+仆役位+支援位",
            5 => "主力位+辅助位+辅助位+仆役位+支援位",
            6 => "主力位+辅助位×3+仆役位+支援位",
            7 => "主力位+辅助位×4+仆役位+支援位",
            _ => width > 0 ? $"宽度{width}(主力位+辅助位×{Math.Max(0, width - 3)}+仆役位+支援位)" : "未知",
        };
        return $"宽度{width}: {slots}";
    }

    /// <summary>该宽度下单方战斗位数量。</summary>
    public static int WidthCapacity(int width) => width switch
    {
        1 => 1, 2 => 2, 3 => 3, 4 => 4, 5 => 5, 6 => 6, 7 => 7, _ => 4,
    };

    /// <summary>规则书 3.2: 宽度 → 一方战斗位构成(1主力 2辅助 3仆役 4支援)。
    /// 宽2=[主力+支援];宽3=[主力+辅助+支援];宽4=[主力+辅助+仆役+支援];
    /// 宽n(n≥5)=[主力+辅助×(n-3)+仆役+支援]。</summary>
    public static int[] SlotTemplate(int width)
    {
        if (width <= 1) return new[] { 1 };                    // 宽1=[主力]
        if (width == 2) return new[] { 1, 4 };                 // 宽2=[主力+支援]
        var t = new List<int> { 1 };
        int aux = Math.Max(1, width - 3);                      // 宽3/4:1个辅助; 宽5+:n-3
        for (int i = 0; i < aux; i++) t.Add(2);
        if (width >= 4) t.Add(3);                              // 仆役位(仆役类单位占据, 否则降级为支援位)
        t.Add(4);                                              // 支援位
        return t.ToArray();
    }

    /// <summary>规则书 3.1.3/3.2: 仆役位只能由仆役单位(召唤/人偶/使魔 UType≥3)占据;
    /// 非仆役单位排到仆役位时, 该位降级为支援位使用(属性不计入战斗属性)。</summary>
    public static bool IsServantLike(UnitDef u) => u != null && u.UType >= 3;

    public List<BattleUnit> Left = new();
    public List<BattleUnit> Right = new();
    public int LeftMain, RightMain;

    public Tactic[] Tactics = new Tactic[3];        // 1左 2右
    public bool TacticsResolved;                   // 双方战术已结算(克制奖励只发一次)
    public bool MainAttrRolled;                    // 随机属性已掷
    public int[] MainAttr = new int[3];             // 1左 2右 主要属性索引
    public int RandAttr = -1;
    public int LeftWin, RightWin;
    public int LeftFinalWin, RightFinalWin;
    public int LeftFloor, RightFloor;
    public int LeftBase, RightBase;
    public bool LeftOk, RightOk;
    public int LeftRetreatTag, RightRetreatTag;   // 追击给对方叠加的撤退FP(仅+1,不重复)
    public int LeftRetreatTagUsed, RightRetreatTagUsed; // 追击是否已给目标叠加
    public bool LeftDuel, RightDuel;              // 死斗宣言:最终工序内无法撤退

    // 复杂机制触发器
    public List<(UnitDef Src, EffectLine El)> KillTriggers = new();
    public List<(UnitDef Src, EffectLine El)> WinTriggers = new();

    public event Action<string> LogEvent;
    public void Log(string msg) => LogEvent?.Invoke(msg);

    // ---------- 开始 ----------
    public void Start(KsgWorld w, int uidLeft, int uidRight, int width)
    {
        Active = true;
        Phase = 1;
        FirstEncounter = true;
        Width = width;
        Left.Clear(); Right.Clear();
        var a = FindUnit(w, uidLeft);
        var b = FindUnit(w, uidRight);
        if (a == null || b == null) { Log("单位不存在"); return; }
        a.BattleSide = 1; a.InBattle = true; a.BattleSlot = 1;
        b.BattleSide = 2; b.InBattle = true; b.BattleSlot = 1;
        Left.Add(new BattleUnit { Unit = a, Side = 1, Slot = 1 });
        Right.Add(new BattleUnit { Unit = b, Side = 2, Slot = 1 });
        LeftMain = a.Id; RightMain = b.Id;
        foreach (var u in new[] { a, b })
        {
            u.SettleMp();
            u.ResetAllRecast();   // 参战前回转重置为0(待积累)
            u.ItemUsesThisRound = 0;   // 礼装每轮次数重置
            // 预设:宝具回转给一半, 让战斗中有使用机会(演示友好)
            foreach (var p in u.Phantasms) p.CurRecast = p.Recast / 2;
        }
        Log($"━━━━━━━━ 战斗开始 {a.Name} vs {b.Name} ━━━━━━━━");
        Log($"战场:灵脉{leyline()} 宽度{Width} ({WidthDescription(Width)}) {(Day ? "[昼]" : "[夜]")}");
        Log($"左方:主力 {a.Name} | 右方:主力 {b.Name}");
        Log($"[战场] 单方容量 {WidthCapacity(Width)} 战斗位(主力位必有;多余单位在战斗开始时被移出)");
        ApplyLevelWinBonus();
    }

    /// <summary>多单位开战: 每方最多宽度容量个单位, 按规则书 3.2 槽位模板分配
    /// [主力, 辅助×n, 仆役, 支援]; 非仆役单位不占用仆役位(该位降级为支援位)。
    /// 超容量的单位在战斗开始时被移出战斗位。</summary>
    public void StartParty(KsgWorld w, IReadOnlyList<UnitDef> leftSide, IReadOnlyList<UnitDef> rightSide, int width)
    {
        Active = true;
        Phase = 1;
        FirstEncounter = true;
        Width = width;
        Left.Clear(); Right.Clear();

        void PlaceSide(List<UnitDef> sideUnits, int side, List<BattleUnit> target)
        {
            if (sideUnits == null || sideUnits.Count == 0) return;
            int cap = WidthCapacity(Width);
            int[] tmpl = SlotTemplate(Width);
            int count = Math.Min(cap, sideUnits.Count);
            int si = 0; // 模板下标
            for (int i = 0; i < count; i++)
            {
                UnitDef u = sideUnits[i];
                if (u == null) continue;
                if (si >= tmpl.Length)
                {
                    Log($"{u.Name} 超出战斗位容量({cap}),本次战斗不参战");
                    u.InBattle = false;
                    u.BattleSide = 0;
                    u.BattleSlot = 0;
                    continue;
                }
                int slot = tmpl[si];
                si++;
                // 仆役位只能由仆役单位占据;非仆役单位排到仆役位时,该位降级为支援位(规则书 3.2)
                if (slot == 3 && !IsServantLike(u))
                {
                    Log($"{u.Name} 非仆役单位,仆役位降级为支援位(不贡献战斗属性)");
                    slot = 4;
                }
                u.BattleSide = side;
                u.InBattle = true;
                u.BattleSlot = slot;
                target.Add(new BattleUnit { Unit = u, Side = side, Slot = slot });
            }
        }

        PlaceSide(new List<UnitDef>(leftSide), 1, Left);
        PlaceSide(new List<UnitDef>(rightSide), 2, Right);
        if (Left.Count == 0 || Right.Count == 0)
        {
            Log("任一方无单位,无法开始战斗");
            Active = false;
            return;
        }
        LeftMain = Left[0].Uid;
        RightMain = Right[0].Uid;

        foreach (var bu in Left) PrepUnit(bu.Unit);
        foreach (var bu in Right) PrepUnit(bu.Unit);

        Log($"━━━━━━━━ 战斗开始 左方{Left.Count} vs 右方{Right.Count} ━━━━━━━━");
        Log($"战场:灵脉{leyline()} 宽度{Width} ({WidthDescription(Width)}) {(Day ? "[昼]" : "[夜]")}");
        Log($"左方: {LeftUnitsDesc(Left)}");
        Log($"右方: {RightUnitsDesc(Right)}");
        Log($"[战场] 单方容量 {WidthCapacity(Width)} 战斗位;超容量的单位在本次战斗中不参战");
        ApplyLevelWinBonus();
    }

    /// <summary>规则书 第六节: 处于[主力位]的单位的等级提供 [+等级×1%] 胜率补正。
    /// 记录当前主力等级(Start/StartParty 时初始化; 主力退场替补时重算), 在电池计算时计入。</summary>
    public int LeftLevelBonus, RightLevelBonus;

    public void ApplyLevelWinBonus()
    {
        LeftLevelBonus = Left.Count > 0 ? Left[0].Unit.Level : 0;
        RightLevelBonus = Right.Count > 0 ? Right[0].Unit.Level : 0;
        Log($"     · 主力位等级补正: 左方 +{LeftLevelBonus}% / 右方 +{RightLevelBonus}% (规则书第六节,仅计入战斗属性后)");
    }

    private void PrepUnit(UnitDef u)
    {
        u.SettleMp();
        u.ResetAllRecast();
        u.ItemUsesThisRound = 0;
        foreach (var p in u.Phantasms) p.CurRecast = p.Recast / 2;
    }

    private static string LeftUnitsDesc(List<BattleUnit> side) => string.Join("、", side.ConvertAll(b => b.Unit.Name + "(" + SlotName(b.Slot) + ")"));
    private static string RightUnitsDesc(List<BattleUnit> side) => string.Join("、", side.ConvertAll(b => b.Unit.Name + "(" + SlotName(b.Slot) + ")"));

    /// <summary>战斗位容量(规则书 3.2): 宽度不足时多余单位不允许参战。</summary>
    public bool CanEnterBattle(int side) => (side == 1 ? Left.Count : Right.Count) < WidthCapacity(Width);

    private int leyline() => 1;

    private UnitDef FindUnit(KsgWorld w, int id)
    {
        foreach (var u in w.Units) if (u.Id == id) return u;
        return null;
    }

    // ---------- 战术 ----------
    public void SetTactic(int side, Tactic t)
    {
        if (side < 1 || side > 2) return;
        if (Tactics[side] == t) return;                 // 重复点击同战术:无变化
        if (TacticsResolved) return;                    // 已结算后禁止修改
        Tactics[side] = t;
        Log($"{SideName(side)} 选择战术:{TacticUtil.Name(t)}");
        // 双方都选完才结算一次克制奖励
        if (Tactics[1] != Tactic.None && Tactics[2] != Tactic.None && !TacticsResolved)
        {
            TacticsResolved = true;
            int c = TacticUtil.Counter(Tactics[1], Tactics[2]);
            if (c > 0)
            {
                Log($"[战术克制] 左方{TacticUtil.Name(Tactics[1])} 克制 右方! 左方主力位 +20%胜率");
                AddWin(1, 20);
            }
            else if (c < 0)
            {
                Log($"[战术克制] 右方{TacticUtil.Name(Tactics[2])} 克制 左方! 右方主力位 +20%胜率");
                AddWin(2, 20);
            }
        }
    }

    // ---------- 战斗属性 ----------
    public void PickMainAttr(int side, int attr)
    {
        MainAttr[side] = attr;
        Log($"{SideName(side)} ▸ 主要属性:{AttrName(attr)}");
    }

    public bool MainAttrSet()
    {
        return MainAttr[1] >= 0 && MainAttr[2] >= 0;
    }

    public void RollRandAttr()
    {
        RandAttr = Dice.Next(6);
        Log($"✦ 随机属性:{AttrName(RandAttr)}");
    }

    public void BatteryCheck()
    {
        if (SeaMode) { SeaBatteryCheck(); return; }
        int lv = 0, rv = 0;
        lv = SideAttrTotal(1);
        rv = SideAttrTotal(2);
        if (lv > rv) { LeftBase = lv - rv; RightBase = 0; }
        else { LeftBase = 0; RightBase = rv - lv; }
        LeftWin = LeftBase + LeftLevelBonus;
        RightWin = RightBase + RightLevelBonus;
        Log($"[战斗属性] 左方总值{lv} 右方总值{rv} → 基础胜率 左{LeftBase}% 右{RightBase}% (含主力等级补正 左{LeftLevelBonus}%/右{RightLevelBonus}%)");
    }

    private int SideAttrTotal(int side)
    {
        var list = side == 1 ? Left : Right;
        int main = MainAttr[side];
        int total = 0;
        foreach (var bu in list)
        {
            UnitDef u = bu.Unit;
            int v1 = (main >= 0 && main < 6) ? u.AttrValue(main) : 0;
            int v2 = RandAttr >= 0 ? u.AttrValue(RandAttr) : 0;
            switch (bu.Slot)
            {
                case 1: total += v1 + v2; break;
                case 2:
                case 3: total += (v1 + v2) / 2; break;
            }
        }
        return total;
    }

    public void AddWin(int side, int n)
    {
        if (side == 1) LeftWin += n;
        else RightWin += n;
        Log($"[胜率修正] {SideName(side)} {n:+0;-0}%(现 {LeftWin}% vs {RightWin}%)");
    }

    // ---------- 大航海战斗表结算(可切换) ----------

    /// <summary>启用《大航海战斗表》结算链: 优劣组合表基础胜率 + clamp(50+差值/2) 最终胜率。</summary>
    public bool SeaMode;
    /// <summary>对抗属性(表内 属性A/B/C; 默认 筋力/耐久/敏捷)。</summary>
    public int[] SeaPickAttr = { 0, 1, 2 };
    /// <summary>手动优劣调整(每点 = ±10 战力, 逐属性)。</summary>
    public int[] SeaBias = new int[3];
    /// <summary>辅助胜率正常(true=战前补正全额求和; false=辅助减半)。</summary>
    public bool SeaAuxRateNormal = true;
    public int SeaLeftBattleValue, SeaRightBattleValue;
    public int SeaLeftHalf, SeaRightHalf;
    public double SeaLeftDiff;
    public System.Collections.Generic.List<SeaSettle.Row> SeaRows = new();

    /// <summary>大航海式结算: ①魔力/扣减/保底 ②属性总值(主力+辅助÷2)/战斗属性 ③优劣→基础胜率→最终胜率。</summary>
    public void SeaBatteryCheck()
    {
        SeaSettle.Side a = BuildSeaSide(Left, "左方");
        SeaSettle.Side b = BuildSeaSide(Right, "右方");
        a.AuxRateNormal = SeaAuxRateNormal;
        b.AuxRateNormal = SeaAuxRateNormal;
        // 战斗内已累积的胜率修正(战术克制/指令/技能)折算进主力「战前胜率」栏; 底限与最终修正在 Effective() 中照常生效
        if (a.Main != null) a.Main.PreWin = LeftWin;
        if (b.Main != null) b.Main.PreWin = RightWin;
        a.FloorRate = LeftFloor;
        b.FloorRate = RightFloor;

        var cfg = new SeaSettle.Picks(2) { Attr = SeaPickAttr };
        for (int i = 0; i < SeaPickAttr.Length && i < 3; i++)
        {
            cfg.Bias[0][i] = SeaBias[i];
            cfg.Bias[1][i] = SeaBias[i];
        }
        (SeaSettle.SideResult ra, SeaSettle.SideResult rb) = SeaSettle.Settle2(a, b, cfg);
        SeaRows = SeaSettle.Report2(ra, rb, cfg);
        SeaLeftBattleValue = (int)Math.Round(ra.BattleValue);
        SeaRightBattleValue = (int)Math.Round(rb.BattleValue);
        SeaLeftHalf = ra.HalfRate;
        SeaRightHalf = rb.HalfRate;
        SeaLeftDiff = ra.Diff;
        LeftWin = ra.FinalRate;
        RightWin = rb.FinalRate;

        Log($"[大航海] 属性总值 左[{SeaAttrTotalText(ra)}] 右[{SeaAttrTotalText(rb)}]");
        Log($"[大航海] 战斗属性 {SeaAttrPickText(cfg, ra)} = 左{SeaLeftBattleValue} / 右{SeaRightBattleValue}");
        Log($"[大航海] 基础胜率(优劣组合) 左{ra.BaseRate}% : 右{rb.BaseRate}%");
        Log($"[大航海] 胜率合计 左{ra.Chain:0.#} : 右{rb.Chain:0.#} → 差值 {ra.Diff:0.#}");
        Log($"[大航海] 最终胜率 左{ra.FinalRate}% : 右{rb.FinalRate}% (差值减半版 左{ra.HalfRate}% : 右{rb.HalfRate}%)");
    }

    private static string SeaAttrTotalText(SeaSettle.SideResult r)
    {
        var parts = new System.Collections.Generic.List<string>();
        for (int i = 0; i < SeaSettle.AttrCount; i++)
            parts.Add($"{SeaSettle.AttrName(i)}{r.Total[i]:0.#}");
        return string.Join(" ", parts);
    }

    private static string SeaAttrPickText(SeaSettle.Picks cfg, SeaSettle.SideResult r)
    {
        var parts = new System.Collections.Generic.List<string>();
        for (int p = 0; p < cfg.Count && p < 4; p++)
        {
            string verdict = r.Verdict[p] switch { 3 => "优", 2 => "平", _ => "劣" };
            parts.Add($"{SeaSettle.AttrName(cfg.Attr[p])}{r.PickValue[p]:0.#}({verdict})");
        }
        return string.Join(" ", parts);
    }

    /// <summary>战斗位 → 结算参战位: 主力位=主力; 辅助/仆役位=辅助(减半); 支援位不贡献属性(规则书 3.2)。</summary>
    private static SeaSettle.Side BuildSeaSide(System.Collections.Generic.List<BattleUnit> list, string name)
    {
        var side = new SeaSettle.Side { Name = name };
        foreach (BattleUnit bu in list)
        {
            if (bu.Slot == 4) continue;
            side.Members.Add(new SeaSettle.Member
            {
                Unit = bu.Unit,
                Slot = bu.Slot == 1 ? 0 : side.Members.Count,
            });
        }
        return side;
    }

    public void AddFinalWin(int side, int n)
    {
        if (side == 1) LeftFinalWin += n; else RightFinalWin += n;
        Log($"[最终胜率修正] {SideName(side)} {n:+0;-0}%");
    }

    public void AddFloor(int side, int n)
    {
        if (side == 1) LeftFloor += n; else RightFloor += n;
        Log($"[底限胜率] {SideName(side)} {n:+0;-0}%");
    }

    // ---------- 指令 ----------
    public void IssueOrder(int side, Order o)
    {
        UnitDef u = GetMain(side);
        UnitDef e = GetMain(side == 1 ? 2 : 1);
        if (u == null || e == null) return;
        switch (o)
        {
            case Order.Charge:
                Log($"⚔ {u.Name} 宣言[冲锋]!");
                int at = Dice.Next(4);   // 筋/耐/敏/幸
                int diff = u.AttrValue(at) - e.AttrValue(at);
                int rate = Math.Clamp(50 + diff, 0, 100);
                Log($"     · 判定属性:{AttrName(at)} 差={diff} 成功率={rate}%");
                if (Dice.RollPct(rate))
                {
                    Log("     · 冲锋成功!+20%胜率补正");
                    AddWin(side, 20);
                }
                else
                {
                    Log("     · 冲锋失败!-10%底限穿透");
                    AddFloor(side, -10);
                }
                break;
            case Order.Pursue:
                Log($"→ {u.Name} 宣言[追击]! 目标{e.Name}撤退消耗FP+1");
                // 追击只增加对方撤退成本一次
                if (side == 1 && LeftRetreatTagUsed == 0) { RightRetreatTag += 1; LeftRetreatTagUsed = 1; }
                if (side == 2 && RightRetreatTagUsed == 0) { LeftRetreatTag += 1; RightRetreatTagUsed = 1; }
                break;
            case Order.Cover:
                Log($"🛡 {u.Name} 宣言[掩护]! 替代目标承受冲锋/追击,-20%胜率");
                AddWin(side, -20);
                break;
            case Order.Duel:
                if (side == 1) LeftDuel = true; else RightDuel = true;
                Log($"☠ {u.Name} 宣言[死斗]!+20%胜率, 最终工序内无法撤退");
                AddWin(side, 20);
                break;
        }
    }

    // ---------- 撤退 ----------
    public bool Retreat(int side)
    {
        UnitDef u = GetMain(side);
        if (u == null) return false;
        // 死斗宣言后禁止撤退(最终工序内无法撤退,若撤退尝试失败记录日志)
        if (side == 1 && LeftDuel)
        {
            Log($"✖ {u.Name} 已宣言[死斗],最终工序内无法撤退!");
            return false;
        }
        if (side == 2 && RightDuel)
        {
            Log($"✖ {u.Name} 已宣言[死斗],最终工序内无法撤退!");
            return false;
        }
        int cost = Phase switch { 2 => 0, 3 => 1, 4 => 2, 5 => 3, _ => 1 };
        cost += side == 1 ? LeftRetreatTag : RightRetreatTag;
        int avail = u.Fp + u.TpFp;
        if (avail < cost)
        {
            Log($"✖ {u.Name} 撤退失败!FP不足(需{cost},持有{avail})");
            return false;
        }
        int pay = cost;
        if (u.TpFp >= pay) { u.TpFp -= pay; pay = 0; }
        else { pay -= u.TpFp; u.TpFp = 0; u.Fp -= pay; }
        Log($"↩ {u.Name} 宣言撤退!消耗FP{cost}(剩余{u.Fp}),进入游荡状态");
        u.InBattle = false;
        u.BattleSide = 0;
        u.BattleSlot = 0;
        u.Roaming = 1;
        var list = side == 1 ? Left : Right;
        list.RemoveAll(b => b.Uid == u.Id);
        if (side == 1 && Left.Count > 0)
        {
            // 规则书 3.2: 主力位退场/撤退 → 辅助→仆役→支援 顺序替补进主力位
            Left[0].Slot = 1; Left[0].Unit.BattleSlot = 1; LeftMain = Left[0].Uid;
            // 规则书 第六节: 等级胜率补正切换到新主力
            int newBonus = Left[0].Unit.Level;
            if (newBonus != LeftLevelBonus)
            {
                AddWin(1, newBonus - LeftLevelBonus);
                Log($"     · 主力位等级补正更新: 左方新主力{Left[0].Unit.Name} +{newBonus}%");
                LeftLevelBonus = newBonus;
            }
        }
        if (side == 2 && Right.Count > 0)
        {
            Right[0].Slot = 1; Right[0].Unit.BattleSlot = 1; RightMain = Right[0].Uid;
            int newBonus = Right[0].Unit.Level;
            if (newBonus != RightLevelBonus)
            {
                AddWin(2, newBonus - RightLevelBonus);
                Log($"     · 主力位等级补正更新: 右方新主力{Right[0].Unit.Name} +{newBonus}%");
                RightLevelBonus = newBonus;
            }
        }
        if (list.Count == 0)
        {
            Log("□ 一方战斗位已空,战斗立即结束");
            if (side == 1) { RightOk = true; LeftOk = false; }
            else { LeftOk = true; RightOk = false; }
            return true;
        }
        return false;
    }

    // ---------- 胜率决胜 ----------
    public void Effective()
    {
        int lw = Math.Clamp(LeftWin, 0, 100);
        int rw = Math.Clamp(RightWin, 0, 100);
        if (LeftFloor > 0 && lw < LeftFloor) lw = LeftFloor;
        if (RightFloor > 0 && rw < RightFloor) rw = RightFloor;
        lw = Math.Clamp(lw + LeftFinalWin, 0, 100);
        rw = Math.Clamp(rw + RightFinalWin, 0, 100);
        LeftWin = lw; RightWin = rw;
        Log($"【最终胜率】左方 {lw}% : 右方 {rw}%");
        int r = Dice.Roll();
        bool lWin;
        if (lw == rw) lWin = r <= 50;
        else if (lw > rw) lWin = r > rw;
        else lWin = r <= lw;
        Log($"【决胜检定】骰 {r} → {(lWin ? "左方胜!" : "右方胜!")}");
        LeftOk = lWin; RightOk = !lWin;
    }

    // ---------- 结束 ----------
    public void End(KsgWorld w)
    {
        Log("════════ 战斗结束 ════════");
        // 等级魔耗
        foreach (var bu in Left) { SettleUnit(bu.Unit); }
        foreach (var bu in Right) { SettleUnit(bu.Unit); }
        // 胜利触发器(胜方触发 OnWin 效果)
        FireWinTriggers(w);
        // 俘虏/摧毁
        HandleEnd(LeftOk, Left, Right, "左方");
        HandleEnd(RightOk, Right, Left, "右方");
        foreach (var bu in Left) bu.Unit.InBattle = false;
        foreach (var bu in Right) bu.Unit.InBattle = false;
        Active = false;
    }

    /// <summary>战斗胜利时结算 OnWin 触发器(胜方单位的效果)。</summary>
    private void FireWinTriggers(KsgWorld w)
    {
        if (WinTriggers.Count == 0) return;
        foreach (var (src, el) in WinTriggers)
        {
            if (!src.InBattle) continue;
            int side = src.BattleSide;
            bool sideWon = side == 1 ? LeftOk : RightOk;
            if (sideWon && el.Flag == EffFlag.OnWin)
            {
                var sub = new EffectLine { Flag = EffFlag.WinUp, Value = el.Value, Target = -2 };
                KsgEffects.ApplyEffect(this, w, src, src, sub);
                Log($"  · {src.Name} 胜利触发:胜率+{el.Value}%");
            }
        }
        WinTriggers.Clear();
    }

    /// <summary>单位退场/被击杀时结算 OnKill 触发器(击杀者效果)。</summary>
    public void FireKillTriggers(KsgWorld w, int killerSide)
    {
        if (KillTriggers.Count == 0) return;
        var remain = new List<(UnitDef, EffectLine)>();
        foreach (var (src, el) in KillTriggers)
        {
            // 触发器属于击杀方
            if (src.BattleSide == killerSide && el.Flag == EffFlag.OnKill)
            {
                UnitDef tgt = src;
                if (el.Target == 0)
                {
                    var list = killerSide == 1 ? Right : Left;
                    foreach (var bu in list) KsgEffects.ApplyEffect(this, w, bu.Unit, src, new EffectLine { Flag = EffFlag.WinDown, Value = el.Value, Target = 0 });
                }
                else
                {
                    KsgEffects.ApplyEffect(this, w, tgt, src, new EffectLine { Flag = EffFlag.WinUp, Value = el.Value, Target = -2 });
                }
                Log($"  · {src.Name} 击杀触发:效果{el.Value}");
            }
            else remain.Add((src, el));
        }
        KillTriggers = remain;
    }

    private void SettleUnit(UnitDef u)
    {
        int cost = u.Level / 2;
        u.MpCur = Math.Max(u.MpCur - cost, u.MpFloor);
        u.SettleMp();
    }

    private void HandleEnd(bool sideWon, List<BattleUnit> winners, List<BattleUnit> losers, string winName)
    {
        if (!sideWon) return;
        UnitDef main = winners.Count > 0 ? winners[0].Unit : null;
        foreach (var lb in losers)
        {
            var u = lb.Unit;
            if (u.UType == 3 || u.UType == 4)
            {
                Log($"{u.Name}(召唤/人偶) 战败,被摧毁");
                u.Alive = false;
            }
            else
            {
                Log($"{u.Name} 成为俘虏(归{winName}主力位 {main?.Name})");
                u.Roaming = 1;
            }
        }
    }

    // ---------- 工具 ----------
    public UnitDef GetMain(int side) => side == 1 ? UnitById(LeftMain) : UnitById(RightMain);

    public UnitDef UnitById(int id)
    {
        foreach (var bu in Left) if (bu.Uid == id) return bu.Unit;
        foreach (var bu in Right) if (bu.Uid == id) return bu.Unit;
        return null;
    }

    public static string SideName(int s) => s == 1 ? "左方" : "右方";
    public static string AttrName(int a) => new[] { "筋力", "耐久", "敏捷", "魔力", "幸运", "宝具" }[Math.Clamp(a, 0, 5)];
    public static string SlotName(int s) => s switch { 1 => "主力位", 2 => "辅助位", 3 => "仆役位", 4 => "支援位", _ => "未参战" };
}

internal static class ResetExt
{
    public static void ResetAllRecast(this UnitDef u)
    {
        foreach (var s in u.Skills) s.ResetRecast();
        foreach (var p in u.Phantasms) p.ResetRecast();
    }
}
