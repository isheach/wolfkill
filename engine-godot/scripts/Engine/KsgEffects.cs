using System;
using System.Collections.Generic;
using KsgGodot.Model;

namespace KsgGodot.Engine;

/// <summary>效果结算引擎(与 C 版 ksg_effects.c 逻辑一致)</summary>
public static class KsgEffects
{
    public static event Action<string> VerboseEvent;

    private static void Verbose(string msg) => VerboseEvent?.Invoke(msg);
    public static event Action<string> LogEvent;
    private static void Log(string msg) => LogEvent?.Invoke(msg);

    // ---------- 条件检查 ----------
    public static bool CondCheck(KsgBattle b, UnitDef self, UnitDef target, EffectLine el)
    {
        switch (el.Cond)
        {
            case Cond.None: return true;
            case Cond.OwnMain: return self.BattleSlot == 1;
            case Cond.OwnSupport: return self.BattleSlot == 2;
            case Cond.OwnRear: return self.BattleSlot == 4;
            case Cond.TargetTrait: return target != null && (target.Traits & el.CondArg) != 0;
            case Cond.TargetNotTrait: return target != null && (target.Traits & el.CondArg) == 0;
            case Cond.TargetStatusEq: return target != null && target.HasStatus((StatusKind)el.CondArg);
            case Cond.TargetStatusGe: return target != null && target.GetStatus((StatusKind)el.CondArg) >= el.CondArg2;
            case Cond.StatusNot: return target != null && !target.HasStatus((StatusKind)el.CondArg);
            case Cond.FriendStatus:
                {
                    if (!TryGetSideUnits(b, self, false, out List<BattleUnit> friends)) return false;
                    foreach (BattleUnit friend in friends)
                    {
                        if (!ReferenceEquals(friend.Unit, self) &&
                            friend.Unit.HasStatus((StatusKind)el.CondArg))
                            return true;
                    }
                    return false;
                }
            case Cond.TargetLuckGe: return target != null && target.AttrValue(4) >= (el.CondArg > 0 ? el.CondArg : 40);
            case Cond.TargetLevelGe: return target != null && target.Level >= el.CondArg;
            case Cond.LevelDiff: return target != null && self.Level - target.Level >= el.CondArg;
            case Cond.Day: return b == null || b.Day;
            case Cond.Night: return b != null && !b.Day;
            case Cond.FirstEncounter: return b != null && b.Active && b.FirstEncounter;
            case Cond.SelfTrait: return (self.Traits & el.CondArg) != 0;
            case Cond.SelfIsMaster: return self.IsMaster;
            case Cond.SelfIsServant: return self.IsServant;
            case Cond.SelfMp: return self.MpCur >= el.CondArg;
            case Cond.SelfHp: return self.AttrValue(el.CondArg) >= el.CondArg2;
            case Cond.HasCs:
                {
                    if (!TryGetSideUnits(b, self, false, out List<BattleUnit> friends)) return false;
                    foreach (BattleUnit friend in friends)
                        if (friend.Unit.Cs > 0) return true;
                    return false;
                }
            case Cond.MpUnder: return self.MpCur < 0;
            case Cond.FriendInBattle:
                return TryGetSideUnits(b, self, false, out List<BattleUnit> friendsInBattle) &&
                       friendsInBattle.Count > 1;
            case Cond.EnemyInBattle:
                return TryGetSideUnits(b, self, true, out List<BattleUnit> enemiesInBattle) &&
                       enemiesInBattle.Count > 1;
            case Cond.EnemyTactic:
                {
                    if (!TryGetSides(b, self, out _, out int enemySide)) return false;
                    return b.Tactics[enemySide] == (Tactic)el.CondArg;
                }
            case Cond.SelfTactic:
                {
                    if (!TryGetSides(b, self, out int mySide, out _)) return false;
                    return b.Tactics[mySide] == (Tactic)el.CondArg;
                }
            case Cond.TacticNotPaired:
                {
                    if (!TryGetSides(b, self, out int mySide, out int enemySide)) return false;
                    return TacticUtil.Counter(b.Tactics[mySide], b.Tactics[enemySide]) >= 0;
                }
            case Cond.EnemyIsSummon: return target != null && target.UType == 3;
            case Cond.TargetAgiLt: return target != null && target.AttrValue(2) < el.CondArg;
            case Cond.TargetAgiGe: return target != null && target.AttrValue(2) >= el.CondArg;
            case Cond.EnemyMainMaster:
                {
                    if (!TryGetSides(b, self, out _, out int enemySide)) return false;
                    var em = b.GetMain(enemySide);
                    return em != null && em.IsMaster;
                }
            case Cond.EnemyIsServant:
                {
                    if (!TryGetSides(b, self, out _, out int enemySide)) return false;
                    var em = b.GetMain(enemySide);
                    return em != null && em.IsServant;
                }
            default: return false;
        }
    }

    private static bool TryGetSides(KsgBattle b, UnitDef self, out int mySide, out int enemySide)
    {
        mySide = self?.BattleSide ?? 0;
        enemySide = mySide == 1 ? 2 : 1;
        return b != null && b.Active && (mySide == 1 || mySide == 2);
    }

    private static bool TryGetSideUnits(
        KsgBattle b, UnitDef self, bool enemy, out List<BattleUnit> units)
    {
        units = null;
        if (!TryGetSides(b, self, out int mySide, out int enemySide)) return false;
        int side = enemy ? enemySide : mySide;
        units = side == 1 ? b.Left : b.Right;
        return true;
    }

    // ---------- 目标解析 ----------
    public static List<UnitDef> ResolveTargets(KsgBattle b, UnitDef self, EffectLine el)
    {
        var list = new List<UnitDef>();
        if (el.Target == -2) { list.Add(self); return list; }
        if (b == null || !b.Active) { list.Add(self); return list; }
        int mySide = self.BattleSide;
        int enemySide = mySide == 1 ? 2 : 1;
        if (el.Target == 0)
        {
            foreach (var bu in enemySide == 1 ? b.Left : b.Right) list.Add(bu.Unit);
        }
        else if (el.Target == -1)
        {
            foreach (var bu in mySide == 1 ? b.Left : b.Right) list.Add(bu.Unit);
        }
        else
        {
            var en = enemySide == 1 ? b.Left : b.Right;
            int idx = el.Target - 1;
            if (idx >= 0 && idx < en.Count) list.Add(en[idx].Unit);
        }
        return list;
    }

    // ---------- 判定 ----------
    public static bool EffectCheck(UnitDef self, UnitDef target, EffectLine el, out int rate)
    {
        if (el.Chance > 0) rate = el.Chance;
        else if (el.ChanceAttrBase >= 0 && el.ChanceAttrBase < 7)
            rate = self.AttrValue(el.ChanceAttrBase);
        else { rate = 0; return true; }
        if (el.LuckHalve && target != null && target.AttrValue(4) >= 40) rate /= 2;
        if (el.ChanceNeg && target != null)
        {
            rate -= target.GetStatus(StatusKind.ResUp);
            rate += target.GetStatus(StatusKind.ResDown);
            // 状态抵抗/免疫
            if (target.GetStatus(StatusKind.StateRes) == el.Status) rate = rate / 2 - 20;
            if (target.GetStatus(StatusKind.StateIm) == el.Status) rate = -1;
        }
        rate = Math.Clamp(rate, 0, 100);
        return rate <= 0 ? false : Dice.RollPct(rate);
    }

    /// <summary>攻击性效果(会被回避/无敌拦截)</summary>
    private static bool IsOffensive(EffFlag flag)
    {
        switch (flag)
        {
            case EffFlag.AttrDown:
            case EffFlag.AttrDownConst:
            case EffFlag.WinDown:
            case EffFlag.FinalWinDown:
            case EffFlag.FloorPen:
            case EffFlag.HitPen:
            case EffFlag.ResDown:
            case EffFlag.ManaDown:
            case EffFlag.StatusGive:
            case EffFlag.BurnBlow:
            case EffFlag.ElectricBlow:
            case EffFlag.PoisonBlow:
            case EffFlag.Death:
            case EffFlag.BoundDeath:
            case EffFlag.FpDown:
            case EffFlag.RecastLose:
                return true;
            default:
                return false;
        }
    }

    /// <summary>施加攻击性效果前的 回避/无敌 拦截;[必中]穿透回避、[无敌贯通]穿透无敌。
    /// 资源特效由调用方传入(经 resource.Feat)。返回 true 表示被拦截。</summary>
    public static bool IsBlocked(UnitDef target, UnitDef src, EffectLine el, int resFeat)
    {
        if (src != null && src.Id == target.Id) return false;
        if (!IsOffensive(el.Flag)) return false;
        int feat = resFeat;
        bool hasPierce = (feat & (int)Feat.Pierce) != 0;
        bool hasInvPierce = (feat & (int)Feat.InvPierce) != 0;
        // [无敌]: 不受自身外任意效果影响
        if (target.HasStatus(StatusKind.Invincible) && !hasInvPierce)
            return true;
        // [回避]: 下一个来源非自身的效果无效化
        if (target.HasStatus(StatusKind.Evade) && !hasPierce)
            return true;
        return false;
    }

    // ---------- 效果应用 ----------
    public static void ApplyEffect(KsgBattle b, KsgWorld w, UnitDef target, UnitDef src, EffectLine el, int resFeat = 0)
    {
        if (!target.Alive) return;
        if (!CondCheck(b, src, target, el)) return;
        if (!EffectCheck(src, target, el, out int rate))
        {
            Verbose($"  · {target.Name} 效果判定失败({rate}%)");
            return;
        }
        int times = el.Times > 0 ? el.Times : 1;
        for (int t = 0; t < times; t++)
        {
            switch (el.Flag)
            {
                case EffFlag.AttrUp: ModAttr(target, el.Attr, el.Value, false); break;
                case EffFlag.AttrDown: ModAttr(target, el.Attr, -el.Value, false); break;
                case EffFlag.AttrUpConst: ModAttr(target, el.Attr, el.Value, true); break;
                case EffFlag.AttrDownConst: ModAttr(target, el.Attr, -el.Value, true); break;

                case EffFlag.WinUp:
                case EffFlag.WinDown:
                    {
                        if (b != null && b.Active && target.InBattle)
                        {
                            int side = target.BattleSide == 1 ? 1 : 2;
                            int v = el.Flag == EffFlag.WinUp ? el.Value : -el.Value;
                            b.AddWin(side, v);
                        }
                        break;
                    }
                case EffFlag.FinalWinUp:
                case EffFlag.FinalWinDown:
                    {
                        if (b != null && b.Active && target.InBattle)
                        {
                            int side = target.BattleSide == 1 ? 1 : 2;
                            int v = el.Flag == EffFlag.FinalWinUp ? el.Value : -el.Value;
                            b.AddFinalWin(side, v);
                        }
                        break;
                    }
                case EffFlag.FloorUp:
                case EffFlag.FloorPen:
                    {
                        if (b != null && b.Active && target.InBattle)
                        {
                            int side = target.BattleSide == 1 ? 1 : 2;
                            int v = el.Flag == EffFlag.FloorUp ? el.Value : -el.Value;
                            b.AddFloor(side, v);
                        }
                        break;
                    }
                case EffFlag.HitUp: target.HitMod += el.Value; break;
                case EffFlag.HitFinalUp: target.HitFinalMod += el.Value; break;
                case EffFlag.HitPen: target.HitMod -= el.Value; break;

                case EffFlag.ResUp: target.GainStatus(StatusKind.ResUp, el.Value, src.Id); break;
                case EffFlag.ResDown: target.GainStatus(StatusKind.ResDown, el.Value, src.Id); break;
                case EffFlag.StateRes:
                    target.GainStatus(StatusKind.StateRes, el.Status, src.Id); break;
                case EffFlag.StateIm:
                    target.GainStatus(StatusKind.StateIm, el.Status, src.Id); break;

                case EffFlag.ManaUp:
                    target.MpCur = Math.Min(target.MpCur + el.Value, target.MpCap);
                    break;
                case EffFlag.ManaDown:
                    target.MpCur = Math.Max(target.MpCur - el.Value, target.MpFloor);
                    break;

                case EffFlag.StatusGive:
                    target.GainStatus((StatusKind)el.Status, el.Layers, src.Id);
                    Verbose($"  · {target.Name} 获得 {StatusUtil.Name((StatusKind)el.Status)}{el.Layers}");
                    break;
                case EffFlag.StatusRemove:
                    target.LoseStatus((StatusKind)el.Status, el.Layers > 0 ? el.Layers : 1);
                    break;

                case EffFlag.Recast:
                    RecastTarget(target, el.Value);
                    break;
                case EffFlag.FpUp: target.Fp += el.Value; break;
                case EffFlag.FpDown: target.Fp = Math.Max(0, target.Fp - el.Value); break;
                case EffFlag.TpFp: target.TpFp += el.Value; break;

                case EffFlag.Death:
                    DoDeath(target, el, src);
                    break;
                case EffFlag.BoundDeath:
                    DoBoundDeath(target, el);
                    break;

                case EffFlag.Evade: target.GainStatus(StatusKind.Evade, 1, src.Id); break;
                case EffFlag.Protect: target.GainStatus(StatusKind.Protect, 1, src.Id); break;
                case EffFlag.Invincible: target.GainStatus(StatusKind.Invincible, 1, src.Id); break;
                case EffFlag.Pierce: target.GainStatus(StatusKind.EffectIm, 0, src.Id); break;

                case EffFlag.BurnBlow: DoBurnBlow(target); break;
                case EffFlag.ElectricBlow: DoElectricBlow(b, target); break;
                case EffFlag.PoisonBlow: DoPoisonBlow(b, target); break;

                // ---- 复杂机制模块 ----
                case EffFlag.PledgeDecl: DoPledgeDecl(b, src, target, el); break;
                case EffFlag.TickProc: DoTickProc(b, w, target, el, isRound: false); break;
                case EffFlag.TickRound: DoTickProc(b, w, target, el, isRound: true); break;
                case EffFlag.RandomGive: DoRandomGive(target, el, src); break;
                case EffFlag.OnKill: RegisterKillTrigger(b, src, el); break;
                case EffFlag.OnWin: RegisterWinTrigger(b, src, el); break;
                case EffFlag.Charge: target.GainStatus(StatusKind.Charge, el.Layers > 0 ? el.Layers : el.Value, src.Id); break;
                case EffFlag.GrantCs: target.Cs += el.Value; break;
                case EffFlag.GrantBadge: target.GainStatus(StatusKind.Badge, el.Value > 0 ? el.Value : 1, src.Id); break;
                case EffFlag.Regen: target.MpCur = Math.Min(target.MpCur + el.Value, target.MpCap); break;

                default: break;
            }
        }
    }

    // ---------- 复杂机制实现 ----------

    /// <summary>宣言对抗(规则书"宣言"机制): 各工序双方暗宣言状态, 相同→本工序失效, 不同→给予目标状态层数。
    /// 演示简化: 每工序开始双方各掷一次, 相同则跳过, 不同则给敌方主力宣言状态(Value层)。</summary>
    private static void DoPledgeDecl(KsgBattle b, UnitDef src, UnitDef target, EffectLine el)
    {
        if (b == null || !b.Active) return;
        if ((StatusKind)el.Status == StatusKind.None) return;
        int mine = Dice.Roll();
        int theirs = Dice.Roll();
        if (mine == theirs)
        {
            Log($"  · {src.Name} 宣言:{StatusUtil.Name((StatusKind)el.Status)} 与敌方主力宣言相同,本工序效果失效");
            return;
        }
        UnitDef enemyMain = b.GetMain(src.BattleSide == 1 ? 2 : 1);
        if (enemyMain != null)
        {
            int layers = Math.Max(1, el.Value);
            enemyMain.GainStatus((StatusKind)el.Status, layers, src.Id);
            Verbose($"  · 宣言不同! 敌方主力 {enemyMain.Name} 获得 {StatusUtil.Name((StatusKind)el.Status)}{layers}");
        }
    }

    /// <summary>每工序/回合结算(抽奖型 dot): 目标持对应状态层数, 每天按 Chance% 判定, 成功执行效果。
    /// 演示: 层数作为成功率修正; el.Status 为结算判定状态(0=通用), success 效果由 el.Value 表达(伤害/胜率等)。</summary>
    private static void DoTickProc(KsgBattle b, KsgWorld w, UnitDef target, EffectLine el, bool isRound)
    {
        if (b == null || !b.Active) return;
        if (el.Status > 0)
        {
            int layers = target.GetStatus((StatusKind)el.Status);
            if (layers <= 0) return;
            int rate = el.Chance > 0 ? el.Chance : layers * 10;   // 无Chance默认层数*10%
            rate = Math.Clamp(rate, 0, 100);
            if (Dice.RollPct(rate))
            {
                Verbose($"  · {(isRound ? "每回合" : "每工序")}结算:{target.Name} {StatusUtil.Name((StatusKind)el.Status)}{layers} 判定{rate}%成功");
                // 成功时执行对应小型效果(胜率惩罚/属性)
                if (el.Flag != EffFlag.None)
                {
                    var sub = new EffectLine { Flag = el.Value > 0 ? EffFlag.WinDown : EffFlag.WinUp, Value = Math.Abs(el.Value), Target = el.Target };
                    ApplyEffect(b, w, target, target, sub);
                }
            }
            else Verbose($"  · {(isRound ? "每回合" : "每工序")}结算:{target.Name} 判定{rate}%失败");
        }
    }

    /// <summary>随机状态赋予: 从状态池 STATUS_POOL 随机一项, 给予层数。</summary>
    private static readonly StatusKind[] RandomPool = {
        StatusKind.Burn, StatusKind.Electric, StatusKind.Freeze, StatusKind.Poison,
        StatusKind.Curse, StatusKind.Charm, StatusKind.Confuse, StatusKind.Fear, StatusKind.Tired,
    };

    private static void DoRandomGive(UnitDef target, EffectLine el, UnitDef src)
    {
        StatusKind pick = RandomPool[Dice.Next(RandomPool.Length)];
        int layers = el.Layers > 0 ? el.Layers : 1;
        target.GainStatus(pick, layers, src.Id);
        Verbose($"  · 随机状态:{target.Name} 获得 {StatusUtil.Name(pick)}{layers}");
    }

    /// <summary>击杀触发器: 记录到战斗场景, 当击杀发生时结算。</summary>
    private static void RegisterKillTrigger(KsgBattle b, UnitDef src, EffectLine el)
    {
        if (b != null && b.KillTriggers != null)
            b.KillTriggers.Add((src, el));
    }

    /// <summary>胜利触发器: 战斗胜利时将效果应用于己方。</summary>
    private static void RegisterWinTrigger(KsgBattle b, UnitDef src, EffectLine el)
    {
        if (b != null && b.WinTriggers != null)
            b.WinTriggers.Add((src, el));
    }

    /// <summary>每工序结算时对双方蓄力状态 -1 层, 归零结算。</summary>
    public static void TickCharges(KsgBattle b, KsgWorld w)
    {
        if (b == null) return;
        void Step(UnitDef u, int side)
        {
            int ch = u.GetStatus(StatusKind.Charge);
            if (ch > 0)
            {
                ch -= 1;
                if (ch > 0) { u.LoseStatus(StatusKind.Charge, 1); }
                else { u.ClearStatus(StatusKind.Charge); Verbose($"  · {u.Name} 蓄力完成,触发结算"); }
            }
        }
        foreach (var bu in b.Left) Step(bu.Unit, 1);
        foreach (var bu in b.Right) Step(bu.Unit, 2);
    }

    private static void ModAttr(UnitDef u, int attr, int v, bool perm)
    {
        if (attr < 0 || attr >= 7) return;
        if (perm) u.AttrPerm[attr] += v;
        else u.AttrMod[attr] += v;
        Verbose($"  · {u.Name} {AttrName(attr)} {v:+0;-0}{(perm ? "(常驻)" : "")}");
    }

    private static string AttrName(int a) => new[] { "筋力", "耐久", "敏捷", "魔力", "幸运", "宝具", "回路" }[Math.Clamp(a, 0, 6)];

    private static void RecastTarget(UnitDef u, int v)
    {
        foreach (var s in u.Skills) if (s.Recast > 0) s.CurRecast = Math.Min(s.Recast, s.CurRecast + v);
        foreach (var p in u.Phantasms) if (p.Recast > 0) p.CurRecast = Math.Min(p.Recast, p.CurRecast + v);
    }

    private static void DoDeath(UnitDef t, EffectLine el, UnitDef src)
    {
        int r2 = el.Value;
        if (el.LuckHalve && t.AttrValue(4) >= 40) r2 /= 2;
        r2 = Math.Clamp(r2, 0, 100);
        Log($"  · {t.Name} 受到[即死]判定 {r2}%");
        if (Dice.RollPct(r2))
        {
            if (t.Cs > 0)
            {
                t.Cs--;
                Log($"    以1枚令咒抵消!");
            }
            else
            {
                t.Alive = false;
                Log($"    {t.Name} 即死,退场!");
                // 击杀触发(复杂机制: 击杀者获得效果)
                UnitKilled?.Invoke(src, t);
            }
        }
    }

    /// <summary>单位被击杀时触发(战斗场景订阅以结算 OnKill 效果)。</summary>
    public static event System.Action<UnitDef, UnitDef> UnitKilled;

    private static void DoBoundDeath(UnitDef t, EffectLine el)
    {
        // 无敌/回避拦截
        if (t.HasStatus(StatusKind.Evade))
        {
            Log($"  · {t.Name} 以[回避]闪避了轰击!");
            t.ClearStatus(StatusKind.Evade);
            return;
        }
        if (t.HasStatus(StatusKind.Invincible))
        {
            Log($"  · {t.Name} 以[无敌]无效化轰击!");
            return;
        }
        int rate = Math.Clamp(el.Value, 0, 100);
        if (Dice.RollPct(rate))
        {
            if (t.UType == 3 || t.UType == 4)
            {
                Log($"  · 轰击成功!{t.Name}(召唤/人偶) 退场");
                t.Alive = false;
            }
            else Log($"  · 轰击判定成功!{t.Name} 受到轰击影响");
        }
        else Verbose($"  · {t.Name} 轰击判定失败({rate}%)");
    }

    private static void DoBurnBlow(UnitDef t)
    {
        int layers = t.GetStatus(StatusKind.Burn);
        if (layers > 0)
        {
            for (int a = 0; a < 5; a++) t.AttrMod[a] -= 5 * layers;
            t.ClearStatus(StatusKind.Burn);
            Log($"  · {t.Name} [爆燃]!清除灼伤{layers}层,除宝具外全属性-{5 * layers}");
        }
    }

    private static void DoElectricBlow(KsgBattle b, UnitDef t)
    {
        int layers = t.GetStatus(StatusKind.Electric);
        if (layers > 0)
        {
            if (b != null && b.Active && t.InBattle)
                b.AddWin(t.BattleSide == 1 ? 1 : 2, -10 * layers);
            t.ClearStatus(StatusKind.Electric);
            Log($"  · {t.Name} [激荡]!清除感电{layers}层,胜率-{10 * layers}%");
        }
    }

    private static void DoPoisonBlow(KsgBattle b, UnitDef t)
    {
        int layers = t.GetStatus(StatusKind.Poison);
        if (layers > 0)
        {
            int r2 = Math.Clamp(10 * layers - t.AttrValue(1) / 2, 0, 100);
            if (t.AttrValue(1) >= 20) r2 /= 2;
            Log($"  · {t.Name} [毒发]!中毒{layers}层,即死判定 {r2}%");
            if (Dice.RollPct(r2))
            {
                if (t.Cs > 0) { t.Cs--; Log("    消耗1令咒抵消!"); }
                else { t.Alive = false; Log($"    {t.Name} 毒发身亡!"); }
            }
            int remain = (layers + 1) / 2;
            t.LoseStatus(StatusKind.Poison, layers - remain);
        }
    }

    /// <summary>标准发动:校验魔力/回转→扣除魔力→重置回转→按目标应用效果(引擎统一入口)</summary>
    /// <returns>是否成功发动</returns>
    public static bool CastResource(KsgBattle b, KsgWorld w, UnitDef src, ResourceDef r, int targetOverride, out string failReason)
    {
        failReason = "";
        if (r == null || r.IsPassive) { failReason = "常驻或空资源"; return false; }
        if (r.Cost > 0 && r.Cost > src.MpCur) { failReason = "魔力不足"; return false; }
        if (r.Recast > 0 && r.CurRecast < r.Recast) { failReason = $"回转未完成({r.CurRecast}/{r.Recast})"; return false; }
        // 礼装每轮次数上限(规则: 御主每轮5次, 从者0, Caster视为御主放宽)
        if (r.Kind == ResKind.Item)
        {
            int maxUses = src.UType == 2 ? 5 : (src.UType == 1 && src.ServantClass == 5 ? 5 : 0);
            if (src.ItemUsesThisRound >= maxUses)
            {
                failReason = $"本轮礼装使用已达上限({maxUses})";
                return false;
            }
            src.ItemUsesThisRound++;
        }
        if (r.Cost > 0) src.MpCur -= r.Cost;
        r.ResetRecast();
        ApplyResource(b, w, src, r, targetOverride);
        // 规则 1.4: 宝具类型减少灵脉人流量(对军-1/对城-2/对界-3)
        if (b != null && b.LeylineRef != null && r.Kind == ResKind.Np)
        {
            int flowDown = r.Type switch
            {
                (int)NpType.Army => 1,
                (int)NpType.Castle => 2,
                (int)NpType.World => 3,
                _ => 0,
            };
            if (flowDown > 0)
            {
                b.LeylineRef.FlowDown(flowDown);
                Verbose($"  · 灵脉[{b.LeylineRef.Name}] 人流量-{flowDown}(现{b.LeylineRef.Flow})");
            }
        }
        return true;
    }

    // ---------- 资源全效果结算 ----------
    public static void ApplyResource(KsgBattle b, KsgWorld w, UnitDef src, ResourceDef r, int targetOverride)
    {
        foreach (var el in r.Effects)
        {
            if (el.Flag == EffFlag.None) continue;
            var targets = new List<UnitDef>();
            if (targetOverride != 0)
            {
                var t = b != null ? b.UnitById(targetOverride) : null;
                if (t != null) targets.Add(t);
            }
            else targets = ResolveTargets(b, src, el);
            foreach (var t in targets)
            {
                if (r != null && IsBlocked(t, src, el, r.Feat))
                {
                    if (t.HasStatus(StatusKind.Evade))
                    {
                        Log($"  · {t.Name} 以[回避]闪避了 {src.Name} 的 {r.Name}!");
                        t.LoseStatus(StatusKind.Evade, 1);
                    }
                    else if (t.HasStatus(StatusKind.Invincible))
                        Log($"  · {t.Name} 以[无敌]拦截了 {src.Name} 的 {r.Name}!");
                    continue;
                }
                ApplyEffect(b, w, t, src, el, r != null ? r.Feat : 0);
            }
        }
    }
}
