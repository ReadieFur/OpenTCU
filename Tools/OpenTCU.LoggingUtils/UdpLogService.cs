using System.Text;

namespace OpenTCU.LoggingUtils
{
    internal class UdpLogService : UdpServiceBase
    {
        public UdpLogService(WiFiManager wifi, CancellationToken ct) : base(wifi, 49152, ct) { }

        protected override async Task ProcessDataAsync(byte[] data)
        {
            string message = Encoding.UTF8.GetString(data);
            Logger.Write(message);
        }
    }
}
