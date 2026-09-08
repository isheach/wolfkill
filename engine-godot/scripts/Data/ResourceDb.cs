using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text.Json;
using Godot;
using KsgGodot.Model;

namespace KsgGodot.Data;

/// <summary>资源库：从 JSON 加载资源，并在进入游戏前审计数据契约。</summary>
public static class ResourceDb
{
    private const int ExpectedResourceCount = 758;
    private const int ExpectedEffectCount = 1433;

    private static List<ResourceDef> _all = new();
    public static bool Loaded { get; private set; }

    public static IReadOnlyList<ResourceDef> All => _all;

    /// <summary>技能等级变体表: id → (等级范围, 各级数值序列数组[每条效果/序列])。来自 rules 原文(批次1,233个)。</summary>
    public static Dictionary<int, (string[] RankRange, List<int[]> Series)> RankVariants { get; } = new();

    /// <summary>查询某技能的等级变体(无则 null)。</summary>
    public static (string[] RankRange, List<int[]> Series)? RankVariantFor(int id)
        => RankVariants.TryGetValue(id, out var v) ? v : null;

    public static bool Load()
    {
        if (Loaded) return true;

        const string path = "res://data/ksg_resources.json";
        if (!Godot.FileAccess.FileExists(path))
        {
            GD.PushError("资源库 JSON 不存在: " + path);
            return false;
        }

        try
        {
            string text = Godot.FileAccess.GetFileAsString(path);
            using JsonDocument json = JsonDocument.Parse(text);
            JsonElement root = json.RootElement;
            JsonElement resources = Required(root, "resources", "root");
            if (resources.ValueKind != JsonValueKind.Array)
                throw DataError("root.resources", "应为数组");

            var loaded = new List<ResourceDef>(resources.GetArrayLength());
            int index = 0;
            foreach (JsonElement item in resources.EnumerateArray())
            {
                string context = $"resources[{index}]";
                if (item.ValueKind != JsonValueKind.Object)
                    throw DataError(context, "应为对象");
                loaded.Add(ParseResource(item, context));
                index++;
            }

            if (!Audit(loaded))
                return false;

            _all = loaded;
            Loaded = true;
            LoadRankVariants();
            LoadSkillOwner();
            GD.Print($"资源库加载: {_all.Count} 条");
            return true;
        }
        catch (JsonException ex)
        {
            GD.PushError($"资源库 JSON 语法错误（行 {ex.LineNumber}, 字节 {ex.BytePositionInLine}）: {ex.Message}");
        }
        catch (InvalidDataException ex)
        {
            GD.PushError("资源库数据错误: " + ex.Message);
        }
        catch (Exception ex)
        {
            GD.PushError($"资源库加载失败: {ex.GetType().Name}: {ex.Message}");
        }

        _all.Clear();
        Loaded = false;
        return false;
    }

    private static ResourceDef ParseResource(JsonElement data, string context)
    {
        string name = ReadString(data, "name", context);
        context += $"('{name}')";

        var resource = new ResourceDef
        {
            Id = ReadInt(data, "id", context),
            Name = name,
            Kind = ParseKind(ReadString(data, "kind", context), context + ".kind"),
            Rank = ParseRank(ReadString(data, "rank", context), context + ".rank"),
            When = ParseWhen(ReadString(data, "when", context), context + ".when"),
            Cost = ReadInt(data, "cost", context),
            Recast = ReadInt(data, "recast", context),
            CountPerRound = ReadInt(data, "count_per_round", context),
            Reserve = ReadInt(data, "reserve", context),
            ReserveMax = ReadInt(data, "reserve_max", context),
            Unique = ReadInt(data, "unique", context),
            Prereq = ReadOptionalString(data, "prereq"),
            ExclusiveWith = ReadOptionalString(data, "exclusive"),
            Text = ReadString(data, "text", context),
        };

        resource.Type = ParseResourceType(
            ReadString(data, "type", context), resource.Kind, context + ".type");
        resource.Focus = ParseFocus(ReadString(data, "focus", context), context + ".focus");

        JsonElement feat = Required(data, "feat", context);
        if (feat.ValueKind != JsonValueKind.Array)
            throw DataError(context + ".feat", "应为字符串数组");
        int featIndex = 0;
        foreach (JsonElement item in feat.EnumerateArray())
        {
            if (item.ValueKind != JsonValueKind.String)
                throw DataError($"{context}.feat[{featIndex}]", "应为字符串枚举");
            resource.Feat |= ParseFeat(item.GetString() ?? "", $"{context}.feat[{featIndex}]");
            featIndex++;
        }

        JsonElement effects = Required(data, "effects", context);
        if (effects.ValueKind != JsonValueKind.Array)
            throw DataError(context + ".effects", "应为数组");
        int effectIndex = 0;
        foreach (JsonElement effect in effects.EnumerateArray())
        {
            string effectContext = $"{context}.effects[{effectIndex}]";
            if (effect.ValueKind != JsonValueKind.Object)
                throw DataError(effectContext, "应为对象");
            resource.Effects.Add(ParseEffect(effect, effectContext));
            effectIndex++;
        }

        return resource;
    }

    private static EffectLine ParseEffect(JsonElement data, string context)
    {
        Cond cond = ParseCond(ReadString(data, "cond", context), context + ".cond");
        JsonElement condArg = Required(data, "cond_arg", context);
        JsonElement condArg2 = Required(data, "cond_arg2", context);

        return new EffectLine
        {
            Flag = ParseEffectFlag(ReadString(data, "flag", context), context + ".flag"),
            Attr = ParseAttr(Required(data, "attr", context), context + ".attr", allowEmpty: true),
            Value = ParseScalar(Required(data, "value", context), context + ".value"),
            Status = ParseStatusOrSlot(Required(data, "status", context), context + ".status", allowEmpty: true),
            Layers = ParseScalar(Required(data, "layers", context), context + ".layers"),
            Rank = ParseRankScalar(Required(data, "rank", context), context + ".rank"),
            Target = ParseTarget(Required(data, "target", context), context + ".target"),
            Times = ReadInt(data, "times", context),
            Cond = cond,
            CondArg = ParseCondArg(condArg, cond, second: false, context + ".cond_arg"),
            CondArg2 = ParseCondArg(condArg2, cond, second: true, context + ".cond_arg2"),
            Chance = ReadInt(data, "chance", context),
            ChanceAttrBase = ParseAttr(
                Required(data, "chance_attr_base", context), context + ".chance_attr_base", allowEmpty: false),
            ChanceNeg = ReadBool(data, "chance_neg", context),
            LuckHalve = ReadBool(data, "luck_halve", context),
            Cap = ReadInt(data, "cap", context),
            Desc = ReadString(data, "desc", context),
        };
    }

    private static ResKind ParseKind(string value, string context) => value switch
    {
        "skill" => ResKind.Skill,
        "np" => ResKind.Np,
        "item" => ResKind.Item,
        _ => throw UnknownEnum(context, value),
    };

    private static int ParseResourceType(string value, ResKind kind, string context)
    {
        if (kind == ResKind.Np)
        {
            return value switch
            {
                "KS_NP_HUMAN" => (int)NpType.Human,
                "KS_NP_ARMORY" => (int)NpType.Army,
                "KS_NP_CASTLE" => (int)NpType.Castle,
                "KS_NP_WORLD" => (int)NpType.World,
                "KS_NP_BOUND" => (int)NpType.Bound,
                "KS_NP_MAKLESS" => (int)NpType.NoType,
                _ => throw UnknownEnum(context, value),
            };
        }

        return value switch
        {
            "KS_T_CLASS" => (int)SkillType.Class,
            "KS_T_TALENT" => (int)SkillType.Talent,
            "KS_T_TECHNIQUE" => (int)SkillType.Technique,
            "KS_T_BLESS" => (int)SkillType.Bless,
            "KS_T_CROWN" => (int)SkillType.Crown,
            "KS_T_WEAPON" => (int)SkillType.Weapon,
            "KS_T_MAGIC" => (int)SkillType.Magic,
            _ => throw UnknownEnum(context, value),
        };
    }

    private static Rank ParseRank(string value, string context) => value switch
    {
        "-" or "KS_RANK_NEG" => Rank.Neg,
        "E" or "KS_RANK_E" => Rank.E,
        "D" or "KS_RANK_D" => Rank.D,
        "C" or "KS_RANK_C" => Rank.C,
        "B" or "KS_RANK_B" => Rank.B,
        "A" or "KS_RANK_A" => Rank.A,
        "EX" or "KS_RANK_EX" => Rank.Ex,
        _ => throw UnknownEnum(context, value),
    };

    private static When ParseWhen(string value, string context) => value switch
    {
        "passive" or "KS_WHEN_PASSIVE" => When.Passive,
        "act" or "KS_WHEN_ACT" => When.Act,
        "battle_start" or "KS_WHEN_BATTLE_START" => When.BattleStart,
        "proc" or "KS_WHEN_PROC" => When.Proc,
        "any" or "KS_WHEN_ANY" => When.Any,
        _ => throw UnknownEnum(context, value),
    };

    private static int ParseFocus(string value, string context) => value switch
    {
        "" or "FC_NONE" => (int)Focus.None,
        "FC_DECISIVE" => (int)Focus.Decisive,
        "FC_INSTAKILL" => (int)Focus.InstantKill,
        "FC_SWORD" => (int)Focus.MagicSword,
        "FC_DEFENSE" => (int)Focus.Defense,
        "FC_OFFENSE" => (int)Focus.Offense,
        "FC_BUFF" => (int)Focus.Buff,
        "FC_SUMMON" => (int)Focus.Summon,
        "FC_STATUS" => (int)Focus.Status,
        "FC_SUPPLY" => (int)Focus.Supply,
        "FC_SPECIAL" => (int)Focus.Special,
        "FC_ANTITRAIT" => (int)Focus.AntiTrait,
        _ => throw UnknownEnum(context, value),
    };

    private static int ParseFeat(string value, string context) => value switch
    {
        "KS_F_NONE" => (int)Feat.None,
        "KS_F_MAIN" => (int)Feat.Main,
        "KS_F_SUPPORT" => (int)Feat.Support,
        "KS_F_SERVANT" => (int)Feat.Servant,
        "KS_F_REAR" => (int)Feat.Rear,
        "KS_F_ASSIST" => (int)Feat.Assist,
        "KS_F_COUNTER" => (int)Feat.Counter,
        "KS_F_RIDE" => (int)Feat.Ride,
        "KS_F_BURST_READY" => (int)Feat.ChargeReady,
        "KS_F_PIERCE" => (int)Feat.Pierce,
        "KS_F_INV_PIERCE" => (int)Feat.InvPierce,
        "KS_F_ENERGY" => (int)Feat.Energy,
        "KS_F_UNIQUE" => (int)Feat.Unique,
        _ => throw UnknownEnum(context, value),
    };

    private static EffFlag ParseEffectFlag(string value, string context) => value switch
    {
        "EF_NONE" => EffFlag.None,
        "EF_ATTR_UP" => EffFlag.AttrUp,
        "EF_ATTR_DOWN" => EffFlag.AttrDown,
        "EF_ATTR_UP_CONST" => EffFlag.AttrUpConst,
        "EF_ATTR_DOWN_CONST" => EffFlag.AttrDownConst,
        "EF_WIN_UP" => EffFlag.WinUp,
        "EF_WIN_DOWN" => EffFlag.WinDown,
        "EF_FINAL_WIN_UP" => EffFlag.FinalWinUp,
        "EF_FINAL_WIN_DOWN" => EffFlag.FinalWinDown,
        "EF_FLOOR_UP" => EffFlag.FloorUp,
        "EF_FLOOR_PEN" => EffFlag.FloorPen,
        "EF_HIT_UP" => EffFlag.HitUp,
        "EF_HIT_FINAL_UP" => EffFlag.HitFinalUp,
        "EF_HIT_PEN" => EffFlag.HitPen,
        "EF_RES_UP" => EffFlag.ResUp,
        "EF_RES_DOWN" => EffFlag.ResDown,
        "EF_STATE_RES" => EffFlag.StateRes,
        "EF_STATE_IM" => EffFlag.StateIm,
        "EF_EFFECT_IM" => EffFlag.EffectIm,
        "EF_MANA_UP" => EffFlag.ManaUp,
        "EF_MANA_DOWN" => EffFlag.ManaDown,
        "EF_STATUS_GIVE" => EffFlag.StatusGive,
        "EF_STATUS_REMOVE" => EffFlag.StatusRemove,
        "EF_BURN_BLOW" => EffFlag.BurnBlow,
        "EF_ELECTRIC_BLOW" => EffFlag.ElectricBlow,
        "EF_POISON_BLOW" => EffFlag.PoisonBlow,
        "EF_RECAST" => EffFlag.Recast,
        "EF_RECAST_LOSE" => EffFlag.RecastLose,
        "EF_FP_UP" => EffFlag.FpUp,
        "EF_FP_DOWN" => EffFlag.FpDown,
        "EF_TP_FP" => EffFlag.TpFp,
        "EF_DEATH" => EffFlag.Death,
        "EF_BOUND_DEATH" => EffFlag.BoundDeath,
        "EF_PIERCE" => EffFlag.Pierce,
        "EF_INV_PIERCE" => EffFlag.InvPierce,
        "EF_EVADE" => EffFlag.Evade,
        "EF_PROTECT" => EffFlag.Protect,
        "EF_INVINCIBLE" => EffFlag.Invincible,
        "EF_RETALIATE" => EffFlag.Retaliate,
        "EF_SUMMON" => EffFlag.Summon,
        "EF_INFO" => EffFlag.Info,
        "EF_CS" => EffFlag.Cs,
        "EF_OTHER" => EffFlag.Other,
        // ---- 复杂机制模块 ----
        "EF_PLEDGE_DECL" => EffFlag.PledgeDecl,
        "EF_TICK_PROC" => EffFlag.TickProc,
        "EF_TICK_ROUND" => EffFlag.TickRound,
        "EF_RANDOM_GIVE" => EffFlag.RandomGive,
        "EF_ON_KILL" => EffFlag.OnKill,
        "EF_ON_WIN" => EffFlag.OnWin,
        "EF_CHARGE" => EffFlag.Charge,
        "EF_GRANT_CS" => EffFlag.GrantCs,
        "EF_GRANT_BADGE" => EffFlag.GrantBadge,
        "EF_SEAL" => EffFlag.Seal,
        "EF_REGEN" => EffFlag.Regen,
        _ => throw UnknownEnum(context, value),
    };

    private static Cond ParseCond(string value, string context) => value switch
    {
        "KC_NONE" => Cond.None,
        "KC_OWN_MAIN" => Cond.OwnMain,
        "KC_OWN_SUPPORT" => Cond.OwnSupport,
        "KC_OWN_REAR" => Cond.OwnRear,
        "KC_TARGET_TRAIT" => Cond.TargetTrait,
        "KC_TARGET_NOT_TRAIT" => Cond.TargetNotTrait,
        "KC_TARGET_STATUS_EQ" => Cond.TargetStatusEq,
        "KC_TARGET_STATUS_GE" => Cond.TargetStatusGe,
        "KC_STATUS_NOT" => Cond.StatusNot,
        "KC_FRIEND_STATUS" => Cond.FriendStatus,
        "KC_TARGET_LUCK_GE" => Cond.TargetLuckGe,
        "KC_TARGET_LEVEL_GE" => Cond.TargetLevelGe,
        "KC_LEVEL_DIFF" => Cond.LevelDiff,
        "KC_DAY" => Cond.Day,
        "KC_NIGHT" => Cond.Night,
        "KC_FIRST_ENCOUNTER" => Cond.FirstEncounter,
        "KC_ENEMY_MAIN_MASTER" => Cond.EnemyMainMaster,
        "KC_SELF_HP" => Cond.SelfHp,
        "KC_SELF_MP" => Cond.SelfMp,
        "KC_FRIEND_IN_BATTLE" => Cond.FriendInBattle,
        "KC_ENEMY_IN_BATTLE" => Cond.EnemyInBattle,
        "KC_SELF_TRAIT" => Cond.SelfTrait,
        "KC_ENEMY_TACTIC" => Cond.EnemyTactic,
        "KC_SELF_TACTIC" => Cond.SelfTactic,
        "KC_TACTIC_NOT_PAIRED" => Cond.TacticNotPaired,
        "KC_ENEMY_IS_SERVANT" => Cond.EnemyIsServant,
        "KC_ENEMY_IS_SUMMON" => Cond.EnemyIsSummon,
        "KC_SELF_IS_MASTER" => Cond.SelfIsMaster,
        "KC_SELF_IS_SERVANT" => Cond.SelfIsServant,
        "KC_HAS_CS" => Cond.HasCs,
        "KC_MP_UNDER" => Cond.MpUnder,
        "KC_TARGET_AGI_LT" => Cond.TargetAgiLt,
        "KC_TARGET_AGI_GE" => Cond.TargetAgiGe,
        _ => throw UnknownEnum(context, value),
    };

    private static int ParseCondArg(JsonElement value, Cond cond, bool second, string context)
    {
        if (!second)
        {
            if (cond is Cond.TargetTrait or Cond.TargetNotTrait or Cond.SelfTrait)
                return ParseTrait(value, context);
            if (cond is Cond.TargetStatusEq or Cond.TargetStatusGe or Cond.StatusNot or Cond.FriendStatus)
                return ParseStatus(value, context);
            if (cond == Cond.SelfHp)
                return ParseAttr(value, context, allowEmpty: false);
            if (cond is Cond.EnemyTactic or Cond.SelfTactic)
                return ParseTactic(value, context);
        }

        return ParseScalar(value, context);
    }

    private static int ParseAttr(JsonElement value, string context, bool allowEmpty)
    {
        if (value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out int number))
        {
            if (number >= -1 && number <= 6) return number;
            throw DataError(context, $"属性索引超出范围: {number}");
        }
        if (value.ValueKind != JsonValueKind.String)
            throw DataError(context, "应为属性枚举或整数");

        string symbol = value.GetString() ?? "";
        if (allowEmpty && symbol.Length == 0) return -1;
        return symbol switch
        {
            "A_STR" => 0,
            "A_END" => 1,
            "A_AGI" => 2,
            "A_MAG" => 3,
            "A_LUK" => 4,
            "A_NP" => 5,
            "A_CIRCUIT" => 6,
            _ => throw UnknownEnum(context, symbol),
        };
    }

    private static int ParseStatus(JsonElement value, string context)
    {
        int result = ParseStatusOrSlot(value, context, allowEmpty: false);
        if (result < 0 || result > (int)StatusKind.TraitGive)
            throw DataError(context, $"状态枚举超出范围: {result}");
        return result;
    }

    private static int ParseStatusOrSlot(JsonElement value, string context, bool allowEmpty)
    {
        if (value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out int number))
            return number;
        if (value.ValueKind != JsonValueKind.String)
            throw DataError(context, "应为状态/战斗位枚举或整数");

        string symbol = value.GetString() ?? "";
        if (allowEmpty && symbol.Length == 0) return (int)StatusKind.None;
        if (!symbol.StartsWith("S_", StringComparison.Ordinal) &&
            !symbol.StartsWith("KS_SLOT_", StringComparison.Ordinal))
            throw UnknownEnum(context, symbol);
        return SymbolInt(symbol, context, allowNumericString: false);
    }

    private static int ParseTrait(JsonElement value, string context)
    {
        if (value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out int number))
            return number;
        if (value.ValueKind != JsonValueKind.String)
            throw DataError(context, "应为特性枚举或整数");
        string symbol = value.GetString() ?? "";
        if (!symbol.StartsWith("TR_", StringComparison.Ordinal))
            throw UnknownEnum(context, symbol);
        return SymbolInt(symbol, context, allowNumericString: false);
    }

    private static int ParseTactic(JsonElement value, string context)
    {
        if (value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out int number))
            return number;
        if (value.ValueKind != JsonValueKind.String)
            throw DataError(context, "应为战术枚举或整数");
        string symbol = value.GetString() ?? "";
        if (!symbol.StartsWith("T_", StringComparison.Ordinal))
            throw UnknownEnum(context, symbol);
        return SymbolInt(symbol, context, allowNumericString: false);
    }

    private static int ParseRankScalar(JsonElement value, string context)
    {
        if (value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out int number))
            return number;
        if (value.ValueKind != JsonValueKind.String)
            throw DataError(context, "应为等级枚举或整数");
        return (int)ParseRank(value.GetString() ?? "", context);
    }

    private static int ParseScalar(JsonElement value, string context)
    {
        if (value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out int number))
            return number;
        if (value.ValueKind != JsonValueKind.String)
            throw DataError(context, "应为整数或受支持的符号枚举");
        return SymbolInt(value.GetString() ?? "", context, allowNumericString: true);
    }

    private static int SymbolInt(string symbol, string context, bool allowNumericString)
    {
        if (allowNumericString && int.TryParse(symbol, NumberStyles.Integer, CultureInfo.InvariantCulture, out int number))
            return number;

        return symbol switch
        {
            "A_STR" => 0,
            "A_END" => 1,
            "A_AGI" => 2,
            "A_MAG" => 3,
            "A_LUK" => 4,
            "A_NP" => 5,
            "A_CIRCUIT" => 6,

            "S_NONE" => (int)StatusKind.None,
            "S_EVADE" => (int)StatusKind.Evade,
            "S_INVINCIBLE" => (int)StatusKind.Invincible,
            "S_PROTECT" => (int)StatusKind.Protect,
            "S_RESUP" => (int)StatusKind.ResUp,
            "S_STATE_RES" => (int)StatusKind.StateRes,
            "S_STATE_IM" => (int)StatusKind.StateIm,
            "S_EFFECT_IM" => (int)StatusKind.EffectIm,
            "S_TIRED" => (int)StatusKind.Tired,
            "S_CRIPPLED" => (int)StatusKind.Crippled,
            "S_LAG" => (int)StatusKind.Lag,
            "S_CURSE" => (int)StatusKind.Curse,
            "S_SEAL" => (int)StatusKind.Seal,
            "S_SKILL_SEAL" => (int)StatusKind.SkillSeal,
            "S_NP_SEAL" => (int)StatusKind.NpSeal,
            "S_RESDOWN" => (int)StatusKind.ResDown,
            "S_POISON" => (int)StatusKind.Poison,
            "S_BURN" => (int)StatusKind.Burn,
            "S_FREEZE" => (int)StatusKind.Freeze,
            "S_ELECTRIC" => (int)StatusKind.Electric,
            "S_STONE" => (int)StatusKind.Stone,
            "S_STUN" => (int)StatusKind.Stun,
            "S_CHARM" => (int)StatusKind.Charm,
            "S_CONFUSE" => (int)StatusKind.Confuse,
            "S_FEAR" => (int)StatusKind.Fear,
            "S_RES_BREAK" => (int)StatusKind.ResBreak,
            "S_TRAIT_GIVE" => (int)StatusKind.TraitGive,
            "S_CHARGE" => (int)StatusKind.Charge,
            "S_BADGE" => (int)StatusKind.Badge,

            "TR_HUMAN" => 1 << 0,
            "TR_DIVINITY" => 1 << 1,
            "TR_DEMONIC" => 1 << 2,
            "TR_DRAGON" => 1 << 3,
            "TR_BEAST" => 1 << 4,
            "TR_MONSTER" => 1 << 5,
            "TR_GOLEM" => 1 << 6,

            "KS_SLOT_NONE" => 0,
            "KS_SLOT_MAIN" => 1,
            "KS_SLOT_SUPPORT" => 2,
            "KS_SLOT_SERVANT" => 3,
            "KS_SLOT_REAR" => 4,

            "T_NONE" => 0,
            "T_STRIKE" => 1,
            "T_RAID" => 2,
            "T_PROBE" => 3,
            "T_HOLD" => 4,
            _ => throw UnknownEnum(context, symbol),
        };
    }

    private static int ParseTarget(JsonElement value, string context)
    {
        if (value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out int number))
            return number;
        if (value.ValueKind != JsonValueKind.String)
            throw DataError(context, "应为目标枚举或整数");

        string symbol = value.GetString() ?? "";
        if (symbol == "self") return -2;
        if (symbol == "ally_all") return -1;
        if (symbol == "enemy_all") return 0;
        if (symbol.StartsWith("enemy_", StringComparison.Ordinal) &&
            int.TryParse(symbol.AsSpan(6), NumberStyles.None, CultureInfo.InvariantCulture, out number) && number > 0)
            return number;
        throw UnknownEnum(context, symbol);
    }

    private static JsonElement Required(JsonElement data, string key, string context)
    {
        if (data.ValueKind != JsonValueKind.Object)
            throw DataError(context, "应为对象");
        if (!data.TryGetProperty(key, out JsonElement value))
            throw DataError(context + "." + key, "缺少必需字段");
        return value;
    }

    private static string ReadString(JsonElement data, string key, string context)
    {
        JsonElement value = Required(data, key, context);
        if (value.ValueKind != JsonValueKind.String)
            throw DataError(context + "." + key, "应为字符串");
        return value.GetString() ?? "";
    }

    /// <summary>可选字符串(字段缺失时返回默认值, 用于 prereq/exclusive 等可选元数据)。</summary>
    private static string ReadOptionalString(JsonElement data, string key, string fallback = "")
    {
        if (!data.TryGetProperty(key, out JsonElement value)) return fallback;
        if (value.ValueKind != JsonValueKind.String) return fallback;
        return value.GetString() ?? fallback;
    }

    private static int ReadInt(JsonElement data, string key, string context)
    {
        JsonElement value = Required(data, key, context);
        if (value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out int number))
            return number;
        if (value.ValueKind == JsonValueKind.String &&
            int.TryParse(value.GetString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out number))
            return number;
        throw DataError(context + "." + key, "应为整数");
    }

    private static bool ReadBool(JsonElement data, string key, string context)
    {
        JsonElement value = Required(data, key, context);
        if (value.ValueKind == JsonValueKind.True) return true;
        if (value.ValueKind == JsonValueKind.False) return false;
        if (value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out int number) && number is 0 or 1)
            return number == 1;
        if (value.ValueKind == JsonValueKind.String && bool.TryParse(value.GetString(), out bool result))
            return result;
        throw DataError(context + "." + key, "应为布尔值");
    }

    private static InvalidDataException UnknownEnum(string context, string value) =>
        DataError(context, $"未知枚举值 '{value}'（不会静默按 0 处理）");

    private static InvalidDataException DataError(string context, string message) =>
        new($"{context}: {message}");

    private static bool Audit(IReadOnlyList<ResourceDef> resources)
    {
        int skills = 0;
        int nps = 0;
        int items = 0;
        int effects = 0;
        var ids = new HashSet<int>();
        var errors = new List<string>();

        foreach (ResourceDef resource in resources)
        {
            switch (resource.Kind)
            {
                case ResKind.Skill: skills++; break;
                case ResKind.Np: nps++; break;
                case ResKind.Item: items++; break;
            }
            effects += resource.Effects.Count;
            if (!ids.Add(resource.Id)) errors.Add($"资源 ID 重复: {resource.Id}");
            if (resource.Effects.Count == 0) errors.Add($"资源无效果行: {resource.Id}/{resource.Name}");
        }

        if (resources.Count != ExpectedResourceCount)
            errors.Add($"总资源应为 {ExpectedResourceCount}，实际 {resources.Count}");
        if (effects != ExpectedEffectCount)
            errors.Add($"总效果应为 {ExpectedEffectCount}，实际 {effects}");
        if (skills != 468 || nps != 247 || items != 43)
            errors.Add($"分类数量应为 468/247/43，实际 {skills}/{nps}/{items}");

        AuditSamples(resources, errors);
        GD.Print($"资源库审计: 总资源={resources.Count}, 总效果={effects}, 技能/宝具/礼装={skills}/{nps}/{items}, 未知枚举=0");

        if (errors.Count == 0)
        {
            GD.Print("资源库抽查: 12/12 通过（技能/宝具/礼装、完整字段、条件、符号枚举与召唤效果）");
            return true;
        }

        foreach (string error in errors)
            GD.PushError("资源库审计失败: " + error);
        return false;
    }

    private static void AuditSamples(IReadOnlyList<ResourceDef> resources, List<string> errors)
    {
        ResourceDef courage = Sample(resources, "勇猛", errors);
        Check(courage != null && courage.Kind == ResKind.Skill &&
              courage.Type == (int)SkillType.Talent && courage.Effects.Count == 3,
              "勇猛应为 Talent 且有 3 条效果", errors);

        ResourceDef excalibur = Sample(resources, "誓约胜利之剑·光炮", errors);
        Check(excalibur != null && excalibur.Kind == ResKind.Np &&
              excalibur.Type == (int)NpType.Castle && excalibur.Focus == (int)Focus.Decisive &&
              excalibur.Effects.Count >= 2 && excalibur.Effects[1].Cond == Cond.HasCs,
              "誓约胜利之剑·光炮的类型/面向/令咒条件不正确", errors);

        ResourceDef ingot = Sample(resources, "灰锭", errors);
        Check(ingot != null && ingot.Kind == ResKind.Item && ingot.Reserve == 1 && ingot.ReserveMax == 1,
              "灰锭应为储备 1/1 的礼装", errors);

        ResourceDef magicBullet = Sample(resources, "魔弹宝石", errors);
        Check(magicBullet != null && magicBullet.Kind == ResKind.Item &&
              magicBullet.Type == (int)SkillType.Magic && magicBullet.When == When.Proc &&
              magicBullet.Effects.Count == 1 && magicBullet.Effects[0].Target == 1,
              "魔弹宝石的类型/时机/目标不正确", errors);

        ResourceDef calico = Sample(resources, "卡利科M950A冲锋手枪", errors);
        Check(calico != null && calico.Kind == ResKind.Item &&
              calico.Type == (int)SkillType.Weapon && calico.When == When.Any &&
              calico.Reserve == 100 && calico.Effects.Count == 2 && calico.Effects[0].ChanceNeg,
              "卡利科M950A冲锋手枪的类型/储备/负面判定不正确", errors);

        ResourceDef mysticEyes = Sample(resources, "石化之魔眼", errors);
        Check(mysticEyes != null && mysticEyes.Effects.Count > 0 &&
              mysticEyes.Effects[0].Flag == EffFlag.StatusGive &&
              mysticEyes.Effects[0].Status == (int)StatusKind.Stone &&
              mysticEyes.Effects[0].Chance == 75 && mysticEyes.Effects[0].ChanceNeg,
              "石化之魔眼的状态/概率/负面判定不正确", errors);

        ResourceDef army = Sample(resources, "王之军势·阿刻琉斯之壁垒", errors);
        Check(army != null && army.Effects.Count > 0 && army.Effects[0].Flag == EffFlag.Summon &&
              army.Effects[0].CondArg == 100 && army.Effects[0].CondArg2 == (1 << 0),
              "王之军势·阿刻琉斯之壁垒的召唤/总属性/人型特性不正确", errors);

        ResourceDef zabaniya = Sample(resources, "妄想心音·交错而鸣", errors);
        Check(zabaniya != null && zabaniya.Effects.Count > 0 &&
              zabaniya.Effects[0].Flag == EffFlag.Death && zabaniya.Effects[0].Chance == 90 &&
              zabaniya.Effects[0].LuckHalve,
              "妄想心音·交错而鸣的即死/幸运减半字段不正确", errors);

        ResourceDef dojigiri = Sample(resources, "童子切安纲", errors);
        Check(dojigiri != null && dojigiri.Effects.Count == 2 &&
              dojigiri.Effects[0].Cond == Cond.TargetTrait && dojigiri.Effects[0].CondArg == (1 << 1) &&
              dojigiri.Effects[1].Cond == Cond.TargetTrait && dojigiri.Effects[1].CondArg == (1 << 2),
              "童子切安纲的神性/魔性条件不正确", errors);

        ResourceDef continuation = Sample(resources, "战斗续行·不屈", errors);
        Check(continuation != null && continuation.Effects.Count > 0 &&
              continuation.Effects[0].Layers == (int)StatusKind.Crippled,
              "战斗续行·不屈未正确解析符号层数 S_CRIPPLED", errors);

        ResourceDef surgery = Sample(resources, "外科手术", errors);
        Check(surgery != null && surgery.Effects.Count > 0 &&
              surgery.Effects[0].Value == (int)StatusKind.Poison,
              "外科手术未正确解析符号数值 S_POISON", errors);

        ResourceDef tsubame = Sample(resources, "秘剑·燕返", errors);
        Check(tsubame != null && tsubame.Effects.Count > 0 && tsubame.Effects[0].ChanceAttrBase == 2,
              "秘剑·燕返未正确解析判定属性 A_AGI", errors);
    }

    private static ResourceDef Sample(IReadOnlyList<ResourceDef> resources, string name, List<string> errors)
    {
        ResourceDef found = null;
        foreach (ResourceDef resource in resources)
        {
            if (resource.Name != name) continue;
            if (found != null)
            {
                errors.Add($"抽查资源名称不唯一: {name}");
                return null;
            }
            found = resource;
        }
        if (found == null) errors.Add($"缺少抽查资源: {name}");
        return found;
    }

    private static void Check(bool condition, string message, List<string> errors)
    {
        if (!condition) errors.Add(message);
    }

    // ---------- 搜索 ----------

    public static List<ResourceDef> Search(string keyword, ResKind? kind = null, int? focus = null)
    {
        var result = new List<ResourceDef>();
        foreach (ResourceDef resource in _all)
        {
            if (kind.HasValue && resource.Kind != kind.Value) continue;
            if (focus.HasValue && resource.Focus != focus.Value) continue;
            if (!string.IsNullOrEmpty(keyword) && !resource.Name.Contains(keyword, StringComparison.Ordinal))
                continue;
            result.Add(resource);
        }
        return result;
    }

    /// <summary>技能归属表: id → servant/master/both(规则书: 从者库/御主库)。</summary>
    public static Dictionary<int, string> SkillOwner { get; } = new();

    /// <summary>查询某技能的归属(无记录返回 "both" 宽松处理)。</summary>
    public static string OwnerOf(int id)
        => SkillOwner.TryGetValue(id, out string v) ? v : "both";

    /// <summary>查询某技能的归属(无记录返回 "both" 宽松处理)。</summary>
    public static string OwnerOf(ResourceDef r)
        => r == null ? "both" : OwnerOf(r.Id);

    public static ResourceDef FindById(int id)
    {
        foreach (ResourceDef resource in _all)
            if (resource.Id == id) return resource;
        return null;
    }

    /// <summary>加载技能等级变体表(规则书原文批次的真实等级数值)。</summary>
    private static void LoadRankVariants()
    {
        RankVariants.Clear();
        const string path = "res://data/rank_variants.json";
        if (!Godot.FileAccess.FileExists(path))
        {
            GD.Print("[等级变体] 未找到 rank_variants.json(仍可用系数缩放兜底)");
            return;
        }
        try
        {
            string text = Godot.FileAccess.GetFileAsString(path);
            using JsonDocument json = JsonDocument.Parse(text);
            JsonElement root = json.RootElement;
            if (!root.TryGetProperty("skills", out JsonElement skills) || skills.ValueKind != JsonValueKind.Array)
            {
                GD.PushWarning("[等级变体] rank_variants.json 缺少 skills 数组");
                return;
            }
            foreach (JsonElement item in skills.EnumerateArray())
            {
                if (!item.TryGetProperty("id", out JsonElement idEl) ||
                    idEl.ValueKind != JsonValueKind.Number) continue;
                int id = idEl.GetInt32();
                var range = new List<string>();
                if (item.TryGetProperty("rank_range", out JsonElement rr) && rr.ValueKind == JsonValueKind.Array)
                    foreach (JsonElement r in rr.EnumerateArray()) range.Add(r.GetString() ?? "");
                var series = new List<int[]>();
                if (item.TryGetProperty("series", out JsonElement ss) && ss.ValueKind == JsonValueKind.Array)
                {
                    foreach (JsonElement s in ss.EnumerateArray())
                    {
                        var vals = new List<int>();
                        if (s.ValueKind == JsonValueKind.Array)
                            foreach (JsonElement v in s.EnumerateArray())
                                if (v.ValueKind == JsonValueKind.Number) vals.Add(v.GetInt32());
                        series.Add(vals.ToArray());
                    }
                }
                RankVariants[id] = (range.ToArray(), series);
            }
            GD.Print($"[等级变体] 加载 {RankVariants.Count} 个技能的等级数值表(规则书原文)");
            Building.CardBuildRules.VariantLookup = RankVariantFor;
        }
        catch (Exception ex)
        {
            GD.PushWarning($"[等级变体] 加载失败: {ex.Message}");
        }
    }

    /// <summary>加载技能归属表(规则书: 从者库/御主库 → servant/master/both)。</summary>
    private static void LoadSkillOwner()
    {
        SkillOwner.Clear();
        const string path = "res://data/skill_owner.json";
        if (!Godot.FileAccess.FileExists(path))
        {
            GD.Print("[技能归属] 未找到 skill_owner.json(技能不限从者/御主)");
            return;
        }
        try
        {
            string text = Godot.FileAccess.GetFileAsString(path);
            using JsonDocument json = JsonDocument.Parse(text);
            JsonElement root = json.RootElement;
            if (!root.TryGetProperty("skills", out JsonElement skills) || skills.ValueKind != JsonValueKind.Array)
            {
                GD.PushWarning("[技能归属] skill_owner.json 缺少 skills 数组");
                return;
            }
            foreach (JsonElement item in skills.EnumerateArray())
            {
                if (!item.TryGetProperty("id", out JsonElement idEl) ||
                    idEl.ValueKind != JsonValueKind.Number) continue;
                string owner = item.TryGetProperty("owner", out JsonElement o) && o.ValueKind == JsonValueKind.String
                    ? o.GetString() ?? "both"
                    : "both";
                SkillOwner[idEl.GetInt32()] = owner;
            }
            GD.Print($"[技能归属] 加载 {SkillOwner.Count} 个技能的从者/御主归属");
            Building.CardBuildRules.OwnerLookup = id => OwnerOf(id);
        }
        catch (Exception ex)
        {
            GD.PushWarning($"[技能归属] 加载失败: {ex.Message}");
        }
    }

    /// <summary>
    /// 兼容旧调用。资源库存在同名条目；存档与持久化恢复必须改用 <see cref="FindById"/>。
    /// </summary>
    public static ResourceDef FindByName(string name)
    {
        foreach (ResourceDef resource in _all)
            if (resource.Name == name) return resource;
        return null;
    }

    public static string KindName(ResKind kind) => kind switch
    {
        ResKind.Skill => "技能",
        ResKind.Np => "宝具",
        _ => "礼装",
    };
}
