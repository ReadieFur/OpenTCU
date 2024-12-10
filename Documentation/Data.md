This document contains the currently discovered data codes that are sent over the CAN bus.  
All data codes are in hexadecimal format.

## Data Codes
### 100
- **Origin:** TCU
- **Length:** 3 or 8
- **Use:** Send and request configuration data.

#### Known messages
- `05`, `2E`, `02`, `06`, `**`, `**`, `00`, `00`  
  Sets the wheel circumference in millimeters  
  `**` is the value to be set, in little-endian format.  
  Example: 2160mm (default) -> `0870` -> `70`, `08`  
  Requires a bike system restart to take effect.

<a name="str_request"></a>
- `03`, `22`, `02`, `*A`, `00`, `00`, `00`, `00`  
  Gets a string.  
  `*A` is the string ID. Below is a table of known string codes.
  | ID   | Key |
  |------|--------|
  | `02` | Motor serial number |
  | `03` | Motor hardware ID |
  | `04` | Bike serial number |
  
  See [string response](#str_response) for the response.

<a name="cont_100"></a>
- `30`, `00`, `00`  
Continuation of a request for string data.

<a name="get_wheel_circumference_request"></a>
- `03`, `22`, `02`, `06`, `00`, `00`, `00`, `00`
  Gets the wheel circumference in millimeters.
  See [get wheel circumference response](#get_wheel_circumference_response) for the response.

### 101
- **Origin:** Other
- **Length:** 8
- **Use:** Provide configuration data.

#### Known messages
<a name="str_response"></a>
- `10`, `*A`, `62`, `02`, `*B`, `**`, `**`, `**`  
  Response to a [request for a string](#str_request).  
  `*A` unknown.  
  `*B` the ID that the string is associated with.  
  `**` is the string data.  
  Requires a [continuation message](#cont_100) to get the full string, the remaining data is sent in the [continuation response](#str_response_cont).

<a href="str_response_cont"></a>
- `21`, `**`, `**`, `**`, `**`, `**`, `**`, `**` | `22`, `**`, `**`, `**`, `**`, `**`, `**`, `**` | `23`, `**`, `**`, `**`, `*A`, `*A`, `*A`, `*A`  
  Continuation data for a request for a string.  
  D0 is always `21`, `22` or `23`, possibly indicating the order of the data.  
  `**` the string characters, this is null terminated when the string is complete, unless the string fills all bits.  
  `*A` seems to be a set of bits that appears to indicate the end of the data frames.  

<a name="get_wheel_circumference_response"></a>
- `05`, `62`, `02`, `06`, `**`, `**`, `E0`, `AA`
  Response to a [request for the wheel circumference](#get_wheel_circumference_request).  
  `**` is the wheel circumference in millimeters (little-endian).  
  Example: `70`, `08` -> `0870` -> 2160mm.

<!-- 03,6E,02,06,E0,B3,F8,02 | Confirmation response to set wheel circumference message. -->

### 201
- **Origin:** Motor
- **Length:** 5
- **Use:** Live information about the drive system.

D1 and D2 combined contain the speed of the bike in km/h * 100 (little-endian).  
Example: `EA`, `01` -> `01EA` -> 490 -> 4.9km/h.

D6 possibly contains the motion state of the bike. Recordings show that it is `62` when the bike is stationary and `63` when the bike is moving.

### 300
- **Origin:** TCU
- **Length:** 8
- **Use:** Send runtime commands and configuration.

D1 possibly the motor power mode. `00` off, `01` low, `02` medium, `03` high.
D2 controls the walk mode. `5A` disables walk mode, `A5` enables walk mode.
D5 ease setting, `00` to `64`.
D7 maximum motor power setting, `00` to `64`.
<!-- D8 possibly system clock. -->

### 401
- **Origin:** Battery
- **Length:** 8
- **Use:** Battery power information.

D1 and D2 combined contain the battery voltage in mV (little-endian).  
Example: `35`, `9F` -> `9F35` -> 40757mV -> 40.757V.

D5, D6, D7 and D8 combined contain the battery current in mA (little-endian with two's complement).  
Example 1: `46`, `00`, `00`, `00` -> `00 000046` -> 70mA -> 0.07A.  
Example 2: `5B`, `F0`, `FF`, `FF` -> `FF FFF05B` -> -4005mA -> -4.005A.  
The value is positive when discharging and negative when charging.

### 404
- **Origin:** TCU

#### Known messages
- `02`, `30`, `50`, `00`, `01`  
  Enable battery charge limit.

- `02`, `30`, `00`, `00`, `00`
  Disable battery charge limit.

<!-- TODO:
TCU uses 402 for charge% and battery health.
203 contains data for cadence and motor power.
Rider power seems to need both 201 and 203.
-->