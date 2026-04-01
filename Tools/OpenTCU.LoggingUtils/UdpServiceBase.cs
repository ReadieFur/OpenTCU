using System;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

namespace OpenTCU.LoggingUtils
{
    public abstract class UdpServiceBase
    {
        protected readonly WiFiManager _wifi;
        protected readonly int _port;
        protected readonly CancellationToken _ct;
        protected UdpClient? _client;
        private Task? _task;

        public UdpServiceBase(WiFiManager wifi, int port, CancellationToken ct)
        {
            _wifi = wifi;
            _port = port;
            _ct = ct;
        }

        public void Start()
        {
            if (_task is not null)
                throw new Exception($"{GetType().Name} is already running.");
            _task = Task.Run(ServiceLoop);
        }

        private async Task ServiceLoop()
        {
            while (!_ct.IsCancellationRequested)
            {
                if (_wifi.IsConnected && _wifi.ClientAddress != null)
                {
                    try
                    {
                        using (_client = new UdpClient())
                        {
                            // Configure Socket
                            _client.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
                            _client.Client.Bind(new IPEndPoint(_wifi.ClientAddress, _port));

                            //Logger.WriteLine($"{GetType().Name} bound to {_wifi.ClientAddress}:{_port}");

                            while (_wifi.IsConnected)
                            {
                                byte[] recievedBytes = (await _client.ReceiveAsync(_ct)).Buffer;
                                if (recievedBytes.Length > 0)
                                    await ProcessDataAsync(recievedBytes);
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        Logger.WriteLine($"{GetType().Name} Error: {ex.Message}");
                        try { await Task.Delay(2000, _ct); } // Cool down before retry
                        catch (OperationCanceledException) { }
                    }
                }
                else
                {
                    // Wait for WifiManager to restore connection
                    try { await Task.Delay(1000, _ct); }
                    catch (OperationCanceledException) { }
                }
            }
        }

        protected abstract Task ProcessDataAsync(byte[] data);
    }
}
