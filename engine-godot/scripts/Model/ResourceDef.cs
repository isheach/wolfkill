using System;
using System.Collections.Generic;

namespace KsgGodot.Model;

/// <summary>资源类型</summary>
public enum ResKind { Skill, Np, Item }

/// <summary>技能类型(面向)</summary>
public enum SkillType
{
    Class = 1, Talent, Technique, Bless, Crown, Weapon, Magic
}

/// <summary>宝具类型</summary>
public enum NpType
{
    Human = 1, Army, Castle, World, Bound, NoType
}

/// <summary>宝具面向</summary>
public enum Focus
{
    None = 0, Decisive = 1, InstantKill, MagicSword, Defense, Offense,
    Buff, Summon, Status, Supply, Special, AntiTrait
}

/// <summary>等级</summary>
public enum Rank { Neg = 0, E = 1, D, C, B, A, Ex }

public static class RankUtil
{
    public static Rank Parse(string s)
    {
        switch ((s ?? "").Trim().ToUpper())
        {
            case "EX": return Rank.Ex;
            case "A": return Rank.A;
            case "B": return Rank.B;
            case "C": return Rank.C;
            case "D": return Rank.D;
            case "E": return Rank.E;
            default: return Rank.Neg;
        }
    }

    public static string Name(Rank r) => r switch
    {
        Rank.Ex => "EX", Rank.A => "A", Rank.B => "B", Rank.C => "C",
        Rank.D => "D", Rank.E => "E", _ => "-"
    };
}

/// <summary>发动时机</summary>
public enum When { Passive = 0, Act, BattleStart, Proc, Any }

/// <summary>特效(位掩码, 与 C 版 KS_F_* 对应)</summary>
[System.Flags]
public enum Feat
{
    None = 0,
    Main = 1 << 0,       // [主力位]
    Support = 1 << 1,    // [辅助位]
    Servant = 1 << 2,    // [仆役位]
    Rear = 1 << 3,       // [支援位]
    Assist = 1 << 4,     // [支援](可在主/辅/支位)
    Counter = 1 << 5,    // [反击]
    Ride = 1 << 6,       // [骑乘]
    ChargeReady = 1 << 7,// [蓄力]
    Pierce = 1 << 8,     // [必中]
    InvPierce = 1 << 9,  // [无敌贯通]
    Energy = 1 << 10,    // [爆发]
    Unique = 1 << 11,    // [唯一]
}

/// <summary>效果旗标(与 C 版 EF_* 对应)</summary>
public enum EffFlag
{
    None = 0,
    AttrUp, AttrDown, AttrUpConst, AttrDownConst,
    WinUp, WinDown, FinalWinUp, FinalWinDown,
    FloorUp, FloorPen, HitUp, HitFinalUp, HitPen,
    ResUp, ResDown, StateRes, StateIm, EffectIm,
    ManaUp, ManaDown, StatusGive, StatusRemove,
    BurnBlow, ElectricBlow, PoisonBlow,
    Recast, RecastLose, FpUp, FpDown, TpFp,
    Death, BoundDeath, Pierce, InvPierce, Evade, Protect, Invincible,
    Retaliate, Summon, Info, Cs, Other,

    // ---- 复杂机制模块(高频缺失机制) ----
    PledgeDecl,        // 宣言: 每工序双方暗宣言状态, 相同则本工序失效, 不同则给予目标状态(层数=Value)
    TickProc,          // 每工序结算: 目标按层数(Status)进行一次判定(概率Chance), 成功执行Value对应效果
    TickRound,         // 每回合结算: 同上, 轮次结束时结算
    RandomGive,        // 随机状态赋予: 从[状态池 Status] 随机一项给予目标, 层数=Layer
    OnKill,            // 击杀触发: 击杀目标时给予指定单位效果
    OnWin,             // 胜利触发: 本场战斗胜利时给予指定单位效果
    Charge,            // 蓄力: 解放时给予状态[蓄力], 每工序-1层, 0层时结算效果(值Value)
    GrantCs,           // 给予令咒
    GrantBadge,        // 给予加符
    Seal,              // 封印: 目标指定技能无效化(层数)
    Regen,             // 再生: 每工序恢复Value魔力(演示: 魔力+层数)
}

/// <summary>效果条件(与 C 版 KC_* 对应)</summary>
public enum Cond
{
    None = 0,
    OwnMain, OwnSupport, OwnRear,
    TargetTrait, TargetNotTrait, TargetStatusEq, TargetStatusGe, StatusNot,
    FriendStatus, TargetLuckGe, TargetLevelGe, LevelDiff,
    Day, Night, FirstEncounter, EnemyMainMaster,
    SelfHp, SelfMp, FriendInBattle, EnemyInBattle,
    SelfTrait, EnemyTactic, SelfTactic, TacticNotPaired,
    EnemyIsServant, EnemyIsSummon, SelfIsMaster, SelfIsServant,
    HasCs, MpUnder, TargetAgiLt, TargetAgiGe,
}

/// <summary>一条效果行</summary>
public class EffectLine
{
    public EffFlag Flag;
    public int Attr = -1;          // 属性索引(-1=无)
    public int Value;
    public int Status;             // 状态枚举值(-1=无)
    public int Layers;
    public int Rank;               // 等级阈值(0=无)
    public int Target;             // 0=敌全体 -1=己全体 -2=自身 正=敌第N位
    public int Times = 1;
    public Cond Cond;
    public int CondArg;
    public int CondArg2;
    public int Chance;             // 判定成功率(0=必定)
    public int ChanceAttrBase = -1;// 属性判定(-1=无)
    public bool ChanceNeg;         // 负面判定(受抗性)
    public bool LuckHalve;         // 目标幸运>=40 成功率减半
    public int Cap;                // 总值上限(<=0 不限)
    public string Desc = "";

    public EffectLine Clone() => (EffectLine)MemberwiseClone();
}

/// <summary>等级数值系数(A=1.0, B=0.8, C=0.6, D=0.4, E=0.2, EX/“-”=1.0)。</summary>
public static class RankScale
{
    public static double Factor(Rank r) => r switch
    {
        Rank.A or Rank.Ex or Rank.Neg => 1.0,
        Rank.B => 0.8,
        Rank.C => 0.6,
        Rank.D => 0.4,
        Rank.E => 0.2,
        _ => 1.0,
    };

    /// <summary>原等级→购入等级的相对缩放系数。</summary>
    public static double FactorFrom(Rank baseRank, Rank effective) =>
        effective <= Rank.Neg || effective >= baseRank ? 1.0
        : Factor(effective) / Factor(baseRank);

    public static int ScaleValue(int value, double factor) =>
        factor >= 1.0 ? value : (int)System.Math.Round(value * factor);
}

/// <summary>一条资源(技能/宝具/礼装)</summary>
public class ResourceDef
{
    public int Id;
    public string Name = "";
    /// <summary>卡面自定义名(重命名);为空时显示 Name。仅作用于卡面展示与存档,不改变效果。</summary>
    public string DisplayName = "";
    /// <summary>卡面自定义注释(玩家备注);不改变效果。</summary>
    public string Note = "";
    public ResKind Kind;
    public int Type;               // SkillType / NpType
    public int Focus;              // 宝具面向
    public Rank Rank = Rank.E;
    /// <summary>实际购入等级(建卡时可低于原 Rank,默认=原最高等级)。RP 与效果数值据此计算。</summary>
    public Rank EffectiveRank = Rank.Neg;
    public When When;
    public int Cost;
    public int Recast;
    public int CurRecast;          // 当前回转
    public int Feat;
    public int CountPerRound;
    public int Reserve;
    public int ReserveMax;
    public int Unique;
    /// <summary>购入前置: 必须已持有该名称资源才能购入(空=无前置)。</summary>
    public string Prereq = "";
    /// <summary>购入互斥: 持有该名称资源时不能购入(空=无互斥)。</summary>
    public string ExclusiveWith = "";
    public List<EffectLine> Effects = new();
    public string Text = "";

    public bool HasFeat(Feat f) => (Feat & (int)f) != 0;
    public bool IsPassive => When == When.Passive;
    public bool ReadyToCast => Recast <= 0 || CurRecast >= Recast;
    public void ResetRecast() => CurRecast = 0;
    /// <summary>回合结束回转+3</summary>
    public void TickRoundRecast() { if (Recast > 0) { CurRecast += 3; if (CurRecast > Recast) CurRecast = Recast; } }
    /// <summary>工序结束回转+1</summary>
    public void TickProcRecast() { if (Recast > 0) { CurRecast += 1; if (CurRecast > Recast) CurRecast = Recast; } }

    /// <summary>为单位创建独立资源实例，避免回转与储备在不同卡牌之间共享。</summary>
    public ResourceDef DeepClone()
    {
        var copy = new ResourceDef
        {
            Id = Id,
            Name = Name,
            DisplayName = DisplayName,
            Note = Note,
            Kind = Kind,
            Type = Type,
            Focus = Focus,
            Rank = Rank,
            When = When,
            Cost = Cost,
            Recast = Recast,
            CurRecast = CurRecast,
            Feat = Feat,
            CountPerRound = CountPerRound,
            Reserve = Reserve,
            ReserveMax = ReserveMax,
            Unique = Unique,
            Prereq = Prereq,
            ExclusiveWith = ExclusiveWith,
            Text = Text,
        };
        foreach (EffectLine effect in Effects)
            copy.Effects.Add(effect.Clone());
        return copy;
    }

    /// <summary>实际生效等级: 未指定(=Neg)时用原 Rank。</summary>
    public Rank EffectiveRankNow => EffectiveRank <= Rank.Neg ? Rank : EffectiveRank;

    /// <summary>以指定等级生成副本: 效果数值/判定按等级系数缩放, Rank 与 EffectiveRank 记为购入等级。
    /// 低等级时数值降低但 RP 更省(见 CardBuildRules)。</summary>
    public ResourceDef DeepClone(Rank effective)
    {
        if (effective <= Rank.Neg || effective >= Rank)
            return DeepClone();   // 最高等级或无效 → 原样

        var copy = new ResourceDef
        {
            Id = Id,
            Name = Name,
            DisplayName = DisplayName,
            Note = Note,
            Kind = Kind,
            Type = Type,
            Focus = Focus,
            Rank = Rank,
            EffectiveRank = effective,
            When = When,
            Cost = Cost,
            Recast = Recast,
            CurRecast = CurRecast,
            Feat = Feat,
            CountPerRound = CountPerRound,
            Reserve = Reserve,
            ReserveMax = ReserveMax,
            Unique = Unique,
            Text = Text,
        };
        double factor = RankScale.FactorFrom(Rank, effective);
        foreach (EffectLine effect in Effects)
        {
            EffectLine ec = effect.Clone();
            ec.Value = RankScale.ScaleValue(effect.Value, factor);
            ec.Chance = RankScale.ScaleValue(effect.Chance, factor);
            ec.Layers = RankScale.ScaleValue(effect.Layers, factor);
            copy.Effects.Add(ec);
        }
        return copy;
    }

    /// <summary>
    /// 创建仅用于本次结算的副本并覆盖状态层数。0 层会跳过对应状态效果，原资源不被修改。
    /// </summary>
    public ResourceDef WithLayerOverrides(IReadOnlyDictionary<int, int> overrides)
    {
        var copy = DeepClone();
        if (overrides == null) return copy;

        foreach (KeyValuePair<int, int> pair in overrides)
        {
            if (pair.Key < 0 || pair.Key >= copy.Effects.Count)
                throw new ArgumentOutOfRangeException(nameof(overrides), $"效果索引越界: {pair.Key}");

            EffectLine effect = copy.Effects[pair.Key];
            StatusKind status = (StatusKind)effect.Status;
            if (effect.Flag != EffFlag.StatusGive || !StatusUtil.UsesLayerCount(status))
                throw new ArgumentException($"效果 {pair.Key} 不是可手动输入层数的状态效果", nameof(overrides));

            int layers = Math.Clamp(pair.Value, 0, StatusUtil.ManualInputMax(status));
            if (layers == 0) effect.Flag = EffFlag.None;
            else effect.Layers = layers;
        }
        return copy;
    }

    public string KindName => Kind switch
    {
        ResKind.Skill => "技能",
        ResKind.Np => "宝具",
        _ => "礼装",
    };

    /// <summary>卡面显示名:优先自定义名,否则原名称。</summary>
    public string CardName => string.IsNullOrWhiteSpace(DisplayName) ? Name : DisplayName;

    /// <summary>完整显示标题(含自定义名与原名的说明),用于已选列表。</summary>
    public string CardTitle => string.IsNullOrWhiteSpace(DisplayName)
        ? Name
        : $"{DisplayName}（{Name}）";
}
