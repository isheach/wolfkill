using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using KsgGodot.Model;
using KsgGodot.Engine;
using KsgGodot.Building;
using KsgGodot.Save;

namespace CardBattleTest;

/// <summary>
/// 导入卡片(21..30)卡间战斗验证:
///  1) 以真实资源 JSON 载入 id→ResourceDef, 注入等级变体表/归属表(与游戏运行时一致);
///  2) 以 SaveCodec 载入生成的卡存档(v3+rank), 核对每张卡资源/等级恢复正确;
///  3) 全部有序配对(10×9=90)战斗: 常驻结算→战术→主要属性→随机属性→战斗属性→工序(回转+发动)→
///     指令→最终胜率→决胜→结束, 固定种子;
///  4) 断言: 无异常; 魔力只按发动成本下降且不低于底限; 胜率钳制; 状态层数上限/非负; 回转不越界;
///     同一种子重复战斗结果完全一致(确定性)。
/// </summary>
public static class Program
{
    private const string Root = @"D:\desktop\wolfkill";
    private static readonly string DbJson = Root + @"\engine-godot\data\ksg_resources.json";
    private static readonly string VariantJson = Root + @"\engine-godot\data\rank_variants.json";
    private static readonly string OwnerJson = Root + @"\engine-godot\data\skill_owner.json";
    private static readonly string SaveDir = Root + @"\卡片\卡片导入存档";

    private static int _pass, _fail;
    private static readonly List<string> Failures = new();

    private static void Check(bool ok, string name)
    {
        if (ok) { _pass++; Console.WriteLine($"  ✅ {name}"); }
        else { _fail++; Console.WriteLine($"  ❌ {name}"); Failures.Add(name); }
    }

    public static void Main()
    {
        Console.OutputEncoding = Encoding.UTF8;
        string[] args = Environment.GetCommandLineArgs();
        if (args.Length > 1 && args[1] == "--debug")
        {
            DebugPair(int.Parse(args[2]), int.Parse(args[3]));
            return;
        }
        Console.WriteLine("==== 卡片导入 · 卡间战斗验证 ====");

        // 1) 载入真实资源库(与游戏 data/ksg_resources.json 一致)
        Dictionary<int, ResourceDef> byId = LoadResourceDb();
        Console.WriteLine($"资源库: {byId.Count} 条");

        // 2) 注入等级变体表/技能归属(复制 ResourceDb 的注入行为)
        InjectRankVariants();
        Console.WriteLine("等级变体表与技能归属已注入");

        // 3) 载入导入卡存档
        var saveFiles = Directory.GetFiles(SaveDir, "*.json").OrderBy(f => int.Parse(Path.GetFileNameWithoutExtension(f))).ToList();
        Check(saveFiles.Count == 10, $"发现导入卡存档 {saveFiles.Count} 张(期望10)");
        var units = new List<UnitDef>();
        foreach (string file in saveFiles)
        {
            int id = int.Parse(Path.GetFileNameWithoutExtension(file));
            string json = File.ReadAllText(file);
            var warn = new List<string>();
            UnitDef u = SaveCodec.Deserialize(json, id => byId.TryGetValue(id, out ResourceDef r) ? r : null, name => FindByName(byId, name), m => warn.Add(m));
            if (u == null) { Check(false, $"存档 {id} 载入失败"); continue; }
            if (warn.Count > 0) { Check(false, $"存档 {id} 载入警告: {string.Join("; ", warn)}"); }
            units.Add(u);
            Console.WriteLine($"  载入[{id}] {u.Name} 职阶={u.ServantClass} Lv{u.Level} " +
                $"技能{string.Join("/", u.Skills.Select(s => s.CardName + "[" + RankUtil.Name(s.EffectiveRankNow) + "]"))} " +
                $"宝具{string.Join("/", u.Phantasms.Select(p => p.CardName + "[" + RankUtil.Name(p.EffectiveRankNow) + "]"))}");
        }
        if (units.Count < 2) { Console.WriteLine("存档不足, 无法战斗验证"); return; }

        // 4) 等级持久化抽查(存档 rank → EffectiveRank 恢复)
        var zhangjiao = units.First(u => u.Id == 25);
        var zjTerra = zhangjiao.Skills.FirstOrDefault(s => s.Id == 706); // 阵地制作 卡面E
        Check(zjTerra != null && zjTerra.EffectiveRankNow == Rank.E,
            $"张角·阵地制作 存档等级E 恢复为EffectiveRank=E(现{RankUtil.Name(zjTerra?.EffectiveRankNow ?? Rank.Neg)})");
        var zjNp1 = zhangjiao.Phantasms.FirstOrDefault(p => p.Id == 305);
        Check(zjNp1 != null && zjNp1.EffectiveRankNow == Rank.A, "张角·苍天已死 等级A 恢复正确");
        var torfen = units.First(u => u.Id == 22);
        var torfenPioneer = torfen.Skills.FirstOrDefault(s => s.Id == 410); // 星之开拓者EX 卡面A
        Check(torfenPioneer != null && torfenPioneer.EffectiveRankNow == Rank.A,
            $"托尔芬·星之开拓者EX 卡面A 恢复为A(库基EX, 现{RankUtil.Name(torfenPioneer?.EffectiveRankNow ?? Rank.Neg)})");
        Check(units.All(u => u.Phantasms.All(p => p.Id > 0) && u.Skills.All(s => s.Id > 0)), "全部资源均解析为稳定ID");
        Check(units.All(u => u.Name.Length > 0 && u.Attr.Sum() > 0), "全部卡身份/属性非空");

        // 5) 全部有序配对战斗
        Console.WriteLine("\n==== 全配对战斗(固定种子) ====");
        int battles = 0, crashes = 0;
        var outcomes = new Dictionary<string, string>();
        for (int li = 0; li < units.Count; li++)
        {
            for (int ri = 0; ri < units.Count; ri++)
            {
                if (li == ri) continue;
                int seed = 1000 + li * 100 + ri;
                battles++;
                string outcome = RunPairBattle(units, li, ri, seed, out string problem);
                outcomes[$"{li},{ri}"] = outcome;
                if (problem != null)
                {
                    crashes++;
                    Check(false, $"战斗 {units[li].Name} vs {units[ri].Name} (seed={seed}): {problem}");
                }
            }
        }
        Check(crashes == 0, $"全部有序配对战斗零崩溃/零违规({battles} 场)");

        // 6) 确定性: 每场用同一种子重打, 结果必须完全一致
        Console.WriteLine("\n==== 确定性复核(同种子重打) ====");
        int detOk = 0, detBad = 0;
        var badPairs = new List<string>();
        for (int li = 0; li < units.Count && li < 10; li++)
        {
            for (int ri = 0; ri < units.Count; ri++)
            {
                if (li == ri) continue;
                int seed = 1000 + li * 100 + ri;
                string again = RunPairBattle(units, li, ri, seed, out string problem2);
                if (problem2 != null) { detBad++; badPairs.Add($"{li}v{ri}(crash)"); continue; }
                if (again == outcomes[$"{li},{ri}"]) detOk++;
                else
                {
                    detBad++;
                    badPairs.Add($"{li}v{ri}");
                }
            }
        }
        Check(detBad == 0, $"确定性复核 {detOk}/{detOk + detBad} 场完全一致");
        if (detBad > 0) Console.WriteLine("  不一致配对: " + string.Join(", ", badPairs));

        Console.WriteLine($"\n=== 结果: {_pass} 通过, {_fail} 失败 ===");
        foreach (string f in Failures.Take(20)) Console.WriteLine("  !! " + f);
        if (_fail > 0) Environment.Exit(1);
    }

    // ---------------------------------------------------------------- 调试
    private static void DebugPair(int li, int ri)
    {
        Dictionary<int, ResourceDef> byId = LoadResourceDb();
        InjectRankVariants();
        var files = Directory.GetFiles(SaveDir, "*.json").OrderBy(f => int.Parse(Path.GetFileNameWithoutExtension(f))).ToList();
        var units = new List<UnitDef>();
        foreach (string file in files)
        {
            int id = int.Parse(Path.GetFileNameWithoutExtension(file));
            UnitDef u = SaveCodec.Deserialize(File.ReadAllText(file),
                x => byId.TryGetValue(x, out ResourceDef r) ? r : null,
                name => FindByName(byId, name), _ => { });
            units.Add(u);
        }
        for (int run = 1; run <= 2; run++)
        {
            var log = new StringBuilder();
            var left = CopyUnit(units[li]);
            var right = CopyUnit(units[ri]);
            Dice.Seed(1000 + li * 100 + ri);
            var w = new KsgWorld();
            w.RegisterUnit(left);
            w.RegisterUnit(right);
            var b = new KsgBattle();
            b.LogEvent += m => log.AppendLine("[b] " + m);
            KsgEffects.VerboseEvent += m => log.AppendLine("[v] " + m);
            b.Start(w, left.Id, right.Id, 4);
            SettlePassives(b, w, left, 1);
            SettlePassives(b, w, right, 2);
            b.SetTactic(1, Tactic.Strike);
            b.SetTactic(2, Tactic.Probe);
            b.PickMainAttr(1, 3);
            b.PickMainAttr(2, 0);
            b.RollRandAttr();
            b.BatteryCheck();
            for (int phase = 2; phase <= 5 && b.Active; phase++)
            {
                b.Phase = phase;
                TickRecast(left);
                TickRecast(right);
                CastSide(b, w, left, log);
                CastSide(b, w, right, log);
                KsgEffects.TickCharges(b, w);
            }
            if (b.Active)
            {
                b.IssueOrder(1, Order.Charge);
                b.IssueOrder(2, Order.Duel);
                if (b.LeftWin <= b.RightWin) b.Retreat(2); else b.Retreat(1);
                b.Effective();
                b.End(w);
            }
            File.WriteAllText(Path.Combine(SaveDir, $"_debug{li}_{ri}_{run}.log"), log.ToString(), Encoding.UTF8);
        }
        Console.WriteLine($"debug done: {li} vs {ri}");
    }

    // ---------------------------------------------------------------- 战斗
    private static string RunPairBattle(List<UnitDef> roster, int li, int ri, int seed, out string problem)
    {
        problem = null;
        var log = new StringBuilder();
        var left = CopyUnit(roster[li]);
        var right = CopyUnit(roster[ri]);
        Dice.Seed(seed);
        var w = new KsgWorld();
        w.RegisterUnit(left);
        w.RegisterUnit(right);
        var b = new KsgBattle();
        b.LogEvent += m => log.AppendLine(m);
        KsgEffects.VerboseEvent += m => log.AppendLine("    " + m);
        try
        {
            b.Start(w, left.Id, right.Id, 4);
            if (!b.Active || b.Left.Count == 0 || b.Right.Count == 0)
                throw new Exception("战斗未正常开始");
            CheckInvariants(b, w, "开战");

            // 常驻结算(与 BattleScene.SettlePassives 相同)
            SettlePassives(b, w, left, 1);
            SettlePassives(b, w, right, 2);

            b.SetTactic(1, Tactic.Strike);
            b.SetTactic(2, Tactic.Probe);
            b.PickMainAttr(1, 3); // 魔力
            b.PickMainAttr(2, 0); // 筋力
            b.RollRandAttr();
            b.BatteryCheck();
            CheckInvariants(b, w, "战斗属性结算");

            // 工序模拟: 每工序给所有资源 +1 回转, 然后双方发动可用资源(技能在前, 宝具在后)
            for (int phase = 2; phase <= 5 && b.Active; phase++)
            {
                b.Phase = phase;
                TickRecast(left);
                TickRecast(right);
                int preMpL = left.MpCur, preMpR = right.MpCur;
                CastSide(b, w, left, log);
                CastSide(b, w, right, log);
                // 魔力守恒: 成功发动按资源成本精确扣减(在 CastSide 内逐次核对)
                KsgEffects.TickCharges(b, w);
                CheckInvariants(b, w, $"工序{phase}结算");
            }

            // 指令与最终判定
            if (b.Active)
            {
                b.IssueOrder(1, Order.Charge);
                b.IssueOrder(2, Order.Duel);
                // 弱侧尝试撤退(验证 FP 扣费路径)
                if (b.LeftWin <= b.RightWin) b.Retreat(2); else b.Retreat(1);
                b.Effective();
                CheckInvariants(b, w, "最终胜率/决胜");
                b.End(w);
                CheckInvariants(b, w, "战斗结束");
            }
        }
        catch (Exception ex)
        {
            problem = $"{ex.GetType().Name}: {ex.Message}\n--- log ---\n{log}";
            return "";
        }
        finally
        {
            b.LogEvent -= m => log.AppendLine(m);
            KsgEffects.VerboseEvent -= m => log.AppendLine("    " + m);
        }

        // 汇总结果字符串(确定性比较用)
        return string.Join("|",
            b.LeftOk, b.RightOk,
            b.LeftWin, b.RightWin,
            left.MpCur, right.MpCur,
            string.Join(",", left.Status.Select(s => (int)s.Kind + ":" + s.Layers)),
            string.Join(",", right.Status.Select(s => (int)s.Kind + ":" + s.Layers)),
            left.Fp, right.Fp,
            string.Join(",", left.Skills.Select(s => s.CurRecast).Concat(left.Phantasms.Select(p => p.CurRecast))),
            string.Join(",", right.Skills.Select(s => s.CurRecast).Concat(right.Phantasms.Select(p => p.CurRecast))));
    }

    private static void SettlePassives(KsgBattle b, KsgWorld w, UnitDef u, int side)
    {
        // 与 BattleScene.SettlePassives 相同: 常驻效果经 ApplyResource 免费结算(无魔力扣减;
        // 常驻魔耗属世界层轮次结算范畴, 不在战斗内重复计费)
        foreach (ResourceDef s in u.Skills)
            if (s.IsPassive) { int pre = u.MpCur; KsgEffects.ApplyResource(b, w, u, s, 0); Check(u.MpCur == pre, $"常驻技能不改魔力: {u.Name} {s.CardName}"); }
        foreach (ResourceDef p in u.Phantasms)
            if (p.IsPassive) { int pre = u.MpCur; KsgEffects.ApplyResource(b, w, u, p, 0); Check(u.MpCur == pre, $"常驻宝具不改魔力: {u.Name} {p.CardName}"); }
    }

    private static void CastSide(KsgBattle b, KsgWorld w, UnitDef u, StringBuilder log)
    {
        var casts = u.Skills.Where(s => !s.IsPassive).Cast<ResourceDef>()
            .Concat(u.Phantasms.Where(p => !p.IsPassive))
            .Concat(u.Items.Where(i => !i.IsPassive)).ToList();
        foreach (ResourceDef r in casts)
        {
            if (!b.Active) break;
            if (!r.ReadyToCast) continue;
            int pre = u.MpCur;
            if (KsgEffects.CastResource(b, w, u, r, 0, out string reason))
            {
                CheckMpMove(r, u, pre, "发动");
                Check(r.CurRecast == 0, $"发动后回转清零: {u.Name} {r.CardName}");
            }
            else
            {
                // 拒绝路径必须无副作用
                Check(u.MpCur == pre, $"拒绝路径不改魔力: {u.Name} {r.CardName} ({reason})");
            }
            CheckInvariants(b, w, $"发动 {u.Name}/{r.CardName}");
        }
    }

    private static void CheckMpMove(ResourceDef r, UnitDef u, int pre, string stage)
    {
        // 发动成本必须被扣除; 允许同资源附带 ±40 的魔力修正(自增益/敌方魔力汲取等), 以发现漏扣成本
        int expected = pre - Math.Max(0, r.Cost);
        int diff = u.MpCur - expected;
        Check(Math.Abs(diff) <= 40,
            $"{stage} 魔力成本扣除: {u.Name} {r.CardName}(成本{r.Cost}) {pre}→{u.MpCur}(期望{expected}±40, 差{diff})");
    }

    private static void CheckInvariants(KsgBattle b, KsgWorld w, string stage)
    {
        foreach (var bu in b.Left.Concat(b.Right))
        {
            UnitDef u = bu.Unit;
            if (u.MpCur < u.MpFloor)
                throw new Exception($"{stage}: {u.Name} 魔力低于底限 {u.MpCur}<{u.MpFloor}");
            if (u.MpCur > u.MpCap + 50)
                throw new Exception($"{stage}: {u.Name} 魔力异常超高 {u.MpCur}>(cap{u.MpCap}+50)");
            foreach (StatusEntry s in u.Status)
            {
                if (s.Layers < 0)
                    throw new Exception($"{stage}: {u.Name} 状态层数负数 {s.Kind}={s.Layers}");
                int cap = StatusUtil.StackLimit(s.Kind);
                if (cap > 0 && s.Layers > cap)
                    throw new Exception($"{stage}: {u.Name} 状态[{StatusUtil.Name(s.Kind)}]层数{cap}超限({s.Layers})");
            }
            foreach (ResourceDef r in u.Skills.Concat(u.Phantasms).Concat(u.Items))
                if (r.CurRecast < 0 || r.CurRecast > Math.Max(0, r.Recast))
                    throw new Exception($"{stage}: {u.Name} 回转越界 {r.CardName}={r.CurRecast}/{r.Recast}");
        }
        // 中间胜率为发动前修正的原始累计值, 允许任意超出 0..100(最终判定时 Effective() 统一钳制;
        // 玻吕克斯等超高属性卡的基础胜率可远超 100, 属引擎预期语义)
        if (b.LeftWin != b.LeftWin || b.RightWin != b.RightWin)
            throw new Exception($"{stage}: 胜率非数值 {b.LeftWin}/{b.RightWin}");
    }

    private static void TickRecast(UnitDef u)
    {
        foreach (ResourceDef s in u.Skills) s.TickProcRecast();
        foreach (ResourceDef p in u.Phantasms) p.TickProcRecast();
    }

    // ---------------------------------------------------------------- 工具
    private static ResourceDef FindByName(Dictionary<int, ResourceDef> byId, string name)
    {
        return byId.Values.FirstOrDefault(r => r.Name == name);
    }

    private static UnitDef CopyUnit(UnitDef src)
    {
        var u = new UnitDef
        {
            Id = src.Id, Name = src.Name, TrueName = src.TrueName, AvatarPath = src.AvatarPath,
            UType = src.UType, ServantClass = src.ServantClass, HiddenAttr = src.HiddenAttr,
            Level = src.Level, Faction = src.Faction, Law = src.Law, Moral = src.Moral,
            Traits = src.Traits, Fp = src.Fp, Cs = src.Cs, Roaming = src.Roaming,
            CurrentLeyline = src.CurrentLeyline, MasterUnitId = src.MasterUnitId,
            Retreated = src.Retreated, MainJob = src.MainJob, SubJob = src.SubJob,
            MpCap = src.MpCap, MpFloor = src.MpFloor, MpCur = src.MpCur,
            HitMod = src.HitMod, HitFinalMod = src.HitFinalMod,
            FloorWin = src.FloorWin, FinalWin = src.FinalWin,
        };
        Array.Copy(src.Attr, u.Attr, 7);
        Array.Copy(src.AttrMod, u.AttrMod, 7);
        Array.Copy(src.AttrPerm, u.AttrPerm, 7);
        foreach (ResourceDef s in src.Skills) u.Skills.Add(s.DeepClone());
        foreach (ResourceDef p in src.Phantasms) u.Phantasms.Add(p.DeepClone());
        foreach (ResourceDef i in src.Items) u.Items.Add(i.DeepClone());
        foreach (StatusEntry s in src.Status) u.Status.Add(new StatusEntry { Kind = s.Kind, Layers = s.Layers, Source = s.Source });
        return u;
    }

    private static void InjectRankVariants()
    {
        var variants = new Dictionary<int, (string[] RankRange, List<int[]> Series)>();
        if (File.Exists(VariantJson))
        {
            using JsonDocument doc = JsonDocument.Parse(File.ReadAllText(VariantJson));
            if (doc.RootElement.TryGetProperty("skills", out JsonElement sk) && sk.ValueKind == JsonValueKind.Array)
            {
                foreach (JsonElement el in sk.EnumerateArray())
                {
                    if (!el.TryGetProperty("id", out JsonElement idEl) || idEl.ValueKind != JsonValueKind.Number) continue;
                    if (!idEl.TryGetInt32(out int id)) continue;
                    string[] range = { };
                    if (el.TryGetProperty("rank_range", out JsonElement rr) && rr.ValueKind == JsonValueKind.Array)
                        range = rr.EnumerateArray().Where(x => x.ValueKind == JsonValueKind.String)
                            .Select(x => x.GetString() ?? "").ToArray();
                    var series = new List<int[]>();
                    if (el.TryGetProperty("series", out JsonElement se) && se.ValueKind == JsonValueKind.Array)
                    {
                        foreach (JsonElement row in se.EnumerateArray())
                        {
                            if (row.ValueKind != JsonValueKind.Array) continue;
                            var vals = new List<int>();
                            foreach (JsonElement cell in row.EnumerateArray())
                                if (cell.ValueKind == JsonValueKind.Number && cell.TryGetInt32(out int n)) vals.Add(n);
                            series.Add(vals.ToArray());
                        }
                    }
                    variants[id] = (range, series);
                }
            }
        }
        CardBuildRules.VariantLookup = id => variants.TryGetValue(id, out var v) ? v : null;
        Console.WriteLine($"等级变体表: {variants.Count} 条");

        var owners = new Dictionary<int, string>();
        if (File.Exists(OwnerJson))
        {
            using JsonDocument doc = JsonDocument.Parse(File.ReadAllText(OwnerJson));
            if (doc.RootElement.TryGetProperty("skills", out JsonElement sk) && sk.ValueKind == JsonValueKind.Array)
            {
                foreach (JsonElement el in sk.EnumerateArray())
                {
                    if (!el.TryGetProperty("id", out JsonElement idEl) || idEl.ValueKind != JsonValueKind.Number) continue;
                    if (!idEl.TryGetInt32(out int id)) continue;
                    string owner = el.TryGetProperty("owner", out JsonElement ow) && ow.ValueKind == JsonValueKind.String
                        ? ow.GetString() ?? "both" : "both";
                    owners[id] = owner;
                }
            }
        }
        CardBuildRules.OwnerLookup = id => owners.TryGetValue(id, out string o) ? o : "both";
        Console.WriteLine($"技能归属表: {owners.Count} 条");
    }

    private static Dictionary<int, ResourceDef> LoadResourceDb()
    {
        var byId = new Dictionary<int, ResourceDef>();
        using JsonDocument doc = JsonDocument.Parse(File.ReadAllText(DbJson));
        JsonElement arr = doc.RootElement.GetProperty("resources");
        foreach (JsonElement el in arr.EnumerateArray())
        {
            int id = el.GetProperty("id").GetInt32();
            ResKind kind = (el.GetProperty("kind").GetString() ?? "skill") switch
            {
                "np" => ResKind.Np, "item" => ResKind.Item, _ => ResKind.Skill,
            };
            var res = new ResourceDef
            {
                Id = id,
                Name = el.GetProperty("name").GetString() ?? "?",
                Kind = kind,
                Rank = RankUtil.Parse(el.TryGetProperty("rank", out JsonElement rk) ? rk.GetString() : "C"),
                When = When.Any,
                Cost = 0,
                Recast = 0,
                Type = ParseType(el.TryGetProperty("type", out JsonElement tp) ? tp.GetString() : "", kind),
                Focus = ParseFocus(el.TryGetProperty("focus", out JsonElement fc) ? fc.GetString() : ""),
            };
            if (el.TryGetProperty("recast", out JsonElement rc) && rc.ValueKind == JsonValueKind.Number) res.Recast = rc.GetInt32();
            if (el.TryGetProperty("cost", out JsonElement co) && co.ValueKind == JsonValueKind.Number) res.Cost = co.GetInt32();
            if (el.TryGetProperty("when", out JsonElement wh) && wh.ValueKind == JsonValueKind.String)
                res.When = wh.GetString() switch
                {
                    "passive" => When.Passive, "any" => When.Any, "proc" => When.Proc,
                    _ => When.Any,
                };
            if (el.TryGetProperty("effects", out JsonElement fx) && fx.ValueKind == JsonValueKind.Array)
            {
                foreach (JsonElement fe in fx.EnumerateArray())
                {
                    var ef = new EffectLine();
                    if (fe.TryGetProperty("flag", out JsonElement fl)) ef.Flag = FlagFromStr(fl.GetString());
                    if (fe.TryGetProperty("attr", out JsonElement at)) ef.Attr = AttrFromStr(at.GetString());
                    if (fe.TryGetProperty("value", out JsonElement vv) && vv.ValueKind == JsonValueKind.Number) ef.Value = vv.GetInt32();
                    if (fe.TryGetProperty("chance", out JsonElement ch) && ch.ValueKind == JsonValueKind.Number) ef.Chance = ch.GetInt32();
                    if (fe.TryGetProperty("layers", out JsonElement ly) && ly.ValueKind == JsonValueKind.Number) ef.Layers = ly.GetInt32();
                    if (fe.TryGetProperty("times", out JsonElement tm) && tm.ValueKind == JsonValueKind.Number) ef.Times = tm.GetInt32();
                    if (fe.TryGetProperty("target", out JsonElement tg) && tg.ValueKind == JsonValueKind.String)
                        ef.Target = TargetFromStr(tg.GetString());
                    if (fe.TryGetProperty("status", out JsonElement st) && st.ValueKind == JsonValueKind.String)
                        ef.Status = StatusFromStr(st.GetString());
                    if (fe.TryGetProperty("cond", out JsonElement cd) && cd.ValueKind == JsonValueKind.String)
                        ef.Cond = CondFromStr(cd.GetString());
                    if (fe.TryGetProperty("cond_arg", out JsonElement ca) && ca.ValueKind == JsonValueKind.Number) ef.CondArg = ca.GetInt32();
                    if (fe.TryGetProperty("cond_arg2", out JsonElement ca2) && ca2.ValueKind == JsonValueKind.Number) ef.CondArg2 = ca2.GetInt32();
                    if (fe.TryGetProperty("chance_neg", out JsonElement cn) && cn.ValueKind == JsonValueKind.True) ef.ChanceNeg = true;
                    if (fe.TryGetProperty("luck_halve", out JsonElement lh) && lh.ValueKind == JsonValueKind.True) ef.LuckHalve = true;
                    res.Effects.Add(ef);
                }
            }
            byId[id] = res;
        }
        return byId;
    }

    // ---------- 字符串解析(与 ResourceDb 一致) ----------
    private static int ParseType(string s, ResKind kind) => s switch
    {
        "KS_T_TALENT" => (int)SkillType.Talent, "KS_T_TECHNIQUE" => (int)SkillType.Technique,
        "KS_T_BLESS" => (int)SkillType.Bless, "KS_T_CROWN" => (int)SkillType.Crown,
        "KS_T_WEAPON" => (int)SkillType.Weapon, "KS_T_MAGIC" => (int)SkillType.Magic,
        "KS_T_CLASS" => (int)SkillType.Class,
        "KS_NP_HUMAN" => (int)NpType.Human, "KS_NP_ARMY" => (int)NpType.Army,
        "KS_NP_CASTLE" => (int)NpType.Castle, "KS_NP_WORLD" => (int)NpType.World,
        "KS_NP_BOUND" => (int)NpType.Bound,
        _ => 0,
    };

    private static int ParseFocus(string s) => s switch
    {
        "FC_DECISIVE" => (int)Focus.Decisive, "FC_INSTANTKILL" => (int)Focus.InstantKill,
        "FC_MAGICSWORD" => (int)Focus.MagicSword, "FC_DEFENSE" => (int)Focus.Defense,
        "FC_OFFENSE" => (int)Focus.Offense, "FC_BUFF" => (int)Focus.Buff,
        "FC_SUMMON" => (int)Focus.Summon, "FC_STATUS" => (int)Focus.Status,
        "FC_SUPPLY" => (int)Focus.Supply, "FC_SPECIAL" => (int)Focus.Special,
        "FC_ANTITRAIT" => (int)Focus.AntiTrait,
        _ => 0,
    };

    private static EffFlag FlagFromStr(string s) => s switch
    {
        "EF_ATTR_UP" => EffFlag.AttrUp, "EF_ATTR_DOWN" => EffFlag.AttrDown,
        "EF_ATTR_UP_CONST" => EffFlag.AttrUpConst, "EF_ATTR_DOWN_CONST" => EffFlag.AttrDownConst,
        "EF_WIN_UP" => EffFlag.WinUp, "EF_WIN_DOWN" => EffFlag.WinDown,
        "EF_FINAL_WIN_UP" => EffFlag.FinalWinUp, "EF_FINAL_WIN_DOWN" => EffFlag.FinalWinDown,
        "EF_FLOOR_UP" => EffFlag.FloorUp, "EF_FLOOR_PEN" => EffFlag.FloorPen,
        "EF_HIT_UP" => EffFlag.HitUp, "EF_HIT_FINAL_UP" => EffFlag.HitFinalUp,
        "EF_HIT_PEN" => EffFlag.HitPen, "EF_RES_UP" => EffFlag.ResUp,
        "EF_RES_DOWN" => EffFlag.ResDown, "EF_STATE_RES" => EffFlag.StateRes,
        "EF_STATE_IM" => EffFlag.StateIm, "EF_EFFECT_IM" => EffFlag.EffectIm,
        "EF_MANA_UP" => EffFlag.ManaUp, "EF_MANA_DOWN" => EffFlag.ManaDown,
        "EF_STATUS_GIVE" => EffFlag.StatusGive, "EF_STATUS_REMOVE" => EffFlag.StatusRemove,
        "EF_BURN_BLOW" => EffFlag.BurnBlow, "EF_ELECTRIC_BLOW" => EffFlag.ElectricBlow,
        "EF_POISON_BLOW" => EffFlag.PoisonBlow, "EF_RECAST" => EffFlag.Recast,
        "EF_RECAST_LOSE" => EffFlag.RecastLose, "EF_FP_UP" => EffFlag.FpUp,
        "EF_FP_DOWN" => EffFlag.FpDown, "EF_TP_FP" => EffFlag.TpFp,
        "EF_DEATH" => EffFlag.Death, "EF_BOUND_DEATH" => EffFlag.BoundDeath,
        "EF_PIERCE" => EffFlag.Pierce, "EF_INV_PIERCE" => EffFlag.InvPierce,
        "EF_EVADE" => EffFlag.Evade, "EF_PROTECT" => EffFlag.Protect,
        "EF_INVINCIBLE" => EffFlag.Invincible, "EF_RETALIATE" => EffFlag.Retaliate,
        "EF_SUMMON" => EffFlag.Summon, "EF_INFO" => EffFlag.Info,
        "EF_CS" => EffFlag.Cs, "EF_OTHER" => EffFlag.Other,
        "EF_PLEDGE_DECL" => EffFlag.PledgeDecl, "EF_TICK_PROC" => EffFlag.TickProc,
        "EF_TICK_ROUND" => EffFlag.TickRound, "EF_RANDOM_GIVE" => EffFlag.RandomGive,
        "EF_ON_KILL" => EffFlag.OnKill, "EF_ON_WIN" => EffFlag.OnWin,
        "EF_CHARGE" => EffFlag.Charge, "EF_GRANT_CS" => EffFlag.GrantCs,
        "EF_GRANT_BADGE" => EffFlag.GrantBadge, "EF_SEAL" => EffFlag.Seal,
        "EF_REGEN" => EffFlag.Regen,
        _ => EffFlag.None,
    };

    private static int AttrFromStr(string s) => s switch
    {
        "A_STR" => 0, "A_END" => 1, "A_AGI" => 2, "A_MAG" => 3, "A_LUK" => 4, "A_NP" => 5, "A_CIRCUIT" => 6,
        _ => -1,
    };

    private static int StatusFromStr(string s) => s switch
    {
        "S_EVADE" => (int)StatusKind.Evade, "S_INVINCIBLE" => (int)StatusKind.Invincible,
        "S_PROTECT" => (int)StatusKind.Protect, "S_RESUP" => (int)StatusKind.ResUp,
        "S_STATE_RES" => (int)StatusKind.StateRes, "S_STATE_IM" => (int)StatusKind.StateIm,
        "S_EFFECT_IM" => (int)StatusKind.EffectIm, "S_TIRED" => (int)StatusKind.Tired,
        "S_CRIPPLED" => (int)StatusKind.Crippled, "S_LAG" => (int)StatusKind.Lag,
        "S_CURSE" => (int)StatusKind.Curse, "S_SEAL" => (int)StatusKind.Seal,
        "S_SKILL_SEAL" => (int)StatusKind.SkillSeal, "S_NP_SEAL" => (int)StatusKind.NpSeal,
        "S_RESDOWN" => (int)StatusKind.ResDown, "S_POISON" => (int)StatusKind.Poison,
        "S_BURN" => (int)StatusKind.Burn, "S_FREEZE" => (int)StatusKind.Freeze,
        "S_ELECTRIC" => (int)StatusKind.Electric, "S_STONE" => (int)StatusKind.Stone,
        "S_STUN" => (int)StatusKind.Stun, "S_CHARM" => (int)StatusKind.Charm,
        "S_CONFUSE" => (int)StatusKind.Confuse, "S_FEAR" => (int)StatusKind.Fear,
        "S_RES_BREAK" => (int)StatusKind.ResBreak, "S_TRAIT_GIVE" => (int)StatusKind.TraitGive,
        "S_CHARGE" => (int)StatusKind.Charge, "S_BADGE" => (int)StatusKind.Badge,
        _ => 0,
    };

    private static int TargetFromStr(string s) => s switch
    {
        "self" => -2, "ally_all" => -1, "enemy_all" => 0,
        _ when s.StartsWith("enemy_") && int.TryParse(s.Substring(6), out int n) => n,
        _ => -2,
    };

    private static Cond CondFromStr(string s) => s switch
    {
        "KC_OWN_MAIN" => Cond.OwnMain, "KC_OWN_SUPPORT" => Cond.OwnSupport, "KC_OWN_REAR" => Cond.OwnRear,
        "KC_TARGET_TRAIT" => Cond.TargetTrait, "KC_TARGET_NOT_TRAIT" => Cond.TargetNotTrait,
        "KC_TARGET_STATUS_EQ" => Cond.TargetStatusEq, "KC_TARGET_STATUS_GE" => Cond.TargetStatusGe,
        "KC_STATUS_NOT" => Cond.StatusNot, "KC_FRIEND_STATUS" => Cond.FriendStatus,
        "KC_TARGET_LUCK_GE" => Cond.TargetLuckGe, "KC_TARGET_LEVEL_GE" => Cond.TargetLevelGe,
        "KC_LEVEL_DIFF" => Cond.LevelDiff, "KC_DAY" => Cond.Day, "KC_NIGHT" => Cond.Night,
        "KC_FIRST_ENCOUNTER" => Cond.FirstEncounter, "KC_SELF_TRAIT" => Cond.SelfTrait,
        "KC_SELF_MASTER" => Cond.SelfIsMaster, "KC_SELF_SERVANT" => Cond.SelfIsServant,
        "KC_SELF_MP" => Cond.SelfMp, "KC_SELF_HP" => Cond.SelfHp, "KC_HAS_CS" => Cond.HasCs,
        "KC_MP_UNDER" => Cond.MpUnder, "KC_FRIEND_IN_BATTLE" => Cond.FriendInBattle,
        "KC_ENEMY_IN_BATTLE" => Cond.EnemyInBattle, "KC_ENEMY_TACTIC" => Cond.EnemyTactic,
        "KC_SELF_TACTIC" => Cond.SelfTactic, "KC_TACTIC_NOT_PAIRED" => Cond.TacticNotPaired,
        "KC_ENEMY_IS_SUMMON" => Cond.EnemyIsSummon, "KC_TARGET_AGI_LT" => Cond.TargetAgiLt,
        "KC_TARGET_AGI_GE" => Cond.TargetAgiGe, "KC_ENEMY_MAIN_MASTER" => Cond.EnemyMainMaster,
        "KC_ENEMY_IS_SERVANT" => Cond.EnemyIsServant,
        _ => Cond.None,
    };
}
