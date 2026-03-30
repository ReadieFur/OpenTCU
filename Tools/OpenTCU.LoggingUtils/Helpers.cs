using System.Buffers.Binary;

namespace OpenTCU.LoggingUtils
{
    internal static class Helpers
    {
        public static byte[] CanDumpToGvretFrame(SCanDump canDump)
        {
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

            return packet;
        }
    }
}
