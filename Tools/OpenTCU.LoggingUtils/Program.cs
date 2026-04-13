using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using OpenTCU.LoggingUtils;

if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
{
    try { Console.SetBufferSize(120, 1000); } catch { }
    Console.Title = "OpenTCU Live Logger";
}

CancellationTokenSource cts = new();

WiFiManager wifiManager = new(cts.Token);
wifiManager.Start();

UdpLogService udpLogService = new(wifiManager, cts.Token);
udpLogService.Start();

UdpBusService udpBusService = new(wifiManager, cts.Token) { LogToConsole = true };
udpBusService.Start();

Dictionary<Type, (FileWriterBase, CancellationTokenSource)> fileWriters = new();

void Quit()
{
    Logger.WriteLine("Shutting down...");
    cts.Cancel();
    foreach (var writerInfo in fileWriters.Values)
    {
        writerInfo.Item2.Cancel();
        writerInfo.Item1.Dispose(); // Should automatically be triggered by the cancellation token, but calling her to keep the program open until we can garantee that the file writer has stopped.
    }
}

void ToggleFileWriter<T>() where T : FileWriterBase
{
    if (fileWriters.TryGetValue(typeof(T), out var writerInfo))
    {
        Logger.WriteLine($"Stopping {typeof(T).Name}.");
        writerInfo.Item2.Cancel();
        fileWriters.Remove(typeof(T));
    }
    else
    {
        CancellationTokenSource cts = new();
        //Logger.WriteLine("Starting {typeof(T).Name}."); // Log is output by the BusFileWriter constructor
        T fw = (T)Activator.CreateInstance(typeof(T), udpBusService, cts.Token)!;
        fw.Start();
        fileWriters[typeof(T)] = (fw, cts);
    }
}


void Help()
{
    StringBuilder sb = new();
    sb.AppendLine("==== OpenTCU Live Logger ====");
    sb.AppendLine("Press the following keys to toggle logging options:");
    sb.AppendLine("\tl - Toggle OpenTCU logs.");
    sb.AppendLine("\tb - Toggle CAN bus data logs.");
    sb.AppendLine("\tg - Toggle GVRET bus file logging.");
    sb.AppendLine("\tk - Toggle Kayak bus file logging.");
    sb.AppendLine("\th - Show this help message.");
    sb.AppendLine("\tq - Quit the application.");
    Console.WriteLine(sb.ToString());
}

//Console.CancelKeyPress += (_, _) => Quit(); // This seems to hang for some reason.
Help();
while (!cts.IsCancellationRequested)
{
    switch (Console.ReadKey(true).KeyChar)
    {
        case 'l':
            {
                Logger.WriteLine((!udpLogService.LogToConsole ? "Enabling" : "Disabling") + " console logging for general log messages.");
                udpLogService.LogToConsole = !udpLogService.LogToConsole;
                break;
            }
        case 'b':
            {
                Logger.WriteLine((!udpBusService.LogToConsole ? "Enabling" : "Disabling") + " console logging for CAN bus data.");
                udpBusService.LogToConsole = !udpBusService.LogToConsole;
                break;
            }
        case 'g':
            {
                ToggleFileWriter<GvretFileLogger>();
                break;
            }
        case 'k':
            {
                ToggleFileWriter<KayakFileLogger>();
                break;
            }
        case 'h':
            {
                Help();
                break;
            }
        case 'q':
            {
                Quit();
                break;
            }
        default:
            break;
    }
}

//await Task.Delay(Timeout.Infinite, cts.Token);
