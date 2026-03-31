using System;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using ManagedNativeWifi;

namespace OpenTCU.LoggingUtils
{
    public class WiFiManager
    {
        private const string SSID_PREFIX = "OpenTCU";
        private readonly CancellationToken _ct;
        private Task? _monitorTask;

        public IPAddress? OpenTCUAddress { get; private set; }
        public IPAddress? ClientAddress { get; private set; }
        public bool IsConnected { get; private set; }

        public WiFiManager(CancellationToken ct)
        {
            _ct = ct;
        }

        public async Task StartAsync()
        {
            if (!NativeWifi.EnumerateInterfaces().Any())
                throw new Exception("No Wi-Fi interfaces found.");

            if (_monitorTask is not null)
                throw new Exception("WiFiManager is already running.");

            // Run the monitor loop in the background
            _monitorTask = Task.Run(MonitorLoop, _ct);
        }

        private async Task MonitorLoop()
        {
            while (!_ct.IsCancellationRequested)
            {
                AvailableNetworkPack? connectedNetwork = NativeWifi.EnumerateAvailableNetworks().FirstOrDefault(n =>
                    n.InterfaceInfo.State == InterfaceState.Connected
                    && n.Ssid.ToString().StartsWith(SSID_PREFIX));

                if (connectedNetwork is null)
                {
                    IsConnected = false;
                    Logger.WriteLine("OpenTCU Wi-Fi not detected or disconnected. Attempting connection...");
                    await AttemptConnection();
                }
                else if (OpenTCUAddress is null || ClientAddress is null || !IsConnected)
                {
                    NetworkInterface networkInterface = NetworkInterface.GetAllNetworkInterfaces()
                        .FirstOrDefault(n => string.Equals(n.Id, connectedNetwork.InterfaceInfo.Id.ToString("B"), StringComparison.OrdinalIgnoreCase))!;

                    IPInterfaceProperties ipProps = networkInterface.GetIPProperties();
                    IPAddress? clientAddr = ipProps.UnicastAddresses.FirstOrDefault(a => a.Address.AddressFamily == AddressFamily.InterNetwork)?.Address;
                    IPAddress? gatewayAddr = ipProps.GatewayAddresses
                        .Select(g => g.Address)
                        .FirstOrDefault(a => a.AddressFamily == AddressFamily.InterNetwork);

                    if (gatewayAddr is null || clientAddr is null)
                        throw new Exception("Network was connected but failed to obtain properties.");

                    OpenTCUAddress = gatewayAddr;
                    ClientAddress = clientAddr;
                    IsConnected = true;
                    Logger.WriteLine($"Connected! Client: {ClientAddress}, Gateway: {OpenTCUAddress}");
                }

                await Task.Delay(5000, _ct);
            }
        }

        private async Task AttemptConnection()
        {
            try
            {
                await NativeWifi.ScanNetworksAsync(TimeSpan.FromSeconds(5));

                AvailableNetworkPack? networkPack = NativeWifi.EnumerateAvailableNetworks()
                    .FirstOrDefault(n => n.Ssid.ToString().StartsWith(SSID_PREFIX));
                if (networkPack is null)
                    return;

                string profileXml = CreateProfileXml(networkPack.Ssid.ToString());
                NativeWifi.SetProfile(networkPack.InterfaceInfo.Id, ProfileType.AllUser, profileXml, null, true);

                bool success = await Task.Run(() => NativeWifi.ConnectNetwork(
                    networkPack.InterfaceInfo.Id,
                    networkPack.Ssid.ToString(),
                    networkPack.BssType));

                if (success)
                    await WaitForDhcp(networkPack.InterfaceInfo.Id);
            }
            catch (Exception ex) { Logger.WriteLine($"Wi-Fi Error: {ex.Message}"); }
        }

        private async Task WaitForDhcp(Guid interfaceId)
        {
            DateTime startTime = DateTime.Now;
            while ((DateTime.Now - startTime).TotalSeconds < 30)
            {
                NetworkInterface? networkInterface = NetworkInterface.GetAllNetworkInterfaces()
                    .FirstOrDefault(n => string.Equals(n.Id, interfaceId.ToString("B"), StringComparison.OrdinalIgnoreCase));

                if (networkInterface?.OperationalStatus == OperationalStatus.Up)
                {
                    IPInterfaceProperties ipProps = networkInterface.GetIPProperties();
                    IPAddress? addr = ipProps.UnicastAddresses
                        .FirstOrDefault(a => a.Address.AddressFamily == AddressFamily.InterNetwork)?.Address;

                    IPAddress? gateway = ipProps.GatewayAddresses
                        .Select(g => g.Address)
                        .FirstOrDefault(a => a.AddressFamily == AddressFamily.InterNetwork);

                    if (addr is not null && gateway is not null)
                    {
                        ClientAddress = addr;
                        OpenTCUAddress = gateway;
                        IsConnected = true;
                        Logger.WriteLine($"Connected! Client: {ClientAddress}, Gateway: {OpenTCUAddress}");
                        return;
                    }
                }

                await Task.Delay(1000, _ct);
            }
        }

        private string CreateProfileXml(string ssid) => $@"
            <WLANProfile xmlns=""http://www.microsoft.com/networking/WLAN/profile/v1"">
                <name>{ssid}</name>
                <SSIDConfig><SSID><name>{ssid}</name></SSID></SSIDConfig>
                <connectionType>ESS</connectionType>
                <connectionMode>manual</connectionMode>
                <MSM><security><authEncryption>
                    <authentication>open</authentication>
                    <encryption>none</encryption>
                    <useOneX>false</useOneX>
                </authEncryption></security></MSM>
            </WLANProfile>";
    }
}
