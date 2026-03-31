using System.Runtime.InteropServices;

namespace OpenTCU.LoggingUtils
{
    internal class UdpBusService : UdpServiceBase
    {
        public event EventHandler<SCanDump>? CanDumpReceived;

        public UdpBusService(WiFiManager wifi, CancellationToken ct) : base(wifi, 49153, ct) { }

        protected override async Task ProcessDataAsync(byte[] data)
        {
            if (data.Length == Marshal.SizeOf<SCanDump>())
            {
                GCHandle handle = GCHandle.Alloc(data, GCHandleType.Pinned);
                try
                {
                    SCanDump canDump = Marshal.PtrToStructure<SCanDump>(handle.AddrOfPinnedObject());

                    string dataString;
                    unsafe
                    {
                        byte* hexData = canDump.Data;
                        byte[] dataBytes = new byte[canDump.Length];
                        Marshal.Copy((IntPtr)hexData, dataBytes, 0, canDump.Length);
                        dataString = BitConverter.ToString(dataBytes).Replace("-", " ");
                    }

                    //Logger.WriteLine(
                    //    $"Timestamp: {canDump.Timestamp}, "
                    //    + "Bus: " + (canDump.Bus ? "1" : "0") + ", "
                    //    + $"ID: 0x{canDump.Id:X3}, "
                    //    + $"EXT: {canDump.IsExtended}, "
                    //    + $"RTR: {canDump.IsRemote}, "
                    //    + $"Len: {canDump.Length}, "
                    //    + $"Data: {dataString}"
                    //);

                    CanDumpReceived?.Invoke(this, canDump);
                }
                catch (Exception ex) { Logger.WriteLine(ex.Message); }
                finally { handle.Free(); }
            }
            else
            {
                Logger.WriteLine($"Received invalid bus data of length: {data.Length}");
            }
        }
    }
}
