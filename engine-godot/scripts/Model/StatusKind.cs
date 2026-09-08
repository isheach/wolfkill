namespace KsgGodot.Model;

/// <summary>状态(词典附录三)</summary>
public enum StatusKind
{
    None = 0,
    // 强化
    Evade, Invincible, Protect, ResUp, StateRes, StateIm, EffectIm,
    // 弱化
    Tired, Crippled, Lag, Curse, Seal, SkillSeal, NpSeal, ResDown,
    // 异常
    Poison, Burn, Freeze, Electric, Stone, Stun, Charm, Confuse, Fear,
    ResBreak, TraitGive,
    // 机制层数(复杂机制模块)
    Charge,        // 蓄力层数(每工序-1, 0时结算)
    Badge,         // 加符层数
}

public static class StatusUtil
{
    /// <summary>状态归类: 1强化 2弱化 3异常</summary>
    public static int Class(StatusKind k)
    {
        if (k >= StatusKind.Evade && k <= StatusKind.EffectIm) return 1;
        if (k >= StatusKind.Tired && k <= StatusKind.ResDown) return 2;
        return 3;
    }

    public static string Name(StatusKind k) => k switch
    {
        StatusKind.Evade => "回避",
        StatusKind.Invincible => "无敌",
        StatusKind.Protect => "保护",
        StatusKind.ResUp => "抗性上升",
        StatusKind.StateRes => "状态抵抗",
        StatusKind.StateIm => "状态免疫",
        StatusKind.EffectIm => "效果免疫",
        StatusKind.Tired => "疲惫",
        StatusKind.Crippled => "残废",
        StatusKind.Lag => "迟滞",
        StatusKind.Curse => "诅咒",
        StatusKind.Seal => "封印",
        StatusKind.SkillSeal => "技能封印",
        StatusKind.NpSeal => "宝具封印",
        StatusKind.ResDown => "抗性下降",
        StatusKind.Poison => "中毒",
        StatusKind.Burn => "灼伤",
        StatusKind.Freeze => "冻结",
        StatusKind.Electric => "感电",
        StatusKind.Stone => "石化",
        StatusKind.Stun => "晕眩",
        StatusKind.Charm => "魅惑",
        StatusKind.Confuse => "混乱",
        StatusKind.Fear => "恐惧",
        StatusKind.ResBreak => "抗性破除",
        StatusKind.TraitGive => "特性赋予",
        StatusKind.Charge => "蓄力",
        StatusKind.Badge => "加符",
        _ => "状态",
    };

    /// <summary>这些弱化/异常状态的 Layers 表示可由玩家输入的实际层数。</summary>
    public static bool UsesLayerCount(StatusKind k) =>
        k >= StatusKind.Tired && k <= StatusKind.ResBreak;

    /// <summary>C 版已明确的叠加上限；0 表示规则未规定硬上限。</summary>
    public static int StackLimit(StatusKind k) => k switch
    {
        StatusKind.Curse => 20,
        StatusKind.Electric => 20,
        StatusKind.Charm => 9,
        _ => 0,
    };

    public static int ManualInputMax(StatusKind k)
    {
        int limit = StackLimit(k);
        return limit > 0 ? limit : 99;
    }
}

/// <summary>单位持有的一条状态</summary>
public class StatusEntry
{
    public StatusKind Kind;
    public int Layers;
    public int Source;   // 来源单位id

    public StatusEntry Clone() => new() { Kind = Kind, Layers = Layers, Source = Source };
}
