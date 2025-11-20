/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/

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

// Replace with your network credentials
const char* ssid = "beskar-5";
const char* password = "mandalore";

// Replace these strings to customize for your droid
String droidname = "Grek";
String droidcolor = "green";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String output26State = "off";
String output27State = "off";



// Assign output variables to GPIO pins
const int output26 = 26;
const int output27 = 27;

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

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

// Helper function to set emotion state
void setEmotion(String emotionName, CRGB color, int scriptNumber) {
  Serial.print("Activating maestro sequence ");
  Serial.print(scriptNumber);
  Serial.print(" (");
  Serial.print(emotionName);
  Serial.println(")");
  Serial.print("Eyes ");
  Serial.println(color == CRGB::Black ? "Off" : 
                 color == CRGB::White ? "White" :
                 color == CRGB::Green ? "Green" :
                 color == CRGB::Yellow ? "Yellow" :
                 color == CRGB::Red ? "Red" : "Unknown");
  leds[0] = color;  // LED 1
  leds[1] = color;  // LED 2
  leds[2] = CRGB::Black;  // LED 3 always off
  FastLED.show();
  maestro.restartScript(scriptNumber);
}

// Helper function to generate emotion button HTML
void createEmotionButton(WiFiClient &client, String emotion, String label) {
  client.println("<p><a href=\"/maestro/" + emotion + "\"><button class=\"button\">" + label + "</button></a></p>");
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
            if (header.indexOf("GET /maestro/sleep") >= 0) {
              setEmotion("sleep", CRGB::Black, 0);
            } else if (header.indexOf("GET /maestro/wake") >= 0) {
              setEmotion("wake", CRGB::White, 1);
            } else if (header.indexOf("GET /maestro/happy") >= 0) {
              setEmotion("happy", CRGB::Green, 2);
            } else if (header.indexOf("GET /maestro/curious") >= 0) {
              setEmotion("curious", CRGB::Yellow, 3);
            } else if (header.indexOf("GET /maestro/angry") >= 0) {
              setEmotion("angry", CRGB::Red, 4);
          }
            
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>");
            client.println("html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: " + droidcolor +"; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println("</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>" + droidname + " Control System</h1>");

            // emote buttons
            createEmotionButton(client, "sleep", "go to sleep");
            createEmotionButton(client, "wake", "wake up");
            createEmotionButton(client, "happy", "happy");
            createEmotionButton(client, "curious", "curious");
            createEmotionButton(client, "angry", "angry");

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
