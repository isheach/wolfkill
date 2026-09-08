using System;
using System.Collections.Generic;
using Godot;
using KsgGodot.Model;

namespace KsgGodot.UI;

/// <summary>对战场景:双方按工序交互触发,真实结算;支持手动输入层数类效果</summary>
public partial class BattleScene : Control
{
    private readonly System.Collections.Generic.List<UnitDef> _leftSide = new();
    private readonly System.Collections.Generic.List<UnitDef> _rightSide = new();
    private readonly int _width;
    private Engine.KsgBattle _battle;
    private Engine.KsgWorld _world;
    private enum BattlePhase { Tactic, Initial, Main, Final, Resolve, Ended }
    private BattlePhase _phase = BattlePhase.Tactic;
    private bool _castPending;
    private bool _nextWasDisabled;

    // UI
    private readonly System.Collections.Generic.HashSet<string> _usedThisPhase = new();
    private readonly System.Collections.Generic.Dictionary<Button,
        (UnitDef unit, ResourceDef res)> _castButtons = new();
    private bool _phaseTacticsDone = false;
    private bool _phaseAttrDone = false;
    private RichTextLabel _log;
    private VBoxContainer _actionBox;
    private Label _statusLabel;
    private Button _nextBtn;
    private HBoxContainer _cmdRow;

    public BattleScene(UnitDef a, UnitDef b, int width = 4)
    {
        _leftSide.Add(a);
        _rightSide.Add(b);
        _width = width;
    }

    // 联机战斗: 主机权威 + 分机提交指令
    public Net.NetLayer NetBridge;
    public bool NetIsHost;
    public int RemotePeerId = -1;   // 主机: 分机的peer id(收其指令); 分机: 自己的对端(发指令)
    public int RemoteSide = 0;      // 分机操控方: 1=左方 2=右方 0=仅观战
    public int[] InitialWinBonus = new int[3];   // 世界层工房增益: [1]=左方 [2]=右方 初始胜率修正
    public int[] InitialResUp = new int[3];      // 己方抗性上升%(黑厄深阱)
    public int[] EnemySkillLevelDown = new int[3]; // 敌方技能效果等级-1(制裁机关)
    public int AmplifySelfMagic;               // 己方[类型:魔术]技能效果等级+1(增幅模块)

    /// <summary>多单位开战构造。</summary>
    public BattleScene(System.Collections.Generic.List<UnitDef> leftSide,
        System.Collections.Generic.List<UnitDef> rightSide, int width)
    {
        if (leftSide != null) _leftSide.AddRange(leftSide);
        if (rightSide != null) _rightSide.AddRange(rightSide);
        _width = width;
    }

    public override void _Ready()
    {
        AppTheme.Apply(this);
        _world = new Engine.KsgWorld();
        _world.LogEvent += AppendLog;
        Engine.KsgEffects.LogEvent += AppendLog;
        Engine.KsgEffects.VerboseEvent += AppendLog;
        Engine.KsgEffects.UnitKilled += OnUnitKilled;

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
            Name = "BattleContent",
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        root.AddThemeConstantOverride("separation", 6);
        margin.AddChild(root);

        string title = _leftSide.Count == 1 && _rightSide.Count == 1
            ? "对战：" + _leftSide[0].Name + " vs " + _rightSide[0].Name
            : $"对战：左方{_leftSide.Count}人 vs 右方{_rightSide.Count}人";
        root.AddChild(AppTheme.MakeHeader(
            title,
            () => SceneRouter.GoMainMenu(this)));

        _statusLabel = new Label { Text = "", AutowrapMode = TextServer.AutowrapMode.WordSmart };
        root.AddChild(_statusLabel);
        _statusLabel.CustomMinimumSize = new Vector2(0, 72);

        _log = new RichTextLabel
        {
            Name = "BattleLog",
            ThemeTypeVariation = "BattleLog",
            CustomMinimumSize = new Vector2(0, 150),
            SizeFlagsVertical = SizeFlags.ExpandFill,
            ScrollFollowing = true,
            BbcodeEnabled = false,
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
        };
        root.AddChild(_log);

        _actionBox = new VBoxContainer
        {
            Name = "BattleActionList", SizeFlagsHorizontal = SizeFlags.ExpandFill
        };
        var scroll = new ScrollContainer
        {
            Name = "BattleActionScroll",
            CustomMinimumSize = new Vector2(0, 150),
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
            HorizontalScrollMode = ScrollContainer.ScrollMode.Disabled,
            VerticalScrollMode = ScrollContainer.ScrollMode.Auto,
        };
        scroll.AddChild(_actionBox);
        root.AddChild(scroll);

        _cmdRow = new HBoxContainer();
        root.AddChild(_cmdRow);
        _nextBtn = new Button { Text = "下一步 →", CustomMinimumSize = new Vector2(160, 40) };
        _nextBtn.Pressed += NextPhase;
        _cmdRow.AddChild(_nextBtn);

        _battle = new Engine.KsgBattle();
        _battle.LogEvent += AppendLog;
        if (!RemoteOnly)
        {
            // 无论如何都要注册进本场世界；RegisterUnit 会保留稳定 ID。
            foreach (UnitDef u in _leftSide) _world.RegisterUnit(u);
            foreach (UnitDef u in _rightSide) _world.RegisterUnit(u);
            foreach (UnitDef u in _leftSide) if (u.Id <= 0) u.Id = _world.RegisterUnit(u);
            foreach (UnitDef u in _rightSide) if (u.Id <= 0) u.Id = _world.RegisterUnit(u);
            // 多单位开战(单卡场景自动转成1v1队伍)
            if (_leftSide.Count > 1 || _rightSide.Count > 1)
                _battle.StartParty(_world, _leftSide, _rightSide, _width);
            else
                _battle.Start(_world, _leftSide[0].Id, _rightSide[0].Id, _width);
            // 工房增益(世界层注入): 初始胜率修正
            if (InitialWinBonus[1] != 0) _battle.AddWin(1, InitialWinBonus[1]);
            if (InitialWinBonus[2] != 0) _battle.AddWin(2, InitialWinBonus[2]);
            // 黑厄深阱: 己方主力抗性上升(演示: 以 HitFinalMod +10 近似, 工期1轮)
            if (InitialResUp[1] > 0 && _battle.Left.Count > 0)
            {
                _battle.Left[0].Unit.HitFinalMod += InitialResUp[1];
                AppendLog($"[工房] 黑厄深阱: 己方主力获得抗性上升+{InitialResUp[1]}%");
            }
            if (InitialResUp[2] > 0 && _battle.Right.Count > 0)
            {
                _battle.Right[0].Unit.HitFinalMod += InitialResUp[2];
                AppendLog($"[工房] 黑厄深阱: 己方主力获得抗性上升+{InitialResUp[2]}%");
            }
        }

        SettlePassives();

        RefreshStatus();
        PhaseTactics();
        InitNetBattle();

        if (RemoteOnly)
        {
            // 纯远程模式: 无本地引擎状态, 显示遥控面板(等待主机广播)
            AppendLog("┈ 联机战斗遥控面板 ┈");
            AppendLog("你的指令将提交主机结算, 日志与状态由主机广播。");
        }
    }

    /// <summary>仅远程(分机无本地卡面): 不初始化战斗数据, 纯指令+日志。</summary>
    public bool RemoteOnly;

    /// <summary>联机战斗初始化: 挂接网络消息。</summary>
    private void InitNetBattle()
    {
        if (NetBridge == null || !NetBridge.Active) return;
        NetBridge.OnMsg += OnNetBattleMsg;
        if (NetIsHost)
        {
            AppendLog("[联机] 本战斗为主机权威(分机指令已接入)");
        }
        else
        {
            AppendLog($"[联机] 本战斗为分机视角(指令将提交主机, 操控方: {(RemoteSide == 1 ? "左方" : RemoteSide == 2 ? "右方" : "观战")})");
        }
    }

    /// <summary>网络消息处理: 主机收分机指令; 分机收日志广播。</summary>
    private void OnNetBattleMsg(int fromPeer, int msgId, string payload)
    {
        if (NetBridge == null) return;
        if (NetIsHost)
        {
            // 主机: 执行分机指令
            if ((Net.NetMsg)msgId == Net.NetMsg.Action)
            {
                var m = payload.Split(':');
                if (m.Length < 2) return;
                switch (m[0])
                {
                    case "tactic":
                        if (int.TryParse(m[1], out int side) && int.TryParse(m[2], out int t))
                            _battle.SetTactic(side, (Engine.Tactic)t);
                        break;
                    case "attr":
                        if (int.TryParse(m[1], out int sa) && int.TryParse(m[2], out int idx))
                        {
                            _battle.PickMainAttr(sa, idx);
                            if (_battle.MainAttr[1] >= 0 && _battle.MainAttr[2] >= 0 && !_battle.MainAttrRolled)
                            {
                                _battle.RollRandAttr();
                                _battle.MainAttrRolled = true;
                            }
                        }
                        break;
                    case "next":
                        NextPhase();
                        break;
                    case "cast":
                        // 分机发动技能: cast:{side}:{resName}
                        if (int.TryParse(m[1], out int castSide))
                        {
                            string resName = m.Length >= 3 ? m[2] : "";
                            var sideList = castSide == 1 ? _leftSide : _rightSide;
                            foreach (var cu in sideList)
                            {
                                ResourceDef found = null;
                                foreach (var s in cu.Skills) if (s.CardName == resName) { found = s; break; }
                                if (found == null)
                                    foreach (var p in cu.Phantasms) if (p.CardName == resName) { found = p; break; }
                                if (found == null)
                                    foreach (var it in cu.Items) if (it.CardName == resName) { found = it; break; }
                                if (found != null)
                                {
                                    CastBy(cu, found);
                                    break;
                                }
                            }
                        }
                        break;
                    case "castlist":
                        // 分机请求该侧可发动列表
                        if (int.TryParse(m[1], out int csSide))
                            BroadcastCastList(csSide);
                        break;
                    case "order":
                        if (int.TryParse(m[1], out int so))
                        {
                            Engine.Order ord = Engine.Order.Charge;
                            if (m.Length >= 3)
                            {
                                string on = m[2].Replace("Engine.Order.", "");
                                if (on.Contains("Duel")) ord = Engine.Order.Duel;
                                else if (on.Contains("Pursue")) ord = Engine.Order.Pursue;
                                else if (on.Contains("Cover")) ord = Engine.Order.Cover;
                            }
                            _battle.IssueOrder(so, ord);
                        }
                        break;
                }
                RefreshStatus();
                // 广播最新状态给分机
                BroadcastBattleState();
            }
        }
        else
        {
            // 分机: 主机广播的日志/状态
            if ((Net.NetMsg)msgId == Net.NetMsg.WorldSnapshot)
            {
                var parts = payload.Split('|');
                if (parts.Length >= 2)
                {
                    _log.Clear();
                    _log.AppendText(parts[0] + "\n");
                    _statusLabel.Text = parts[1];
                }
                else
                {
                    AppendLog("[联机] " + payload);
                }
            }
            else if ((Net.NetMsg)msgId == Net.NetMsg.Error && payload.StartsWith("castlist:"))
            {
                RenderRemoteCastList(payload.Substring("castlist:".Length));
            }
            else if ((Net.NetMsg)msgId == Net.NetMsg.HelloAck)
            {
                AppendLog("[联机] 主机确认连接");
                // 请求我方可发动列表
                if (RemoteSide == 1 || RemoteSide == 2)
                    NetBridge.SendTo(RemotePeerId, Net.NetMsg.Action, $"castlist:{RemoteSide}");
            }
        }
    }

    /// <summary>主机广播战斗日志与状态。</summary>
    private void BroadcastBattleState()
    {
        if (NetBridge == null || !NetBridge.Active || !NetIsHost) return;
        string logText = _log.Text ?? "";
        NetBridge.Broadcast(Net.NetMsg.WorldSnapshot, logText + "|" + _statusLabel.Text);
    }

    /// <summary>主机广播某侧可发动技能/宝具/礼装列表(分机渲染遥控按钮)。</summary>
    private void BroadcastCastList(int side)
    {
        if (NetBridge == null || !NetBridge.Active || !NetIsHost) return;
        var sideList = side == 1 ? _leftSide : _rightSide;
        if (sideList.Count == 0) return;
        var items = new System.Collections.Generic.List<string>();
        foreach (var cu in sideList)
        {
            foreach (var s in cu.Skills)
                if (!s.IsPassive) items.Add($"{cu.Name}:[技能]{s.CardName}");
            foreach (var p in cu.Phantasms) items.Add($"{cu.Name}:[宝具]{p.CardName}");
            foreach (var it in cu.Items) items.Add($"{cu.Name}:[礼装]{it.CardName}");
        }
        if (items.Count > 0)
            NetBridge.Broadcast(Net.NetMsg.Error, "castlist:" + side + ":" + string.Join(";", items));
    }

    /// <summary>分机渲染远程技能按钮(来自主机 castlist 广播)。</summary>
    private void RenderRemoteCastList(string payload)
    {
        // payload = "side:unit:[技能]Name;..."
        var sep = payload.IndexOf(':');
        if (sep <= 0) return;
        if (!int.TryParse(payload.Substring(0, sep), out int side)) return;
        string list = payload.Substring(sep + 1);
        if (side != RemoteSide) return;   // 只显示自己操控侧
        _actionBox.AddChild(new HSeparator());
        _actionBox.AddChild(new Label { Text = $"{(side == 1 ? "左方" : "右方")} 可发动能力(点击提交主机):" });
        foreach (var entry in list.Split(';'))
        {
            if (string.IsNullOrEmpty(entry)) continue;
            int colon = entry.IndexOf(":");
            if (colon <= 0) continue;
            string unitName = entry.Substring(0, colon);
            string resName = entry.Substring(colon + 1);
            var btn = new Button
            {
                Text = $"{resName} ({unitName})",
                CustomMinimumSize = new Vector2(0, 30),
                SizeFlagsHorizontal = SizeFlags.ExpandFill,
            };
            string rn = resName.IndexOf(']') >= 0 ? resName.Substring(resName.IndexOf(']') + 1) : resName;
            btn.Pressed += () =>
            {
                NetBridge?.SendTo(RemotePeerId, Net.NetMsg.Action, $"cast:{side}:{rn}");
            };
            _actionBox.AddChild(btn);
        }
    }

    /// <summary>战斗开始时结算全部常驻效果(发动时机:常驻 的技能/宝具/礼装)。
    /// 常驻效果立即对自身/己方/敌方生效(属性补正、胜率补正、状态赋予等)。</summary>
    private void SettlePassives()
    {
        AppendLog("━━ 常驻效果结算(发动时机:常驻) ━━");
        void ApplyAll(UnitDef u, int side)
        {
            foreach (ResourceDef s in u.Skills)
                if (s.IsPassive)
                {
                    Engine.KsgEffects.ApplyResource(_battle, _world, u, s, 0);
                    AppendLog($"  · {u.Name} 常驻技能「{s.CardName}」生效");
                }
            foreach (ResourceDef p in u.Phantasms)
                if (p.IsPassive)
                {
                    Engine.KsgEffects.ApplyResource(_battle, _world, u, p, 0);
                    AppendLog($"  · {u.Name} 常驻宝具「{p.CardName}」生效");
                }
            foreach (ResourceDef it in u.Items)
                if (it.IsPassive)
                {
                    Engine.KsgEffects.ApplyResource(_battle, _world, u, it, 0);
                    AppendLog($"  · {u.Name} 常驻礼装「{it.CardName}」生效");
                }
        }
        foreach (UnitDef u in _leftSide) ApplyAll(u, 1);
        foreach (UnitDef u in _rightSide) ApplyAll(u, 2);
        RefreshStatus();
    }

    public override void _ExitTree()
    {
        Engine.KsgEffects.LogEvent -= AppendLog;
        Engine.KsgEffects.VerboseEvent -= AppendLog;
        Engine.KsgEffects.UnitKilled -= OnUnitKilled;
        if (_world != null) _world.LogEvent -= AppendLog;
        if (_battle != null) _battle.LogEvent -= AppendLog;
        if (NetBridge != null && NetBridge.Active) NetBridge.OnMsg -= OnNetBattleMsg;
    }

    private void OnUnitKilled(UnitDef killer, UnitDef victim)
    {
        if (_battle == null) return;
        _battle.FireKillTriggers(_world, killer != null ? killer.BattleSide : 0);
    }

    private void AppendLog(string msg)
    {
        _log.AppendText(msg + "\n");
    }

    private void RefreshStatus()
    {
        var text = new System.Text.StringBuilder();
        text.AppendLine("【左方】" + StatusLine(_leftSide));
        text.AppendLine("【右方】" + StatusLine(_rightSide));
        _statusLabel.Text = text.ToString();
    }

    private static string StatusLine(System.Collections.Generic.List<UnitDef> side)
    {
        var parts = new System.Collections.Generic.List<string>();
        foreach (UnitDef u in side)
            parts.Add($"{u.Name} Lv{u.Level} 魔力{u.MpCur}/{u.MpCap} [{u.AttrBrief()}] 状态:{u.StatusBrief()}");
        return string.Join(" | ", parts);
    }

    // ---------- 阶段 ----------

    private void SetPhase(BattlePhase phase)
    {
        _phase = phase;
        string label = phase switch
        {
            BattlePhase.Tactic => "阶段:战斗开始(战术)",
            BattlePhase.Initial => "阶段:初始工序",
            BattlePhase.Main => "阶段:主要工序",
            BattlePhase.Final => "阶段:最终工序",
            BattlePhase.Resolve => "阶段:决胜检定",
            _ => "阶段:结束",
        };
        var stageLabel = GetNodeOrNull<Label>("StageLabel");
        if (stageLabel == null)
        {
            stageLabel = new Label { Name = "StageLabel", ThemeTypeVariation = "SectionTitle", CustomMinimumSize = new Vector2(0, 26) };
            AddChild(stageLabel);
            MoveChild(stageLabel, 0);
        }
        stageLabel.Text = label;
    }

    /// <summary>阶段门控:非当前允许阶段返回 false</summary>
    private bool InPhase(BattlePhase phase)
    {
        bool ok = _phase == phase;
        if (!ok) AppendLog("（当前阶段不允许此操作）");
        return ok;
    }

    private void PhaseTactics()
    {
        SetPhase(BattlePhase.Tactic);
        _actionBox.ClearChildren();
        AppendLog("━━ 战斗开始时:选择战术(强击>破袭>试探>扼守) ━━");
        if (RemoteOnly)
        {
            if (RemoteSide == 1) AddTacticButtons("左方", 1);
            else if (RemoteSide == 2) AddTacticButtons("右方", 2);
            else _actionBox.AddChild(new Label { Text = "(观战: 等待双方选择战术…)", ThemeTypeVariation = "Muted" });
        }
        else
        {
            AddTacticButtons("左方", 1);
            AddTacticButtons("右方", 2);
        }
        _nextBtn.Text = "选择完成后 →";
        _nextBtn.Disabled = true;
    }

    private void AddTacticButtons(string label, int side)
    {
        _actionBox.AddChild(new Label { Text = label + "战术:" });
        foreach (var t in new[] { Engine.Tactic.Strike, Engine.Tactic.Raid, Engine.Tactic.Probe, Engine.Tactic.Hold })
        {
            var btn = new Button { Text = Engine.TacticUtil.Name(t), CustomMinimumSize = new Vector2(120, 36) };
            btn.Pressed += () =>
            {
                if (RemoteCtrl(side))
                {
                    // 分机遥控: 指令发给主机
                    NetBridge?.SendTo(RemotePeerId, Net.NetMsg.Action, $"tactic:{side}:{(int)t}");
                    return;
                }
                _battle.SetTactic(side, t);
                RefreshStatus();
                // 双方都选完各自战术后才可前进(克制奖励只结算一次,由引擎保证)
                _nextBtn.Disabled = !(_battle.Tactics[1] != Engine.Tactic.None
                                      && _battle.Tactics[2] != Engine.Tactic.None);
            };
            _actionBox.AddChild(btn);
        }
    }

    /// <summary>分机是否遥控该侧(分机且该侧为自己操控)。</summary>
    private bool RemoteCtrl(int side)
    {
        if (NetBridge == null || !NetBridge.Active) return false;
        if (NetIsHost) return false;            // 主机本地操作
        return RemoteSide == side;              // 分机: 操作自己的侧
    }

    private void NextPhase()
    {
        // 分机: 遥控侧完成当前阶段后, 提交"下一步"给主机(主机决定推进)
        if (RemoteCtrl(_phase switch
            {
                BattlePhase.Tactic => 1,
                _ => RemoteSide,
            }))
        {
            NetBridge?.SendTo(RemotePeerId, Net.NetMsg.Action, "next");
            return;
        }
        switch (_phase)
        {
            case BattlePhase.Tactic:
                if (_battle.Tactics[1] == Engine.Tactic.None || _battle.Tactics[2] == Engine.Tactic.None)
                {
                    AppendLog("请双方向各选完战术");
                    return;
                }
                PhaseInitial();
                break;
            case BattlePhase.Initial:
                if (!_battle.MainAttrSet())
                {
                    AppendLog("请先选择主要属性");
                    return;
                }
                _battle.BatteryCheck();
                // 进入主要工序: 蓄力-1等机制结算
                Engine.KsgEffects.TickCharges(_battle, _world);
                _nextBtn.Disabled = true;
                PhaseMain();
                break;
            case BattlePhase.Main:
                Engine.KsgEffects.TickCharges(_battle, _world);
                PhaseFinal();
                break;
            case BattlePhase.Final:
                PhaseResolve();
                break;
            default:
                break;
        }
        RefreshStatus();
    }

    private void PhaseInitial()
    {
        SetPhase(BattlePhase.Initial);
        _usedThisPhase.Clear();
        _castButtons.Clear();
        _actionBox.ClearChildren();
        AppendLog("━━ 初始工序:选择主要属性; 同时可发动技能/宝具/礼装、指令(每工序限一次) ━━");
        if (RemoteOnly)
        {
            if (RemoteSide == 1) { AddAttrButtons("左方", 1); AddCastSection("左方", 1); }
            else if (RemoteSide == 2) { AddAttrButtons("右方", 2); AddCastSection("右方", 2); }
            else _actionBox.AddChild(new Label { Text = "(观战: 等待双方选择…)", ThemeTypeVariation = "Muted" });
            AddOrderButtons();
        }
        else
        {
            AddAttrButtons("左方", 1);
            AddAttrButtons("右方", 2);
            AddCastSection("左方", 1);
            AddCastSection("右方", 2);
            AddOrderButtons();
        }
        _nextBtn.Text = "确定并进入主要工序 →";
        _nextBtn.Disabled = true;
        // 主机: 广播可发动列表(分机遥控)
        if (NetBridge != null && NetBridge.Active && NetIsHost)
        {
            if (RemoteSide == 1 || RemoteSide == 2) BroadcastCastList(RemoteSide);
        }
    }

    private void AddAttrButtons(string label, int side)
    {
        _actionBox.AddChild(new Label { Text = label + "主要属性:" });
        for (int a = 0; a < 6; a++)
        {
            int attr = a;
            var btn = new Button { Text = Engine.KsgBattle.AttrName(attr), CustomMinimumSize = new Vector2(100, 32) };
            btn.Pressed += () =>
            {
                if (RemoteCtrl(side))
                {
                    NetBridge?.SendTo(RemotePeerId, Net.NetMsg.Action, $"attr:{side}:{attr}");
                    return;
                }
                _battle.PickMainAttr(side, attr);
                if (!_battle.MainAttrRolled && _battle.MainAttr[1] >= 0 && _battle.MainAttr[2] >= 0)
                {
                    _battle.RollRandAttr();
                    _battle.MainAttrRolled = true;
                }
                _nextBtn.Disabled = !_battle.MainAttrSet();
                RefreshStatus();
            };
            _actionBox.AddChild(btn);
        }
    }

    private void PhaseMain()
    {
        SetPhase(BattlePhase.Main);
        _usedThisPhase.Clear();
        _castButtons.Clear();
        _actionBox.ClearChildren();
        AppendLog("━━ 主要工序:双方触发技能/宝具(可多选) ━━");
        if (RemoteOnly)
        {
            if (RemoteSide == 1) AddCastSection("左方", 1);
            else if (RemoteSide == 2) AddCastSection("右方", 2);
            else _actionBox.AddChild(new Label { Text = "(观战: 等待双方发动能力…)", ThemeTypeVariation = "Muted" });
            AddOrderButtons();
        }
        else
        {
            AddCastSection("左方", 1);
            AddCastSection("右方", 2);
            AddOrderButtons();
        }
        _nextBtn.Text = "进入最终工序 →";
        _nextBtn.Disabled = false;
        // 主机: 向分机广播本侧可发动列表(分机遥控)
        if (NetBridge != null && NetBridge.Active && NetIsHost)
        {
            if (RemoteSide == 1 || RemoteSide == 2) BroadcastCastList(RemoteSide);
        }
    }

    private void AddCastSection(string label, int side)
    {
        var sideUnits = side == 1 ? _leftSide : _rightSide;
        if (RemoteOnly && sideUnits.Count == 0)
        {
            _actionBox.AddChild(new HSeparator());
            _actionBox.AddChild(new Label { Text = $"{label}: 技能/宝具将显示于主机广播(遥控已接通)" });
            return;
        }
        foreach (UnitDef u in sideUnits)
        {
            _actionBox.AddChild(new HSeparator());
            _actionBox.AddChild(new Label { Text = $"{label} · {u.Name}:可用能力(点击发动)" });
            UnitDef unit = u;   // 闭包捕获
            foreach (var s in u.Skills)
            {
                if (s.IsPassive) continue;
                var btn = new Button
                {
                    Text = $"[技能] {s.CardName}{RankUtil.Name(s.Rank)} 魔{s.Cost}/回{s.Recast}",
                    CustomMinimumSize = new Vector2(0, 32),
                    SizeFlagsHorizontal = SizeFlags.ExpandFill,
                };
                var r = s;
                _castButtons[btn] = (unit: unit, res: r);
                btn.Pressed += () => CastBy(unit, r);
                _actionBox.AddChild(btn);
            }
            foreach (var p in u.Phantasms)
            {
                var btn = new Button
                {
                    Text = $"[宝具] {p.CardName}{RankUtil.Name(p.Rank)} 魔{p.Cost}/回{p.Recast}",
                    CustomMinimumSize = new Vector2(0, 32),
                    SizeFlagsHorizontal = SizeFlags.ExpandFill,
                };
                var r = p;
                _castButtons[btn] = (unit: unit, res: r);
                btn.Pressed += () => CastBy(unit, r);
                _actionBox.AddChild(btn);
            }
            foreach (var it in u.Items)
            {
                var btn = new Button
                {
                    Text = $"[礼装] {it.Name}",
                    CustomMinimumSize = new Vector2(0, 28),
                    SizeFlagsHorizontal = SizeFlags.ExpandFill,
                };
                var r = it;
                _castButtons[btn] = (unit: unit, res: r);
                btn.Pressed += () => CastBy(unit, r);
                _actionBox.AddChild(btn);
            }
        }
    }

    private void AddOrderButtons()
    {
        _actionBox.AddChild(new Label { Text = "指令(主力):" });
        var row = new HBoxContainer();
        var c1 = new Button { Text = "左方冲锋" };
        c1.Pressed += () => { if (RemoteCtrl(1)) { NetBridge?.SendTo(RemotePeerId, Net.NetMsg.Action, "order:1:Engine.Order.Charge"); } else { _battle.IssueOrder(1, Engine.Order.Charge); RefreshStatus(); } };
        row.AddChild(c1);
        var c2 = new Button { Text = "右方冲锋" };
        c2.Pressed += () => { if (RemoteCtrl(2)) { NetBridge?.SendTo(RemotePeerId, Net.NetMsg.Action, "order:2:Engine.Order.Charge"); } else { _battle.IssueOrder(2, Engine.Order.Charge); RefreshStatus(); } };
        row.AddChild(c2);
        var d1 = new Button { Text = "左方死斗" };
        d1.Pressed += () => { if (RemoteCtrl(1)) { NetBridge?.SendTo(RemotePeerId, Net.NetMsg.Action, "order:1:Engine.Order.Duel"); } else { _battle.IssueOrder(1, Engine.Order.Duel); RefreshStatus(); } };
        row.AddChild(d1);
        var d2 = new Button { Text = "右方死斗" };
        d2.Pressed += () => { if (RemoteCtrl(2)) { NetBridge?.SendTo(RemotePeerId, Net.NetMsg.Action, "order:2:Engine.Order.Duel"); } else { _battle.IssueOrder(2, Engine.Order.Duel); RefreshStatus(); } };
        row.AddChild(d2);
        var p1 = new Button { Text = "左方追击" };
        p1.Pressed += () => { if (RemoteCtrl(1)) { NetBridge?.SendTo(RemotePeerId, Net.NetMsg.Action, "order:1:Engine.Order.Pursue"); } else { _battle.IssueOrder(1, Engine.Order.Pursue); RefreshStatus(); } };
        row.AddChild(p1);
        var p2 = new Button { Text = "右方追击" };
        p2.Pressed += () => { if (RemoteCtrl(2)) { NetBridge?.SendTo(RemotePeerId, Net.NetMsg.Action, "order:2:Engine.Order.Pursue"); } else { _battle.IssueOrder(2, Engine.Order.Pursue); RefreshStatus(); } };
        row.AddChild(p2);
        var cover1 = new Button { Text = "左方掩护" };
        cover1.Pressed += () => { if (RemoteCtrl(1)) { NetBridge?.SendTo(RemotePeerId, Net.NetMsg.Action, "order:1:Engine.Order.Cover"); } else { _battle.IssueOrder(1, Engine.Order.Cover); RefreshStatus(); } };
        row.AddChild(cover1);
        var cover2 = new Button { Text = "右方掩护" };
        cover2.Pressed += () => { if (RemoteCtrl(2)) { NetBridge?.SendTo(RemotePeerId, Net.NetMsg.Action, "order:2:Engine.Order.Cover"); } else { _battle.IssueOrder(2, Engine.Order.Cover); RefreshStatus(); } };
        row.AddChild(cover2);
        // 撤退
        var r1 = new Button { Text = "左方撤退" };
        r1.Pressed += () => { _battle.Retreat(1); RefreshStatus(); };
        row.AddChild(r1);
        var r2 = new Button { Text = "右方撤退" };
        r2.Pressed += () => { _battle.Retreat(2); RefreshStatus(); };
        row.AddChild(r2);
        _actionBox.AddChild(row);
    }

    private void CastBy(UnitDef u, ResourceDef r)
    {
        if (!InPhase(BattlePhase.Initial) && !InPhase(BattlePhase.Main) && !InPhase(BattlePhase.Final))
            return;
        if (_castPending)
        {
            AppendLog("请先确认或取消当前的状态层数输入");
            return;
        }
        if (!CanCast(u, r)) return;

        var manualEffectIndices = new List<int>();
        for (int i = 0; i < r.Effects.Count; i++)
        {
            EffectLine effect = r.Effects[i];
            if (effect.Flag == EffFlag.StatusGive && effect.Status > 0 &&
                StatusUtil.UsesLayerCount((StatusKind)effect.Status))
                manualEffectIndices.Add(i);
        }

        if (manualEffectIndices.Count > 0)
        {
            AskManualLayers(u, r, manualEffectIndices);
            return;
        }
        AskAndCommitCast(u, r);
    }

    /// <summary>目标选择(自身/敌方主力/敌方全体)后发动</summary>
    private void AskAndCommitCast(UnitDef unit, ResourceDef resource)
    {
        _castPending = true;
        _nextWasDisabled = _nextBtn.Disabled;
        _nextBtn.Disabled = true;

        var popup = new ConfirmationDialog
        {
            Title = "目标选择与发动确认",
            DialogText = $"{resource.Name}：选择目标后发动。",
            Exclusive = true,
        };
        AddChild(popup);
        popup.GetOkButton().Text = "发动";

        var opt = new OptionButton { CustomMinimumSize = new Vector2(340, 0) };
        opt.AddItem("自身");
        opt.AddItem("敌方主力位");
        opt.AddItem("敌方全体");
        opt.AddItem("己方全体");
        opt.Selected = 0;
        var box = new VBoxContainer();
        box.AddChild(new HSeparator());
        box.AddChild(new Label { Text = "目标:" });
        box.AddChild(opt);
        popup.AddChild(box);

        bool resolved = false;
        popup.Confirmed += () =>
        {
            if (resolved) return;
            resolved = true;
            int overrideTarget = opt.Selected switch
            {
                1 => 1,            // 敌方第1(主力)
                2 => 0,            // 敌方全体
                3 => -1,           // 己方全体
                _ => -2,           // 自身
            };
            EndPendingCast();
            CommitCastWithTarget(unit, resource, null, overrideTarget);
            popup.QueueFree();
        };
        popup.Canceled += () =>
        {
            if (resolved) return;
            resolved = true;
            EndPendingCast();
            AppendLog($"已取消发动 {resource.Name}");
            popup.QueueFree();
        };
        popup.PopupCentered(new Vector2I(420, 180));
    }

    /// <summary>指定目标发动(供目标选择)</summary>
    private void CommitCastWithTarget(UnitDef unit, ResourceDef resource,
        IReadOnlyDictionary<int, int> layerOverrides, int targetOverride)
    {
        if (!CanCast(unit, resource)) return;
        ResourceDef effective = layerOverrides == null
            ? resource
            : resource.WithLayerOverrides(layerOverrides);
        // 工房等级修正(制裁机关/增幅模块)
        _battleAddLog = null;
        effective = ApplyWorkshopRankMod(unit, effective);
        if (effective != resource && _battleAddLog != null)
            AppendLog(_battleAddLog);
        if (!Engine.KsgEffects.CastResource(_battle, _world, unit, effective, targetOverride, out string why))
        {
            AppendLog($"✖ 发动失败: {why}");
            return;
        }
        _usedThisPhase.Add(unit.Id + ":" + resource.Name);
        AppendLog($"✦ {unit.Name} 发动 {resource.Name}[{RankUtil.Name(resource.Rank)}] 魔耗{resource.Cost} 目标={TargetName2(targetOverride)}");

        foreach (var s in unit.Skills) s.TickProcRecast();
        foreach (var p in unit.Phantasms) p.TickProcRecast();
        RefreshStatus();
        RefreshCastButtons();
    }

    private static string TargetName2(int target) => target switch
    {
        -2 => "自身",
        -1 => "己方全体",
        0 => "敌方全体",
        1 => "敌方主力位",
        _ => $"敌方第{target}位",
    };

    private string _battleAddLog;

    /// <summary>工房等级修正: 制裁机关(敌方技能/宝具效果降1级) / 增幅模块(己方魔术技能升1级)。
    /// 返回修正后的资源(不修改原卡)。</summary>
    private ResourceDef ApplyWorkshopRankMod(UnitDef unit, ResourceDef resource)
    {
        if (_battle == null || (EnemySkillLevelDown[1] == 0 && EnemySkillLevelDown[2] == 0
            && AmplifySelfMagic == 0)) return resource;
        int side = unit.BattleSide == 1 ? 1 : 2;
        // 制裁机关: 敌方(相对工房主)技能/宝具等级-1
        bool isEnemy = unit.BattleSide != 1 && EnemySkillLevelDown[2] > 0;
        if (_battle.Left.Count > 0 && _battle.Right.Count > 0)
        {
            var owner = _battle.Left[0].Unit;   // 工房主=袭击方(左)
            isEnemy = owner.Id != unit.Id && EnemySkillLevelDown[2] > 0;
        }
        if (isEnemy && resource.Rank > Rank.E && resource.Rank <= Rank.A)
        {
            Rank lower = resource.Rank - 1;
            var clone = resource.DeepClone(lower);
            _battleAddLog = $"[制裁机关] {unit.Name} 的 {resource.Name} 效果等级降为 {RankUtil.Name(lower)}!";
            return clone;
        }
        // 增幅模块: 己方类型:魔术技能等级+1
        if (resource.Type == (int)SkillType.Magic && resource.Rank < Rank.A && resource.Kind == ResKind.Skill
            && AmplifySelfMagic > 0 && unit.BattleSide == 1)
        {
            Rank higher = resource.Rank + 1;
            var clone = resource.DeepClone(higher);
            _battleAddLog = $"[增幅模块] {unit.Name} 的 {resource.Name} 效果等级升为 {RankUtil.Name(higher)}!";
            return clone;
        }
        return resource;
    }

    private bool CanCast(UnitDef unit, ResourceDef resource)
    {
        if (resource.IsPassive) return false;
        string key = unit.Id + ":" + resource.Name;
        if (_usedThisPhase.Contains(key))
        {
            AppendLog($"✖ 本工序 {unit.Name} 已发动过 {resource.Name}(同资源每工序限一次)");
            return false;
        }
        if (resource.Cost > unit.MpCur)
        {
            AppendLog($"✖ {unit.Name} 魔力不足,无法发动 {resource.Name}");
            return false;
        }
        if (resource.Recast > 0 && resource.CurRecast < resource.Recast)
        {
            AppendLog($"✖ {unit.Name} 回转未完成({resource.CurRecast}/{resource.Recast}),无法发动 {resource.Name}");
            return false;
        }
        return true;
    }

    private void CommitCast(UnitDef unit, ResourceDef resource, IReadOnlyDictionary<int, int> layerOverrides)
    {
        // 对话框打开期间状态理论上不会变化，但提交前仍重新验证，避免未来异步操作引入竞态。
        if (!CanCast(unit, resource)) return;

        ResourceDef effective = layerOverrides == null
            ? resource
            : resource.WithLayerOverrides(layerOverrides);
        _battleAddLog = null;
        effective = ApplyWorkshopRankMod(unit, effective);
        if (effective != resource && _battleAddLog != null)
            AppendLog(_battleAddLog);
        if (!Engine.KsgEffects.CastResource(_battle, _world, unit, effective, 0, out string why))
        {
            AppendLog($"✖ 发动失败: {why}");
            return;
        }
        _usedThisPhase.Add(unit.Id + ":" + resource.Name);
        AppendLog($"✦ {unit.Name} 发动 {resource.Name}[{RankUtil.Name(resource.Rank)}] 魔耗{resource.Cost}");

        foreach (var s in unit.Skills) s.TickProcRecast();
        foreach (var p in unit.Phantasms) p.TickProcRecast();
        RefreshStatus();
        RefreshCastButtons();
    }

    /// <summary>刷新技能按钮可用性(回转/魔力/已用/封印等实时可见)</summary>
    private void RefreshCastButtons()
    {
        foreach (var pair in _castButtons)
        {
            Button b = pair.Key;
            UnitDef u = pair.Value.unit;
            ResourceDef r = pair.Value.res;
            string key = u.Id + ":" + r.Name;
            bool used = _usedThisPhase.Contains(key);
            bool ready = r.ReadyToCast && r.Cost <= u.MpCur && !used && !r.IsPassive;
            b.Disabled = !ready;
            string suffix = used ? "（已用）" : (r.ReadyToCast ? "" : $"（回转{r.CurRecast}/{r.Recast}）");
            string baseText = b.Text.Split('（')[0];
            b.Text = baseText + suffix;
        }
    }

    /// <summary>先收集每条可变状态的层数；确认后才支付魔力并执行一次完整资源效果。</summary>
    private void AskManualLayers(UnitDef unit, ResourceDef resource, List<int> effectIndices)
    {
        _castPending = true;
        _nextWasDisabled = _nextBtn.Disabled;
        _nextBtn.Disabled = true;

        var popup = new ConfirmationDialog
        {
            Title = "手动输入状态层数",
            DialogText = $"{resource.Name}：确认后才会消耗魔力并结算。层数 0 表示跳过该条状态效果。",
            Exclusive = true,
        };
        AddChild(popup);
        popup.GetOkButton().Text = "确认发动";

        var inputs = new Dictionary<int, SpinBox>();
        var box = new VBoxContainer();
        box.AddThemeConstantOverride("separation", 8);
        popup.AddChild(box);
        foreach (int index in effectIndices)
        {
            EffectLine effect = resource.Effects[index];
            StatusKind status = (StatusKind)effect.Status;
            int maximum = System.Math.Max(StatusUtil.ManualInputMax(status), effect.Layers);
            var row = new HBoxContainer();
            row.AddChild(new Label
            {
                Text = $"{StatusUtil.Name(status)}（{TargetName(effect.Target)}）:",
                CustomMinimumSize = new Vector2(260, 0),
            });
            var input = new SpinBox
            {
                MinValue = 0,
                MaxValue = maximum,
                Step = 1,
                Value = System.Math.Clamp(effect.Layers, 0, maximum),
                CustomMinimumSize = new Vector2(120, 0),
            };
            row.AddChild(input);
            box.AddChild(row);
            inputs[index] = input;
        }

        bool resolved = false;
        popup.Confirmed += () =>
        {
            if (resolved) return;
            resolved = true;
            var overrides = new Dictionary<int, int>();
            foreach (KeyValuePair<int, SpinBox> pair in inputs)
                overrides[pair.Key] = (int)pair.Value.Value;
            EndPendingCast();
            CommitCast(unit, resource, overrides);
            popup.QueueFree();
        };
        popup.Canceled += () =>
        {
            if (resolved) return;
            resolved = true;
            EndPendingCast();
            AppendLog($"已取消发动 {resource.Name}（未消耗魔力、未改变回转或状态）");
            popup.QueueFree();
        };
        popup.PopupCentered(new Vector2I(560, 220 + effectIndices.Count * 48));
    }

    private void EndPendingCast()
    {
        _castPending = false;
        _nextBtn.Disabled = _nextWasDisabled;
    }

    private static string TargetName(int target) => target switch
    {
        -2 => "自身",
        -1 => "己方全体",
        0 => "敌方全体",
        _ when target > 0 => $"敌方第 {target} 位",
        _ => "目标",
    };

    private void PhaseFinal()
    {
        SetPhase(BattlePhase.Final);
        _usedThisPhase.Clear();
        _castButtons.Clear();
        _actionBox.ClearChildren();
        AppendLog("━━ 最终工序:再次触发(同上," + "也可直接决胜) ━━");
        AddOrderButtons();
        _nextBtn.Text = "决胜检定 →";
        _nextBtn.Disabled = false;
    }

    private void PhaseResolve()
    {
        SetPhase(BattlePhase.Resolve);
        _actionBox.ClearChildren();
        AppendLog("━━ 决胜检定 ━━");
        _battle.Effective();
        _battle.End(_world);
        AppendLog("战斗结束。可再战或返回主菜单。");
        SetPhase(BattlePhase.Ended);
        _nextBtn.Text = "再战一场";
        _nextBtn.Pressed -= NextPhase;
        _nextBtn.Pressed += () => SceneRouter.GoBattleSelect(this);
        var again = new Button { Text = "返回主菜单" };
        _cmdRow.AddChild(again);
        again.Pressed += () => SceneRouter.GoMainMenu(this);
        RefreshStatus();
        // 通知战斗结果(供世界流程回写: 胜者/败者)
        Finished?.Invoke(new BattleResult { LeftWin = _battle.LeftOk, RightWin = _battle.RightOk });
    }

    /// <summary>战斗结果(供世界遭遇战回调)。</summary>
    public class BattleResult
    {
        public bool LeftWin;
        public bool RightWin;
    }

    /// <summary>战斗结算完成时触发(世界流程订阅)。</summary>
    public event Action<BattleResult> Finished;
}

internal static class NodeEx
{
    public static void ClearChildren(this Node n)
    {
        foreach (Node c in n.GetChildren())
            if (c is Control con && !con.Name.ToString().StartsWith("_"))
                continue;
        foreach (Node c in new System.Collections.Generic.List<Node>(n.GetChildren()))
            c.QueueFree();
    }
}
