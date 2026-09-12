using System;
using System.Collections.Generic;
using KsgGodot.Model;

namespace KsgGodot.Engine;

/// <summary>
/// 大航海战斗表式战斗结算(移植自《大航海战斗表》v2.35 公式)。
///
/// 工序:
///   ① 初始工序: 决定参战人选 → 魔力结算(当前+礼装/令咒补充-消耗) → 魔力不足扣减 → 角色保底
///   ② 主要工序: 属性总值(主力全额 + 辅助÷2) → 选定战斗属性 → 手动/自动优劣 → 战斗属性总值
///   ③ 最终工序: 基础胜率(优劣组合表) → 胜率链合计 → 差值 → 最终胜率 clamp(50+差值/2)
///
/// 纯计算模块: 不引用 Godot, 可被引擎自测(工具工程)直接编译引用。
/// </summary>
public static class SeaSettle
{
    public const int AttrCount = 6;
    /// <summary>属性名(与 UnitDef.Attr 索引一致)。</summary>
    public static readonly string[] AttrNames = { "筋力", "耐久", "敏捷", "魔力", "幸运", "宝具" };
    /// <summary>单人魔力不足扣减步长: 每 -20 魔力。</summary>
    public const int MpStep = 20;
    /// <summary>单人魔力不足扣减: 每步扣属性。</summary>
    public const int MpStepPenalty = 10;
    public const int MaxAux = 4;                 // 主力 + 辅助×4 (五人版)
    public const int ThreeWayPool = 150;         // 三方保有胜率总分

    // ==================== 输入 ====================

    /// <summary>参战位(主力或辅助)。</summary>
    public class Member
    {
        public UnitDef Unit;
        public int Slot;                 // 0=主力 1..4=辅助
        public int Spend;                // 本场魔力消耗(GM 填写)
        public int PreWin, PreLoss;      // 战前胜率补正 / 惩罚(逐位)

        // ---- 计算结果 ----
        public int FinalMp;              // 最终魔力 = 当前魔力 + 礼装/令咒补充 - 消耗
        public int Deficit;              // 魔力不足扣减(<=0)
        public readonly int[] Attr = new int[AttrCount];   // 保底后的属性

        public bool IsMain => Slot == 0;
    }

    /// <summary>一方(组别)。</summary>
    public class Side
    {
        public string Name = "";
        public readonly List<Member> Members = new();

        public int MpGift;               // 礼装补充
        public int MpCommand;            // 令咒补充
        public bool SoloAction;          // 单独行动(魔力不足扣减减半)
        public bool AuxRateNormal = true;// 辅助胜率正常(false → 辅助减半)

        public int WinBonus, WinPenalty; // 初始工序: 胜率补正 / 惩罚
        public int PickWin, PickLoss;    // 主要工序: 胜率补正 / 惩罚
        public int Extra1, Extra2;       // 补充胜率(两个栏位)
        public int FloorRate;            // 底限胜率

        public bool StarPioneer;         // 星之开拓者(三方: 等差归零)
        public int KeepBase;             // 保有胜率基准(三方)

        public Member Main => Members.Count > 0 ? Members[0] : null;
    }

    /// <summary>属性选择与补正(全局共享: 双方对抗同一组属性)。</summary>
    public class Picks
    {
        public int[] Attr = { 0, 1, 2 };          // 对抗属性索引(2方3个; 三方四人版可4个)
        public int[][] Bias;                      // 各方的手动优劣调整(每点 = +10)
        public int[] Bonus1 = new int[4];         // 补正一(宝具不适用)
        public int[] Bonus2 = new int[4];         // 补正二

        public Picks(int sides)
        {
            Bias = new int[sides][];
            for (int i = 0; i < sides; i++) Bias[i] = new int[4];
        }

        public int Count => Attr != null ? Attr.Length : 0;
    }

    // ==================== 输出 ====================

    public class SideResult
    {
        public Side S;
        public readonly double[] Total = new double[AttrCount];  // 属性总值
        public int LevelTotal;                                   // 等级总值(仅主力等级)
        public int PreRate;                                      // 战前胜率(补-惩)
        public int InitRate;                                     // 初始工序胜率(补-惩)
        public int MainRate;                                     // 主要工序胜率(补-惩)
        public readonly double[] PickValue = new double[4];       // 选定属性的战力值
        public readonly int[] Verdict = new int[4];               // 3优 / 2平 / 1劣
        public double BattleValue;                               // 战斗属性总值
        public int BaseRate;                                     // 基础胜率
        public double Chain;                                     // 胜率合计
        public double Diff;                                      // 胜率差值
        public int FinalRate;                                    // 最终胜率
        public int HalfRate;                                     // 差值减半后的胜率
        public int HalfDiff;                                     // 差值(减半后)

        // ---- 三方专有 ----
        public double LevelTerm;        // 等差(等级差加乘)
        public int QuadScore;           // 四维优劣得分
        public double PowerTerm;        // 战力差加乘
        public double Relative;         // 相对胜率
        public double KeepRate;         // 保有胜率
        public bool Excluded;           // 胜率为负被排除

        public Member Main => S?.Main;
    }

    /// <summary>展示行(供 UI 渲染成工序表格)。</summary>
    public class Row
    {
        public string Phase = "";
        public string Label = "";
        public string[] Cells = Array.Empty<string>();
        public bool Head;
        public Row(string phase, string label, bool head, params string[] cells)
        {
            Phase = phase; Label = label; Cells = cells; Head = head;
        }
    }

    // ==================== ① 初始工序 ====================

    /// <summary>魔力结算与魔力不足扣减; 结果写回 Member.FinalMp / Deficit / Attr。</summary>
    public static void Prepare(Side s)
    {
        foreach (Member m in s.Members)
        {
            int cur = m.Unit != null ? m.Unit.MpCur : 0;
            m.FinalMp = cur + s.MpGift + s.MpCommand - m.Spend;
            // 大航海表: 每不足 20 点魔力 → 除宝具外全属性 -10; 单独行动减半
            int deficit = m.FinalMp < 0
                ? (int)(Math.Floor(Math.Abs(m.FinalMp) / (double)MpStep) * MpStepPenalty)
                : 0;
            if (s.SoloAction) deficit /= 2;
            m.Deficit = -deficit;

            for (int a = 0; a < AttrCount; a++)
            {
                int v = (m.Unit != null ? m.Unit.AttrValue(a) : 0) + (a == 5 ? 0 : m.Deficit);
                m.Attr[a] = Math.Max(0, v);      // 角色保底: 不低于 0
            }
        }
    }

    /// <summary>战前胜率汇总: 补正按「主力全额 / 辅助减半(可选)」, 惩罚全额求和(表内 H26/H27)。</summary>
    private static void SumPreRate(Side s, out int preRate)
    {
        double win = 0, loss = 0;
        foreach (Member m in s.Members)
        {
            double w = m.PreWin;
            if (!m.IsMain && !s.AuxRateNormal) w /= 2.0;
            win += w;
            loss += m.PreLoss;
        }
        preRate = (int)Math.Round(win - loss);
    }

    // ==================== ② 主要工序 ====================

    /// <summary>属性总值 = 主力全额 + Σ辅助÷2; 等级总值 = 仅主力等级。</summary>
    public static SideResult Compute(Side s)
    {
        Prepare(s);
        var r = new SideResult { S = s };
        for (int a = 0; a < AttrCount; a++)
        {
            double total = 0;
            foreach (Member m in s.Members)
                total += m.IsMain ? m.Attr[a] : m.Attr[a] / 2.0;
            r.Total[a] = total;
        }
        r.LevelTotal = s.Main?.Unit?.Level ?? 0;
        SumPreRate(s, out int pre);
        r.PreRate = pre;
        r.InitRate = s.WinBonus - s.WinPenalty;
        r.MainRate = s.PickWin - s.PickLoss;
        return r;
    }

    /// <summary>选定属性的战力值: 总值 + 手动优劣×10 + 补正一(宝具除外) + 补正二, 保底 0。</summary>
    private static void ComputePicks(SideResult r, Picks cfg, int sideIndex)
    {
        double sum = 0;
        for (int p = 0; p < cfg.Count && p < 4; p++)
        {
            int a = cfg.Attr[p];
            double v = r.Total[Math.Clamp(a, 0, AttrCount - 1)]
                       + cfg.Bias[sideIndex][p] * 10
                       + (a != 5 ? cfg.Bonus1[p] : 0)
                       + cfg.Bonus2[p];
            r.PickValue[p] = Math.Max(0, v);
            sum += r.PickValue[p];
        }
        r.BattleValue = sum;
    }

    /// <summary>基础胜率: 优劣组合表(表内 E98)。v=3优 2平 1劣。</summary>
    public static int BaseRateByVerdicts(int[] verdicts, int count)
    {
        int sum = 0, hasSup = 0, hasInf = 0;
        for (int i = 0; i < count && i < verdicts.Length; i++)
        {
            sum += verdicts[i];
            if (verdicts[i] == 3) hasSup++;
            if (verdicts[i] == 1) hasInf++;
        }
        return sum switch
        {
            9 => 90,                                        // 3优
            8 => 80,                                        // 2优1平
            7 => hasInf > 0 ? 70 : 60,                      // 2优1劣 / 1优2平
            6 => 50,                                        // 3平 / 1优1平1劣
            5 => hasSup > 0 ? 30 : 40,                      // 1优2劣 / 2平1劣
            4 => 20,                                        // 1平2劣
            3 => 10,                                        // 3劣
            _ => 50,
        };
    }

    // ==================== ③ 最终工序: 双方 ====================

    /// <summary>双方结算(1v1)。返回 (左, 右)。</summary>
    public static (SideResult A, SideResult B) Settle2(Side a, Side b, Picks cfg)
    {
        SideResult ra = Compute(a);
        SideResult rb = Compute(b);
        ComputePicks(ra, cfg, 0);
        ComputePicks(rb, cfg, 1);

        int n = Math.Min(cfg.Count, 4);
        for (int p = 0; p < n; p++)
        {
            ra.Verdict[p] = ra.PickValue[p] > rb.PickValue[p] ? 3
                          : ra.PickValue[p] == rb.PickValue[p] ? 2 : 1;
            rb.Verdict[p] = 4 - ra.Verdict[p];               // 优↔劣 互补(平保持 2)
        }
        ra.BaseRate = BaseRateByVerdicts(ra.Verdict, n);
        rb.BaseRate = 100 - ra.BaseRate;                     // 表内 E99 = 100 - E98

        // 胜率合计 = 主力等级 + 基础胜率 + 战斗属性总值 + 战前 + 初始 + 主要 + 补充×2
        ra.Chain = ra.LevelTotal + ra.BaseRate + ra.BattleValue + ra.PreRate + ra.InitRate + ra.MainRate + a.Extra1 + a.Extra2;
        rb.Chain = rb.LevelTotal + rb.BaseRate + rb.BattleValue + rb.PreRate + rb.InitRate + rb.MainRate + b.Extra1 + b.Extra2;

        ra.Diff = ra.Chain - rb.Chain;
        rb.Diff = -ra.Diff;
        ra.HalfDiff = (int)Math.Round(ra.Diff / 2.0);
        rb.HalfDiff = -ra.HalfDiff;

        ra.FinalRate = ClampRate(50 + ra.Diff / 2.0, a.FloorRate, b.FloorRate);
        ra.HalfRate = ClampRate(50 + ra.Diff / 4.0, a.FloorRate, b.FloorRate);
        rb.FinalRate = 100 - ra.FinalRate;                   // 表内 O99 = 100 - O98
        rb.HalfRate = 100 - ra.HalfRate;
        return (ra, rb);
    }

    /// <summary>最终胜率: clamp(50 + 差值/2, 本方底限, 100 - 对方底限), 负值归 0(表内 O98)。</summary>
    private static int ClampRate(double v, int floorSelf, int floorOther)
    {
        double hi = 100 - floorOther;
        double r = Math.Min(v, hi);
        if (r < floorSelf) r = floorSelf;
        if (r < 0) r = 0;
        if (r > 100) r = 100;
        return (int)Math.Round(r);
    }

    // ==================== ③ 最终工序: 三方混战 ====================

    /// <summary>三方混战结算(按 组A/组B/组C 顺序)。</summary>
    public static List<SideResult> Settle3(Side[] sides, Picks cfg)
    {
        if (sides == null || sides.Length != 3) throw new ArgumentException("三方混战需要 3 方");
        var rs = new List<SideResult>(3);
        for (int i = 0; i < 3; i++)
        {
            SideResult r = Compute(sides[i]);
            ComputePicks(r, cfg, i);
            rs.Add(r);
        }

        int n = Math.Min(cfg.Count, 4);
        // 四维优劣: 单属性 一优二劣=30 / 二平一劣=15 / 三平=10, 四属性求和 +10
        for (int i = 0; i < 3; i++)
        {
            int score = 0;
            for (int p = 0; p < n; p++)
            {
                double mine = rs[i].PickValue[p];
                double o1 = rs[(i + 1) % 3].PickValue[p];
                double o2 = rs[(i + 2) % 3].PickValue[p];
                if (mine > o1 && mine > o2) score += 30;
                else if (mine == o1 && mine == o2) score += 10;
                else if ((mine == o1 && mine > o2) || (mine == o2 && mine > o1)) score += 15;
            }
            rs[i].QuadScore = score + 10;
        }

        double avgLevel = (rs[0].LevelTotal + rs[1].LevelTotal + rs[2].LevelTotal) / 3.0;
        double avgPower = (rs[0].BattleValue + rs[1].BattleValue + rs[2].BattleValue) / 3.0;
        double sumExtra = 0;
        for (int i = 0; i < 3; i++)
        {
            SideResult r = rs[i];
            // 等差: 等级差加乘; 「星之开拓者」时该项归零(表内 B101)
            r.LevelTerm = r.S.StarPioneer ? 0 : r.LevelTotal - avgLevel;
            r.PowerTerm = r.BattleValue - avgPower;
            double e = r.LevelTerm + r.QuadScore + r.PowerTerm;          // 胜率
            double f = r.PreRate + r.InitRate + r.MainRate;              // 战前+初始+主要
            double extras = r.S.Extra1 + r.S.Extra2;
            r.Chain = e + f + extras;
            sumExtra += f + extras;
        }
        // 相对胜率 = 本方合计 - (三方 战前+初始+主要+补充 之和)/3 (表内 I101)
        for (int i = 0; i < 3; i++) rs[i].Relative = rs[i].Chain - sumExtra / 3.0;

        // 保有胜率(150 分制, 表内 K101 分支 + 比例分配 E106/F106)
        var keep = new double[3];
        for (int i = 0; i < 3; i++)
        {
            SideResult me = rs[i];
            SideResult x = rs[(i + 1) % 3];
            SideResult y = rs[(i + 2) % 3];
            bool xLow = x.Relative < x.S.KeepBase;
            bool yLow = y.Relative < y.S.KeepBase;
            if (xLow && yLow) keep[i] = ThreeWayPool - x.S.KeepBase - y.S.KeepBase;
            else if (me.Relative < me.S.KeepBase) keep[i] = me.S.KeepBase;
            else if (xLow) keep[i] = Proportional(me.Relative, y.Relative, y.S.KeepBase);
            else if (yLow) keep[i] = Proportional(me.Relative, x.Relative, x.S.KeepBase);
            else keep[i] = me.Relative;
        }
        for (int i = 0; i < 3; i++)
        {
            rs[i].KeepRate = keep[i];
            rs[i].Excluded = keep[i] < 0;
        }
        // 有负胜率时: 排除负者, 剩余方按相对胜率比例分配 150 分(表内 104:108 备用表)
        if (rs[0].Excluded || rs[1].Excluded || rs[2].Excluded)
        {
            var alive = new List<SideResult>();
            foreach (SideResult r in rs) if (!r.Excluded) alive.Add(r);
            if (alive.Count == 1)
            {
                alive[0].KeepRate = ThreeWayPool;
            }
            else if (alive.Count > 1)
            {
                double sum = 0;
                foreach (SideResult r in alive) sum += Math.Max(0, r.Relative);
                if (sum <= 0) sum = alive.Count;
                foreach (SideResult r in alive)
                {
                    double w = Math.Max(0, r.Relative) / sum;
                    r.KeepRate = Math.Round(w * ThreeWayPool, 1);
                }
            }
        }
        foreach (SideResult r in rs)
        {
            if (r.KeepRate < 0) r.KeepRate = 0;
            r.Excluded = r.Excluded && r.KeepRate <= 0;
        }
        // 150 分制保有胜率 → 百分比(最大余数法保证三方合计恰为 100%)
        double sumKeep = 0;
        foreach (SideResult r in rs) sumKeep += r.KeepRate;
        if (sumKeep <= 0)
        {
            foreach (SideResult r in rs) r.FinalRate = 0;
        }
        else
        {
            var exact = new double[3];
            var baseRate = new int[3];
            int assigned = 0;
            for (int i = 0; i < 3; i++)
            {
                exact[i] = rs[i].KeepRate / sumKeep * 100.0;
                baseRate[i] = (int)Math.Floor(exact[i]);
                assigned += baseRate[i];
            }
            for (int i = 0; i < 3; i++) rs[i].FinalRate = baseRate[i];
            for (int rest = 100 - assigned, k = 0; rest > 0 && k < 3; k++)
            {
                // 余数最大者优先补 1%(并列时按 组A→C 顺序)
                int best = -1;
                double bestFrac = -1;
                for (int i = 0; i < 3; i++)
                {
                    if (rs[i].KeepRate <= 0) continue;
                    double frac = exact[i] - baseRate[i];
                    if (frac > bestFrac + 1e-9) { bestFrac = frac; best = i; }
                }
                if (best < 0) break;
                rs[best].FinalRate++;
                exact[best] = baseRate[best];   // 该方本轮已补齐
                rest--;
            }
        }
        foreach (SideResult r in rs) r.HalfRate = r.FinalRate;
        return rs;
    }

    private static double Proportional(double self, double other, int otherBase)
    {
        double denom = self + other;
        if (Math.Abs(denom) < 1e-9) return self;
        double share = self / denom;
        return share * ThreeWayPool - share * otherBase;
    }

    // ==================== 展示: 工序表格 ====================

    /// <summary>属性名(越界返回 "?")。</summary>
    public static string AttrName(int a) => a >= 0 && a < AttrCount ? AttrNames[a] : "?";

    private static string N(double v) => Math.Abs(v - Math.Round(v)) < 1e-9
        ? ((int)Math.Round(v)).ToString()
        : v.ToString("0.#");

    /// <summary>构造双方结算的工序表格行(与 Excel 各表段对应)。</summary>
    public static List<Row> Report2(SideResult a, SideResult b, Picks cfg)
    {
        var rows = new List<Row>();

        rows.Add(new Row("①初始工序", "项目", true, "阶段", "左方", "右方"));
        rows.Add(new Row("①初始工序", "参战人选", false, "组别", SideRoster(a.S), SideRoster(b.S)));
        rows.Add(new Row("①初始工序", "当前魔力", false, "魔力", N(MpCurSum(a.S)), N(MpCurSum(b.S))));
        rows.Add(new Row("①初始工序", "礼装/令咒补充", false, "补充", $"{a.S.MpGift}/{a.S.MpCommand}", $"{b.S.MpGift}/{b.S.MpCommand}"));
        rows.Add(new Row("①初始工序", "最终魔力", false, "最终魔力", FinalMpText(a.S), FinalMpText(b.S)));
        rows.Add(new Row("①初始工序", "魔力不足扣减", false, "每-20→-10", DeficitText(a.S), DeficitText(b.S)));
        for (int i = 0; i < AttrCount; i++)
            rows.Add(new Row("①初始工序", "属性(保底后)", false, AttrName(i), AttrCells(a, i), AttrCells(b, i)));

        rows.Add(new Row("②主要工序", "属性总值", true, "属性", "左方", "右方"));
        for (int i = 0; i < AttrCount; i++)
            rows.Add(new Row("②主要工序", "总值", false, AttrName(i), N(a.Total[i]), N(b.Total[i])));
        rows.Add(new Row("②主要工序", "等级总值", false, "仅主力", N(a.LevelTotal), N(b.LevelTotal)));
        rows.Add(new Row("②主要工序", "战前胜率", false, "补-惩", N(a.PreRate), N(b.PreRate)));
        rows.Add(new Row("②主要工序", "初始/主要胜率", false, "补-惩", $"{a.InitRate}/{a.MainRate}", $"{b.InitRate}/{b.MainRate}"));
        rows.Add(new Row("②主要工序", "战斗属性", true, "属性", "左方", "右方"));
        for (int p = 0; p < cfg.Count && p < 4; p++)
        {
            rows.Add(new Row("②主要工序", AttrName(cfg.Attr[p]),
                false,
                $"手动{a.S.Members.Count switch { 0 => 0, _ => cfg.Bias[0][p] }}×10+补正",
                N(a.PickValue[p]), N(b.PickValue[p])));
        }
        rows.Add(new Row("②主要工序", "战斗属性总值", false, "合计", N(a.BattleValue), N(b.BattleValue)));
        rows.Add(new Row("②主要工序", "自动优劣", false, "3优2平1劣", VerdictText(a, cfg.Count), VerdictText(b, cfg.Count)));

        rows.Add(new Row("③最终工序", "胜率结算", true, "项目", "左方", "右方"));
        rows.Add(new Row("③最终工序", "主力等级", false, "等级", N(a.LevelTotal), N(b.LevelTotal)));
        rows.Add(new Row("③最终工序", "基础胜率(优劣组合)", false, "组合表", N(a.BaseRate), N(b.BaseRate)));
        rows.Add(new Row("③最终工序", "战斗属性总值", false, "战力", N(a.BattleValue), N(b.BattleValue)));
        rows.Add(new Row("③最终工序", "胜率补正项", false, "战前+初始+主要",
            N(a.PreRate + a.InitRate + a.MainRate), N(b.PreRate + b.InitRate + b.MainRate)));
        rows.Add(new Row("③最终工序", "补充胜率", false, "两栏", N(a.S.Extra1 + a.S.Extra2), N(b.S.Extra1 + b.S.Extra2)));
        rows.Add(new Row("③最终工序", "双方胜率合计", false, "合计", N(a.Chain), N(b.Chain)));
        rows.Add(new Row("③最终工序", "胜率差值", false, "左-右", N(a.Diff), N(b.Diff)));
        rows.Add(new Row("③最终工序", "最终胜率", false, "clamp(50+差值/2)", $"{a.FinalRate}%", $"{b.FinalRate}%"));
        rows.Add(new Row("③最终工序", "差值减半胜率", false, "clamp(50+差值/4)", $"{a.HalfRate}%", $"{b.HalfRate}%"));
        return rows;
    }

    /// <summary>构造三方混战结算的工序表格行。</summary>
    public static List<Row> Report3(List<SideResult> rs, Picks cfg)
    {
        var rows = new List<Row>();
        string[] names = { rs[0].S.Name, rs[1].S.Name, rs[2].S.Name };
        rows.Add(new Row("①初始工序", "项目", true, "阶段", names[0], names[1], names[2]));
        rows.Add(new Row("①初始工序", "参战人选", false, "组别",
            SideRoster(rs[0].S), SideRoster(rs[1].S), SideRoster(rs[2].S)));
        rows.Add(new Row("①初始工序", "最终魔力", false, "含补充/消耗",
            FinalMpText(rs[0].S), FinalMpText(rs[1].S), FinalMpText(rs[2].S)));
        rows.Add(new Row("①初始工序", "魔力不足扣减", false, "每-20→-10",
            DeficitText(rs[0].S), DeficitText(rs[1].S), DeficitText(rs[2].S)));
        for (int i = 0; i < AttrCount; i++)
            rows.Add(new Row("①初始工序", "属性(保底后)", false, AttrName(i),
                AttrCells(rs[0], i), AttrCells(rs[1], i), AttrCells(rs[2], i)));

        rows.Add(new Row("②主要工序", "属性总值", true, "属性", names[0], names[1], names[2]));
        for (int i = 0; i < AttrCount; i++)
            rows.Add(new Row("②主要工序", "总值", false, AttrName(i), N(rs[0].Total[i]), N(rs[1].Total[i]), N(rs[2].Total[i])));
        rows.Add(new Row("②主要工序", "等级总值", false, "仅主力", N(rs[0].LevelTotal), N(rs[1].LevelTotal), N(rs[2].LevelTotal)));
        rows.Add(new Row("②主要工序", "战斗属性", true, "属性", names[0], names[1], names[2]));
        for (int p = 0; p < cfg.Count && p < 4; p++)
            rows.Add(new Row("②主要工序", AttrName(cfg.Attr[p]), false, "战力",
                N(rs[0].PickValue[p]), N(rs[1].PickValue[p]), N(rs[2].PickValue[p])));
        rows.Add(new Row("②主要工序", "战斗属性总值", false, "合计",
            N(rs[0].BattleValue), N(rs[1].BattleValue), N(rs[2].BattleValue)));

        rows.Add(new Row("③最终工序", "胜率结算", true, "项目", names[0], names[1], names[2]));
        rows.Add(new Row("③最终工序", "等差(等级差加乘)", false, "可被星之开拓者归零",
            N(rs[0].LevelTerm), N(rs[1].LevelTerm), N(rs[2].LevelTerm)));
        rows.Add(new Row("③最终工序", "四维优劣", false, "30/15/10 +10",
            N(rs[0].QuadScore), N(rs[1].QuadScore), N(rs[2].QuadScore)));
        rows.Add(new Row("③最终工序", "战力差加乘", false, "对三方均值",
            N(rs[0].PowerTerm), N(rs[1].PowerTerm), N(rs[2].PowerTerm)));
        rows.Add(new Row("③最终工序", "相对胜率", false, "扣三方均值",
            N(rs[0].Relative), N(rs[1].Relative), N(rs[2].Relative)));
        rows.Add(new Row("③最终工序", "保有胜率", false, "150 分制",
            $"{N(rs[0].KeepRate)}{(rs[0].Excluded ? "(排除)" : "")}",
            $"{N(rs[1].KeepRate)}{(rs[1].Excluded ? "(排除)" : "")}",
            $"{N(rs[2].KeepRate)}{(rs[2].Excluded ? "(排除)" : "")}"));
        rows.Add(new Row("③最终工序", "最终胜率", false, "归一化",
            $"{rs[0].FinalRate}%", $"{rs[1].FinalRate}%", $"{rs[2].FinalRate}%"));
        return rows;
    }

    private static int MpCurSum(Side s)
    {
        int sum = 0;
        foreach (Member m in s.Members) sum += m.Unit != null ? m.Unit.MpCur : 0;
        return sum;
    }

    private static string FinalMpText(Side s)
    {
        var parts = new List<string>();
        foreach (Member m in s.Members)
            parts.Add($"{SlotTag(m.Slot)}{m.FinalMp}");
        return string.Join(" ", parts);
    }

    private static string DeficitText(Side s)
    {
        var parts = new List<string>();
        foreach (Member m in s.Members)
            if (m.Deficit != 0) parts.Add($"{SlotTag(m.Slot)}{m.Deficit}");
        return parts.Count > 0 ? string.Join(" ", parts) : "无";
    }

    private static string SideRoster(Side s)
    {
        if (s == null || s.Members.Count == 0) return "(空)";
        var parts = new List<string>();
        foreach (Member m in s.Members)
            parts.Add($"{SlotTag(m.Slot)}{(m.Unit != null ? m.Unit.Name : "无")}");
        return string.Join(" ", parts);
    }

    private static string AttrCells(SideResult r, int attr)
    {
        var parts = new List<string>();
        foreach (Member m in r.S.Members)
            parts.Add($"{SlotTag(m.Slot)}{m.Attr[attr]}");
        return string.Join(" ", parts);
    }

    private static string VerdictText(SideResult r, int count)
    {
        var parts = new List<string>();
        for (int p = 0; p < count && p < 4; p++)
            parts.Add(r.Verdict[p] switch { 3 => "优", 2 => "平", _ => "劣" });
        return string.Join("", parts);
    }

    private static string SlotTag(int slot) => slot == 0 ? "主" : $"{slot}";
}
