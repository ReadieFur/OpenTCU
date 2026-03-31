using System.IO;
using System.Runtime.InteropServices;

namespace OpenTCU.Visualiser
{
    internal static class ConsoleHelper
    {
        private const int STD_OUTPUT_HANDLE = -11;
        private const uint ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004;
        private const uint SC_CLOSE = 0xF060;
        private const uint MF_GRAYED = 0x00000001;
        private const int SW_HIDE = 0;
        private const int SW_SHOW = 5;

        private static object _lock = new();

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool AllocConsole();

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool FreeConsole();

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr GetStdHandle(int nStdHandle);

        [DllImport("kernel32.dll")]
        private static extern bool GetConsoleMode(IntPtr hConsoleHandle, out uint lpMode);

        [DllImport("kernel32.dll")]
        private static extern bool SetConsoleMode(IntPtr hConsoleHandle, uint dwMode);

        [DllImport("user32.dll")]
        private static extern IntPtr GetSystemMenu(IntPtr hWnd, bool bRevert);

        [DllImport("user32.dll")]
        private static extern bool EnableMenuItem(IntPtr hMenu, uint uIDEnableItem, uint uEnable);

        [DllImport("kernel32.dll")]
        private static extern IntPtr GetConsoleWindow();

        [DllImport("user32.dll")]
        private static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

        public static void Create()
        {
            lock (_lock)
            {
                if (GetConsoleWindow() != IntPtr.Zero)
                    return; // Console already exists

                if (!AllocConsole())
                    throw new Exception("Failed to start console backend.");

                // Enable ANSI escape codes for colored output in the console.
                var writer = new StreamWriter(Console.OpenStandardOutput()) { AutoFlush = true };
                Console.SetOut(writer);
                var iStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
                if (GetConsoleMode(iStdOut, out uint outConsoleMode))
                {
                    outConsoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                    SetConsoleMode(iStdOut, outConsoleMode);
                }

                // Get the handle to the Console Window
                IntPtr hConsole = GetConsoleWindow();
                if (hConsole != IntPtr.Zero)
                {
                    // Get the "System Menu" (the one with Move, Size, Close)
                    IntPtr hMenu = GetSystemMenu(hConsole, false);
                    if (hMenu != IntPtr.Zero)
                    {
                        // Disable (Gray out) the Close button
                        EnableMenuItem(hMenu, SC_CLOSE, MF_GRAYED);
                    }
                }

                Console.SetBufferSize(120, 1000);
                Console.Title = "OpenTCU Live Logger";
            }
        }

        public static void Destroy()
        {
            lock (_lock)
            {
                if (GetConsoleWindow() == IntPtr.Zero)
                    return; // No console to destroy
                FreeConsole();
            }
        }

        public static void Show()
        {
            lock (_lock)
            {
                var hConsole = GetConsoleWindow();
                if (hConsole != IntPtr.Zero)
                    ShowWindow(hConsole, SW_SHOW);
            }
        }

        public static void Hide()
        {
            lock (_lock)
            {
                var hConsole = GetConsoleWindow();
                if (hConsole != IntPtr.Zero)
                    ShowWindow(hConsole, SW_HIDE);
            }
        }
    }
}
