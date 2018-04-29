/*

  Candy Point
  -----------

  This program wait for a RFID token, then send to Raspberry PI via Serial and if the user associated with this RFID has sufficient points, moves servo to unlock a door.
  Upon the door is closed, which is detected when button is pressed, turns a LED on.

  Data is exchanged in JSON format.
  There are these types of messages:

  {"type":"status","code":"init"} // Inform the raspberry that the arduino is initializing
  {"type":"status","code":"ready"} // Inform the raspberry that the arduino is ready
  {"type":"status","code":"waiting"} // Inform the raspberry that the arduino is waiting for a response
  {"type":"request","params":{ "rfid": "0x0000000000"}} // Send a request to raspberry to obtain info about a RFID
  {"type":"response","result":{ "rfid": "0x0000000000", "points": 100}} // Response received from raspberry with points associated to a RFID
  {"type":"response","result":{ "rfid": "0x0000000000", "pin": 1234}} // Response received from raspberry with pin associated to a RFID

  Created 22 Apr 2018
  by Intermark IT

  This code is in the public domain.

*/

#include <Servo.h>
#include <SoftwareSerial.h>
#include "RDM6300.h"
#include <ArduinoJson.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiAvrI2c.h"

// Sensors pins
#define BUTTON 8
#define RED_LED 4
#define GREEN_LED 5
#define SERVO 10
#define RFID_RX 2
#define RFID_TX 3

// RDIF reader
SoftwareSerial rdm_serial(RFID_RX, RFID_TX);
RDM6300<SoftwareSerial> rdm(&rdm_serial);

// OLED I2C Address - 0x3C or 0x3D
#define I2C_ADDRESS 0x3C
// Define proper RST_PIN if required
#define RST_PIN -1
// OLED display
SSD1306AsciiAvrI2c oled;

// Struct that stores request info
struct Request {
  String rfid;
} request;

// Struct that stores response info
struct Response {
  String rfid;
  int points;
  int pin;  
} response;

// Variables
bool ready = true; // Bool variable to control when read a new RFID
bool locked = true; // Bool variable to check is door is closed or open

// Functions
void initDisplay();
void blinkLed(int pin, int n = 1);
String readRFID();
String parseData(unsigned long long data);
void unlock();
void lock();
void sendStatus(String status);
void sendRequest(struct Request *request);
struct Response getResponse();
void waitForLock();
void showPin(String pin);
void showPointsAndUnlock(String points);
void displayText(String header, String body);
void displayScrollText(String text, bool endDelay);

// Arduino functions ---------------------------------------------------------
void setup() {
  Serial.begin(9600);
  while (!Serial) continue;
  // Initialize OLED display
  initDisplay();
  // Send init status
  sendStatus("init");
  // Init leds and button
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUTTON, INPUT);
  // Lock the door (servo)
  lock();
}

void loop() {
  if (ready) { // Wait for a new RFID
    // Send ready status
    sendStatus("ready");
    // Read RFID from the card reader
    request.rfid = readRFID();
    // Send RFID obtained to Raspberry
    sendRequest(&request);
    // Change ready state to receive data from Raspberry
    ready = false;
  } else { // Wait and check response from Raspberry
    if (locked) {
      // Send waiting status
      sendStatus("waiting");
      // Get response from Raspberry
      response = getResponse(&request);
      // Check that response is valid (RFID is not empty)
      if (response.rfid != "") {
        if (response.pin > 0) { // Raspberry returns a pin code, it corresponding to a new user
          showPin(response.pin);
        } else { // Raspberry return points, check if sufficient to unlock the door
          showPointsAndUnlock(response.points);
          return;
        }
      }
      // Change ready state to read a new RFID from Arduino
      ready = true;
    } else {
      // Check if button is pressed (close the door and change to ready status)
      waitForLock();
    }
  }
}

// Sketch functions ---------------------------------------------------------

// Initialize OLED display with the I2C addr 0x3C (for the 128x64)
void initDisplay() {
  #if RST_PIN >= 0
    oled.begin(&Adafruit128x64, I2C_ADDRESS, RST_PIN);
  #else // RST_PIN >= 0
    oled.begin(&Adafruit128x64, I2C_ADDRESS);
  #endif // RST_PIN >= 0
}

// Blink led "n" times
void blinkLed(int pin, int n = 1) {
  for(int i = 0; i < n; i++) {
    digitalWrite(pin, HIGH);
    delay(200);
    digitalWrite(pin, LOW);
    delay(200);
  }
}

// Read RFID from the card reader
String readRFID() {
  String rfid = "";
  // The "read" method loop until a new RFID is ready
  unsigned long previousMillis = millis();
  do {
    rfid = parseData(rdm.read());
  } while ((millis() - previousMillis) <= 1000);
  return rfid;
}

// Convert long to hexadecima string
String parseData(unsigned long long data) {
  union {
    unsigned long long ull;
    unsigned long ul[2];
  } tmp;
  tmp.ull = data;
  String output = String(tmp.ul[1], HEX);
  unsigned long beacon = 0x10000000;
  while (beacon > 0) {
    if (tmp.ul[0] < beacon)
      output = output + "0";
    else
      break;
    beacon >>= 4;
  }
  output = "0x" + output + String(tmp.ul[0], HEX);
  return output;
}

// Unlock door (servo)
void unlock() {
  Servo servo;
  servo.attach(SERVO);
  servo.write(180);
  digitalWrite(RED_LED, LOW);
  blinkLed(GREEN_LED, 3);
  digitalWrite(GREEN_LED, HIGH);
  servo.detach();
  ready = false;
  locked = false;
}

// Lock door (servo)
void lock() {
  Servo servo;
  servo.attach(SERVO);
  servo.write(0);  
  digitalWrite(GREEN_LED, LOW);
  blinkLed(RED_LED, 3);
  digitalWrite(RED_LED, HIGH);
  servo.detach();
  ready = true;
  locked = true;
}

// Send Arduino status to Raspberry
void sendStatus(String status) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  String jsonStatus = "";
  root["type"] = "status";
  root["code"] = status;
  root.printTo(jsonStatus);
  Serial.println(jsonStatus);    
  delay(100);
  // Capitalize status to show in display
  status.setCharAt(0, toupper(status.charAt(0)));
  displayText("CANDY POINT", status);
}

// Send request to Raspberry
void sendRequest(struct Request *request) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  String jsonRequest = "";
  root["type"] = "request";
  JsonObject& params = root.createNestedObject("params");
  params["rfid"] = request->rfid;
  root.printTo(jsonRequest);  
  Serial.println(jsonRequest);    
  delay(100);
}

// Get response from Raspberry
struct Response getResponse(struct Request *request) {
  // Wait until data available form Raspberry
  while (Serial.available() == 0) {}
  // Read and parse JSON response
  Response response;
  response.rfid = "";
  response.points = 0;
  response.pin = 0;
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(Serial);
  // It's a json object
  if (root.success()) {
    if (root["type"] == "response") {
      if (request->rfid == root["result"]["rfid"].as<String>()) {
        response.rfid = root["result"]["rfid"].as<String>();
        response.points = root["result"]["points"].as<int>();
        response.pin = root["result"]["pin"].as<int>();
      }
    }
  }
  return response;
}

// If button is pressed, lock the door, and change status to ready for read a new RFID
void waitForLock() {
  static bool buttonPressed = true;
  buttonPressed = (digitalRead(BUTTON) == LOW);
  if (buttonPressed) {
    lock();
  }      
}

// Display pin code in OLED display
void showPin(int pin) {
  displayScrollText("Sorry, you are not registered, please register using the following PIN...", false);
  displayText("PIN", String(pin));
}

// Display remaining points in OLED display and unlock the door if has sufficient points
void showPointsAndUnlock(int points) {
  if (points > -1) { // User has sufficient points
    unlock();
    displayText("POINTS", String(points));
    displayScrollText("Take your candy and...  ENJOY!!!", true);
  } else { // User has insufficient points
    lock();
    displayScrollText("You don't have enough points...  SORRY!!!", true);
  }
}

// Display a message with header and body in OLED display at big font size
void displayText(String header, String body) {
  oled.clear();
  oled.setFont(Adafruit5x7);
  oled.set1X();
  oled.setCursor(oled.displayWidth()/2 - header.length()*6/2, 0);
  oled.println(header);
  oled.println();
  oled.set2X();
  oled.setCursor(oled.displayWidth()/2 - body.length()*12/2, oled.displayHeight()/2);
  oled.println(body);
  delay(3000);
}

// Display scrolling text in OLED display
void displayScrollText(String text, bool endDelay) {
  const int width = 10; // Width of the marquee display (in characters)
  const char termination = '$'; // Special character ($) to determine the end of text
  oled.clear();  
  oled.set2X();
  String message = text + termination;
  // Loop once through the string
  for (int offset = 0; offset < message.length() && message.charAt((offset + width-1) % message.length()) != termination; offset++) {
    oled.clear();  
    // Construct the string to display for this iteration
    String t = "";
    for (int i = 0; i < width; i++)
      t += message.charAt((offset + i) % message.length());
    // Print the string for this iteration
    oled.println();
    oled.println(t);
    if (offset == 0)
      delay(750); // Significant delay before start scrolling
    else
      delay(250); // Short delay so the text doesn't move too fast
  }
  if (endDelay) // After scroll, delay 3 seconds
    delay(3000);
}

