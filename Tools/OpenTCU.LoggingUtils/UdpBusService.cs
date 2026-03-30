using System.Buffers.Binary;
using System.Runtime.InteropServices;

namespace OpenTCU.LoggingUtils
{
    internal class UdpBusService : UdpServiceBase
    {
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

                    Logger.WriteLine(
                        $"Timestamp: {canDump.Timestamp}, "
                        + "Bus: " + (canDump.Bus ? "1" : "0") + ", "
                        + $"ID: 0x{canDump.Id:X3}, "
                        + $"EXT: {canDump.IsExtended}, "
                        + $"RTR: {canDump.IsRemote}, "
                        + $"Len: {canDump.Length}, "
                        + $"Data: {dataString}"
                    );

                    // Create GVRET frame
                    byte[] packet = new byte[20];
                    packet[0] = 0xF1; // Sync
                    packet[1] = 0x00; // Can frame command

                    // Timestamp (little-endian, microseconds)
                    // GVRET uses the MSB (Bit 31) of the ID to signal extended
                    uint timestampUs = canDump.Timestamp * 1000; // Convert ms to us
                    BinaryPrimitives.WriteUInt32LittleEndian(packet.AsSpan(2), timestampUs);

                    // ID + Extended flag
                    uint idWithFlags = canDump.Id;
                    if (canDump.IsExtended) idWithFlags |= 0x80000000;
                    BinaryPrimitives.WriteUInt32LittleEndian(packet.AsSpan(6), idWithFlags);

                    packet[10] = canDump.Length; // DLC
                    packet[11] = (byte)(canDump.Bus ? 1 : 0); // Bus

                    // Copy data bytes
                    unsafe
                    {
                        for (int i = 0; i < canDump.Length; i++)
                            packet[12 + i] = canDump.Data[i];
                    }
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
