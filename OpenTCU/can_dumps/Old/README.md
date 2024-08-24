In my tests the SPI is reading from the bike and TWAI is reading from the controller.
The data is formatted like so: timestamp, isSPI, id, isExtended, isRemote, length, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]
The data is in hex format except for the timestamp, the is* and length values which are in decimal/boolean.

It is important that you pay attention to the length value as the data outputs previous logs to fill the space  
e.g.
A data length of 8  
113966,0,300,0,0,8,3,5a,64,5a,64,0,64,ad

and a data length of 3, you can see data items 4 5 6 7 and 8 are the same as the previous message and these should be ignored
113967,0,301,0,0,3,4e,d,0,5a,64,0,64,ad
