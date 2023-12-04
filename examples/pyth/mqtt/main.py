# python 3.6

import random
import time
import matplotlib.pyplot as plt
import os
from paho.mqtt import client as mqtt_client
from plot_data import DataPlot, RealtimePlot

broker = '10.0.0.2'
port = 1883
topic = "pid/p"
# Generate a Client ID with the publish prefix.
client_id = f'publish-{random.randint(0, 1000)}'
username = 'theUser'
password = 'nurEinTraum'

pwmSig = []
powWatt = []
currPWM=0.0
# Create a plot
fig, (ax1,ax2) = plt.subplots(2,figsize=(16,9), gridspec_kw={'height_ratios': [1, 2]})
plt.title('Controller Energy Harvester - PWM/AvailablePower')
#plt.plot(pwmSig, color="red", linewidth=2.0, label="PWM")
plt.xlabel("Zeit")
plt.ylabel("PWM")
plt.legend()

data = DataPlot()
dataPlotting= RealtimePlot(ax1)



count=0

location = 0 # For the best location

def plotAccu():
	global pwmSig
	print("PwmSig: ", pwmSig)
	ax2.plot(pwmSig)	



def connect_mqtt() -> mqtt_client:
    def on_connect(client, userdata, flags, rc):
        if rc == 0:
            print("Connected to MQTT Broker!")
        else:
            print("Failed to connect, return code %d\n", rc)

    client = mqtt_client.Client(client_id)
    # client.username_pw_set(username, password)
    client.on_connect = on_connect
    client.connect(broker, port)
    return client



def on_message_pidP(mosq, obj, msg):
		  print(f" PID p `{msg.payload.decode()}`")

def on_message_pidI(mosq, obj, msg):
		  print(f" PID I `{msg.payload.decode()}`")
def on_message_pidD(mosq, obj, msg):
		  print(f" PID D `{msg.payload.decode()}`")

def subscribe(client: mqtt_client):
	def on_message(client, userdata, msg):
		powW=0.0
		#	print(f"Received `{msg.payload.decode()}` from `{msg.topic}` topic")
		if msg.topic=="energy/pwm":
			global pwmSig,currPWM
			currPWM=float(msg.payload.decode("utf-8"))
			pwmSig.append(currPWM)
			print("received payload for pwm",currPWM);
		if msg.topic=="energy/available":
			powW=float(msg.payload.decode("utf-8"))
			global powWatt 
			global count
			print(" ener avail, pwm: ",currPWM, " avail watt: ",powW);
			count+=1
			data.add(count,currPWM,powW)
			dataPlotting.plot(data)
			plt.pause(0.001)
			if count % 20 == 0:
					  plotAccu()
					  input("enter key")

	client.subscribe([("pid/#", 0), ("energy/#", 0)])
	client.on_message = on_message

def run():
	client = connect_mqtt()
	subscribe(client)
	# Plotten der Werte
	#plt.plot(pwmSig, color="red", linewidth=2.0, label="PWM")
	#plt.xlabel("Zeit")
	#plt.ylabel("PWM")
	#plt.legend()
	#plt.show()
	client.loop_forever()


if __name__ == '__main__':
    run()
