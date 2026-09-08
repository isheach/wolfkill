using Godot;
using System;

namespace KsgGodot.UI;

/// <summary>全局 UI 主题与页面级公共控件；所有页面均由代码构建，不依赖 Inspector。</summary>
public static class AppTheme
{
    public const string AppVersion = "0.3.0-C";
    public const string DataDate = "2026-09-04";

    private static Theme _shared;
    public static Theme Shared => _shared ??= Create();

    public static void Apply(Control page)
    {
        page.Theme = Shared;
        page.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
    }

    public static HBoxContainer MakeHeader(string title, Action goBack)
    {
        var row = new HBoxContainer { CustomMinimumSize = new Vector2(0, 48) };
        row.AddChild(new Label
        {
            Text = title,
            ThemeTypeVariation = "PageTitle",
            SizeFlagsHorizontal = Control.SizeFlags.ExpandFill,
            TextOverrunBehavior = TextServer.OverrunBehavior.TrimEllipsis,
        });
        var back = new Button
        {
            Name = "BackToMainMenu",
            Text = "← 返回主菜单",
            ThemeTypeVariation = "BackButton",
            CustomMinimumSize = new Vector2(150, 40),
        };
        back.Pressed += goBack;
        row.AddChild(back);
        return row;
    }

    public static Button MakeDangerButton(string text)
    {
        return new Button { Text = text, ThemeTypeVariation = "DangerButton" };
    }

    private static Theme Create()
    {
        var theme = new Theme();
        var uiFont = new SystemFont
        {
            FontNames = new[]
            {
                "Microsoft YaHei UI", "Microsoft YaHei", "Noto Sans CJK SC",
                "Source Han Sans SC", "SimHei", "Arial Unicode MS"
            },
            FontWeight = 400,
        };
        var monoFont = new SystemFont
        {
            FontNames = new[]
            {
                "Cascadia Mono", "Sarasa Mono SC", "Noto Sans Mono CJK SC",
                "Microsoft YaHei UI", "Consolas"
            },
            FontWeight = 400,
        };

        foreach (string type in new[]
                 {
                     "Label", "Button", "LineEdit", "TextEdit", "OptionButton", "CheckButton",
                     "RichTextLabel", "ItemList", "TabContainer", "PopupMenu", "Window"
                 })
            theme.SetFont("font", type, uiFont);

        theme.SetDefaultFont(uiFont);
        theme.SetDefaultFontSize(16);
        theme.SetFontSize("font_size", "Label", 16);
        theme.SetFontSize("font_size", "Button", 16);
        theme.SetFontSize("font_size", "LineEdit", 16);
        theme.SetFontSize("font_size", "OptionButton", 16);
        theme.SetFontSize("font_size", "ItemList", 16);
        theme.SetFontSize("font_size", "RichTextLabel", 16);

        theme.SetTypeVariation("PageTitle", "Label");
        theme.SetTypeVariation("SectionTitle", "Label");
        theme.SetTypeVariation("Muted", "Label");
        theme.SetTypeVariation("DangerButton", "Button");
        theme.SetTypeVariation("BackButton", "Button");
        theme.SetTypeVariation("BattleLog", "RichTextLabel");

        theme.SetFontSize("font_size", "PageTitle", 27);
        theme.SetFontSize("font_size", "SectionTitle", 19);
        theme.SetColor("font_color", "PageTitle", new Color("f4e6bd"));
        theme.SetColor("font_color", "SectionTitle", new Color("dcc58d"));
        theme.SetColor("font_color", "Muted", new Color("aaa5b8"));
        theme.SetColor("font_color", "Label", new Color("eeeaf5"));
        theme.SetColor("font_color", "Button", new Color("f8f5ff"));
        theme.SetColor("font_hover_color", "Button", Colors.White);
        theme.SetColor("font_pressed_color", "Button", new Color("fff3c9"));
        theme.SetColor("font_disabled_color", "Button", new Color("777180"));

        theme.SetStylebox("normal", "Button", Box("2d2940", "514969"));
        theme.SetStylebox("hover", "Button", Box("403957", "b39bd7"));
        theme.SetStylebox("pressed", "Button", Box("5b4b78", "e0c27c"));
        theme.SetStylebox("disabled", "Button", Box("24212d", "3f3a49"));
        theme.SetStylebox("focus", "Button", Box("00000000", "ddc780", 2));

        theme.SetColor("font_color", "DangerButton", new Color("ffe9e9"));
        theme.SetColor("font_hover_color", "DangerButton", Colors.White);
        theme.SetStylebox("normal", "DangerButton", Box("672f39", "a84d5d"));
        theme.SetStylebox("hover", "DangerButton", Box("8c3b4b", "f08b9d"));
        theme.SetStylebox("pressed", "DangerButton", Box("4c222b", "ffb0bc"));
        theme.SetStylebox("disabled", "DangerButton", Box("35292d", "564249"));
        theme.SetStylebox("focus", "DangerButton", Box("00000000", "ff9bab", 2));

        theme.SetStylebox("normal", "BackButton", Box("283849", "55718e"));
        theme.SetStylebox("hover", "BackButton", Box("35506a", "91bde5"));
        theme.SetStylebox("pressed", "BackButton", Box("263b50", "c2def5"));

        theme.SetStylebox("panel", "PanelContainer", Box("1d1a28d9", "393347"));
        theme.SetStylebox("normal", "LineEdit", Box("191722", "494258"));
        theme.SetStylebox("focus", "LineEdit", Box("211d2d", "c0a968", 2));
        theme.SetStylebox("panel", "ItemList", Box("181620", "3c3648"));
        theme.SetStylebox("panel", "TabContainer", Box("181620", "3c3648"));

        theme.SetFont("normal_font", "BattleLog", monoFont);
        theme.SetFontSize("normal_font_size", "BattleLog", 15);
        theme.SetColor("default_color", "BattleLog", new Color("d7f0df"));

        theme.SetConstant("separation", "VBoxContainer", 8);
        theme.SetConstant("separation", "HBoxContainer", 8);
        theme.SetConstant("outline_size", "Label", 0);
        return theme;
    }

    private static StyleBoxFlat Box(string background, string border, int borderWidth = 1)
    {
        var box = new StyleBoxFlat
        {
            BgColor = new Color(background),
            BorderColor = new Color(border),
            CornerRadiusTopLeft = 6,
            CornerRadiusTopRight = 6,
            CornerRadiusBottomLeft = 6,
            CornerRadiusBottomRight = 6,
            ContentMarginLeft = 12,
            ContentMarginRight = 12,
            ContentMarginTop = 8,
            ContentMarginBottom = 8,
        };
        box.SetBorderWidthAll(borderWidth);
        return box;
    }
}
