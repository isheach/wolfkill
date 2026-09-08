using Godot;
using KsgGodot.Building;
using KsgGodot.Model;
using System;
using System.Collections.Generic;
using System.Text;

namespace KsgGodot.UI;

/// <summary>已建卡单位一览：会话/存档列表、完整卡面详情与确认式存档操作。</summary>
public partial class UnitListScene : Control
{
    private static readonly string[] ClassNames =
    {
        "无职阶", "Saber", "Lancer", "Archer", "Rider", "Caster",
        "Assassin", "Berserker", "Ruler", "Avenger", "Shielder"
    };
    private static readonly string[] HiddenNames = { "天", "地", "人", "星", "兽" };

    private ItemList _sessionList;
    private ItemList _diskList;
    private RichTextLabel _detail;
    private TextureRect _detailAvatar;
    private Label _info;
    private Button _saveSessionButton;
    private Button _removeSessionButton;
    private Button _loadDiskButton;
    private Button _deleteDiskButton;
    private ConfirmationDialog _confirmDialog;
    private Action _pendingConfirmedAction;
    private List<UnitDef> _sessionUnits = new();
    private List<UnitDef> _diskUnits = new();
    private UnitDef _selected;
    private bool _selectedFromDisk;

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
            Name = "UnitListContent",
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        margin.AddChild(root);

        var header = AppTheme.MakeHeader("已建卡单位", () => SceneRouter.GoMainMenu(this));
        root.AddChild(header);
        var refresh = new Button { Name = "RefreshSaves", Text = "重新扫描存档" };
        refresh.Pressed += Refresh;
        header.AddChild(refresh);

        var split = new HSplitContainer
        {
            Name = "UnitSplit",
            SplitOffsets = new[] { 390 },
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        root.AddChild(split);
        split.AddChild(BuildListPanel());
        split.AddChild(BuildDetailPanel());

        _info = new Label
        {
            Name = "UnitListInfo",
            ThemeTypeVariation = "Muted",
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
            Text = "请选择左侧单位查看完整卡面。",
            CustomMinimumSize = new Vector2(0, 30),
        };
        root.AddChild(_info);

        _confirmDialog = new ConfirmationDialog
        {
            Name = "SaveOperationConfirm",
            OkButtonText = "确认",
            CancelButtonText = "取消",
        };
        _confirmDialog.Confirmed += () =>
        {
            Action action = _pendingConfirmedAction;
            _pendingConfirmedAction = null;
            action?.Invoke();
        };
        _confirmDialog.Canceled += () => _pendingConfirmedAction = null;
        AddChild(_confirmDialog);

        Refresh();
    }

    private Control BuildListPanel()
    {
        var panel = new VBoxContainer
        {
            Name = "UnitLists",
            CustomMinimumSize = new Vector2(330, 0),
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };

        panel.AddChild(new Label { Text = "当前会话", ThemeTypeVariation = "SectionTitle" });
        _sessionList = new ItemList
        {
            Name = "SessionUnitList",
            CustomMinimumSize = new Vector2(0, 150),
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
            SelectMode = ItemList.SelectModeEnum.Single,
        };
        _sessionList.ItemSelected += SelectSession;
        panel.AddChild(_sessionList);

        var sessionActions = new HBoxContainer();
        _saveSessionButton = new Button
        {
            Name = "SaveSessionUnit", Text = "写入存档", SizeFlagsHorizontal = SizeFlags.ExpandFill
        };
        _saveSessionButton.Pressed += RequestSaveSelectedSession;
        sessionActions.AddChild(_saveSessionButton);
        _removeSessionButton = new Button
        {
            Name = "RemoveSessionUnit", Text = "移出会话", SizeFlagsHorizontal = SizeFlags.ExpandFill
        };
        _removeSessionButton.Pressed += RemoveSelectedSession;
        sessionActions.AddChild(_removeSessionButton);
        panel.AddChild(sessionActions);

        panel.AddChild(new HSeparator());
        panel.AddChild(new Label { Text = "本地存档", ThemeTypeVariation = "SectionTitle" });
        _diskList = new ItemList
        {
            Name = "DiskUnitList",
            CustomMinimumSize = new Vector2(0, 150),
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
            SelectMode = ItemList.SelectModeEnum.Single,
        };
        _diskList.ItemSelected += SelectDisk;
        panel.AddChild(_diskList);

        var diskActions = new HBoxContainer();
        _loadDiskButton = new Button
        {
            Name = "LoadDiskUnit", Text = "载入到会话", SizeFlagsHorizontal = SizeFlags.ExpandFill
        };
        _loadDiskButton.Pressed += LoadSelectedDisk;
        diskActions.AddChild(_loadDiskButton);
        _deleteDiskButton = AppTheme.MakeDangerButton("删除存档…");
        _deleteDiskButton.Name = "DeleteDiskUnit";
        _deleteDiskButton.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        _deleteDiskButton.Pressed += RequestDeleteSelectedDisk;
        diskActions.AddChild(_deleteDiskButton);
        panel.AddChild(diskActions);
        return panel;
    }

    private Control BuildDetailPanel()
    {
        var panel = new PanelContainer
        {
            Name = "UnitDetailPanel",
            CustomMinimumSize = new Vector2(320, 0),
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        var detailBox = new VBoxContainer
        {
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        panel.AddChild(detailBox);
        _detailAvatar = new TextureRect
        {
            Name = "UnitDetailAvatar",
            CustomMinimumSize = new Vector2(0, 128),
            StretchMode = TextureRect.StretchModeEnum.KeepAspectCentered,
            ExpandMode = TextureRect.ExpandModeEnum.IgnoreSize,
        };
        _detailAvatar.Visible = false;
        detailBox.AddChild(_detailAvatar);
        _detail = new RichTextLabel
        {
            Name = "UnitDetail",
            Text = "请选择左侧单位查看完整卡面。",
            BbcodeEnabled = false,
            FitContent = false,
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        detailBox.AddChild(_detail);
        return panel;
    }

    private void Refresh()
    {
        int keepId = _selected?.Id ?? 0;
        bool keepDisk = _selectedFromDisk;
        _sessionUnits = new List<UnitDef>(Save.CurrentSave.All);
        _diskUnits = Save.SaveManager.LoadAll();

        _sessionList.Clear();
        foreach (UnitDef unit in _sessionUnits)
            _sessionList.AddItem(UnitListLabel(unit));
        _diskList.Clear();
        foreach (UnitDef unit in _diskUnits)
            _diskList.AddItem(UnitListLabel(unit));

        _selected = null;
        _selectedFromDisk = false;
        int restoreIndex = keepDisk
            ? _diskUnits.FindIndex(unit => unit.Id == keepId)
            : _sessionUnits.FindIndex(unit => unit.Id == keepId);
        if (restoreIndex >= 0)
        {
            if (keepDisk)
            {
                _diskList.Select(restoreIndex);
                SelectDisk(restoreIndex);
            }
            else
            {
                _sessionList.Select(restoreIndex);
                SelectSession(restoreIndex);
            }
        }
        else if (_sessionUnits.Count > 0)
        {
            _sessionList.Select(0);
            SelectSession(0);
        }
        else if (_diskUnits.Count > 0)
        {
            _diskList.Select(0);
            SelectDisk(0);
        }
        else
        {
            _detail.Text = "当前会话与 user://saves 中均没有单位。\n请先使用建卡向导创建一张卡牌。";
            UpdateActionState();
        }
        UpdateSectionCounts();
    }

    private void UpdateSectionCounts()
    {
        Node sessionTitle = _sessionList.GetParent().GetChild(0);
        if (sessionTitle is Label sessionLabel)
            sessionLabel.Text = $"当前会话（{_sessionUnits.Count}）";

        for (int index = 0; index < _diskList.GetParent().GetChildCount(); index++)
        {
            if (_diskList.GetParent().GetChild(index) is Label label &&
                label.Text.StartsWith("本地存档", StringComparison.Ordinal))
            {
                label.Text = $"本地存档（{_diskUnits.Count}）";
                break;
            }
        }
    }

    private void SelectSession(long index)
    {
        int selectedIndex = (int)index;
        if (selectedIndex < 0 || selectedIndex >= _sessionUnits.Count) return;
        _diskList.DeselectAll();
        _selected = _sessionUnits[selectedIndex];
        _selectedFromDisk = false;
        ShowDetail(_selected, "当前会话");
        UpdateActionState();
    }

    private void SelectDisk(long index)
    {
        int selectedIndex = (int)index;
        if (selectedIndex < 0 || selectedIndex >= _diskUnits.Count) return;
        _sessionList.DeselectAll();
        _selected = _diskUnits[selectedIndex];
        _selectedFromDisk = true;
        ShowDetail(_selected, "本地存档");
        UpdateActionState();
    }

    private void UpdateActionState()
    {
        bool hasSession = _selected != null && !_selectedFromDisk;
        bool hasDisk = _selected != null && _selectedFromDisk;
        _saveSessionButton.Disabled = !hasSession;
        _removeSessionButton.Disabled = !hasSession;
        _loadDiskButton.Disabled = !hasDisk;
        _deleteDiskButton.Disabled = !hasDisk;
        _saveSessionButton.Text = hasSession && Save.SaveManager.Exists(_selected.Id)
            ? "覆盖存档…" : "写入存档";
    }

    private void RequestSaveSelectedSession()
    {
        if (_selected == null || _selectedFromDisk) return;
        UnitDef unit = _selected;
        if (!Save.SaveManager.Exists(unit.Id))
        {
            SaveSessionUnit(unit);
            return;
        }
        Confirm(
            "确认覆盖存档",
            $"确定用当前会话中的卡面覆盖存档 #{unit.Id}“{unit.Name}”吗？",
            () => SaveSessionUnit(unit));
    }

    private void SaveSessionUnit(UnitDef unit)
    {
        bool existed = Save.SaveManager.Exists(unit.Id);
        if (Save.SaveManager.Save(unit))
            _info.Text = existed
                ? $"已覆盖存档 #{unit.Id} {unit.Name}。"
                : $"已写入存档 #{unit.Id} {unit.Name}。";
        else
            _info.Text = $"保存 #{unit.Id} {unit.Name} 失败，请查看错误日志。";
        _selected = unit;
        _selectedFromDisk = false;
        Refresh();
    }

    private void RemoveSelectedSession()
    {
        if (_selected == null || _selectedFromDisk) return;
        UnitDef unit = _selected;
        Save.CurrentSave.All.Remove(unit);
        if (ReferenceEquals(Save.CurrentSave.Current, unit))
            Save.CurrentSave.Current = null;
        _info.Text = $"已将 #{unit.Id} {unit.Name} 移出本次会话；本地存档未删除。";
        _selected = null;
        Refresh();
    }

    private void LoadSelectedDisk()
    {
        if (_selected == null || !_selectedFromDisk) return;
        UnitDef unit = _selected;
        Save.CurrentSave.Upsert(unit);
        _info.Text = $"已载入 #{unit.Id} {unit.Name} 到会话，可直接用于对战选择。";
        _selected = unit;
        _selectedFromDisk = false;
        Refresh();
    }

    private void RequestDeleteSelectedDisk()
    {
        if (_selected == null || !_selectedFromDisk) return;
        UnitDef unit = _selected;
        Confirm(
            "确认删除存档",
            $"确定永久删除本地存档 #{unit.Id}“{unit.Name}”吗？\n当前会话中的副本不会一同删除。",
            () => DeleteSave(unit));
    }

    private void DeleteSave(UnitDef unit)
    {
        if (Save.SaveManager.Delete(unit.Id))
            _info.Text = $"已删除本地存档 #{unit.Id} {unit.Name}；如仍在会话中，可再次写入存档。";
        else
            _info.Text = $"删除存档 #{unit.Id} 失败，请查看错误日志。";
        _selected = null;
        Refresh();
    }

    private void Confirm(string title, string text, Action action)
    {
        _pendingConfirmedAction = action;
        _confirmDialog.Title = title;
        _confirmDialog.DialogText = text;
        _confirmDialog.PopupCentered(new Vector2I(560, 240));
    }

    private void ShowDetail(UnitDef unit, string source)
    {
        // 头像
        string avatarAbs = Save.SaveManager.ResolveFile(unit.AvatarPath);
        if (!string.IsNullOrEmpty(avatarAbs) && System.IO.File.Exists(avatarAbs))
        {
            var img = new Image();
            if (img.Load(avatarAbs) == Error.Ok)
            {
                _detailAvatar.Texture = ImageTexture.CreateFromImage(img);
                _detailAvatar.Visible = true;
            }
            else { _detailAvatar.Visible = false; }
        }
        else { _detailAvatar.Visible = false; }

        var text = new StringBuilder();
        text.AppendLine($"#{unit.Id}　{unit.Name}");
        text.AppendLine($"真名：{Display(unit.TrueName)}");
        text.AppendLine($"来源：{source}");
        text.AppendLine($"类型：{UnitTypeName(unit.UType)}　等级：Lv{unit.Level}");
        text.AppendLine($"职阶：{ClassName(unit.ServantClass)}");
        text.AppendLine($"隐藏属性：{(unit.IsServant ? HiddenName(unit.HiddenAttr) : "不适用")}");
        text.AppendLine($"人物阵营：{LawName(unit.Law)}·{MoralName(unit.Moral)}　战斗阵营：{unit.Faction}");
        text.AppendLine($"生存状态：{(unit.Alive ? "存活" : "已退场")}　行动：{(unit.Acted != 0 ? "已行动" : "未行动")}　游荡：{unit.Roaming}");
        text.AppendLine();
        text.AppendLine("属性");
        text.AppendLine($"筋力 {AttrDisplay(unit, 0)}　耐久 {AttrDisplay(unit, 1)}　敏捷 {AttrDisplay(unit, 2)}");
        text.AppendLine($"魔力 {AttrDisplay(unit, 3)}　幸运 {AttrDisplay(unit, 4)}　宝具 {AttrDisplay(unit, 5)}");
        text.AppendLine($"回路 {AttrDisplay(unit, 6)}");
        text.AppendLine($"魔力池：当前 {unit.MpCur} / 上限 {unit.MpCap} / 下限 {unit.MpFloor}");
        text.AppendLine($"脱离值 FP：{unit.Fp}　令咒：{unit.Cs}　每轮礼装已用：{unit.ItemUsesThisRound}");
        text.AppendLine();
        AppendResources(text, "技能", unit.Skills);
        AppendResources(text, "宝具", unit.Phantasms);
        AppendResources(text, "礼装", unit.Items);
        text.AppendLine("状态");
        if (unit.Status.Count == 0)
            text.AppendLine("  （无）");
        else
            foreach (StatusEntry status in unit.Status)
                text.AppendLine($"  {StatusUtil.Name(status.Kind)} ×{status.Layers}　来源单位 #{status.Source}");
        _detail.Text = text.ToString();
    }

    private static void AppendResources(StringBuilder text, string title, List<ResourceDef> resources)
    {
        text.AppendLine($"{title}（{resources.Count}）");
        if (resources.Count == 0)
        {
            text.AppendLine("  （无）");
            return;
        }
        foreach (ResourceDef resource in resources)
        {
            string runtime = resource.Recast > 0
                ? $"，回转 {resource.CurRecast}/{resource.Recast}"
                : "";
            string reserve = resource.ReserveMax > 0
                ? $"，储备 {resource.Reserve}/{resource.ReserveMax}"
                : "";
            string focus = resource.Kind == ResKind.Np
                ? $"，面向 {CardBuildRules.FocusName(resource.Focus)}"
                : "";
            text.AppendLine($"  #{resource.Id} {resource.CardTitle} [{RankUtil.Name(resource.Rank)}]{focus}{runtime}{reserve}，魔耗 {resource.Cost}" +
                            (string.IsNullOrWhiteSpace(resource.Note) ? "" : $"　※ {resource.Note}"));
        }
    }

    private static string UnitListLabel(UnitDef unit)
    {
        return $"#{unit.Id}　{unit.Name}　·　{UnitTypeName(unit.UType)} Lv{unit.Level}　·　{(unit.Alive ? "存活" : "退场")}";
    }

    private static string AttrDisplay(UnitDef unit, int index)
    {
        int raw = unit.Attr[index];
        int current = unit.AttrValue(index);
        return current == raw ? current.ToString() : $"{current}（基础{raw}）";
    }

    private static string UnitTypeName(int type) => type switch
    {
        1 => "从者", 2 => "御主", 3 => "召唤物", 4 => "人偶", 5 => "使魔", _ => "单位",
    };

    private static string ClassName(int value)
    {
        return value >= 0 && value < ClassNames.Length ? ClassNames[value] : $"未知({value})";
    }

    private static string HiddenName(int value)
    {
        return value >= 0 && value < HiddenNames.Length ? HiddenNames[value] : $"未知({value})";
    }

    private static string LawName(int value) => value switch
    {
        0 => "秩序", 2 => "混沌", _ => "中立",
    };

    private static string MoralName(int value) => value switch
    {
        0 => "善", 2 => "恶", _ => "中庸",
    };

    private static string Display(string value) => string.IsNullOrWhiteSpace(value) ? "（未填写）" : value;
}
