using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;

namespace OpenTCU.LoggingUtils
{
    public abstract class FileWriterBase
    {
        private readonly string _filePath;
        private readonly UdpBusService _bus;
        private readonly CancellationToken _ct;
        private bool _started = false;
        private StreamWriter _writer;
        private Task? _task;
        protected readonly Channel<string> _channel = Channel.CreateUnbounded<string>(new() { SingleReader = true, SingleWriter = false });

        public FileWriterBase(UdpBusService bus, string extension, CancellationToken ct)
        {
            _bus = bus;
            _ct = ct;

            string recordingsDataDir = Path.Combine(Environment.CurrentDirectory, "..", "..", "..", "..", "..", "Recordings", "data");
            _filePath = Path.Combine(
                (Directory.Exists(recordingsDataDir) ? recordingsDataDir : Environment.CurrentDirectory),
                $"OpenTCU_{DateTime.Now:yyyyMMdd_HHmmss}{extension}");

            Logger.WriteLine($"Writing CAN bus data to: {_filePath}");
            _writer = new(_filePath, append: false, encoding: Encoding.UTF8) { AutoFlush = false };
        }

        public void Dispose()
        {
            _bus.CanDumpReceived -= Bus_CanDumpReceived;
            try
            {
                _writer.Flush();
                _writer.Dispose();
            }
            catch (ObjectDisposedException) { }
        }

        public async void Start()
        {
            lock (this)
            {
                if (_ct.IsCancellationRequested)
                    throw new InvalidOperationException("Cancellation has already been requested.");

                if (_started)
                    throw new Exception("BusFileWriter is already running.");
                _started = true;
            }

            _task = Task.Run(WriterTask);

            _channel.Writer.TryWrite(WriteHeader());

            _bus.CanDumpReceived += Bus_CanDumpReceived;
        }

        private async Task WriterTask()
        {
            try
            {
                uint lineCount = 0;
                Stopwatch lastFlush = Stopwatch.StartNew();

                await foreach (string line in _channel.Reader.ReadAllAsync(_ct))
                {
                    await _writer.WriteAsync(line);

                    // Flush every X lines to gaurantee data is written to disk without being truncated (i.e. always end a write in a newline instead of possibly outputting partial data).
                    if (++lineCount >= 1000 || lastFlush.Elapsed >= TimeSpan.FromSeconds(5))
                    {
                        lineCount = 0;
                        lastFlush.Restart();
                        await _writer.FlushAsync();
                    }
                }
            }
            catch (OperationCanceledException) { /* _ct triggered */ }
            finally { Dispose(); }
        }

        private void Bus_CanDumpReceived(object? sender, SCanDump frame)
        {
            if (_ct.IsCancellationRequested)
                return;

            _channel.Writer.TryWrite(WriteFrame(frame));
        }

        protected virtual string WriteHeader() => string.Empty;

        protected abstract string WriteFrame(SCanDump frame);
    }
}
