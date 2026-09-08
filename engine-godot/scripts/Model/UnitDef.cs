using System;
using System.Collections.Generic;

namespace KsgGodot.Model;

/// <summary>单位(从者/御主/召唤物) — 建卡产物</summary>
public class UnitDef
{
    public int Id;
    public string Name = "";
    public string TrueName = "";
    /// <summary>头像图片资源路径(user:// 相对路径; 空=无)。</summary>
    public string AvatarPath = "";
    public int UType;              // 1从者 2御主 3召唤物 4人偶 5使魔
    public int ServantClass;       // 1..9 职阶
    public int HiddenAttr;         // 0天 1地 2人 3星 4兽
    public int Level = 60;
    public int Faction;
    public int Law, Moral;         // 人物阵营
    public int Traits;             // 特性位
    public int Fp = 1;             // 脱离值
    public int Cs = 3;             // 令咒
    public int Roaming;
    public int CurrentLeyline;     // 当前灵脉Id(世界层)
    public int MasterUnitId;       // 契约御主Id(从者的主人; 0=无)
    public bool Retreated;         // 已退场(世界层)
    public string MainJob = "";    // 御主主职业(魔术师/体术师)
    public string SubJob = "";     // 御主子职业(可空)

    // 六属性 + 回路
    public int[] Attr = new int[7];
    public int[] AttrMod = new int[7];   // 临时补正
    public int[] AttrPerm = new int[7];  // 常驻补正
    public int HitMod;                   // 判定基础补正
    public int HitFinalMod;              // 判定最终补正
    public int FloorWin;                 // 底限胜率
    public int FinalWin;                 // 最终胜率
    public int TpFp;

    public int MpCap = 150, MpFloor = -100, MpCur = 50;
    public int ItemUsesThisRound;
    public int Acted;
    public bool Alive = true;

    public List<ResourceDef> Skills = new();
    public List<ResourceDef> Phantasms = new();
    public List<ResourceDef> Items = new();
    public List<StatusEntry> Status = new();
    public List<KeyValuePair<StatusKind, int>> StatusDict;

    // 战斗位
    public int BattleSide;          // 1左 2右 0无
    public int BattleSlot;          // 1主力 2辅助 3仆役 4支援 0无
    public bool InBattle;

    public bool IsServant => UType == 1;
    public bool IsMaster => UType == 2;

    public int AttrValue(int a)
    {
        int v = Attr[a] + AttrMod[a] + AttrPerm[a];
        // 疲惫:除宝具外 -5*层
        int tired = GetStatus(StatusKind.Tired);
        if (tired > 0 && a != 5) v -= 5 * tired;
        // 冻结:筋/耐/敏 -5*层
        if (a == 0 || a == 1 || a == 2)
        {
            int fz = GetStatus(StatusKind.Freeze);
            if (fz > 0) v -= 5 * fz;
            if (GetStatus(StatusKind.Stone) > 0) v -= 60;
        }
        // 宝具/回路允许0, 其余下限5
        if (a == 5 || a == 6) return Math.Max(0, v);
        return Math.Max(5, v);
    }

    public int GetStatus(StatusKind k)
    {
        foreach (var s in Status)
            if (s.Kind == k) return s.Layers;
        return 0;
    }

    public void GainStatus(StatusKind k, int layers, int src)
    {
        // 状态免疫
        foreach (var s in Status)
            if (s.Kind == StatusKind.StateIm && s.Layers == (int)k)
                return;
        int add = layers > 0 ? layers : 1;
        if (add <= 0) add = 0;
        int cap = StatusUtil.StackLimit(k);
        foreach (var s in Status)
        {
            if (s.Kind == k)
            {
                s.Layers += add;
                if (cap > 0 && s.Layers > cap) s.Layers = cap;
                s.Source = src;
                return;
            }
        }
        Status.Add(new StatusEntry { Kind = k, Layers = add, Source = src });
    }

    public void LoseStatus(StatusKind k, int layers)
    {
        for (int i = 0; i < Status.Count; i++)
        {
            if (Status[i].Kind == k)
            {
                Status[i].Layers -= layers;
                if (Status[i].Layers <= 0) Status.RemoveAt(i);
                return;
            }
        }
    }

    public void ClearStatus(StatusKind k)
    {
        for (int i = Status.Count - 1; i >= 0; i--)
            if (Status[i].Kind == k) Status.RemoveAt(i);
    }

    public bool HasStatus(StatusKind k) => GetStatus(k) > 0;

    /// <summary>魔力结算(上限抹除/溢出不保留)</summary>
    public void SettleMp()
    {
        if (MpCur > MpCap) MpCur = MpCap;
        // 从者魔力不足:每-20 → 除宝具外全属性-10(简化保留)
    }

    public string AttrBrief()
    {
        return $"筋{AttrValue(0)} 耐{AttrValue(1)} 敏{AttrValue(2)} 魔{AttrValue(3)} 幸{AttrValue(4)} 宝{AttrValue(5)}";
    }

    public string StatusBrief()
    {
        var parts = new List<string>();
        foreach (var s in Status)
            parts.Add($"{StatusUtil.Name(s.Kind)}{s.Layers}");
        return parts.Count > 0 ? string.Join(" ", parts) : "(无)";
    }
}


/// <summary>灵脉(规则书 1.4): 魔力量/人流量/灵脉主</summary>
public class LeylineDef
{
    public int Id;
    public string Name = "";
    public int Mana;         // 魔力量
    public int Flow;         // 人流量(下限0)
    public int OwnerUnitId;  // 灵脉主(0=无主)
    public bool HasWorkshop;
    public bool HasShrine;

    public int LeylineClass
    {
        get
        {
            if (Mana <= 0) return 0;          // 空
            if (Mana < 20) return 1;          // 小(5~15)
            if (Mana < 40) return 2;          // 中(20~35)
            return 3;                         // 大(40+)
        }
    }

    public static string ClassName(int c) => c switch
    {
        0 => "空", 1 => "小", 2 => "中", _ => "大",
    };

    public void FlowDown(int n)
    {
        Flow = Math.Max(0, Flow - n);
    }
}

/// <summary>圣杯(简化): 圣杯供魔 = 10 + 规模*10/轮</summary>
public class GrailDef
{
    public int Scale = 1;
    public int OwnerUnitId;
    public int SupplyPerRound => 10 + Scale * 10;
}
