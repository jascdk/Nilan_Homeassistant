# Make your Nilan Air ventilating system way more cool ;)

This little cool project lets you use you Home Assistant to control and read values from your Nilan air vent system. I have the Nilan Comfort 300 combined with the CTS602 panel. It works great, but do not know if it is compatible with other models.

The code for the project is not developed by me, but I made some adjustmenst to it, so it would integrate better with Home Assistant. The project is originally made for use with OpenHab.

For the original project look here: https://github.com/DanGunvald/NilanModbus

Please proceed this project at your own risk!!!

UPDATE 1/1-2020 : Now added a .ino file for use with a Nilan VPL15 system. Creates some others sensors over the Comfort 300 system. thanks to Martin Skeem for editing / coding :)

## Okay lets get to it!

### Installing the firmware:

I used the arduino editor to upload the code to my ESP8266 (for now a wemos D1 mini). If your sketch wont compile please check if you use the arduino.json V. 5 or V.6 library. This code uses V.5 and wont build with V.6. 

For setting up your wifi and mqtt broker provide your credentials in the configuration.h file


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

`ventilation/temp/#`- This will give the temperatures from all the sensors.

`ventilation/moist/#`- This will give the humidity from the systems humidity sensor.

`ventilation/#` - This gives the output of the system - fan speed etc. Remember the payloads are given in values not text.

### Integrate with Home Assistant.

For my integration i use a package with all my Nilan config yaml in just one file. The file can be downloaded above (config.yaml).

After a restart of Home Assistant you will get alot of new sensors. These can be integrated in Home Assistant in different ways. I use the integrated Lovelace UI to make my UI. You can see below, how it can look like:)

![HA_GUI](https://github.com/jascdk/Nilan_Homeassistant/blob/master/Home%20Assistant/HA_GUI.png)

### Making External displays, that shows you the Nilan Data:

I have tried to make some LCD´s using some 4x16 rows displays together with an ESP32 running ESP-Home www.esphome.io .

If you wanna try it out you can use my provided .yaml code for ESP-Home above:)

### SPECIAL THANKS for contribution to this project goes to: @anderskvist https://github.com/anderskvist :)
















