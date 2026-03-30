using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using ManagedNativeWifi;
using System.Net.NetworkInformation;
using OpenTCU.LoggingUtils;
using System.Buffers.Binary;

if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
    Console.SetBufferSize(120, 1000);

CancellationTokenSource cts = new();

#region Connect to OpenTCU Wi-Fi Network
if (NativeWifi.EnumerateInterfaces().Count() == 0)
    throw new Exception("No Wi-Fi interfaces found.");

Logger.WriteLine("Scanning for OpenTCU networks...");
IPAddress? openTCUAddress = null;
IPAddress? clientAddress = null;
while (true)
{
    await NativeWifi.ScanNetworksAsync(TimeSpan.FromSeconds(5));

    if (NativeWifi.EnumerateAvailableNetworks().FirstOrDefault(n => n.Ssid.ToString().StartsWith("OpenTCU")) is not AvailableNetworkPack networkPack)
    {
        Logger.WriteLine("Failed to find OpenTCU network, retrying...");
        await Task.Delay(5000);
        continue;
    }

    Logger.WriteLine($"Connecting to network: {networkPack.Ssid}");

    string profileXml = $@"<?xml version=""1.0""?>
    <WLANProfile xmlns=""http://www.microsoft.com/networking/WLAN/profile/v1"">
        <name>{networkPack.Ssid}</name>
        <SSIDConfig>
            <SSID>
                <name>{networkPack.Ssid}</name>
            </SSID>
        </SSIDConfig>
        <connectionType>ESS</connectionType>
        <connectionMode>manual</connectionMode>
        <MSM>
            <security>
                <authEncryption>
                    <authentication>open</authentication>
                    <encryption>none</encryption>
                    <useOneX>false</useOneX>
                </authEncryption>
            </security>
        </MSM>
    </WLANProfile>";
    NativeWifi.SetProfile(networkPack.InterfaceInfo.Id, ProfileType.AllUser, profileXml, null, true);

    if (!await Task.Run(() => NativeWifi.ConnectNetwork(networkPack.InterfaceInfo.Id, networkPack.Ssid.ToString(), networkPack.BssType)))
    {
        Logger.WriteLine("Failed to connect to network, retrying...");
        await Task.Delay(5000);
        continue;
    }

    Logger.WriteLine($"Successfully connected to {networkPack.Ssid}, waiting for DHCP");
    DateTime dhcpStartTime = DateTime.Now;
    bool gotDhcp = false;
    while (!gotDhcp)
    {
        if (NativeWifi.EnumerateInterfaces().FirstOrDefault(i => i.Id == networkPack.InterfaceInfo.Id) is not InterfaceInfo interfaceInfo)
        {
            Logger.WriteLine("Failed to find Wi-Fi interface, retrying...");
            await Task.Delay(1000);
            continue;
        }

        NetworkInterface? networkInterface = NetworkInterface.GetAllNetworkInterfaces()
            .FirstOrDefault(n => string.Equals(n.Id, interfaceInfo.Id.ToString("B"), StringComparison.OrdinalIgnoreCase));
        if (networkInterface is null || networkInterface.OperationalStatus != OperationalStatus.Up)
        {
            Logger.WriteLine("Wi-Fi interface is not up, retrying...");
            await Task.Delay(1000);
            continue;
        }

        IPInterfaceProperties ipProps = networkInterface.GetIPProperties();
        IPAddress? assignedAddress = ipProps.UnicastAddresses.FirstOrDefault(a => a.Address.AddressFamily == AddressFamily.InterNetwork)?.Address;
        if (assignedAddress is null)
        {
            if ((DateTime.Now - dhcpStartTime).TotalSeconds > 30)
            {
                Logger.WriteLine("Failed to obtain DHCP lease within timeout, retrying...");
                break;
            }

            await Task.Delay(1000);
            continue;
        }
        clientAddress = assignedAddress;

        openTCUAddress = ipProps.GatewayAddresses.Select(g => g.Address).FirstOrDefault(a => a.AddressFamily == AddressFamily.InterNetwork);
        if (openTCUAddress is null)
        {
            Logger.WriteLine("Failed to find gateway address, retrying...");
            await Task.Delay(1000);
            continue;
        }

        Logger.WriteLine($"Obtained IP address: {assignedAddress}");
        gotDhcp = true;
    }
    if (!gotDhcp)
    {
        await Task.Delay(5000);
        continue;
    }

    break;
}

Logger.WriteLine("Establishing connection...");
#endregion

#region UDP streams
IPEndPoint remoteEP = new(IPAddress.Any, 0); // ESP32 AP is often always 192.168.4.1

using UdpClient udpLogClient = new();
udpLogClient.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
udpLogClient.Client.Bind(new IPEndPoint(clientAddress!, 49152));
_ = Task.Run(() =>
{
    CancellationToken ct = cts.Token;
    while (!ct.IsCancellationRequested)
    {
        try
        {
            byte[] recievedBytes = udpLogClient.Receive(ref remoteEP);
            if (recievedBytes.Length > 0)
            {
                string message = System.Text.Encoding.UTF8.GetString(recievedBytes);
                Logger.Write(message);
            }
        }
        catch (Exception ex) { Logger.WriteLine(ex.Message); }
    }
});

using UdpClient udpBusClient = new();
udpBusClient.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
udpBusClient.Client.Bind(new IPEndPoint(clientAddress!, 49153));
_ = Task.Run(() =>
{
    CancellationToken ct = cts.Token;

    /*TcpListener gvretSrver = new(IPAddress.Any, 3333);
    gvretSrver.Start();
    using TcpClient gvretClient = gvretSrver.AcceptTcpClient();
    using NetworkStream gvretStream = gvretClient.GetStream();*/

    while (!ct.IsCancellationRequested)
    {
        try
        {
            byte[] recievedBytes = udpBusClient.Receive(ref remoteEP);

            if (recievedBytes.Length == Marshal.SizeOf<SCanDump>())
            {
                GCHandle handle = GCHandle.Alloc(recievedBytes, GCHandleType.Pinned);
                try
                {
                    SCanDump canDump = Marshal.PtrToStructure<SCanDump>(handle.AddrOfPinnedObject());

                    string dataString;
                    unsafe
                    {
                        byte* hexData = canDump.Data;
                        byte[] dataBytes = new byte[canDump.Length];
                        Marshal.Copy((IntPtr)hexData, dataBytes, 0, canDump.Length);
                        dataString = BitConverter.ToString(dataBytes).Replace("-", " ");
                    }

                    Logger.WriteLine(
                        $"Timestamp: {canDump.Timestamp}, "
                        + "Bus: " + (canDump.Bus ? "1" : "0") + ", "
                        + $"ID: 0x{canDump.Id:X3}, "
                        + $"EXT: {canDump.IsExtended}, "
                        + $"RTR: {canDump.IsRemote}, "
                        + $"Len: {canDump.Length}, "
                        + $"Data: {dataString}"
                    );

                    // Create GVRET frame
                    byte[] packet = new byte[20];
                    packet[0] = 0xF1; // Sync
                    packet[1] = 0x00; // Can frame command

                    // Timestamp (little-endian, microseconds)
                    // GVRET uses the MSB (Bit 31) of the ID to signal extended
                    uint timestampUs = canDump.Timestamp * 1000; // Convert ms to us
                    BinaryPrimitives.WriteUInt32LittleEndian(packet.AsSpan(2), timestampUs);

                    // ID + Extended flag
                    uint idWithFlags = canDump.Id;
                    if (canDump.IsExtended) idWithFlags |= 0x80000000;
                    BinaryPrimitives.WriteUInt32LittleEndian(packet.AsSpan(6), idWithFlags);

                    packet[10] = canDump.Length; // DLC
                    packet[11] = (byte)(canDump.Bus ? 1 : 0); // Bus

                    // Copy data bytes
                    unsafe
                    {
                        for (int i = 0; i < canDump.Length; i++)
                            packet[12 + i] = canDump.Data[i];
                    }
                }
                catch (Exception ex) { Logger.WriteLine(ex.Message); }
                finally { handle.Free(); }
            }
            else
            {
                Logger.WriteLine($"Received invalid bus data of length: {recievedBytes.Length}");
            }
        }
        catch (Exception ex) { Logger.WriteLine(ex.Message); }
    }

    //gvretSrver.Stop();
});
#endregion

Console.CancelKeyPress += (s, e) =>
{
    Logger.WriteLine("Shutting down...");
    cts.Cancel();
};
cts.Token.WaitHandle.WaitOne();
