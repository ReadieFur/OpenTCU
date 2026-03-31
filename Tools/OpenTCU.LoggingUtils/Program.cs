using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using OpenTCU.LoggingUtils;

if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
{
    Console.SetBufferSize(120, 1000);
    Console.Title = "OpenTCU Live Logger";
}

CancellationTokenSource cts = new();

WiFiManager wifiManager = new(cts.Token);
wifiManager.Start();

UdpLogService udpLogService = new(wifiManager, cts.Token);
udpLogService.Start();

UdpBusService udpBusService = new(wifiManager, cts.Token) { LogToConsole = true };
udpBusService.Start();

Console.CancelKeyPress += (s, e) =>
{
    Logger.WriteLine("Shutting down...");
    cts.Cancel();
};
await Task.Delay(Timeout.Infinite, cts.Token);
