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
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
        }

        public override async void BeginInit()
        {
            base.BeginInit();

            ConsoleHelper.Show();

            CancellationToken ct = CancellationToken.None;

            WiFiManager wifi = new(ct);
            await wifi.StartAsync();

            UdpLogService udpLogService = new(wifi, ct);
            udpLogService.Start();

            UdpBusService udpBusService = new(wifi, ct);
            udpBusService.Start();
        }
    }
}
