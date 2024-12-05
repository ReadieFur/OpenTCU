This document contains the currently discovered data codes that are sent over the CAN bus.

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

<a name="sn_request"></a>
- `03`, `22`, `02`, `*A`, `00`, `00`, `00`, `00`  
Gets a serial number for a given device.  
`*A` is the device ID.  
See [serial number response](#sn_response) for the response.

<a name="cont_100"></a>
- `30`, `00`, `00`  
Continuation of a request for serial number data.

### 101
- **Origin:** Other
- **Length:** 8
- **Use:** Provide configuration data.

#### Known messages

<a name="sn_response"></a>
- `10`, `*A`, `62`, `02`, `*B`, `**`, `**`, `**`  
Response to a [request for serial number data](#sn_request).  
`*A` unknown.  
`*B` indicates the device that the serial number is associated with.  
`**` is the serial number data.  
Requires a [continuation message](#cont_100) to get the full serial number, the remaining data is sent in the [continuation response](#sn_response_cont).

<a href="sn_response_cont"></a>
- `21`, `**`, `**`, `**`, `**`, `**`, `**`, `**` | `22`, `**`, `**`, `**`, `**`, `**`, `**`, `**` | `23`, `**`, `**`, `**`, `*A`, `*A`, `*A`, `*A`  
Continuation data for a request for serial number data.  
D0 is always `21`, `22` or `23`, possibly indicating the order of the data.  
`**` is the serial number data, this is null terminated when the string is complete.  
`*A` seems to be a set of bits that appears to indicate the end of the data frames.  
