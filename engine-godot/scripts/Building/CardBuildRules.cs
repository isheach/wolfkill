using System;
using System.Collections.Generic;
using KsgGodot.Model;

namespace KsgGodot.Building;

/// <summary>
/// 第一版可执行建卡规则。资源库尚未携带御主职业与礼装类别元数据，相关限制采用界面明示的
/// 保守规则；所有 RP 与栏位判断集中在这里，避免 UI、存档和测试各写一套。
/// </summary>
public static class CardBuildRules
{
    public const int ServantRp = 24;
    public const int ServantSkillSlots = 3;
    public const int ServantNpSlots = 3;
    public const int MasterSkillSlots = 3;
    public const int MasterItemSlots = 3;
    public const int NpExpansionCost = 2;
    public const int ItemExpansionCost = 1;
    public const int SimplifiedItemCost = 3;

    public static int TotalRp(UnitDef unit)
    {
        if (unit == null) return 0;
        if (unit.IsServant) return ServantRp;
        if (!unit.IsMaster) return 0;
        int levelRp = unit.Level switch
        {
            >= 40 => 12,
            >= 30 => 8,
            >= 20 => 4,
            _ => 0,
        };
        return 12 + levelRp;
    }

    public static int RankCost(ResourceDef resource)
    {
        if (resource == null) return 0;
        if (resource.Kind == ResKind.Item) return SimplifiedItemCost;
        Rank rank = resource.EffectiveRankNow;
        return rank switch
        {
            Rank.Ex => resource.Kind == ResKind.Np ? 9 : 7,
            Rank.A => 5,
            Rank.B => 4,
            Rank.C => 3,
            Rank.D => 2,
            Rank.E => 1,
            // “-”同属特殊等级。当前资源没有独立价格字段，按 EX 保守计费。
            _ => resource.Kind == ResKind.Np ? 9 : 7,
        };
    }

    public static int SpentRp(UnitDef unit)
    {
        if (unit == null) return 0;
        int spent = 0;
        foreach (ResourceDef skill in unit.Skills)
            if (skill.Type != (int)SkillType.Class) spent += RankCost(skill);
        foreach (ResourceDef phantasm in unit.Phantasms) spent += RankCost(phantasm);
        foreach (ResourceDef item in unit.Items) spent += RankCost(item);

        if (unit.IsServant)
            spent += Math.Max(0, unit.Phantasms.Count - 1) * NpExpansionCost;
        if (unit.IsMaster)
            spent += Math.Max(0, unit.Items.Count - 1) * ItemExpansionCost;
        return spent;
    }

    public static int RemainingRp(UnitDef unit) => TotalRp(unit) - SpentRp(unit);

    public static int IncrementalCost(UnitDef unit, ResourceDef resource)
    {
        if (unit == null || resource == null) return 0;
        int cost = RankCost(resource);
        if (unit.IsServant && resource.Kind == ResKind.Np && unit.Phantasms.Count >= 1)
            cost += NpExpansionCost;
        if (unit.IsMaster && resource.Kind == ResKind.Item && unit.Items.Count >= 1)
            cost += ItemExpansionCost;
        return cost;
    }

    public static bool CanAdd(UnitDef unit, ResourceDef resource, out string reason)
    {
        reason = "";
        if (unit == null || resource == null)
        {
            reason = "单位或资源为空";
            return false;
        }
        if (!unit.IsServant && !unit.IsMaster)
        {
            reason = "请先选择从者或御主模式";
            return false;
        }
        if (ContainsResource(unit, resource.Id))
        {
            reason = "已经持有同一资源";
            return false;
        }

        // 购入前置: 必须已持有指定资源(规则书"仅能通过[X]获取")
        if (!string.IsNullOrEmpty(resource.Prereq))
        {
            bool hasPrereq = false;
            foreach (ResourceDef held in unit.Skills)
                if (held.Name == resource.Prereq) { hasPrereq = true; break; }
            foreach (ResourceDef held in unit.Phantasms)
                if (held.Name == resource.Prereq) { hasPrereq = true; break; }
            if (!hasPrereq)
            {
                reason = $"购入前置：需先持有「{resource.Prereq}」（规则书：仅能通过其获取）";
                return false;
            }
        }

        // 购入互斥: 不能与指定资源同持(规则书"无法与[X]一同购入")
        if (!string.IsNullOrEmpty(resource.ExclusiveWith))
        {
            foreach (ResourceDef held in unit.Skills)
                if (held.Name == resource.ExclusiveWith)
                {
                    reason = $"互斥：不能与「{resource.ExclusiveWith}」一同购入";
                    return false;
                }
            foreach (ResourceDef held in unit.Phantasms)
                if (held.Name == resource.ExclusiveWith)
                {
                    reason = $"互斥：不能与「{resource.ExclusiveWith}」一同购入";
                    return false;
                }
        }

        if (resource.Kind == ResKind.Skill)
        {
            // 归属限制: 从者库技能仅供从者, 御主库技能仅供御主(规则书两库分离)
            string owner = OwnerLookup?.Invoke(resource.Id) ?? "both";
            if (unit.IsServant && owner == "master")
            {
                reason = $"御主专属技能：从者不能直接购入「{resource.Name}」（规则书：御主库技能）";
                return false;
            }
            if (unit.IsMaster && owner == "servant")
            {
                reason = $"从者专属技能：御主不能直接购入「{resource.Name}」（规则书：从者库技能）";
                return false;
            }
            // 御主职业限制: 职业技能必须所属职业已选(主/子职业); 未匹配职业的御主技能视为通用技能自由购买
            if (unit.IsMaster && owner == "master")
            {
                if (BelongsToSomeJob(resource.Name) && !MatchAnyJob(resource.Name, unit.MainJob, unit.SubJob))
                {
                    reason = $"「{resource.Name}」属于特定职业职业技能; 需先选择对应主/子职业(当前主:{unit.MainJob} 子:{unit.SubJob})";
                    return false;
                }
            }
            if (resource.Type == (int)SkillType.Class)
            {
                reason = "职阶技能由从者职阶自动配置，不能手动购买";
                return false;
            }
            int purchasedSkills = PurchasedSkillCount(unit);
            int maxSkills = unit.IsServant ? ServantSkillSlots : MasterSkillSlots;
            if (purchasedSkills >= maxSkills)
            {
                reason = $"保有技能栏已满（{maxSkills}格）";
                return false;
            }
            foreach (ResourceDef skill in unit.Skills)
            {
                if (skill.Type != (int)SkillType.Class && skill.Type == resource.Type)
                {
                    reason = $"技能面向“{SkillFaceName(resource.Type)}”已经购入1个";
                    return false;
                }
            }
        }
        else if (resource.Kind == ResKind.Np)
        {
            if (!unit.IsServant)
            {
                reason = "御主简化建卡不能直接购买宝具";
                return false;
            }
            if (unit.Phantasms.Count >= ServantNpSlots)
            {
                reason = $"宝具栏已达上限（{ServantNpSlots}格）";
                return false;
            }
            foreach (ResourceDef phantasm in unit.Phantasms)
            {
                if (phantasm.Focus == resource.Focus)
                {
                    reason = $"宝具面向“{FocusName(resource.Focus)}”已经购入1个";
                    return false;
                }
            }
        }
        else
        {
            if (!unit.IsMaster)
            {
                reason = "从者不能在本建卡器中直接购买礼装";
                return false;
            }
            if (unit.Items.Count >= MasterItemSlots)
            {
                reason = $"初始礼装栏已达上限（{MasterItemSlots}格）";
                return false;
            }
        }

        int cost = IncrementalCost(unit, resource);
        if (RemainingRp(unit) < cost)
        {
            reason = $"RP不足：需要{cost}，剩余{RemainingRp(unit)}";
            return false;
        }
        return true;
    }

    /// <summary>技能名是否属于任一职业的技能清单。</summary>
    private static bool BelongsToSomeJob(string skillName)
    {
        string flat = skillName.Split('(')[0].Split('（')[0].Trim();
        foreach (var kv in Model.MasterJobs.JobSkills)
            foreach (var s in kv.Value)
                if (flat.Contains(s) || s.Contains(flat))
                    return true;
        return false;
    }

    /// <summary>技能名是否匹配已选主/子职业。</summary>
    private static bool MatchAnyJob(string skillName, string mainJob, string subJob)
        => Model.MasterJobs.SkillInJobs(skillName, mainJob, subJob);

    public static bool TryAdd(UnitDef unit, ResourceDef resource, out string reason)
    {
        return TryAdd(unit, resource, resource.EffectiveRankNow, out reason);
    }

    /// <summary>以指定等级加入(建卡时可买低等级版本;效果数值按规则书等级表,缺失时系数缩放)。</summary>
    public static bool TryAdd(UnitDef unit, ResourceDef resource, Rank effectiveRank, out string reason)
    {
        if (!CanAdd(unit, resource, out reason)) return false;
        ResourceDef copy = BuildRankedCopy(resource, effectiveRank);
        switch (copy.Kind)
        {
            case ResKind.Skill: unit.Skills.Add(copy); break;
            case ResKind.Np: unit.Phantasms.Add(copy); break;
            case ResKind.Item: unit.Items.Add(copy); break;
        }
        return true;
    }

    /// <summary>生成指定等级的资源副本: 优先使用规则书等级变体表的真实数值, 否则系数缩放。
    /// 变体表由 ResourceDb 注入(纯 C# 测试环境不注入时走系数兜底)。</summary>
    public static Func<int, (string[] RankRange, List<int[]> Series)?> VariantLookup;

    /// <summary>技能归属查询由 ResourceDb 注入: id → "servant"/"master"/"both"(未注入时宽松为 both)。</summary>
    public static Func<int, string> OwnerLookup;

    /// <summary>生成指定等级的资源副本: 优先使用规则书等级变体表的真实数值, 否则系数缩放。
    /// 供建卡、存档载入(卡面 rank)与测试共用。</summary>
    public static ResourceDef RankedCopy(ResourceDef source, Rank effectiveRank)
        => BuildRankedCopy(source, effectiveRank);

    private static ResourceDef BuildRankedCopy(ResourceDef source, Rank effectiveRank)
    {
        // 无变体表且未降级 → 原样
        if (effectiveRank <= Rank.Neg || effectiveRank == source.Rank)
            return source.DeepClone();

        // 先按系数缩放兜底(降级时); 升级时不动数值(由变体表覆盖)
        ResourceDef copy = effectiveRank < source.Rank
            ? source.DeepClone(effectiveRank)
            : source.DeepClone();
        copy.EffectiveRank = effectiveRank;

        // 尝试用等级变体表替换效果数值(真实规则书数值, 覆盖升降级)
        var variant = VariantLookup?.Invoke(source.Id);
        if (variant.HasValue)
        {
            var (range, series) = variant.Value;
            // 找该等级在范围中的索引(A..E)
            int idx = System.Array.IndexOf(range, RankUtil.Name(effectiveRank));
            if (idx >= 0)
            {
                int effectPos = 0;
                foreach (EffectLine effect in copy.Effects)
                {
                    if (effect.Value == 0 && effect.Chance == 0 && effect.Layers == 0) continue;
                    if (effectPos < series.Count && idx < series[effectPos].Length)
                    {
                        int real = series[effectPos][idx];
                        if (effect.Value != 0) effect.Value = real;
                        else if (effect.Chance != 0) effect.Chance = real;
                        else if (effect.Layers != 0) effect.Layers = real;
                    }
                    effectPos++;
                }
            }
        }
        return copy;
    }

    public static bool Remove(UnitDef unit, int resourceId)
    {
        if (unit == null || resourceId <= 0) return false;
        int index = unit.Skills.FindIndex(resource =>
            resource.Id == resourceId && resource.Type != (int)SkillType.Class);
        if (index >= 0)
        {
            unit.Skills.RemoveAt(index);
            return true;
        }
        index = unit.Phantasms.FindIndex(resource => resource.Id == resourceId);
        if (index >= 0)
        {
            unit.Phantasms.RemoveAt(index);
            return true;
        }
        index = unit.Items.FindIndex(resource => resource.Id == resourceId);
        if (index >= 0)
        {
            unit.Items.RemoveAt(index);
            return true;
        }
        return false;
    }

    public static bool Validate(UnitDef unit, out string reason)
    {
        reason = "";
        if (unit == null || (!unit.IsServant && !unit.IsMaster))
        {
            reason = "卡面类型无效";
            return false;
        }
        if (string.IsNullOrWhiteSpace(unit.Name))
        {
            reason = "请填写角色代号";
            return false;
        }
        if (unit.IsServant)
        {
            if (unit.ServantClass < 1 || unit.ServantClass > 10 ||
                unit.HiddenAttr < 0 || unit.HiddenAttr > 4)
            {
                reason = "从者职阶或隐藏属性无效";
                return false;
            }
            if (unit.Items.Count > 0)
            {
                reason = "从者不能直接持有建卡礼装";
                return false;
            }
            if (PurchasedSkillCount(unit) > ServantSkillSlots || unit.Phantasms.Count > ServantNpSlots)
            {
                reason = "从者资源栏位超过上限";
                return false;
            }
        }
        else
        {
            if (unit.Level is not (10 or 20 or 30 or 40))
            {
                reason = "御主等级必须为10、20、30或40";
                return false;
            }
            if (unit.ServantClass != 0 || unit.HiddenAttr != -1 || unit.Phantasms.Count > 0)
            {
                reason = "御主卡含有不适用的从者字段或宝具";
                return false;
            }
            if (PurchasedSkillCount(unit) > MasterSkillSlots || unit.Items.Count > MasterItemSlots)
            {
                reason = "御主资源栏位超过上限";
                return false;
            }
        }

        var resourceIds = new HashSet<int>();
        var skillFaces = new HashSet<int>();
        var npFaces = new HashSet<int>();
        foreach (ResourceDef skill in unit.Skills)
        {
            if (!resourceIds.Add(skill.Id))
            {
                reason = $"资源重复：{skill.Name}";
                return false;
            }
            if (skill.Type != (int)SkillType.Class && !skillFaces.Add(skill.Type))
            {
                reason = $"技能面向重复：{SkillFaceName(skill.Type)}";
                return false;
            }
        }
        foreach (ResourceDef phantasm in unit.Phantasms)
        {
            if (!resourceIds.Add(phantasm.Id) || !npFaces.Add(phantasm.Focus))
            {
                reason = $"宝具资源或面向重复：{phantasm.Name}";
                return false;
            }
        }
        foreach (ResourceDef item in unit.Items)
        {
            if (!resourceIds.Add(item.Id))
            {
                reason = $"资源重复：{item.Name}";
                return false;
            }
        }
        if (SpentRp(unit) > TotalRp(unit))
        {
            reason = $"RP超额：已用{SpentRp(unit)}，总计{TotalRp(unit)}";
            return false;
        }
        return true;
    }

    public static int PurchasedSkillCount(UnitDef unit)
    {
        int count = 0;
        if (unit == null) return count;
        foreach (ResourceDef skill in unit.Skills)
            if (skill.Type != (int)SkillType.Class) count++;
        return count;
    }

    public static string SkillFaceName(int type) => type switch
    {
        (int)SkillType.Talent => "天赋",
        (int)SkillType.Technique => "技艺",
        (int)SkillType.Bless => "祝福",
        (int)SkillType.Crown => "荣冠",
        (int)SkillType.Weapon => "兵器",
        (int)SkillType.Magic => "魔术",
        (int)SkillType.Class => "职阶",
        _ => "未知",
    };

    public static string FocusName(int focus) => focus switch
    {
        (int)Focus.Decisive => "决战",
        (int)Focus.InstantKill => "即死",
        (int)Focus.MagicSword => "魔剑",
        (int)Focus.Defense => "防御",
        (int)Focus.Offense => "进攻",
        (int)Focus.Buff => "增益",
        (int)Focus.Summon => "召唤",
        (int)Focus.Status => "状态",
        (int)Focus.Supply => "补给",
        (int)Focus.Special => "特殊",
        (int)Focus.AntiTrait => "特攻",
        _ => "无",
    };

    private static bool ContainsResource(UnitDef unit, int id)
    {
        foreach (ResourceDef resource in unit.Skills) if (resource.Id == id) return true;
        foreach (ResourceDef resource in unit.Phantasms) if (resource.Id == id) return true;
        foreach (ResourceDef resource in unit.Items) if (resource.Id == id) return true;
        return false;
    }
}
