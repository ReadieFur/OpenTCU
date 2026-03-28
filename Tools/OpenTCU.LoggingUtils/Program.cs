using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using ManagedNativeWifi;
using System.Net.NetworkInformation;
using OpenTCU.LoggingUtils;

//START:

#region Connect to OpenTCU Wi-Fi Network
if (NativeWifi.EnumerateInterfaces().Count() == 0)
    throw new Exception($"[{DateTime.Now:HH:mm:ss}] No Wi-Fi interfaces found.");

Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] Scanning for OpenTCU networks...");
while (true)
{
    await NativeWifi.ScanNetworksAsync(TimeSpan.FromSeconds(5));

    if (NativeWifi.EnumerateAvailableNetworks().FirstOrDefault(n => n.Ssid.ToString().StartsWith("OpenTCU")) is not AvailableNetworkPack networkPack)
    {
        Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] Failed to find OpenTCU network, retrying...");
        await Task.Delay(5000);
        continue;
    }

    Console.WriteLine($"[{DateTime.Now:HH:mm:ss}]Connecting to network: {networkPack.Ssid}");

    if (!await Task.Run(() => NativeWifi.ConnectNetwork(networkPack.InterfaceInfo.Id, networkPack.Ssid.ToString(), networkPack.BssType)))
    {
        Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] Failed to connect to network, retrying...");
        await Task.Delay(5000);
        continue;
    }

    Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] Successfully connected to {networkPack.Ssid}, waiting for DHCP");
    DateTime dhcpStartTime = DateTime.Now;
    bool gotDhcp = false;
    while (true)
    {
        if (NativeWifi.EnumerateInterfaces().FirstOrDefault(i => i.Id == networkPack.InterfaceInfo.Id) is not InterfaceInfo interfaceInfo)
        {
            Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] Failed to find Wi-Fi interface, retrying...");
            break;
        }

        NetworkInterface? networkInterface = NetworkInterface.GetAllNetworkInterfaces().FirstOrDefault(n => n.Id == interfaceInfo.Id.ToString("B"));
        if (networkInterface is null || networkInterface.OperationalStatus != OperationalStatus.Up)
        {
            Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] Wi-Fi interface is not up, retrying...");
            break;
        }

        IPAddress? ip = networkInterface.GetIPProperties().UnicastAddresses.FirstOrDefault(a => a.Address.AddressFamily == AddressFamily.InterNetwork)?.Address;
        if (ip is null)
        {
            if ((DateTime.Now - dhcpStartTime).TotalSeconds > 30)
            {
                Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] Failed to obtain DHCP lease within timeout, retrying...");
                break;
            }

            await Task.Delay(1000);
            continue;
        }

        Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] Obtained IP address: {ip}, connection successful!");
        gotDhcp = true;
    }
    if (!gotDhcp)
    {
        await Task.Delay(5000);
        continue;
    }

    break;
}
#endregion

#region Connect to UDP streams
IPEndPoint openTCUEndpoint = new(IPAddress.Any, 0); // ESP32 AP is often always 192.168.4.1

CancellationTokenSource cts = new();

using UdpClient udpLogClient = new();
udpLogClient.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
udpLogClient.Client.Bind(new IPEndPoint(IPAddress.Any, 49152));
_ = Task.Run(() =>
{
    CancellationToken ct = cts.Token;
    while (!ct.IsCancellationRequested)
    {
        try
        {
            byte[] recievedBytes = udpLogClient.Receive(ref openTCUEndpoint);
            if (recievedBytes.Length > 0)
            {
                string message = System.Text.Encoding.UTF8.GetString(recievedBytes);
                Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] {message}");
            }
        }
        catch (Exception ex) { Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] {ex.Message}"); }
    }
});

using UdpClient udpBusClient = new();
udpBusClient.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
udpBusClient.Client.Bind(new IPEndPoint(IPAddress.Any, 49153));
_ = Task.Run(() =>
{
    CancellationToken ct = cts.Token;
    while (!ct.IsCancellationRequested)
    {
        try
        {
            byte[] recievedBytes = udpBusClient.Receive(ref openTCUEndpoint);
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

                    Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] "
                        + $"Timestamp: {canDump.Timestamp}, "
                        + $"Bus: {canDump.Bus}, "
                        + $"ID: 0x{canDump.Id:X3}, "
                        + $"EXT: {canDump.IsExtended}, "
                        + $"RTR: {canDump.IsRemote}, "
                        + $"Len: {canDump.Length}, "
                        + $"Data: {dataString}"
                    );

                    // TODO: Stream data to savvycan from here in a format it can understand (i.e. GVRET).
                }
                catch (Exception ex) { Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] {ex.Message}"); }
                finally { handle.Free(); }
            }
            else
            {
                Console.WriteLine($"[{DateTime.Now:HH:mm:ss} Received invalid bus data of length {recievedBytes.Length}]");
            }
        }
        catch (Exception ex) { Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] {ex.Message}"); }
    }
});

Console.WriteLine("Press enter to exit...");
Console.ReadLine();
cts.Cancel();
#endregion
