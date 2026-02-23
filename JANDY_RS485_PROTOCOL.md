# Jandy RS485 Protocol Documentation

## Overview

This document describes the Jandy RS485 serial communication protocol used by Aqualink control systems to communicate with various pool and spa equipment on the RS485 network. This protocol is reverse-engineered from the AqualinkD project's source code analysis.

The RS485 protocol is used by the following Jandy/Pentair systems:
- Aqualink RS8, RS12, RS16 (and variants)
- Aquapure (SWG - Salt Water Chlorinator) systems
- Jandy ePumps (variable speed pumps)
- JXi and LX Heaters
- Chemical feeder and analyzer systems
- Heat pumps
- Color control lights
- iAqualinkTouch panels
- OneTouch keypads
- AllButton simulators
- Remote control adapters

---

## Physical Layer

### Serial Port Configuration

- **Baud Rate**: 9600 bps
- **Data Bits**: 8
- **Stop Bits**: 1
- **Parity**: None
- **Flow Control**: None (hardware or software)
- **Serial Standard**: RS485
- **Bus Topology**: Multi-drop (up to multiple devices on single RS485 bus)

### Serial Port Initialization

From `aq_serial.c`:

```c
// Open with non-blocking mode and no controlling terminal
_RS485_fds = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);

// Configure for 8N1, 9600 baud
cfsetispeed(&tty, B9600);
cfsetospeed(&tty, B9600);
tty.c_cflag &= ~PARENB;    // No parity
tty.c_cflag &= ~CSTOPB;    // 1 stop bit
tty.c_cflag |= CS8;         // 8 data bits
tty.c_cflag &= ~CRTSCTS;    // No hardware flow control
```

### Port Modes

- **Non-blocking I/O**: Used to prevent blocking on Data Carrier Detect (DCD)
- **Raw Mode**: No canonical processing, no echo
- **Read Timeout**: Configurable via `VMIN=0` and `VTIME=10` (1 second timeout)
- **Low Latency Mode**: Can be set via `ASYNC_LOW_LATENCY` ioctl for FTDI adapters

---

## Protocol Overview

### Two Supported Protocols

The AqualinkD system supports two main protocols:

1. **Jandy Protocol** (DLE/STX/ETX based)
2. **Pentair Protocol** (PP1/PP2/PP3/PP4 header based)

This document focuses on the **Jandy Protocol**.

---

## Frame Structure - Jandy Protocol

### Basic Packet Format

The Jandy protocol uses a simple framing schema with delimiters and checksums:

```
 [DLE] [STX] [DEST] [CMD] [DATA...] [CHECKSUM] [DLE] [ETX] 
   1     2      3     4      5-N       N+1      N+2   N+3  
```

Where:
- **DLE**: Data Link Escape (0x10) - frame start marker
- **STX**: Start of Text (0x02) - indicates start of frame
- **DEST**: Destination device ID (0x00 = master/controller)
- **CMD**: Command byte indicating packet type
- **DATA**: Variable length payload (0 to many bytes)
- **CHECKSUM**: Single byte checksum (sum of DEST through last DATA byte, masked to 8 bits)
- **DLE**: Data Link Escape (0x10) - frame end marker
- **ETX**: End of Text (0x03) - indicates end of frame

---

## Frame Structure - Pentair Protocol

### Basic Packet Format

The Pentair protocol uses a different frame structure:

```
[PP1] [PP2] [PP3] [PP4] [FROM] [DEST] [CMD] [LENGTH] [DATA...] [CHKSUM_HI] [CHKSUM_LO]
  1     2     3     4      5      6      7      8       9-N       N+1        N+2
```

Where:
- **NUL**: Null byte (0x00) - padding
- **PP1**: Protocol Marker 1 (0xFF) - Pentair frame start
- **PP2**: Protocol Marker 2 (0x00) - part of header
- **PP3**: Protocol Marker 3 (0xFF) - part of header
- **PP4**: Protocol Marker 4 (0xA5) - completes 4-byte header
- **FROM**: Source device ID
- **DEST**: Destination device ID (0x10 = master)
- **CMD**: Command byte (0x01=Speed, 0x04=RemoteCtl, 0x06=Power, 0x07=Status)
- **LENGTH**: Data length (number of data bytes following)
- **DATA**: Variable length payload
- **CHKSUM_HI**: 16-bit checksum high byte (sum of CMD through last DATA byte)
- **CHKSUM_LO**: 16-bit checksum low byte

The Pentair protocol is used primarily by Pentair IntelliFlo and variable speed pumps (device IDs 0x60-0x6F).

---

## Jandy Protocol

### Important Note on DLE Escaping

If the value 0x10 (DLE) appears in the data portion of the packet (after STX and before the final DLE), it must be escaped by inserting a NUL byte (0x00) after it. The parser must skip these escape sequences.

### Frame Delimiters - Definitions

| Constant | Value | Name |
|----------|-------|------|
| DLE | 0x10 | Data Link Escape |
| STX | 0x02 | Start of Text |
| ETX | 0x03 | End of Text |


---

## Packet Constants and Offsets

### Standard Packet Offsets

Within the data portion (after STX):

| Offset | Field | Description |
|--------|-------|-------------|
| 0-1 | Header | DLE (0x10) + STX (0x02) when counting from packet start |
| 2 | PKT_DEST | Destination device ID (0x00 = master) |
| 3 | PKT_CMD | Command type |
| 4+ | PKT_DATA | Command-specific data |



---

## Device IDs

### Master/Controller

| Device | ID Range | Hex | Notes |
|--------|----------|-----|-------|
| Master/Controller | 0x00 | 0x00 | Destination for all ACKs and responses |

### Control Panels/Keypads

| Device | ID Range | Decimal | Hex | Count | Notes |
|--------|----------|---------|-----|-------|-------|
| AllButton | 0x08-0x0B | 8-11 | 0x08-0x0B | 4 | Emulated keypad |
| OneTouch | 0x40-0x43 | 64-67 | 0x40-0x43 | 4 | Physical keypad |
| AqualinkTouch (iAqlnk Touch) | 0x30-0x33 | 48-51 | 0x30-0x33 | 4 | Touch panel |
| iAqualink2 (Jandy Link) | 0xA0-0xA3 | 160-163 | 0xA0-0xA3 | 4 | Cloud interface |
| PDA (AquaPalm) | 0x60-0x63 | 96-99 | 0x60-0x63 | 4 | Handheld remote |
| RS Serial Adapter | 0x48-0x49 | 72-73 | 0x48-0x49 | 2 | Serial gateway |

### Equipment Devices

| Device | ID Range | Decimal | Hex | Count | Notes |
|--------|----------|---------|-----|-------|-------|
| Aquapure SWG | 0x50-0x53 | 80-83 | 0x50-0x53 | 4 | Salt chlorinator |
| LX Heater | 0x38-0x3B | 56-59 | 0x38-0x3B | 4 | Heating system |
| JXi Heater | 0x68-0x6B | 104-107 | 0x68-0x6B | 4 | Jandy JXi heater |
| ePump Standard | 0x78-0x7B | 120-123 | 0x78-0x7B | 4 | Variable speed pump (standard range) |
| ePump Extended | 0xE0-0xE3 | 224-227 | 0xE0-0xE3 | 4 | Variable speed pump (extended for panel rev W+) |
| Heat Pump | 0x70-0x73 | 112-115 | 0x70-0x73 | 4 | Heat pump device |
| Chemistry Feeder | 0x80-0x83 | 128-131 | 0x80-0x83 | 4 | Chemical feeder (ChemLink) |
| Chemistry Analyzer | 0x84-0x87 | 132-135 | 0x84-0x87 | 4 | Chemical analyzer (TrueSense, guess) |
| Jandy Lights | 0xF0-0xF4 | 240-244 | 0xF0-0xF4 | 5 | Colored light control |
| Spa Remote | 0x20-0x23 | 32-35 | 0x20-0x23 | 4 | Remote control for spa |
| Remote Power Center | 0x28-0x2B | 40-43 | 0x28-0x2B | 4 | Remote power management |
| PC Dock | 0x58-0x5B | 88-91 | 0x58-0x5B | 4 | PC docking station |

---

## Command Types (CMD Byte)

### Core Commands

| Command | Value | Name | Direction | Description |
|---------|-------|------|-----------|-------------|
| CMD_PROBE | 0x00 | Probe | To Device | Polling/probe message |
| CMD_ACK | 0x01 | Acknowledge | From Device | Acknowledgment response |
| CMD_STATUS | 0x02 | Status | Bidirectional | Status information (display panels) |
| CMD_MSG | 0x03 | Message | To Device | Display message (16 bytes) |
| CMD_MSG_LONG | 0x04 | Long Message | To Device | Display message (128 bytes) |
| CMD_MSG_LOOP_ST | 0x08 | Message Loop Start | From Device | Start message loop cycle |

### Checksum-Related Constants

| Constant | Value | Description |
|----------|-------|-------------|
| ACK_NORMAL | 0x80 | Normal ACK response |
| ACK_SCREEN_BUSY_SCROLL | 0x81 | Screen busy, cache next message |
| ACK_SCREEN_BUSY_BLOCK | 0x83 | Screen busy, don't send more |

Panel compatibility notes:
- Some keypads use 0x00, others 0x80 (version/implementation dependent)
- Using 0x80 for ACK may trigger CMD_MSG_LOOP_ST cycle

### Aquapure (SWG) Commands

| Command | Value | Name | Direction | Description |
|---------|-------|------|-----------|-------------|
| CMD_PERCENT | 0x11 | Set SWG % | To SWG | Set chlorine generation percentage (0-100, >100 = boost) |
| CMD_PPM | 0x16 | PPM/Status | From SWG | Return PPM and device status |

Example: Set SWG to 75%:
```
[0x10, 0x02, 0x50, 0x11, 0x4B, checksum, 0x10, 0x03]
                     ↑    ↑    ↑
                   SWG   %SET  75 decimal
```

### ePump Commands

| Command | Value | Name | Direction | Description |
|---------|-------|------|-----------|-------------|
| CMD_EPUMP_STATUS | 0x1F | Status Request | Bidirectional | Get/Set pump status (RPM, Watts, GPM) |
| CMD_EPUMP_RPM | 0x44 | Set RPM | To Pump | Set pump speed (RPM mode) |
| CMD_EPUMP_WATTS | 0x45 | Set Watts | To Pump | Set pump power (Watts mode) |

Response format from ePump (0x1F):
```
CMD=0x1F | Next CMD | Unused | Unused | Hi_Watts | Lo_Watts | Hi_RPM | Lo_RPM | ...
Bytes:   4        5        6        7          8         9       10      11
```

Calculation examples:
- Watts = (Byte8 × 256) + Byte7
- RPM = (Byte10 × 256) + Byte11

### Heater Commands

#### JXi Heater

| Command | Value | Name | Description |
|---------|-------|------|-------------|
| CMD_JXI_PING | 0x0C | Ping | Poll heater status |
| CMD_JXI_STATUS | 0x0D | Status | Return heater status |

#### LX Heater

Similar to JXi but with different device ID range (0x38-0x3B).

### iAqualinkTouch Commands

| Command | Value | Name | Description |
|---------|-------|------|-------------|
| CMD_IAQ_PAGE_START | 0x23 | Page Start | Begin new menu page |
| CMD_IAQ_PAGE_BUTTON | 0x24 | Button | Button definition for current page |
| CMD_IAQ_PAGE_MSG | 0x25 | Page Message | Message content for page |
| CMD_IAQ_TABLE_MSG | 0x26 | Table Message | Table data populate command |
| CMD_IAQ_PAGE_END | 0x28 | Page End | End of page definition |
| CMD_IAQ_STARTUP | 0x29 | Startup | Startup message |
| CMD_IAQ_POLL | 0x30 | Poll | Ready to receive commands |
| CMD_IAQ_CTRL_READY | 0x31 | Control Ready | Ready for big control command |
| CMD_IAQ_PAGE_CONTINUE | 0x40 | Page Continue | Multiple pages continue cycle |
| CMD_IAQ_MSG_LONG | 0x2C | Long Message | Display popup message |
| CMD_IAQ_MAIN_STATUS | 0x70 | Main Status | Main screen status (large, 120+ bytes) |
| CMD_IAQ_1TOUCH_STATUS | 0x71 | OneTouch Status | OneTouch emulation status |
| CMD_IAQ_AUX_STATUS | 0x72 | Auxiliary Status | Auxiliary equipment status (large) |
| CMD_IAQ_CMD_READY | 0x73 | Command Ready | Ready to receive command |
| CMD_IAQ_TITLE_MESSAGE | 0x2D | Title Message | Product name/title message |

#### iAqualinkTouch Page IDs

These define different screen pages in the iAqualinkTouch interface:

| Page | ID | Purpose |
|------|----|---------| 
| Home | 0x01 | Home screen |
| Status | 0x5B | Status screen (or 0x2A alternate) |
| Devices | 0x36 | Device control (or 0x35, 0x51 alternates) |
| Set Temperature | 0x39 | Temperature setting |
| Menu | 0x0F | Main menu |
| Set VSP | 0x1E | Variable speed pump setup |
| Set Time | 0x4B | Time setting |
| Set Date | 0x4E | Date setting |
| Set SWG | 0x30 | SWG setup |
| Set Boost | 0x1D | Chlorine boost setting |
| Set Quick Boost | 0x3F | Quick boost setting |
| OneTouch | 0x4D | OneTouch page |
| Color Light | 0x48 | Color light control |
| System Setup | 0x14 | System setup (or 0x49, 0x4A alternates) |
| VSP Setup | 0x2D | VSP detailed setup |
| Freeze Protect | 0x11 | Freeze protection |
| Label Aux | 0x32 | Auxiliary labeling |
| Help | 0x0C | Help screen |
| Service Mode | 0x5E | Service/timeout mode |

### PDA Commands

| Command | Value | Name |
|---------|-------|------|
| CMD_PDA_0x04 | 0x04 | Unknown (menu building?) |
| CMD_PDA_0x05 | 0x05 | Unknown |
| CMD_PDA_0x1B | 0x1B | Unknown |
| CMD_PDA_HIGHLIGHT | 0x08 | Highlight line |
| CMD_PDA_CLEAR | 0x09 | Clear display |
| CMD_PDA_SHIFTLINES | 0x0F | Shift display lines |
| CMD_PDA_HIGHLIGHTCHARS | 0x10 | Highlight characters |

### Serial Adapter Commands

| Command | Value | Name | Description |
|---------|-------|------|-------------|
| RSSA_DEV_STATUS | 0x13 | Device Status | Status or error response |
| RSSA_DEV_READY | 0x07 | Device Ready | Ready to receive command |

---

## Checksum Calculation

### Jandy Protocol Checksum

The checksum is calculated by summing all bytes from DEST through the last DATA byte, then masking to 8 bits.

```c
int generate_checksum(unsigned char* packet, int length)
{
  int i, sum, n;
  n = length - 3;  // Exclude DLE, ETX, and trailing NUL
  sum = 0;
  for (i = 0; i < n; i++)
    sum += (int) packet[i];
  return (sum & 0x0FF);
}
```

The checksum is placed at `packet[length-3]` (before the final DLE).

### Example Checksum Calculation

For packet: `[DLE, STX, 0x50, 0x11, 0x4B, CHKSUM, DLE, ETX]`

```
sum = 0x50 + 0x11 + 0x4B = 0xAC
checksum = 0xAC & 0xFF = 0xAC
```

### Jandy Checksum Bug Workaround

There's a known bug in the Jandy OneTouch protocol where long messages (0x0a command) to OneTouch devices have incorrect checksums. The code detects this pattern:
- packet[3] == 0x04
- packet[4] == 0x03
- packet[length-3] == 0x0a

In this case, the checksum is ignored (forced valid) with a warning log.

### Checksum Validation

```c
bool check_jandy_checksum(unsigned char* packet, int length)
{
  if (generate_checksum(packet, length) == packet[length-3])
    return true;
  
  // Known bug workaround
  if (packet[3] == 0x04 && packet[4] == 0x03 && packet[length-3] == 0x0a)
    return true;  // Forced valid with LOG warning
  
  return false;
}
```

---

## Aquapure (SWG) Protocol

### Overview

The Aquapure Salt Water Chlorinator communicates via the Jandy protocol to report its status and receive percentage/power commands.

### Device IDs: 0x50-0x53

Up to 4 Aquapure units can be independently controlled (though AqualinkD only supports one in v1.0).

### SWG Status Bytes

When the master sends a query to the SWG, the SWG responds with its operational status.

| Status | Value | Name | Description |
|--------|-------|------|-------------|
| SWG_STATUS_ON | 0x00 | Normal Operation | Generating chlorine |
| SWG_STATUS_NO_FLOW | 0x01 | No Flow | Water not flowing through cell |
| SWG_STATUS_LOW_SALT | 0x02 | Low Salt | Salt level too low |
| SWG_STATUS_HI_SALT | 0x04 | High Salt | Salt level too high |
| SWG_STATUS_CLEAN_CELL | 0x08 | Clean Cell | Cell cleaning cycle active |
| SWG_STATUS_TURNING_OFF | 0x09 | Turning Off | Shutdown in progress |
| SWG_STATUS_HIGH_CURRENT | 0x10 | High Current | Excessive current draw |
| SWG_STATUS_LOW_VOLTS | 0x20 | Low Voltage | Insufficient voltage |
| SWG_STATUS_LOW_TEMP | 0x40 | Low Temperature | Water temperature too low |
| SWG_STATUS_CHECK_PCB | 0x80 | Check PCB | PCB/control board error |
| SWG_STATUS_GENFAULT | 0xFD | General Fault | General fault state |
| SWG_STATUS_UNKNOWN | 0xFE | Unknown | No status received |
| SWG_STATUS_OFF | 0xFF | Off | Device off (AqualinkD internal state) |

### Example SWG Communication Flow

**Query SWG Status:**
```
Master → SWG: [DLE, STX, 0x50, 0x02, CHKSUM, DLE, ETX]
                          ↑    ↑
                        SWG   CMD_STATUS
```

**SWG Response:**
```
SWG → Master: [DLE, STX, 0x00, 0x16, PPM_VAL, STATUS, CHKSUM, DLE, ETX]
                           ↑    ↑     ↑        ↑
                        Master CMD_PPM PPM*100  Status byte
```

Where:
- PPM_VAL: Division factor for PPM (actual PPM = PPM_VAL × 100)
- STATUS: One of the status values above
- Example: `[DLE, STX, 0x00, 0x16, 0x0C, 0x00, CHKSUM, DLE, ETX]` means 1200 PPM, On

**Set SWG Percentage:**
```
Master → SWG: [DLE, STX, 0x50, 0x11, PERCENT, CHKSUM, DLE, ETX]
                          ↑    ↑     ↑
                        SWG   %SET   0-100 (>100 = boost mode)
```

Example: Set to 75%:
```
[DLE, STX, 0x50, 0x11, 0x4B, CHKSUM, DLE, ETX]  (0x4B = 75 decimal)
```

**Boost Mode:**
```
Master → SWG: [DLE, STX, 0x50, 0x11, 0x65, CHKSUM, DLE, ETX]  (0x65 = 101 decimal)
```

### SWG Packet Examples

From actual systems (from protocol notes):

```
AR %% | HEX: 0x10|0x02|0x50|0x11|0xff|0x72|0x10|0x03|
       In service/timeout: Set to 0xFF (all on in special mode)

SWG response: 0x10|0x02|0x00|0x16|0x0C|0x00|0x1E|0x10|0x03|
       Status = 0x00 (on), PPM = 0x0C (1200 PPM)
```

---

## ePump (Variable Speed Pump) Protocol

### Overview

Jandy ePumps are variable-speed DC or AC motors that can operate in RPM or Watts mode. They communicate via the RS485 bus to report their current operational status and receive speed/power commands.

### Device IDs

- **Standard Range**: 0x78-0x7B (120-123 decimal)
- **Extended Range**: 0xE0-0xE3 (224-227 decimal) - Panel revision W and later

### ePump Status and Control

#### Get/Set Watts

**Command**: CMD_EPUMP_WATTS (0x45)

```
Master → Pump: [DLE, STX, 0x78, 0x45, 0x00, HI_WATTS, LO_WATTS, CHKSUM, DLE, ETX]
                          ↑    ↑           ↑         ↑
                        Pump   WATTS_CMD  Reserved  Watts value (16-bit)

Pump → Master: [DLE, STX, 0x00, 0x1F, 0x45, 0x00, HI_WATTS, LO_WATTS, ..., CHKSUM, DLE, ETX]
                          ↑    ↑     ↑
                       Master  STATUS (0x1F)  Original CMD echoed
```

Watts Calculation: `watts = (packet[8] × 256) + packet[7]`

**Example: Set to 1309 Watts**
```
1309 = 0x51D
Hi_byte = 0x05  (1309 >> 8)
Lo_byte = 0x1D  (1309 & 0xFF)

Packet: [DLE, STX, 0x78, 0x45, 0x00, 0x05, 0x1D, CHKSUM, DLE, ETX]
```

#### Get/Set RPM

**Command**: CMD_EPUMP_RPM (0x44)

Similar format to Watts but for RPM control:

```
Master → Pump: [DLE, STX, 0x78, 0x44, 0x00, HI_RPM, LO_RPM, CHKSUM, DLE, ETX]
Pump → Master: [DLE, STX, 0x00, 0x1F, 0x44, 0x00, HI_RPM, LO_RPM, ..., CHKSUM, DLE, ETX]
```

RPM Calculation: `rpm = (packet[6] × 256) + packet[7]`

#### Status Response Structure

When pump responds with status (0x1F), the full packet contains:

| Offset | Field | Description |
|--------|-------|-------------|
| 0-1 | Header | DLE + STX |
| 2 | DEST | 0x00 (Master) |
| 3 | CMD | 0x1F (Status) |
| 4 | Orig_CMD | Echo of original command (0x44, 0x45, etc.) |
| 5 | Reserved | 0x00 |
| 6-7 | WATTS_HI/LO | Current watts (16-bit) |
| 8-9 | RPM_HI/LO | Current RPM (16-bit) |
| 10+ | Other fields | Pressure, temperature, etc. (device dependent) |

---

## Heater Protocol

### JXi Heater (Jandy)

**Device IDs**: 0x68-0x6B (104-107 decimal)

#### Commands

| Command | Value | Description |
|---------|-------|-------------|
| CMD_JXI_PING | 0x0C | Ping/poll heater |
| CMD_JXI_STATUS | 0x0D | Status response |

#### Status Indicators

From status packets (CMD_JXI_STATUS):
- Byte 6 == 0x10: Error condition
- Other values: Normal operational status

#### Example Packets

```c
// From protocol notes in devices_jandy.c
"LXi status | HEX: 0x10|0x02|0x00|0x0d|0x00|0x00|0x00|0x1f|0x10|0x03|"
"LXi status | HEX: 0x10|0x02|0x00|0x0a|0x00|0x00|0x00|0x1f|0x10|0x03|"
                                                              ↑ ERROR if 0x10
```

### LX Heater

**Device IDs**: 0x38-0x3B (56-59 decimal)

Similar protocol to JXi but with different device ID range.

---

## Chemical Control Protocol

### Chemistry Feeder (ChemLink)

**Device IDs**: 0x80-0x83 (128-131 decimal)

Chemistry feeders communicate to dispense specific chemicals or adjust chemical dosing levels.

### Chemistry Analyzer (TrueSense)

**Device IDs**: 0x84-0x87 (132-135 decimal) [GUESS - not fully documented in code]

These devices report chemical levels (pH, ORP, salt levels, etc.) back to the control system.

---

## Heat Pump Protocol

**Device IDs**: 0x70-0x73 (112-115 decimal)

Heat pumps can operate in heating or cooling mode and report their operational state (on/off, heating/cooling, error state, etc.) via the RS485 protocol.

---

## Light Control Protocol (Jandy Lights)

**Device IDs**: 0xF0-0xF4 (240-244 decimal)

Color-changing lights can be controlled to display different colors and brightness levels via RS485 commands.

---

## iAqualinkTouch Protocol

### Overview

The iAqualinkTouch is a more advanced control panel with a graphical display and touch interface, communicating via the Jandy RS485 protocol but with more complex message structures for GUI rendering.

### Device IDs: 0x30-0x33

### Key Protocol Features

1. **Paged Menus**: Screens are defined with multiple commands
2. **Large Status Messages**: Status packets can be 120+ bytes (up to full packet buffer)
3. **Button Definitions**: Dynamic button layout per page
4. **Table Messages**: Structured data (equipment lists, parameters, etc.)

### Page Definition Flow

```
Master → Touch: PAGE_START (0x23) - Begin new page
Master → Touch: PAGE_BUTTON (0x24) - Define button
Master → Touch: PAGE_MSG (0x25) - Add text/content
Master → Touch: PAGE_CONTINUE (0x40) - More pages follow
Master → Touch: PAGE_END (0x28) - Page complete
```

### Status Messages

#### Main Status (0x70)
Large packet (typically 100+ bytes) containing complete system status suitable for main equipment view.

#### OneTouch Status (0x71)
Status suitable for OneTouch emulation mode.

#### AUX Status (0x72)
Status for auxiliary equipment (pumps, heaters, etc.).

### Button Keys (Key Codes)

**Navigation Keys:**
| Key | Value | Function |
|-----|-------|----------|
| HOME | 0x01 | Home page |
| MENU | 0x02 | Menu |
| ONETOUCH | 0x03 | OneTouch |
| HELP | 0x04 | Help |
| BACK | 0x05 | Back |
| STATUS | 0x06 | Status |
| PREV_PAGE | 0x20 | Previous page |
| NEXT_PAGE | 0x21 | Next page |

**Grid Keys (Numbered 0x11-0x1F):**

The screen has a 3×5 grid of buttons:
```
Button Layout:
Column 1    Column 2    Column 3
Row 1: 0x11 Row 1: 0x16 Row 1: 0x1B (KEY01-KEY05 = Column 1)
Row 2: 0x12 Row 2: 0x17 Row 2: 0x1C (KEY06-KEY10 = Column 2)
Row 3: 0x13 Row 3: 0x18 Row 3: 0x1D (KEY11-KEY15 = Column 3)
Row 4: 0x14 Row 4: 0x19 Row 4: 0x1E
Row 5: 0x15 Row 5: 0x1A Row 5: 0x1F
```

---

## Control Panel Communication

### ACK (Acknowledgment) Responses

After receiving a command, devices must respond with an ACK to indicate successful receipt.

#### ACK Types

| Type | Value | Purpose |
|------|-------|---------|
| ACK_NORMAL | 0x80 | Normal acknowledgment |
| ACK_SCREEN_BUSY_SCROLL | 0x81 | Screen busy but can cache next message |
| ACK_SCREEN_BUSY_BLOCK | 0x83 | Screen busy, stop sending |

#### ACK Packet Structure

```c
// From aq_serial.c - _send_ack()
unsigned char ackPacket[] = { DLE, STX, DEV_MASTER, CMD_ACK, ack_type, command, checksum, DLE, ETX };
```

Example - Send normal ACK:
```
[DLE, STX, 0x00, CMD_ACK, 0x80, 0x00, CHKSUM, DLE, ETX]
                          ↑    ↑
                      ACK_NORMAL  No command
```

#### Special Case: DLE Escaping in ACK

If the command being acknowledged is DLE (0x10), the packet must be escaped:

```
Original: [DLE, STX, 0x00, CMD_ACK, 0x80, 0x10, CHKSUM, DLE, ETX]
Escaped : [DLE, STX, 0x00, CMD_ACK, 0x80, 0x10, 0x00, 0x10, DLE, ETX]
                                              ↑    ↑ escape NUL + extra DLE
```

### OneTouch Keypad Keys

OneTouch keypads communicate keypress events via specific key codes:

| Key | Value | Function |
|-----|-------|----------|
| UP | 0x06 | Up arrow |
| DOWN | 0x05 | Down arrow |
| SELECT | 0x04 | Select/OK |
| PAGE_UP/SELECT_1 | 0x03 | Top button |
| BACK/SELECT_2 | 0x02 | Middle button |
| PAGE_DOWN/SELECT_3 | 0x01 | Bottom button |

From display lines, OneTouch can control equipment on/off or navigate menu:

```
Keypress → Master: [DLE, STX, 0x00, key_code, details, CHKSUM, DLE, ETX]
```

---

## Packet Reception and Parsing

### Packet Reception State Machine

From `get_packet()` in `aq_serial.c`:

1. **Wait for DLE**: First byte must be 0x10
2. **Expect STX**: Next byte must be 0x02
3. **Collect Payload**: Read bytes into buffer, tracking index
4. **Handle Escaping**: If byte is DLE, check if followed by NUL (escape) or 0x03 (end)
5. **Validate Checksum**: Verify calculated checksum matches packet
6. **Return Success/Error**

### Error Codes

| Code | Symbol | Meaning |
|------|--------|---------|
| 0 | Success | Packet received and valid |
| -1 | AQSERR_READ | Serial read error |
| -2 | AQSERR_TIMEOUT | Read timeout |
| -3 | AQSERR_CHKSUM | Checksum validation failed |
| -4 | AQSERR_2LARGE | Packet exceeds max length |
| -5 | AQSERR_2SMALL | Packet too short (<5 bytes) |

### Packet Receiving - Frame Format Detection

The parser automatically detects protocol type:

```c
protocolType getProtocolType(const unsigned char* packet)
{
  if (packet[0] == DLE)          // 0x10
    return JANDY;                // This is Jandy protocol
  else if (packet[0] == PP1)     // 0xFF
    return PENTAIR;              // This is Pentair protocol
  return P_UNKNOWN;
}
```

---

## Frame Delay and Timing

### Frame Delay Configuration

The system supports configurable delay between packet transmission and reception to prevent bus collisions:

```c
_aqconfig_.frame_delay  // Configurable delay in milliseconds
```

When `frame_delay > 0`:
- Track time of last serial read
- Before sending packet, wait until minimum elapsed time since last read
- Calculate minimum wait = `frame_delay` ms
- Use `nanosleep()` for precise timing

### Example Timing

```
Last Read:    T=0ms
Send Delay:   frame_delay = 50ms
Current Time: T=30ms
Must wait:    20ms more before sending
```

---

## Known Issues & Quirks

### 1. Jandy OneTouch Long Message Checksum Bug

**Description**: Long messages (0x0A command) to OneTouch devices sometimes have invalid checksums.

**Detection Pattern**:
```c
if (packet[3] == 0x04 && packet[4] == 0x03 && packet[length-3] == 0x0a)
  // This is the known bug - force checksum valid
```

**Workaround**: AqualinkD logs a debug message and accepts the packet anyway.

### 2. SWG Status Interpretation

**Issue**: The panel sometimes shows SWG status differently than the actual device status due to timing of when messages are received.

**Examples**:
- "AQUAPURE" prefix is 8 characters in display (MSG_SWG_PCT_LEN)
- "SALT" prefix is 4 characters (MSG_SWG_PPM_LEN)
- Timeouts or missing ACKs can mark SWG as offline

### 3. Multiple SWG Support

**Current Limitation**: AqualinkD only supports one SWG device despite the protocol allowing up to 4 (IDs 0x50-0x53).

**Code Comment**:
```c
// Capture the SWG ID. We could have more than one, but for the moment 
// AqualinkD only supports one so we'll pick the first one.
```

### 4. ePump Extended ID Range

**Note**: Newer panel revisions (Rev W and later) support additional ePump IDs in range 0xE0-0xE3 in addition to the standard 0x78-0x7B.

### 5. iAqualinkTouch Large Packets

**Issue**: iAqualinkTouch status packets (0x70, 0x71, 0x72) can exceed 128 bytes, requiring larger buffer.

**Solution**: Max packet length increased to 512 bytes, with warning log for packets over 128 bytes (except for those specific status commands).

---

## Pentair Protocol (Brief Overview)

For reference, the Pentair protocol is also supported:

### Pentair Packet Header

```
[0xFF] [0x00] [0xFF] [0xA5] [FROM] [DEST] [CMD] [LENGTH] [DATA...] [CHKSUM_HI] [CHKSUM_LO]
```

### Pentair Device IDs

- **Master**: 0x10
- **Pumps**: 0x60-0x6F (96-111 decimal)

### Pentair Commands

| Command | Value | Description |
|---------|-------|-------------|
| PEN_CMD_SPEED | 0x01 | Set pump speed (RPM or GPM) |
| PEN_CMD_REMOTECTL | 0x04 | Remote control |
| PEN_CMD_POWER | 0x06 | Set pump power (ON/OFF) |
| PEN_CMD_STATUS | 0x07 | Status request/response |

### Pentair Checksum

16-bit checksum calculated over data portion, stored as two bytes (high, low).

---

## Implementation Notes

### Packet Logging

The system can log all RS485 packets in multiple formats:

- **Raw Byte Logging**: Individual bytes as received
- **Formatted Logging**: Hex display with protocol interpretation
- **Pretty Printing**: Human-readable descriptions

### Configuration Options (from _aqconfig_)

```c
_aqconfig_.ftdi_low_latency          // Enable low latency mode for FTDI adapters
_aqconfig_.frame_delay               // Delay between packets (milliseconds)
_aqconfig_.log_protocol_packets      // Log all packets to file
_aqconfig_.log_raw_bytes             // Log individual bytes received
```

### Bit Shifting and Multi-Byte Values

When combining two bytes into a 16-bit value:

```c
uint16_t value_16bit = (high_byte << 8) | low_byte
                     = (high_byte * 256) + low_byte
```

Example: RPM from bytes [0x0B, 0x82]:
```
RPM = (0x0B * 256) + 0x82 = 2816 + 130 = 2946 RPM
```

---



### Packet Capture Example

Real packets from the decoding directory (from repository):

```
AR %% Set to 75%:
HEX: 0x10|0x02|0x50|0x11|0x4B|0x72|0x10|0x03|

SWG Response PPM=1200, Status=ON:
HEX: 0x10|0x02|0x00|0x16|0x0C|0x00|0x1E|0x10|0x03|

ePump Watts Response:
HEX: 0x10|0x02|0x00|0x1f|0x45|0x00|0x05|0x1d|0x10|0x03|
     (Watts = 0x051D = 1309)

LXi Heater Status:
HEX: 0x10|0x02|0x00|0x0d|0x00|0x00|0x00|0x1f|0x10|0x03|
```

---

## References

### Source Files

Key files in the AqualinkD project that implement this protocol:

- `source/aq_serial.c` - Serial I/O and low-level protocol handling
- `source/aq_serial.h` - Protocol definitions and constants
- `source/devices_jandy.c` - Jandy device handling and packet processing
- `source/devices_jandy.h` - Jandy device declarations
- `source/devices_pentair.c` - Pentair protocol implementation
- `source/rs_devices.h` - Device ID ranges and helper functions
- `source/packetLogger.c` - Packet logging utilities

### Relevant Constants

See `aq_serial.h` for:
- Frame delimiters (NUL, DLE, STX, ETX)
- Command byte definitions (CMD_*)
- ACK response types
- Device ID ranges
- Packet length limits
- Error codes

---

## Conclusion

The Jandy RS485 protocol is a mature, well-established protocol used across Jandy Aqualink pool control systems. While it has some quirks and undocumented messages, the AqualinkD project provides a solid open-source reference implementation.

The protocol supports:
- Multiple device types (pumps, heaters, chemical control, lighting)
- Multiple panel types (OneTouch, AqualinkTouch, PDA, AllButton emulation)
- Reliable communication via checksums
- Extensibility for new device types and panels

Future versions may add support for:
- Multiple SWG devices simultaneously
- Additional undocumented device types or commands
- Protocol optimizations and performance improvements

---

