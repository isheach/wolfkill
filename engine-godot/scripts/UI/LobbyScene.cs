using System;
using Godot;

namespace KsgGodot.UI;

/// <summary>联机大厅: 创建房间(主机) 或 连接房间(分机)</summary>
public partial class LobbyScene : Control
{
    private LineEdit _portEdit, _hostEdit;
    private Label _info;

    public override void _Ready()
    {
        AppTheme.Apply(this);
        var margin = new MarginContainer { AnchorRight = 1, AnchorBottom = 1 };
        margin.AddThemeConstantOverride("margin_left", 16);
        margin.AddThemeConstantOverride("margin_right", 16);
        margin.AddThemeConstantOverride("margin_top", 10);
        AddChild(margin);

        var root = new VBoxContainer();
        root.AddThemeConstantOverride("separation", 10);
        margin.AddChild(root);

        root.AddChild(AppTheme.MakeHeader("联机大厅", () => SceneRouter.GoMainMenu(this)));
        _info = new Label
        {
            Text = "主机: 创建房间并完全掌控世界规则判定。\n分机: 输入主机IP连入, 操控自己的御主/从者。",
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
            ThemeTypeVariation = "Muted",
        };
        root.AddChild(_info);

        // 端口
        var portRow = new HBoxContainer();
        portRow.AddChild(new Label { Text = "端口:", CustomMinimumSize = new Vector2(70, 0) });
        _portEdit = new LineEdit { Text = "31415", CustomMinimumSize = new Vector2(160, 0) };
        portRow.AddChild(_portEdit);
        root.AddChild(portRow);

        // 主机
        var hostBtn = new Button { Text = "创建房间(主机)", CustomMinimumSize = new Vector2(0, 44) };
        hostBtn.Pressed += () =>
        {
            int port = int.TryParse(_portEdit.Text, out int p) ? p : 31415;
            SceneRouter.GoWorld(this, port: port, joinHost: null);
        };
        root.AddChild(hostBtn);

        // 分机
        var hostRow = new HBoxContainer();
        root.AddChild(hostRow);
        hostRow.AddChild(new Label { Text = "主机IP:", CustomMinimumSize = new Vector2(70, 0) });
        _hostEdit = new LineEdit { Text = "127.0.0.1", CustomMinimumSize = new Vector2(200, 0), SizeFlagsHorizontal = SizeFlags.ExpandFill };
        hostRow.AddChild(_hostEdit);

        var joinBtn = new Button { Text = "连接主机(分机)", CustomMinimumSize = new Vector2(0, 44) };
        joinBtn.Pressed += () =>
        {
            int port = int.TryParse(_portEdit.Text, out int p) ? p : 31415;
            string host = _hostEdit.Text.Trim();
            if (string.IsNullOrEmpty(host)) host = "127.0.0.1";
            SceneRouter.GoWorld(this, port: port, joinHost: host);
        };
        root.AddChild(joinBtn);

        root.AddChild(new Label
        {
            Text = "提示: 主机与分机须在可互通的网络(同一局域网通常即可)。",
            ThemeTypeVariation = "Muted",
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
        });
    }
}