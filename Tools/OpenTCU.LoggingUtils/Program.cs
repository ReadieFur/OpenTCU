using System.Runtime.InteropServices;
using OpenTCU.LoggingUtils;

if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
    Console.SetBufferSize(120, 1000);

CancellationTokenSource cts = new();

WiFiManager wifi = new(cts.Token);
await wifi.StartAsync();

UdpLogService udpLogService = new(wifi, cts.Token);
//udpLogService.Start();

UdpBusService udpBusService = new(wifi, cts.Token);
//udpBusService.Start();

GvretTcpService gvretTcpService = new(udpBusService, cts.Token);
gvretTcpService.Start();

Console.CancelKeyPress += (s, e) =>
{
    Logger.WriteLine("Shutting down...");
    cts.Cancel();
};
await Task.Delay(Timeout.Infinite, cts.Token);
