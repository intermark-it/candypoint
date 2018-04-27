/*

  Candy Point
  -----------

  This program wait for a RFID token, then send to Raspberry PI via Serial and if the user associated with this RFID has sufficient points, moves servo to unlock a door.
  Upon the door is closed, which is detected when button is pressed, turns a LED on.

  Data is exchanged in JSON format.
  There are these types of messages:

  {"type":"status","code":"init"} // Inform the raspberry that the arduino is initializing
  {"type":"status","code":"ready"} // Inform the raspberry that the arduino is ready
  {"type":"status","code":"waiting"} // Inform the raspberry that the arduino is waiting for a new RFID
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

// Sensors pins
#define BUTTON 8
#define RED_LED 4
#define GREEN_LED 5
#define SERVO 10
#define RFID_RX 2
#define RFID_TX 3

SoftwareSerial rdm_serial(RFID_RX, RFID_TX);
RDM6300<SoftwareSerial> rdm(&rdm_serial);

// Minimum points required to unlock the door
#define REQUIRED_POINTS 5

// Struct that stores request info
struct Request {
  String rfid;
};

// Struct that stores response info
struct Response {
  String rfid;
  int points;
  int pin;  
};

// Variables
String rfid = ""; // The last RFID identifier readed
bool waiting = true; // Bool variable to control when read a new RFID
bool buttonPressed = true; // Bool variable to control when button is pressed
bool locked = true; // Bool variable to check is door is closed or open

// Functions
void blinkLed(int pin, int n = 1);
String readRFID();
String parseData(unsigned long long data);
void unlock();
void lock();
void sendStatus(String status);
void sendRequest(struct Request *request);
struct Response getResponse();

// Arduino functions ---------------------------------------------------------
void setup() {
  Serial.begin(9600);
  while (!Serial) continue;
  // Send init status
  sendStatus("init");
  // Init leds and button
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUTTON, INPUT);
  // Lock the door (servo)
  lock();
  // Send ready status
  sendStatus("ready");
}

void loop() {
  if (waiting) { // Wait for new rfid
    // Send waiting status
    sendStatus("waiting");
    // Read RFID from the card reader
    rfid = readRFID();
    // Send RFID obtained to Raspberry
    Request request;
    request.rfid = rfid;
    sendRequest(&request);
    // Change waiting state to receive data from Raspberry
    waiting = false;
  } else { // Wait and check response from Raspberry
    if (locked) { // Door is closed
      while (Serial.available() == 0) {}
      Response response = getResponse();
      // Check that result rfid is same that last readed
      if (response.rfid == rfid) {
        if (response.pin > 0) { // Raspberry returns a pin code, it corresponding to new user
          // TODO send pin code to display
          waiting = true;
        } else {
          if (response.points > REQUIRED_POINTS) { // Raspberry response is ok, unlock the door
            unlock();
          } else
            waiting = true;
        }
      } else
        waiting = true;
    } else { // Door is open
      // Check if door is open and button pressed, then lock de door and prepare for a new rfid
      buttonPressed = (digitalRead(BUTTON) == LOW);
      if (buttonPressed) {
        lock();
        waiting = true;
      }      
    }
  }
}

// Internal functions ---------------------------------------------------------

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
  // The "read" method loop until a new rfid is ready
  unsigned long previousMillis = millis();
  do {
    rfid = parseData(rdm.read());
  } while ((millis() - previousMillis) <= 2000);
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
struct Response getResponse() {
  Response response;
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(Serial);
  // It's a json object
  if (root.success()) {
    if (root["type"] == "response") {
      response.rfid = root["result"]["rfid"].as<String>();
      response.points = root["result"]["points"].as<int>();
      response.pin = root["result"]["pin"].as<int>();
    }
  }
  return response;
}

