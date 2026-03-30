using System.Net;
using System.Net.Sockets;

namespace OpenTCU.LoggingUtils
{
    internal class GvretTcpService
    {
        private readonly UdpBusService _bus;
        private readonly int _port = 3333;
        private readonly CancellationToken _ct;
        private Task? _task;
        private TcpListener _listener;

        public GvretTcpService(UdpBusService bus, CancellationToken ct)
        {
            _bus = bus;
            _ct = ct;
            _listener = new TcpListener(IPAddress.Any, _port);
        }

        public void Start()
        {
            if (_task is not null)
                throw new Exception("TCP Service already started.");
            _task = Task.Run(ListenLoop, _ct);
        }

        private async Task ListenLoop()
        {
            try
            {
                _listener.Start();
                Logger.WriteLine($"SavvyCAN TCP Server started on port {_port}");

                while (!_ct.IsCancellationRequested)
                {
                    // Pass the cancellation token to the accept call
                    using TcpClient client = await _listener.AcceptTcpClientAsync(_ct);
                    Logger.WriteLine("SavvyCAN client connected.");

                    // Handle the client in a sub-loop
                    await HandleClientAsync(client);

                    Logger.WriteLine("SavvyCAN client disconnected.");
                }
            }
            catch (OperationCanceledException) { /* Normal shutdown */ }
            catch (Exception ex)
            {
                Logger.WriteLine($"TCP Server Error: {ex.Message}");
            }
            finally
            {
                _listener.Stop();
            }
        }

        private async Task HandleClientAsync(TcpClient client)
        {
            using NetworkStream stream = client.GetStream();

            // Local helper to pipe data from the Bus Event to the TCP Stream
            EventHandler<SCanDump> onData = (sender, data) =>
            {
                try
                {
                    byte[] packet = Helpers.CanDumpToGvretFrame(data);
                    stream.Write(packet, 0, packet.Length);
                }
                catch
                {
                    // If write fails (client disconnected), we'll catch it in the loop below
                }
            };

            // Subscribe to the Bus Event
            _bus.CanDumpReceived += onData;

            try
            {
                // Stay in this loop until the client disconnects or program cancels
                byte[] buffer = new byte[1];
                while (!_ct.IsCancellationRequested && client.Connected)
                {
                    // We check if the client is still alive by attempting a 0-byte peek or small read
                    if (stream.DataAvailable)
                    {
                        // SavvyCAN might send commands (like reset), 
                        // for now we just clear the buffer
                        await stream.ReadExactlyAsync(buffer, 0, 1, _ct);
                    }
                    await Task.Delay(100, _ct);
                }
            }
            finally
            {
                // CRITICAL: Unsubscribe so we don't leak memory or send data to a dead socket
                _bus.CanDumpReceived -= onData;
            }
        }
    }
}
