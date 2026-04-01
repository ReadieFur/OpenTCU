using System.Collections.Generic;
using System.Threading;

namespace OpenTCU.LoggingUtils
{
    public class GvretFileLogger : FileWriterBase
    {
        public GvretFileLogger(UdpBusService bus, CancellationToken ct) : base(bus, ".gvret.csv", ct) {}

        protected override string WriteHeader()
        {
            return "Timestamp,Bus,ID,IsExtended,IsRemote,Length,D0,D1,D2,D3,D4,D5,D6,D7\n";
        }

        protected override string WriteFrame(SCanDump frame)
        {
            List<string> data = [
                (frame.Timestamp / 1_000.0d).ToString(),
                frame.Bus ? "1" : "0",
                frame.Id.ToString("X3"),
                frame.IsExtended ? "1" : "0",
                frame.IsRemote ? "1" : "0",
                frame.Length.ToString()
            ];

            for (int i = 0; i < frame.Length; i++)
                unsafe { data.Add(frame.Data[i].ToString("X2")); }
            for (int i = frame.Length - 1; i < 7; i++)
                data.Add(string.Empty);

            return string.Join(',', data) + "\n";
        }
    }
}
