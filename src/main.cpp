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
  - DFPlayer Mini MP3 module support
  - Multiple emotes with coordinated LEDs, sounds, and servos
  - Performance optimized HTTP handling
  - Debug mode for development and troubleshooting
*********/

// Load Wi-Fi library + async HTTP server
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

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

// DFPlayer Mini MP3 Module
#define DFPLAYER_SERIAL_NUM 2     // Use Serial2
#define DFPLAYER_RX_PIN 25        // ESP32 RX (connects to DFPlayer TX) - Changed from GPIO 9
#define DFPLAYER_TX_PIN 26        // ESP32 TX (connects to DFPlayer RX) - Changed from GPIO 10
#define DFPLAYER_BAUD 9600

// NOTE: GPIO 9 and 10 are used for flash and will cause boot issues on most ESP32 boards
// GPIO 25 and 26 are safe general-purpose pins

// Maestro library
#include <PololuMaestro.h>
HardwareSerial maestroSerial(MAESTRO_SERIAL_NUM);
MiniMaestro maestro(maestroSerial);
bool maestroAvailable = MAESTRO_ENABLED;  // Track if Maestro is available

// DFPlayer Mini library
#include <DFRobotDFPlayerMini.h>
HardwareSerial dfPlayerSerial(DFPLAYER_SERIAL_NUM);
DFRobotDFPlayerMini dfPlayer;
bool dfPlayerAvailable = false;     // Track if DFPlayer initialized successfully

// FastLED library
#include <FastLED.h>

// LED Strip Configuration
#define NUM_LEDS 3
#define LED_PIN 5  // Change this to your desired GPIO pin
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

// LED Brightness settings (0-255)
#define EYE_BRIGHTNESS 50        // Brightness for LEDs 1 & 2 (eyes) - comfortable viewing
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

// Button definition structure (used for Emotes, Actions, and Eye Colors)
struct Button {
  const char* path;        // URL path (e.g., "sleep")
  const char* label;       // Button label (e.g., "go to sleep")
  const char* colorName;   // Color name for serial output
  CRGB color;              // LED color for LEDs 1 & 2
  CRGB led3Color;          // LED 3 color (separate control)
  bool preserveLED12;      // If true, don't change LEDs 1 & 2
  bool preserveLED3;       // If true, don't change LED 3
  int scriptNumber;        // Maestro script number (-1 = no script)
  int mp3Track;            // DFPlayer track number (-1 = no sound)
};

// EMOTES: Change eye colors and trigger servo sequences
// Ordered to match Maestro script programming (0-6)
const Button emotes[] = {
  // path          label           colorName  LED1&2 color   LED3 color      preserve12  preserve3  script# mp3#
  {"angry",        "angry",        "Red",     CRGB::Red,     CRGB::Black,    false,      false,     0,      -1},
  {"curious",      "curious",      "Yellow",  CRGB::Yellow,  CRGB::Black,    false,      false,     1,      -1},
  {"happy",        "happy",        "Green",   CRGB::Green,   CRGB::Black,    false,      false,     2,      -1},
  {"no",           "no",           "Red",     CRGB::DarkOrange,  CRGB::Black,    false,      false,     3,      -1},  
  {"sad",          "sad",          "Blue",    CRGB::Blue,    CRGB::Black,    false,      false,     4,      -1},
  {"scared",       "scared",       "Purple",  CRGB::Purple,  CRGB::Black,    false,      false,     5,      -1},
  {"sleep",        "go to sleep",  "Off",     CRGB::Black,   CRGB::Black,    false,      false,     6,      -1},
  {"wake",         "wake up",      "White",   CRGB::White,   CRGB::Black,    false,      false,     7,      -1},
  {"yes",          "yes",          "Green",   CRGB::Green,   CRGB::Black,    false,      false,     8,      -1}
};
const int numEmotes = sizeof(emotes) / sizeof(emotes[0]);

// ACTIONS: Utility functions that don't change eye colors
const Button actions[] = {
  // path          label           colorName  LED1&2 color   LED3 color      preserve12  preserve3  script# mp3#
  {"flashlight",  "flashlight",  "Flashlight Toggle",  CRGB::Black, CRGB::White, true,  false, -1, -1},
  {"idle_start",  "idle on",     "Idle Mode On",       CRGB::Black, CRGB::Black, true,  true,  -1, -1},
  {"idle_stop",   "idle off",    "Idle Mode Off",      CRGB::Black, CRGB::Black, true,  true,  -1, -1}
};
const int numActions = sizeof(actions) / sizeof(actions[0]);

// EYE COLORS: Just change eye colors without servo movements (preserves flashlight state)
const Button eyeColors[] = {
  // path          label           colorName  LED1&2 color   LED3 color      preserve12  preserve3  script# mp3#
  {"color_white",  "white",        "White",   CRGB::White,   CRGB::Black,    false,      true,      -1,     -1},
  {"color_yellow", "yellow",       "Yellow",  CRGB::Yellow,  CRGB::Black,    false,      true,      -1,     -1},
  {"color_orange", "orange",       "Orange",  CRGB::Orange,  CRGB::Black,    false,      true,      -1,     -1},
  {"color_green",  "green",        "Green",   CRGB::Green,   CRGB::Black,    false,      true,      -1,     -1},
  {"color_red",    "red",          "Red",     CRGB::Red,     CRGB::Black,    false,      true,      -1,     -1},
  {"color_blue",   "blue",         "Blue",    CRGB::Blue,    CRGB::Black,    false,      true,      -1,     -1},
  {"color_purple", "purple",       "Purple",  CRGB::Purple,  CRGB::Black,    false,      true,      -1,     -1}
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
  
  // Initialize Maestro Serial Connection
  if (maestroAvailable) {
    maestroSerial.begin(MAESTRO_BAUD, SERIAL_8N1, MAESTRO_RX_PIN, MAESTRO_TX_PIN);
    Serial.println("Maestro serial initialized");
  } else {
    Serial.println("Maestro disabled in configuration");
  }

  // Initialize DFPlayer Serial Connection
  dfPlayerSerial.begin(DFPLAYER_BAUD, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  Serial.println("Initializing DFPlayer...");
  if (!dfPlayer.begin(dfPlayerSerial)) {
    Serial.println("DFPlayer initialization failed!");
    Serial.println("Check connections and SD card");
    dfPlayerAvailable = false;
  } else {
    Serial.println("DFPlayer initialized successfully");
    dfPlayer.volume(20);  // Set volume (0-30)
    dfPlayerAvailable = true;
  }

  // Initialize FastLED
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  // Note: We don't use global brightness - each LED has individual brightness control
  FastLED.clear();
  FastLED.show();
  
  // Set initial eye color to orange (startup state)
  leds[0] = scaleEyeColor(CRGB::Orange);  // LED 1 (eye) - dimmed for comfort
  leds[1] = scaleEyeColor(CRGB::Orange);  // LED 2 (eye) - dimmed for comfort
  leds[2] = CRGB::Black;                   // LED 3 (flashlight off)
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
  out += ",\"dfplayer\":";
  out += (dfPlayerAvailable ? "true" : "false");
  out += ",\"flashlight\":";
  out += (leds[2] != CRGB::Black ? "true" : "false");
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

// Safety function to ensure eyes (LEDs 1 & 2) are always synchronized
void verifyEyeSync() {
  if (leds[0] != leds[1]) {
    #if DEBUG_MODE
      Serial.println("WARNING: Eye LEDs out of sync! Forcing synchronization.");
      Serial.print("LED 0: R="); Serial.print(leds[0].r); 
      Serial.print(" G="); Serial.print(leds[0].g); 
      Serial.print(" B="); Serial.println(leds[0].b);
      Serial.print("LED 1: R="); Serial.print(leds[1].r); 
      Serial.print(" G="); Serial.print(leds[1].g); 
      Serial.print(" B="); Serial.println(leds[1].b);
    #endif
    // Force both eyes to match LED 0
    leds[1] = leds[0];
  }
}

// Helper function to trigger button action (emote or action)
void triggerButton(const Button &button, bool fromIdle = false) {
  // Any user-initiated trigger cancels idle mode
  if (!fromIdle) {
    idleMode = false;
    idleSequenceRunning = false;
  }

  // Track last action for status display
  lastEmote = button.label;
  
  #if DEBUG_MODE
    Serial.print("Triggering: ");
    Serial.println(button.path);
    Serial.print("Eyes ");
    Serial.println(button.colorName);
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
      preEmoteColor = leds[0];
      preEmotePath = currentEye;
      pendingColorRestore = true;
    } else {
      // Explicit eye color change: cancel any pending restore
      pendingColorRestore = false;
    }
    leds[0] = eyeColor;  // LED 1 (dimmed for eye comfort)
    leds[1] = eyeColor;  // LED 2 (always same as LED 1)
    currentEye = button.path;
  }
  
  // Only update LED 3 if not preserving its state
  if (!button.preserveLED3) {
    leds[2] = flashlightColor;  // LED 3 (full brightness for flashlight)
  }
  
  interrupts();  // Re-enable interrupts
  
  // Verify eyes are synchronized before displaying
  verifyEyeSync();
  
  FastLED.show();
  
  // Play MP3 track if specified (mp3Track >= 0) and DFPlayer is available
  if (button.mp3Track >= 0) {
    if (dfPlayerAvailable) {
      #if DEBUG_MODE
        Serial.print("Playing MP3 track ");
        Serial.println(button.mp3Track);
      #endif
      dfPlayer.play(button.mp3Track);
    } else {
      // Always show errors/warnings
      Serial.println("MP3 requested but DFPlayer not available");
    }
  }
  
  // Only trigger maestro script if specified (scriptNumber >= 0) and Maestro is available
  if (button.scriptNumber >= 0) {
    if (maestroAvailable) {
      #if DEBUG_MODE
        Serial.print("Activating maestro sequence ");
        Serial.println(button.scriptNumber);
      #endif
      maestro.restartScript(button.scriptNumber);
    } else {
      // Always show errors/warnings
      Serial.println("Maestro script requested but Maestro not available");
    }
  }
}

// Append one button's HTML to out. Click fires JS that hits /maestro/<path>.
// data-path lets JS toggle the .on highlight on the right button.
void appendButton(String &out, const Button &button) {
  out += "<button data-path=\"";
  out += button.path;
  out += "\" onclick=\"t('";
  out += button.path;
  out += "')\" class=\"button\">";
  out += button.label;
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
        ".button-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(112px, 1fr)); gap: 8px; max-width: 1200px; margin: 0 auto 15px auto; }"
        ".button { background-color: ";
  out += droidcolor;
  out += "; border: none; border-radius: 6px; color: white; padding: 15px 11px;"
         "font-family: inherit; font-size: 15px; font-weight: bold; cursor: pointer;"
         "transition: all 0.3s; text-align: center; box-shadow: 0 3px 5px rgba(0,0,0,0.3); }"
         ".button:hover { transform: translateY(-2px); box-shadow: 0 5px 9px rgba(0,0,0,0.4); opacity: 0.9; }"
         ".button:active { transform: translateY(0); box-shadow: 0 2px 3px rgba(0,0,0,0.3); }"
         ".button.on { outline: 3px solid #ffffff; outline-offset: -3px; filter: brightness(1.25); }"
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

  out += "<h2>Actions</h2><div class=\"button-grid\">";
  for (int i = 0; i < numActions; i++) appendButton(out, actions[i]);
  out += "</div>";

  out += "<h2>Eye Colors</h2><div class=\"button-grid\">";
  for (int i = 0; i < numEyeColors; i++) appendButton(out, eyeColors[i]);
  out += "</div>";

  // Status console: static structure, values populated by SSE pushes
  out += "<div class=\"status-console\"><h3>System Status</h3><div class=\"status-grid\">"
         "<div class=\"status-item\"><strong>Network:</strong> ";
  out += droidname;
  out += " (192.168.4.1)</div>"
         "<div class=\"status-item\"><strong>Maestro:</strong> <span id=\"ms\">&mdash;</span></div>"
         "<div class=\"status-item\"><strong>DFPlayer:</strong> <span id=\"ds\">&mdash;</span></div>"
         "<div class=\"status-item\"><strong>Idle:</strong> <span id=\"im\">&mdash;</span></div>"
         "<div class=\"status-item\"><strong>Status:</strong> <span id=\"ss\">&mdash;</span></div>"
         "<div class=\"status-item\"><strong>Last:</strong> <span id=\"le\">&mdash;</span></div>"
         "</div></div>";

  // Embedded JS: fire-and-forget action triggers + SSE for live status pushes.
  // No polling: the server pushes state changes the moment they happen.
  out += "<script>"
         "function hl(p,on){const b=document.querySelector('button[data-path=\"'+p+'\"]');if(b)b.classList.toggle('on',on);}"
         "function r(d){if(!d)return;"
         "document.getElementById('le').textContent=d.lastEmote;"
         "document.getElementById('im').textContent=d.idle?'On':'Off';"
         "document.getElementById('ms').textContent=d.maestro?'Connected':'Disabled';"
         "document.getElementById('ds').textContent=d.dfplayer?'Connected':'Not Available';"
         "document.getElementById('ss').textContent=d.status;"
         "document.querySelectorAll('button.on').forEach(b=>b.classList.remove('on'));"
         "if(d.currentEye)hl(d.currentEye,true);"
         "if(d.idle)hl('idle_start',true);"
         "if(d.flashlight)hl('flashlight',true);}"
         "async function t(p){try{await fetch('/maestro/'+p);}catch(e){}}"
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
    bool isOff = (leds[2] == CRGB::Black);
    if (isOff) {
      leds[2] = scaleFlashlightColor(CRGB::White);
      lastEmote = "flashlight on";
    } else {
      leds[2] = CRGB::Black;
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
    for (int i = 0; i < numActions && !handled; i++) {
      if (strcmp(path, actions[i].path) == 0) {
        triggerButton(actions[i]);
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
    req->send(200, "text/html", pageHtml);
  });

  // Action trigger — runs the requested droid action, returns minimal 200.
  // State updates flow back to clients via SSE, so no body is needed.
  server.on("^\\/maestro\\/(.+)$", HTTP_GET, [](AsyncWebServerRequest *req) {
    String path = req->pathArg(0);
    dispatchMaestroAction(path.c_str());
    req->send(200, "text/plain", "OK");
  });

  // Server-Sent Events endpoint — clients open once, receive pushes.
  events.onConnect([](AsyncEventSourceClient *client) {
    String json;
    buildStatusJson(json);
    client->send(json.c_str(), "message", millis());
  });
  server.addHandler(&events);

  server.begin();
}

void loop(){
  // Restore eye color once the emote servo sequence finishes (rate-limited polling)
  if (pendingColorRestore && maestroAvailable && (millis() - lastScriptStatusCheck >= SCRIPT_STATUS_POLL_MS)) {
    lastScriptStatusCheck = millis();
    if (maestro.getScriptStatus() == 1) {  // 1 = script stopped
      noInterrupts();
      leds[0] = preEmoteColor;
      leds[1] = preEmoteColor;
      interrupts();
      FastLED.show();
      currentEye = preEmotePath;
      pendingColorRestore = false;
      if (idleMode) {
        idleSequenceRunning = false;
        idleNextEmoteTime = millis() + random(IDLE_MIN_DELAY_MS, IDLE_MAX_DELAY_MS);
      }
      pushStatus();  // Eye-color restored — notify subscribers
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
