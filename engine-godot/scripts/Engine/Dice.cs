using System;

namespace KsgGodot.Engine;

/// <summary>随机。默认系统随机;可用 Seed(int) 注入固定种子以做确定性回归(任务书阶段E)。</summary>
public static class Dice
{
    private static Random Rng = new();
    public static bool Seeded { get; private set; }
    public static int SeedValue { get; private set; }

    /// <summary>固定种子(确定性测试/回归)。0 表示恢复为系统随机。</summary>
    public static void Seed(int seed)
    {
        if (seed == 0)
        {
            Rng = new Random();
            Seeded = false;
            return;
        }
        Rng = new Random(seed);
        Seeded = true;
        SeedValue = seed;
    }

    public static int Roll() => Rng.Next(1, 101);              // 1..100
    public static bool RollPct(int percent)
    {
        if (percent <= 0) return false;
        if (percent >= 100) return true;
        return Rng.Next(1, 101) <= percent;
    }

    /// <summary>掷 0..maxExclusive-1(与 RollPct 共用同一种子序列;用于随机属性/随机目标等)。</summary>
    public static int Next(int maxExclusive)
        => maxExclusive <= 0 ? 0 : Rng.Next(maxExclusive);
}

/// <summary>判定(基础/最终成功率三层修正)</summary>
public class Check
{
    public int Base;        // 基础成功率
    public int BaseMod;     // 基础补正
    public int BasePen;     // 基础惩罚
    public int FinalMod;    // 最终补正
    public int FinalPen;    // 最终惩罚
    public bool Negative;   // 负面判定(受抗性)
    public int StatusKind;  // 状态判定(0=非)

    public int Final()
    {
        int b = Math.Clamp(Base + BaseMod - BasePen, 0, 100);
        return Math.Clamp(b + FinalMod - FinalPen, 0, 100);
    }
}

/// <summary>结算日志(仿 C 版文本)</summary>
public static class Log
{
    public static event Action<string> OnLog;

    public static void Write(string msg)
    {
        OnLog?.Invoke(msg);
    }
}