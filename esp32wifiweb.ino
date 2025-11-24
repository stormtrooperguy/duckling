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

// Load Wi-Fi library
#include <WiFi.h>

// *** IMPORTANT: Customize these values for your installation ***
// Replace these strings to customize for your droid
String droidname = "YourDroidName";      // Change this! Will be your WiFi SSID
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

// Forward declarations
CRGB scaleEyeColor(CRGB color);
CRGB scaleFlashlightColor(CRGB color);

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
  {"flashlight",  "flashlight",  "Flashlight Toggle",  CRGB::Black, CRGB::White, true, false, -1,     -1}
};
const int numActions = sizeof(actions) / sizeof(actions[0]);

// EYE COLORS: Just change eye colors without servo movements (preserves flashlight state)
const Button eyeColors[] = {
  // path          label           colorName  LED1&2 color   LED3 color      preserve12  preserve3  script# mp3#
  {"color_white",  "white",        "White",   CRGB::White,   CRGB::Black,    false,      true,      -1,     -1},
  {"color_yellow", "yellow",       "Yellow",  CRGB::Yellow,  CRGB::Black,    false,      true,      -1,     -1},
  {"color_green",  "green",        "Green",   CRGB::Green,   CRGB::Black,    false,      true,      -1,     -1},
  {"color_red",    "red",          "Red",     CRGB::Red,     CRGB::Black,    false,      true,      -1,     -1},
  {"color_blue",   "blue",         "Blue",    CRGB::Blue,    CRGB::Black,    false,      true,      -1,     -1},
  {"color_purple", "purple",       "Purple",  CRGB::Purple,  CRGB::Black,    false,      true,      -1,     -1}
};
const int numEyeColors = sizeof(eyeColors) / sizeof(eyeColors[0]);

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;
#define MAX_HEADER_SIZE 512  // Limit header size to prevent memory issues

// Status tracking for web display
String lastEmote = "None";
String systemStatus = "Initializing...";

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (optimized for faster response)
const long timeoutTime = 1000;  // Reduced from 2000ms to 1000ms

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
  
  // Set initial eye color to white (wake up state)
  leds[0] = scaleEyeColor(CRGB::White);  // LED 1 (eye) - dimmed for comfort
  leds[1] = scaleEyeColor(CRGB::White);  // LED 2 (eye) - dimmed for comfort
  leds[2] = CRGB::Black;                  // LED 3 (flashlight off)
  FastLED.show();
  lastEmote = "wake up (startup)";
  Serial.println("Eyes initialized to white (brightness 50)");

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
  
  server.begin();
}

// Helper function to generate status HTML for web display
String getStatusHTML() {
  String status = "<div class=\"status-console\">";
  status += "<h3>System Status</h3>";
  status += "<div class=\"status-grid\">";
  
  // Network info
  status += "<div class=\"status-item\"><strong>Network:</strong> " + droidname + " (192.168.4.1)</div>";
  
  // Component status
  status += "<div class=\"status-item\"><strong>Maestro:</strong> ";
  status += maestroAvailable ? "Connected" : "Disabled";
  status += "</div>";
  
  status += "<div class=\"status-item\"><strong>DFPlayer:</strong> ";
  status += dfPlayerAvailable ? "Connected" : "Not Available";
  status += "</div>";
  
  // System status
  status += "<div class=\"status-item\"><strong>Status:</strong> " + systemStatus + "</div>";
  
  // Last emote
  status += "<div class=\"status-item\"><strong>Last Emote:</strong> " + lastEmote + "</div>";
  
  status += "</div></div>";
  return status;
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
void triggerButton(const Button &button) {
  // Track last action for status display
  lastEmote = String(button.label);
  
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
    leds[0] = eyeColor;  // LED 1 (dimmed for eye comfort)
    leds[1] = eyeColor;  // LED 2 (always same as LED 1)
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

// Helper function to generate emote button HTML (optimized)
void createButton(WiFiClient &client, const Button &button) {
  // Combine multiple small writes into one for better performance
  String btn = String("<a href=\"/maestro/") + button.path + "\" class=\"button\">" + button.label + "</a>";
  client.println(btn);
}

void loop(){
  WiFiClient client = server.accept();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    client.setNoDelay(true);                // Disable Nagle's algorithm for faster response
    currentTime = millis();
    previousTime = currentTime;
    #if DEBUG_MODE
      Serial.println("New Client.");
    #endif
    header.reserve(MAX_HEADER_SIZE);        // Pre-allocate memory to avoid reallocation
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        // Only add to header if we haven't exceeded max size
        if (header.length() < MAX_HEADER_SIZE) {
          header += c;
        }
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // Handle emote requests
            for (int i = 0; i < numEmotes; i++) {
              String searchPath = String("GET /maestro/") + emotes[i].path;
              if (header.indexOf(searchPath) >= 0) {
                triggerButton(emotes[i]);
                break;
              }
            }
            
            // Handle action requests
            for (int i = 0; i < numActions; i++) {
              String searchPath = String("GET /maestro/") + actions[i].path;
              if (header.indexOf(searchPath) >= 0) {
                // Special handling for flashlight toggle
                if (String(actions[i].path) == "flashlight") {
                  // Toggle LED 3: if it's on (not black), turn off; if off, turn white at max brightness
                  noInterrupts();  // Prevent race conditions
                  bool isOff = (leds[2] == CRGB::Black);
                  if (isOff) {
                    leds[2] = scaleFlashlightColor(CRGB::White);  // Full brightness (255)
                    lastEmote = "flashlight on";
                  } else {
                    leds[2] = CRGB::Black;
                    lastEmote = "flashlight off";
                  }
                  interrupts();
                  verifyEyeSync();  // Ensure eyes stay synchronized
                  FastLED.show();
                  #if DEBUG_MODE
                    Serial.print("Flashlight toggled: ");
                    Serial.println(lastEmote);
                  #endif
                } else {
                  triggerButton(actions[i]);
                }
                break;
              }
            }
            
            // Handle eye color requests
            for (int i = 0; i < numEyeColors; i++) {
              String searchPath = String("GET /maestro/") + eyeColors[i].path;
              if (header.indexOf(searchPath) >= 0) {
                triggerButton(eyeColors[i]);
                break;
              }
            }
            
            // Display the HTML web page (optimized with larger chunks)
            client.print(
              "<!DOCTYPE html><html>"
              "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
              "<link rel=\"icon\" href=\"data:,\">"
              "<style>"
              "* { margin: 0; padding: 0; box-sizing: border-box; }"
              "html { font-family: Helvetica, Arial, sans-serif; }"
              "body { background-color: #1a1a1a; color: #ffffff; padding: 11px; }"
              "h1 { text-align: center; margin-bottom: 15px; font-size: 24px; }"
              "h2 { text-align: center; margin: 15px 0 8px 0; font-size: 17px; color: #aaa; }"
              ".button-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(112px, 1fr)); gap: 8px; max-width: 1200px; margin: 0 auto 15px auto; }"
              ".button { background-color: "
            );
            client.print(droidcolor);
            client.print(
              "; border: none; border-radius: 6px; color: white; padding: 15px 11px;"
              "text-decoration: none; font-size: 15px; font-weight: bold; cursor: pointer;"
              "transition: all 0.3s; display: block; text-align: center; box-shadow: 0 3px 5px rgba(0,0,0,0.3); }"
              ".button:hover { transform: translateY(-2px); box-shadow: 0 5px 9px rgba(0,0,0,0.4); opacity: 0.9; }"
              ".button:active { transform: translateY(0); box-shadow: 0 2px 3px rgba(0,0,0,0.3); }"
              ".status-console { position: fixed; bottom: 0; left: 0; right: 0; background-color: #2a2a2a; "
              "border-top: 2px solid #444; padding: 8px 11px; box-shadow: 0 -2px 8px rgba(0,0,0,0.5); }"
              ".status-console h3 { margin: 0 0 6px 0; font-size: 11px; color: #888; text-align: center; }"
              ".status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(112px, 1fr)); gap: 6px; "
              "max-width: 1200px; margin: 0 auto; font-size: 9px; }"
              ".status-item { background-color: #1a1a1a; padding: 5px 8px; border-radius: 3px; border: 1px solid #444; }"
              ".status-item strong { color: #aaa; margin-right: 5px; }"
              "body { padding-bottom: 68px; }"
              "</style></head>"
              "<body><h1>BDX Control System ("
            );
            client.print(droidname);
            client.print(")</h1>");
            
            // Emotes section
            client.println("<h2>Emotes</h2><div class=\"button-grid\">");
            for (int i = 0; i < numEmotes; i++) {
              createButton(client, emotes[i]);
            }
            client.println("</div>");
            
            // Actions section
            client.println("<h2>Actions</h2><div class=\"button-grid\">");
            for (int i = 0; i < numActions; i++) {
              createButton(client, actions[i]);
            }
            client.println("</div>");
            
            // Eye Colors section
            client.println("<h2>Eye Colors</h2><div class=\"button-grid\">");
            for (int i = 0; i < numEyeColors; i++) {
              createButton(client, eyeColors[i]);
            }
            client.println("</div>");
            
            // Add status console
            client.print(getStatusHTML());
            
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection (stop() handles flushing automatically)
    client.stop();
    #if DEBUG_MODE
      Serial.println("Client disconnected.");
      Serial.println("");
    #endif
  }
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
