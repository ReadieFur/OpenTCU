In my tests the SPI is reading from the bike and TWAI is reading from the controller.
The data is formatted like so: timestamp, isSPI, id, isExtended, isRemote, length, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]
The data is in hex format except for the timestamp, is* and length which are in decimal/boolean.
