using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace OpenTCU.LoggingUtils
{
    public class BusFileWriter
    {
        private readonly string _filePath;
        private readonly UdpBusService _bus;
        private readonly CancellationToken _ct;
        private Task? _task;

        public BusFileWriter(UdpBusService bus, CancellationToken ct)
        {
            _bus = bus;
            _ct = ct;
            _filePath = Path.Combine(Environment.CurrentDirectory, $"buslog_{DateTime.Now:yyyyMMdd_HHmmss}.asc");
            Logger.WriteLine($"Writing CAN bus data to: {_filePath}");
        }

        public void Start()
        {
            if (_task is not null)
                throw new Exception("BusFileWriter is already running.");
            _task = Task.Run(async () => await WriteLoopAsync(), _ct);
        }

        private async Task WriteLoopAsync()
        {
        }
    }
}
