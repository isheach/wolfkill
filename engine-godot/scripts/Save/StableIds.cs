namespace KsgGodot.Save;

/// <summary>稳定 ID 分配(纯 C#,不依赖 Godot IO):
/// 起始于已知已存在的 ID 集合,分配最小的未使用正整数。
/// SaveManager(Godot 端)与纯 C# 测试共用此逻辑,保证行为一致。</summary>
public static class StableIds
{
    /// <summary>在既有 ID 集合基础上分配一个未使用的最小正整数 ID。</summary>
    public static int Allocate(System.Collections.Generic.IEnumerable<int> existingIds)
    {
        var used = new System.Collections.Generic.HashSet<int>();
        foreach (int id in existingIds)
        {
            if (id > 0 && !used.Contains(id)) used.Add(id);
        }
        int candidate = 1;
        while (used.Contains(candidate)) candidate++;
        return candidate;
    }

    /// <summary>判断某 ID 是否与既有集合冲突。</summary>
    public static bool IsFree(int id, System.Collections.Generic.IEnumerable<int> existingIds)
    {
        if (id <= 0) return false;
        foreach (int e in existingIds)
            if (e == id) return false;
        return true;
    }
}