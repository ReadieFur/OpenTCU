<!-- Discuss the modular design of the project, realtime considerations and data optimizations. -->

# OpenTCU CAN Bus Software
This document contains a quick insight to the software decisions and implementations made for the OpenTCU project.  
*This document is not complete yet and has more to be added.*

- [OpenTCU CAN Bus Software](#opentcu-can-bus-software)
  - [Considerations](#considerations)
  - [Modules](#modules)
    - [Service Manager](#service-manager)
    - [CAN Bus](#can-bus)
    - [Networking](#networking)

## Considerations
Due to where this device will be operating, it is extremely important that it is able to perform operations on data in realtime because it will be managing the bike's system which the user will not want to have any delays on their input to.

The software portion of this project has been designed to be as modular as possible. This is to allow for easy expansion of the software to include new features and to allow for easy debugging of the software.  

For lower level access and more control over the flash space used I have opted to use the esp-idf framework instead of the Arduino framework.  
Doing this meant that I can have two boot partitions which allows for OTA updates which is an ideal features considering that this device will be embedded inside a bike where access to the device is limited.  

## Modules
The majority of the core code behind this project has its work committed to the `lib/esp32-libs` submodule. This submodule contains a lot of the core code that I expect to reuse on the ESP32 for other projects such as the service manager, logger and networking functionality.  
The code that is specific to this project is contained within the `src` directory.

### Service Manager
The service manager allows for the creation of "services" that can be run in the background of the main program. This has been designed to make use of the FreeRTOS task scheduler to set priorities, stack sizes for each service individually and allowing them to seemingly run in parallel.  

Each service class should inherit from the `AService` abstract class. The services must override the `RunServiceImpl` which is where the main logic of the service should be placed, if this method exits prematurely the service manager will see this as an error and depending on how FreeRTOS is configured, it will either throw an error or restart the ESP32.  
This abstract class has a few helper methods to wrap the creation and management of the service, such as setting up the FreeRTOS task, creating the semaphores used for signaling the status of the service, stopping the service etc.  
Services can also have dependencies on other services, this is done by calling the templated helper method `AddDependency<>` in the constructor of the service. During runtime an instance of this dependency can be retrieved by calling the `GetService<>` method.

The `ServiceManager` class is a static class that manages all installed services states and dependencies. This class makes heavy use of templating in order to make the code easier to use when installing, starting, stopping and querying services.  All services that are to be run must be installed to the service manager by calling the `InstallService<>` method.  
Services can be installed in any order however they cannot be started until all of a given service's dependencies have been started, this is checked during the call to `StartService<>`, if a service's dependencies are not met either by not having all of them installed or started, a value of `EServiceResult` will be returned indicating the error. Likewise a service that is depended upon cannot be stopped until the service depending on it has been stopped.

By having a service manager it means the program can be created in a much more modular way, which significantly increases code maintainability and readability.

### CAN Bus
The CAN folder contains all of the CAN bus related operations for this project. A core component to this is the `BusMaster` class, the `BusMaster` class is a service that initializes the CAN transceivers, intercepts messages, interprets them and relays them to the other bus.  

Due to the criticality of this service, it runs with the highest priority of any service in the system in order to ensure that data is transmitted across the bus without delay or having to drop any frames, if this were to happen the rider of the bike would be able to notice as there would be delays in their inputs and the bike's response, possibly even causing the bike to turn off if the motor does not receive the clock from the TCU frequently enough. If the bus is to run too slow, fail to initialize or have a critical runtime error, the ESP32 will restart in order to attempt to recover from the error. Through my testing I have found that with my current OpenTCU initialization, the ESP32 can boot fast enough during operation of the bike and not cause the bike to turn off.

During initialization of this service, depending on the hardware configuration, two of either the `McpCan` and/or `TwaiCan` classes are created, these two instances both inherit from the `ACan` abstract class. The classes that inherit from `ACan` handle the low level operations of the CAN bus for specific hardware. A predefined timeout on the read/write operations has been set based on the known bus speed of 250kbps, this is to ensure that the program does not hang if an operation takes too long.

Each of the CAN classes that are created on their own FreeRTOS task, this allows for seemingly parallel operation of the two buses. This task follows a simple order of operations being; read from the bus, interpret the message, log data, relay the message to the other bus.  

During the data interception stage we read the raw bytes from each message and check them against the known message IDs discovered in the [Reversed Data](./BusDecoding.md#reversed-data) section of the [Bus Decoding](./BusDecoding.md) document. If the message ID is known then the value is recorded and if desired, modified, allowing us to perform the man in the middle (MITM) attack on the data. One example of the MITM attack being performed is during the transmission of the bikes speed. If the user changes the virtual wheel circumference, a multiplier is created from the virtual wheel size and the real wheel size, this multiplier is then applied to the speed value before it is sent from the motor to the TCU, which in turn results in the TCU displaying the correct, real speed, to the user, instead of the incorrect speed the motor thinks the bike is going.

### Networking
The networking folder currently only contains the code for the OpenTCU Bluetooth API, however in the future I would like to integrate bluetooth communication with the TCU so everything can be managed from my client app.  

Because I am using esp-idf, getting Bluetooth Low Energy (BLE) working is rather tedious and requires a lot of work, for this reason I have placed the core of the BLE code for both servers and clients in the `lib/esp32-libs` submodule. The BLE server code in this module abstracts a lot of the setup and management of the BLE server, allowing for easy creation of services and characteristics. Using the BLE wrapper is extremely simple, the programmer only needs to call the BLE initialization method, create an instance of the `GattServerService` class, providing a UUID for the service, and then add characteristics to the service by calling the `AddAttribute` method. The `AddAttribute` method has overloads for various types of configuration, the most common call takes a UUID for the characteristic, the permissions for the characteristic, the maximum size of the value, a read callback and a write callback.  
Getting the attribute creation to be this easy was no simple feat, as the esp-idf ble api is extremely low level and the documentation is rather misleading. To summaries how it works when a service is created, all characteristics must be defined before exposing the service, upon exposing the service an attribute table is created, though this attribute table does not have UUIDs that correspond to the characteristics UUIDs, so I had to create a mapping between the characteristic UUIDs and the attribute table UUIDs that are dynamically generated during runtime. Then when the GattServerEventHandler is called, the service UUID is checked and the event is passed to the `GattServerService` instance that corresponds to the service UUID, this then checks the attribute mapping table and calls the correct callback for the characteristic that the event is related to. It is important that management of the references kept in this mapping table are managed correctly as this required a lot of dynamic memory allocation so it is important that the memory is freed when the service is destroyed.

The Bluetooth API allows the end user to customize the OpenTCU settings and view runtime stats via a mobile application.  

Read/write properties exposed via the OpenTCU BLE API:
- Real wheel circumference
- Virtual wheel circumference

Read-only properties exposed via the OpenTCU BLE API:
- Reported wheel speed
- Real wheel speed
- Battery voltage
- Battery current
- Ease assist level
- Power assist level
- Walk mode status

There are also some additional BLE characteristics that are used for debugging purposes, such as the ability to write custom data to a specific bus, this us useful for testing codes and values on the fly without having to recompile and flash new firmware to the ESP32.
