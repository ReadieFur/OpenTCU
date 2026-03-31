//#define LOG_CLASS

using System;
using System.Diagnostics;

namespace OpenTCU.LoggingUtils
{
    public static class Logger
    {
        private static string BuildLog(string message)
        {
            string log = $"[{DateTime.Now:HH:mm:ss:fff}]";
#if LOG_CLASS
            string? @class = new StackFrame(2).GetMethod()?.DeclaringType?.Name;
            if (@class is not null)
                log += $" {@class} |";
#endif
            log += $" {message}";
            return log;
        }

        public static void WriteLine(string message) => Console.WriteLine(BuildLog(message));

        public static void Write(string message) => Console.Write(BuildLog(message));
    }
}
