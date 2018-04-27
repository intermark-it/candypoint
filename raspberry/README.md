# Raspberry

The Raspberry Python script to communicate with Arduino and Cloud Services

## Python

This script has been build and proved with Python 2.7.14 version

## Getting Started

To ensure a correct use of the script you must install the following python packages:
- pip install pyserial
- pip install requests
- pip install json

## The Script

This program waits for a JSON object from Arduino, reads the JSON and extracts an RFID token, this RFID token is the hexadecimal employeeID. 
Then it sends the RFID token to an API service and waits for a JSON wich tells Raspberry if the employee is active an has points to do an action OR if the employee is not active so it displays a PIN code in the screen.

There are these types of messages:
  {"type":"status","code":"init"} // Inform the raspberry that the arduino is initializing
  {"type":"status","code":"ready"} // Inform the raspberry that the arduino is ready
  {"type":"status","code":"waiting"} // Inform the raspberry that the arduino is waiting for a new RFID
  {"type":"request","params":{ "rfid": "0x0000000000"}} // Request received from Arduino to obtain info about a RFID
  
  {"type":"response","result":{ "rfid": "0x0000000000", "points": 100}} // Response sent to Arduino with points associated to a RFID
  {"type":"response","result":{ "rfid": "0x0000000000", "pin": 1234}} // Response sent to Arduino with pin associated to a RFID