using Godot;

namespace KsgGodot.UI;

/// <summary>场景跳转(代码构建切换)</summary>
public static class SceneRouter
{
    public static void GoCardBuilder(Node from)
    {
        var s = new CardBuilderScene();
        Switch(from, s);
    }

    public static void GoUnitList(Node from)
    {
        var s = new UnitListScene();
        Switch(from, s);
    }

    public static void GoBattleSelect(Node from)
    {
        var s = new BattleSelectScene();
        Switch(from, s);
    }

    public static void GoDictionary(Node from)
    {
        var s = new DictionaryScene();
        Switch(from, s);
    }

    /// <summary>大航海战斗表(结算流程展示 / GM 计算器)。</summary>
    public static void GoSeaTable(Node from)
    {
        var s = new SeaTableScene();
        Switch(from, s);
    }

    public static void GoBattle(Node from, Model.UnitDef left, Model.UnitDef right, int width = 4)
    {
        var s = new BattleScene(left, right, width);
        Switch(from, s);
    }

    /// <summary>多单位对战(每方可上多名单位)。</summary>
    public static void GoBattleParty(Node from,
        System.Collections.Generic.List<Model.UnitDef> leftSide,
        System.Collections.Generic.List<Model.UnitDef> rightSide,
        int width)
    {
        var s = new BattleScene(leftSide, rightSide, width);
        Switch(from, s);
    }

    public static void GoMainMenu(Node from)
    {
        var s = new MainMenu();
        Switch(from, s);
    }

    /// <summary>圣杯战争世界(单人全控)。</summary>
    public static void GoWorld(Node from)
    {
        var s = new WorldScene();
        Switch(from, s);
    }

    /// <summary>圣杯战争世界(联机: joinHost=null 时为主机)。</summary>
    public static void GoWorld(Node from, int port, string joinHost)
    {
        var s = new WorldScene { NetworkPort = port, JoinHost = joinHost };
        Switch(from, s);
    }

    /// <summary>联机大厅。</summary>
    public static void GoLobby(Node from)
    {
        var s = new LobbyScene();
        Switch(from, s);
    }

    private static void Switch(Node from, Control next)
    {
        if (from == null || next == null || !GodotObject.IsInstanceValid(from) ||
            !from.IsInsideTree() || from.IsQueuedForDeletion() || from.HasMeta("scene_switching"))
        {
            if (next != null && GodotObject.IsInstanceValid(next)) next.Free();
            return;
        }

        // 同一按钮在一帧内被连续触发时只允许一次切换，避免根节点残留重复页面。
        from.SetMeta("scene_switching", true);
        from.ProcessMode = Node.ProcessModeEnum.Disabled;
        if (from is CanvasItem canvas) canvas.Hide();
        var root = from.GetTree().Root;
        root.AddChild(next);
        from.GetTree().CurrentScene = next;
        if (from != root && from.IsInsideTree())
            from.QueueFree();
    }
}
