using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace OpenTCU.LoggingUtils
{
    public class UdpLogService : UdpServiceBase
    {
        public bool LogToConsole = true;

        public UdpLogService(WiFiManager wifi, CancellationToken ct) : base(wifi, 49152, ct) { }

        protected override async Task ProcessDataAsync(byte[] data)
        {
            if (!LogToConsole)
                return;
            string message = Encoding.UTF8.GetString(data);
            Logger.Write(message);
        }
    }
}
