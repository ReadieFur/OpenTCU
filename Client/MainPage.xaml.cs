using Plugin.BLE;
using Plugin.BLE.Abstractions.Contracts;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.CompilerServices;

namespace ReadieFur.OpenTCU.Client
{
    public partial class MainPage : ContentPage
    {
        private MainPageViewModel _viewModel => (MainPageViewModel)BindingContext;
        private BLEWrapper _bleWrapper = new();
        private OpenTCUDevice? _device = null;
        private CancellationTokenSource? _runtimeStatsCancellationTokenSource = null;
        private Task? _runtimeStatsTask = null;

        public MainPage()
        {
            InitializeComponent();

            DeviceElementsEnabled(false);

            _bleWrapper.Adapter.ScanTimeoutElapsed += Adapter_ScanTimeoutElapsed;
            _bleWrapper.Adapter.DeviceDiscovered += OnBleDeviceDiscovered;
            _bleWrapper.Adapter.DeviceDisconnected += OnDeviceDisconnected;

            ScanButton.Clicked += OnScannerClicked_Scan;
        }

        private async void ContentPage_Loaded(object sender, EventArgs e)
        {
            Debug.WriteLine("MainPage loaded.");

            //Check if we are already connected to a valid device.
            if (_device is null)
                return;

            IDevice? device = _bleWrapper.Adapter.ConnectedDevices.FirstOrDefault(d => d.Name == null || d.Name.StartsWith("OpenTCU"));
            if (device is null)
                return;

            try
            {
                _device = await OpenTCUDevice.Initialize(device);

                SPersistentData persistentData = await _device.GetPersistentData();
                _viewModel.RealCircumference = persistentData.BaseWheelCircumference;
                _viewModel.EmulatedCircumference = persistentData.TargetWheelCircumference;
                _viewModel.Pin = persistentData.Pin;

                _runtimeStatsCancellationTokenSource = new();
                _runtimeStatsTask = new Task(RuntimeStatsTask);
                _runtimeStatsTask.Start();
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Failed to initialize OpenTCUDevice: {ex.Message}");
                return;
            }

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

            

            DeviceElementsEnabled(true);
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
            if (e.Device != _device?.Device)
                return;

            _runtimeStatsCancellationTokenSource?.Cancel();
            if (_runtimeStatsTask is not null)
                await _runtimeStatsTask;
            _runtimeStatsCancellationTokenSource = null;
            _runtimeStatsTask = null;
            _device = null;

            await Dispatcher.DispatchAsync(() =>
            {
                ScanButton.Text = "Scan for devices";
                ScanButton.IsEnabled = true;
                DeviceElementsEnabled(false);
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
            if (sender is not Button button || button.BindingContext is not IDevice device || _device?.Device == device)
                return;

            ScanButton.IsEnabled = false;
            ScanButton.Text = "Connecting...";

            if (_bleWrapper.Adapter.IsScanning)
                await _bleWrapper.Adapter.StopScanningForDevicesAsync();

            if (_device is not null)
                await _bleWrapper.Adapter.DisconnectDeviceAsync(_device.Device);

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
            try
            {
                _device = await OpenTCUDevice.Initialize(device);
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Failed to initialize OpenTCUDevice: {ex.Message}");
                ScanButton.IsEnabled = true;
                ScanButton.Text = "Scan for devices";
                for (int i = 0; i < DeviceList.Children.Count; i++)
                    if (DeviceList.Children[i] is Button b)
                        b.IsEnabled = true;
                return;
            }

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

            SPersistentData persistentData = await _device.GetPersistentData();
            _viewModel.RealCircumference = persistentData.BaseWheelCircumference;
            _viewModel.EmulatedCircumference = persistentData.TargetWheelCircumference;
            _viewModel.Pin = persistentData.Pin;

            _runtimeStatsCancellationTokenSource = new();
            _runtimeStatsTask = new Task(RuntimeStatsTask);
            _runtimeStatsTask.Start();

            DeviceElementsEnabled(true);
        }

        private async void RuntimeStatsTask()
        {
            while (!_runtimeStatsCancellationTokenSource?.Token.IsCancellationRequested ?? false && _device is not null)
            {
                try
                {
                    SRuntimeStats runtimeStats = await _device!.GetRuntimeStats();
                    //Debug.WriteLine($"BikeSpeed: {runtimeStats.BikeSpeed}, RealSpeed: {runtimeStats.RealSpeed}, Cadance: {runtimeStats.Cadance}, RiderPower: {runtimeStats.RiderPower}, MotorPower: {runtimeStats.MotorPower}, BatteryVoltage: {runtimeStats.BatteryVoltage}, BatteryCurrent: {runtimeStats.BatteryCurrent}, EaseSetting: {runtimeStats.EaseSetting}, PowerSetting: {runtimeStats.PowerSetting}, WalkMode: {runtimeStats.WalkMode}");
                    _viewModel.BikeSpeed = runtimeStats.BikeSpeed;
                    _viewModel.RealSpeed = runtimeStats.RealSpeed;
                    _viewModel.Cadance = runtimeStats.Cadance;
                    _viewModel.RiderPower = runtimeStats.RiderPower;
                    _viewModel.MotorPower = runtimeStats.MotorPower;
                    _viewModel.BatteryVoltage = runtimeStats.BatteryVoltage;
                    _viewModel.BatteryCurrent = runtimeStats.BatteryCurrent;
                    _viewModel.EaseSetting = runtimeStats.EaseSetting;
                    _viewModel.PowerSetting = runtimeStats.PowerSetting;
                    _viewModel.WalkMode = runtimeStats.WalkMode;
                }
                catch (Exception ex)
                {
                    Debug.WriteLine($"Failed to get runtime stats: {ex.Message}");
                }

                await Task.Delay(1000);
            }
        }

        private async void OnDeviceButtonClicked_Disconnect(object? sender, EventArgs e)
        {
            if (sender is not Button button || button.BindingContext is not IDevice device || _device is null || device != _device.Device)
                return;

            await _bleWrapper.Adapter.DisconnectDeviceAsync(_device.Device);

            _runtimeStatsCancellationTokenSource?.Cancel();
            if (_runtimeStatsTask is not null)
                await _runtimeStatsTask;
            _runtimeStatsCancellationTokenSource = null;
            _runtimeStatsTask = null;
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

            await _bleWrapper.Adapter.DisconnectDeviceAsync(_device.Device);

            _runtimeStatsCancellationTokenSource?.Cancel();
            if (_runtimeStatsTask is not null)
                await _runtimeStatsTask;
            _runtimeStatsCancellationTokenSource = null;
            _runtimeStatsTask = null;
            _device = null;

            //ScanButton.Text = "Scan for devices"; //Text set by OnDeviceDisconnected.
            ScanButton.Clicked -= OnScannerClicked_Disconnect;
            ScanButton.Clicked += OnScannerClicked_Scan;

            for (int i = 0; i < DeviceList.Children.Count; i++)
                if (DeviceList.Children[i] is Button b)
                    b.IsEnabled = true;
        }

        private void DeviceElementsEnabled(bool isEnabled)
        {
            //Iterate over all children in DeviceOptionsPanel and set the IsEnabled property.
            for (int i = 0; i < DeviceOptionsPanel.Children.Count; i++)
                if (DeviceOptionsPanel.Children[i] is View view)
                    view.IsEnabled = isEnabled;
        }

        private async void ApplyOptionsButton_Clicked(object sender, EventArgs e)
        {
            if (_device is null)
                return;

            ApplyOptionsButton.IsEnabled = false;

            if (_viewModel.RealCircumference > 2400)
                _viewModel.RealCircumference = 2400;
            else if (_viewModel.RealCircumference < 2000)
                _viewModel.RealCircumference = 2000;

            if (_viewModel.EmulatedCircumference > 2400)
                _viewModel.EmulatedCircumference = 2400;
            else if (_viewModel.EmulatedCircumference < 800)
                _viewModel.EmulatedCircumference = 800;

            if (_viewModel.Pin < uint.MinValue)
                _viewModel.Pin = uint.MinValue;
            else if (_viewModel.Pin > uint.MaxValue)
                _viewModel.Pin = uint.MaxValue;

            SPersistentData persistentData = new()
            {
                BaseWheelCircumference = (UInt16)_viewModel.RealCircumference,
                TargetWheelCircumference = (UInt16)_viewModel.EmulatedCircumference,
                Pin = (UInt32)_viewModel.Pin
            };
            await _device.SetPersistentData(persistentData); //TODO: Alert user of success/failure.

            //Get the settings from the device to re-validate the entries.
            persistentData = await _device.GetPersistentData();
            _viewModel.RealCircumference = persistentData.BaseWheelCircumference;
            _viewModel.EmulatedCircumference = persistentData.TargetWheelCircumference;
            _viewModel.Pin = persistentData.Pin;

            ApplyOptionsButton.IsEnabled = true;
        }
    }
}
