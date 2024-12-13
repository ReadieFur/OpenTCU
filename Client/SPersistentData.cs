using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ReadieFur.OpenTCU.Client
{
    internal struct SPersistentData
    {
        public String DeviceName;
        public String BikeSerialNumber;
        public UInt16 BaseWheelCircumference;
        public UInt16 TargetWheelCircumference;
        public UInt32 Pin;
    }
}
