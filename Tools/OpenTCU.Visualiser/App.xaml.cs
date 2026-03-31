using System.Windows;

namespace OpenTCU.Visualiser
{
    public partial class App : Application
    {
        protected override async void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);
            ConsoleHelper.Create();
            ConsoleHelper.Hide();
        }
    }
}
