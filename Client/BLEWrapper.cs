using Plugin.BLE;
using Plugin.BLE.Abstractions.Contracts;
using System.Diagnostics;

namespace ReadieFur.OpenTCU.Client
{
    internal class BLEWrapper
    {
        private readonly IBluetoothLE _ble = CrossBluetoothLE.Current;
        public readonly IAdapter Adapter = CrossBluetoothLE.Current.Adapter;

        public BLEWrapper()
        {
#if DEBUG
            Adapter.DeviceDiscovered += (s, a) =>
            {
                if (a.Device.Name != null && a.Device.Name.StartsWith("OpenTCU"))
                {
                    Debug.WriteLine($"Found device: {a.Device.Name}");
                }
            };
#endif
        }

        public async Task<bool> CheckCaps()
        {
            //Check if BLE is available.
            if (!_ble.IsAvailable)
            {
                Debug.WriteLine("BLE is not available on this device.");
                return false;
            }

            //Check if Bluetooth is turned on.
            if (!_ble.IsOn)
            {
                Debug.WriteLine("Bluetooth is off. Please enable it.");
                return false;
            }

            //Check and request permissions.
            bool permissionsGranted = await CheckAndRequestPermissionsAsync();
            if (!permissionsGranted)
            {
                Debug.WriteLine("Required permissions are not granted.");
                return false;
            }

            return true;
        }

        public async Task GetServices(IDevice device)
        {
            try
            {
                await Adapter.ConnectToDeviceAsync(device);
                Debug.WriteLine("Connected to device.");

                var services = await device.GetServicesAsync();
                foreach (var service in services)
                {
                    Debug.WriteLine($"Service: {service.Id}");
                    var characteristics = await service.GetCharacteristicsAsync();
                    foreach (var characteristic in characteristics)
                    {
                        Debug.WriteLine($"Characteristic: {characteristic.Id}");
                    }
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Error getting services: {ex.Message}");
            }
        }

        private async Task<bool> CheckAndRequestPermissionsAsync()
        {
            try
            {
                //Android.
                if (DeviceInfo.Platform == DevicePlatform.Android)
                {
                    //Only required on Android 12 and above.
                    if (DeviceInfo.Version.Major >= 12)
                    {
                        if (await Permissions.RequestAsync<Permissions.Bluetooth>() != PermissionStatus.Granted)
                        {
                            Debug.WriteLine("Bluetooth permission was denied.");
                            return false;
                        }
                    }
                }

                //Android & iOS.
                if (DeviceInfo.Platform == DevicePlatform.iOS)
                {
                    if (await Permissions.RequestAsync<Permissions.LocationWhenInUse>() != PermissionStatus.Granted)
                    {
                        Debug.WriteLine("Location permission was denied.");
                        return false;
                    }
                }

                return true;
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Error checking permissions: {ex.Message}");
                return false;
            }
        }
    }
}
