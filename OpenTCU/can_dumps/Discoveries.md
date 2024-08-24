## 4**
Battery information  
Data in little endian  
All values to be divided by 1000 after conversion

### 400
Unclear

### 401
Bits 1-2 bits are for voltage  
e.g. `BA 9E` -> `9E BA` -> `39614` /1000 = `39.614V`

Bits 3 and 4 always 0

Bits 5-8 are current draw in twos compliment
e.g. `4D 00 00 00` -> `00 00 00 4D` -> `77`...  
Result is positive and less than the 32 bit maximum value (2^31 - 1) so skip twos compliment step  
/1000 = `0.077`

e.g2. `5B F0 FF FF` -> `FF FF F0 5B` -> `4294963291`  
Value is larger than 32 bit maximum (would overflow to negative in code) so perform twos compliment step.  
Subtract 2^32 from the value -> `-4005` -> /1000 = `-4.005`  

This value appears to be negative when the battery is charging.

### 402
Bit 1 is percentage remaining (not little endian or /1000)
e.g. `4C` = `76%`

Bits 2-4 are always 0.  

Bits 5-8 are remaining capacity in watt hours.  
e.g. `2C 7B 07 00` -> `00 07 7B 2C` -> `490284` -> /1000 = `490.284Wh`

According to the reference document I am looking at, the 402 messages supply the state of charge relative to a maximum capacity of 504Wh. Hence the above value is above the batteries maximum capacity. The reference document discovered that the 403 messages state the real capacity of the battery. Therefore in order to calculate the real battery capacity remaining you must divide the value above by (504/<capacity 403 message>) 

### 403
According to the reference document, this ID should supply the real capacity of the battery.  
However my recordings do not match up with the reference document therefore the above capacity calculation cannot be relied on.  
Below I will record the reference documents findings anyway for future reference.  

Bit 1 always constant, unclear of purpose.

Bits 2-4 always 0.

Bits 5-8 maximum capacity?  
e.g. (according to my recordings, which disproves the above findings)  
`94 E0 09 00` -> `00 09 E0 94` -> `647316` -> /1000 = `647.316Wh`?

e.g. (according to the reference document)  
`D0 DD 06 00` -> `00 06 DD D0` -> `450000` -> /1000 = `450Wh`?
