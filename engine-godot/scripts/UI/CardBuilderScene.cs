using Godot;
using KsgGodot.Building;
using KsgGodot.Model;
using System;
using System.Collections.Generic;

namespace KsgGodot.UI;

/// <summary>建卡向导：属性分配、资源搜索、面向/栏位限制与 RP 结算。</summary>
public partial class CardBuilderScene : Control
{
    private static readonly string[] ClassNames = {
        "Saber", "Lancer", "Archer", "Rider", "Caster",
        "Assassin", "Berserker", "Ruler", "Avenger", "Shielder"
    };

    // 各职阶基础属性（筋/耐/敏/魔/幸）。宝具属性不在建卡时分配。
    private static readonly int[][] ClassBase = {
        new[] { 20, 20, 20, 20, 20 },
        new[] { 10, 10, 30, 20, 10 },
        new[] { 20, 10, 20, 0, 20 },
        new[] { 10, 20, 20, 0, 20 },
        new[] { 0, 0, 0, 30, 20 },
        new[] { 0, 0, 20, 0, 20 },
        new[] { 20, 20, 20, 0, 0 },
        new[] { 10, 20, 10, 20, 30 },
        new[] { 10, 20, 20, 20, 0 },
        new[] { 20, 20, 20, 10, 10 },
    };

    private static readonly string[][] ClassSkillNames = {
        new[] { "对魔力", "骑乘" },
        new[] { "对魔力" },
        new[] { "对魔力", "单独行动" },
        new[] { "对魔力", "骑乘" },
        new[] { "道具制作", "阵地制作" },
        new[] { "气息遮蔽" },
        new[] { "狂化" },
        new[] { "对魔力", "真名看破", "神明裁决" },
        new[] { "复仇者", "忘却补正", "自我回复" },
        new[] { "己阵防御" },
    };

    private static readonly string[] HiddenNames = { "天", "地", "人", "星", "兽" };
    private static readonly int[][] HiddenLevel = {
        new[] { 60, 70 }, new[] { 50, 70 }, new[] { 40, 60 },
        new[] { 40, 60 }, new[] { 70, 70 },
    };
    private static readonly string[] FaceNames = {
        "决战", "即死", "魔剑", "防御", "进攻", "增益", "召唤", "状态", "补给", "特殊", "特攻"
    };

    private UnitDef _u;
    private int _cls;
    private int _hidden = 2;
    private int _level = 60;
    // 0..4 为五项通常属性，6 为御主回路；宝具属性 5 不参与分配。
    private readonly int[] _alloc = new int[7];
    private int _pts;

    private Label _ptsLabel;
    private Label _levelLabel;
    private Label _previewLabel;
    private Label _rpLabel;
    private HSlider _levelSlider;
    private VBoxContainer _allocBox;
    private Label _searchLabel;
    private LineEdit _searchBox;
    private ItemList[] _tabLists = new ItemList[3];   // 0技能 1宝具 2礼装
    private TabContainer _searchTabs;
    private OptionButton _faceFilter;
    private RichTextLabel _hoverDetail;
    private Label _hoverCaption;
    private OptionButton _classOpt;
    private OptionButton _hiddenOpt;
    private List<ResourceDef> _searchResults = new();
    private readonly List<ResourceDef> _selectedResources = new();
    private ItemList _selectedList;
    private Label _selectedLabel;
    private HBoxContainer _renameBox;
    private LineEdit _renameEdit;
    private LineEdit _noteEdit;
    private Button _applyRename;
    private Button _clearRename;
    private Label _renameHint;
    private Label _resultLabel;
    private CheckButton _isMasterBtn;
    private OptionButton _mainJobOpt, _subJobOpt;
    private Control _jobRow;
    private ConfirmationDialog _overwriteDialog;
    // 头像
    private TextureRect _avatarPreview;
    private HSlider _avatarScaleSlider;
    private Label _avatarScaleLabel;
    private Image _avatarSourceImage;   // 原始选择图
    private string _avatarSourceName = "";
    private Label _avatarHint;

    public override void _Ready()
    {
        AppTheme.Apply(this);
        Data.ResourceDb.Load();
        _u = new UnitDef { UType = 1 };
        BuildUI();
        ConfigureMode(false, true);
        RefreshAvatarUi();
    }

    private void BuildUI()
    {
        var margin = new MarginContainer
        {
            Name = "PageMargin", AnchorRight = 1, AnchorBottom = 1,
        };
        margin.AddThemeConstantOverride("margin_left", 16);
        margin.AddThemeConstantOverride("margin_top", 12);
        margin.AddThemeConstantOverride("margin_right", 16);
        margin.AddThemeConstantOverride("margin_bottom", 12);
        AddChild(margin);

        var pageScroll = new ScrollContainer
        {
            Name = "CardBuilderScroll",
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
            HorizontalScrollMode = ScrollContainer.ScrollMode.Disabled,
            VerticalScrollMode = ScrollContainer.ScrollMode.Auto,
        };
        margin.AddChild(pageScroll);

        var root = new VBoxContainer
        {
            Name = "CardBuilderContent",
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
        };
        root.AddThemeConstantOverride("separation", 6);
        pageScroll.AddChild(root);

        root.AddChild(AppTheme.MakeHeader("建卡向导", () => SceneRouter.GoMainMenu(this)));

        var modeRow = new HBoxContainer();
        root.AddChild(modeRow);
        modeRow.AddChild(new Label { Text = "卡面类型:" });
        _isMasterBtn = new CheckButton { Name = "MasterMode", Text = "御主（简化）" };
        _isMasterBtn.Toggled += OnModeToggle;
        modeRow.AddChild(_isMasterBtn);

        // 御主职业(主职业+子职业)
        var jobRow = new HBoxContainer { Name = "JobRow" };
        root.AddChild(jobRow);
        jobRow.AddChild(new Label { Text = "主职业:" });
        _mainJobOpt = new OptionButton { CustomMinimumSize = new Vector2(130, 0) };
        foreach (var j in Model.MasterJobs.MainJobs) _mainJobOpt.AddItem(j);
        _mainJobOpt.ItemSelected += _ =>
        {
            if (_u == null) return;
            _u.MainJob = _mainJobOpt.Selected >= 0 ? _mainJobOpt.GetItemText(_mainJobOpt.Selected) : "";
            RefreshPreview();
        };
        jobRow.AddChild(_mainJobOpt);
        jobRow.AddChild(new Label { Text = "子职业:" });
        _subJobOpt = new OptionButton { CustomMinimumSize = new Vector2(140, 0) };
        foreach (var j in Model.MasterJobs.SubJobs) _subJobOpt.AddItem(j);
        _subJobOpt.ItemSelected += _ =>
        {
            if (_u == null) return;
            _u.SubJob = _subJobOpt.Selected >= 0 ? _subJobOpt.GetItemText(_subJobOpt.Selected) : "";
            if (_u.SubJob == "无") _u.SubJob = "";
            RefreshPreview();
        };
        jobRow.AddChild(_subJobOpt);
        jobRow.Visible = false;   // 默认从者隐藏
        _jobRow = jobRow;

        var nameRow = new HBoxContainer();
        root.AddChild(nameRow);
        nameRow.AddChild(new Label { Text = "代号:" });
        var nameEdit = new LineEdit
        {
            Name = "NameEdit", PlaceholderText = "例如：谕天之剑", CustomMinimumSize = new Vector2(180, 0)
        };
        nameEdit.TextChanged += value => { _u.Name = value.Trim(); RefreshPreview(); };
        nameRow.AddChild(nameEdit);
        nameRow.AddChild(new Label { Text = "真名:" });
        var trueEdit = new LineEdit
        {
            Name = "TrueNameEdit", PlaceholderText = "例如：亚瑟·潘德拉贡", CustomMinimumSize = new Vector2(180, 0)
        };
        trueEdit.TextChanged += value => { _u.TrueName = value.Trim(); RefreshPreview(); };
        nameRow.AddChild(trueEdit);

        var classRow = new HBoxContainer();
        root.AddChild(classRow);
        classRow.AddChild(new Label { Text = "职阶:" });
        _classOpt = new OptionButton { Name = "ClassOption", CustomMinimumSize = new Vector2(140, 0) };
        foreach (string className in ClassNames) _classOpt.AddItem(className);
        _classOpt.ItemSelected += index =>
        {
            _cls = (int)index;
            Recalc();
            RefreshClassSkills();
            RefreshAllocUI();
            RefreshPreview();
        };
        classRow.AddChild(_classOpt);
        classRow.AddChild(new Label { Text = "隐藏属性:" });
        _hiddenOpt = new OptionButton { Name = "HiddenOption", CustomMinimumSize = new Vector2(160, 0) };
        foreach (string hiddenName in HiddenNames) _hiddenOpt.AddItem(hiddenName);
        _hiddenOpt.Select(_hidden);
        _hiddenOpt.ItemSelected += index =>
        {
            _hidden = (int)index;
            ApplyServantLevelRange();
            Recalc();
            RefreshPreview();
        };
        classRow.AddChild(_hiddenOpt);

        var levelRow = new HBoxContainer();
        root.AddChild(levelRow);
        levelRow.AddChild(new Label { Text = "等级:" });
        _levelSlider = new HSlider
        {
            Name = "LevelSlider", MinValue = 40, MaxValue = 70, Step = 10,
            Value = 60, CustomMinimumSize = new Vector2(240, 0)
        };
        _levelSlider.ValueChanged += value =>
        {
            _level = (int)value;
            Recalc();
            RefreshPreview();
        };
        levelRow.AddChild(_levelSlider);
        _levelLabel = new Label { Text = "60", CustomMinimumSize = new Vector2(40, 0) };
        levelRow.AddChild(_levelLabel);

        _ptsLabel = new Label { CustomMinimumSize = new Vector2(0, 26) };
        root.AddChild(_ptsLabel);
        _allocBox = new VBoxContainer { Name = "AllocationBox" };
        root.AddChild(_allocBox);

        _previewLabel = new Label { AutowrapMode = TextServer.AutowrapMode.WordSmart };
        root.AddChild(_previewLabel);
        _rpLabel = new Label { Name = "RpLabel", AutowrapMode = TextServer.AutowrapMode.WordSmart };
        root.AddChild(_rpLabel);

        // ---- 头像选择区 ----
        var avatarRow = new HBoxContainer { Name = "AvatarRow" };
        root.AddChild(avatarRow);
        avatarRow.AddChild(new Label { Text = "头像:", CustomMinimumSize = new Vector2(44, 0) });
        _avatarPreview = new TextureRect
        {
            Name = "AvatarPreview",
            CustomMinimumSize = new Vector2(96, 96),
            StretchMode = TextureRect.StretchModeEnum.KeepAspectCentered,
            ExpandMode = TextureRect.ExpandModeEnum.IgnoreSize,
        };
        avatarRow.AddChild(_avatarPreview);
        var avatarBox = new VBoxContainer { SizeFlagsHorizontal = SizeFlags.ExpandFill };
        avatarRow.AddChild(avatarBox);
        var avatarBtnRow = new HBoxContainer();
        avatarBox.AddChild(avatarBtnRow);
        var pickAvatar = new Button { Name = "PickAvatar", Text = "选择图片…" };
        pickAvatar.Pressed += PickAvatarImage;
        avatarBtnRow.AddChild(pickAvatar);
        var applyAvatar = new Button { Name = "ApplyAvatar", Text = "应用为头像" };
        applyAvatar.Pressed += ApplyAvatar;
        avatarBtnRow.AddChild(applyAvatar);
        var clearAvatar = new Button { Name = "ClearAvatar", Text = "清除头像" };
        clearAvatar.Pressed += () => { _avatarSourceImage = null; _u.AvatarPath = ""; RefreshAvatarUi(); };
        avatarBtnRow.AddChild(clearAvatar);
        var scaleRow = new HBoxContainer();
        avatarBox.AddChild(scaleRow);
        scaleRow.AddChild(new Label { Text = "缩放:" });
        _avatarScaleSlider = new HSlider
        {
            Name = "AvatarScale", MinValue = 25, MaxValue = 500, Step = 5, Value = 100,
            CustomMinimumSize = new Vector2(180, 0),
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
        };
        _avatarScaleSlider.ValueChanged += _ => RefreshAvatarPreview();
        scaleRow.AddChild(_avatarScaleSlider);
        _avatarScaleLabel = new Label { Text = "100%", CustomMinimumSize = new Vector2(52, 0) };
        scaleRow.AddChild(_avatarScaleLabel);
        _avatarHint = new Label
        {
            Text = "支持 PNG/JPG/WebP；可直接放大到 256×256 作为头像，不糊。",
            ThemeTypeVariation = "Muted",
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
        };
        avatarBox.AddChild(_avatarHint);

        root.AddChild(new HSeparator());
        root.AddChild(new Label
        {
            Text = "━━━ 技能 / 宝具 / 礼装 ━━━（单击查看效果，双击购买加入卡面）",
            CustomMinimumSize = new Vector2(0, 28)
        });

        var searchRow = new HBoxContainer();
        root.AddChild(searchRow);
        searchRow.AddChild(new Label { Text = "搜索:" });
        _searchBox = new LineEdit
        {
            Name = "ResourceSearch", PlaceholderText = "输入名称，如：誓约骑士/对魔力/黑键",
            CustomMinimumSize = new Vector2(320, 0)
        };
        _searchBox.TextChanged += _ => DoSearch();
        searchRow.AddChild(_searchBox);


        _faceFilter = new OptionButton { Name = "FaceFilter" };
        _faceFilter.AddItem("全部面向");
        foreach (string face in FaceNames) _faceFilter.AddItem(face);
        _faceFilter.ItemSelected += _ => DoSearch();
        searchRow.AddChild(_faceFilter);

        _searchLabel = new Label { CustomMinimumSize = new Vector2(150, 0) };
        searchRow.AddChild(_searchLabel);

        // 左右排版: 左侧窄列表, 右侧宽详情
        var split = new HSplitContainer
        {
            Name = "ResourceSplit",
            CustomMinimumSize = new Vector2(0, 330),
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        root.AddChild(split);

        // 左: 搜索结果分三类 Tab(技能/宝具/礼装)
        _searchTabs = new TabContainer
        {
            Name = "ResourceTabs",
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        string[] tabNames = { "技能", "宝具", "礼装" };
        for (int i = 0; i < 3; i++)
        {
            var tab = new VBoxContainer { Name = "Tab" + tabNames[i] };
            var list = new ItemList
            {
                Name = "ResourceResults_" + tabNames[i],
                SizeFlagsHorizontal = SizeFlags.ExpandFill,
                SizeFlagsVertical = SizeFlags.ExpandFill,
                SelectMode = ItemList.SelectModeEnum.Single,
            };
            list.AddThemeFontSizeOverride("font_size", 15);
            // 单击选中 → 右侧详情
            int listIndex = i;
            list.ItemSelected += index => ShowSelectedDetail(listIndex, (int)index);
            list.ItemActivated += OnSearchActivate;   // 双击加入
            tab.AddChild(list);
            _tabLists[i] = list;
            _searchTabs.AddChild(tab);
        }
        _searchTabs.SetTabTitle(0, "技能");
        _searchTabs.SetTabTitle(1, "宝具");
        _searchTabs.SetTabTitle(2, "礼装");
        split.AddChild(_searchTabs);

        // 右: 选中详情预览面板(名称/等级/效果/原文)
        var detailPanel = new PanelContainer
        {
            Name = "ResourceDetailPanel",
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        var detailBox = new VBoxContainer
        {
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        detailPanel.AddChild(detailBox);
        _hoverCaption = new Label { Text = "单击左侧资源查看详情", ThemeTypeVariation = "Muted" };
        _hoverCaption.AddThemeFontSizeOverride("font_size", 17);
        detailBox.AddChild(_hoverCaption);
        _hoverDetail = new RichTextLabel
        {
            Name = "ResourceDetail", BbcodeEnabled = false,
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
            Text = "",
        };
        _hoverDetail.AddThemeFontSizeOverride("normal_font_size", 16);
        detailBox.AddChild(_hoverDetail);
        split.AddChild(detailPanel);

        // 显式设分割位置: 左列表约300px, 右侧详情尽量宽
        split.SplitOffset = 300;

        _selectedLabel = new Label { Text = "已选资源(单击可重命名/注释)：" };
        root.AddChild(_selectedLabel);
        _selectedList = new ItemList
        {
            Name = "SelectedResources", CustomMinimumSize = new Vector2(0, 110),
            SelectMode = ItemList.SelectModeEnum.Single
        };
        _selectedList.ItemSelected += OnSelectedResourcePick;
        root.AddChild(_selectedList);

        // 重命名/注释编辑区(选中已选资源后出现)
        _renameBox = new HBoxContainer { Name = "RenameRow" };
        _renameBox.AddChild(new Label { Text = "显示名:" });
        _renameEdit = new LineEdit { PlaceholderText = "自定义名(留空=原名)", CustomMinimumSize = new Vector2(220, 0) };
        _renameBox.AddChild(_renameEdit);
        _renameBox.AddChild(new Label { Text = "注释:" });
        _noteEdit = new LineEdit { PlaceholderText = "玩家备注(不影响效果)", CustomMinimumSize = new Vector2(260, 0) };
        _renameBox.AddChild(_noteEdit);
        _applyRename = new Button { Text = "应用" };
        _applyRename.Pressed += ApplyRename;
        _renameBox.AddChild(_applyRename);
        _clearRename = new Button { Text = "清除" };
        _clearRename.Pressed += ClearRename;
        _renameBox.AddChild(_clearRename);
        _renameBox.Visible = false;
        root.AddChild(_renameBox);
        _renameHint = new Label { Text = "", ThemeTypeVariation = "Muted" };
        _renameHint.Visible = false;
        root.AddChild(_renameHint);

        var removeButton = new Button { Name = "RemoveResource", Text = "移除选中资源" };
        removeButton.Pressed += RemoveSelectedResource;
        root.AddChild(removeButton);

        _resultLabel = new Label
        {
            Name = "BuildResult", AutowrapMode = TextServer.AutowrapMode.WordSmart
        };
        root.AddChild(_resultLabel);

        var saveButton = new Button
        {
            Name = "SaveCard", Text = "保存此卡", CustomMinimumSize = new Vector2(0, 42)
        };
        saveButton.Pressed += RequestSave;
        root.AddChild(saveButton);

        _overwriteDialog = new ConfirmationDialog
        {
            Name = "OverwriteConfirm", Title = "确认覆盖存档",
            OkButtonText = "覆盖", CancelButtonText = "取消"
        };
        _overwriteDialog.Confirmed += SaveCurrentCard;
        AddChild(_overwriteDialog);
    }

    private void OnModeToggle(bool master)
    {
        ConfigureMode(master, true);
        _resultLabel.Text = master
            ? "已切换为御主简化建卡；从者专用字段和已选资源已清空。"
            : "已切换为从者建卡；御主回路、礼装和已选资源已清空。";
    }

    private void ConfigureMode(bool master, bool clearBuild)
    {
        if (_jobRow != null) _jobRow.Visible = master;   // 御主显示职业选择
        if (master && _jobRow != null && _u != null)
        {
            if (string.IsNullOrEmpty(_u.MainJob) && _mainJobOpt != null && _mainJobOpt.Selected >= 0)
                _u.MainJob = _mainJobOpt.GetItemText(_mainJobOpt.Selected);
            if (_u.SubJob == "无" || string.IsNullOrEmpty(_u.SubJob)) _u.SubJob = "无";
        }
        if (clearBuild)
        {
            Array.Clear(_alloc, 0, _alloc.Length);
            _u.Skills.Clear();
            _u.Phantasms.Clear();
            _u.Items.Clear();
            _u.Status.Clear();
        }

        _classOpt.Disabled = master;
        _hiddenOpt.Disabled = master;
        if (master)
        {
            _level = 40;
            _levelSlider.MinValue = 10;
            _levelSlider.MaxValue = 40;
            _levelSlider.Step = 10;
            _levelSlider.SetValueNoSignal(_level);
            _faceFilter.Select(0);
            _faceFilter.Disabled = true;   // 御主无宝具, 面向过滤不适用
        }
        else
        {
            _level = 60;
            _levelSlider.Step = 10;
            ApplyServantLevelRange();
            _faceFilter.Disabled = false;  // 从者有宝具, 面向过滤可用
        }

        Recalc();
        RefreshClassSkills();
        RefreshAllocUI();
        RefreshPreview();
        DoSearch();
    }

    private void ApplyServantLevelRange()
    {
        int low = HiddenLevel[_hidden][0];
        int high = HiddenLevel[_hidden][1];
        _levelSlider.MinValue = low;
        _levelSlider.MaxValue = high;
        _level = Math.Clamp(_level, low, high);
        // 地属性最低 50；其余规则档位均为 10 的倍数。
        if (_level % 10 != 0) _level = ((int)Math.Round(_level / 10.0)) * 10;
        _level = Math.Clamp(_level, low, high);
        _levelSlider.SetValueNoSignal(_level);
    }

    private void Recalc()
    {
        bool servant = !_isMasterBtn.ButtonPressed;
        _u.UType = servant ? 1 : 2;
        _u.ServantClass = servant ? _cls + 1 : 0;
        _u.HiddenAttr = servant ? _hidden : -1;
        _u.Level = _level;
        _u.Traits = 1; // 人型
        if (servant && _hidden == 0) _u.Traits |= 1 << 1; // 天：神性

        Array.Clear(_u.Attr, 0, _u.Attr.Length);
        if (servant)
        {
            _pts = 120 + (_level - 40) * 2;
            for (int attribute = 0; attribute < 5; attribute++)
            {
                _pts -= _alloc[attribute];
                _u.Attr[attribute] = ClassBase[_cls][attribute] + _alloc[attribute];
            }
            _u.Attr[5] = 0;
            _u.Attr[6] = 0;
            _u.MpCap = 150;
            _u.MpFloor = -100;
            _u.MpCur = 50;
        }
        else
        {
            _pts = 80;
            for (int attribute = 0; attribute < 5; attribute++)
            {
                _pts -= _alloc[attribute];
                _u.Attr[attribute] = _alloc[attribute];
            }
            _pts -= _alloc[6];
            _u.Attr[5] = 0;
            _u.Attr[6] = _alloc[6];
            _u.MpCap = _u.Attr[6];
            _u.MpFloor = -50;
            _u.MpCur = _u.MpCap;
        }
        _levelLabel.Text = _level.ToString();
        RefreshPtsLabel();
    }

    private void RefreshClassSkills()
    {
        _u.Skills.RemoveAll(skill => skill.Type == (int)SkillType.Class);
        if (!_u.IsServant) return;

        foreach (string name in ClassSkillNames[_cls])
        {
            ResourceDef skill = Data.ResourceDb.FindByName(name);
            if (skill != null && skill.Kind == ResKind.Skill && skill.Type == (int)SkillType.Class)
                _u.Skills.Add(skill.DeepClone());
            else
                GD.PushWarning($"职阶 {ClassNames[_cls]} 的自动技能“{name}”未在资源库找到");
        }
    }

    private void RefreshAllocUI()
    {
        foreach (Node node in _allocBox.GetChildren())
        {
            _allocBox.RemoveChild(node);
            node.QueueFree();
        }

        int[] attributes = _u.IsServant ? new[] { 0, 1, 2, 3, 4 } : new[] { 0, 1, 2, 3, 4, 6 };
        string[] names = { "筋力", "耐久", "敏捷", "魔力", "幸运", "宝具", "回路" };
        foreach (int attribute in attributes)
        {
            int index = attribute;
            int baseValue = _u.IsServant ? ClassBase[_cls][index] : 0;
            int maximum = _u.IsServant ? 60 : 50;
            var row = new HBoxContainer();
            row.AddChild(new Label { Text = names[index], CustomMinimumSize = new Vector2(48, 0) });
            var slider = new HSlider
            {
                Name = $"Attr{index}", MinValue = 0, MaxValue = maximum, Step = 5,
                Value = _alloc[index], CustomMinimumSize = new Vector2(200, 0),
                SizeFlagsHorizontal = SizeFlags.ExpandFill,
            };
            var valueLabel = new Label
            {
                Text = $"基础{baseValue}+分配{_alloc[index]}", CustomMinimumSize = new Vector2(140, 0)
            };
            slider.ValueChanged += value =>
            {
                int delta = (int)value - _alloc[index];
                if (_pts - delta < 0)
                {
                    slider.SetValueNoSignal(_alloc[index]);
                    _resultLabel.Text = "⚠️ 可分配属性点不足";
                    return;
                }
                _alloc[index] = (int)value;
                valueLabel.Text = $"基础{baseValue}+分配{_alloc[index]}";
                Recalc();
                RefreshPreview();
            };
            row.AddChild(slider);
            row.AddChild(valueLabel);
            _allocBox.AddChild(row);
        }
        RefreshPtsLabel();
    }

    private void RefreshPtsLabel()
    {
        string detail = _u.IsServant
            ? "五项通常属性，每项分配上限60；宝具属性不在此处分配"
            : "五项通常属性与回路共用80点，每项分配上限50";
        _ptsLabel.Text = $"(Lv{_level}) 剩余可分配点数：{_pts}　{detail}";
    }

    private void RefreshPreview()
    {
        string type = _u.IsServant
            ? $"[{ClassNames[_cls]}] 隐藏属性：{HiddenNames[_hidden]}"
            : "[御主·简化]";
        string circuit = _u.IsMaster ? $" 回路{_u.Attr[6]}" : "";
        _previewLabel.Text =
            $"{_u.Name}{(string.IsNullOrEmpty(_u.TrueName) ? "" : $"({_u.TrueName})")} Lv{_level} {type}\n" +
            $"属性：筋{_u.Attr[0]} 耐{_u.Attr[1]} 敏{_u.Attr[2]} 魔{_u.Attr[3]} 幸{_u.Attr[4]} 宝{_u.Attr[5]}{circuit}";
        RefreshSelectedResources();
    }

    private void RefreshSelectedResources()
    {
        _selectedResources.Clear();
        _selectedList.Clear();
        foreach (ResourceDef skill in _u.Skills)
        {
            bool automatic = skill.Type == (int)SkillType.Class;
            _selectedResources.Add(skill);
            string rankTxt = skill.EffectiveRankNow != skill.Rank && !automatic
                ? $"[{RankUtil.Name(skill.EffectiveRankNow)}↓]"
                : $"[{RankUtil.Name(skill.Rank)}]";
            _selectedList.AddItem($"{(automatic ? "自动职阶" : "技能")} · {skill.CardTitle}{rankTxt}" +
                                  (automatic ? " · 0RP" : $" · {CardBuildRules.RankCost(skill)}RP"));
        }
        foreach (ResourceDef phantasm in _u.Phantasms)
        {
            _selectedResources.Add(phantasm);
            string rankTxt = phantasm.EffectiveRankNow != phantasm.Rank
                ? $"[{RankUtil.Name(phantasm.EffectiveRankNow)}↓]"
                : $"[{RankUtil.Name(phantasm.Rank)}]";
            _selectedList.AddItem($"宝具·{CardBuildRules.FocusName(phantasm.Focus)} · {phantasm.CardTitle}{rankTxt}");
        }
        foreach (ResourceDef item in _u.Items)
        {
            _selectedResources.Add(item);
            _selectedList.AddItem($"礼装 · {item.CardTitle} · {CardBuildRules.RankCost(item)}RP");
        }

        int used = CardBuildRules.SpentRp(_u);
        int total = CardBuildRules.TotalRp(_u);
        _selectedLabel.Text = $"已选资源：{_selectedResources.Count} 项";
        _rpLabel.Text = _u.IsServant
            ? $"RP：{used}/{total}（剩余 {total - used}）｜保有技能 {CardBuildRules.PurchasedSkillCount(_u)}/3，宝具 {_u.Phantasms.Count}/3。每面向至多1个；第2、3宝具各含2RP扩容。"
            : $"RP：{used}/{total}（剩余 {total - used}）｜技能 {CardBuildRules.PurchasedSkillCount(_u)}/3，礼装 {_u.Items.Count}/3。当前礼装类别未入库，统一按3RP保守计费，第2、3礼装各含1RP扩容；职业前置请按资源文本复核。";
    }

    private void DoSearch()
    {
        if (_searchBox == null) return;
        string keyword = _searchBox.Text.Trim();
        int? focus = _faceFilter.Selected > 0 ? _faceFilter.Selected : null;
        ResKind[] kinds = { ResKind.Skill, ResKind.Np, ResKind.Item };
        var grouped = new List<ResourceDef>[3];
        _searchResults.Clear();
        for (int g = 0; g < 3; g++)
        {
            grouped[g] = Data.ResourceDb.Search(keyword, kinds[g], focus);
            grouped[g].RemoveAll(resource =>
                (resource.Kind == ResKind.Skill && resource.Type == (int)SkillType.Class) ||
                (_u.IsServant && resource.Kind == ResKind.Item) ||
                (_u.IsMaster && resource.Kind == ResKind.Np) ||
                (_u.IsServant && resource.Kind == ResKind.Skill && Data.ResourceDb.OwnerOf(resource) == "master") ||
                (_u.IsMaster && resource.Kind == ResKind.Skill && Data.ResourceDb.OwnerOf(resource) == "servant"));
            _searchResults.AddRange(grouped[g]);

            _tabLists[g].Clear();
            foreach (ResourceDef resource in grouped[g])
            {
                string face = resource.Kind switch
                {
                    ResKind.Skill => CardBuildRules.SkillFaceName(resource.Type),
                    ResKind.Np => CardBuildRules.FocusName(resource.Focus),
                    _ => "礼装",
                };
                int cost = CardBuildRules.IncrementalCost(_u, resource);
                _tabLists[g].AddItem($"#{resource.Id} {resource.Name}[{RankUtil.Name(resource.Rank)}] " +
                                    $"{face} · 加入{cost}RP · 魔{resource.Cost} 回{resource.Recast}");
            }
        }
        _searchLabel.Text = $"可购买：技能{grouped[0].Count} / 宝具{grouped[1].Count} / 礼装{grouped[2].Count}";
        HideHover();
    }

    private void OnKindFilterSelected(long selected)
    {
        // 已按 Tab 分组; 仅保留"宝具 Tab 才可用面向过滤"的联动
        bool supportsFocus = _u.IsServant && selected == 0;   // old: All 时允许
        _faceFilter.Disabled = !_u.IsServant;
        if (!_u.IsServant) _faceFilter.Select(0);
        DoSearch();
    }

    private void OnSearchActivate(long index)
    {
        int tab = _searchTabs.CurrentTab;
        if (tab < 0 || tab >= _tabLists.Length)
        {
            return;
        }
        ResKind[] kinds = { ResKind.Skill, ResKind.Np, ResKind.Item };
        string keyword = _searchBox.Text.Trim();
        int? focus = _faceFilter.Selected > 0 ? _faceFilter.Selected : null;
        var group = Data.ResourceDb.Search(keyword, kinds[tab], focus);
        group.RemoveAll(resource =>
            (resource.Kind == ResKind.Skill && resource.Type == (int)SkillType.Class) ||
            (_u.IsServant && resource.Kind == ResKind.Item) ||
            (_u.IsMaster && resource.Kind == ResKind.Np) ||
                (_u.IsServant && resource.Kind == ResKind.Skill && Data.ResourceDb.OwnerOf(resource) == "master") ||
                (_u.IsMaster && resource.Kind == ResKind.Skill && Data.ResourceDb.OwnerOf(resource) == "servant"));
        if (index < 0 || index >= group.Count) return;
        ResourceDef resource = group[(int)index];
        int cost = CardBuildRules.IncrementalCost(_u, resource);
        // 等级选择: 技能/宝具有低等级可买时弹出(否则直接加入)
        var variant = Data.ResourceDb.RankVariantFor(resource.Id);
        if (resource.Kind != ResKind.Item && variant.HasValue &&
            (variant.Value.RankRange.Length > 1 || variant.Value.RankRange.Length == 0) &&
            resource.Rank != Rank.E && resource.Rank != Rank.Neg)
        {
            AskRankAndAdd(_u, resource, variant.Value);
            return;
        }
        if (CardBuildRules.TryAdd(_u, resource, out string reason))
        {
            _resultLabel.Text = $"✅ 已加入：{resource.Name}（本次 {cost}RP）\n{resource.Text}";
            RefreshPreview();
            DoSearch();
        }
        else
        {
            _resultLabel.Text = $"⚠️ 无法加入“{resource.Name}”：{reason}";
        }
    }

    /// <summary>弹出等级选择(真实等级范围+各等级RP+数值提示), 确认后以所选等级加入。</summary>
    private void AskRankAndAdd(UnitDef unit, ResourceDef resource, (string[] RankRange, List<int[]> Series) variant)
    {
        var popup = new ConfirmationDialog
        {
            Title = $"选择购买等级 — {resource.Name}",
            DialogText = "同一技能可购买低等级版本(数值与RP按规则书等级表)。",
            OkButtonText = "购买此等级",
            CancelButtonText = "取消",
            Exclusive = true,
        };
        AddChild(popup);

        var box = new VBoxContainer();
        box.AddChild(new HSeparator());
        var opt = new OptionButton { CustomMinimumSize = new Vector2(340, 0) };
        string[] ranks = variant.RankRange.Length > 0 ? variant.RankRange : new[] { RankUtil.Name(resource.Rank) };
        int selIdx = 0;
        for (int i = 0; i < ranks.Length; i++)
        {
            string rn = ranks[i];
            Rank rr = RankUtil.Parse(rn);
            // RP: 按等级
            int rp = rr switch { Rank.Ex => 7, Rank.A => 5, Rank.B => 4, Rank.C => 3, Rank.D => 2, Rank.E => 1, _ => 5 };
            // 该等级的实际数值(第1序列)
            string valStr = "";
            if (variant.Series.Count > 0 && i < variant.Series[0].Length)
                valStr = $"（数值 {string.Join("/", variant.Series[0])} → 本级 {variant.Series[0][i]}）";
            opt.AddItem($"{rn} 级 — {rp}RP{valStr}");
            if (rn == RankUtil.Name(resource.Rank) || rn == RankUtil.Name(resource.EffectiveRankNow)) selIdx = i;
        }
        if (opt.ItemCount == 0) { opt.AddItem($"{RankUtil.Name(resource.Rank)} 级"); selIdx = 0; }
        opt.Selected = selIdx;
        box.AddChild(new Label { Text = "等级(可按规则书范围选任意等级):" });
        box.AddChild(opt);
        var tip = new Label
        {
            Text = $"规则书等级范围: {string.Join(" / ", variant.RankRange)}\nRP对照 A=5 B=4 C=3 D=2 E=1（第2件宝具另+2扩容）\n数值来自规则书原文各等级序列",
            ThemeTypeVariation = "Muted",
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
        };
        box.AddChild(tip);
        popup.AddChild(box);

        bool resolved = false;
        popup.Confirmed += () =>
        {
            if (resolved) return;
            resolved = true;
            int sel = opt.Selected;
            if (sel < 0 || sel >= opt.ItemCount)
            {
                popup.QueueFree();
                return;
            }
            string rankName = ranks.Length > 0 && sel < ranks.Length ? ranks[sel] : "A";
            Rank chosen = RankUtil.Parse(rankName);
            if (CardBuildRules.TryAdd(unit, resource, chosen, out string reason))
            {
                _resultLabel.Text = $"✅ 已加入：{resource.Name}[{RankUtil.Name(chosen)}]（{CardBuildRules.RankCost(unit.Skills.Count > 0 ? unit.Skills[^1] : resource)}RP，数值按规则书等级表）\n{resource.Text}";
                RefreshPreview();
                DoSearch();
            }
            else
            {
                _resultLabel.Text = $"⚠️ 无法加入“{resource.Name}”：{reason}";
            }
            popup.QueueFree();
        };
        popup.Canceled += () =>
        {
            if (resolved) return;
            resolved = true;
            popup.QueueFree();
        };
        popup.PopupCentered(new Vector2I(620, 380));
    }

    /// <summary>选中资源后显示其完整效果与原文(右侧详情面板)</summary>
    private void ShowSelectedDetail(int listIndex, int itemIndex)
    {
        if (listIndex < 0 || listIndex >= _tabLists.Length) return;
        ResKind[] kinds = { ResKind.Skill, ResKind.Np, ResKind.Item };
        string keyword = _searchBox.Text.Trim();
        int? focus = _faceFilter.Selected > 0 ? _faceFilter.Selected : null;
        var group = Data.ResourceDb.Search(keyword, kinds[listIndex], focus);
        group.RemoveAll(resource =>
            (resource.Kind == ResKind.Skill && resource.Type == (int)SkillType.Class) ||
            (_u.IsServant && resource.Kind == ResKind.Item) ||
            (_u.IsMaster && resource.Kind == ResKind.Np) ||
                (_u.IsServant && resource.Kind == ResKind.Skill && Data.ResourceDb.OwnerOf(resource) == "master") ||
                (_u.IsMaster && resource.Kind == ResKind.Skill && Data.ResourceDb.OwnerOf(resource) == "servant"));
        if (itemIndex < 0 || itemIndex >= group.Count) return;
        ResourceDef resource = group[itemIndex];
        int cost = CardBuildRules.IncrementalCost(_u, resource);
        var sb = new System.Text.StringBuilder();
        sb.AppendLine($"【{resource.Name}】[{RankUtil.Name(resource.Rank)}] {resource.KindName} 加入需{cost}RP");
        sb.AppendLine($"时机: {WhenText(resource.When)} | 魔耗: {resource.Cost} | 回转: {resource.Recast}" +
                      (resource.ReserveMax > 0 ? $" | 储备: {resource.Reserve}/{resource.ReserveMax}" : ""));
        sb.AppendLine();
        sb.AppendLine("效果:");
        if (resource.Effects.Count == 0)
        {
            sb.AppendLine("  (无效果行)");
        }
        else
        {
            for (int i = 0; i < resource.Effects.Count; i++)
            {
                EffectLine el = resource.Effects[i];
                string effName = EffFlagCn(el.Flag);
                var parts = new System.Collections.Generic.List<string> { effName };
                if (el.Value != 0) parts.Add($"数值{Sign(el.Value)}");
                if (el.Attr >= 0 && el.Attr < 7 && (el.Flag == EffFlag.AttrUp || el.Flag == EffFlag.AttrDown ||
                    el.Flag == EffFlag.AttrUpConst || el.Flag == EffFlag.AttrDownConst))
                    parts.Add(AttrCn(el.Attr));
                if (el.Status != 0 && (el.Flag == EffFlag.StatusGive || el.Flag == EffFlag.StatusRemove ||
                    el.Flag == EffFlag.StateRes || el.Flag == EffFlag.StateIm || el.Flag == EffFlag.EffectIm))
                    parts.Add(StatusUtil.Name((StatusKind)el.Status));
                if (el.Layers != 0) parts.Add($"层{el.Layers}");
                parts.Add(EffTargetCn(el.Target));
                if (el.Times > 1) parts.Add($"×{el.Times}");
                var ch = new System.Collections.Generic.List<string>();
                if (el.Chance > 0) ch.Add($"{el.Chance}%");
                else if (el.ChanceAttrBase >= 0 && el.ChanceAttrBase < 7) ch.Add($"以{AttrCn(el.ChanceAttrBase)}判定");
                if (el.ChanceNeg) ch.Add("负面");
                if (el.LuckHalve) ch.Add("幸运≥40减半");
                if (ch.Count > 0) parts.Add($"判定[{string.Join(" ", ch)}]");
                if (el.Cond != Cond.None) parts.Add($"条件[{CondCn(el.Cond, el.CondArg, el.CondArg2)}]");
                if (el.Cap > 0) parts.Add($"上限{el.Cap}");
                sb.AppendLine($"  {i + 1}. {string.Join(" ", parts)}");
            }
        }
        sb.AppendLine();
        sb.AppendLine("原文:");
        sb.AppendLine(string.IsNullOrWhiteSpace(resource.Text) ? "  (无原文)" : resource.Text);
        _hoverDetail.Text = sb.ToString();
        _hoverCaption.Text = $"选中: {resource.Name}";
    }

    private void HideHover()
    {
        _hoverDetail.Text = "";
        _hoverCaption.Text = "单击左侧资源查看详情";
    }

    private static string EffFlagCn(EffFlag flag) => flag switch
    {
        EffFlag.AttrUp => "属性上升", EffFlag.AttrDown => "属性下降",
        EffFlag.AttrUpConst => "属性常驻上升", EffFlag.AttrDownConst => "属性常驻下降",
        EffFlag.WinUp => "胜率上升", EffFlag.WinDown => "胜率下降",
        EffFlag.FinalWinUp => "最终胜率上升", EffFlag.FinalWinDown => "最终胜率下降",
        EffFlag.FloorUp => "底限胜率上升", EffFlag.FloorPen => "底限穿透",
        EffFlag.HitUp => "判定成功率上升", EffFlag.HitFinalUp => "判定最终成功率上升", EffFlag.HitPen => "判定成功率下降",
        EffFlag.ResUp => "抗性上升", EffFlag.ResDown => "抗性下降",
        EffFlag.StateRes => "状态抵抗", EffFlag.StateIm => "状态免疫", EffFlag.EffectIm => "效果免疫",
        EffFlag.ManaUp => "魔力增加", EffFlag.ManaDown => "魔力减少",
        EffFlag.StatusGive => "赋予状态", EffFlag.StatusRemove => "移除状态",
        EffFlag.Recast => "回转增加", EffFlag.RecastLose => "回转减少",
        EffFlag.FpUp => "脱离值增加", EffFlag.FpDown => "脱离值减少", EffFlag.TpFp => "战斗中脱离值增加",
        EffFlag.Death => "即死", EffFlag.BoundDeath => "限定即死(轰击)",
        EffFlag.Pierce => "必中", EffFlag.InvPierce => "无敌贯通",
        EffFlag.Evade => "赋予回避", EffFlag.Protect => "赋予保护", EffFlag.Invincible => "赋予无敌",
        EffFlag.Retaliate => "反击", EffFlag.Info => "情报", EffFlag.Cs => "令咒",
        EffFlag.BurnBlow => "爆燃(灼伤)", EffFlag.ElectricBlow => "激荡(感电)", EffFlag.PoisonBlow => "毒发(中毒)",
        EffFlag.Summon => "召唤", _ => flag.ToString(),
    };

    private static string Sign(int v) => v > 0 ? $"+{v}" : v.ToString();
    private static string AttrCn(int a) => a switch { 0 => "筋力", 1 => "耐久", 2 => "敏捷", 3 => "魔力", 4 => "幸运", 5 => "宝具", 6 => "回路", _ => "?" };
    private static string EffTargetCn(int t) => t switch { -2 => "自身", -1 => "己方全体", 0 => "敌方全体", > 0 => $"敌方第{t}位", _ => "?" };
    private static string CondCn(Cond c, int a, int b) => c switch
    {
        Cond.OwnMain => "自身在主力位", Cond.OwnSupport => "自身在辅助位", Cond.OwnRear => "自身在支援位",
        Cond.TargetTrait => $"目标持特性{a}", Cond.TargetNotTrait => $"目标不持特性{a}",
        Cond.TargetStatusEq => $"目标持状态{StatusUtil.Name((StatusKind)a)}",
        Cond.TargetStatusGe => $"目标状态{StatusUtil.Name((StatusKind)a)}≥{b}",
        Cond.StatusNot => $"目标不持状态{StatusUtil.Name((StatusKind)a)}",
        Cond.TargetLuckGe => $"目标幸运≥{a}", Cond.TargetLevelGe => $"目标等级≥{a}",
        Cond.LevelDiff => $"等级差≥{a}", Cond.Day => "昼间", Cond.Night => "夜间",
        Cond.SelfTrait => $"自身持特性{a}", Cond.SelfIsMaster => "自身为御主", Cond.SelfIsServant => "自身为从者",
        Cond.SelfMp => $"魔力≥{a}", Cond.SelfHp => $"自身{AttrCn(a)}≥{b}", Cond.HasCs => "持有令咒",
        Cond.FriendInBattle => "己方有其他单位参战", Cond.EnemyMainMaster => "敌方主力为御主",
        Cond.EnemyIsServant => "敌方主力为从者", Cond.EnemyIsSummon => "敌方主力为召唤物",
        Cond.MpUnder => $"魔力<{a}", Cond.TargetAgiLt => $"目标敏捷<{a}", Cond.TargetAgiGe => $"目标敏捷≥{a}",
        Cond.EnemyTactic => $"敌方战术={a}", Cond.SelfTactic => $"自身战术={a}",
        Cond.TacticNotPaired => "己方战术未被克制", Cond.FirstEncounter => "初次同场战斗",
        _ => c.ToString(),
    };

    private static string WhenText(When when) => when switch
    {
        When.Passive => "被动", When.Act => "主动", When.BattleStart => "战斗开始",
        When.Proc => "工序", When.Any => "任意", _ => when.ToString(),
    };

    private void RemoveSelectedResource()
    {
        int selected = _selectedList.GetSelectedItems().Length > 0
            ? _selectedList.GetSelectedItems()[0]
            : -1;
        if (selected < 0 || selected >= _selectedResources.Count)
        {
            _resultLabel.Text = "请先在已选列表中选择要移除的资源";
            return;
        }

        ResourceDef resource = _selectedResources[selected];
        if (resource.Kind == ResKind.Skill && resource.Type == (int)SkillType.Class)
        {
            _resultLabel.Text = "职阶技能由职阶自动配置，不能单独移除";
            return;
        }
        if (CardBuildRules.Remove(_u, resource.Id))
        {
            _resultLabel.Text = $"已移除：{resource.Name}，相关 RP 已返还";
            RefreshPreview();
            DoSearch();
        }
    }

    /// <summary>单击已选资源: 显示重命名/注释编辑区(只影响卡面展示与存档,不改变效果)。</summary>
    private void OnSelectedResourcePick(long index)
    {
        if (index < 0 || index >= _selectedResources.Count)
        {
            _renameBox.Visible = false;
            _renameHint.Visible = false;
            return;
        }
        ResourceDef resource = _selectedResources[(int)index];
        if (resource.Kind == ResKind.Skill && resource.Type == (int)SkillType.Class)
        {
            _renameBox.Visible = false;
            _renameHint.Text = "职阶技能由职阶自动配置，不可重命名";
            _renameHint.Visible = true;
            return;
        }
        _renameBox.Visible = true;
        _renameHint.Visible = true;
        _renameEdit.Text = resource.DisplayName;
        _noteEdit.Text = resource.Note;
        _renameHint.Text = $"正在编辑: {resource.Name}[{RankUtil.Name(resource.Rank)}]（改名/注释仅影响卡面展示,效果与原文不变）";
    }

    private void ApplyRename()
    {
        if (_selectedList.GetSelectedItems().Length == 0)
        {
            _resultLabel.Text = "请先在已选列表中单击一个资源";
            return;
        }
        int index = _selectedList.GetSelectedItems()[0];
        if (index < 0 || index >= _selectedResources.Count) return;
        ResourceDef resource = _selectedResources[index];
        string display = _renameEdit.Text.Trim();
        resource.DisplayName = display;
        resource.Note = _noteEdit.Text.Trim();
        RefreshPreview();
        _resultLabel.Text = string.IsNullOrWhiteSpace(display)
            ? $"已恢复原名「{resource.Name}」并保存注释"
            : $"已重命名为「{display}」（原名 {resource.Name}），效果不变；注释已保存";
    }

    private void ClearRename()
    {
        if (_selectedList.GetSelectedItems().Length == 0) return;
        int index = _selectedList.GetSelectedItems()[0];
        if (index < 0 || index >= _selectedResources.Count) return;
        ResourceDef resource = _selectedResources[index];
        resource.DisplayName = "";
        resource.Note = "";
        _renameEdit.Text = "";
        _noteEdit.Text = "";
        RefreshPreview();
        _resultLabel.Text = $"已清除「{resource.Name}」的自定义名与注释";
    }

    // ---------- 头像 ----------

    private void PickAvatarImage()
    {
        var fd = new FileDialog
        {
            Title = "选择头像图片",
            FileMode = FileDialog.FileModeEnum.OpenFile,
            Access = FileDialog.AccessEnum.Filesystem,
            UseNativeDialog = true,
            Filters = new[] { "*.png, *.jpg, *.jpeg, *.webp ;图片文件" },
        };
        fd.FileSelected += path =>
        {
            var img = new Image();
            Error err = img.Load(path);
            if (err != Error.Ok)
            {
                _resultLabel.Text = $"❌ 无法读取图片：{path}";
                return;
            }
            _avatarSourceImage = img;
            _avatarSourceName = System.IO.Path.GetFileName(path);
            _avatarScaleSlider.Value = 100;
            RefreshAvatarPreview();
            _avatarHint.Text = $"已选：{_avatarSourceName}（原始 {img.GetWidth()}×{img.GetHeight()}）→ 应用为256×256头像";
        };
        AddChild(fd);
        fd.PopupCentered();
    }

    /// <summary>根据原图+缩放比例渲染预览(等比例缩放到预览框宽度内)。</summary>
    private void RefreshAvatarPreview()
    {
        if (_avatarSourceImage == null) return;
        double scale = _avatarScaleSlider.Value / 100.0;
        _avatarScaleLabel.Text = $"{(int)_avatarScaleSlider.Value}%";
        Image work = _avatarSourceImage;

        // 先等比缩放到目标显示尺寸(预览框96px按scale)
        int baseSize = 96;
        int scaled = System.Math.Max(1, (int)System.Math.Round(baseSize * scale));
        Image display = (Image)work.Duplicate();
        // 保持宽高比: 取短边缩放到 scaled
        int sw = display.GetWidth();
        int sh = display.GetHeight();
        float ratio = System.Math.Min((float)scaled / sw, (float)scaled / sh);
        int nw = System.Math.Max(1, (int)(sw * ratio));
        int nh = System.Math.Max(1, (int)(sh * ratio));
        display.Resize(nw, nh, Image.Interpolation.Bilinear);
        var tex = ImageTexture.CreateFromImage(display);
        _avatarPreview.Texture = tex;
    }

    /// <summary>将当前缩放视图居中裁剪为 256×256 并保存到 user://avatars/。</summary>
    private void ApplyAvatar()
    {
        if (_avatarSourceImage == null)
        {
            _resultLabel.Text = "请先选择一张图片";
            return;
        }
        double scale = _avatarScaleSlider.Value / 100.0;
        const int target = 256;

        // 原图按比例放大到至少覆盖 target: 取短边 * scale 作为目标尺寸
        int sw = _avatarSourceImage.GetWidth();
        int sh = _avatarSourceImage.GetHeight();
        float smallSide = System.Math.Min(sw, sh);
        if (smallSide <= 0) { _resultLabel.Text = "图片无效"; return; }
        double ratio = (target / (smallSide * scale)) > 1
            ? target / (smallSide * scale)          // 放大
            : (double)target / (smallSide * scale); // 缩小到恰好覆盖
        int nw = System.Math.Max(target, (int)System.Math.Ceiling(sw * ratio));
        int nh = System.Math.Max(target, (int)System.Math.Ceiling(sh * ratio));

        Image work = (Image)_avatarSourceImage.Duplicate();
        work.Resize(nw, nh, Image.Interpolation.Lanczos);

        // 居中裁剪 target×target
        int cx = (nw - target) / 2;
        int cy = (nh - target) / 2;
        var region = new Rect2I(cx, cy, target, target);
        Image cropped = work.GetRegion(region);
        cropped.Resize(target, target, Image.Interpolation.Lanczos);

        // 保存头像到便携/用户头像目录(先确保目录存在)
        try
        {
            System.IO.Directory.CreateDirectory(Save.SaveManager.AvatarsDir);
            string leaf = $"card{_u.Id}.png";
            string abs = System.IO.Path.Combine(Save.SaveManager.AvatarsDir, leaf);
            Error err = cropped.SavePng(abs);
            _avatarHint.Text = $"头像已保存为 256×256：{abs}";
            if (err != Error.Ok)
            {
                _resultLabel.Text = $"❌ 保存头像失败(err={err})";
                return;
            }
            _u.AvatarPath = leaf;   // 只存文件名, 便于随文件夹携带
        }
        catch (Exception ex)
        {
            _resultLabel.Text = $"❌ 保存头像失败:{ex.Message}";
            return;
        }
        RefreshAvatarUi();
        _resultLabel.Text = "✅ 头像已应用(保存卡面后生效)";
    }

    private void RefreshAvatarUi()
    {
        string resolved = Save.SaveManager.ResolveFile(_u.AvatarPath);
        if (!string.IsNullOrEmpty(resolved) && System.IO.File.Exists(resolved))
        {
            var img = new Image();
            if (img.Load(resolved) == Error.Ok)
                _avatarPreview.Texture = ImageTexture.CreateFromImage(img);
        }
        else
        {
            _avatarPreview.Texture = null;
        }
    }

    private void RequestSave()
    {
        Recalc();
        if (_pts < 0)
        {
            _resultLabel.Text = "❌ 属性点分配超额，不能保存";
            return;
        }
        if (!CardBuildRules.Validate(_u, out string reason))
        {
            _resultLabel.Text = $"❌ 不能保存：{reason}";
            return;
        }

        if (_u.Id > 0 && Save.SaveManager.Exists(_u.Id))
        {
            _overwriteDialog.DialogText = $"存档 #{_u.Id} 已存在。确定用当前卡面覆盖“{_u.Name}”吗？";
            _overwriteDialog.PopupCentered();
            return;
        }
        SaveCurrentCard();
    }

    private void SaveCurrentCard()
    {
        bool isNew = _u.Id <= 0 || !Save.SaveManager.Exists(_u.Id);
        // 头像文件按卡 ID 命名: 确保文件名与卡一致(card{id}.png)
        if (_u.Id > 0)
        {
            string want = $"card{_u.Id}.png";
            if (_u.AvatarPath != want)
            {
                string srcAbs = Save.SaveManager.ResolveFile(_u.AvatarPath);
                string target = System.IO.Path.Combine(Save.SaveManager.AvatarsDir, want);
                try
                {
                    if (!string.IsNullOrEmpty(srcAbs) && System.IO.File.Exists(srcAbs) && !System.IO.File.Exists(target))
                    {
                        System.IO.Directory.CreateDirectory(Save.SaveManager.AvatarsDir);
                        System.IO.File.Copy(srcAbs, target);
                    }
                }
                catch { }
                _u.AvatarPath = want;
            }
        }
        if (Save.SaveManager.Save(_u))
            _resultLabel.Text = $"✅ 已{(isNew ? "新建" : "覆盖")}存档 #{_u.Id}，并加入当前会话";
        else
            _resultLabel.Text = "❌ 保存失败";
    }
}
