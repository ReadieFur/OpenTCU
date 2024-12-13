using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Plugin.BLE;
using Plugin.BLE.Abstractions.Contracts;
using System.Diagnostics;

namespace ReadieFur.OpenTCU.Client
{
    internal class OpenTCUDevice
    {
        static readonly Guid MainServiceGuid = new Guid("0000aa6c-0000-1000-8000-00805f9b34fb");
        static readonly Guid RuntimeStatsGuid = new Guid("ad09c337-0000-1000-8000-00805f9b34fb");
        static readonly Guid PersistentDataGuid = new Guid("3a3d3a3d-0000-1000-8000-00805f9b34fb");
        static readonly Guid APEnabledGuid = new Guid("b45d9ced-0000-1000-8000-00805f9b34fb");

        static readonly Guid DebugServiceGuid = new Guid("0000911d-0000-1000-8000-00805f9b34fb");
        static readonly Guid InjectCANGuid = new Guid("78fdc1ce-0000-1000-8000-00805f9b34fb");
        static readonly Guid LogWhitelistGuid = new Guid("1450d8e6-0000-1000-8000-00805f9b34fb");
        static readonly Guid RebootGuid = new Guid("bfb5e32f-0000-1000-8000-00805f9b34fb");
        static readonly Guid ToggleRuntimeStatsGuid = new Guid("9ed8266d-0000-1000-8000-00805f9b34fb");

        public IDevice Device { get; private init; }
        private IReadOnlyDictionary<Guid, IService> _services { get; init; }
        private IReadOnlyDictionary<Guid, ICharacteristic> _characteristics { get; init; }

        private OpenTCUDevice() {}

        public static async Task<OpenTCUDevice> Initialize(IDevice device)
        {
            if (device is null)
                throw new ArgumentNullException(nameof(device));

            Dictionary <Guid, IService> services = new();
            Dictionary<Guid, ICharacteristic> characteristics = new();

            if (await device.GetServiceAsync(MainServiceGuid) is not IService mainService)
                throw new InvalidOperationException("Main service not found.");
            if (await mainService.GetCharacteristicAsync(RuntimeStatsGuid) is not ICharacteristic runtimeStatsCharacteristic)
                throw new InvalidOperationException("Runtime stats characteristic not found.");
            if (await mainService.GetCharacteristicAsync(PersistentDataGuid) is not ICharacteristic persistentDataCharacteristic)
                throw new InvalidOperationException("Persistent data characteristic not found.");
            if (await mainService.GetCharacteristicAsync(APEnabledGuid) is not ICharacteristic apEnabledCharacteristic)
                throw new InvalidOperationException("AP enabled characteristic not found.");
            services.Add(MainServiceGuid, mainService);
            characteristics.Add(RuntimeStatsGuid, runtimeStatsCharacteristic);
            characteristics.Add(PersistentDataGuid, persistentDataCharacteristic);
            characteristics.Add(APEnabledGuid, apEnabledCharacteristic);

            IService? debugService = await device.GetServiceAsync(DebugServiceGuid);
            if (debugService is not null)
            {
                ICharacteristic? injectCANCharacteristic = await debugService.GetCharacteristicAsync(InjectCANGuid);
                ICharacteristic? logWhitelistCharacteristic = await debugService.GetCharacteristicAsync(LogWhitelistGuid);
                ICharacteristic? rebootCharacteristic = await debugService.GetCharacteristicAsync(RebootGuid);
                ICharacteristic? toggleRuntimeStatsCharacteristic = await debugService.GetCharacteristicAsync(ToggleRuntimeStatsGuid);

                services.Add(DebugServiceGuid, debugService);
                characteristics.Add(InjectCANGuid, injectCANCharacteristic);
                characteristics.Add(LogWhitelistGuid, logWhitelistCharacteristic);
                characteristics.Add(RebootGuid, rebootCharacteristic);
                characteristics.Add(ToggleRuntimeStatsGuid, toggleRuntimeStatsCharacteristic);
            }

            OpenTCUDevice openTCUDevice = new()
            {
                Device = device,
                _services = services,
                _characteristics = characteristics
            };

            return openTCUDevice;
        }

        public async Task<SPersistentData> GetPersistentData()
        {
            var data = await _characteristics[PersistentDataGuid].ReadAsync();
            Debug.WriteLine($"Get persistent data ({data.data.Length}): {BitConverter.ToString(data.data)}");

            SPersistentData persistentData = new();
            //persistentData.DeviceName
            //persistentData.BikeSerialNumber
            persistentData.BaseWheelCircumference = BitConverter.ToUInt16(data.data, 0);
            persistentData.TargetWheelCircumference = BitConverter.ToUInt16(data.data, 2);
            persistentData.Pin = BitConverter.ToUInt32(data.data, 4);

            return persistentData;
        }

        public async Task<bool> SetPersistentData(SPersistentData persistentData)
        {
            byte[] data = new byte[8];
            BitConverter.GetBytes(persistentData.BaseWheelCircumference).CopyTo(data, 0);
            BitConverter.GetBytes(persistentData.TargetWheelCircumference).CopyTo(data, 2);
            BitConverter.GetBytes(persistentData.Pin).CopyTo(data, 4);

            int res = await _characteristics[PersistentDataGuid].WriteAsync(data);
            Debug.WriteLineIf(res != 0, $"Failed to write persistent data: {res}");
            Debug.WriteLineIf(res == 0, "Written persistent data.");
            return res == 0;
        }

        public async Task<SRuntimeStats> GetRuntimeStats()
        {
            var data = await _characteristics[RuntimeStatsGuid].ReadAsync();
            //Debug.WriteLine($"Get runtime stats ({data.data.Length}): {BitConverter.ToString(data.data)}");

            SRuntimeStats runtimeStats = new();
            runtimeStats.BikeSpeed = BitConverter.ToUInt16(data.data, 0);
            runtimeStats.RealSpeed = BitConverter.ToUInt16(data.data, 2);
            runtimeStats.Cadance = BitConverter.ToUInt16(data.data, 4);
            runtimeStats.RiderPower = BitConverter.ToUInt16(data.data, 6);
            runtimeStats.MotorPower = BitConverter.ToUInt16(data.data, 8);
            runtimeStats.BatteryVoltage = BitConverter.ToUInt16(data.data, 10);
            runtimeStats.BatteryCurrent = BitConverter.ToUInt32(data.data, 12);
            runtimeStats.EaseSetting = data.data[16];
            runtimeStats.PowerSetting = data.data[17];
            runtimeStats.WalkMode = data.data[18] != 0;

            return runtimeStats;
        }

        public static bool operator ==(OpenTCUDevice a, OpenTCUDevice b)
        {
            if (a is null && b is null)
                return true;
            if (a is null || b is null)
                return false;
            return a.Device == b.Device;
        }

        public static bool operator !=(OpenTCUDevice a, OpenTCUDevice b)
        {
            return !(a == b);
        }

        public override bool Equals(object? obj)
        {
            if (obj is OpenTCUDevice device)
                return this == device;
            return false;
        }

        public override int GetHashCode()
        {
            return Device.GetHashCode();
        }
    }
}
