# ESP32 Droid Control System

A web-based control system for animatronic droids using ESP32, featuring LED eye control, servo movements via Pololu Maestro, and optional MP3 sound effects via DFPlayer Mini.

## Features

- **WiFi Access Point**: ESP32 creates its own WiFi network for direct device control
- **Web Interface**: Responsive landscape-optimized tablet interface
- **LED Eye Control**: 3 addressable LEDs (WS2812B/NeoPixel) for expressive eyes
- **Servo Control**: Optional integration with Pololu Maestro for complex servo sequences
- **Sound Effects**: Optional DFPlayer Mini for MP3 audio playback
- **Modular Design**: Run with any combination of components (LEDs only, LEDs + Servos, full system)
- **Multiple Emotes**: Pre-configured emotional states and directional movements
- **Flashlight Mode**: Independent LED 3 control for utility lighting

## Hardware Requirements

### Core Components
- **ESP32 Development Board**
- **3x WS2812B/NeoPixel LEDs** (addressable RGB LEDs)
- **Pololu Maestro Servo Controller** (Mini Maestro) - optional, can be disabled
- **DFPlayer Mini MP3 Module** - optional, auto-detected
- **MicroSD Card** (for DFPlayer, if using audio)
- **5V Power Supply** (adequate for servos and LEDs)

**Note**: Both the Maestro and DFPlayer are optional. The system will operate with LEDs and web interface even if these modules are not connected.

### Recommended
- Tablet (landscape orientation) for web interface
- Speaker (for DFPlayer audio output)

## Wiring Connections

**📋 See [WIRING_DIAGRAM.txt](WIRING_DIAGRAM.txt) for detailed ASCII art wiring diagrams and step-by-step connection guide.**

### ESP32 Pin Assignments

| Component | ESP32 Pin | Serial Port | Notes |
|-----------|-----------|-------------|-------|
| **LED Strip Data** | GPIO 5 | - | WS2812B data line |
| **Maestro RX** | GPIO 16 | Serial1 | Receives from Maestro TX |
| **Maestro TX** | GPIO 17 | Serial1 | Transmits to Maestro RX |
| **DFPlayer RX** | GPIO 9 | Serial2 | Receives from DFPlayer TX |
| **DFPlayer TX** | GPIO 10 | Serial2 | Transmits to DFPlayer RX |

**Note**: All pin assignments are easily configurable at the top of the sketch. See [Serial Port Configuration](#serial-port-configuration) below.

### LED Strip (WS2812B)
```
ESP32 GPIO 5 → LED Data In
ESP32 GND     → LED GND
5V Power      → LED VCC
```

### Pololu Maestro (Serial1)
```
ESP32 GPIO 17 (TX) → Maestro RX
ESP32 GPIO 16 (RX) → Maestro TX
ESP32 GND          → Maestro GND
```

### DFPlayer Mini (Serial2)
```
ESP32 GPIO 10 (TX) → DFPlayer RX (through 1kΩ resistor)
ESP32 GPIO 9  (RX) → DFPlayer TX
ESP32 GND          → DFPlayer GND
5V                 → DFPlayer VCC
DFPlayer SPK+      → Speaker +
DFPlayer SPK-      → Speaker -
```

**Note**: DFPlayer RX typically requires a 1kΩ resistor between ESP32 TX and DFPlayer RX pin.

## Software Requirements

### Arduino IDE Setup

1. **Install ESP32 Board Support**
   - Add to Board Manager URLs: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board → Boards Manager → Search "ESP32" → Install

2. **Install Required Libraries**
   - `FastLED` by Daniel Garcia
   - `PololuMaestro` by Pololu
   - `DFRobotDFPlayerMini` by DFRobot

   Install via: Sketch → Include Library → Manage Libraries

## Configuration

### Basic Settings

Edit these variables at the top of the sketch:

```cpp
// Droid Configuration
String droidname = "Grek";           // WiFi SSID name
String droidcolor = "green";         // Button color (CSS color name)
const char* ap_password = "k7Rm9pQx2w";  // WiFi password (8-63 chars)

// LED Configuration
#define NUM_LEDS 3        // Number of LEDs
#define LED_PIN 5         // Data pin for LED strip
#define LED_TYPE WS2812B  // LED chip type
```

### Serial Port Configuration

All serial port settings are centralized at the top of the sketch for easy configuration:

```cpp
// Serial Port Configuration
// Maestro Servo Controller
#define MAESTRO_ENABLED true      // Set to false if Maestro not connected
#define MAESTRO_SERIAL_NUM 1      // Use Serial1
#define MAESTRO_RX_PIN 16         // ESP32 RX (connects to Maestro TX)
#define MAESTRO_TX_PIN 17         // ESP32 TX (connects to Maestro RX)
#define MAESTRO_BAUD 9600

// DFPlayer Mini MP3 Module
#define DFPLAYER_SERIAL_NUM 2     // Use Serial2
#define DFPLAYER_RX_PIN 9         // ESP32 RX (connects to DFPlayer TX)
#define DFPLAYER_TX_PIN 10        // ESP32 TX (connects to DFPlayer RX)
#define DFPLAYER_BAUD 9600
```

**To disable Maestro**: Set `MAESTRO_ENABLED false` if the Maestro board is not connected. The system will operate normally with LEDs and sound, skipping servo commands.

**To change pins**: Simply modify the `_RX_PIN` and `_TX_PIN` values.

**To use different serial ports**: Change `MAESTRO_SERIAL_NUM` or `DFPLAYER_SERIAL_NUM` (ESP32 supports Serial1, Serial2).

**To change baud rates**: Modify the `_BAUD` definitions (both devices default to 9600).

### Network Settings

- **IP Address**: 192.168.4.1 (fixed)
- **Network**: 192.168.4.0/24
- **Gateway**: 192.168.4.1
- **SSID**: Uses `droidname` variable
- **Password**: Uses `ap_password` variable

### DFPlayer Volume

Adjust volume (0-30) in `setup()`:
```cpp
dfPlayer.volume(20);  // Default: 20
```

### FastLED Brightness

Adjust brightness (0-255) in `setup()`:
```cpp
FastLED.setBrightness(50);  // Default: 50
```

## Usage

### Initial Setup

1. Upload the sketch to ESP32
2. Open Serial Monitor (115200 baud) to see connection details
3. ESP32 will create a WiFi access point

### Connecting

1. **Connect to WiFi**:
   - SSID: `Grek` (or your droidname)
   - Password: `k7Rm9pQx2w` (or your custom password)

2. **Open Web Interface**:
   - Navigate to: `http://192.168.4.1`
   - Interface optimized for landscape tablets

### Available Emotes

| Emote | Eye Color | Maestro Script | Description |
|-------|-----------|----------------|-------------|
| **go to sleep** | Off | 0 | All LEDs off |
| **wake up** | White | 1 | Eyes turn white |
| **happy** | Green | 2 | Happy expression |
| **curious** | Yellow | 3 | Curious expression |
| **angry** | Red | 4 | Angry expression |
| **sad** | Blue | 5 | Sad expression |
| **idle** | White | 6 | Idle animation |
| **scared** | Purple | 11 | Scared expression |
| **look left** | Preserved | 7 | Look left (preserves eye color) |
| **look right** | Preserved | 8 | Look right (preserves eye color) |
| **look up** | Preserved | 9 | Look up (preserves eye color) |
| **look down** | Preserved | 10 | Look down (preserves eye color) |
| **flashlight on** | Preserved | None | LED 3 white (utility light) |
| **flashlight off** | Preserved | None | LED 3 off |

**Note**: "Preserved" means the emote maintains the current eye color while executing movement.

## Adding New Emotes

### Step 1: Add to Emote Array

Add a new line to the `emotes[]` array:

```cpp
const Emote emotes[] = {
  // ... existing emotes ...
  {"newemote", "New Emote", "Orange", CRGB::Orange, CRGB::Black, false, 12, 5},
  //    |           |           |          |            |         |      |   |
  //    |           |           |          |            |         |      |   MP3 track (or -1)
  //    |           |           |          |            |         |      Maestro script (or -1)
  //    |           |           |          |            |         Preserve LED 1&2? (true/false)
  //    |           |           |          |            LED 3 color
  //    |           |           |          LED 1&2 color
  //    |           |           Color name for serial output
  //    |           Button label
  //    URL path
};
```

### Step 2: Parameters Explained

- **path**: URL endpoint (alphanumeric, no spaces)
- **label**: Button text shown on web interface
- **colorName**: Descriptive name for serial debugging
- **color**: LED 1&2 color (use CRGB:: constants)
- **led3Color**: LED 3 color (usually CRGB::Black for off)
- **preserveLED12**: `true` keeps current eye color, `false` changes it
- **scriptNumber**: Maestro script number (0+) or `-1` for none
- **mp3Track**: MP3 file number (1+) or `-1` for no sound

### Available Colors

```cpp
CRGB::Black, CRGB::White, CRGB::Red, CRGB::Green, CRGB::Blue,
CRGB::Yellow, CRGB::Orange, CRGB::Purple, CRGB::Cyan, CRGB::Magenta
```

Or use RGB values: `CRGB(255, 128, 0)` for custom colors.

## DFPlayer Setup

### SD Card Preparation

1. **Format**: FAT32 format
2. **Create folder**: `mp3` or `01`
3. **Add files**: Name as `0001.mp3`, `0002.mp3`, etc.
4. **File numbers**: Match the `mp3Track` parameter in emote definitions

### File Structure Example
```
SD Card Root
└── mp3/
    ├── 0001.mp3  (Track 1 - Happy sound)
    ├── 0002.mp3  (Track 2 - Angry sound)
    ├── 0003.mp3  (Track 3 - Sad sound)
    └── ...
```

### Troubleshooting DFPlayer

- Check serial monitor for initialization messages
- Ensure SD card is formatted as FAT32
- Verify file names follow `000X.mp3` format
- Check wiring (use 1kΩ resistor on RX line)
- Try lower volume if sound is distorted

## Maestro Servo Configuration

### Script Numbers

Each emote can trigger a Maestro script. Program your servo sequences in Maestro Control Center:

- Script 0: Sleep position
- Script 1: Wake animation
- Script 2: Happy movement
- Script 3: Curious movement
- Script 4: Angry movement
- Script 5: Sad movement
- Script 6: Idle animation
- Script 7-10: Directional looks
- Script 11: Scared movement
- Script 12+: Your custom scripts

## Web Interface

### Layout

The web interface uses a responsive grid layout optimized for landscape tablets:

- **Header**: Droid name and title
- **Button Grid**: Auto-arranges buttons across available width
- **Dark Theme**: Reduces eye strain
- **Large Touch Targets**: Easy interaction

### Customization

Change button color by modifying:
```cpp
String droidcolor = "green";  // Any CSS color name or hex code
```

## Troubleshooting

### WiFi Issues

**Problem**: Can't see WiFi network
- Check serial monitor for "Access Point started!" message
- Verify droidname is set correctly
- Ensure ESP32 has adequate power supply

**Problem**: Can't connect to WiFi
- Verify password is 8-63 characters
- Check that password matches `ap_password` variable

### LED Issues

**Problem**: LEDs don't light up
- Check LED_PIN matches wiring (default: GPIO 5)
- Verify LED type (WS2812B vs other types)
- Ensure adequate 5V power supply
- Check LED data line connection

**Problem**: Wrong colors
- Verify COLOR_ORDER setting (GRB vs RGB vs BRG)
- Adjust in LED configuration section

### Maestro Issues

**Problem**: Servos don't move
- Check that `MAESTRO_ENABLED` is set to `true`
- Check serial connections match configuration (default: GPIO 16 RX, GPIO 17 TX)
- Verify pin assignments in `MAESTRO_RX_PIN` and `MAESTRO_TX_PIN` definitions
- Verify baud rate matches (default: 9600)
- Ensure Maestro is powered separately
- Check script numbers match programmed sequences
- Check serial monitor for "Maestro serial initialized" message
- If you see "Maestro script requested but Maestro not available", the board is disabled

**Problem**: Want to run without Maestro board
- Set `MAESTRO_ENABLED false` at the top of the sketch
- System will operate normally with LEDs and sounds
- Serial monitor will show "Maestro disabled in configuration"

### DFPlayer Issues

**Problem**: No sound
- Check serial monitor for initialization errors
- Verify serial connections match configuration (default: GPIO 9 RX, GPIO 10 TX)
- Check pin assignments in `DFPLAYER_RX_PIN` and `DFPLAYER_TX_PIN` definitions
- Verify SD card is inserted and formatted (FAT32)
- Check speaker connections
- Adjust volume: `dfPlayer.volume(25);`
- Remember: 1kΩ resistor required on DFPlayer RX line

**Problem**: Wrong track plays
- Verify file naming: `0001.mp3`, `0002.mp3`, etc.
- Check SD card folder structure
- Ensure track numbers match emote definitions

## Serial Monitor Output

Monitor debugging info at 115200 baud:

### With All Components Connected

```
Maestro serial initialized
Initializing DFPlayer...
DFPlayer initialized successfully
Configuring Access Point...
Access Point started!
SSID: Grek
Password: k7Rm9pQx2w
AP IP address: 192.168.4.1
Connect to this network and navigate to http://192.168.4.1

Setting emote: happy
Eyes Green
Playing MP3 track 2
Activating maestro sequence 2
```

### With Maestro Disabled

```
Maestro disabled in configuration
Initializing DFPlayer...
DFPlayer initialized successfully
Configuring Access Point...
Access Point started!
SSID: Grek
Password: k7Rm9pQx2w
AP IP address: 192.168.4.1
Connect to this network and navigate to http://192.168.4.1

Setting emote: happy
Eyes Green
Playing MP3 track 2
Maestro script requested but Maestro not available
```

### With DFPlayer Missing

```
Maestro serial initialized
Initializing DFPlayer...
DFPlayer initialization failed!
Check connections and SD card
Configuring Access Point...
Access Point started!
SSID: Grek
Password: k7Rm9pQx2w
AP IP address: 192.168.4.1
Connect to this network and navigate to http://192.168.4.1

Setting emote: happy
Eyes Green
MP3 requested but DFPlayer not available
Activating maestro sequence 2
```

## Power System

### Recommended Power Setup

This system is designed to run from an **18V battery** with buck converters:

```
18V Battery
    │
    ├─── Buck Converter (18V → 5V, 2A+) ──→ ESP32, LEDs, DFPlayer
    │
    └─── Buck Converter (18V → 8V, servo current) ──→ Maestro Servo Controller
```

### Power Requirements

**5V Rail (from first buck converter):**
- **ESP32**: ~500mA (WiFi active)
- **LED Strip**: ~60mA per LED at full brightness (3 LEDs = 180mA max)
- **DFPlayer**: ~50mA
- **Total 5V**: ~750mA (recommend 2A+ converter for headroom)

**8V Rail (from second buck converter):**
- **Servos**: Varies by servo type and quantity
- Calculate based on your specific servos (stall current × number of servos)
- Typical hobby servo: 500mA-2A under load
- Size buck converter accordingly

### Buck Converter Specifications

**For 5V Rail:**
- Input: 18V
- Output: 5V regulated
- Current: Minimum 2A (3A recommended)
- Must handle ~750mA continuous + spikes

**For 8V Rail:**
- Input: 18V  
- Output: 8V regulated
- Current: Based on servo requirements (typically 3A-5A minimum)
- Must handle servo stall current

### Important Notes

- **Common Ground**: All grounds (battery, both buck converters, ESP32, Maestro) must be connected together
- **Testing**: For initial testing, USB power to ESP32 is acceptable (LEDs only, no servos)
- **Production**: Always use battery + buck converter system for mobile operation
- **Fusing**: Add appropriate fuses on both buck converter outputs for safety

## License

This project uses the following open-source libraries:
- FastLED (MIT License)
- PololuMaestro (MIT License)
- DFRobotDFPlayerMini (MIT License)

## Credits

- FastLED Library: Daniel Garcia
- PololuMaestro Library: Pololu Corporation
- DFPlayer Mini Library: DFRobot
- WiFi Web Server Example: Rui Santos (randomnerdtutorials.com)

## Support

For issues or questions:
1. Check Serial Monitor output (115200 baud)
2. Verify wiring against pin assignments
3. Confirm all libraries are installed
4. Check power supply is adequate

## Version History

- **v1.0**: Initial release with full feature set
  - ESP32 Access Point mode
  - FastLED eye control with 3 LEDs
  - Pololu Maestro integration
  - DFPlayer Mini MP3 support
  - Landscape tablet web interface
  - 14 pre-configured emotes
  - Flashlight utility mode

