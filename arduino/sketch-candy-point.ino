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

SoftwareSerial rdm_serial(2, 3);
RDM6300<SoftwareSerial> rdm(&rdm_serial);
Servo servo;

#define REQUIRED_POINTS 5
#define PRETTYPRINT true

// Sensors pins
#define BUTTON 8
#define RED_LED 4
#define GREEN_LED 5
#define SERVO 10

bool locked;

// Blink led "n" times
void blink(int pin, int n = 1) {
  for(int i = 0; i < n; i++) {
    digitalWrite(pin, HIGH);
    delay(200);
    digitalWrite(pin, LOW);
    delay(200);
  }
}

// Convert long to hexadecima string
String parse_data(unsigned long long data) {
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

// Unlock door
void unlock() {
  servo.write(180);
  digitalWrite(RED_LED, LOW);
  blink(GREEN_LED, 3);
  digitalWrite(GREEN_LED, HIGH);
  locked = false;
}

// Lock door
void lock() {
  servo.write(0);  
  digitalWrite(GREEN_LED, LOW);
  blink(RED_LED, 3);
  digitalWrite(RED_LED, HIGH);
  locked = true;
}

// Return true if door is open
bool isOpen() {
  return (locked == false);
}

// Return tru if door is closed
bool isClosed() {
  return (locked == true);
}

// Send Arduino status to Raspberry
void sendStatus(String status) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "status";
  root["code"] = status;
  if (PRETTYPRINT)
    root.prettyPrintTo(Serial);  
  else
    root.printTo(Serial);  
  delay(100);
}

// Send request to Raspberry with RFID readed
void sendRequest(String rfid) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "request";
  JsonObject& params = root.createNestedObject("params");
  params["rfid"] = rfid;
  if (PRETTYPRINT)
    root.prettyPrintTo(Serial);  
  else
    root.printTo(Serial);  
  delay(100);
}

void setup() {
  Serial.begin(9600);
  while (!Serial) continue;
  // Send init state
  sendStatus("init");
  // Init servo, leds and button
  servo.attach(SERVO);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUTTON, INPUT);
  // Lock the door
  lock();
  // Send ready state
  sendStatus("ready");
}

void loop() {
  static unsigned long long data = 0;
  static String rfid = "";
  static bool waiting_rfid = true;
  static bool buttonPressed = false;

  // Wait for new RFID
  if (waiting_rfid) {
    // Send waiting state
    sendStatus("waiting");
    // The "read" method loop until a new RFID is ready
    data = rdm.read();
    // Convert to hexadecimal string identifier
    rfid = parse_data(data);
    // Send RFID obtained
    sendRequest(rfid);
    // Change waiting state to receive data from Raspberry
    waiting_rfid = false;
  } else {
    if (isClosed() && (Serial.available() > 0)) {
      DynamicJsonBuffer jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(Serial);
      // It's a json object
      if (root.success()) {
        String type = root["type"];
        if (type == "response") {
          String rfid = root["result"]["rfid"];
          int pin = root["result"]["pin"];
          int points = root["result"]["points"];
          if (pin > 0) { // Raspberry returns a pin code, it's a RFID corresponding to new user
            //TODO send pin code to display
          } else {
            if (points > REQUIRED_POINTS) { // Raspberry response is OK, unlock the door and blink green led to notify user
              unlock();
            }
          }
        }
      }
    } else {
      // Check if door is open and button pressed, then lock de door and prepare for a new rfid
      buttonPressed = (digitalRead(BUTTON) == LOW);
      if (buttonPressed && isOpen()) {
        waiting_rfid = true;
        lock(); 
        delay(1000);
      }      
    }
  }
}

