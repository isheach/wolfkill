using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using Godot;
using KsgGodot.Model;

namespace KsgGodot.Save;

/// <summary>
/// 建卡存档：优先存到“游戏可执行文件旁 saves/”（随游戏文件夹一起分发、可携带）；
/// 开发/受限目录下自动回退到 user://saves（%APPDATA%）。旧 user:// 存档启动时自动迁移。
/// 同一 ID 再次保存即覆盖。
/// </summary>
public static class SaveManager
{
    /// <summary>可携带根目录（exe 旁）。若为空则不可写，回退 user://。</summary>
    public static string PortableRoot = "";

    public static bool PortableMode => PortableRoot.Length > 0;

    private static readonly object IdLock = new();

    /// <summary>存档目录绝对路径（不含尾部斜杠）。</summary>
    public static string SavesDir => PortableMode
        ? Path.Combine(PortableRoot, "saves")
        : ProjectSettings.GlobalizePath("user://saves");

    /// <summary>头像目录绝对路径。</summary>
    public static string AvatarsDir => PortableMode
        ? Path.Combine(PortableRoot, "avatars")
        : ProjectSettings.GlobalizePath("user://avatars");

    /// <summary>旧版 user:// 目录（用于迁移与兼容读取）。</summary>
    private static string LegacySavesDir => ProjectSettings.GlobalizePath("user://saves");

    private static string LegacyAvatarsDir => ProjectSettings.GlobalizePath("user://avatars");

    /// <summary>
    /// 启动时初始化：检测 exe 旁可写性；若可写则把旧 user:// 存档/头像迁移过去。
    /// </summary>
    public static void InitializePortable()
    {
        if (PortableMode) return;

        try
        {
            string exePath = OS.GetExecutablePath();
            string dir = Path.GetDirectoryName(exePath);
            if (string.IsNullOrEmpty(dir)) return;

            // 测试可写性：尝试创建 saves 目录
            string probe = Path.Combine(dir, "saves");
            Directory.CreateDirectory(probe);
            string testFile = Path.Combine(probe, ".write_test");
            File.WriteAllText(testFile, "1");
            File.Delete(testFile);

            PortableRoot = dir;
            GD.Print($"[存档] 便携模式启用：{PortableRoot}（存档随游戏文件夹携带）");

            // 迁移旧存档
            MigrateLegacy();
        }
        catch (Exception ex)
        {
            PortableRoot = "";
            GD.Print($"[存档] exe 旁不可写({ex.GetType().Name})，回退 user:// 存档目录");
        }
    }

    private static void MigrateLegacy()
    {
        try
        {
            if (!Directory.Exists(LegacySavesDir)) return;
            Directory.CreateDirectory(SavesDir);
            int moved = 0;
            foreach (string file in Directory.GetFiles(LegacySavesDir, "*.json"))
            {
                string target = Path.Combine(SavesDir, Path.GetFileName(file));
                if (!File.Exists(target))
                {
                    File.Copy(file, target);
                    moved++;
                }
            }
            if (moved > 0) GD.Print($"[存档] 已从 user:// 迁移 {moved} 个存档到便携目录");

            if (Directory.Exists(LegacyAvatarsDir))
            {
                Directory.CreateDirectory(AvatarsDir);
                int movedAv = 0;
                foreach (string file in Directory.GetFiles(LegacyAvatarsDir, "*.png"))
                {
                    string target = Path.Combine(AvatarsDir, Path.GetFileName(file));
                    if (!File.Exists(target))
                    {
                        File.Copy(file, target);
                        movedAv++;
                    }
                }
                if (movedAv > 0) GD.Print($"[存档] 已迁移 {movedAv} 个头像到便携目录");
            }
        }
        catch (Exception ex)
        {
            GD.PushWarning($"[存档] 迁移旧存档失败: {ex.Message}");
        }
    }

    /// <summary>解析一条资源路径为可读取的绝对路径（优先便携目录,其次 user://）。</summary>
    public static string ResolveFile(string leaf)
    {
        if (string.IsNullOrEmpty(leaf)) return "";
        if (PortableMode)
        {
            string portable = Path.Combine(AvatarsDir, leaf);
            if (File.Exists(portable)) return portable;
        }
        string legacy = Path.Combine(LegacyAvatarsDir, leaf);
        if (File.Exists(legacy)) return legacy;
        // user:// 旧格式完整路径
        if (leaf.StartsWith("user://")) return ProjectSettings.GlobalizePath(leaf);
        return "";
    }

    public static bool EnsureDir()
    {
        try
        {
            Directory.CreateDirectory(SavesDir);
            return true;
        }
        catch (Exception ex)
        {
            GD.PushError($"无法创建存档目录 {SavesDir}: {ex.Message}");
            return false;
        }
    }

    public static int[] ListSaveIds()
    {
        if (!EnsureDir()) return Array.Empty<int>();

        var ids = new List<int>();
        foreach (string file in Directory.GetFiles(SavesDir, "*.json"))
        {
            string stem = Path.GetFileNameWithoutExtension(file);
            if (int.TryParse(stem, NumberStyles.None, CultureInfo.InvariantCulture, out int id) && id > 0)
                ids.Add(id);
            else
                GD.PushWarning($"忽略命名不规范的存档文件: {file}");
        }
        ids.Sort();
        return ids.ToArray();
    }

    /// <summary>兼容旧界面调用；新代码优先使用 <see cref="ListSaveIds"/>。</summary>
    public static string[] ListSaveNames()
    {
        int[] ids = ListSaveIds();
        var names = new string[ids.Length];
        for (int i = 0; i < ids.Length; i++)
            names[i] = ids[i].ToString(CultureInfo.InvariantCulture);
        return names;
    }

    public static bool Save(UnitDef unit)
    {
        if (unit == null)
        {
            GD.PushError("保存失败: 单位为空");
            return false;
        }
        if (!EnsureDir()) return false;

        bool assignedNow = false;
        try
        {
            if (unit.Id <= 0)
            {
                unit.Id = AllocateId();
                assignedNow = true;
            }

            string path = PathFor(unit.Id);
            string json = SaveCodec.Serialize(unit);
            File.WriteAllText(path, json);

            CurrentSave.Upsert(unit);
            GD.Print($"卡牌存档已保存: id={unit.Id}, name={unit.Name}, version={SaveCodec.CurrentVersion} @ {path}");
            return true;
        }
        catch (Exception ex)
        {
            if (assignedNow) unit.Id = 0;
            GD.PushError($"保存卡牌失败: {ex.GetType().Name}: {ex.Message}");
            return false;
        }
    }

    public static bool Exists(int id)
    {
        return id > 0 && File.Exists(PathFor(id));
    }

    /// <summary>扫描并载入全部有效存档；单个损坏文件不会阻止其他存档载入。</summary>
    public static List<UnitDef> LoadAll()
    {
        var units = new List<UnitDef>();
        foreach (int id in ListSaveIds())
        {
            UnitDef unit = Load(id);
            if (unit != null) units.Add(unit);
        }
        return units;
    }

    /// <summary>应用启动后只自动扫描一次，避免用户主动移出会话后返回主菜单又被重新加入。</summary>
    public static int LoadAllIntoSessionOnce()
    {
        if (CurrentSave.DiskScanned) return 0;
        CurrentSave.DiskScanned = true;
        int loaded = 0;
        foreach (UnitDef unit in LoadAll())
        {
            CurrentSave.Upsert(unit);
            loaded++;
        }
        GD.Print($"启动存档扫描: 发现并载入 {loaded} 张卡牌");
        return loaded;
    }

    public static bool Delete(int id)
    {
        if (id <= 0)
        {
            GD.PushError("删除失败: 存档 ID 必须为正整数");
            return false;
        }
        if (!EnsureDir()) return false;
        string path = PathFor(id);
        if (!File.Exists(path)) return true;
        try
        {
            File.Delete(path);
            GD.Print($"已删除卡牌存档: id={id}");
            return true;
        }
        catch (Exception ex)
        {
            GD.PushError($"删除存档 {path} 失败: {ex.Message}");
            return false;
        }
    }

    public static UnitDef Load(int id)
    {
        if (id <= 0)
        {
            GD.PushError("载入失败: 存档 ID 必须为正整数");
            return null;
        }
        if (!Data.ResourceDb.Load()) return null;

        string path = PathFor(id);
        if (!File.Exists(path)) return null;

        try
        {
            string text = File.ReadAllText(path);
            UnitDef unit = SaveCodec.Deserialize(
                text,
                Data.ResourceDb.FindById,
                Data.ResourceDb.FindByName,
                message => GD.PushWarning($"存档 {id}: {message}"));

            if (unit.Id != id)
            {
                GD.PushWarning($"存档文件名 ID={id} 与内容 ID={unit.Id} 不一致；以文件名为准");
                unit.Id = id;
            }
            return unit;
        }
        catch (Exception ex)
        {
            GD.PushError($"载入存档 {path} 失败: {ex.GetType().Name}: {ex.Message}");
            return null;
        }
    }

    private static int AllocateId()
    {
        lock (IdLock)
        {
            var usedIds = new List<int>();
            foreach (int id in ListSaveIds()) usedIds.Add(id);
            foreach (UnitDef unit in CurrentSave.All)
                if (unit.Id > 0) usedIds.Add(unit.Id);
            int newId = StableIds.Allocate(usedIds);
            if (newId == int.MaxValue)
                throw new InvalidOperationException("存档 ID 已耗尽");
            return newId;
        }
    }

    private static string PathFor(int id) => Path.Combine(SavesDir, $"{id.ToString(CultureInfo.InvariantCulture)}.json");
}