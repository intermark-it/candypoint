"""
  Candy Point
  -----------
  This program wait for a RFID token, then send to Raspberry PI via Serial and if the user associated with this RFID has sufficient points, moves servo to unlock a door.
  Upon the door is closed, which is detected with a light sensor, turns a LED on.
  Data is exchanged in JSON format.

  There are these types of messages:
  {"type":"status","code":"init"} // Inform the raspberry that the arduino is initializing
  {"type":"status","code":"ready"} // Inform the raspberry that the arduino is ready
  {"type":"status","code":"waiting"} // Inform the raspberry that the arduino is waiting for a new RFID
  {"type":"request","params":{ "rfid": "0x0000000000"}} // Send a request to raspberry to obtain info about a RFID
  {"type":"response","result":{ "rfid": "0x0000000000", "points": 100, "openVault": true/false, "active": true/false, "message": text}} // Response received from raspberry with points associated to a RFID
  {"type":"response","result":{ "rfid": "0x0000000000", "pin": 1234, "openVault": true/false, "active": true/false, "message": text}} // Response received from raspberry with pin associated to a RFID

  Created 25 Apr 2018
  by Intermark IT
  This code is in the public domain.
"""
import requests
import serial
import json

ser = serial.Serial('/dev/ttyACM0', 9600)

#Service URL
url = 'https://candy-vault.appspot.com/_ah/api/candy/v1/user/validate'
#Infinite raspberry waiting loop
while 1 :
   worker = ser.readline()
   request = json.loads(worker)
   #check arduino status: init, ready or waiting
   if 'code' in request:
      if request['code'] == 'init':
         print("Initializing Arduino")
      elif request['code'] == 'ready':
         print("Arduino is ready")
      elif request['code'] == 'waiting':
         print("Arduino is waiting")
   #check request params
   elif 'params' in request:
      #check rfid code in params
      if 'rfid' in request['params']:
         params = request['params']
         print("RFID code: " + params['rfid'])

         # POST with JSON 
         r = requests.post(url + "/" + params['rfid'])

         # Response, status etc
         response = {}
         response['type'] = 'response'
         result = {}
         result['rfid'] = params['rfid']
         data = json.loads(r.text)
         if data:
            result['message'] = data['message']
            result['openVault'] = data['openVault']
            result['active'] = data['active']
            active = data['active']
            if active:
               #prepare response result with points
               result['points'] = data['points']
               print("You have "+data['points']+" points")
            else:
               #prepare response result with pin
               result['pin'] = data['pin']
               print("Here is your PIN CODE: "+data['pin'])
         #Set the response result and send to arduino
         response['result'] = result
         ser.write(json.dumps(response))

         print("Raspberry response: " + json.dumps(response))
      else:
         print("Not RFID code in params")
   else:
      print("Raspberry is waiting")
