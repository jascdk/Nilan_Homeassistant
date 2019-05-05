# Make your Nilan Air ventilating system way more cool ;)

This little cool project lets you use you Home Assistant to control and read values from your Nilan air vent system. I have the Nilan Comfort 300 combined with the CTS602 panel. It works great, but do not know if it is compatible with other models.

The code for the project is not developed by me, but I made some adjustmenst to it, so it would integrate better with Home Assistant. The project is originally made for use with OpenHab.

For the original project look here: https://github.com/DanGunvald/NilanModbus

Please proceed this project at your own risk!!!

## Okay lets get to it!

### Installing the firmware:

I used the arduino editor to upload the code to my ESP8266 (for now a wemos D1 mini). If your sketch wont compile please check if you use the arduino.json V. 5 or V.6 library. This code uses V.5 and wont build with V.6. 

In the code you just need to fill in : Wifi SSID, PASSWORD and MQTT Broker IP Adress.


### Setup the Hardware:

For my project i use af Wemos D1 mini board connected to a RS485 board (bought form ali-express). You connect from the Wemos the RX to RX on the RS485 and the TX to TX. This wont work if you cross them.

Then connect the RS485 A,B and GND channel to the corresponding ports on you Nilan Vent System.

### Getting values by HTTP:

You can get some json values from the Nilan by calling to it via HTTP. Just use your browser and type:

`DEVICE` - corresponds to the IP adress you you device (esp8266)

`http://[device]/help` - This should give you som registers

`http://[device]/read/output` - This would for example give you some status of the output

`http://[device]/set/[group]/[adress]/[value]`- This would make you able to send commands through HTTP

### Getting values by MQTT:

Here is where it all shines - the code puts out som useful MQTT topics to monitor different thing.

Any MQTT-Tool (I use on my mac a tool called "MQTT Box") can be used to get the values by subscribing to :

`temp/#`- This will give the temperatures from all the sensors.

`ventilation/#` - This gives the output of the system - fan speed etc. Remember the payloads are given in values not text.

### Integrate with Home Assistant.

For my integration i use a package with all my Nilan config in just one file. The file can be downloaded above.

Put the code inside the Packages folder and you would be able to get something like this after a restart - cool right:):

![HA_GUI](HA_GUI.png)












