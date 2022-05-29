# Unofficial gateway for Nilan ventilation system

This little cool project lets you use you lets you control and read values from your Nilan air vent system. Used with Nilan Comfort 300 LR combined with the CTS602 panel. And should be compatible with other models

![SVG preview of system](images/overview.svg)

This is a fork of https://github.com/jascdk/Nilan_Homeassistant

Which in turn is a fork of https://github.com/DanGunvald/NilanModbus

# How to use:
You can use this either via MQTT messages or via web request

## Web
You can get some json values from the Nilan by calling to it via HTTP. Just use your browser and type:

`http://[ip]/help` - This should give you som registers

`http://[ip]/read/app` - This would for example give you some status of the output

`http://[ip]/get/[adress]/[amountOfAdresessToRead]/[0=InputRegister(default),1=HoldingRegister]`- This would make you able to read raw data from controller 

`http://[ip]/set/[group]/[adress]/[value]`- This would make you able to send commands through HTTP 



e.g

`http://10.0.1.16/read/app` This is a great test info you got a modbus connection as the reads from the safest area of the modbus index. Other commands might fail as controller don't know the status of the requested index.

`http://10.0.1.16/get/610/6/1` Will return read values of addresses 610-615 in holding register range. 

`http://10.0.1.16/set/control/1004/2700` This will set your temperature to 27 degrees. 


## Getting values by MQTT:

There is a lot of topics to be found here. I recommend using "MQTT Explorer" to se what is published.

### Listen

Here are some to listen on:

`ventilation/temp/#`- This will give the temperatures from all the sensors.

`ventilation/moist/#`- This will give the humidity from the systems humidity sensor.

`ventilation/#` - This gives the output of the system - fan speed etc. Remember the payloads are given in values not text.

### Write back

Here are all commands you are able to send back for controlling it. I recommend sending the commands as retained messages to make sure that any faults or reboot of the controller does not affect the outcome. Retained messages are cleared once the command is accepted.

| Command | Input |Description |
| ---   |---| ---|
|`ventilation/ventset`| 0-4 | Set ventilation speed |
|`ventilation/modeset`| 0-4 |Actual operation mode.0=Off, 1=Heat, 2=Cool, 3=Auto, 4=Service |
|`ventilation/runset`| 0-1 | User on / off select (equal to ON/OFF keys) |
|`ventilation/tempset`| 500-2500 | Set temperature the celsius * 100 |
|`ventilation/programset`| 0 - 4 | Start week program index |
|`ventilation/gateway/update`| 1 | Gateway has OTA active always but can be hard to reash if some but in config is made. This puts gateway into OTA update mode for 60  seconds.  |
|`ventilation/gateway/reboot`| 1 | Reboots gateway |


# Installation
Should run on most ESP8266 boards like: wemos D1 mini or nodeMCU.

## Config
Edit configuration.h file to your liking including settings for wifi and mqtt broker.

## Upload to hardware
I recommend using platform IO https://platformio.org/ inside Visual Studio Code as depenencies will be downloaded automatic i most cases due to the `platformio.ini` file.

## Make electrical connection
You can use both a hardware interface or a software one. Both should give you the same result. In theory they both should give the same result but I tent to use the hardware one.

Connect Tx of ESP to Rx on RS485 board. And Rx of ESP to Tx of RS485 board.

RS485 I used: [MAX3485 Module TTL To RS485 Module MCU](https://www.aliexpress.com/item/32828100565.html)
![alt](images/RS485-board.JPG)

This is my setup with the Nilan HMI still connected and working fine:
![alt](images/connection.JPG)

