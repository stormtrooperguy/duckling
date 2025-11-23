# ESP32 Droid Control System

A web-based control system for animatronic droids using ESP32, featuring LED eye control, servo movements via Pololu Maestro, and optional MP3 sound effects via DFPlayer Mini.

## Features

- **WiFi Access Point**: ESP32 creates its own WiFi network for direct device control
- **Web Interface**: Responsive landscape-optimized interface for 8" tablets with fast response times
- **LED Eye Control**: 3 addressable LEDs (WS2812B/NeoPixel) for expressive eyes
- **Servo Control**: Optional integration with Pololu Maestro for complex servo sequences
- **Sound Effects**: Optional DFPlayer Mini for MP3 audio playback
- **Modular Design**: Run with any combination of components (LEDs only, LEDs + Servos, full system)
- **Emotes System**: Pre-configured emotional states with coordinated eye colors and servo movements
- **Actions System**: Utility functions (like flashlight toggle) that preserve eye state
- **Eye Colors System**: Quick eye color changes without servo movements (6 colors available)
- **Performance Optimized**: Fast HTTP response (~200-400ms) with optional debug mode for development

## Hardware Requirements

### Core Components
- **ESP32 Development Board**
- **3x WS2812B/NeoPixel LEDs** (addressable RGB LEDs)
- **Pololu Maestro Servo Controller** (Mini Maestro) - optional, can be disabled
- **DFPlayer Mini MP3 Module** - optional, auto-detected
- **MicroSD Card** (for DFPlayer, if using audio)
- **Power Supply** 18V tool battery expected; 5V input adequate to power lights and sound, but not servos

**Note**: Both the Maestro and DFPlayer are optional, though without the Maestro it's not a very exciting project. The system will operate with LEDs and web interface even if these modules are not connected.

### Recommended
- 8" Tablet (landscape orientation) for web interface; optimized for this size
- Smaller or larger screens will work but may require CSS adjustments
- Speaker (for DFPlayer audio output)

## Wiring Connections

**📋 See [WIRING_DIAGRAM.txt](WIRING_DIAGRAM.txt) for detailed ASCII art wiring diagrams and step-by-step connection guide.**

### ESP32 Pin Assignments

| Component | ESP32 Pin | Serial Port | Notes |
|-----------|-----------|-------------|-------|
| **LED Strip Data** | GPIO 5 | - | WS2812B data line |
| **Maestro RX** | GPIO 16 | Serial1 | Receives from Maestro TX |
| **Maestro TX** | GPIO 17 | Serial1 | Transmits to Maestro RX |
| **DFPlayer RX** | GPIO 25 | Serial2 | Receives from DFPlayer TX |
| **DFPlayer TX** | GPIO 26 | Serial2 | Transmits to DFPlayer RX |

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
ESP32 GPIO 26 (TX) → DFPlayer RX (through 1kΩ resistor)
ESP32 GPIO 25 (RX) → DFPlayer TX
ESP32 GND          → DFPlayer GND
5V                 → DFPlayer VCC
DFPlayer SPK+      → Speaker +
DFPlayer SPK-      → Speaker -
```

**Note**: 
- DFPlayer RX typically requires a 1kΩ resistor between ESP32 TX and DFPlayer RX pin.
- GPIO 25 and 26 are safe general-purpose pins that won't conflict with flash memory.

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
// Debug Configuration
#define DEBUG_MODE false  // Set to true for verbose serial debugging

// Droid Configuration
String droidname = "YourDroidName";  // WiFi SSID name - CHANGE THIS!
String droidcolor = "green";         // Button color (CSS color name)
const char* ap_password = "CHANGE_ME_12345";  // WiFi password (8-63 chars) - CHANGE THIS!

// LED Configuration
#define NUM_LEDS 3        // Number of LEDs
#define LED_PIN 5         // Data pin for LED strip
#define LED_TYPE WS2812B  // LED chip type
```

**⚠️ SECURITY NOTE**: Before deploying your code:
1. Change `droidname` to your desired WiFi network name
2. Change `ap_password` to a strong, unique password (8-63 characters)
3. Never commit real passwords to source control
4. Use different passwords for different droids

### Debug Mode

The system includes a debug flag for controlling serial output verbosity:

```cpp
#define DEBUG_MODE false  // Production: false, Development: true
```

**DEBUG_MODE = false (Production - Default)**
- Maximum performance (~200-400ms response time)
- Minimal serial output (initialization and errors only)
- Recommended for normal operation

**DEBUG_MODE = true (Development)**
- Verbose logging of all operations
- Shows every client connection, emote trigger, and device action
- Slower performance (~700-900ms response time due to serial overhead)
- Use for troubleshooting and development

**What gets logged:**

| Message Type | DEBUG_MODE = false | DEBUG_MODE = true |
|--------------|-------------------|-------------------|
| Startup info (WiFi, IP, etc.) | ✅ Always shown | ✅ Always shown |
| Error messages | ✅ Always shown | ✅ Always shown |
| Client connections | ❌ Hidden | ✅ Shown |
| Emote triggers | ❌ Hidden | ✅ Shown |
| LED changes | ❌ Hidden | ✅ Shown |
| MP3 playback | ❌ Hidden | ✅ Shown |
| Maestro commands | ❌ Hidden | ✅ Shown |

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
#define DFPLAYER_RX_PIN 25        // ESP32 RX (connects to DFPlayer TX)
#define DFPLAYER_TX_PIN 26        // ESP32 TX (connects to DFPlayer RX)
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
- **SSID**: Uses `droidname` variable (must be customized)
- **Password**: Uses `ap_password` variable (must be customized)

**Remember**: Always change the default SSID and password before first use!

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
4. **Eyes automatically turn white on startup** (wake up state)

### Connecting

1. **Connect to WiFi**:
   - SSID: Your configured droidname (e.g., "R2D2")
   - Password: Your configured password

2. **Open Web Interface**:
   - Navigate to: `http://192.168.4.1`
   - Interface optimized for 8" landscape tablets

### Available Emotes

Emotes change eye colors and trigger servo sequences:

| Emote | Eye Color | Maestro Script | Description |
|-------|-----------|----------------|-------------|
| **angry** | Red | 0 | Angry expression |
| **curious** | Yellow | 1 | Curious expression |
| **happy** | Green | 2 | Happy expression |
| **sad** | Blue | 3 | Sad expression |
| **go to sleep** | Off | 4 | All LEDs off |
| **wake up** | White | 5 | Eyes turn white |
| **yes** | Green | 6 | Affirmative response |
| **no** | Red | 7 | Negative response |
| **scared** | Purple | 8 | Scared expression |

### Available Actions

Actions provide utility functions without changing eye colors:

| Action | Eye Color | LED 3 | Description |
|--------|-----------|-------|-------------|
| **flashlight** | Preserved | Toggle | Toggle LED 3 flashlight on/off |

**Note**: "Preserved" means the action maintains the current eye color (LEDs 1 & 2) while only affecting LED 3.

### Available Eye Colors

Eye colors change only the eye LEDs without triggering servos (preserves flashlight state):

| Color | Eye Color | LED 3 | Servo | Description |
|-------|-----------|-------|-------|-------------|
| **white** | White | Preserved | No | Set eyes to white |
| **yellow** | Yellow | Preserved | No | Set eyes to yellow |
| **green** | Green | Preserved | No | Set eyes to green |
| **red** | Red | Preserved | No | Set eyes to red |
| **blue** | Blue | Preserved | No | Set eyes to blue |
| **purple** | Purple | Preserved | No | Set eyes to purple |

**Note**: Eye color buttons preserve the flashlight state (LED 3) while changing eye colors.

## Adding New Emotes, Actions, or Eye Colors

### Emotes vs Actions vs Eye Colors

- **Emotes**: Change eye colors (LEDs 1 & 2) and typically trigger servo sequences
- **Actions**: Utility functions that preserve eye color and usually only affect LED 3
- **Eye Colors**: Change eye colors without servos, preserving LED 3 (flashlight) state

### Step 1: Add to Appropriate Array

For a new emote (changes eye color + servos), add to `emotes[]`:

```cpp
const Button emotes[] = {
  // ... existing emotes ...
  {"newemote", "New Emote", "Orange", CRGB::Orange, CRGB::Black, false, false, 12, 5},
  //    |           |           |          |            |         |      |      |   |
  //    |           |           |          |            |         |      |      |   MP3 track (or -1)
  //    |           |           |          |            |         |      |      Maestro script (or -1)
  //    |           |           |          |            |         |      Preserve LED 3? (false)
  //    |           |           |          |            |         Preserve LED 1&2? (false for emotes)
  //    |           |           |          |            LED 3 color
  //    |           |           |          LED 1&2 color
  //    |           |           Color name for serial output
  //    |           Button label
  //    URL path
};
```

For a new action (preserves eye color), add to `actions[]`:

```cpp
const Button actions[] = {
  // ... existing actions ...
  {"myaction", "My Action", "Action", CRGB::Black, CRGB::Purple, true, false, -1, 7},
  //                                                              |     |      |    |
  //                                                              |     |      |    MP3 track
  //                                                              |     |      No servo script
  //                                                              |     Preserve LED 3? (false)
  //                                                              Preserve eyes: true
};
```

**Note**: The flashlight action has special toggle logic in the code. For simple actions, use `triggerButton()`. For custom toggle behavior, add special handling in the action request loop (see flashlight implementation).

For a new eye color (changes eyes only), add to `eyeColors[]`:

```cpp
const Button eyeColors[] = {
  // ... existing colors ...
  {"color_orange", "orange", "Orange", CRGB::Orange, CRGB::Black, false, true, -1, -1},
  //                                                               |      |     |    |
  //                                                               |      |     |    No MP3
  //                                                               |      |     No servo
  //                                                               |      Preserve LED 3: true!
  //                                                               Change eyes: false
};
```

### Step 2: Parameters Explained

- **path**: URL endpoint (alphanumeric, no spaces)
- **label**: Button text shown on web interface
- **colorName**: Descriptive name for serial debugging
- **color**: LED 1&2 color (ignored if preserveLED12 is true)
- **led3Color**: LED 3 color (ignored if preserveLED3 is true)
- **preserveLED12**: `false` for emotes/eye colors (changes eyes), `true` for actions (preserves eyes)
- **preserveLED3**: `true` for eye colors (preserve flashlight), `false` for emotes/actions
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

The web interface uses a responsive grid layout optimized for 8" landscape tablets:

- **Header**: Droid name and title (24px ultra-compact)
- **Emotes Section**: Emotional expressions with eye color changes and servo movements
- **Actions Section**: Utility functions that preserve eye state
- **Eye Colors Section**: Quick eye color changes without servo movements
- **Status Console**: Real-time feedback at bottom of screen (9px font)
- **Button Grid**: Auto-arranges buttons across available width (112px minimum)
- **Dark Theme**: Reduces eye strain
- **Touch-Friendly Buttons**: Compact sizing optimized for 8" tablet screens

### Customization

**Button Color:**
```cpp
String droidcolor = "green";  // Any CSS color name or hex code
```

**UI Sizing (for different screen sizes):**

The interface is optimized for 8" tablets with compact sizing. To adjust for larger/smaller screens, modify these CSS values in the code:

| Element | Current (8" tablet) | Larger Screens | Smaller Screens |
|---------|---------------------|----------------|-----------------|
| h1 font-size | 24px | 32px | 18px |
| h2 font-size | 17px | 22px | 14px |
| Button font-size | 15px | 20px | 12px |
| Button padding | 15px 11px | 20px 15px | 12px 8px |
| Button min-width | 112px | 150px | 90px |
| Gap spacing | 8px | 10px | 6px |
| Status font-size | 9px | 12px | 8px |

### Performance Tips

For optimal web interface responsiveness:

1. **Disable Debug Mode** in production:
   ```cpp
   #define DEBUG_MODE false  // ~500ms faster per request
   ```

2. **Keep WiFi signal strong**: Position ESP32 for good signal to tablet

3. **Minimize concurrent connections**: One control device at a time for best performance

4. **Expected response times**:
   - With DEBUG_MODE = false: ~200-400ms (fast, responsive)
   - With DEBUG_MODE = true: ~700-900ms (slower, diagnostic)

## Troubleshooting

### General Debugging

**Enable Debug Mode for detailed diagnostics:**

1. Set `DEBUG_MODE true` in the sketch
2. Upload the modified code
3. Open Serial Monitor (115200 baud)
4. Observe detailed logging of all operations
5. Set back to `DEBUG_MODE false` when done

Debug mode shows:
- Every client connection and disconnection
- Each emote trigger with full details
- LED color changes
- MP3 track playback attempts
- Maestro servo commands
- Eye synchronization warnings (if LEDs 1 & 2 become mismatched)

**Note**: Debug mode adds ~500ms overhead per operation due to serial output. Use only for troubleshooting.

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

**Problem**: Eyes (LEDs 1 & 2) showing different colors
- This is automatically detected and corrected by the code
- If DEBUG_MODE is enabled, you'll see "WARNING: Eye LEDs out of sync!"
- The system uses interrupt protection to prevent race conditions
- Both eyes are automatically synchronized before each display update
- If this happens frequently, check for loose wiring or power supply issues

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

**Problem**: ESP32 won't boot / constant reboot loop
- **CRITICAL**: GPIO 9 and 10 are connected to flash memory on most ESP32 boards
- Using these pins will prevent the ESP32 from booting
- The code now uses GPIO 25 (RX) and GPIO 26 (TX) which are safe
- If you modified the pins, avoid GPIO 6, 7, 8, 9, 10, 11 (flash pins)

**Problem**: No sound
- Check serial monitor for initialization errors
- Verify serial connections match configuration (default: GPIO 25 RX, GPIO 26 TX)
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

### Startup (Always Shown)

```
Maestro serial initialized
Initializing DFPlayer...
DFPlayer initialized successfully
Eyes initialized to white
Configuring Access Point...
Access Point started!
SSID: YourDroidName
Password: YourPassword
AP IP address: 192.168.4.1
Connect to this network and navigate to http://192.168.4.1
```

### During Operation (DEBUG_MODE = false - Production)

```
(No output during normal operation)
(Only errors will be shown)
```

### During Operation (DEBUG_MODE = true - Development)

```
New Client.
Setting emote: happy
Eyes Green
Playing MP3 track 2
Activating maestro sequence 2
Client disconnected.

New Client.
Setting emote: angry
Eyes Red
Playing MP3 track 4
Activating maestro sequence 4
Client disconnected.
```

### Error Messages (Always Shown Regardless of DEBUG_MODE)

**When DFPlayer missing:**
```
Initializing DFPlayer...
DFPlayer initialization failed!
Check connections and SD card
...
MP3 requested but DFPlayer not available
```

**When Maestro disabled:**
```
Maestro disabled in configuration
...
Maestro script requested but Maestro not available
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
  - Ultra-compact web interface optimized for 8" landscape tablets
  - 9 pre-configured emotes (emotional expressions)
  - 1 utility action (flashlight toggle)
  - 6 eye color options (quick color changes without servo movements)
  - Automatic white eye startup

