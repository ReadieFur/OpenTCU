using Plugin.BLE;
using Plugin.BLE.Abstractions.Contracts;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.CompilerServices;

namespace ReadieFur.OpenTCU.Client
{
    public partial class MainPage : ContentPage
    {
        private BLEWrapper _bleWrapper = new();
        private IDevice? _device = null;
        private MainPageViewModel _viewModel => (MainPageViewModel)BindingContext;

        public MainPage()
        {
            InitializeComponent();

            _bleWrapper.Adapter.ScanTimeoutElapsed += Adapter_ScanTimeoutElapsed;
            _bleWrapper.Adapter.DeviceDiscovered += OnBleDeviceDiscovered;
            _bleWrapper.Adapter.DeviceDisconnected += OnDeviceDisconnected;

            ScanButton.Clicked += OnScannerClicked_Scan;
        }

        private void ContentPage_Loaded(object sender, EventArgs e)
        {
            Debug.WriteLine("MainPage loaded.");

            //Check if we are already connected to a valid device.
            if (_device is null)
                return;

            IDevice? device = _bleWrapper.Adapter.ConnectedDevices.FirstOrDefault(d => d.Name == null || d.Name.StartsWith("OpenTCU"));
            if (device is null)
                return;
            _device = device;

            DeviceList.Clear();

            Button button = new()
            {
                Text = device.Name,
                HorizontalOptions = LayoutOptions.Fill
            };
            button.BindingContext = device;

            /*button.Clicked += OnDeviceButtonClicked_Disconnect;
            button.IsEnabled = true;*/
            button.IsEnabled = false;

            DeviceList.Add(button);

            ScanButton.Text = "Connected";
            //DevicePanel.IsEnabled = true;
        }

        private async void ContentPage_Unloaded(object sender, EventArgs e)
        {
            if (_bleWrapper.Adapter.IsScanning)
                await _bleWrapper.Adapter.StopScanningForDevicesAsync();
        }

        private void Adapter_ScanTimeoutElapsed(object? sender, EventArgs e)
        {
            if (ScanButton.Text != "Scanning...")
                return;
            ScanButton.Text = "Scan for devices";
        }

        private async void OnDeviceDisconnected(object? sender, Plugin.BLE.Abstractions.EventArgs.DeviceEventArgs e)
        {
            if (e.Device != _device)
                return;
            _device = null;

            await Dispatcher.DispatchAsync(() =>
            {
                ScanButton.Text = "Scan for devices";
                ScanButton.IsEnabled = true;
                //DevicePanel.IsEnabled = false;
            });
        }

        private void OnBleDeviceDiscovered(object? sender, Plugin.BLE.Abstractions.EventArgs.DeviceEventArgs args)
        {
            if (args.Device.Name == null || !args.Device.Name.StartsWith("OpenTCU"))
                return;

            Button button = new()
            {
                Text = args.Device.Name,
                HorizontalOptions = LayoutOptions.Fill
            };
            button.BindingContext = args.Device;

            button.Clicked += OnDeviceButtonClicked_Connect;

            DeviceList.Children.Add(button);
        }

        private async void OnDeviceButtonClicked_Connect(object? sender, EventArgs e)
        {
            if (sender is not Button button || button.BindingContext is not IDevice device || _device == device)
                return;

            ScanButton.IsEnabled = false;
            ScanButton.Text = "Connecting...";

            if (_bleWrapper.Adapter.IsScanning)
                await _bleWrapper.Adapter.StopScanningForDevicesAsync();

            if (_device is not null)
                await _bleWrapper.Adapter.DisconnectDeviceAsync(_device);

            //Lock the UI to prevent connection to multiple devices during this connection attempt.
            for (int i = 0; i < DeviceList.Children.Count; i++)
                if (DeviceList.Children[i] is Button b)
                    b.IsEnabled = false;

            Task connectionTask = _bleWrapper.Adapter.ConnectToDeviceAsync(device);
            await connectionTask;

            if (!connectionTask.IsCompletedSuccessfully)
            {
                //TODO: Alert user of connection failure.
                Debug.WriteLine("Failed to connect to device.");
                ScanButton.IsEnabled = true;
                ScanButton.Text = "Scan for devices";
                for (int i = 0; i < DeviceList.Children.Count; i++)
                    if (DeviceList.Children[i] is Button b)
                        b.IsEnabled = true;
                return;
            }

            Debug.WriteLine("Connected to device.");
            _device = device;

            ScanButton.Text = "Connected";
            ScanButton.Clicked -= OnScannerClicked_Scan;
            ScanButton.Clicked += OnScannerClicked_Disconnect;
            ScanButton.IsEnabled = true;

            //Remove all buttons from the list except the one that was clicked.
            for (int i = 0; i < DeviceList.Children.Count; i++)
            {
                if (DeviceList.Children[i] != button)
                    DeviceList.Children.RemoveAt(i--);
            }

            /*button.Clicked -= OnDeviceButtonClicked_Connect;
            button.Clicked += OnDeviceButtonClicked_Disconnect;
            button.IsEnabled = true;*/

            //DevicePanel.IsEnabled = true;
        }

        private async void OnDeviceButtonClicked_Disconnect(object? sender, EventArgs e)
        {
            if (sender is not Button button || button.BindingContext is not IDevice device || _device is null || device != _device)
                return;

            await _bleWrapper.Adapter.DisconnectDeviceAsync(_device);
            _device = null;

            button.Clicked -= OnDeviceButtonClicked_Disconnect;
            button.Clicked += OnDeviceButtonClicked_Connect;
        }

        private async void OnScannerClicked_Scan(object? sender, EventArgs e)
        {
            if (!await _bleWrapper.CheckCaps())
            {
                //TODO: Alert user of missing permissions.
                return;
            }

            if (_bleWrapper.Adapter.IsScanning)
            {
                await _bleWrapper.Adapter.StopScanningForDevicesAsync();
                ScanButton.Text = "Scan for devices";
                return;
            }

            DeviceList.Clear();
            _ = _bleWrapper.Adapter.StartScanningForDevicesAsync();
            ScanButton.Text = "Scanning...";
        }

        private async void OnScannerClicked_Disconnect(object? sender, EventArgs e)
        {
            if (_device is null)
                return;

            await _bleWrapper.Adapter.DisconnectDeviceAsync(_device);
            _device = null;

            //ScanButton.Text = "Scan for devices"; //Text set by OnDeviceDisconnected.
            ScanButton.Clicked -= OnScannerClicked_Disconnect;
            ScanButton.Clicked += OnScannerClicked_Scan;

            for (int i = 0; i < DeviceList.Children.Count; i++)
                if (DeviceList.Children[i] is Button b)
                    b.IsEnabled = true;
        }
    }
}
