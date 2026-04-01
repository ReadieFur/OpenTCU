using System.Globalization;
using System.Threading;

namespace OpenTCU.LoggingUtils
{
    public class KayakFileLogger : FileWriterBase
    {
        public KayakFileLogger(UdpBusService bus, CancellationToken ct) : base(bus, ".kayak.log", ct) { }

        protected override string WriteFrame(SCanDump frame)
        {
            string timestamp = (frame.Timestamp / 1_000.0d).ToString("F6", CultureInfo.InvariantCulture);

            string data = $"({timestamp}) "
                + "can" + (frame.Bus ? "1" : "0") + " "
                + $"{frame.Id:X3}#";

            for (int i = 0; i < frame.Length; i++)
                unsafe { data += frame.Data[i].ToString("X2"); }

            return data + "\n";
        }
    }
}
