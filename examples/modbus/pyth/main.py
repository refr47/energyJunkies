from pyModbusTCP.client import ModbusClient
from datetime import datetime, time
import random
import json
import time


# datetime object containing current date and time
def current_time():
    now = datetime.now().isoformat()
    return now

host = '10.0.0.7'   #ip of your
port = 502
client = ModbusClient()
client.host = '10.0.0.7'
client.port = 502
client.debug=False
client.auto_open=True
numberOfValues=4
while True:
    rr = client.read_holding_registers(40085,numberOfValues)
    if rr:
        for idx, x in enumerate(rr):
            if x >> 15:
                print("negativ")
            print(idx, format(x))
        print('reg ad #0 to 9: %s' % rr) 
       
    else: 
        print('unable to read registers')
    