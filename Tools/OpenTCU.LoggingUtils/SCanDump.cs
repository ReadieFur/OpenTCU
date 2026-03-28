using System.Runtime.InteropServices;

namespace OpenTCU.LoggingUtils
{
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct SCanDump
    {
        // In ESP-IDF (C++), ulong is 32-bit (4 bytes). Use uint in C#.
        public uint Timestamp;

        // Marshalling as I1 ensures it only takes 1 byte, matching C++ bool
        [MarshalAs(UnmanagedType.I1)]
        public bool Bus;

        public uint Id;

        [MarshalAs(UnmanagedType.I1)]
        public bool IsExtended;

        [MarshalAs(UnmanagedType.I1)]
        public bool IsRemote;

        public byte Length;

        // Fixed-size buffers require the 'unsafe' keyword
        public unsafe fixed byte Data[8];
    }
}
