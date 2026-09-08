using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text.Json;
using KsgGodot.Model;

namespace KsgGodot.Save;

/// <summary>存档 JSON 编解码。版本 2 起资源列表保存稳定 ID；仍可读取版本 1 的名称列表。</summary>
public static class SaveCodec
{
    public const int CurrentVersion = 3;

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true,
    };

    public static string Serialize(UnitDef unit)
    {
        if (unit == null) throw new ArgumentNullException(nameof(unit));
        if (unit.Id <= 0) throw new InvalidDataException("保存前必须为卡牌分配正整数 ID");

        var status = new List<Dictionary<string, int>>();
        foreach (StatusEntry entry in unit.Status)
        {
            status.Add(new Dictionary<string, int>
            {
                ["kind"] = (int)entry.Kind,
                ["layers"] = entry.Layers,
                ["source"] = entry.Source,
            });
        }

        var payload = new Dictionary<string, object>
        {
            ["version"] = CurrentVersion,
            ["id"] = unit.Id,
            ["name"] = unit.Name,
            ["true_name"] = unit.TrueName,
            ["avatar_path"] = unit.AvatarPath,
            ["utype"] = unit.UType,
            ["servant_class"] = unit.ServantClass,
            ["hidden_attr"] = unit.HiddenAttr,
            ["level"] = unit.Level,
            ["faction"] = unit.Faction,
            ["law"] = unit.Law,
            ["moral"] = unit.Moral,
            ["traits"] = unit.Traits,
            ["fp"] = unit.Fp,
            ["cs"] = unit.Cs,
            ["roaming"] = unit.Roaming,
            ["alive"] = unit.Alive,
            ["main_job"] = unit.MainJob,
            ["sub_job"] = unit.SubJob,
            ["attr"] = unit.Attr,
            ["attr_perm"] = unit.AttrPerm,
            ["mp_cap"] = unit.MpCap,
            ["mp_floor"] = unit.MpFloor,
            ["mp_cur"] = unit.MpCur,
            ["skills"] = ResourceIds(unit.Skills),
            ["phantasms"] = ResourceIds(unit.Phantasms),
            ["items"] = ResourceIds(unit.Items),
            ["status"] = status,
        };
        return JsonSerializer.Serialize(payload, JsonOptions);
    }

    public static UnitDef Deserialize(
        string json,
        Func<int, ResourceDef> findById,
        Func<string, ResourceDef> findByName,
        Action<string> warning = null)
    {
        if (findById == null) throw new ArgumentNullException(nameof(findById));
        if (findByName == null) throw new ArgumentNullException(nameof(findByName));

        using JsonDocument document = JsonDocument.Parse(json);
        JsonElement root = document.RootElement;
        if (root.ValueKind != JsonValueKind.Object)
            throw new InvalidDataException("存档根节点应为 JSON 对象");

        int version = ReadInt(root, "version", 1);
        if (version < 1 || version > CurrentVersion)
            throw new InvalidDataException($"不支持的存档版本 {version}，当前最高支持 {CurrentVersion}");

        var unit = new UnitDef
        {
            Id = ReadInt(root, "id", 0),
            Name = ReadString(root, "name", ""),
            TrueName = ReadString(root, "true_name", ""),
            AvatarPath = ReadString(root, "avatar_path", ""),
            UType = ReadInt(root, "utype", 1),
            ServantClass = ReadInt(root, "servant_class", 0),
            HiddenAttr = ReadInt(root, "hidden_attr", 2),
            Level = ReadInt(root, "level", 60),
            Faction = ReadInt(root, "faction", 0),
            Law = ReadInt(root, "law", 1),
            Moral = ReadInt(root, "moral", 1),
            Traits = ReadInt(root, "traits", 1),
            Fp = ReadInt(root, "fp", 1),
            Cs = ReadInt(root, "cs", 3),
            Roaming = ReadInt(root, "roaming", 0),
            Alive = ReadBool(root, "alive", true),
            MainJob = ReadString(root, "main_job", ""),
            SubJob = ReadString(root, "sub_job", ""),
            MpCap = ReadInt(root, "mp_cap", 150),
            MpFloor = ReadInt(root, "mp_floor", -100),
            MpCur = ReadInt(root, "mp_cur", 50),
        };

        ReadIntArray(root, "attr", unit.Attr);
        ReadIntArray(root, "attr_perm", unit.AttrPerm);
        ReadResources(root, "skills", unit.Skills, findById, findByName, warning);
        ReadResources(root, "phantasms", unit.Phantasms, findById, findByName, warning);
        ReadResources(root, "items", unit.Items, findById, findByName, warning);
        ReadStatus(root, unit.Status, warning);
        return unit;
    }

    /// <summary>版本 3 起:资源列表为对象数组 [{id, dname, note, rank?}],兼容旧版纯数字/字符串。
    /// rank 为可选的购入等级(版本3附加字段,旧读取器会忽略),缺省时使用资源库基础等级。</summary>
    private static List<Dictionary<string, object>> ResourceIds(List<ResourceDef> resources)
    {
        var list = new List<Dictionary<string, object>>(resources.Count);
        foreach (ResourceDef resource in resources)
        {
            if (resource.Id <= 0)
                throw new InvalidDataException($"资源 '{resource.Name}' 缺少稳定 ID，拒绝写入存档");
            var entry = new Dictionary<string, object>
            {
                ["id"] = resource.Id,
            };
            if (!string.IsNullOrWhiteSpace(resource.DisplayName))
                entry["dname"] = resource.DisplayName;
            if (!string.IsNullOrWhiteSpace(resource.Note))
                entry["note"] = resource.Note;
            // 保存实际购入等级(与资源库基础等级不同时),载入后按等级变体/系数恢复数值
            Rank effective = resource.EffectiveRankNow;
            if (effective != Rank.Neg && effective != resource.Rank)
                entry["rank"] = RankUtil.Name(effective);
            list.Add(entry);
        }
        return list;
    }

    private static void ReadResources(
        JsonElement root,
        string key,
        List<ResourceDef> target,
        Func<int, ResourceDef> findById,
        Func<string, ResourceDef> findByName,
        Action<string> warning)
    {
        if (!root.TryGetProperty(key, out JsonElement values)) return;
        if (values.ValueKind != JsonValueKind.Array)
            throw new InvalidDataException($"字段 '{key}' 应为数组");

        var seen = new HashSet<int>();
        foreach (JsonElement value in values.EnumerateArray())
        {
            ResourceDef resource = null;
            int customId = 0;
            string dname = "", note = "";
            string rankName = "";
            if (value.ValueKind == JsonValueKind.Object)
            {
                // 版本 3 对象格式: { id, dname?, note?, rank? }
                if (value.TryGetProperty("id", out JsonElement idEl) &&
                    idEl.ValueKind == JsonValueKind.Number && idEl.TryGetInt32(out customId))
                {
                    resource = findById(customId);
                    if (resource == null) warning?.Invoke($"资源 ID {customId} 不存在，已跳过");
                    if (value.TryGetProperty("dname", out JsonElement dn) && dn.ValueKind == JsonValueKind.String)
                        dname = dn.GetString() ?? "";
                    if (value.TryGetProperty("note", out JsonElement nt) && nt.ValueKind == JsonValueKind.String)
                        note = nt.GetString() ?? "";
                    // 附加字段 rank: 卡面购入等级(版本3扩展,旧读取器忽略)
                    if (value.TryGetProperty("rank", out JsonElement rk) && rk.ValueKind == JsonValueKind.String)
                        rankName = rk.GetString() ?? "";
                }
                else
                {
                    warning?.Invoke($"资源列表 '{key}' 含无效对象，已跳过");
                }
            }
            else if (value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out int id))
            {
                resource = findById(id);
                if (resource == null) warning?.Invoke($"资源 ID {id} 不存在，已跳过");
            }
            else if (value.ValueKind == JsonValueKind.String)
            {
                string legacy = value.GetString() ?? "";
                if (int.TryParse(legacy, NumberStyles.Integer, CultureInfo.InvariantCulture, out id))
                {
                    resource = findById(id);
                    if (resource == null) warning?.Invoke($"资源 ID {id} 不存在，已跳过");
                }
                else
                {
                    resource = findByName(legacy);
                    warning?.Invoke($"旧版存档按名称恢复资源 '{legacy}'；若资源重名只能沿用首条，重新保存后将升级为稳定 ID");
                    if (resource == null) warning?.Invoke($"资源名称 '{legacy}' 不存在，已跳过");
                }
            }
            else
            {
                warning?.Invoke($"资源列表 '{key}' 含无效值，已跳过");
            }

            if (resource != null && seen.Add(resource.Id))
            {
                // 附加 rank:按卡面购等级生成副本(等级变体/系数缩放由 CardBuildRules 处理)
                ResourceDef copy;
                Rank savedRank = RankUtil.Parse(rankName);
                if (savedRank != Rank.Neg && savedRank != resource.Rank)
                    copy = KsgGodot.Building.CardBuildRules.RankedCopy(resource, savedRank);
                else
                    copy = resource.DeepClone();
                if (!string.IsNullOrWhiteSpace(dname)) copy.DisplayName = dname;
                if (!string.IsNullOrWhiteSpace(note)) copy.Note = note;
                target.Add(copy);
            }
        }
    }

    private static void ReadStatus(JsonElement root, List<StatusEntry> target, Action<string> warning)
    {
        if (!root.TryGetProperty("status", out JsonElement values)) return;
        if (values.ValueKind != JsonValueKind.Array)
            throw new InvalidDataException("字段 'status' 应为数组");

        foreach (JsonElement value in values.EnumerateArray())
        {
            if (value.ValueKind != JsonValueKind.Object)
            {
                warning?.Invoke("状态列表含非对象值，已跳过");
                continue;
            }
            int kind = ReadInt(value, "kind", 0);
            int layers = ReadInt(value, "layers", 0);
            int source = ReadInt(value, "source", 0);
            if (kind <= (int)StatusKind.None || kind > (int)StatusKind.TraitGive || layers <= 0)
            {
                warning?.Invoke($"无效状态 kind={kind}, layers={layers}，已跳过");
                continue;
            }
            target.Add(new StatusEntry { Kind = (StatusKind)kind, Layers = layers, Source = source });
        }
    }

    private static void ReadIntArray(JsonElement root, string key, int[] target)
    {
        if (!root.TryGetProperty(key, out JsonElement values)) return;
        if (values.ValueKind != JsonValueKind.Array)
            throw new InvalidDataException($"字段 '{key}' 应为数组");

        int index = 0;
        foreach (JsonElement value in values.EnumerateArray())
        {
            if (index >= target.Length) break;
            target[index++] = ElementInt(value, key);
        }
    }

    private static int ReadInt(JsonElement root, string key, int fallback)
    {
        if (!root.TryGetProperty(key, out JsonElement value)) return fallback;
        return ElementInt(value, key);
    }

    private static int ElementInt(JsonElement value, string key)
    {
        if (value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out int result))
            return result;
        if (value.ValueKind == JsonValueKind.String &&
            int.TryParse(value.GetString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out result))
            return result;
        throw new InvalidDataException($"字段 '{key}' 应为整数");
    }

    private static string ReadString(JsonElement root, string key, string fallback)
    {
        if (!root.TryGetProperty(key, out JsonElement value)) return fallback;
        if (value.ValueKind != JsonValueKind.String)
            throw new InvalidDataException($"字段 '{key}' 应为字符串");
        return value.GetString() ?? fallback;
    }

    private static bool ReadBool(JsonElement root, string key, bool fallback)
    {
        if (!root.TryGetProperty(key, out JsonElement value)) return fallback;
        if (value.ValueKind == JsonValueKind.True) return true;
        if (value.ValueKind == JsonValueKind.False) return false;
        if (value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out int number) && number is 0 or 1)
            return number == 1;
        throw new InvalidDataException($"字段 '{key}' 应为布尔值");
    }
}
