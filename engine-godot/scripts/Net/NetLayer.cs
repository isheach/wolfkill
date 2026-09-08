using System;
using System.Collections.Generic;
using Godot;

namespace KsgGodot.Net;

/// <summary>网络消息类型(主机↔分机)</summary>
public enum NetMsg
{
    Hello = 1,        // 分机→主机: 请求加入 {seatId}
    HelloAck,         // 主机→分机: 接受 {seatId}
    Action,           // 分机→主机: 行动指令 {unitId:action}
    CsUse,            // 分机→主机: 令咒 {masterId:servantId:usage}
    Pact,             // 分机→主机: 契约 {type:otherId}
    NextTurn,         // 主机→分机: 回合推进 {day:phase}
    WorldSnapshot,    // 主机→分机: 世界快照JSON
    Error,            // 双向: 错误
}

/// <summary>基于 Godot ENetMultiplayerPeer 的联机层(4.7, 已验证可双向收发)。</summary>
public partial class NetLayer : Node
{
    public bool IsHost;
    public int Port = 31415;
    public ENetMultiplayerPeer Peer;
    public bool Active;
    public HashSet<int> PeerIds = new();   // 已连接 peer(主机侧)
    private float _elapsed;
    private bool _pollStarted;
    private float _acc;

    public event Action<int, int, string> OnMsg;   // (fromPeer, msgId, payload)
    public event Action<int> OnPeerLeft;

    private static readonly System.Text.Encoding Utf8 = System.Text.Encoding.UTF8;

    public void Host(int port)
    {
        Port = port;
        IsHost = true;
        Peer = new ENetMultiplayerPeer();
        Error err = Peer.CreateServer(port, 16);
        if (err != Error.Ok) { GD.PushError($"[联机] 主机监听失败: {err}"); return; }
        Active = true;
        GD.Print($"[联机] 主机已监听 :{port}");
    }

    public void Join(string host, int port)
    {
        Port = port;
        IsHost = false;
        Peer = new ENetMultiplayerPeer();
        Error err = Peer.CreateClient(host, port);
        if (err != Error.Ok) { GD.PushError($"[联机] 连接失败: {err}"); return; }
        Active = true;
        GD.Print($"[联机] 分机已发起连接 {host}:{port}");
    }

    public override void _Process(double delta)
    {
        if (!Active || Peer == null) return;
        _elapsed += (float)delta;
        if (!_pollStarted)
        {
            if (_elapsed < 0.4f) return;   // 握手初期避开引擎噪音
            _pollStarted = true;
        }
        _acc += (float)delta;
        if (_acc < 0.05f) return;   // 20fps 轮询
        _acc = 0;
        try
        {
            Peer.Poll();
        }
        catch
        {
            return;
        }
        var st = Peer.GetConnectionStatus();
        if (st == MultiplayerPeer.ConnectionStatus.Connected)
        {
            while (Peer.GetAvailablePacketCount() > 0)
            {
                try
                {
                    var bytes = Peer.GetPacket();
                    int from = (int)Peer.GetPacketPeer();
                    if (IsHost) PeerIds.Add(from);
                    string raw = Utf8.GetString(bytes);
                    int sep = raw.IndexOf('|');
                    int msgId = sep > 0 && int.TryParse(raw.Substring(0, sep), out int m) ? m : 0;
                    string payload = sep > 0 ? raw.Substring(sep + 1) : raw;
                    OnMsg?.Invoke(from, msgId, payload);
                }
                catch (Exception ex)
                {
                    GD.PushWarning($"[联机] 收包异常: {ex.Message}");
                }
            }
        }
    }

    public void SendTo(int peer, NetMsg msg, string payload = "")
    {
        if (!Active || Peer == null) return;
        if (Peer.GetConnectionStatus() != MultiplayerPeer.ConnectionStatus.Connected) return;
        string raw = $"{(int)msg}|{payload}";
        var bytes = Utf8.GetBytes(raw);
        Peer.SetTargetPeer(IsHost ? peer : 1);
        Peer.PutPacket(bytes);
    }

    public void Broadcast(NetMsg msg, string payload = "")
    {
        if (!Active || Peer == null || !IsHost) return;
        if (PeerIds.Count == 0) return;
        string raw = $"{(int)msg}|{payload}";
        var bytes = Utf8.GetBytes(raw);
        foreach (int pid in PeerIds)
        {
            Peer.SetTargetPeer(pid);
            Peer.PutPacket(bytes);
        }
    }

    public void Close()
    {
        try { Peer?.Close(); } catch { }
        Peer = null;
        Active = false;
        PeerIds.Clear();
    }

    public override void _ExitTree() => Close();
}

/// <summary>世界快照(JSON可序列化): 主机→分机的世界状态</summary>
public partial class WorldSnapshotData
{
    public int Day;
    public bool Phase;
    public int Turn;
    public int WinnerMasterId;
    public bool Finished;
    public List<LyInfo> Leylines = new();
    public List<UnitInfo> Units = new();
    public List<PactInfo> Pacts = new();

    public partial class LyInfo
    {
        public int Id;
        public string Name = "";
        public int Mana, Flow;
        public bool HasWorkshop, HasShrine;
    }

    public partial class UnitInfo
    {
        public int Id;
        public string Name = "";
        public int UType;
        public int Level;
        public int Leyline;
        public int Mp, MpCap, Cs, Funds;
    }

    public partial class PactInfo
    {
        public string Type = "";
        public string A = "", B = "";
        public int TurnsLeft;
    }

    public static WorldSnapshotData From(Engine.KsgWorld w, Engine.WorldCampaign c, Engine.WorldAdvanced a)
    {
        var s = new WorldSnapshotData
        {
            Day = c.Day, Phase = c.Phase, Turn = w.Round,
            WinnerMasterId = c.WinnerMasterId, Finished = c.Finished,
        };
        foreach (var ly in w.Leylines)
            s.Leylines.Add(new LyInfo { Id = ly.Id, Name = ly.Name, Mana = ly.Mana, Flow = ly.Flow, HasWorkshop = ly.HasWorkshop, HasShrine = ly.HasShrine });
        foreach (var u in w.Units)
            s.Units.Add(new UnitInfo { Id = u.Id, Name = u.Name, UType = u.UType, Level = u.Level, Leyline = u.CurrentLeyline, Mp = u.MpCur, MpCap = u.MpCap, Cs = u.Cs, Funds = a.FundOf(u.Id) });
        foreach (var p in c.Pacts)
            s.Pacts.Add(new PactInfo { Type = p.Type.ToString(), A = NameOf(w, p.AUnitId), B = NameOf(w, p.BUnitId), TurnsLeft = p.TurnsLeft });
        return s;
    }

    private static string NameOf(Engine.KsgWorld w, int id) => w.Units.Find(u => u.Id == id)?.Name ?? $"#{id}";

    public string ToJson() => System.Text.Json.JsonSerializer.Serialize(this);
    public static WorldSnapshotData FromJson(string json) =>
        System.Text.Json.JsonSerializer.Deserialize<WorldSnapshotData>(json);
}
