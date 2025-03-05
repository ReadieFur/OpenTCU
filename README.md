# OpenTCU

Specialized TCU reverse engineered!

The end goal of this project is to completely reverse engineer the TCU so the bike can operate without the original TCU (this would also allow keeping the TCU from detecting abnormal data).

*This project is under heavy research and development, so expect a lot of changes and updates.*

## Table of Contents
- [OpenTCU](#opentcu)
  - [Table of Contents](#table-of-contents)
  - [Current Progress](#current-progress)
  - [Documentation](#documentation)

## Current Progress
- Hardware
  - [x] CAN bus interception
  - [x] Hardware selection
  - [x] PCB design
  - [x] Connector replication
  - [ ] Waterproofing
- Software
  - Core
    - [x] Service manager
  - CAN bus
    - [x] Interception & relay
    - Decoding
      - [x] Speed data
      - [x] Assistance mode
      - [ ] Battery data (partial)
      - [ ] Configuration data (partial)
      - [ ] Motor data
      - [ ] TCU clock
      - [ ] Rider stats
      - [ ] Diagnostic information
    - [x] Modification
  - API
    - [x] BLE API
    - [x] OTA Updates
    - [ ] TCU API

## Documentation
Documentation is brief at the moment but will be expanded upon as the project progresses.  
| Document | Description |
| :-- | :-- |
| [Hardware](Documentation/Hardware.md) | Overview of files and discoveries made about the hardware of the TCU and the reverse engineered models that are used. |
| [Bus Decoding](Documentation/BusDecoding.md) | Overview of the process taken to reverse engineer the CAN bus recordings and discoveries about the data that is sent on the bus. |
| [Software](Documentation/Software.md) | Quick insight to the software decisions and implementations made. |
