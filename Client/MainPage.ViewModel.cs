using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;

namespace ReadieFur.OpenTCU.Client
{
    public class MainPageViewModel : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler? PropertyChanged;

        private int _realCircumference = 2160;
        public int RealCircumference
        {
            get => _realCircumference;
            set
            {
                if (_realCircumference != value)
                {
                    //Don't check value ranges here as it can cause issues with text input, only validate it externally upon final submission.
                    /*if (value > 2400)
                        _baseCircumference = 2400;
                    else if (value < 2000)
                        _baseCircumference = 2000;
                    else*/
                    _realCircumference = value;
                    OnPropertyChanged();
                    OnPropertyChanged(nameof(CircumferenceMultiplier));
                    OnPropertyChanged(nameof(MaxMotorSpeed));
                }
            }
        }

        private int _emulatedCircumference = 2160;
        public int EmulatedCircumference
        {
            get => _emulatedCircumference;
            set
            {
                if (_emulatedCircumference != value)
                {
                    //Don't check value ranges here as it can cause issues with text input, only validate it externally upon final submission.
                    /*if (value > 2400)
                        _emulatedCircumference = 2400;
                    else if (value < 800)
                        _emulatedCircumference = 800;
                    else*/
                    _emulatedCircumference = value;
                    OnPropertyChanged();
                    OnPropertyChanged(nameof(CircumferenceMultiplier));
                    OnPropertyChanged(nameof(MaxMotorSpeed));
                }
            }
        }

        private uint _pin = 123456;
        public uint Pin
        {
            get => _pin;
            set
            {
                if (_pin != value)
                {
                    //Don't check value ranges here as it can cause issues with text input, only validate it externally upon final submission.
                    /*if (value > 999999)
                        _pin = 999999;
                    else if (value < 100000)
                        _pin = 100000;
                    else*/
                    _pin = value;
                    OnPropertyChanged();
                }
            }
        }

        public string CircumferenceMultiplier => $"{(double)RealCircumference / EmulatedCircumference:F2}x";

        public string MaxMotorSpeed => $"{25 * ((double)RealCircumference / EmulatedCircumference):F2}kph";

        private double _bikeSpeed = 0;
        public double BikeSpeed
        {
            get => _bikeSpeed;
            set
            {
                if (_bikeSpeed != value)
                {
                    _bikeSpeed = value;
                    OnPropertyChanged();
                    OnPropertyChanged(nameof(BikeSpeedStr));
                }
            }
        }
        public string BikeSpeedStr => $"{BikeSpeed / 100:F2}kph";

        private double _realSpeed = 0;
        public double RealSpeed
        {
            get => _realSpeed;
            set
            {
                if (_realSpeed != value)
                {
                    _realSpeed = value;
                    OnPropertyChanged();
                    OnPropertyChanged(nameof(RealSpeedStr));
                }
            }
        }
        public string RealSpeedStr => $"{RealSpeed / 100:F2}kph";

        private double _cadance = 0;
        public double Cadance
        {
            get => _cadance;
            set
            {
                if (_cadance != value)
                {
                    _cadance = value;
                    OnPropertyChanged();
                }
            }
        }


        private double _riderPower = 0;
        public double RiderPower
        {
            get => _riderPower;
            set
            {
                if (_riderPower != value)
                {
                    _riderPower = value;
                    OnPropertyChanged();
                    OnPropertyChanged(nameof(RiderPowerStr));
                }
            }
        }
        public string RiderPowerStr => $"{RiderPower:F2}W";

        private double _motorPower = 0;
        public double MotorPower
        {
            get => _motorPower;
            set
            {
                if (_motorPower != value)
                {
                    _motorPower = value;
                    OnPropertyChanged();
                    OnPropertyChanged(nameof(MotorPowerStr));
                }
            }
        }
        public string MotorPowerStr => $"{MotorPower:F2}W";

        private double _batteryVoltage = 0;
        public double BatteryVoltage
        {
            get => _batteryVoltage;
            set
            {
                if (_batteryVoltage != value)
                {
                    _batteryVoltage = value;
                    OnPropertyChanged();
                    OnPropertyChanged(nameof(BatteryVoltageStr));
                }
            }
        }
        public string BatteryVoltageStr => $"{BatteryVoltage / 1000:F2}V";

        private double _batteryCurrent = 0;
        public double BatteryCurrent
        {
            get => _batteryCurrent;
            set
            {
                if (_batteryCurrent != value)
                {
                    _batteryCurrent = value;
                    OnPropertyChanged();
                    OnPropertyChanged(nameof(BatteryCurrentStr));
                }
            }
        }
        public string BatteryCurrentStr => $"{BatteryCurrent / 1000:F2}A";

        private double _easeSetting = 0;
        public double EaseSetting
        {
            get => _easeSetting;
            set
            {
                if (_easeSetting != value)
                {
                    _easeSetting = value;
                    OnPropertyChanged();
                }
            }
        }

        private double _powerSetting = 0;
        public double PowerSetting
        {
            get => _powerSetting;
            set
            {
                if (_powerSetting != value)
                {
                    _powerSetting = value;
                    OnPropertyChanged();
                }
            }
        }

        private bool _walkMode = false;
        public bool WalkMode
        {
            get => _walkMode;
            set
            {
                if (_walkMode != value)
                {
                    _walkMode = value;
                    OnPropertyChanged();
                }
            }
        }

        protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null) =>
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}
