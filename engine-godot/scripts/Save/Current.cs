using KsgGodot.Model;
using System.Collections.Generic;

namespace KsgGodot.Save;

/// <summary>会话内当前已建卡(供对战选择)</summary>
public static class CurrentSave
{
    public static UnitDef Current;
    public static readonly List<UnitDef> All = new();
    public static bool DiskScanned;

    /// <summary>按稳定存档 ID 加入或更新会话，避免重复载入同一张卡。</summary>
    public static void Upsert(UnitDef unit)
    {
        if (unit == null) return;
        for (int i = 0; i < All.Count; i++)
        {
            if (ReferenceEquals(All[i], unit) || (unit.Id > 0 && All[i].Id == unit.Id))
            {
                All[i] = unit;
                Current = unit;
                return;
            }
        }
        All.Add(unit);
        Current = unit;
    }

    public static bool ContainsId(int id)
    {
        if (id <= 0) return false;
        foreach (UnitDef unit in All)
            if (unit.Id == id) return true;
        return false;
    }
}
