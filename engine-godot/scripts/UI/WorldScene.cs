using System;
using System.Collections.Generic;
using Godot;
using KsgGodot.Model;
using KsgGodot.Engine;

namespace KsgGodot.UI;

/// <summary>圣杯战争世界场景(单人/主机全控): 灵脉地图/行动/回合/契约/工房/圣杯</summary>
public partial class WorldScene : Control
{
    private Engine.KsgWorld _world;
    private Engine.WorldCampaign _campaign;
    private Engine.WorldAdvanced _advanced;
    private Random _rng = new();

    private VBoxContainer _root;
    private Label _status;
    private RichTextLabel _log;
    private ScrollContainer _actionScroll;
    private VBoxContainer _actionBox;
    private Button _nextBtn;
    private int _currentMasterId;      // 当前行动的单位

    // 联机
    public int NetworkPort = 0;            // 0=纯单人
    public string JoinHost = null;         // null=主机; 非null=分机
    private Net.NetLayer _net;
    private bool _isHost;
    private Net.WorldSnapshotData _remoteSnap;   // 分机: 主机快照

    public WorldScene()
    {
    }

    /// <summary>日志外发(临时验证用)。</summary>
    public event Action<string> OnLogLine;

    public override void _Ready()
    {
        AppTheme.Apply(this);

        var margin = new MarginContainer
        {
            AnchorRight = 1, AnchorBottom = 1,
        };
        margin.AddThemeConstantOverride("margin_left", 14);
        margin.AddThemeConstantOverride("margin_right", 14);
        margin.AddThemeConstantOverride("margin_top", 8);
        margin.AddThemeConstantOverride("margin_bottom", 10);
        AddChild(margin);

        _root = new VBoxContainer();
        margin.AddChild(_root);

        _root.AddChild(AppTheme.MakeHeader(
            "圣杯战争 · 世界",
            () => SceneRouter.GoMainMenu(this)));

        _status = new Label { Text = "", AutowrapMode = TextServer.AutowrapMode.WordSmart };
        _status.CustomMinimumSize = new Vector2(0, 46);
        _root.AddChild(_status);

        _log = new RichTextLabel
        {
            ThemeTypeVariation = "BattleLog",
            CustomMinimumSize = new Vector2(0, 120),
            SizeFlagsVertical = SizeFlags.ExpandFill,
            ScrollFollowing = true,
            BbcodeEnabled = false,
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
        };
        _root.AddChild(_log);

        _actionScroll = new ScrollContainer
        {
            CustomMinimumSize = new Vector2(0, 170),
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        _actionBox = new VBoxContainer { SizeFlagsHorizontal = SizeFlags.ExpandFill };
        _actionScroll.AddChild(_actionBox);
        _root.AddChild(_actionScroll);

        var cmdRow = new HBoxContainer();
        _root.AddChild(cmdRow);
        _nextBtn = new Button { Text = "下一个行动 →", CustomMinimumSize = new Vector2(200, 44) };
        _nextBtn.Pressed += NextAction;
        cmdRow.AddChild(_nextBtn);
        _nextBtn.Visible = false;   // 出场设置阶段不可行动

        // 联机: 分机直接等主机世界; 主机/单人在开场先做"出场设置"(配对+出生地)
        if (JoinHost != null)
        {
            _isHost = false;
            if (NetworkPort > 0)
            {
                _net = new Net.NetLayer();
                AddChild(_net);
                _net.OnMsg += OnNetMsg;
                _net.OnPeerLeft += p => AppendLog($"[联机] peer{p} 离开");
                _net.Join(JoinHost, NetworkPort);
                AppendLog($"[联机] 分机模式: 连接 {JoinHost}:{NetworkPort}");
                _helloSent = false;
                _nextBtn.Text = "等待主机回合(分机仅查看)";
                _nextBtn.Disabled = true;
            }
            AppendLog("[联机] 分机模式: 世界状态以主机广播为准");
            _status.Text = "等待主机完成出场设置并开始圣杯战争…";
            if (_net != null)
            {
                _actionBox.AddChild(new Label
                {
                    Text = "已连接主机。等待主机广播世界状态…",
                    ThemeTypeVariation = "Muted",
                });
            }
            return;
        }

        _isHost = true;
        if (NetworkPort > 0)
        {
            _net = new Net.NetLayer();
            AddChild(_net);
            _net.OnMsg += OnNetMsg;
            _net.OnPeerLeft += p => AppendLog($"[联机] peer{p} 离开");
            _net.Host(NetworkPort);
            AppendLog($"[联机] 主机模式: 监听端口 {NetworkPort}");
        }

        // 出场设置: 配对 + 出生地
        ShowSetupPanel();
    }

    // ================= 出场设置(配对御主×从者, 选择出生地) =================

    /// <summary>一个出场席位: 御主名/从者名/出生地。</summary>
    private class SeatSel
    {
        public string MasterKey = "";    // "存档:名称" 或 "演示:职业"
        public string ServantKey = "";
        public int LeylineId = 1;
        public bool IsDemo;              // 演示(非存档)对手席位
    }

    private readonly List<SeatSel> _seats = new();
    private Label _seatListLabel;
    private OptionButton _masterOpt, _servantOpt, _leylineOpt;
    private bool _setupStarted;

    /// <summary>列出存档卡(带类型标注)。</summary>
    private string SavedCardLabel(UnitDef u) =>
        (u.IsMaster ? "[御主]" : u.IsServant ? "[从者]" : "[其他]") + u.Name + (u.IsMaster ? " 职业:" + (u.MainJob ?? "") : "");

    private void ShowSetupPanel()
    {
        _setupStarted = true;
        _actionBox.ClearChildren();
        var saved = Save.CurrentSave.All;

        _actionBox.AddChild(new Label
        {
            Text = "── 出场设置: 为每位参战御主配对该从者, 并选择其出生灵脉 ──",
            ThemeTypeVariation = "SectionTitle",
        });

        // 1. 当前席位选择
        var row = new HBoxContainer();
        _actionBox.AddChild(row);
        row.AddChild(new Label { Text = "御主卡:", CustomMinimumSize = new Vector2(58, 0) });
        _masterOpt = new OptionButton { CustomMinimumSize = new Vector2(150, 0) };
        int masterIdx = 0;
        foreach (var u in saved)
            if (u.IsMaster) { _masterOpt.AddItem(SavedCardLabel(u)); masterIdx++; }
        _masterOpt.AddItem("(新建)演示御主");
        _masterOpt.Selected = masterIdx;   // 默认演示御主
        row.AddChild(_masterOpt);

        row.AddChild(new Label { Text = "从者卡:", CustomMinimumSize = new Vector2(58, 0) });
        _servantOpt = new OptionButton { CustomMinimumSize = new Vector2(170, 0) };
        int sIdx = 0;
        foreach (var u in saved)
            if (u.IsServant) { _servantOpt.AddItem(SavedCardLabel(u)); sIdx++; }
        _servantOpt.AddItem("(新建)演示从者");
        _servantOpt.Selected = sIdx;
        row.AddChild(_servantOpt);

        row.AddChild(new Label { Text = "出生地:", CustomMinimumSize = new Vector2(58, 0) });
        _leylineOpt = new OptionButton { CustomMinimumSize = new Vector2(150, 0) };
        foreach (var name in Engine.WorldCampaign.LeylineNames) _leylineOpt.AddItem(name);
        _leylineOpt.Selected = 0;
        row.AddChild(_leylineOpt);

        // 2. 按钮
        var btnRow = new HBoxContainer();
        _actionBox.AddChild(btnRow);
        var addSeat = new Button { Text = "＋ 加入此席位", CustomMinimumSize = new Vector2(0, 36) };
        addSeat.Pressed += () =>
        {
            if (_seats.Count >= 6) { AppendLog("最多6个席位"); return; }
            var sel = new SeatSel
            {
                MasterKey = _masterOpt.GetItemText(_masterOpt.Selected),
                ServantKey = _servantOpt.GetItemText(_servantOpt.Selected),
                LeylineId = _leylineOpt.Selected + 1,
            };
            _seats.Add(sel);
            RefreshSeatList();
            AppendLog($"已加入席位: {sel.ServantKey} 由 {sel.MasterKey} 御主带领, 降临「{_leylineOpt.GetItemText(_leylineOpt.Selected)}」");
        };
        btnRow.AddChild(addSeat);

        var addDemo = new Button { Text = "＋ 快速加入演示敌手", CustomMinimumSize = new Vector2(0, 36) };
        addDemo.Pressed += () =>
        {
            if (_seats.Count >= 6) { AppendLog("最多6个席位"); return; }
            _seats.Add(new SeatSel
            {
                MasterKey = "(新建)演示御主", ServantKey = "(新建)演示从者",
                LeylineId = _leylineOpt.Selected + 1,
                IsDemo = true,
            });
            RefreshSeatList();
            AppendLog("已加入演示敌手席位(敌对御主+从者)");
        };
        btnRow.AddChild(addDemo);

        // 3. 席位摘要
        _seatListLabel = new Label
        {
            Text = "尚未添加席位。请先至少加入 2 个席位(敌我双方)再开始。",
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
            ThemeTypeVariation = "Muted",
        };
        _actionBox.AddChild(_seatListLabel);

        // 4. 开始按钮
        var startBtn = new Button
        {
            Text = "开始圣杯战争 (按以上配对降临)",
            CustomMinimumSize = new Vector2(0, 48),
            Modulate = new Color(0.8f, 1.0f, 0.85f),
        };
        startBtn.Pressed += () =>
        {
            if (_seats.Count < 2)
            {
                AppendLog("⚠ 至少需要 2 个席位(攻击方与对手)才能开始");
                return;
            }
            StartWarFromSetup();
        };
        _actionBox.AddChild(startBtn);

        RefreshSeatList();
    }
    private void RefreshSeatList()
    {
        if (_seatListLabel == null) return;
        if (_seats.Count == 0)
        {
            _seatListLabel.Text = "尚未添加席位。请先至少加入 2 个席位(敌我双方)再开始。";
            return;
        }
        var sb = new System.Text.StringBuilder("已配置席位:\n");
        for (int i = 0; i < _seats.Count; i++)
        {
            var s = _seats[i];
            sb.AppendLine($"  {i + 1}. {s.ServantKey} ← {s.MasterKey} @ {Engine.WorldCampaign.LeylineNames[s.LeylineId - 1]}");
        }
        _seatListLabel.Text = sb.ToString();
    }

    /// <summary>按出场设置构建双方并开始圣杯战争。</summary>
    private void StartWarFromSetup()
    {
        var units = new List<UnitDef>();
        var seatLeylines = new List<int>();
        var saved = Save.CurrentSave.All;

        foreach (var sel in _seats)
        {
            UnitDef master = null, servant = null;
            // 御主: 存档卡优先, 否则演示
            foreach (var u in saved)
                if (u.IsMaster && SavedCardLabel(u) == sel.MasterKey) { master = u; break; }
            if (master == null)
            {
                master = MakeDemo(sel.IsDemo ? "敌对御主" : "演示御主", 2, 60, 30, 30, 30, 30, 30);
                int seatNo = units.Count / 2 + 1;
                master.Name = (sel.IsDemo ? "敌御主" : "御主") + seatNo;
            }
            // 从者: 存档卡优先, 否则演示
            foreach (var u in saved)
                if (u.IsServant && SavedCardLabel(u) == sel.ServantKey) { servant = u; break; }
            if (servant == null)
            {
                servant = MakeDemo(sel.IsDemo ? "敌对从者" : "演示从者", 1, 60, 40, 40, 40, 40, 40);
                int seatNo = units.Count / 2 + 1;
                servant.Name = (sel.IsDemo ? "敌从者" : "从者") + seatNo;
            }
            master.MasterUnitId = 0;
            servant.MasterUnitId = master.Id;
            units.Add(master);
            units.Add(servant);
            seatLeylines.Add(sel.LeylineId);
        }

        // Id 可能为 0(演示卡): 注册后按席位配对与降临
        SetupWorld(units, seatLeylines);
    }

    /// <summary>创建世界/注册/降临(指定出生地)/初始资金, 进入行动轮。</summary>
    private void SetupWorld(List<UnitDef> units, List<int> seatLeylines)
    {
        _world = new Engine.KsgWorld();
        _world.LogEvent += AppendLog;
        _campaign = new Engine.WorldCampaign(_world);
        _campaign.LogEvent += AppendLog;
        _advanced = new Engine.WorldAdvanced(_world, _campaign);
        _advanced.LogEvent += AppendLog;

        foreach (var u in units) _world.RegisterUnit(u);
        // 注册后按席位成对配对(御主 i, 从者 i+1), 并按席位顺序应用出生地
        var assigned = new Dictionary<int, int>();
        for (int i = 0; i + 1 < units.Count; i += 2)
        {
            units[i + 1].MasterUnitId = units[i].Id;
            int lid = i / 2 < seatLeylines.Count ? seatLeylines[i / 2] : 1;
            assigned[units[i].Id] = lid;
        }
        _campaign.SetupMap(units.Count, _rng, assigned);

        // 初始资金
        foreach (var u in _world.Units)
            if (u.IsMaster) _advanced.AddFunds(u.Id, 5);

        AppendLog("╔══════════════════════════════╗");
        AppendLog("║    圣杯战争 · 第1日 · 昼      ║");
        AppendLog("╚══════════════════════════════╝");
        AppendLog("全员降临完毕。请依次为每位御主选择行动。");
        _currentMasterId = -1;
        _nextBtn.Visible = true;
        RefreshStatus();
        BeginTurn();
        NextAction();
    }

    public override void _Process(double delta)
    {
        // 分机: 等连接建立后发 Hello(加入主机)
        if (!_isHost && _net != null && _net.Active && !_helloSent && NetworkPort > 0 && JoinHost != null)
        {
            try
            {
                var st = _net.Peer.GetConnectionStatus();
                if (st == Godot.MultiplayerPeer.ConnectionStatus.Connected)
                {
                    _helloSent = true;
                    _net.SendTo(1, Net.NetMsg.Hello, "join");
                    AppendLog("[联机] 已连接主机, 发送加入请求");
                }
            }
            catch { }
        }
        // 分机掉线自动重连(简化: 每5秒检查连接状态)
        if (!_isHost && _net != null && _net.Active && NetworkPort > 0 && JoinHost != null)
        {
            _reconnectAcc += (float)delta;
            if (_reconnectAcc >= 5.0f)
            {
                _reconnectAcc = 0;
                var st = _net.Peer.GetConnectionStatus();
                if (st == Godot.MultiplayerPeer.ConnectionStatus.Disconnected)
                {
                    AppendLog("[联机] 连接断开, 尝试重连…");
                    _net.Close();
                    _net.Join(JoinHost, NetworkPort);
                    _helloSent = false;
                }
            }
        }
    }
    private float _reconnectAcc;
    private bool _helloSent;

    private void OnNetMsg(int fromPeer, int msgId, string payload)
    {
        var msg = (Net.NetMsg)msgId;
        if (_isHost)
        {
            // 主机处理分机指令
            switch (msg)
            {
                case Net.NetMsg.Hello:
                    AppendLog($"[联机] 分机{fromPeer} 加入(或重连)");
                    _net.SendTo(fromPeer, Net.NetMsg.HelloAck, "欢迎");
                    // 重连即重发全量快照(断线恢复)
                    var snap0 = Net.WorldSnapshotData.From(_world, _campaign, _advanced);
                    _net.SendTo(fromPeer, Net.NetMsg.WorldSnapshot, snap0.ToJson());
                    break;
                case Net.NetMsg.Action:
                    ApplyRemoteAction(payload);
                    break;
                case Net.NetMsg.CsUse:
                    ApplyRemoteCs(payload);
                    break;
            }
        }
        else
        {
            // 分机处理主机广播
            switch (msg)
            {
                case Net.NetMsg.HelloAck:
                    AppendLog("[联机] 主机已接受加入");
                    break;
                case Net.NetMsg.Error:
                    if (payload.StartsWith("battle-start:"))
                    {
                        var parts = payload.Split(':');
                        AppendLog($"[联机] 主机方遭遇战! 你被分配操控敌方({(parts.Length > 3 ? parts[3] : "2")})");
                        OpenRemoteBattle(parts.Length > 3 && parts[3] == "1" ? 1 : 2);
                    }
                    else if (payload.StartsWith("battle:"))
                    {
                        AppendLog($"[联机] 主机方发生遭遇战({payload.Substring(7)}); 战斗在主机权威结算, 分机可沿途观战快照");
                    }
                    else if (payload.Length > 0)
                    {
                        AppendLog($"[联机] {payload}");
                    }
                    break;
                case Net.NetMsg.WorldSnapshot:
                    try
                    {
                        _remoteSnap = Net.WorldSnapshotData.FromJson(payload);
                        RenderRemoteSnapshot();
                    }
                    catch (Exception ex)
                    {
                        AppendLog($"[联机] 快照解析失败: {ex.Message}");
                    }
                    break;
                case Net.NetMsg.NextTurn:
                    AppendLog("[联机] 主机推进了回合");
                    break;
            }
        }
    }

    /// <summary>分机渲染主机快照(只读世界视图)。</summary>
    private void RenderRemoteSnapshot()
    {
        if (_remoteSnap == null) return;
        var sb = new System.Text.StringBuilder();
        sb.AppendLine($"第{_remoteSnap.Day}日 · {(_remoteSnap.Phase ? "昼" : "夜")} | 联机模式(只读)");
        foreach (var ly in _remoteSnap.Leylines)
        {
            string units = string.Join(",", _remoteSnap.Units.FindAll(u => u.Leyline == ly.Id).ConvertAll(u => u.Name));
            sb.AppendLine($"[{ly.Name}] 魔{ly.Mana} 人{ly.Flow}" +
                          (ly.HasWorkshop ? " [工房]" : "") + (ly.HasShrine ? " [神殿]" : "") +
                          $" | {units}");
        }
        _status.Text = sb.ToString();
        if (_remoteSnap.Finished)
        {
            _actionBox.ClearChildren();
            _actionBox.AddChild(new Label { Text = $"—— 圣杯战争结束! 胜者: {WinnerNameRemote()}" });
        }
    }

    private string WinnerNameRemote()
    {
        if (_remoteSnap == null || _remoteSnap.WinnerMasterId <= 0) return "无";
        foreach (var u in _remoteSnap.Units)
            if (u.Id == _remoteSnap.WinnerMasterId) return u.Name;
        return "无";
    }

    /// <summary>主机处理分机行动指令("unitId:action")。</summary>
    private void ApplyRemoteAction(string payload)
    {
        var parts = payload.Split(':');
        if (parts.Length < 2) return;
        if (int.TryParse(parts[0], out int unitId) && Enum.TryParse<WorldAction>(parts[1], out var act))
        {
            string r = _campaign.DoAction(unitId, act);
            AppendLog($"[联机] 分机指令: {r}");
            var u = _world.Units.Find(x => x.Id == unitId);
            if (u != null) u.Acted = 1;
            RefreshStatus();
            BroadcastWorld();
        }
    }

    /// <summary>主机处理分机令咒指令("master:servant:usage")。</summary>
    private void ApplyRemoteCs(string payload)
    {
        var parts = payload.Split(':');
        if (parts.Length < 3) return;
        if (int.TryParse(parts[0], out int m) && int.TryParse(parts[1], out int s) && int.TryParse(parts[2], out int u))
        {
            string r = _advanced.UseCommandSeal(m, s, u);
            AppendLog($"[联机] 令咒指令: {r}");
            RefreshStatus();
            BroadcastWorld();
        }
    }

    /// <summary>主机广播世界快照。</summary>
    private void BroadcastWorld()
    {
        if (_net == null || !_net.Active) return;
        var snap = Net.WorldSnapshotData.From(_world, _campaign, _advanced);
        _net.Broadcast(Net.NetMsg.WorldSnapshot, snap.ToJson());
    }

    private void BroadcastTurn()
    {
        if (_net == null || !_net.Active) return;
        _net.Broadcast(Net.NetMsg.NextTurn, $"{_campaign.Day}:{(_campaign.Phase ? 1 : 0)}");
        BroadcastWorld();
    }

    /// <summary>演示单位(出场设置中"新建"类选择): 从资源库带真实技能/宝具。</summary>
    private static UnitDef MakeDemo(string name, int utype, int lv, int s, int e, int a, int m, int lk)
    {
        var u = new UnitDef
        {
            Name = name, TrueName = name, UType = utype,
            Level = lv, MpCur = 80, MpCap = 150,
            Attr = new[] { s, e, a, m, lk, 40, 30 },
            Cs = 3,
        };
        // 演示单位也从资源库取真实技能/宝具(可玩性: 战斗有真实能力)
        if (utype == 1)
        {
            AddResByName(u.Skills, ResKind.Skill, "领袖气质·高洁");
            AddResByName(u.Skills, ResKind.Skill, "魔力放出·炎");
            AddResByName(u.Skills, ResKind.Skill, "回避");
            AddResByName(u.Phantasms, ResKind.Np, "誓约胜利之剑");
        }
        else
        {
            AddResByName(u.Skills, ResKind.Skill, "宝石魔术");
            AddResByName(u.Skills, ResKind.Skill, "神经衰弱");
        }
        return u;
    }

    /// <summary>按名从资源库克隆一条真实资源(找不到则跳过)。</summary>
    private static void AddResByName(List<ResourceDef> list, ResKind kind, string name)
    {
        ResourceDef src = null;
        foreach (var r in Data.ResourceDb.All)
        {
            if (r.Kind != kind) continue;
            bool match = r.Name == name || r.Name.Contains(name) || name.Contains(r.Name.Split('(')[0].Trim());
            if (match || name.Contains(r.Name.Split('·')[0].Trim()) && r.Name.StartsWith(name.Split('·')[0]))
                { src = r; break; }
        }
        if (src != null) list.Add(src.DeepClone());
    }

    private void BeginTurn()
    {
        _campaign.BeginTurnSettlement();
        _advanced.BeginTurn();
    }

    private void NextAction()
    {
        // 下一行动席位
        int nextId = _campaign.NextActionSlot();
        if (nextId <= 0)
        {
            // 行动阶段结束 → 进入下一回合
            _campaign.NextTurn();
            if (_campaign.Finished)
            {
                RefreshStatus();
                _nextBtn.Text = "查看胜者";
                _nextBtn.Disabled = true;
                AppendLog($"—— 圣杯战争结束!胜者: {WinnerName()} ——");
                BroadcastTurn();
                return;
            }
            BeginTurn();
            AppendLog($"—— 进入下一回合 ——");
            RefreshStatus();
            BroadcastTurn();
            NextAction();
            return;
        }
        _currentMasterId = nextId;
        RefreshStatus();
        RenderActions();
    }

    private string WinnerName()
    {
        var w = _world.Units.Find(u => u.Id == _campaign.WinnerMasterId);
        return w != null ? w.Name : "无";
    }

    private void RefreshStatus()
    {
        var sb = new System.Text.StringBuilder();
        sb.AppendLine($"第{_campaign.Day}日 · {(_campaign.Phase ? "昼" : "夜")}" +
                      $" | 行动对象: {NameOf(_currentMasterId)}{(_currentMasterId > 0 ? " (资金 " + _advanced.FundOf(_currentMasterId) + ")" : "")}");
        foreach (var ly in _world.Leylines)
        {
            var here = _world.Units.FindAll(u => u.CurrentLeyline == ly.Id && u.Alive);
            string units = here.Count > 0 ? string.Join(",", here.ConvertAll(u => u.Name)) : "-";
            sb.AppendLine($"[{ly.Name}] 魔{ly.Mana} 人{ly.Flow}" +
                          (ly.HasWorkshop ? " [工房]" : "") + (ly.HasShrine ? " [神殿]" : "") +
                          (_advanced.HasMarble(ly.Id) ? $" [固有结界:{_advanced.MarbleName(ly.Id)}]" : "") +
                          $" | {units}");
        }
        _status.Text = sb.ToString();
    }

    private string NameOf(int id)
    {
        var u = _world.Units.Find(x => x.Id == id);
        return u != null ? u.Name : "-";
    }

    private void RenderActions()
    {
        _actionBox.ClearChildren();
        var u = _world.Units.Find(x => x.Id == _currentMasterId);
        if (u == null) return;
        _actionBox.AddChild(new Label { Text = $"{u.Name} 的行动:" });

        (WorldAction, string)[] actions = {
            (WorldAction.SoulEat, "魂食(人流量-1 → 魔力)"),
            (WorldAction.Move, "机动(移动灵脉)"),
            (WorldAction.Scout, "侦查(广泛)"),
            (WorldAction.Survey, "情报调查"),
            (WorldAction.Intervene, "干涉灵脉"),
            (WorldAction.Make, "制造(礼装)"),
            (WorldAction.Build, "建设(工房)"),
            (WorldAction.Rest, "休整(回魔)"),
        };
        var serving = _world.Units.Find(x => x.MasterUnitId == u.Id && x.Alive && x.IsServant);
        foreach (var (act, label) in actions)
        {
            var btn = new Button { Text = label, CustomMinimumSize = new Vector2(0, 34) };
            var a = act;
            btn.Pressed += () =>
            {
                string r = _campaign.DoAction(u.Id, a);
                AppendLog(r);
                u.Acted = 1;
                RefreshStatus();
                NextAction();
            };
            _actionBox.AddChild(btn);
        }
        // 遭遇战触发: 若本灵脉有敌对从者且我方从者也在, 直接开战
        var myServant = _world.Units.Find(x => x.MasterUnitId == u.Id && x.Alive && x.IsServant);
        if (myServant != null)
        {
            var enemy = _world.Units.Find(x => x.IsServant && x.Alive && x.Id != myServant.Id &&
                                              x.CurrentLeyline == u.CurrentLeyline);
            if (enemy != null)
            {
                _actionBox.AddChild(new HSeparator());
                _actionBox.AddChild(new Label { Text = $"⚠ 遭遇! 发现敌从者「{enemy.Name}」在此灵脉" });
                var atkBtn = new Button
                {
                    Text = $"袭击 {enemy.Name} → 开战",
                    CustomMinimumSize = new Vector2(0, 40),
                    Modulate = new Color(0.95f, 0.55f, 0.55f),
                };
                var foe = enemy;
                atkBtn.Pressed += () =>
                {
                    var myMaster = u;
                    SplitBattle(myMaster, myServant, foe, u.CurrentLeyline);
                };
                _actionBox.AddChild(atkBtn);
            }
        }
        // 令咒用法(若有从者)
        if (serving != null)
        {
            _actionBox.AddChild(new HSeparator());
            _actionBox.AddChild(new Label { Text = $"令咒(剩{u.Cs}): 对{serving.Name}" });
            for (int i = 0; i < Engine.WorldAdvanced.CsUses.Length; i++)
            {
                int usage = i;
                var csBtn = new Button
                {
                    Text = $"令咒[{Engine.WorldAdvanced.CsUses[i].Name}]",
                    CustomMinimumSize = new Vector2(0, 30),
                };
                csBtn.Pressed += () =>
                {
                    string r = _advanced.UseCommandSeal(u.Id, serving.Id, usage);
                    AppendLog(r);
                    RefreshStatus();
                };
                _actionBox.AddChild(csBtn);
            }
        }
        // 契约(与同灵脉他方)
        var others = _world.Units.FindAll(x => x.CurrentLeyline == u.CurrentLeyline && x.Id != u.Id && x.Alive && x.IsMaster);
        if (others.Count > 0)
        {
            _actionBox.AddChild(new HSeparator());
            _actionBox.AddChild(new Label { Text = "契约(与同灵脉御主):" });
            foreach (var other in others)
            {
                var o = other;
                var pactBtn = new Button { Text = $"与{o.Name}缔结[不战契约]", CustomMinimumSize = new Vector2(0, 30) };
                pactBtn.Pressed += () =>
                {
                    _campaign.MakePact(PactType.Truce, u.Id, o.Id, 2);
                    RefreshStatus();
                };
                _actionBox.AddChild(pactBtn);
            }
        }
        // 建设工房(资金)
        if (u.IsMaster)
        {
            _actionBox.AddChild(new HSeparator());
            var wsBtn = new Button
            {
                Text = $"建造工房(1资金, 当前{_advanced.FundOf(u.Id)}资金)",
                CustomMinimumSize = new Vector2(0, 32),
            };
            wsBtn.Pressed += () =>
            {
                var ws = _advanced.BuildWorkshop(u.Id, u.CurrentLeyline, shrine: false);
                AppendLog(ws != null ? $"建成: {ws.Name}" : "建造失败");
                RefreshStatus();
            };
            _actionBox.AddChild(wsBtn);
        }
        _actionBox.AddChild(new HSeparator());
    }

    private void AppendLog(string msg)
    {
        _log.AppendText(msg + "\n");
        OnLogLine?.Invoke(msg);
    }

    /// <summary>分机: 打开联机战斗遥控面板(无本地卡面, 指令提交主机)。</summary>
    private void OpenRemoteBattle(int remoteSide)
    {
        var bat = new BattleScene(new List<UnitDef>(), new List<UnitDef>(), 4)
        {
            RemoteOnly = true,
            NetBridge = _net,
            NetIsHost = false,
            RemoteSide = remoteSide,
            RemotePeerId = 1,   // 分机视角: 对端(主机)固定 peer 1
        };
        AddChild(bat);
        bat.SetAnchorsPreset(Control.LayoutPreset.FullRect);
        AppendLog($"[联机] 战斗遥控面板开启(操控{(remoteSide == 1 ? "左方" : "右方")})");
    }

    /// <summary>遭遇战: 本地(单人/主机)跑 BattleScene, 结束后结果回写世界并通过网络通报。</summary>
    private void SplitBattle(UnitDef myMaster, UnitDef myServant, UnitDef enemyServant, int leylineId)
    {
        AppendLog($"━━ 遭遇战! {myServant.Name} vs {enemyServant.Name} ━━");
        // 网络通报战斗开始(分机遥控右方=敌方侧, 演示参战)
        if (_net != null && _net.Active && _isHost)
            _net.Broadcast(Net.NetMsg.Error, $"battle-start:{myServant.Name}:{enemyServant.Name}:2");

        // 构造从者侧卡面(我方从者+敌方从者)
        var left = myServant;
        var right = enemyServant;
        var battle = new BattleScene(left, right, 4);
        // 工房增益: 当前灵脉自阵营工房(魔能重炮=敌-40, 集束光标=己+40)
        var (wsBonus, wsPenalty) = _advanced.WorkshopBattleBonus(leylineId);
        if (wsBonus > 0 || wsPenalty > 0)
        {
            battle.InitialWinBonus[1] += wsBonus;
            battle.InitialWinBonus[2] += wsPenalty;
            AppendLog($"[工房] 本灵脉工房增益: 己方+{wsBonus}% / 敌方-{wsPenalty}%");
        }
        // 黑厄深阱: 己方主力抗性+10%(袭击方侧)
        foreach (var wsx in _advanced.Workshops)
        {
            if (wsx.Destroyed || wsx.LeylineId != leylineId) continue;
            if (wsx.Parts.Contains(WorkshopPart.DarkPit))
            {
                battle.InitialResUp[1] += 10;
                AppendLog("[工房] 黑厄深阱: 己方主力抗性上升+10%");
            }
            if (wsx.Parts.Contains(WorkshopPart.SanctionOrg))
            {
                battle.EnemySkillLevelDown[2] += 1;
                AppendLog("[工房] 制裁机关: 敌方技能效果等级-1");
            }
            if (wsx.Parts.Contains(WorkshopPart.AmplifyMod))
            {
                battle.AmplifySelfMagic = 1;   // 增幅模块: 己方魔术技能等级+1
                AppendLog("[工房] 增幅模块: 己方[类型:魔术]技能效果等级+1");
            }
        }
        // 固有结界: 若敌方在该灵脉有结界(防守), 敌方+20%胜率、我方撤退FP+1
        var (retreatPen, marbleWin) = _advanced.MarbleBattleEffect(leylineId, enemyServant.Id);
        if (marbleWin > 0 || retreatPen > 0)
        {
            battle.InitialWinBonus[2] += marbleWin;   // 敌方(防守结界持有者)+20%
            AppendLog($"[固有结界] 「{_advanced.MarbleName(leylineId)}」: 防守方+{marbleWin}%胜率, 袭击方撤退FP+{retreatPen}");
        }
        // 联机: 注入主机桥, 接收分机指令(分机遥控右方敌侧)
        if (_net != null && _net.Active && _isHost)
        {
            battle.NetBridge = _net;
            battle.NetIsHost = true;
            battle.RemotePeerId = _net.PeerIds.Count > 0 ? -1 : -1; // 由 OnMsg 的 fromPeer 获得
            battle.RemoteSide = 2;
        }
        battle.Finished += result =>
        {
            // 战斗结束回调: 回写世界
            bool leftWin = result.LeftWin;
            AppendLog($"── 战斗落定: {(leftWin ? myServant.Name : enemyServant.Name)} 胜! ──");
            if (leftWin)
            {
                enemyServant.Alive = false;
                enemyServant.Retreated = true;
                var enemyMaster = _world.Units.Find(x => x.MasterUnitId == enemyServant.Id);
                if (enemyMaster != null) { enemyMaster.Alive = false; enemyMaster.Retreated = true; }
                AppendLog($"  · {enemyServant.Name} 及其御主退场!");
            }
            else
            {
                myServant.Alive = false;
                myServant.Retreated = true;
                myMaster.Alive = false;
                myMaster.Retreated = true;
                AppendLog($"  · {myServant.Name} 及其御主退场!");
            }
            RefreshStatus();
            // 检查是否结束
            var aliveMasters = _world.Units.FindAll(x => x.IsMaster && x.Alive && !x.Retreated);
            if (aliveMasters.Count <= 1)
            {
                _campaign.Finished = true;
                _campaign.WinnerMasterId = aliveMasters.Count == 1 ? aliveMasters[0].Id : 0;
                AppendLog($"—— 圣杯战争结束! 胜者: {(aliveMasters.Count == 1 ? aliveMasters[0].Name : "无")} ——");
            }
            if (_net != null && _net.Active && _isHost)
                _net.Broadcast(Net.NetMsg.WorldSnapshot, Net.WorldSnapshotData.From(_world, _campaign, _advanced).ToJson());
        };
        AddChild(battle);
        battle.SetAnchorsPreset(Control.LayoutPreset.FullRect);
    }

    public override void _ExitTree()
    {
        if (_world != null) _world.LogEvent -= AppendLog;
        if (_campaign != null) _campaign.LogEvent -= AppendLog;
        if (_advanced != null) _advanced.LogEvent -= AppendLog;
    }
}