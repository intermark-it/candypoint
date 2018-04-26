"""
  Candy Point
  -----------
  This program wait for a RFID token, then send to Raspberry PI via Serial and if the user associated with this RFID has sufficient points, moves servo to unlock a door.
  Upon the door is closed, which is detected when button is pressed, turns a LED on.
  Data is exchanged in JSON format.

  There are these types of messages:
  {"type":"status","code":"init"} // Inform the raspberry that the arduino is initializing
  {"type":"status","code":"ready"} // Inform the raspberry that the arduino is ready
  {"type":"status","code":"waiting"} // Inform the raspberry that the arduino is waiting for a new RFID
  {"type":"request","params":{ "rfid": "0x0000000000"}} // Request received from Arduino to obtain info about a RFID
  
  {"type":"response","result":{ "rfid": "0x0000000000", "points": 100}} // Response sent to Arduino with points associated to a RFID
  {"type":"response","result":{ "rfid": "0x0000000000", "pin": 1234}} // Response sent to Arduino with pin associated to a RFID

  Created 25 Apr 2018
  by Intermark IT
  This code is in the public domain.
"""
import requests
import serial
import json

ser = serial.Serial('/dev/ttyACM0', 9600)
#ser.close()

#Service URL
url = 'https://candy-vault.appspot.com/_ah/api/candy/v1/getUserByRfid'
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

         # GET with JSON 
         r = requests.get(url + "/" + params['rfid'])

         # Response, status etc
         response = {}
         response['type'] = 'response'
         result = {}
         result['rfid'] = params['rfid']

         data = json.loads(r.text)
         if 'items' in data:
            items = data['items'][0]
            active = items['active']
			#check if the user is active
            if active:
               points = items['points']
               #prepare response result with points
               result['points'] = items['points']
               print("You have "+items['points']+" points")
            else:
               #prepare response result with pin
               result['pin'] = items['pin']
               print("Here is your PIN CODE: "+items['pin'])
		 else:
			print("No element ITEMS received from service")

         #Set the response result and send to arduino
         response['result'] = result
         ser.write(json.dumps(response))

         print("Raspberry response: " + json.dumps(response))
      else:
         print("Not RFID code in params")
   else:
      print("Raspberry is waiting")