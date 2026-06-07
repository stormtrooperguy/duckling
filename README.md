# ESP32 Droid Control System

A web-based control system for animatronic droids using ESP32, featuring LED eye control, servo movements via Pololu Maestro, and optional MP3 sound effects via DIYables Mini MP3 Player.

## Features

- **WiFi Access Point**: ESP32 creates its own WiFi network for direct device control
- **Web Interface**: Responsive landscape-optimized interface for 8" tablets with fast response times
- **LED Eye Control**: 3 addressable LEDs in the head (WS2812B/NeoPixel) with per-LED brightness (eyes dimmed for comfort, flashlight at max), plus an inline signal-repeater pixel near the controller
- **Servo Control**: Optional integration with Pololu Maestro for complex servo sequences
- **Sound Effects**: Optional DIYables Mini MP3 Player for MP3 audio playback
- **Modular Design**: Run with any combination of components (LEDs only, LEDs + Servos, full system)
- **Emotes System**: Pre-configured emotional states that trigger servo movements (and optionally sound). Eye colors are controlled separately via the Eye Colors buttons — emotes don't touch the eyes.
- **Idle Mode**: Autonomous mode that cycles through emotes in random order with natural timing delays, making the droid look active when not being puppeteered
- **Toggle Switches**: Flashlight and Idle Mode rendered as CSS slide toggles in the web UI, reflecting current state at a glance
- **Eye Colors System**: Quick eye color changes without servo movements (7 colors available)
- **Performance Optimized**: Fast HTTP response (~200-400ms) with optional debug mode for development

## Hardware Requirements

### Core Components
- **ESP32 Development Board**
- **4x WS2812B/NeoPixel LEDs** (addressable RGB LEDs): 3 in the head (2 eyes + 1 flashlight) plus 1 inline repeater pixel placed near the controller end of the long data run, used to clean up the data signal before it reaches the head. The repeater pixel is held off in firmware and isn't visible in normal operation.
- **Pololu Maestro Servo Controller** (Mini Maestro) - optional, can be disabled
- **DIYables Mini MP3 Player** - optional, auto-detected
- **MicroSD Card** (for MP3 player, if using audio)
- **External audio amplifier** (if using audio) — the YX5200-24SS has a small built-in amp on its SPK_1/SPK_2 pins, but for usable volume into anything beyond a tiny piezo speaker you'll want a dedicated amp on the 3.5mm AUX output. This build uses a **DROK 5W+5W Mini Audio Amplifier Board** (PAM8403-class), powered from the 5V rail.
- **Speakers** (if using audio) — pair of **2.5W, 36mm full-range drivers**, one per amplifier channel. Anything in the 2–5W / 4–8Ω range will work; match the amp's per-channel output.
- **Power Supply** 18V tool battery expected; 5V input adequate to power lights and sound, but not servos

**Note**: Both the Maestro and MP3 player are optional, though without the Maestro it's not a very exciting project. The system will operate with LEDs and web interface even if these modules are not connected.

### Recommended
- 8" Tablet (landscape orientation) for web interface; optimized for this size
- Smaller or larger screens will work but may require CSS adjustments

## Wiring Connections

**📋 See [WIRING_DIAGRAM.txt](WIRING_DIAGRAM.txt) for detailed ASCII art wiring diagrams and step-by-step connection guide.**

### ESP32 Pin Assignments

| Component | ESP32 Pin | Serial Port | Notes |
|-----------|-----------|-------------|-------|
| **LED Strip Data** | GPIO 5 | - | WS2812B data line |
| **Maestro RX** | GPIO 16 | Serial1 | Receives from Maestro TX |
| **Maestro TX** | GPIO 17 | Serial1 | Transmits to Maestro RX |
| **MP3 player RX** | GPIO 18 | Serial2 | Receives from MP3 player TX |
| **MP3 player TX** | GPIO 19 | Serial2 | Transmits to MP3 player RX |

**Note**: All pin assignments are easily configurable at the top of the sketch. See [Serial Port Configuration](#serial-port-configuration) below.

### LED Strip (WS2812B)
```
ESP32 GPIO 5 → Repeater LED Data In → Eye 1 → Eye 2 → Flashlight
ESP32 GND     → LED GND (common ground with 5V supply)
5V Power      → LED VCC
```

The chain order matters: the first pixel in the chain is the **inline repeater** placed near the controller. It exists purely to re-drive the data signal before the long run to the head, and is held off in firmware. The remaining three pixels are in the head, in order: Eye 1, Eye 2, Flashlight.

### Pololu Maestro (Serial1)
```
ESP32 GPIO 17 (TX) → Maestro RX
ESP32 GPIO 16 (RX) → Maestro TX
ESP32 GND          → Maestro GND
```

### DIYables Mini MP3 Player (Serial2)
```
Control / power:
  ESP32 GPIO 19 (TX)  → MP3 player RX
  ESP32 GPIO 18 (RX)  → MP3 player TX
  ESP32 GND           → MP3 player GND
  5V rail             → MP3 player VCC

Audio chain (this build uses the AUX out, not the direct speaker pins):
  MP3 player 3.5mm AUX out  → DROK amplifier AUX in     (3.5mm cable)
  5V rail                    → DROK amplifier VCC
  GND                        → DROK amplifier GND
  DROK amplifier L+ / L−    → Left speaker  (2.5W, 36mm)
  DROK amplifier R+ / R−    → Right speaker (2.5W, 36mm)

  (MP3 player SPK_1 / SPK_2 unused in this build — direct-drive would
   work for very small / efficient speakers but not for these.)
```

**Note**:
- No series resistor is required on the MP3 player's RX line — the YX5200-24SS accepts ESP32's 3.3V logic directly. (This is the main wiring difference from a DFPlayer Mini.)
- GPIO 18 and 19 are safe general-purpose pins that won't conflict with flash memory. (They're the chip's default VSPI CLK/MISO pins, but VSPI is never explicitly initialized in this firmware, so the pins are free for UART use.)
- The amplifier and MP3 player share the 5V rail. Class-D amps switch hard and can inject supply noise; if you hear hiss/whine through the speakers, add a small bulk cap (100–470µF) right at the amp's VCC pin.

## Software Requirements

### PlatformIO

This project uses [PlatformIO](https://platformio.org/) for building, library management, and flashing. The project layout follows the standard PlatformIO convention: source in `src/`, dependencies pinned in `platformio.ini`.

**Install PlatformIO Core** (CLI) — see [docs](https://docs.platformio.org/en/latest/core/installation/index.html). On macOS the easiest path is:

```bash
brew install platformio
```

Or install the [PlatformIO IDE extension](https://platformio.org/install/ide) for VS Code, which bundles everything.

**Build, upload, and monitor:**

```bash
pio run                  # compile only
pio run --target upload  # compile + flash to the board
pio device monitor       # open serial monitor (115200 baud)
```

The first `pio run` will download all dependencies into `.pio/` (gitignored). Dependencies and their pinned versions live in `platformio.ini`:

- `FastLED` — LED control (^3.7.0)
- `PololuMaestro` — servo controller (^1.0.0)
- `DIYables_MiniMp3` — MP3 player (^1.0.0)

Adjust `upload_port` / `monitor_port` in `platformio.ini` if your board enumerates as a different serial device.

## Configuration

### Basic Settings

Edit these variables at the top of `src/main.cpp`:

```cpp
// Debug Configuration
#define DEBUG_MODE false  // Set to true for verbose serial debugging

// Droid Configuration
String droidname = "YourDroidName";  // WiFi SSID name - CHANGE THIS!
String droidcolor = "green";         // Button color (CSS color name)
const char* ap_password = "CHANGE_ME_12345";  // WiFi password (8-63 chars) - CHANGE THIS!

// LED Configuration
#define NUM_LEDS 4        // 1 inline repeater + 3 in head (2 eyes + flashlight)
#define REPEATER_LED   0  // Inline signal-repeater pixel (always off)
#define EYE_LED_1      1  // Eye 1
#define EYE_LED_2      2  // Eye 2 (always kept in sync with EYE_LED_1)
#define FLASHLIGHT_LED 3  // Flashlight
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

// DIYables Mini MP3 Player
#define MP3_SERIAL_NUM 2          // Use Serial2
#define MP3_RX_PIN 18             // ESP32 RX (connects to MP3 player TX)
#define MP3_TX_PIN 19             // ESP32 TX (connects to MP3 player RX)
#define MP3_BAUD 9600
```

**To disable Maestro**: Set `MAESTRO_ENABLED false` if the Maestro board is not connected. The system will operate normally with LEDs and sound, skipping servo commands.

**To change pins**: Simply modify the `_RX_PIN` and `_TX_PIN` values.

**To use different serial ports**: Change `MAESTRO_SERIAL_NUM` or `MP3_SERIAL_NUM` (ESP32 supports Serial1, Serial2).

**To change baud rates**: Modify the `_BAUD` definitions (both devices default to 9600).

### Network Settings

- **IP Address**: 192.168.4.1 (fixed)
- **Network**: 192.168.4.0/24
- **Gateway**: 192.168.4.1
- **SSID**: Uses `droidname` variable (must be customized)
- **Password**: Uses `ap_password` variable (must be customized)

**Remember**: Always change the default SSID and password before first use!

### MP3 Player Volume

Adjust volume (0-30) in `setup()`:
```cpp
mp3Player.setVolume(20);  // Default: 20
```

### FastLED Brightness

The system uses **per-LED brightness control** for optimal viewing:

- **Eyes (LEDs 1 & 2)**: Brightness **50** (comfortable for viewing)
- **Flashlight (LED 3)**: Brightness **255** (maximum brightness)

To adjust these values, modify the constants at the top of the sketch:
```cpp
#define EYE_BRIGHTNESS 50        // Brightness for eyes (0-255)
#define FLASHLIGHT_BRIGHTNESS 255 // Brightness for flashlight (0-255)
```

The brightness is applied automatically through scaling functions:
- `scaleEyeColor()` - Dims eye colors to comfortable levels
- `scaleFlashlightColor()` - Keeps flashlight at maximum brightness

## Usage

### Initial Setup

1. Flash the firmware: `pio run --target upload`
2. Open the serial monitor: `pio device monitor` (115200 baud)
3. ESP32 will create a WiFi access point
4. **Eyes automatically turn orange on startup**

### Connecting

1. **Connect to WiFi**:
   - SSID: Your configured droidname (e.g., "R2D2")
   - Password: Your configured password

2. **Open Web Interface**:
   - Navigate to: `http://192.168.4.1`
   - Interface optimized for 8" landscape tablets

### Available Emotes

Emotes trigger a Maestro servo sequence. While the servo script runs, the firmware fills the soundstage with **random short audio clips** drawn from the SD card library (see [MP3 Player Setup](#mp3-player-setup) for how the library is structured). Tracks that are still playing when the servo script ends are allowed to finish naturally; once the player goes idle after the sequence ends, the droid stays silent until the next emote fires. Emotes do **not** touch the eyes — to change eye color, use the **Eye Colors** buttons below.

| Emote | Maestro Script | Audio | Description |
|-------|----------------|-------|-------------|
| **angry** | 0 | random | Angry expression |
| **curious** | 1 | random | Curious expression |
| **dance** | 2 | random | Dance sequence |
| **happy** | 3 | random | Happy expression |
| **no** | 4 | random | Negative response |
| **sad** | 5 | random | Sad expression |
| **scared** | 6 | random | Scared expression |
| **go to sleep** | 7 | **silent** | Sleep position (no audio — `silent = true`) |
| **wake up** | 8 | random | Wake animation |
| **yes** | 9 | random | Affirmative response |

**Decoupled from eye color**: Emotes are pure servo+sound triggers. Whatever eye color was active when an emote fires remains active throughout and after the sequence — the eyes are entirely under the operator's control via the Eye Colors buttons.

### Toggles

Modes that are either on or off are rendered as slide-style switches in the web UI rather than push buttons. The current state is shown by the position of the knob and the colour of the pill.

| Toggle | Endpoint(s) | Effect |
|--------|-------------|--------|
| **Flashlight** | `GET /maestro/flashlight` (one endpoint toggles) | Turns LED 3 on/off; preserves eye colour |
| **Idle Mode** | `GET /maestro/idle_start` / `GET /maestro/idle_stop` (JS picks based on current state) | Starts/stops autonomous emote cycling |

The toggles fan in from the same JSON status that powers the rest of the web UI, so external automation can read `flashlight` and `idle` from `/events` to know the current state.

### Idle Mode

Idle mode makes the droid appear active when no one is puppeteering it. When enabled:

- The droid automatically fires emotes in randomized order, cycling through all 10 emotes before repeating
- Delays between emotes are randomized (6–15 seconds from the start of one emote to the start of the next), producing natural-looking timing
- Eyes are not touched during idle — whatever color the operator last set remains throughout
- Idle mode stops automatically if any emote button or eye color button is pressed
- Flip the **Idle Mode** toggle off to stop manually

Idle mode timing is configured with two constants at the top of the sketch:

```cpp
#define IDLE_MIN_DELAY_MS 6000   // Minimum ms from emote trigger to next emote
#define IDLE_MAX_DELAY_MS 15000  // Maximum ms from emote trigger to next emote
```

### Available Eye Colors

Eye colors change only the eye LEDs without triggering servos (preserves flashlight state):

| Color | Eye Color | LED 3 | Servo | Description |
|-------|-----------|-------|-------|-------------|
| **white** | White | Preserved | No | Set eyes to white |
| **yellow** | Yellow | Preserved | No | Set eyes to yellow |
| **orange** | Orange | Preserved | No | Set eyes to orange |
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

For a new emote (triggers a servo script, fills with random audio), add to `emotes[]`:

```cpp
const Button emotes[] = {
  // ... existing emotes ...
  // path     label        emoji   colorName  LED1&2 color  LED3 color   preserve12 preserve3 script# silent
  {"newemote", "New Emote", "✨",   "Cyan",    CRGB::Cyan,   CRGB::Black, true,      false,    10,     false},
  //    |          |         |       |           |             |           |          |        |       |
  //    |          |         |       |           |             |           |          |        |       Suppress random audio? (false for normal emotes, true to stay silent)
  //    |          |         |       |           |             |           |          |        Maestro script number (0+ or -1 for none)
  //    |          |         |       |           |             |           |          Preserve flashlight state? (false on emotes — flashlight goes to led3Color)
  //    |          |         |       |           |             |           Preserve eyes? (true on emotes — eye color is controlled via the Eye Colors buttons)
  //    |          |         |       |           |             LED 3 color (CRGB::Black if no flashlight change)
  //    |          |         |       |           LED 1&2 thematic color — ignored at runtime while preserveLED12 is true; kept as documentation
  //    |          |         |       Color name for debug serial output
  //    |          |         Emoji glyph rendered above the label
  //    |          Button label shown in the UI
  //    URL path (e.g. /maestro/newemote)
};
```

For a new eye color (changes eyes only), add to `eyeColors[]`:

```cpp
const Button eyeColors[] = {
  // ... existing colors ...
  // path           label    emoji  colorName  LED1&2 color  LED3 color   preserve12 preserve3 script# silent
  {"color_cyan",    "cyan",  "",    "Cyan",    CRGB::Cyan,   CRGB::Black, false,     true,     -1,     false},
  //                                                                       |          |         |       |
  //                                                                       |          |         |       Irrelevant (no script → no audio)
  //                                                                       |          |         No Maestro script
  //                                                                       |          Preserve flashlight: true (don't disturb LED 3)
  //                                                                       Change eyes: false → LED 1&2 set to the color above
};
```

**Note**: The flashlight, idle_start, and idle_stop actions have special handling in `dispatchMaestroAction()` rather than going through `triggerButton()`. For simple actions, you can re-use `triggerButton()` with a new struct entry. For custom toggle or mode behavior, add a `strcmp` branch alongside the flashlight implementation.

### Step 2: Parameters Explained

- **path**: URL endpoint (alphanumeric, no spaces)
- **label**: Button text shown on web interface
- **emoji**: Emoji glyph for emote buttons (empty string `""` for eye colors / non-emote buttons)
- **colorName**: Descriptive name for debug serial output
- **color**: LED 1&2 color (ignored if `preserveLED12` is `true`)
- **led3Color**: LED 3 (flashlight) color (ignored if `preserveLED3` is `true`)
- **preserveLED12**: `true` on emotes (don't touch eyes), `false` on eye colors (set eyes to `color`)
- **preserveLED3**: `true` on eye colors (don't touch the flashlight), `false` on emotes
- **scriptNumber**: Maestro script number (0+) or `-1` for none
- **silent**: `true` to suppress the random-during-animation audio for this emote (e.g. the `sleep` emote), `false` for normal chatter

### Available Colors

```cpp
CRGB::Black, CRGB::White, CRGB::Red, CRGB::Green, CRGB::Blue,
CRGB::Yellow, CRGB::Orange, CRGB::Purple, CRGB::Cyan, CRGB::Magenta
```

Or use RGB values: `CRGB(255, 128, 0)` for custom colors.

## MP3 Player Setup

### How audio works in this build

The firmware does **not** assign specific tracks to specific emotes. Instead, while any non-silent emote's Maestro animation is running, the firmware picks a **random track** from the SD card library every time the player goes idle, producing continuous chatter for the duration of the sequence. Tracks that are still playing when the script ends are left to finish naturally; the droid then stays silent until the next emote fires. The library size (currently **250 tracks**) is configured at the top of `src/main.cpp`:

```cpp
#define AUDIO_TRACK_COUNT 250  // Random selection draws from [1, AUDIO_TRACK_COUNT]
```

Both **MP3** and **WAV** files are supported by the YX5200-24SS — the chip plays whatever sits at the requested index regardless of extension. Short clips (1–2 s) work especially well, since the random-refill cycle paints over each clip's end with a fresh one.

### SD Card Preparation

1. **Format the card FAT32.**
2. **Create a folder named `mp3`** at the root.
3. **Copy files in numerical order:** `0001.wav`, `0002.wav`, … (or `.mp3` — mixing is fine). The chip indexes by the **file write order on the card**, not by filename, so if you ever delete and replace files in place you may break the mapping. Safest move: reformat and re-copy in numerical order.
4. **Set `AUDIO_TRACK_COUNT`** in `src/main.cpp` to the number of files you copied.

### File Structure Example

```
SD Card Root
└── mp3/
    ├── 0001.wav   ← random pool: index 1
    ├── 0002.wav   ← random pool: index 2
    ├── 0003.mp3   ← random pool: index 3 (mixed format is fine)
    ├── …
    └── 0250.wav   ← random pool: index AUDIO_TRACK_COUNT
```

### Troubleshooting MP3 Player

- Check serial monitor for initialization messages
- Ensure SD card is formatted as FAT32
- Verify file names follow `000X.wav` / `000X.mp3` format and were copied in numerical order
- Confirm `AUDIO_TRACK_COUNT` matches the number of files on the card
- Try lower volume if sound is distorted

## Maestro Servo Configuration

### Script Numbers

Each emote triggers a Maestro script by number. Program your servo sequences in Maestro Control Center in this order — the firmware's `emotes[]` array is indexed to match:

- Script 0: Angry
- Script 1: Curious
- Script 2: Dance
- Script 3: Happy
- Script 4: No (negative response)
- Script 5: Sad
- Script 6: Scared
- Script 7: Sleep
- Script 8: Wake
- Script 9: Yes (affirmative)

If you re-record scripts in a different order on the Maestro, update the `scriptNumber` field on each emote row in `src/main.cpp` to match.

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

## Web UI Preview (Local Emulator)

You can preview the web interface in your laptop browser without flashing the ESP32 — useful for tweaking layout, CSS, or testing AJAX behavior.

```bash
python3 tools/preview.py
```

Then open `http://localhost:8080`. The emulator serves the same HTML/CSS/JS the firmware sends and provides mock `/events` (Server-Sent Events) and `/maestro/<x>` endpoints, so button clicks and live status pushes behave realistically. It uses only the Python standard library (no `pip install` needed). Note that the emulator does not simulate idle-mode auto-firing or eye-color restore — it only responds to your clicks.

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
- MP3/WAV track playback attempts
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

### MP3 Player Issues

**Problem**: ESP32 won't boot / constant reboot loop
- **CRITICAL**: GPIO 9 and 10 are connected to flash memory on most ESP32 boards
- Using these pins will prevent the ESP32 from booting
- The code now uses GPIO 18 (RX) and GPIO 19 (TX) which are safe
- If you modified the pins, avoid GPIO 6, 7, 8, 9, 10, 11 (flash pins)

**Problem**: No sound
- Check serial monitor for initialization errors
- Verify serial connections match configuration (default: GPIO 18 RX, GPIO 19 TX)
- Check pin assignments in `MP3_RX_PIN` and `MP3_TX_PIN` definitions
- Verify SD card is inserted and formatted (FAT32)
- Check speaker connections
- Adjust volume: `mp3Player.setVolume(25);`
- No series resistor is required on the YX5200-24SS RX line (it accepts 3.3V logic directly)

**Problem**: Wrong track plays
- Verify file naming: `0001.mp3`, `0002.mp3`, etc.
- Check SD card folder structure
- Ensure track numbers match emote definitions

## Serial Monitor Output

Monitor debugging info at 115200 baud:

### Startup (Always Shown)

```
Maestro serial initialized
Initializing MP3 player...
MP3 player initialized successfully
Eyes initialized to orange (brightness 50)
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
Triggering: happy
Activating maestro sequence 3

Triggering: angry
Activating maestro sequence 0
```

(Eye-color log lines only appear when the trigger actually changes eye color — emotes are silent here on that front. Random audio playback isn't logged per-track to keep DEBUG output readable; check `mp3Player.isPlaying()` from a temporary debug print if you need to verify random refill is firing.)

### Error Messages (Always Shown Regardless of DEBUG_MODE)

**When MP3 player missing:**
```
Initializing MP3 player...
MP3 player initialization failed!
Check connections and SD card
...
MP3 requested but MP3 player not available
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
    ├─── Buck Converter (18V → 5V, 2A+) ──→ ESP32, LEDs, MP3 player
    │
    └─── Buck Converter (18V → 8V, servo current) ──→ Maestro Servo Controller
```

### Power Requirements

**5V Rail (from first buck converter):**
- **ESP32**: ~500mA (WiFi active)
- **LED Strip**:
  - Repeater (1 LED held off): ~1mA quiescent
  - Eyes (2 LEDs at brightness 50): ~24mA
  - Flashlight (1 LED at max brightness 255): ~60mA
  - **Total LEDs**: ~85mA typical
- **MP3 player**: ~50mA
- **Audio amplifier** (DROK 5W+5W, PAM8403-class): ~20mA idle, ~100–400mA at typical sound-effect volume, up to ~3A at sustained full output. Most droid sound effects are short and well under peak, so plan for ~300mA typical.
- **Total 5V**: ~950mA typical with audio active (recommend 2A+ converter for headroom; 3A+ if you anticipate driving the amp near full power)

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
- DIYables_MiniMp3 (MIT License)

## Credits

- FastLED Library: Daniel Garcia
- PololuMaestro Library: Pololu Corporation
- DIYables_MiniMp3 Library: DIYables
- WiFi Web Server Example: Rui Santos (randomnerdtutorials.com)

## Support

For issues or questions:
1. Check Serial Monitor output (115200 baud)
2. Verify wiring against pin assignments
3. Confirm all libraries are installed
4. Check power supply is adequate

## Sources

External references consulted during development — useful starting points for hardware datasheets, library docs, and integration notes when extending the system.

### DIYables Mini MP3 Player (v1.14)

- [DIYables Mini MP3 Player Module — product page](https://diyables.io/products/mp3-player-module)
- [DIYables-Mini-Mp3 — GitHub repository](https://github.com/DIYables/DIYables-Mini-Mp3) (library source, examples, API header)
- [PlatformIO Registry — diyables/DIYables_MiniMp3](https://registry.platformio.org/libraries/diyables/DIYables_MiniMp3)
- [ESP32 Tutorial: Mini MP3 Player Module](https://esp32io.com/tutorials/esp32-mini-mp3-player-module) (wiring, library API walk-through)

## Version History

- **v1.16**: Moved MP3 player UART to GPIO 18 (RX) / GPIO 19 (TX)
  - Previously on GPIO 25/26 — both pin pairs are safe general-purpose pins; the move was driven by physical layout convenience on the droid's wiring harness, not by anything wrong with 25/26.
  - GPIO 18/19 are the chip's default VSPI CLK/MISO pins, but VSPI is never explicitly initialized in this firmware, so the pins are free for UART use via the GPIO matrix.
  - **Re-wire required** if you're tracking this branch: move MP3 player TX from ESP32 GPIO 25 → 18, and MP3 player RX from ESP32 GPIO 26 → 19. The 25/26 pins are now free for other uses.

- **v1.15**: Random-during-animation audio model; `silent` flag; WAV support documented; idle-mode latent bug fix
  - **New audio model.** The per-emote `mp3Track` field is gone. While any emote's Maestro animation is running, `loop()` plays a **random track** from the SD card library (1..`AUDIO_TRACK_COUNT`, currently 250) any time the player is idle, producing continuous chatter for the duration of the sequence. Tracks still playing when the animation ends are allowed to finish naturally. Once silent, the system stays silent until the next emote.
  - **`silent` flag** added to the `Button` struct. `sleep` is the only emote with `silent = true` — it runs its servo sequence without random audio. Everything else defaults to chatty.
  - **`AUDIO_TRACK_COUNT = 250`** at the top of `src/main.cpp` — tune this to match how many files are on the SD card. Both WAV and MP3 are now documented as supported; the YX5200-24SS plays whatever sits at the requested index regardless of extension.
  - **`randomSeed(esp_random())`** added at boot. Previously the "random" idle-emote shuffle (and now random audio picks) produced the same sequence after every power-cycle. The hardware RNG fixes that.
  - **Latent idle-mode bug fix.** Since v1.13 (when all emotes flipped to `preserveLED12 = true`), `idleSequenceRunning` was never cleared in the normal Maestro-available path — idle mode would fire one emote and then hang. The new shared Maestro-status polling block clears it on every script end, which also gives us the hook for the new `animationRunning` state.
  - **Loop refactor.** A single rate-limited (~150 ms) Maestro-status polling block now drives audio refill, animation-end detection, eye-color restore (still wired up as an extension point even though no current emote uses it), and idle-mode advancement.

- **v1.14**: Swapped audio hardware: DFPlayer Mini → DIYables Mini MP3 Player; per-emote MP3 tracks wired up
  - Library: `DFRobotDFPlayerMini` → `diyables/DIYables_MiniMp3` (in `platformio.ini`). Same UART protocol underneath (both YX5200-based), but the DIYables library has a cleaner API: `setVolume()` instead of `volume()`, `begin()` accepts a `Stream&` and returns `bool` like before, and bonus methods (`isPlaying()`, EQ, looping) are now available if we ever want them.
  - Identifier rename throughout `src/main.cpp`: `dfPlayer` → `mp3Player`, `dfPlayerSerial` → `mp3PlayerSerial`, `dfPlayerAvailable` → `mp3PlayerAvailable`, `DFPLAYER_*` constants → `MP3_*`. Status JSON key `dfplayer` → `mp3`; web UI label "DFPlayer:" → "MP3:".
  - GPIO pins unchanged: still GPIO 25 (RX) / GPIO 26 (TX) on Serial2. Safe pins, already known-good.
  - **Wiring change:** the 1kΩ series resistor on the player's RX line is no longer required. The YX5200-24SS accepts ESP32's 3.3V logic directly. (Harmless to leave in place if you don't want to rewire.)
  - Per-emote MP3 tracks are now mapped: angry → 1, curious → 2, dance → 3, happy → 4, no → 5, sad → 6, scared → 7, sleep → 8, wake → 9, yes → 10. SD card needs `0001.mp3` … `0010.mp3` in the `/mp3` folder. Eye-color buttons remain silent.

- **v1.13**: Emotes decoupled from eye color; new `dance` emote
  - New emote `dance` 💃 mapped to Maestro script 2.
  - Maestro script numbers reshuffled to match the updated Maestro upload. Full new order: angry (0), curious (1), dance (2), happy (3), no (4), sad (5), scared (6), sleep (7), wake (8), yes (9). Idle mode now cycles 10 emotes (was 9).
  - Emotes no longer change eye color. `preserveLED12` is now `true` on every row in `emotes[]`; eye color is controlled exclusively via the Eye Colors buttons. The themed colors on each emote row (`CRGB::Red` for angry, etc.) remain in the source as documentation of intent and are easy to re-enable by flipping that row's `preserveLED12` back to `false`.
  - The post-emote eye-color restore machinery in `loop()` stays in place but is unreachable under the new defaults — kept as an extension point.

- **v1.12**: Inline signal-repeater pixel
  - Added a 4th pixel at the head of the chain (index 0) that acts as a hardware signal repeater for the long data run between the controller and the head LEDs. Symptoms before: intermittent flicker / wrong colors with no fix from cable changes or a series resistor on the data line. With the repeater inline, the WS2812 reshapes and re-clocks the data stream before it reaches the head pixels.
  - Firmware change: `NUM_LEDS` 3 → 4. The repeater is initialized to `CRGB::Black` in `setup()` and never touched again. All other LED accesses now go through named index constants (`REPEATER_LED`, `EYE_LED_1`, `EYE_LED_2`, `FLASHLIGHT_LED`) rather than bare integers, so the chain order is self-documenting and future shifts won't require hunting down magic indices.

- **v1.11**: Bug fix — eye color now reliably restores after emote sequences (action queue architecture).
  - Latent race condition since the v1.7 async-server migration: `AsyncWebServer` handlers run on a separate FreeRTOS task and were calling Maestro / FastLED operations concurrently with `loop()`. Caused garbled `getScriptStatus()` responses (no restore) and, in some configurations, full crashes due to stack pressure in the async task.
  - Fix: action queue. The async handler enqueues the requested path (`ActionMsg` of fixed size) and returns immediately; `loop()` drains the queue and runs `dispatchMaestroAction()` itself. All Maestro, FastLED, and MP3 player operations are now single-threaded inside `loop()` — no possibility of concurrent hardware access. State updates still flow back to the UI via SSE the moment `loop()` processes the action.

- **v1.10**: Round buttons + warmer orange
  - Emote and eye-color buttons are now circles (`border-radius: 50%` + `aspect-ratio: 1/1`) on a fixed-width grid. Larger, easier touch targets that visually distinguish actions from the rectangular toggles.
  - Orange shifted again — now `CRGB::DarkOrange` (255, 140, 0). Was `CRGB(255, 150, 0)` in v1.9, was too yellow.

- **v1.9**: Operator UX polish
  - Emote buttons now render as large emoji with a small text label below (😠 angry, 🤔 curious, 😊 happy, 👎 no, 😢 sad, 😨 scared, 😴 sleep, 🌅 wake, 👍 yes). HTML response now declares UTF-8 charset.
  - Each eye-color button is painted in its target color; label text auto-flips to dark for light backgrounds (white, yellow) so it stays legible.
  - Orange shifted slightly toward red — was `CRGB::Orange` (255,165,0), now `CRGB(255,150,0)`. Applied to both the startup color and the `color_orange` button.
  - Added `emoji` field to the `Button` struct (empty string for non-emote buttons).

- **v1.8**: Flashlight and Idle Mode are now CSS slide toggles instead of push buttons. The Actions section is replaced with a new Toggles section. Pure-CSS animation, zero extra firmware load — the JS reads current state from the SSE stream and dispatches the correct endpoint on click. `flashlight`/`idle_start`/`idle_stop` HTTP endpoints remain unchanged for any external automation.

- **v1.7**: Replaced synchronous `WiFiServer` with `ESPAsyncWebServer` and swapped status polling for Server-Sent Events. Fixes the long-session hang where the HTTP server would stop responding while the WiFi AP stayed up. Tablets now hold one persistent connection to `/events`; the firmware pushes JSON whenever state changes, eliminating the ~1,800 short-lived TCP connections per hour that the old polling generated. Status updates are now instant rather than trailing by up to 2 seconds. New dependencies: `me-no-dev/AsyncTCP` and `me-no-dev/ESPAsyncWebServer`.

- **v1.6**: Added orange to the eye color palette and made it the new startup default (was yellow). Total eye color options: 7.

- **v1.5**: Migrated to PlatformIO
  - Added `platformio.ini` with pinned library versions
  - Moved sketch to `src/main.cpp` (renamed from `esp32wifiweb.ino`); git history preserved via `git mv`
  - Added `.gitignore` for `.pio/` and `.vscode/`
  - Build/upload/monitor now via `pio` CLI; Arduino IDE workflow deprecated
  - Verified clean compile: RAM 14.2%, Flash 59.9%

- **v1.4**: Active-state button highlighting in the web UI
  - The button matching the current eye state is highlighted (white inset outline + brightness boost). During an emote sequence the emote button glows; after restore, the underlying eye-color button glows again
  - "idle on" highlights when idle mode is engaged
  - "flashlight" highlights when LED 3 is on
  - JSON status gained `currentEye` (path of active button) and `flashlight` (bool) fields
  - Buttons now carry `data-path` attributes for JS lookup
  - Eye state path tracked with `const char*` (no String allocation)

- **v1.3**: AJAX web interface — no more page reloads
  - Button clicks now use `fetch()` instead of full page navigation. Bandwidth per click dropped from ~5 KB to ~80 bytes
  - New endpoints: `GET /` returns the page (sent once on initial load); `GET /status` returns JSON system state; `GET /maestro/<x>` triggers an action and returns the updated JSON status in the same response
  - Status console now updates live: a 2-second poll keeps it in sync without user interaction. Idle mode state, last emote, and component status visible in real time
  - Added "Idle: On/Off" indicator to status console
  - Page is no longer regenerated and re-sent on every interaction; the tablet only renders the DOM once

- **v1.2**: Efficiency pass for long-running stability
  - Maestro `getScriptStatus()` polling rate-limited to every 150 ms (was every loop iteration, flooding the serial bus)
  - Replaced `String header` with a fixed `char[]` buffer; replaced `String currentLine` with a length counter — eliminates heap fragmentation from char-by-char request reads
  - Request path is now parsed once and dispatched via `strcmp` (was 18 String allocations + 18 full-header `indexOf` scans per HTTP request)
  - `createButton` now writes directly to the client (no per-button String allocation)
  - New tunable: `SCRIPT_STATUS_POLL_MS` (default 150) controls Maestro poll cadence

- **v1.1**: Behavioral improvements
  - Yellow eyes on startup (changed from white)
  - Eye color restore after emote sequences — eyes return to prior color when servo script finishes
  - Idle mode — autonomous random emote cycling with natural randomized delays

- **v1.0**: Initial release with full feature set
  - ESP32 Access Point mode
  - FastLED eye control with 3 LEDs
  - Pololu Maestro integration
  - DIYables Mini MP3 Player MP3 support
  - Ultra-compact web interface optimized for 8" landscape tablets
  - 9 pre-configured emotes (emotional expressions)
  - 1 utility action (flashlight toggle)
  - 6 eye color options (quick color changes without servo movements)
  - Automatic white eye startup
  - Per-LED brightness control (eyes at 50 for comfort, flashlight at 255 for maximum brightness)

