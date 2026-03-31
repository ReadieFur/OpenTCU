using OpenTCU.LoggingUtils;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace OpenTCU.Visualiser
{
    public partial class MainWindow : Window
    {
        private ViewModel BindingContext => (ViewModel)DataContext;

        public MainWindow()
        {
            InitializeComponent();
        }

        // For now always connect to a live instance but in the future add a toggle to switch between live and file mode.
        public override async void BeginInit()
        {
            base.BeginInit();

            ConsoleHelper.Show();

            CancellationToken ct = CancellationToken.None;

            WiFiManager wifiManager = new(ct);
            wifiManager.Start();

            UdpLogService udpLogService = new(wifiManager, ct);
            udpLogService.Start();

            UdpBusService udpBusService = new(wifiManager, ct) { LogToConsole = false };
            udpBusService.CanDumpReceived += (s, e) => Dispatcher.Invoke(() => OnCanFrameReceived(s, e));
            udpBusService.Start();
        }

        private void OnCanFrameReceived(object? sender, SCanDump frame)
        {
            BindingContext.Frames.Add(frame);
        }
    }
}
