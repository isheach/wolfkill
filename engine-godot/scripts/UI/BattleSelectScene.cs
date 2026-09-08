using System.Collections.Generic;
using Godot;

namespace KsgGodot.UI;

/// <summary>可单击切换的 ItemList: 使用 Godot 原生 Toggle 选择模式(单击切换选中),
/// 在原生处理结束后延迟广播 SelectionChanged 以便刷新计数(ItemList 的选择变更在输入派发后才落定)。</summary>
public partial class ToggleSelectList : ItemList
{
    /// <summary>选中集合变化后触发(切换/清空/原生多选)。</summary>
    public event System.Action SelectionChanged;

    public override void _GuiInput(InputEvent ev)
    {
        base._GuiInput(ev);
        CallDeferred(nameof(EmitSelectionChanged));
    }

    private void EmitSelectionChanged() => SelectionChanged?.Invoke();
}

/// <summary>对战选择:从已建卡中选择左右两方,开始战斗</summary>
public partial class BattleSelectScene : Control
{
    private ItemList _left, _right;
    private Label _info;
    private Label _leftCount, _rightCount;
    private HSlider _widthSlider;
    private Label _widthDesc;
    private OptionButton _scenePreset;

    /// <summary>场景预设: 名称 → 战场宽度(规则书 3.2)。</summary>
    private static readonly (string Name, int Width)[] ScenePresets =
    {
        ("自定义(滑条)", 4),
        ("空灵脉 / 荒野", 1),
        ("窄巷 / 小道", 2),
        ("街道", 3),
        ("大道(默认)", 4),
        ("广场 / 主街", 5),
        ("灵脉大路", 6),
        ("固有结界: 灼砂大地", 7),
    };

    public override void _Ready()
    {
        AppTheme.Apply(this);
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
            Name = "BattleSelectContent",
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        root.AddThemeConstantOverride("separation", 8);
        margin.AddChild(root);

        root.AddChild(AppTheme.MakeHeader(
            "对战选择",
            () => SceneRouter.GoMainMenu(this)));

        _info = new Label { Text = "先在[建卡向导]建卡或从[单位一览]载入存档,再进行对战", AutowrapMode = TextServer.AutowrapMode.WordSmart };
        root.AddChild(_info);

        var pickGrid = new GridContainer { Name = "BattlePicks", Columns = 2 };
        root.AddChild(pickGrid);
        pickGrid.AddChild(new Label { Text = "左方(单击切换,可多选):", CustomMinimumSize = new Vector2(64, 0) });
        _left = new ToggleSelectList
        {
            Name = "LeftUnit",
            CustomMinimumSize = new Vector2(420, 168),
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            // Toggle: 单击切换所选卡片的选中状态,多选无需 Ctrl/Shift
            SelectMode = ItemList.SelectModeEnum.Toggle,
        };
        pickGrid.AddChild(_left);
        pickGrid.AddChild(new Label { Text = "右方(单击切换,可多选):", CustomMinimumSize = new Vector2(64, 0) });
        _right = new ToggleSelectList
        {
            Name = "RightUnit",
            CustomMinimumSize = new Vector2(420, 168),
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SelectMode = ItemList.SelectModeEnum.Toggle,
        };
        pickGrid.AddChild(_right);

        _leftCount = new Label { Name = "LeftCount", Text = "已选 0 张", ThemeTypeVariation = "Muted" };
        _rightCount = new Label { Name = "RightCount", Text = "已选 0 张", ThemeTypeVariation = "Muted" };
        pickGrid.AddChild(_leftCount);
        pickGrid.AddChild(_rightCount);
        // 普通单击切换选中(追加/移除), 而不是替换整个选择 —— 便于直观多选
        SetupTogglePick(_left, _leftCount);
        SetupTogglePick(_right, _rightCount);

        var widthRow = new HBoxContainer();
        root.AddChild(widthRow);
        widthRow.AddChild(new Label { Text = "战场宽度:", CustomMinimumSize = new Vector2(72, 0) });
        _scenePreset = new OptionButton { Name = "ScenePreset", CustomMinimumSize = new Vector2(240, 0) };
        foreach ((string name, _) in ScenePresets) _scenePreset.AddItem(name);
        _scenePreset.ItemSelected += _ => ApplyScenePreset();
        widthRow.AddChild(_scenePreset);
        _widthSlider = new HSlider
        {
            Name = "WidthSlider", MinValue = 1, MaxValue = 7, Step = 1, Value = 4,
            CustomMinimumSize = new Vector2(200, 0),
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
        };
        _widthSlider.ValueChanged += _ => OnWidthChanged();
        widthRow.AddChild(_widthSlider);
        _widthDesc = new Label
        {
            Name = "WidthDescription",
            CustomMinimumSize = new Vector2(280, 0),
            Text = Engine.KsgBattle.WidthDescription(4),
        };
        widthRow.AddChild(_widthDesc);

        root.AddChild(new Label
        {
            Text = "提示: 每方人数上限 = 战场宽度容量(宽度越大战斗位越多);超过容量时,多余单位在战斗开始时被移出战斗位。"
                + "战斗位构成(规则书 3.2): 主力位属性全额、辅助位/仆役位减半、支援位不计入战斗属性。",
            ThemeTypeVariation = "Muted",
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
        });

        var btnRow = new HBoxContainer();
        root.AddChild(btnRow);
        var start = new Button { Text = "开始对战", CustomMinimumSize = new Vector2(220, 48) };
        start.Pressed += StartBattle;
        btnRow.AddChild(start);

        RefreshUnits();
    }

    /// <summary>场景预设联动宽度滑条与说明。</summary>
    private void ApplyScenePreset()
    {
        int idx = _scenePreset.Selected;
        if (idx < 0 || idx >= ScenePresets.Length) return;
        _widthSlider.SetValueNoSignal(ScenePresets[idx].Width);
        OnWidthChanged();
    }

    /// <summary>普通单击切换选中(追加/移除),刷新计数; 同时保持 Ctrl/Shift 原生多选手势。</summary>
    private void SetupTogglePick(ItemList list, Label countLabel)
    {
        if (list is ToggleSelectList t) t.SelectionChanged += () => UpdatePickCount(list, countLabel);
        list.ItemSelected += _ => UpdatePickCount(list, countLabel);
    }

    private void UpdatePickCount(ItemList list, Label countLabel)
    {
        if (countLabel == null) return;
        int n = list.GetSelectedItems().Length;
        int cap = Engine.KsgBattle.WidthCapacity((int)_widthSlider.Value);
        countLabel.Text = n == 0
            ? "已选 0 张"
            : n > cap
                ? $"已选 {n} 张(超出容量 {cap},多余单位开战时移出)"
                : $"已选 {n} 张 / 容量 {cap}";
    }

    private void OnWidthChanged()
    {
        int w = (int)_widthSlider.Value;
        _widthDesc.Text = Engine.KsgBattle.WidthDescription(w) +
                          $"　(容量 {Engine.KsgBattle.WidthCapacity(w)} 位)";
        UpdatePickCount(_left, _leftCount);
        UpdatePickCount(_right, _rightCount);
    }

    private void RefreshUnits()
    {
        _left.Clear();
        _right.Clear();
        var all = Save.CurrentSave.All;
        if (all.Count == 0)
        {
            _info.Text = "暂无已建卡。请先在[建卡向导]建卡并保存,或在[单位一览]载入存档。";
            return;
        }
        foreach (var u in all)
        {
            string label = $"#{u.Id} {u.Name}({u.TrueName}) Lv{u.Level}";
            _left.AddItem(label);
            _right.AddItem(label);
        }
        if (all.Count > 1)
        {
            _right.Select(1);
            _left.Select(0);
        }
        else _left.Select(0);
        // 初始默认选择后刷新计数
        UpdatePickCount(_left, _leftCount);
        UpdatePickCount(_right, _rightCount);
    }

    private void StartBattle()
    {
        var all = Save.CurrentSave.All;
        var leftSel = _left.GetSelectedItems();
        var rightSel = _right.GetSelectedItems();
        if (leftSel.Length == 0 || rightSel.Length == 0)
        {
            _info.Text = "两边都要至少选择一名单位!";
            return;
        }
        var leftSide = new List<Model.UnitDef>();
        var rightSide = new List<Model.UnitDef>();
        foreach (int s in leftSel)
        {
            if (s >= 0 && s < all.Count) leftSide.Add(all[s]);
        }
        foreach (int s in rightSel)
        {
            if (s >= 0 && s < all.Count) rightSide.Add(all[s]);
        }
        // 同卡不能同时出现在两边
        foreach (var l in leftSide)
            if (rightSide.Contains(l))
            {
                _info.Text = $"「{l.Name}」不能同时出现在左右两边!";
                return;
            }

        SceneRouter.GoBattleParty(this, leftSide, rightSide, (int)_widthSlider.Value);
    }
}
