/*********
  ESP32 Droid Control System
  
  Originally inspired by work from Rui Santos
  https://randomnerdtutorials.com
  
  Modified extensively by Cursor AI
  Under the supervision of Brian Anderson
  
  Features:
  - WiFi Access Point mode for standalone operation
  - Web interface optimized for landscape tablets
  - FastLED control for addressable RGB LEDs
  - Pololu Maestro servo controller integration
  - DFPlayer Pro audio support (onboard flash) with random-during-animation playback
  - Multiple emotes with coordinated LEDs, sounds, and servos
  - Performance optimized HTTP handling
  - Debug mode for development and troubleshooting
*********/

// Load Wi-Fi library + async HTTP server
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <esp_system.h>  // esp_random() — hardware RNG for randomSeed()

// *** IMPORTANT: Customize these values for your installation ***
// Replace these strings to customize for your droid
String droidname = "Grek";               // Change this! Will be your WiFi SSID
String droidcolor = "green";             // Button color (CSS color name)

// Access Point credentials
// SSID will use droidname above
// *** SECURITY: Change this password before deploying! ***
// Password must be 8-63 characters
const char* ap_password = "changeme";    // Change to a secure password!

// Debug Configuration
#define DEBUG_MODE false  // Set to true to enable verbose serial debugging

// Serial Port Configuration
// Maestro Servo Controller
#define MAESTRO_ENABLED true      // Set to false if Maestro not connected
#define MAESTRO_SERIAL_NUM 1      // Use Serial1
#define MAESTRO_RX_PIN 16         // ESP32 RX (connects to Maestro TX)
#define MAESTRO_TX_PIN 17         // ESP32 TX (connects to Maestro RX)
#define MAESTRO_BAUD 9600

// DFPlayer Pro (DFR0768, DF1201S chip) — UART control at 115200 baud.
// 128MB onboard flash storage replaces the Mini's SD card, eliminating the
// jostle-induced playback crashes we hit in v1.17. Files load via USB-C.
//
// The Pro's UART inputs are 3.3V-tolerant — no 1kΩ resistor needed on the
// RX line, unlike the Mini (this was a v1.17 requirement we can drop). It's
// also less power-rail-sensitive at init; tapping VCC from the buck converter
// directly worked fine in our bring-up. Still safer to share a clean rail.
//
// GPIO 18/19 are the chip's default VSPI CLK/MISO pins; that's only a concern
// if SPI is ever explicitly initialized in this firmware, which it isn't.
#define MP3_SERIAL_NUM 2          // Use Serial2
#define MP3_RX_PIN 18             // ESP32 RX (connects to DFPlayer Pro TX)
#define MP3_TX_PIN 19             // ESP32 TX (connects to DFPlayer Pro RX)
#define MP3_BAUD 115200           // DF1201S native baud (vs Mini's 9600)

// NOTE: GPIO 6-11 are used for SPI flash and will cause boot issues on most
// ESP32 boards. GPIO 18/19 (current MP3 UART) and 25/26 (former MP3 UART) are
// safe general-purpose pins — either pair works for Serial2 via the GPIO matrix.

// Maestro library
#include <PololuMaestro.h>
HardwareSerial maestroSerial(MAESTRO_SERIAL_NUM);
MiniMaestro maestro(maestroSerial);
bool maestroAvailable = MAESTRO_ENABLED;  // Track if Maestro is available

// Action queue: AsyncWebServer handlers enqueue requested paths; loop() drains
// the queue and runs the actual dispatch. Keeps every hardware operation
// (Maestro, LEDs, MP3 player) single-threaded inside loop() so there is no
// concurrent access between the async task and the main task.
#define ACTION_PATH_MAX 32
#define ACTION_QUEUE_DEPTH 8
struct ActionMsg { char path[ACTION_PATH_MAX]; };
QueueHandle_t actionQueue = NULL;

// DFPlayer Pro library (DF1201S)
#include "DFRobot_DF1201S.h"
HardwareSerial mp3PlayerSerial(MP3_SERIAL_NUM);
DFRobot_DF1201S mp3Player;
bool mp3PlayerAvailable = false;    // Track if MP3 player initialized successfully

// Audio library size — number of files copied to the DFPlayer Pro's onboard
// 128MB flash storage via USB-C. Random track selection draws from the
// inclusive range [1, AUDIO_TRACK_COUNT]. We use playFileNum(N) which the
// chip maps to the Nth audio file in physical write order (skipping any
// non-audio entries like macOS metadata folders).
//
// Tested working with WAV files (PCM 16-bit mono 22050 Hz). The Pro is more
// permissive about formats than the YX5200-based Mini was (also supports
// MP3, FLAC, AAC, WMA, APE). See README "MP3 Player Setup" for prep details.
#define AUDIO_TRACK_COUNT 141

// True while a Maestro-driven emote animation is in progress. Set by
// triggerButton() when a non-silent emote with a script is dispatched;
// cleared in loop() when the Maestro reports the script has stopped.
// Drives the "play random audio if nothing is playing" logic.
bool animationRunning = false;

// FastLED library
#include <FastLED.h>

// LED Strip Configuration
// The chain has 4 pixels total: an inline repeater pixel near the controller
// (always off; acts as a signal repeater to clean up the data line before it
// reaches the head), followed by the two eye LEDs and the flashlight LED.
// Always address pixels by these named indices, never by raw integers.
#define NUM_LEDS 4
#define REPEATER_LED   0  // Inline signal-repeater pixel (always off)
#define EYE_LED_1      1  // Eye 1
#define EYE_LED_2      2  // Eye 2 (always kept in sync with EYE_LED_1)
#define FLASHLIGHT_LED 3  // Flashlight
#define LED_PIN 5  // Change this to your desired GPIO pin
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

// LED Brightness settings (0-255)
#define EYE_BRIGHTNESS 100       // Brightness for LEDs 1 & 2 (eyes) - comfortable viewing
#define FLASHLIGHT_BRIGHTNESS 255 // Brightness for LED 3 (flashlight) - maximum

// Idle mode timing
#define IDLE_MIN_DELAY_MS 6000   // Minimum ms from emote trigger to next emote
#define IDLE_MAX_DELAY_MS 15000  // Maximum ms from emote trigger to next emote

// Forward declarations
CRGB scaleEyeColor(CRGB color);
CRGB scaleFlashlightColor(CRGB color);
void shuffleIdleOrder();
void buildStatusJson(String &out);
void pushStatus();
void buildPageHtml(String &out);
void dispatchMaestroAction(const char* path);
void setupWebServer();

// Button definition structure (used for Emotes and Eye Colors)
struct Button {
  const char* path;        // URL path (e.g., "sleep")
  const char* label;       // Button label / status text
  const char* emoji;       // Emoji glyph for emote buttons (empty for eye colors)
  const char* colorName;   // Color name for serial output
  CRGB color;              // LED color for LEDs 1 & 2
  CRGB led3Color;          // LED 3 color (separate control)
  bool preserveLED12;      // If true, don't change LEDs 1 & 2
  bool preserveLED3;       // If true, don't change LED 3
  int scriptNumber;        // Maestro script number (-1 = no script)
  bool silent;             // If true, suppress random audio during this emote's animation
};

// EMOTES: Trigger servo sequences. Eye color is controlled exclusively via
// the Eye Colors buttons; emotes leave the eyes alone (preserveLED12 = true
// on every row). The LED1&2 color values are kept as documentation of each
// emote's "thematic" color and are NOT applied while preserveLED12 stays
// true. Flipping a row's preserveLED12 to false re-enables per-emote eye
// color changes with the documented color.
//
// Audio: while an emote's Maestro script is running, the loop() picks a
// random track from the SD card (1..AUDIO_TRACK_COUNT) any time the player
// is idle, producing continuous chatter for the duration of the animation.
// Tracks that are still playing when the script ends are allowed to finish
// naturally. To suppress this for a specific emote, set silent = true.
//
// Ordered to match Maestro script programming (0-9).
const Button emotes[] = {
  // path        label          emoji  colorName  LED1&2 color       LED3 color    preserve12 preserve3 script# silent
  {"angry",      "angry",       "😠",   "Red",     CRGB::Red,         CRGB::Black,  true,      false,    0,      false},
  {"curious",    "curious",     "🤔",   "Yellow",  CRGB::Yellow,      CRGB::Black,  true,      false,    1,      false},
  {"dance",      "dance",       "💃",   "Pink",    CRGB::HotPink,     CRGB::Black,  true,      false,    2,      false},
  {"happy",      "happy",       "😊",   "Green",   CRGB::Green,       CRGB::Black,  true,      false,    3,      false},
  {"no",         "no",          "👎",   "Orange",  CRGB::DarkOrange,  CRGB::Black,  true,      false,    4,      false},
  {"sad",        "sad",         "😢",   "Blue",    CRGB::Blue,        CRGB::Black,  true,      false,    5,      false},
  {"scared",     "scared",      "😨",   "Purple",  CRGB::Purple,      CRGB::Black,  true,      false,    6,      false},
  {"sleep",      "go to sleep", "😴",   "Off",     CRGB::Black,       CRGB::Black,  true,      false,    7,      true },
  {"wake",       "wake up",     "🌅",   "White",   CRGB::White,       CRGB::Black,  true,      false,    8,      false},
  {"yes",        "yes",         "👍",   "Green",   CRGB::Green,       CRGB::Black,  true,      false,    9,      false}
};
const int numEmotes = sizeof(emotes) / sizeof(emotes[0]);

// ACTIONS: previously held flashlight + idle entries; those are now rendered
// as CSS toggle switches in the web UI and dispatched as special cases below.
// Kept here as an empty extension point for future button-style actions.

// EYE COLORS: Just change eye colors without servo movements (preserves flashlight state)
const Button eyeColors[] = {
  // path           label      emoji  colorName  LED1&2 color      LED3 color    preserve12 preserve3 script# silent
  {"color_white",   "white",   "",    "White",   CRGB::White,      CRGB::Black,  false,     true,     -1,     false},
  {"color_yellow",  "yellow",  "",    "Yellow",  CRGB::Yellow,     CRGB::Black,  false,     true,     -1,     false},
  {"color_orange",  "orange",  "",    "Orange",  CRGB(255,100,0),  CRGB::Black,  false,     true,     -1,     false},
  {"color_green",   "green",   "",    "Green",   CRGB::Green,      CRGB::Black,  false,     true,     -1,     false},
  {"color_red",     "red",     "",    "Red",     CRGB::Red,        CRGB::Black,  false,     true,     -1,     false},
  {"color_blue",    "blue",    "",    "Blue",    CRGB::Blue,       CRGB::Black,  false,     true,     -1,     false},
  {"color_purple",  "purple",  "",    "Purple",  CRGB::Purple,     CRGB::Black,  false,     true,     -1,     false}
};
const int numEyeColors = sizeof(eyeColors) / sizeof(eyeColors[0]);

// Async HTTP server + SSE event stream for live status pushes
AsyncWebServer server(80);
AsyncEventSource events("/events");

// Page HTML, built once in setup(), reused for every "/" request
String pageHtml;

// Status tracking for web display
String lastEmote = "None";
String systemStatus = "Initializing...";

// Eye color restore after emote sequences
CRGB preEmoteColor = CRGB::Black;
bool pendingColorRestore = false;
unsigned long lastScriptStatusCheck = 0;
#define SCRIPT_STATUS_POLL_MS 150  // How often to poll Maestro for script completion

// Current eye state — path of whichever button matches the current LEDs 1&2.
// Used to highlight the active button in the web UI.
const char* currentEye = "color_orange";   // matches startup color
const char* preEmotePath = "color_orange"; // saved for restore alongside preEmoteColor

// Idle mode state
bool idleMode = false;
bool idleSequenceRunning = false;
unsigned long idleNextEmoteTime = 0;
int idleShuffleOrder[16];  // Max emote count
int idleShuffleIndex = 0;

void setup() {
  Serial.begin(115200);
  
  // Action queue (async handlers -> loop()) — sized for 8 buffered clicks.
  actionQueue = xQueueCreate(ACTION_QUEUE_DEPTH, sizeof(ActionMsg));

  // Initialize Maestro Serial Connection
  if (maestroAvailable) {
    maestroSerial.begin(MAESTRO_BAUD, SERIAL_8N1, MAESTRO_RX_PIN, MAESTRO_TX_PIN);
    Serial.println("Maestro serial initialized");
  } else {
    Serial.println("Maestro disabled in configuration");
  }

  // Initialize MP3 Player Serial Connection
  mp3PlayerSerial.begin(MP3_BAUD, SERIAL_8N1, MP3_RX_PIN, MP3_TX_PIN);
  // Give the DFPlayer Pro a moment to finish its own boot sequence before
  // we handshake. Buck-converter power-up can be slower than USB.
  delay(2000);
  Serial.println("Initializing MP3 player...");
  if (!mp3Player.begin(mp3PlayerSerial)) {
    Serial.println("MP3 player initialization failed!");
    Serial.println("Check connections (TX/RX swap is the usual culprit)");
    mp3PlayerAvailable = false;
  } else {
    Serial.println("MP3 player initialized successfully");
    // Suppress the chip's voice prompt that announces mode changes (e.g.
    // saying "music" when we call switchFunction below). This setting
    // persists to the chip's internal flash, so it survives power cycles —
    // but calling it every boot is idempotent and free.
    mp3Player.setPrompt(false);
    delay(100);
    // CRITICAL: switch the chip to MUSIC mode. After a USB-C disconnect the
    // chip may still be in UFDISK mode (acting as a removable drive rather
    // than playing files). Without this, getTotalFile() returns 0 and
    // playFileNum() does nothing. See bring-up notes in dfplayer_pro_test/.
    mp3Player.switchFunction(mp3Player.MUSIC);
    delay(2000);  // chip re-scans onboard flash after switchFunction
    mp3Player.enableAMP();          // defensive: ensure amp section is on
    delay(100);
    mp3Player.setVol(30);           // 0..30; 30 = max
    delay(100);
    // SINGLE play mode = each track plays once and stops. We refill the
    // next random track from loop() via isPlaying() polling.
    mp3Player.setPlayMode(mp3Player.SINGLE);
    mp3PlayerAvailable = true;
  }

  // Seed the random number generator from the ESP32's hardware RNG so the
  // random audio picks (and the idle-emote shuffle) actually differ across
  // power-cycles. Without this, you'd get the same "random" sequence every boot.
  randomSeed(esp_random());

  // Initialize FastLED
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  // Note: We don't use global brightness - each LED has individual brightness control
  FastLED.clear();
  FastLED.show();
  
  // Set initial eye color to orange (startup state)
  leds[REPEATER_LED] = CRGB::Black;                       // Inline repeater - always off
  leds[EYE_LED_1] = scaleEyeColor(CRGB(255,100,0));       // Eye 1 - dimmed for comfort
  leds[EYE_LED_2] = scaleEyeColor(CRGB(255,100,0));       // Eye 2 - dimmed for comfort
  leds[FLASHLIGHT_LED] = CRGB::Black;                     // Flashlight off
  FastLED.show();
  lastEmote = "orange (startup)";
  Serial.println("Eyes initialized to orange (brightness 50)");

  // Configure Access Point
  Serial.println("Configuring Access Point...");
  
  // Set static IP configuration (192.168.4.0/24 network)
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  
  // Configure the soft AP with static IP
  WiFi.softAPConfig(local_IP, gateway, subnet);
  
  // Start Access Point
  WiFi.softAP(droidname.c_str(), ap_password);
  
  // Print AP information
  Serial.println("");
  Serial.println("Access Point started!");
  Serial.print("SSID: ");
  Serial.println(droidname);
  Serial.print("Password: ");
  Serial.println(ap_password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("Connect to this network and navigate to http://192.168.4.1");
  
  // Set system status for web display
  systemStatus = "Ready";

  // Build the page HTML once (uses droidname/droidcolor, never changes)
  buildPageHtml(pageHtml);

  // Register routes + SSE handler and start the async server
  setupWebServer();
}

// Builds the system-state JSON into out. Used by SSE pushes.
void buildStatusJson(String &out) {
  out = "{\"lastEmote\":\"";
  out += lastEmote;
  out += "\",\"idle\":";
  out += (idleMode ? "true" : "false");
  out += ",\"maestro\":";
  out += (maestroAvailable ? "true" : "false");
  out += ",\"mp3\":";
  out += (mp3PlayerAvailable ? "true" : "false");
  out += ",\"flashlight\":";
  out += (leds[FLASHLIGHT_LED] != CRGB::Black ? "true" : "false");
  out += ",\"currentEye\":\"";
  out += currentEye;
  out += "\",\"status\":\"";
  out += systemStatus;
  out += "\"}";
}

// Pushes current state to all SSE subscribers. Cheap if no clients connected.
void pushStatus() {
  if (events.count() == 0) return;
  String json;
  buildStatusJson(json);
  events.send(json.c_str(), "message", millis());
}

// Safety function to ensure eyes are always synchronized
void verifyEyeSync() {
  if (leds[EYE_LED_1] != leds[EYE_LED_2]) {
    #if DEBUG_MODE
      Serial.println("WARNING: Eye LEDs out of sync! Forcing synchronization.");
      Serial.print("Eye 1: R="); Serial.print(leds[EYE_LED_1].r);
      Serial.print(" G="); Serial.print(leds[EYE_LED_1].g);
      Serial.print(" B="); Serial.println(leds[EYE_LED_1].b);
      Serial.print("Eye 2: R="); Serial.print(leds[EYE_LED_2].r);
      Serial.print(" G="); Serial.print(leds[EYE_LED_2].g);
      Serial.print(" B="); Serial.println(leds[EYE_LED_2].b);
    #endif
    // Force eye 2 to match eye 1
    leds[EYE_LED_2] = leds[EYE_LED_1];
  }
}

// Helper function to trigger button action (emote or action)
void triggerButton(const Button &button, bool fromIdle = false) {
  // Emotes (anything with a Maestro script) cancel idle mode — the user is
  // taking direct control. Eye-color buttons (scriptNumber < 0) don't cancel
  // idle so the operator can tweak the eye color mid-idle without restarting
  // the cycle.
  if (!fromIdle && button.scriptNumber >= 0) {
    idleMode = false;
    idleSequenceRunning = false;
  }

  // Track last action for status display
  lastEmote = button.label;
  
  #if DEBUG_MODE
    Serial.print("Triggering: ");
    Serial.println(button.path);
    if (!button.preserveLED12) {
      Serial.print("Eyes ");
      Serial.println(button.colorName);
    }
  #endif
  
  // Store and scale colors with appropriate brightness levels
  CRGB eyeColor = scaleEyeColor(button.color);           // Eyes at brightness 50
  CRGB flashlightColor = scaleFlashlightColor(button.led3Color);  // Flashlight at max brightness
  
  // Disable interrupts during LED update to prevent race conditions
  noInterrupts();
  
  // Only update LEDs 1 & 2 if not preserving their state
  if (!button.preserveLED12) {
    if (button.scriptNumber >= 0 && maestroAvailable) {
      // Emote with servo sequence: save current state to restore after sequence ends
      preEmoteColor = leds[EYE_LED_1];
      preEmotePath = currentEye;
      pendingColorRestore = true;
    } else {
      // Explicit eye color change: cancel any pending restore
      pendingColorRestore = false;
    }
    leds[EYE_LED_1] = eyeColor;  // Eye 1 (dimmed for eye comfort)
    leds[EYE_LED_2] = eyeColor;  // Eye 2 (always same as Eye 1)
    currentEye = button.path;
  }

  // Only update flashlight if not preserving its state
  if (!button.preserveLED3) {
    leds[FLASHLIGHT_LED] = flashlightColor;  // Flashlight (full brightness)
  }
  
  interrupts();  // Re-enable interrupts
  
  // Verify eyes are synchronized before displaying
  verifyEyeSync();
  
  FastLED.show();
  
  // Trigger maestro script if specified (scriptNumber >= 0) and Maestro is
  // available. While the script is running, loop() handles random audio
  // playback — see the AUDIO_TRACK_COUNT comment + the loop() polling block.
  if (button.scriptNumber >= 0) {
    if (maestroAvailable) {
      #if DEBUG_MODE
        Serial.print("Activating maestro sequence ");
        Serial.println(button.scriptNumber);
      #endif
      maestro.restartScript(button.scriptNumber);
      // Flag the animation as in progress unless this emote is explicitly
      // silent. loop() will refill random tracks for the duration via the
      // rate-limited poll block. The FIRST track fires here, right after
      // the script kicks off, so audio starts in sync with the servo motion
      // rather than waiting up to ~150ms for the next poll tick.
      if (!button.silent) {
        animationRunning = true;
        if (mp3PlayerAvailable) {
          mp3Player.playFileNum(random(1, AUDIO_TRACK_COUNT + 1));
        }
      }
    } else {
      // Always show errors/warnings
      Serial.println("Maestro script requested but Maestro not available");
    }
  }
}

// CSS hex string for a CRGB ("#RRGGBB"). Used to color eye-color buttons.
static String rgbToHex(const CRGB &c) {
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.r, c.g, c.b);
  return String(buf);
}

// Perceptual luminance test; true for colors light enough to need dark text.
static bool isLightColor(const CRGB &c) {
  int lum = (c.r * 299 + c.g * 587 + c.b * 114) / 1000;
  return lum > 180;
}

// Append one button's HTML to out. Click fires JS that hits /maestro/<path>.
// data-path lets JS toggle the .on highlight on the right button.
// If button.emoji is non-empty, renders an emote-style button (big emoji
// stacked above a small label). If useOwnColor is true, the button's own
// .color is used as the background (with text color picked for contrast).
void appendButton(String &out, const Button &button, bool useOwnColor = false) {
  out += "<button data-path=\"";
  out += button.path;
  out += "\" onclick=\"t('";
  out += button.path;
  out += "')\" class=\"button";
  if (button.emoji && button.emoji[0]) out += " emote";
  out += "\"";
  if (useOwnColor) {
    out += " style=\"background-color:";
    out += rgbToHex(button.color);
    if (isLightColor(button.color)) out += ";color:#111";
    out += ";\"";
  }
  out += ">";
  if (button.emoji && button.emoji[0]) {
    out += "<span class=\"emoji\">";
    out += button.emoji;
    out += "</span><span class=\"elabel\">";
    out += button.label;
    out += "</span>";
  } else {
    out += button.label;
  }
  out += "</button>";
}

// Builds the full control page HTML into out. Called once at startup; the
// result is stored in pageHtml and served by the "/" route handler.
void buildPageHtml(String &out) {
  out.reserve(4096);
  out = "<!DOCTYPE html><html>"
        "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<link rel=\"icon\" href=\"data:,\">"
        "<style>"
        "* { margin: 0; padding: 0; box-sizing: border-box; }"
        "html { font-family: Helvetica, Arial, sans-serif; }"
        "body { background-color: #1a1a1a; color: #ffffff; padding: 11px; padding-bottom: 78px; }"
        "h1 { text-align: center; margin-bottom: 15px; font-size: 24px; }"
        "h2 { text-align: center; margin: 15px 0 8px 0; font-size: 17px; color: #aaa; }"
        ".button-grid { display: grid; grid-template-columns: repeat(auto-fit, 88px); gap: 10px;"
        "max-width: 1200px; margin: 0 auto 16px auto; justify-content: center; }"
        ".button { background-color: ";
  out += droidcolor;
  out += "; border: none; border-radius: 50%; aspect-ratio: 1 / 1;"
         "color: white; padding: 6px; font-family: inherit; font-size: 12px; font-weight: bold;"
         "cursor: pointer; transition: all 0.3s; text-align: center; box-shadow: 0 3px 5px rgba(0,0,0,0.3);"
         "display: flex; align-items: center; justify-content: center; }"
         ".button:hover { transform: translateY(-2px); box-shadow: 0 5px 9px rgba(0,0,0,0.4); opacity: 0.9; }"
         ".button:active { transform: translateY(0); box-shadow: 0 2px 3px rgba(0,0,0,0.3); }"
         ".button.on { outline: 3px solid #ffffff; outline-offset: -3px; filter: brightness(1.25); }"
         ".button.emote { flex-direction: column; gap: 3px; padding: 6px 4px; }"
         ".button.emote .emoji { font-size: 28px; line-height: 1; }"
         ".button.emote .elabel { font-size: 10px; font-weight: normal; opacity: 0.85; }"
         ".toggle-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 8px; max-width: 1200px; margin: 0 auto 15px auto; }"
         ".toggle { background-color: #2a2a2a; border: 1px solid #444; border-radius: 6px; padding: 12px 16px;"
         "cursor: pointer; display: flex; align-items: center; justify-content: space-between; gap: 12px;"
         "font-size: 15px; font-weight: bold; color: white; transition: all 0.2s;"
         "box-shadow: 0 3px 5px rgba(0,0,0,0.3); user-select: none; -webkit-tap-highlight-color: transparent; }"
         ".toggle:hover { background-color: #353535; transform: translateY(-2px); box-shadow: 0 5px 9px rgba(0,0,0,0.4); }"
         ".toggle:active { transform: translateY(0); }"
         ".toggle-switch { width: 42px; height: 24px; background: #555; border-radius: 12px;"
         "position: relative; flex-shrink: 0; transition: background 0.2s; }"
         ".toggle-switch::before { content: ''; position: absolute; top: 3px; left: 3px;"
         "width: 18px; height: 18px; background: white; border-radius: 50%; transition: transform 0.2s; }"
         ".toggle.on { background-color: #1f3a2a; border-color: #4ade80; }"
         ".toggle.on .toggle-switch { background: #4ade80; }"
         ".toggle.on .toggle-switch::before { transform: translateX(18px); }"
         ".status-console { position: fixed; bottom: 0; left: 0; right: 0; background-color: #2a2a2a;"
         "border-top: 2px solid #444; padding: 8px 11px; box-shadow: 0 -2px 8px rgba(0,0,0,0.5); }"
         ".status-console h3 { margin: 0 0 6px 0; font-size: 11px; color: #888; text-align: center; }"
         ".status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(112px, 1fr)); gap: 6px;"
         "max-width: 1200px; margin: 0 auto; font-size: 9px; }"
         ".status-item { background-color: #1a1a1a; padding: 5px 8px; border-radius: 3px; border: 1px solid #444; }"
         ".status-item strong { color: #aaa; margin-right: 5px; }"
         "</style></head>"
         "<body><h1>BDX Control System (";
  out += droidname;
  out += ")</h1>";

  out += "<h2>Emotes</h2><div class=\"button-grid\">";
  for (int i = 0; i < numEmotes; i++) appendButton(out, emotes[i]);
  out += "</div>";

  out += "<h2>Toggles</h2><div class=\"toggle-grid\">"
         "<div class=\"toggle\" id=\"tog-flash\" onclick=\"tflash()\">"
           "<span>Flashlight</span><span class=\"toggle-switch\"></span>"
         "</div>"
         "<div class=\"toggle\" id=\"tog-idle\" onclick=\"tidle()\">"
           "<span>Idle Mode</span><span class=\"toggle-switch\"></span>"
         "</div>"
         "</div>";

  out += "<h2>Eye Colors</h2><div class=\"button-grid\">";
  for (int i = 0; i < numEyeColors; i++) appendButton(out, eyeColors[i], /*useOwnColor=*/true);
  out += "</div>";

  // Status console: static structure, values populated by SSE pushes
  out += "<div class=\"status-console\"><h3>System Status</h3><div class=\"status-grid\">"
         "<div class=\"status-item\"><strong>Network:</strong> ";
  out += droidname;
  out += " (192.168.4.1)</div>"
         "<div class=\"status-item\"><strong>Maestro:</strong> <span id=\"ms\">&mdash;</span></div>"
         "<div class=\"status-item\"><strong>MP3:</strong> <span id=\"ds\">&mdash;</span></div>"
         "<div class=\"status-item\"><strong>Idle:</strong> <span id=\"im\">&mdash;</span></div>"
         "<div class=\"status-item\"><strong>Status:</strong> <span id=\"ss\">&mdash;</span></div>"
         "<div class=\"status-item\"><strong>Last:</strong> <span id=\"le\">&mdash;</span></div>"
         "</div></div>";

  // Embedded JS: fire-and-forget action triggers + SSE for live status pushes.
  // No polling: the server pushes state changes the moment they happen.
  out += "<script>"
         "let st={};"
         "function hl(p,on){const b=document.querySelector('button[data-path=\"'+p+'\"]');if(b)b.classList.toggle('on',on);}"
         "function r(d){if(!d)return;st=d;"
         "document.getElementById('le').textContent=d.lastEmote;"
         "document.getElementById('im').textContent=d.idle?'On':'Off';"
         "document.getElementById('ms').textContent=d.maestro?'Connected':'Disabled';"
         "document.getElementById('ds').textContent=d.mp3?'Connected':'Not Available';"
         "document.getElementById('ss').textContent=d.status;"
         "document.querySelectorAll('button.on').forEach(b=>b.classList.remove('on'));"
         "if(d.currentEye)hl(d.currentEye,true);"
         "document.getElementById('tog-flash').classList.toggle('on',!!d.flashlight);"
         "document.getElementById('tog-idle').classList.toggle('on',!!d.idle);}"
         "async function t(p){try{await fetch('/maestro/'+p);}catch(e){}}"
         "function tflash(){t('flashlight');}"
         "function tidle(){t(st.idle?'idle_stop':'idle_start');}"
         "const es=new EventSource('/events');"
         "es.onmessage=e=>{try{r(JSON.parse(e.data));}catch(err){}};"
         "</script>"
         "</body></html>";
}

void shuffleIdleOrder() {
  for (int i = 0; i < numEmotes; i++) {
    idleShuffleOrder[i] = i;
  }
  for (int i = numEmotes - 1; i > 0; i--) {
    int j = random(0, i + 1);
    int tmp = idleShuffleOrder[i];
    idleShuffleOrder[i] = idleShuffleOrder[j];
    idleShuffleOrder[j] = tmp;
  }
}

// Runs the action identified by the path component after "/maestro/".
void dispatchMaestroAction(const char* path) {
  bool handled = false;
  if (strcmp(path, "flashlight") == 0) {
    noInterrupts();
    bool isOff = (leds[FLASHLIGHT_LED] == CRGB::Black);
    if (isOff) {
      leds[FLASHLIGHT_LED] = scaleFlashlightColor(CRGB::White);
      lastEmote = "flashlight on";
    } else {
      leds[FLASHLIGHT_LED] = CRGB::Black;
      lastEmote = "flashlight off";
    }
    interrupts();
    verifyEyeSync();
    FastLED.show();
    #if DEBUG_MODE
      Serial.print("Flashlight toggled: ");
      Serial.println(lastEmote);
    #endif
    handled = true;
  } else if (strcmp(path, "idle_start") == 0) {
    shuffleIdleOrder();
    idleShuffleIndex = 0;
    idleSequenceRunning = false;
    idleNextEmoteTime = millis() + random(IDLE_MIN_DELAY_MS, IDLE_MAX_DELAY_MS);
    idleMode = true;
    lastEmote = "idle mode on";
    #if DEBUG_MODE
      Serial.println("Idle mode started");
    #endif
    handled = true;
  } else if (strcmp(path, "idle_stop") == 0) {
    idleMode = false;
    idleSequenceRunning = false;
    lastEmote = "idle mode off";
    #if DEBUG_MODE
      Serial.println("Idle mode stopped");
    #endif
    handled = true;
  }
  if (!handled) {
    for (int i = 0; i < numEmotes && !handled; i++) {
      if (strcmp(path, emotes[i].path) == 0) {
        triggerButton(emotes[i]);
        handled = true;
      }
    }
    for (int i = 0; i < numEyeColors && !handled; i++) {
      if (strcmp(path, eyeColors[i].path) == 0) {
        triggerButton(eyeColors[i]);
        handled = true;
      }
    }
  }
  if (handled) pushStatus();
}


void setupWebServer() {
  // Main page (built once at startup, just streamed out here)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html; charset=utf-8", pageHtml);
  });

  // Server-Sent Events endpoint — clients open once, receive pushes.
  events.onConnect([](AsyncEventSourceClient *client) {
    String json;
    buildStatusJson(json);
    client->send(json.c_str(), "message", millis());
  });
  server.addHandler(&events);

  // Dynamic /maestro/<path> dispatcher. Handled via onNotFound to avoid
  // depending on the ASYNCWEBSERVER_REGEX build flag. The path is enqueued
  // for loop() to actually execute; this keeps all hardware operations on a
  // single thread (no Serial1/FastLED concurrency with the async task).
  server.onNotFound([](AsyncWebServerRequest *req) {
    const String &url = req->url();
    if (req->method() == HTTP_GET && url.startsWith("/maestro/")) {
      if (actionQueue) {
        ActionMsg msg;
        strncpy(msg.path, url.c_str() + 9, ACTION_PATH_MAX - 1);
        msg.path[ACTION_PATH_MAX - 1] = '\0';
        xQueueSend(actionQueue, &msg, 0);  // non-blocking; drop if full
      }
      req->send(200, "text/plain", "OK");
    } else {
      req->send(404, "text/plain", "Not Found");
    }
  });

  server.begin();
}

void loop(){
  // Drain any actions queued by the async HTTP task.
  if (actionQueue) {
    ActionMsg msg;
    while (xQueueReceive(actionQueue, &msg, 0) == pdTRUE) {
      dispatchMaestroAction(msg.path);
    }
  }

  // Rate-limited polling block. Drives three concerns at ~150ms cadence:
  //   1. Audio refill: poll mp3Player.isPlaying() and fire the next random
  //      track when the chip goes idle. The DF1201S's isPlaying() reports
  //      reliably (unlike the YX5200-based chips we tried earlier).
  //   2. Animation-end detection via getScriptStatus(): clear animationRunning
  //      and advance idle mode to its next-emote scheduler.
  //   3. Pre-emote eye-color restore (extension point — currently unused,
  //      but kept wired up for future flexibility).
  //
  // All hardware ops stay single-threaded inside loop() — no concurrent
  // access with the async network task.
  if ((animationRunning || pendingColorRestore) && maestroAvailable &&
      (millis() - lastScriptStatusCheck >= SCRIPT_STATUS_POLL_MS)) {
    lastScriptStatusCheck = millis();

    // (1) Audio refill — only relevant during an active animation. The
    // FIRST track of each animation is fired immediately by triggerButton()
    // for low latency; this block handles subsequent refills as tracks end.
    // Tracks already playing when the animation ends are allowed to finish
    // naturally (we never call stop()).
    if (animationRunning && mp3PlayerAvailable && !mp3Player.isPlaying()) {
      mp3Player.playFileNum(random(1, AUDIO_TRACK_COUNT + 1));
    }

    // (2) Animation-end detection.
    if (maestro.getScriptStatus() == 1) {  // 1 = script stopped
      bool wasAnimation = animationRunning || pendingColorRestore;
      animationRunning = false;

      if (pendingColorRestore) {
        noInterrupts();
        leds[EYE_LED_1] = preEmoteColor;
        leds[EYE_LED_2] = preEmoteColor;
        interrupts();
        FastLED.show();
        currentEye = preEmotePath;
        pendingColorRestore = false;
      }

      if (idleMode && wasAnimation) {
        idleSequenceRunning = false;
        idleNextEmoteTime = millis() + random(IDLE_MIN_DELAY_MS, IDLE_MAX_DELAY_MS);
      }

      pushStatus();  // Script ended — notify subscribers
    }
  }

  // Fire the next idle emote when the timer expires and no sequence is running
  if (idleMode && !idleSequenceRunning && millis() >= idleNextEmoteTime) {
    int idx = idleShuffleOrder[idleShuffleIndex];
    triggerButton(emotes[idx], true);
    idleSequenceRunning = true;
    idleShuffleIndex++;
    if (idleShuffleIndex >= numEmotes) {
      shuffleIdleOrder();
      idleShuffleIndex = 0;
    }
    // If Maestro is unavailable, pendingColorRestore won't be set so we won't
    // get the normal callback — schedule the next emote from here instead
    if (!maestroAvailable) {
      idleSequenceRunning = false;
      idleNextEmoteTime = millis() + random(IDLE_MIN_DELAY_MS, IDLE_MAX_DELAY_MS);
    }
    pushStatus();  // Idle fired a new emote — notify subscribers
  }

  // HTTP handling lives in AsyncWebServer's own task; loop() just runs droid state.
  // A small delay keeps the watchdog happy and yields to the WiFi/HTTP tasks.
  delay(2);
}

// Helper function to scale color brightness for eyes
CRGB scaleEyeColor(CRGB color) {
  return CRGB(
    (color.r * EYE_BRIGHTNESS) / 255,
    (color.g * EYE_BRIGHTNESS) / 255,
    (color.b * EYE_BRIGHTNESS) / 255
  );
}

// Helper function to scale color brightness for flashlight (full brightness)
CRGB scaleFlashlightColor(CRGB color) {
  return CRGB(
    (color.r * FLASHLIGHT_BRIGHTNESS) / 255,
    (color.g * FLASHLIGHT_BRIGHTNESS) / 255,
    (color.b * FLASHLIGHT_BRIGHTNESS) / 255
  );
}
