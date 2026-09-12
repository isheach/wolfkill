using Godot;
using System;
using System.Collections.Generic;
using KsgGodot.Engine;
using KsgGodot.Model;
using KsgGodot.Save;

namespace KsgGodot.UI;

/// <summary>
/// 大航海战斗表(结算流程展示 / GM 计算器)。
/// 按表内工序分栏: ①初始工序(参战人选/魔力/扣减/保底) ②主要工序(属性总值/战斗属性/优劣)
/// ③最终工序(基础胜率/胜率链/差值/最终胜率); 支持双方(1v1)与三方混战。
/// </summary>
public partial class SeaTableScene : Control
{
    private const int MaxAux = SeaSettle.MaxAux;
    private static readonly string[] AttrNames = SeaSettle.AttrNames;

    private OptionButton _mode;
    private readonly OptionButton[] _pick = new OptionButton[4];
    private Label _pickLabel4;
    private Button _roll;
    private CheckBox _auxNormal;
    private HBoxContainer _sidesBox;
    private RichTextLabel _output;
    private Label _hint;

    private readonly List<SideUI> _sides = new();
    private List<UnitDef> _units = new();

    /// <summary>单方 UI 控件集合。</summary>
    private class SideUI
    {
        public string Title = "";
        public PanelContainer Panel;
        public readonly OptionButton[] Unit = new OptionButton[MaxAux + 1];
        public readonly SpinBox[] Spend = new SpinBox[MaxAux + 1];
        public readonly SpinBox[] Bias = new SpinBox[4];
        public SpinBox Gift, Command, KeepBase, Floor;
        public SpinBox PreWin, PreLoss, InitW, InitL, MainW, MainL, Extra1, Extra2;
        public CheckBox SoloChk, PioneerChk;
    }

    public override void _Ready()
    {
        AppTheme.Apply(this);
        Data.ResourceDb.Load();
        SaveManager.InitializePortable();
        SaveManager.LoadAllIntoSessionOnce();
        _units = new List<UnitDef>(CurrentSave.All);

        var root = new VBoxContainer { AnchorRight = 1, AnchorBottom = 1 };
        root.AddThemeConstantOverride("separation", 10);
        AddChild(root);
        root.AddChild(AppTheme.MakeHeader("大航海战斗表 · 结算流程", () => SceneRouter.GoMainMenu(this)));

        // ---- 控制条 ----
        var ctrl = new HBoxContainer();
        ctrl.AddChild(new Label { Text = "模式" });
        _mode = new OptionButton { CustomMinimumSize = new Vector2(150, 0) };
        _mode.AddItem("双方(1v1)");
        _mode.AddItem("三方混战");
        _mode.ItemSelected += _ => Rebuild();
        ctrl.AddChild(_mode);

        ctrl.AddChild(new Label { Text = "   对抗属性" });
        for (int p = 0; p < 4; p++)
        {
            if (p > 0) ctrl.AddChild(new Label { Text = "/" });
            var pick = new OptionButton { CustomMinimumSize = new Vector2(96, 0) };
            foreach (string n in AttrNames) pick.AddItem(n);
            pick.Selected = p;                       // 默认 筋力/耐久/敏捷[/魔力]
            _pick[p] = pick;
            ctrl.AddChild(pick);
        }
        _pickLabel4 = new Label { Text = "(第4项=随机属性)", ThemeTypeVariation = "Muted" };
        ctrl.AddChild(_pickLabel4);

        _roll = new Button { Text = "🎲 掷随机属性" };
        _roll.Pressed += () => { _pick[3].Selected = Dice.Next(6); UpdatePickVisibility(); };
        ctrl.AddChild(_roll);

        _auxNormal = new CheckBox { Text = "辅助胜率正常", ButtonPressed = true };
        _auxNormal.TooltipText = "勾选=战前/胜率补正全额求和; 取消=辅助位补正减半";
        ctrl.AddChild(_auxNormal);

        var settle = new Button { Text = "▶ 结算" };
        settle.Pressed += Settle;
        ctrl.AddChild(settle);
        root.AddChild(ctrl);

        if (_units.Count == 0)
        {
            _hint = new Label
            {
                Text = "当前会话没有单位: 请先到「建卡向导」创建卡片, 或从「已建卡单位一览」载入存档。",
                ThemeTypeVariation = "Muted",
            };
            root.AddChild(_hint);
        }

        // ---- 选人区 ----
        var sideScroll = new ScrollContainer
        {
            CustomMinimumSize = new Vector2(0, 250),
            HorizontalScrollMode = ScrollContainer.ScrollMode.Auto,
            SizeFlagsVertical = SizeFlags.ShrinkBegin,
        };
        _sidesBox = new HBoxContainer();
        sideScroll.AddChild(_sidesBox);
        root.AddChild(sideScroll);

        // ---- 结果(工序表格) ----
        _output = new RichTextLabel
        {
            BbcodeEnabled = true,
            ScrollActive = true,
            SelectionEnabled = true,
            SizeFlagsVertical = SizeFlags.ExpandFill,
            ThemeTypeVariation = "BattleLog",
        };
        root.AddChild(_output);

        Rebuild();
    }

    private bool ThreeWay => _mode != null && _mode.Selected == 1;

    private int PickCount => ThreeWay ? 4 : 3;

    private void UpdatePickVisibility()
    {
        int n = PickCount;
        for (int p = 0; p < 4; p++) _pick[p].Visible = p < n;
        _pickLabel4.Visible = ThreeWay;
        _roll.Visible = ThreeWay;
    }

    /// <summary>按模式重建选人面板(切换模式会重置填写内容)。</summary>
    private void Rebuild()
    {
        UpdatePickVisibility();
        foreach (Node child in _sidesBox.GetChildren()) child.QueueFree();
        _sides.Clear();

        int sideCount = ThreeWay ? 3 : 2;
        string[] titles = { "组别A(左方)", "组别B(右方)", "组别C(第三方)" };
        for (int i = 0; i < sideCount; i++)
        {
            SideUI ui = BuildSide(titles[i]);
            _sides.Add(ui);
            _sidesBox.AddChild(ui.Panel);
        }
        _output.Text = "[color=#aaa5b8]填写参战人选、魔力与对抗属性后点击「结算」;"
                       + "结算表按 ①初始工序 ②主要工序 ③最终工序 排列。[/color]";
    }

    private SideUI BuildSide(string title)
    {
        var ui = new SideUI { Title = title };
        var panel = new PanelContainer { CustomMinimumSize = new Vector2(ThreeWay ? 360 : 470, 0) };
        ui.Panel = panel;
        var box = new VBoxContainer();
        panel.AddChild(box);
        box.AddChild(new Label { Text = title, ThemeTypeVariation = "SectionTitle" });

        // 参战人选 + 魔力消耗
        var grid = new GridContainer { Columns = 3 };
        grid.AddChild(new Label { Text = "战斗位", ThemeTypeVariation = "Muted" });
        grid.AddChild(new Label { Text = "单位(唯一姓名)", ThemeTypeVariation = "Muted" });
        grid.AddChild(new Label { Text = "魔力消耗", ThemeTypeVariation = "Muted" });
        for (int i = 0; i <= MaxAux; i++)
        {
            bool main = i == 0;
            grid.AddChild(new Label
            {
                Text = main ? "主力位" : "辅助位" + i,
                ThemeTypeVariation = main ? "Label" : "Muted",
            });
            var picker = new OptionButton { SizeFlagsHorizontal = SizeFlags.ExpandFill };
            picker.AddItem("(无)");
            foreach (UnitDef u in _units) picker.AddItem(UnitLabel(u));
            picker.Selected = 0;
            ui.Unit[i] = picker;
            grid.AddChild(picker);
            ui.Spend[i] = MakeSpin(0);
            grid.AddChild(ui.Spend[i]);
        }
        box.AddChild(grid);

        // 魔力补充 / 状态
        var mp = new GridContainer { Columns = 4 };
        mp.AddChild(new Label { Text = "礼装补充", ThemeTypeVariation = "Muted" });
        ui.Gift = MakeSpin(0);
        mp.AddChild(ui.Gift);
        mp.AddChild(new Label { Text = "令咒补充", ThemeTypeVariation = "Muted" });
        ui.Command = MakeSpin(0);
        mp.AddChild(ui.Command);
        box.AddChild(mp);

        var chk = new HBoxContainer();
        ui.SoloChk = new CheckBox { Text = "单独行动(扣减减半)" };
        chk.AddChild(ui.SoloChk);
        if (ThreeWay)
        {
            ui.PioneerChk = new CheckBox { Text = "星之开拓者" };
            chk.AddChild(ui.PioneerChk);
        }
        box.AddChild(chk);

        // 胜率补正各项
        var rates = new GridContainer { Columns = 4 };
        AddRateRow(rates, "战前胜补", out ui.PreWin, "战前胜惩", out ui.PreLoss);
        AddRateRow(rates, "初始胜补", out ui.InitW, "初始胜惩", out ui.InitL);
        AddRateRow(rates, "主要胜补", out ui.MainW, "主要胜惩", out ui.MainL);
        AddRateRow(rates, "补充胜率1", out ui.Extra1, "补充胜率2", out ui.Extra2);
        AddRateRow(rates, "底限胜率", out ui.Floor, ThreeWay ? "保有胜率基准" : "—", out ui.KeepBase);
        box.AddChild(rates);

        // 手动优劣调整(每点 = ±10 战力)
        var bias = new HBoxContainer();
        bias.AddChild(new Label { Text = "手动优劣", ThemeTypeVariation = "Muted" });
        for (int p = 0; p < 4; p++)
        {
            ui.Bias[p] = MakeSpin(0, -9, 9);
            ui.Bias[p].CustomMinimumSize = new Vector2(70, 0);
            bias.AddChild(ui.Bias[p]);
        }
        bias.AddChild(new Label { Text = "每点±10战力", ThemeTypeVariation = "Muted" });
        box.AddChild(bias);
        return ui;
    }

    private static void AddRateRow(GridContainer grid, string l1, out SpinBox s1, string l2, out SpinBox s2)
    {
        grid.AddChild(new Label { Text = l1, ThemeTypeVariation = "Muted" });
        s1 = MakeSpin(0);
        grid.AddChild(s1);
        grid.AddChild(new Label { Text = l2, ThemeTypeVariation = "Muted" });
        s2 = MakeSpin(0);
        grid.AddChild(s2);
    }

    private static SpinBox MakeSpin(double value, double min = -999, double max = 999)
    {
        return new SpinBox
        {
            MinValue = min,
            MaxValue = max,
            Step = 1,
            Value = value,
            CustomMinimumSize = new Vector2(96, 0),
            Alignment = HorizontalAlignment.Right,
        };
    }

    private static string UnitLabel(UnitDef u)
    {
        string kind = u.UType switch { 1 => "从者", 2 => "御主", 3 => "召唤物", 4 => "人偶", _ => "使魔" };
        return $"{u.Name}[{kind} Lv{u.Level}]";
    }

    // ==================== 结算 ====================

    private void Settle()
    {
        int sideCount = ThreeWay ? 3 : 2;
        var sides = new List<SeaSettle.Side>();
        foreach (SideUI ui in _sides)
        {
            var side = new SeaSettle.Side
            {
                Name = ui.Title,
                MpGift = (int)ui.Gift.Value,
                MpCommand = (int)ui.Command.Value,
                SoloAction = ui.SoloChk.ButtonPressed,
                AuxRateNormal = _auxNormal.ButtonPressed,
                WinBonus = (int)ui.InitW.Value,
                WinPenalty = (int)ui.InitL.Value,
                PickWin = (int)ui.MainW.Value,
                PickLoss = (int)ui.MainL.Value,
                Extra1 = (int)ui.Extra1.Value,
                Extra2 = (int)ui.Extra2.Value,
                FloorRate = (int)ui.Floor.Value,
                StarPioneer = ui.PioneerChk != null && ui.PioneerChk.ButtonPressed,
                KeepBase = ThreeWay ? (int)ui.KeepBase.Value : 0,
            };
            for (int i = 0; i <= MaxAux; i++)
            {
                int idx = ui.Unit[i].Selected - 1;
                if (idx < 0 || idx >= _units.Count)
                {
                    if (i == 0) side.Members.Add(new SeaSettle.Member { Unit = null, Slot = 0 });
                    continue;
                }
                side.Members.Add(new SeaSettle.Member
                {
                    Unit = _units[idx],
                    Slot = i,
                    Spend = (int)ui.Spend[i].Value,
                });
            }
            sides.Add(side);
        }

        if (sides[0].Members.Count == 0 || sides[1].Members.Count == 0 ||
            sides[0].Main?.Unit == null || sides[1].Main?.Unit == null)
        {
            _output.Text = "[color=#ffb0bc]双方主力位都必须选择单位。[/color]";
            return;
        }
        if (ThreeWay && sides[2].Main?.Unit == null)
        {
            _output.Text = "[color=#ffb0bc]三方混战需要三个组别都选择主力。[/color]";
            return;
        }

        var cfg = new SeaSettle.Picks(sideCount) { Attr = new int[PickCount] };
        for (int p = 0; p < PickCount; p++) cfg.Attr[p] = _pick[p].Selected;
        for (int s = 0; s < sideCount; s++)
            for (int p = 0; p < PickCount; p++)
                cfg.Bias[s][p] = (int)_sides[s].Bias[p].Value;

        var text = new System.Text.StringBuilder();
        if (!ThreeWay)
        {
            (SeaSettle.SideResult ra, SeaSettle.SideResult rb) = SeaSettle.Settle2(sides[0], sides[1], cfg);
            text.Append(Summary(sides[0].Name, ra.FinalRate, ra.HalfRate));
            text.Append(Summary(sides[1].Name, rb.FinalRate, rb.HalfRate));
            text.Append("\n");
            AppendRows(text, SeaSettle.Report2(ra, rb, cfg));
        }
        else
        {
            List<SeaSettle.SideResult> rs = SeaSettle.Settle3(new[] { sides[0], sides[1], sides[2] }, cfg);
            foreach (SeaSettle.SideResult r in rs)
                text.Append(Summary(r.S.Name, r.FinalRate, r.HalfRate, $"保有{r.KeepRate:0.#}/150"));
            text.Append("\n");
            AppendRows(text, SeaSettle.Report3(rs, cfg));
        }
        _output.Text = text.ToString();
    }

    private static string Summary(string name, int rate, int half, string extra = "")
    {
        return $"[b][color=#f4e6bd]{name}[/color][/b] 最终胜率 [b]{rate}%[/b]"
               + (extra.Length > 0 ? $" ({extra})" : "")
               + $"  ·  差值减半版 {half}%\n";
    }

    /// <summary>把工序行渲染成 bbcode 表格(每个工序一张表)。</summary>
    private static void AppendRows(System.Text.StringBuilder sb, List<SeaSettle.Row> rows)
    {
        string phase = "";
        int cols = 0;
        foreach (SeaSettle.Row row in rows)
        {
            if (row.Phase != phase)
            {
                if (cols > 0) sb.Append("[/table]\n");
                phase = row.Phase;
                cols = Math.Max(2, row.Cells.Length);
                sb.Append($"\n[b][color=#dcc58d]{phase}[/color][/b]\n");
                sb.Append($"[table={cols}]");
            }
            foreach (string cell in row.Cells)
            {
                string body = string.IsNullOrEmpty(cell) ? " " : cell;
                sb.Append(row.Head ? $"[cell][b]{body}[/b][/cell]" : $"[cell]{body}[/cell]");
            }
        }
        if (cols > 0) sb.Append("[/table]\n");
    }
}
