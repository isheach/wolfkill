using Godot;

namespace KsgGodot.UI;

/// <summary>主菜单(纯代码构建)</summary>
public partial class MainMenu : Control
{
    public override void _Ready()
    {
        AppTheme.Apply(this);
        // 数据加载
        Data.ResourceDb.Load();
        // 存档目录: exe 旁可写则启用便携模式(旧存档自动迁移)
        Save.SaveManager.InitializePortable();
        Save.SaveManager.LoadAllIntoSessionOnce();
        int saveCount = Save.SaveManager.ListSaveIds().Length;
        string version = ProjectSettings.GetSetting(
            "application/config/version", AppTheme.AppVersion).AsString();
        string dataDate = ProjectSettings.GetSetting(
            "application/config/data_date", AppTheme.DataDate).AsString();

        var center = new CenterContainer { AnchorRight = 1, AnchorBottom = 1 };
        AddChild(center);
        var box = new VBoxContainer { CustomMinimumSize = new Vector2(520, 0) };
        box.AddThemeConstantOverride("separation", 12);
        center.AddChild(box);

        box.AddChild(new Label
        {
            Text = "空想圣杯 · Godot 版",
            ThemeTypeVariation = "PageTitle",
            HorizontalAlignment = HorizontalAlignment.Center,
            CustomMinimumSize = new Vector2(0, 60),
        });
        box.AddChild(new Label
        {
            Text = "KsG 五书重构 · 建卡 / 对战 / 词典",
            ThemeTypeVariation = "Muted",
            HorizontalAlignment = HorizontalAlignment.Center,
            CustomMinimumSize = new Vector2(0, 26),
        });
        box.AddChild(new Label
        {
            Name = "LibraryStatus",
            Text = $"资源库 {Data.ResourceDb.All.Count} 条　·　本地存档 {saveCount} 份　·　当前会话 {Save.CurrentSave.All.Count} 张",
            HorizontalAlignment = HorizontalAlignment.Center,
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
        });
        box.AddChild(new Label
        {
            Name = "VersionStatus",
            Text = $"版本 {version}　·　数据日期 {dataDate}",
            ThemeTypeVariation = "Muted",
            HorizontalAlignment = HorizontalAlignment.Center,
        });
        box.AddChild(new Control { CustomMinimumSize = new Vector2(0, 18) });

        AddButton(box, "建卡向导(搜索 / 滑条)", () => SceneRouter.GoCardBuilder(this));
        AddButton(box, "已建卡单位一览", () => SceneRouter.GoUnitList(this));
        AddButton(box, "圣杯战争(世界 · 单人全控)", () => SceneRouter.GoWorld(this), 56);
        AddButton(box, "联机大厅(主机 / 分机)", () => SceneRouter.GoLobby(this), 56);
        AddButton(box, "对战(双卡结算)", () => SceneRouter.GoBattleSelect(this));
        AddButton(box, "大航海战斗表(结算流程)", () => SceneRouter.GoSeaTable(this));
        AddButton(box, "词典查询(状态 / 特效)", () => SceneRouter.GoDictionary(this));
        AddButton(box, "退出", () => GetTree().Quit(), 44);
    }

    private static void AddButton(VBoxContainer parent, string text, System.Action cb, int h = 52)
    {
        var b = new Button { Text = text, CustomMinimumSize = new Vector2(0, h) };
        b.Pressed += cb;
        parent.AddChild(b);
    }
}
