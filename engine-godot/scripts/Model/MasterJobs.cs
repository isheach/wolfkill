using System;
using System.Collections.Generic;

namespace KsgGodot.Model;

/// <summary>御主职业(规则书附录二): 主职业/子职业 + 职业技能模板</summary>
public static class MasterJobs
{
    /// <summary>主职业</summary>
    public static readonly string[] MainJobs = { "魔术师", "体术师" };

    /// <summary>子职业(含通用子职业)</summary>
    public static readonly string[] SubJobs = {
        "人偶师", "结界师", "咒术师", "炼金术师", "武术家", "刽子手", "剑术师", "武士",
        "死徒", "退魔师", "混血", "传承保菌者", "驱魔师", "修道士", "圣堂骑士", "军部",
        "卢恩使", "魔眼使",  // 通用子职业
        "无",  // 可选不选子职业
    };

    /// <summary>通用子职业(可作为任意主职业的子职业)</summary>
    public static bool IsGeneral(string job) => job == "卢恩使" || job == "魔眼使";

    public static bool IsMainJob(string job) => Array.IndexOf(MainJobs, job) >= 0;
    public static bool IsSubJob(string job) => Array.IndexOf(SubJobs, job) >= 0;

    /// <summary>职业特性描述(演示: 主职业特性+技能模板等级)。</summary>
    public static string JobFeature(string job)
    {
        return job switch
        {
            "魔术师" => "主职业特性: 可习得魔术类职业技能(E级模板起步); 魔力池+20上限",
            "体术师" => "主职业特性: 可习得技艺类职业技能(E级模板起步); 初始FP+1",
            "人偶师" => "子职业: 人偶/构装体相关技能",
            "结界师" => "子职业: 结界/工房相关技能",
            "咒术师" => "子职业: 诅咒/咒术相关技能",
            "炼金术师" => "子职业: 炼金/造物相关技能",
            "武术家" => "子职业: 武术/体术技能",
            "刽子手" => "子职业: 处刑/审判技能",
            "剑术师" => "子职业: 剑术/围棋弈理技能",
            "武士" => "子职业: 武士/魔眼技能",
            "死徒" => "子职业: 吸血/不死技能",
            "退魔师" => "子职业: 退魔/净化技能",
            "混血" => "子职业: 混血特性技能",
            "传承保菌者" => "子职业: 传承/礼装技能",
            "驱魔师" => "子职业: 驱魔/圣言技能",
            "修道士" => "子职业: 灵媒/修行技能",
            "圣堂骑士" => "子职业: 圣典/火器技能",
            "军部" => "子职业: 军务/火力技能",
            "卢恩使" => "通用子职业: 卢恩系列技能(可配任意主职业)",
            "魔眼使" => "通用子职业: 魔眼系列技能(可配任意主职业)",
            _ => "",
        };
    }

    /// <summary>职业绑定的技能名集合(与 ksg_resources 中技能名对应)。</summary>
    public static readonly Dictionary<string, string[]> JobSkills = new()
    {
        ["魔术师"] = new[] { "超越回路", "宝石魔术", "增殖的源", "生命秘术", "反击屏障", "强力召唤", "移动要塞" },
        ["体术师"] = new[] { "强筋锻骨", "身若惊鸿", "先声夺人", "天人合一", "明镜止水", "破境还元", "周天行" },
        ["人偶师"] = new[] { "机巧军势", "机械革命", "义体黎明", "终极巨像", "自我原型" },
        ["结界师"] = new[] { "闲人免进", "七重守护", "罗生三相", "众星拱辰", "阵式混合", "改天换日", "护身结界" },
        ["咒术师"] = new[] { "巫蛊人偶", "神经衰弱", "枯萎之手", "法力汲取", "恶意中伤", "刻印献祭", "祟神使役" },
        ["炼金术师"] = new[] { "分割思考", "自动装械", "炼成总机", "活体熔炉", "机动工造", "周期推演" },
        ["武术家"] = new[] { "毒蛇一艺", "六合八荒", "活杀自在", "错骨缠龙", "一指金刚", "不二法门" },
        ["刽子手"] = new[] { "迟到的正义", "信念的枷锁", "义务的终结", "死亡与新生", "内心的正义", "放逐" },
        ["剑术师"] = new[] { "适情录", "双飞燕", "双倒扑", "接不归", "相思断", "回龙征" },
        ["武士"] = new[] { "魔剑传承", "二天一流", "一刀流", "光芒一闪", "二河白道", "伪·直死之魔眼", "迁延之魔眼" },
        ["死徒"] = new[] { "继理血戒", "暴走冲动", "渴血恶灵", "猩红收割", "触觉延伸", "死亡初拥" },
        ["退魔师"] = new[] { "退魔净眼", "退魔冲动", "此身双灵", "七夜暗杀术", "祟神使役", "净空感应", "破知化物", "禁缚术式" },
        ["混血"] = new[] { "反转冲动", "槛发" },
        ["传承保菌者"] = new[] { "约束之神秘", "流转之传承", "鞘中之锋刃", "鲜血之结末", "因子之烙印", "王之现世身", "洗礼咏唱", "祝福秘迹" },
        ["驱魔师"] = new[] { "灵慑喝止", "破却宣言", "驱魔禁制", "以儆效尤", "告死预言", "第四福音" },
        ["修道士"] = new[] { "殉节历典", "被虐灵媒", "治疗灵媒", "接续灵媒", "律戒原典", "天声启示", "自我鞭策" },
        ["圣堂骑士"] = new[] { "圣母圣咏", "正式外典", "否决圣典", "火葬式典", "资料调取", "资源补给", "执法搭档" },
        ["军部"] = new[] { "枪械精通", "火力支援", "战斗动员", "武装配给", "后勤支援", "区域封锁", "迅速行军" },
        ["卢恩使"] = new[] { "流转之水", "活力之火", "停滞之冰", "共荣之生灵", "丰饶之天使", "觉醒之白昼" },
        ["魔眼使"] = new[] { "魔眼激发", "潜能激发", "意识扩张", "弱点看破", "镜中形影" },
    };

    /// <summary>某技能是否属于某职业。</summary>
    public static bool SkillInJob(string skillName, string job)
    {
        if (string.IsNullOrEmpty(job) || job == "无") return false;
        if (!JobSkills.TryGetValue(job, out var skills)) return false;
        foreach (var s in skills)
            if (skillName.Contains(s) || s.Contains(skillName)) return true;
        return false;
    }

    /// <summary>某技能是否属于该御主的职业(主或子)。</summary>
    public static bool SkillInJobs(string skillName, string mainJob, string subJob)
        => SkillInJob(skillName, mainJob) || SkillInJob(skillName, subJob);
}