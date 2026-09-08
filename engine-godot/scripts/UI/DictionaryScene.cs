using Godot;
using KsgGodot.Building;
using KsgGodot.Model;
using System;
using System.Collections.Generic;
using System.Text;

namespace KsgGodot.UI;

/// <summary>词典查询：状态说明，以及带组合筛选和详情面板的轻量资源浏览器。</summary>
public partial class DictionaryScene : Control
{
    private static readonly string[] FocusNames =
    {
        "全部面向", "决战", "即死", "魔剑", "防御", "进攻", "增益",
        "召唤", "状态", "补给", "特殊", "特攻"
    };

    private LineEdit _search;
    private OptionButton _kindFilter;
    private OptionButton _focusFilter;
    private Label _resultCount;
    private ItemList _resourceList;
    private RichTextLabel _detail;
    private List<ResourceDef> _results = new();

    public override void _Ready()
    {
        AppTheme.Apply(this);
        Data.ResourceDb.Load();

        var margin = new MarginContainer
        {
            Name = "PageMargin", AnchorRight = 1, AnchorBottom = 1,
        };
        margin.AddThemeConstantOverride("margin_left", 16);
        margin.AddThemeConstantOverride("margin_top", 12);
        margin.AddThemeConstantOverride("margin_right", 16);
        margin.AddThemeConstantOverride("margin_bottom", 12);
        AddChild(margin);

        var root = new VBoxContainer
        {
            Name = "DictionaryContent",
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        margin.AddChild(root);
        root.AddChild(AppTheme.MakeHeader(
            "词典查询（规则书附录三）",
            () => SceneRouter.GoMainMenu(this)));

        var tab = new TabContainer
        {
            Name = "DictionaryTabs",
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        root.AddChild(tab);

        tab.AddChild(BuildStatusPage());
        tab.SetTabTitle(0, "状态与特效");
        tab.AddChild(BuildResourcePage());
        tab.SetTabTitle(1, "资源库");

        ApplyFilters();
    }

    private static Control BuildStatusPage()
    {
        var scroll = new ScrollContainer
        {
            Name = "StatusAndEffects",
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
            HorizontalScrollMode = ScrollContainer.ScrollMode.Disabled,
        };
        scroll.AddChild(new Label
        {
            Text = StatusDictionaryText,
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
        });
        return scroll;
    }

    private Control BuildResourcePage()
    {
        var page = new VBoxContainer
        {
            Name = "Resources",
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };

        var filters = new HBoxContainer { Name = "ResourceFilters" };
        page.AddChild(filters);
        _search = new LineEdit
        {
            Name = "DictionarySearch",
            PlaceholderText = "搜索名称、原文或资源 ID",
            ClearButtonEnabled = true,
            CustomMinimumSize = new Vector2(300, 0),
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
        };
        _search.TextChanged += _ => ApplyFilters();
        filters.AddChild(_search);

        _kindFilter = new OptionButton
        {
            Name = "DictionaryKindFilter", CustomMinimumSize = new Vector2(112, 0)
        };
        foreach (string kind in new[] { "全部种类", "技能", "宝具", "礼装" })
            _kindFilter.AddItem(kind);
        _kindFilter.ItemSelected += _ => OnKindChanged();
        filters.AddChild(_kindFilter);

        _focusFilter = new OptionButton
        {
            Name = "DictionaryFocusFilter", CustomMinimumSize = new Vector2(124, 0)
        };
        foreach (string focus in FocusNames) _focusFilter.AddItem(focus);
        _focusFilter.ItemSelected += _ => ApplyFilters();
        filters.AddChild(_focusFilter);

        _resultCount = new Label
        {
            Name = "DictionaryResultCount",
            ThemeTypeVariation = "Muted",
            CustomMinimumSize = new Vector2(90, 0),
            HorizontalAlignment = HorizontalAlignment.Right,
        };
        filters.AddChild(_resultCount);

        var split = new HSplitContainer
        {
            Name = "DictionarySplit",
            SplitOffsets = new[] { 360 },
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        page.AddChild(split);

        _resourceList = new ItemList
        {
            Name = "DictionaryResourceList",
            CustomMinimumSize = new Vector2(300, 0),
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
            SelectMode = ItemList.SelectModeEnum.Single,
        };
        _resourceList.ItemSelected += index => ShowDetail((int)index);
        split.AddChild(_resourceList);

        var detailPanel = new PanelContainer
        {
            Name = "DictionaryDetailPanel",
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        split.AddChild(detailPanel);
        _detail = new RichTextLabel
        {
            Name = "DictionaryDetail",
            FitContent = false,
            BbcodeEnabled = false,
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
            Text = "请从左侧选择一条资源。",
        };
        detailPanel.AddChild(_detail);
        return page;
    }

    private void OnKindChanged()
    {
        bool focusAllowed = _kindFilter.Selected is 0 or 2;
        _focusFilter.Disabled = !focusAllowed;
        if (!focusAllowed) _focusFilter.Select(0);
        ApplyFilters();
    }

    private void ApplyFilters()
    {
        if (_resourceList == null) return;
        string keyword = _search.Text.Trim();
        ResKind? kind = _kindFilter.Selected switch
        {
            1 => ResKind.Skill,
            2 => ResKind.Np,
            3 => ResKind.Item,
            _ => null,
        };
        int focus = _focusFilter.Selected;

        _results = new List<ResourceDef>();
        foreach (ResourceDef resource in Data.ResourceDb.All)
        {
            if (kind.HasValue && resource.Kind != kind.Value) continue;
            if (focus > 0 && (resource.Kind != ResKind.Np || resource.Focus != focus)) continue;
            if (!Matches(resource, keyword)) continue;
            _results.Add(resource);
        }

        _resourceList.Clear();
        foreach (ResourceDef resource in _results)
        {
            string extra = resource.Kind == ResKind.Np
                ? $" · {CardBuildRules.FocusName(resource.Focus)}"
                : "";
            _resourceList.AddItem($"#{resource.Id}　{resource.Name} [{RankUtil.Name(resource.Rank)}] · {resource.KindName}{extra}");
        }

        _resultCount.Text = $"{_results.Count} / {Data.ResourceDb.All.Count} 条";
        if (_results.Count > 0)
        {
            _resourceList.Select(0);
            ShowDetail(0);
        }
        else
        {
            _detail.Text = "没有符合当前搜索与筛选条件的资源。";
        }
    }

    private static bool Matches(ResourceDef resource, string keyword)
    {
        if (string.IsNullOrEmpty(keyword)) return true;
        return resource.Name.Contains(keyword, StringComparison.OrdinalIgnoreCase) ||
               resource.Text.Contains(keyword, StringComparison.OrdinalIgnoreCase) ||
               resource.Id.ToString().Contains(keyword, StringComparison.Ordinal);
    }

    private void ShowDetail(int index)
    {
        if (index < 0 || index >= _results.Count) return;
        ResourceDef resource = _results[index];
        var text = new StringBuilder();
        text.AppendLine($"{resource.Name}　[{RankUtil.Name(resource.Rank)}]");
        text.AppendLine($"资源 ID：{resource.Id}");
        text.AppendLine($"种类：{resource.KindName}");
        text.AppendLine($"类型：{TypeName(resource)}");
        text.AppendLine($"面向：{(resource.Kind == ResKind.Np ? CardBuildRules.FocusName(resource.Focus) : "不适用")}");
        text.AppendLine($"时机：{WhenName(resource.When)}");
        text.AppendLine($"魔耗：{resource.Cost}　回转：{resource.Recast}　每轮次数：{DisplayLimit(resource.CountPerRound)}");
        text.AppendLine($"储备：{resource.Reserve}/{resource.ReserveMax}　唯一：{(resource.Unique != 0 ? "是" : "否")}");
        text.AppendLine($"特性：{FeatNames(resource.Feat)}");
        text.AppendLine();
        text.AppendLine($"效果摘要（{resource.Effects.Count} 条）");
        for (int effectIndex = 0; effectIndex < resource.Effects.Count; effectIndex++)
            text.AppendLine($"{effectIndex + 1}. {EffectSummary(resource.Effects[effectIndex])}");
        text.AppendLine();
        text.AppendLine("资源原文");
        text.AppendLine(string.IsNullOrWhiteSpace(resource.Text) ? "（无原文）" : resource.Text);
        _detail.Text = text.ToString();
    }

    private static string TypeName(ResourceDef resource)
    {
        if (resource.Kind == ResKind.Np)
        {
            return (NpType)resource.Type switch
            {
                NpType.Human => "对人", NpType.Army => "对军", NpType.Castle => "对城",
                NpType.World => "对界", NpType.Bound => "结界", _ => "无类型",
            };
        }
        return (SkillType)resource.Type switch
        {
            SkillType.Class => "职阶", SkillType.Talent => "天赋", SkillType.Technique => "技术",
            SkillType.Bless => "加护", SkillType.Crown => "冠位", SkillType.Weapon => "武技",
            SkillType.Magic => "魔术", _ => "未分类",
        };
    }

    private static string WhenName(When when) => when switch
    {
        When.Passive => "被动", When.Act => "主动", When.BattleStart => "战斗开始",
        When.Proc => "工序", When.Any => "任意", _ => when.ToString(),
    };

    private static string FeatNames(int value)
    {
        if (value == 0) return "无";
        var names = new List<string>();
        foreach ((Feat flag, string name) in new[]
                 {
                     (Feat.Main, "主力位"), (Feat.Support, "辅助位"), (Feat.Servant, "仆役位"),
                     (Feat.Rear, "支援位"), (Feat.Assist, "支援"), (Feat.Counter, "反击"),
                     (Feat.Ride, "骑乘"), (Feat.ChargeReady, "蓄力"), (Feat.Pierce, "必中"),
                     (Feat.InvPierce, "无敌贯通"), (Feat.Energy, "爆发"), (Feat.Unique, "唯一"),
                 })
            if ((value & (int)flag) != 0) names.Add(name);
        return names.Count == 0 ? $"未知({value})" : string.Join("、", names);
    }

    private static string EffectSummary(EffectLine effect)
    {
        var parts = new List<string>();
        string name = EffectName(effect.Flag);
        parts.Add(name);
        // 数值
        if (effect.Value != 0)
            parts.Add($"数值 {Signed(effect.Value)}");
        // 属性
        if (effect.Attr >= 0 && effect.Attr < 7 && (effect.Flag == EffFlag.AttrUp || effect.Flag == EffFlag.AttrDown ||
            effect.Flag == EffFlag.AttrUpConst || effect.Flag == EffFlag.AttrDownConst))
            parts.Add($"属性 {AttrCn(effect.Attr)}");
        // 状态
        if (effect.Status != 0 && (effect.Flag == EffFlag.StatusGive || effect.Flag == EffFlag.StatusRemove ||
            effect.Flag == EffFlag.StateRes || effect.Flag == EffFlag.StateIm || effect.Flag == EffFlag.EffectIm))
            parts.Add($"状态 {StatusUtil.Name((StatusKind)effect.Status)}");
        // 层数
        if (effect.Layers != 0)
            parts.Add($"层数 {effect.Layers}");
        // 目标
        parts.Add($"目标 {TargetName(effect.Target)}");
        // 次数
        if (effect.Times > 1)
            parts.Add($"×{effect.Times}");
        // 判定
        var chanceParts = new List<string>();
        if (effect.Chance > 0)
            chanceParts.Add($"{effect.Chance}%");
        else if (effect.ChanceAttrBase >= 0 && effect.ChanceAttrBase < 7)
            chanceParts.Add($"以{AttrCn(effect.ChanceAttrBase)}判定");
        if (effect.ChanceNeg)
            chanceParts.Add("负面判定");
        if (effect.LuckHalve)
            chanceParts.Add("目标幸运≥40成功率减半");
        if (chanceParts.Count > 0)
            parts.Add($"判定[{string.Join(" ", chanceParts)}]");
        // 条件
        if (effect.Cond != Cond.None)
            parts.Add($"条件[{CondCn(effect.Cond, effect.CondArg, effect.CondArg2)}]");
        // 上限
        if (effect.Cap > 0)
            parts.Add($"上限{effect.Cap}");
        // 原文补充描述(若有效果名以外的机制说明)
        if (!string.IsNullOrWhiteSpace(effect.Desc) && !effect.Desc.StartsWith(name))
            parts.Add($"（{effect.Desc}）");
        return string.Join(" ", parts);
    }

    private static string Signed(int v) => v > 0 ? $"+{v}" : v.ToString();

    private static string AttrCn(int attr) => attr switch
    {
        0 => "筋力", 1 => "耐久", 2 => "敏捷", 3 => "魔力", 4 => "幸运", 5 => "宝具", 6 => "回路", _ => "属性",
    };

    private static string CondCn(Cond cond, int arg, int arg2)
    {
        return cond switch
        {
            Cond.OwnMain => "自身在主力位",
            Cond.OwnSupport => "自身在辅助位",
            Cond.OwnRear => "自身在支援位",
            Cond.TargetTrait => $"目标持有特性{arg}",
            Cond.TargetNotTrait => $"目标不持特性{arg}",
            Cond.TargetStatusEq => $"目标持有状态{StatusUtil.Name((StatusKind)arg)}",
            Cond.TargetStatusGe => $"目标状态{StatusUtil.Name((StatusKind)arg)}≥{arg2}",
            Cond.StatusNot => $"目标不持状态{StatusUtil.Name((StatusKind)arg)}",
            Cond.TargetLuckGe => $"目标幸运≥{arg}",
            Cond.TargetLevelGe => $"目标等级≥{arg}",
            Cond.LevelDiff => $"自身等级-目标≥{arg}",
            Cond.Day => "昼间",
            Cond.Night => "夜间",
            Cond.SelfTrait => $"自身持有特性{arg}",
            Cond.SelfIsMaster => "自身为御主",
            Cond.SelfIsServant => "自身为从者",
            Cond.SelfMp => $"自身魔力≥{arg}",
            Cond.SelfHp => $"自身{AttrCn(arg)}≥{arg2}",
            Cond.HasCs => "持有令咒",
            Cond.FriendInBattle => "己方有其他单位参战",
            Cond.EnemyMainMaster => "敌方主力为御主",
            Cond.EnemyIsServant => "敌方主力为从者",
            Cond.EnemyIsSummon => "敌方主力为召唤物",
            Cond.MpUnder => $"自身魔力<{arg}",
            Cond.TargetAgiLt => $"目标敏捷<{arg}",
            Cond.TargetAgiGe => $"目标敏捷≥{arg}",
            Cond.EnemyTactic => $"敌方战术={arg}",
            Cond.SelfTactic => $"自身战术={arg}",
            Cond.TacticNotPaired => "己方战术未被克制",
            Cond.FirstEncounter => "与敌方主力初次同场战斗",
            _ => cond.ToString(),
        };
    }

    private static string EffectName(EffFlag flag) => flag switch
    {
        EffFlag.AttrUp => "属性上升", EffFlag.AttrDown => "属性下降",
        EffFlag.AttrUpConst => "属性常驻上升", EffFlag.AttrDownConst => "属性常驻下降",
        EffFlag.WinUp => "胜率上升", EffFlag.WinDown => "胜率下降",
        EffFlag.FinalWinUp => "最终胜率上升", EffFlag.FinalWinDown => "最终胜率下降",
        EffFlag.FloorUp => "底限胜率上升", EffFlag.FloorPen => "底限胜率穿透",
        EffFlag.HitUp => "判定成功率上升", EffFlag.HitFinalUp => "判定最终成功率上升",
        EffFlag.HitPen => "判定成功率下降",
        EffFlag.ResUp => "抗性上升", EffFlag.ResDown => "抗性下降",
        EffFlag.StateRes => "状态抵抗", EffFlag.StateIm => "状态免疫", EffFlag.EffectIm => "效果免疫",
        EffFlag.ManaUp => "魔力增加", EffFlag.ManaDown => "魔力减少",
        EffFlag.StatusGive => "赋予状态", EffFlag.StatusRemove => "移除状态",
        EffFlag.Recast => "回转增加", EffFlag.RecastLose => "回转减少",
        EffFlag.FpUp => "脱离值增加", EffFlag.FpDown => "脱离值减少",
        EffFlag.TpFp => "战斗中脱离值增加",
        EffFlag.Death => "即死", EffFlag.BoundDeath => "限定即死(轰击)",
        EffFlag.Pierce => "必中", EffFlag.InvPierce => "无敌贯通",
        EffFlag.Evade => "赋予回避", EffFlag.Protect => "赋予保护", EffFlag.Invincible => "赋予无敌",
        EffFlag.Retaliate => "反击", EffFlag.Info => "情报", EffFlag.Cs => "令咒",
        EffFlag.BurnBlow => "爆燃(灼伤结算)", EffFlag.ElectricBlow => "激荡(感电结算)",
        EffFlag.PoisonBlow => "毒发(中毒结算)",
        EffFlag.Summon => "召唤", EffFlag.Other => "其他",
        _ => flag == EffFlag.None ? "无" : flag.ToString(),
    };

    private static string TargetName(int target) => target switch
    {
        -2 => "自身", -1 => "己方全体", 0 => "敌方全体",
        > 0 => $"敌方第{target}位", _ => "目标",
    };

    private static string DisplayLimit(int value) => value <= 0 ? "不限" : value.ToString();

    private const string StatusDictionaryText = """
── 强化状态 ──
[回避]    令自身受到的下一个(非自身来源)技能/宝具效果无效化
[无敌]    不受来源自身外的任意效果影响,战斗/回合结束时移除
[必中]    令目标的[回避]无效化
[无敌贯通] 无视[无敌]效果
[保护]    将对他人的效果转移到自身,被保护者无法再保护其他单位
[抗性上升] 以自身为目标的负面判定最终成功率惩罚
[状态抵抗] 对应状态判定最终成功率减半,至少-20%
[状态免疫] 对应状态判定默认失败,赋予时直接免除并清除已有

── 弱化状态 ──
[疲惫]    除宝具外全属性-5*层;5层以上时即死判定风险
[残废]    每回合开始50%即死判定,失败需令咒否则退场
[迟滞]    非职阶技能发动结算延迟到下一工序;每工序-1层
[诅咒]    受到的负面判定成功率+5*层%;至多20层
[封印]    指定技能/宝具无效化;战斗结束或回合结束-1层
[技能封印] 无法发动技能;每次战斗/回合结束-1层
[宝具封印] 无法解放宝具;每次战斗/回合结束-1层
[抗性下降] 自身发起的负面判定获得最终成功率补正

── 异常状态 ──
[中毒]    每回合开始20*层%负面判定,成功则筋/耐/敏随机-10;层数-1
[灼伤]    每回合开始层数*20%负面判定,成功则四属性随机-10;层数-1
[冻结]    筋/耐/敏-5*层;每回合开始层数-1
[感电]    轮次结束时-5*层魔力后清除;至多20层
[石化]    筋/耐/敏属性补正-60;无法改变随机属性
[晕眩]    无法发动非职阶技能;每工序开始层数*20%判定,失败解除
[魅惑]    同战斗位+5%*层胜率,对立-5%*层;3层属性+10;6层无法袭击;9层无法进入对立位
[混乱]    单体目标随机选取;层数每次生效-1;回合结束清除
[恐惧]    发动非职阶技能时50%判定失败;被重复赋予转为[混乱1]
[抗性破除] 使对应状态抵抗/免疫/抗性上升对本次判定无效化
[特性赋予] 获得指定特性

── 特效词条 ──
[主力位/辅助位/仆役位/支援位] 仅在该战斗位才能发动/生效
[支援]   可在主力位/辅助位/支援位发动(不同于[支援位])
[反击]   在指定效果生效前优先结算
[蓄力]   宣言后延迟到指定工序/经过一定工序后生效
[爆发]   额外支付一份发动条件令效果再次生效一次
[骑乘]   允许[机动]时同灵脉单位[协助];允许[冲锋]
[必中]/[无敌贯通] 穿透[回避]/[无敌]
""";
}
