// Load Wi-Fi library
#include <WiFi.h>

// Maestro library
#include <PololuMaestro.h>
MiniMaestro maestro(Serial1);

// FastLED library
#include <FastLED.h>

// LED Strip Configuration
#define NUM_LEDS 3
#define LED_PIN 5  // Change this to your desired GPIO pin
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

// Emotion definition structure
struct Emotion {
  const char* path;        // URL path (e.g., "sleep")
  const char* label;       // Button label (e.g., "go to sleep")
  const char* colorName;   // Color name for serial output
  CRGB color;              // LED color
  int scriptNumber;        // Maestro script number
};

// Define all emotions in one place
const Emotion emotions[] = {
  {"sleep",   "go to sleep", "Off",    CRGB::Black,  0},
  {"wake",    "wake up",     "White",  CRGB::White,  1},
  {"happy",   "happy",       "Green",  CRGB::Green,  2},
  {"curious", "curious",     "Yellow", CRGB::Yellow, 3},
  {"angry",   "angry",       "Red",    CRGB::Red,    4}
};
const int numEmotions = sizeof(emotions) / sizeof(emotions[0]);

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

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);

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

// Helper function to set emotion state
void setEmotion(const Emotion &emotion) {
  Serial.print("Activating maestro sequence ");
  Serial.print(emotion.scriptNumber);
  Serial.print(" (");
  Serial.print(emotion.path);
  Serial.println(")");
  Serial.print("Eyes ");
  Serial.println(emotion.colorName);
  leds[0] = emotion.color;  // LED 1
  leds[1] = emotion.color;  // LED 2
  leds[2] = CRGB::Black;     // LED 3 always off
  FastLED.show();
  maestro.restartScript(emotion.scriptNumber);
}

// Helper function to generate emotion button HTML
void createEmotionButton(WiFiClient &client, const Emotion &emotion) {
  client.print("<p><a href=\"/maestro/");
  client.print(emotion.path);
  client.print("\"><button class=\"button\">");
  client.print(emotion.label);
  client.println("</button></a></p>");
}

void loop(){
  WiFiClient client = server.accept();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
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
            
            // Handle emotion requests
            for (int i = 0; i < numEmotions; i++) {
              String path = "/maestro/";
              path += emotions[i].path;
              if (header.indexOf("GET " + path) >= 0) {
                setEmotion(emotions[i]);
                break;
              }
            }
            
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>");
            client.println("html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.print(".button { background-color: ");
            client.print(droidcolor);
            client.println("; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println("</style></head>");
            
            // Web Page Heading
            client.print("<body><h1>");
            client.print(droidname);
            client.println(" Control System</h1>");

            // emote buttons
            for (int i = 0; i < numEmotions; i++) {
              createEmotionButton(client, emotions[i]);
            }

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
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
