using System;
using System.Collections.Generic;
using KsgGodot.Model;

namespace KsgGodot.Engine;

/// <summary>世界状态:回合/昼夜/灵脉/契约(战斗外)</summary>
public class KsgWorld
{
    public int Round = 1;
    public bool Day = true;          // true=昼
    public List<UnitDef> Units = new();
    public int NextUnitId = 1;
    public List<LeylineDef> Leylines = new();
    public GrailDef Grail = new();

    public event Action<string> LogEvent;

    public void Log(string msg) => LogEvent?.Invoke(msg);

    public int RegisterUnit(UnitDef u)
    {
        if (u == null) throw new ArgumentNullException(nameof(u));
        if (Units.Contains(u)) return u.Id;

        if (u.Id <= 0)
        {
            while (Units.Exists(existing => existing.Id == NextUnitId))
            {
                if (NextUnitId == int.MaxValue) throw new InvalidOperationException("运行时单位 ID 已耗尽");
                NextUnitId++;
            }
            u.Id = NextUnitId++;
        }
        else if (Units.Exists(existing => existing.Id == u.Id))
        {
            throw new InvalidOperationException($"单位 ID 重复: {u.Id}");
        }

        Units.Add(u);
        if (u.Id >= NextUnitId && u.Id < int.MaxValue) NextUnitId = u.Id + 1;
        return u.Id;
    }

    /// <summary>轮次结束:魔耗/状态/回转/回路供魔(简化, 战斗外)</summary>
    public void EndRound(List<string> outLog)
    {
        foreach (var u in Units)
        {
            if (!u.Alive) continue;
            // 等级魔耗 + 常驻魔耗
            int lvl = u.Level / 2;
            int passive = 0;
            foreach (var s in u.Skills) if (s.IsPassive) passive += s.Cost;
            foreach (var p in u.Phantasms) if (p.IsPassive) passive += p.Cost;
            int before = u.MpCur;
            u.MpCur = Math.Max(u.MpCur - (lvl + passive), u.MpFloor);
            u.SettleMp();
            // 回转
            foreach (var s in u.Skills) s.TickRoundRecast();
            foreach (var p in u.Phantasms) p.TickRoundRecast();
            outLog.Add($"[{u.Name}] 魔力 {before} → {u.MpCur}(等级/常驻魔耗结算)");
            if (u.IsMaster) u.MpCur = Math.Min(u.MpCur + u.AttrValue(6), u.MpCap);
        }
        // 圣杯供魔(规则: 10 + 规模*10 / 轮): 给圣杯持有者
        if (Grail.OwnerUnitId > 0)
        {
            var gOwner = Units.Find(x => x.Id == Grail.OwnerUnitId);
            if (gOwner != null && gOwner.Alive)
            {
                gOwner.MpCur = Math.Min(gOwner.MpCur + Grail.SupplyPerRound, gOwner.MpCap);
                outLog.Add($"[圣杯供魔] {gOwner.Name} +{Grail.SupplyPerRound} 魔力");
            }
        }
        Round++;
        Day = !Day;
    }

    public int FindResById(List<ResourceDef> list, int id)
    {
        foreach (var r in list) if (r.Id == id) return id;
        return 0;
    }
}
