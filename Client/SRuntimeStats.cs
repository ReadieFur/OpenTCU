using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ReadieFur.OpenTCU.Client
{
    internal struct SRuntimeStats
    {
        public UInt16 BikeSpeed;
        public UInt16 RealSpeed;
        public UInt16 Cadance;
        public UInt16 RiderPower;
        public UInt16 MotorPower;
        public UInt16 BatteryVoltage;
        public UInt32 BatteryCurrent;
        public Byte EaseSetting;
        public Byte PowerSetting;
        public Boolean WalkMode;
    }
}
