namespace ReadieFur.OpenTCU.Client
{
    public partial class App : Application
    {
        public App()
        {
            InitializeComponent();

#if WINDOWS && DEBUG
            //https://stackoverflow.com/questions/72399551/maui-net-set-window-size
            Microsoft.Maui.Handlers.WindowHandler.Mapper.AppendToMapping(nameof(IWindow), (handler, view) =>
            {
                //Set the window to 1280x720 (my monitor is massive and the default size is 75%~ in all directions).
                var mauiWindow = handler.VirtualView;
                var nativeWindow = handler.PlatformView;
                nativeWindow.Activate();
                IntPtr windowHandle = WinRT.Interop.WindowNative.GetWindowHandle(nativeWindow);
                var windowId = Microsoft.UI.Win32Interop.GetWindowIdFromWindow(windowHandle);
                var appWindow = Microsoft.UI.Windowing.AppWindow.GetFromWindowId(windowId);
                appWindow.Resize(new Windows.Graphics.SizeInt32(1280, 720));
                appWindow.Move(new Windows.Graphics.PointInt32((5120 / 2) - (1280 / 2), (1440 / 2) - (720 / 2)));
            });
#endif

            MainPage = new AppShell();
        }
    }
}
