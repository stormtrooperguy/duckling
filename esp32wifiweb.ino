// Load Wi-Fi library
#include <WiFi.h>

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

// Emote definition structure
struct Emote {
  const char* path;        // URL path (e.g., "sleep")
  const char* label;       // Button label (e.g., "go to sleep")
  const char* colorName;   // Color name for serial output
  CRGB color;              // LED color for LEDs 1 & 2
  CRGB led3Color;          // LED 3 color (separate control)
  bool preserveLED12;      // If true, don't change LEDs 1 & 2
  int scriptNumber;        // Maestro script number (-1 = no script)
  int mp3Track;            // DFPlayer track number (-1 = no sound)
};

// Define all emotes in one place
const Emote emotes[] = {
  // path          label           colorName  LED1&2 color   LED3 color      preserve12  script# mp3#
  {"sleep",        "go to sleep",  "Off",     CRGB::Black,   CRGB::Black,    false,      0,      -1},
  {"wake",         "wake up",      "White",   CRGB::White,   CRGB::Black,    false,      1,      -1},
  {"happy",        "happy",        "Green",   CRGB::Green,   CRGB::Black,    false,      2,      -1},
  {"curious",      "curious",      "Yellow",  CRGB::Yellow,  CRGB::Black,    false,      3,      -1},
  {"angry",        "angry",        "Red",     CRGB::Red,     CRGB::Black,    false,      4,      -1},
  {"sad",          "sad",          "Blue",    CRGB::Blue,    CRGB::Black,    false,      5,      -1},
  {"idle",         "idle",         "White",   CRGB::White,   CRGB::Black,    false,      6,      -1},
  {"lookleft",     "look left",    "White",   CRGB::White,   CRGB::Black,    true,       7,      -1},
  {"lookright",    "look right",   "White",   CRGB::White,   CRGB::Black,    true,       8,      -1},
  {"lookup",       "look up",      "White",   CRGB::White,   CRGB::Black,    true,       9,      -1},
  {"lookdown",     "look down",    "White",   CRGB::White,   CRGB::Black,    true,      10,      -1},
  {"scared",       "scared",       "Purple",  CRGB::Purple,  CRGB::Black,    false,     11,      -1},
  {"flashlight_on",  "flashlight on",  "Flashlight On",  CRGB::Black, CRGB::White, true, -1,     -1},
  {"flashlight_off", "flashlight off", "Flashlight Off", CRGB::Black, CRGB::Black, true, -1,     -1}
};
const int numEmotes = sizeof(emotes) / sizeof(emotes[0]);

// Replace these strings to customize for your droid
String droidname = "Grek";
String droidcolor = "green";

// Access Point credentials
// SSID will use droidname, password is randomly generated
// Change this password for security
const char* ap_password = "k7Rm9pQx2w";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;
#define MAX_HEADER_SIZE 512  // Limit header size to prevent memory issues

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
  FastLED.setBrightness(50);  // Set brightness (0-255)
  FastLED.clear();
  FastLED.show();

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
  
  server.begin();
}

// Helper function to set emote state
void setEmote(const Emote &emote) {
  Serial.print("Setting emote: ");
  Serial.println(emote.path);
  
  // Set LED colors
  Serial.print("Eyes ");
  Serial.println(emote.colorName);
  
  // Only update LEDs 1 & 2 if not preserving their state
  if (!emote.preserveLED12) {
    leds[0] = emote.color;  // LED 1
    leds[1] = emote.color;  // LED 2
  }
  
  // Always update LED 3
  leds[2] = emote.led3Color;
  FastLED.show();
  
  // Play MP3 track if specified (mp3Track >= 0) and DFPlayer is available
  if (emote.mp3Track >= 0) {
    if (dfPlayerAvailable) {
      Serial.print("Playing MP3 track ");
      Serial.println(emote.mp3Track);
      dfPlayer.play(emote.mp3Track);
    } else {
      Serial.println("MP3 requested but DFPlayer not available");
    }
  }
  
  // Only trigger maestro script if specified (scriptNumber >= 0) and Maestro is available
  if (emote.scriptNumber >= 0) {
    if (maestroAvailable) {
      Serial.print("Activating maestro sequence ");
      Serial.println(emote.scriptNumber);
      maestro.restartScript(emote.scriptNumber);
    } else {
      Serial.println("Maestro script requested but Maestro not available");
    }
  }
}

// Helper function to generate emote button HTML (optimized)
void createEmoteButton(WiFiClient &client, const Emote &emote) {
  // Combine multiple small writes into one for better performance
  String button = String("<a href=\"/maestro/") + emote.path + "\" class=\"button\">" + emote.label + "</a>";
  client.println(button);
}

void loop(){
  WiFiClient client = server.accept();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    client.setNoDelay(true);                // Disable Nagle's algorithm for faster response
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
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
            
            // Handle emote requests (optimized - check path directly)
            for (int i = 0; i < numEmotes; i++) {
              // Build path string once and check
              String searchPath = String("GET /maestro/") + emotes[i].path;
              if (header.indexOf(searchPath) >= 0) {
                setEmote(emotes[i]);
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
              "body { background-color: #1a1a1a; color: #ffffff; padding: 20px; }"
              "h1 { text-align: center; margin-bottom: 30px; font-size: 48px; }"
              ".button-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; max-width: 1200px; margin: 0 auto; }"
              ".button { background-color: "
            );
            client.print(droidcolor);
            client.print(
              "; border: none; border-radius: 12px; color: white; padding: 30px 20px;"
              "text-decoration: none; font-size: 28px; font-weight: bold; cursor: pointer;"
              "transition: all 0.3s; display: block; text-align: center; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }"
              ".button:hover { transform: translateY(-2px); box-shadow: 0 6px 12px rgba(0,0,0,0.4); opacity: 0.9; }"
              ".button:active { transform: translateY(0); box-shadow: 0 2px 4px rgba(0,0,0,0.3); }"
              "</style></head>"
              "<body><h1>"
            );
            client.print(droidname);
            client.print(" Control System</h1><div class=\"button-grid\">");

            // emote buttons
            for (int i = 0; i < numEmotes; i++) {
              createEmoteButton(client, emotes[i]);
            }

            client.println("</div></body></html>");
            
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
    // Ensure all data is sent before closing
    client.flush();
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
