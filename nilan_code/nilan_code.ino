/**
  Nilan Modbus firmware for D1 Mini (ESP8266) together with a TTL to RS485 Converter https://www.aliexpress.com/item/32836213346.html?spm=a2g0s.9042311.0.0.27424c4dqnr5i7
  
  Written by Dan Gunvald
    https://github.com/DanGunvald/NilanModbus

  Modified to use with Home Assistant by Anders Kvist, Jacob Scherrebeck and other great people :) 
    https://github.com/anderskvist/Nilan_Homeassistant
    https://github.com/jascdk/Nilan_Homeassistant
    
  Read from a Nilan Air Vent System (Danish Brand) using a Wemos D1
  Mini (or other ESP8266-based board) and report the values to an MQTT
  broker. Then use it for your home-automation system like Home Assistant.
  
  This code is currently adopted to be working with the Nilan cts HMI light version. It is slightly different than some of their other hardware boards.
  
  External dependencies. Install using the Arduino library manager:
  
     "Arduino JSON V6 by Benoît Blanchon https://github.com/bblanchon/ArduinoJson - IMPORTANT - Use latest V.6 !!! This code won´t compile with V.5 
     "ModbusMaster by Doc Walker https://github.com/4-20ma/ModbusMaster
     "PubSubClient" by Nick O'Leary https://github.com/knolleary/pubsubclient
     
  Project inspired by https://github.com/DanGunvald/NilanModbus

  Join this Danish Facebook Page for inspiration :) https://www.facebook.com/groups/667765647316443/
*/


#define VERSION "1.6"

/*--------------------------- Configuration ------------------------------*/
// Configuration should be done in the included file:

#include "configuration.h"

/*--------------------------- Libraries ----------------------------------*/

#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ModbusMaster.h>
#include <WiFi.h>
#ifdef ENABLE_ARDUINO_OTA
#include <ArduinoOTA.h>
#endif

/*--------------------------- Global Variables ---------------------------*/
// Nilan Gateway

#define MAXREGSIZE 28
#define SENDINTERVAL 6000 // normally set to 180000 milliseconds = 3 minutes. Define as you like
#define VENTSET 1003
#define RUNSET 1001
#define MODESET 1002
#define TEMPSET 1004
#define SELECTSET 500
#define USERFUNCSET 601
#define USERVENTSET 603
#define USERTIMESET 602
#define USERTEMPSET 604
#define COOLVENT 1101
#define COOLSET 1200

#define WIFI_TIMEOUT_MS 20000

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* mqttserver = MQTT_SERVER;
const char* mqttusername = MQTT_USERNAME;
const char* mqttpassword = MQTT_PASSWORD;

String mqttClientWithMac;

WiFiServer server(80);
WiFiClient client;
PubSubClient mqttclient(client);
static long lastMsg = -SENDINTERVAL;
static int16_t rsbuffer[MAXREGSIZE];
ModbusMaster node;
String req[4]; //operation, group, address, value
enum reqtypes
{
  reqtemp = 0,
  reqtemp1,
  reqtemp2,
  reqmoist,
  reqalarm,
  reqtime,
  reqcontrol,
  reqspeed,
  reqairtemp,
  reqairflow,
  reqairheat,
  reqprogram,
  requser,
  requser2,
  reqactstate,
  reqinfo,
  reqinputairtemp,
  reqapp,
  reqoutput,
  reqdisplay1,
  reqdisplay2,
  reqdisplay,
  reqmax
};
 
String groups[] = {"temp", "temp1", "temp2", "moist", "alarm", "time", "control", "speed", "airtemp", "airflow", "airheat", "program", "user", "user2", "actstate", "info", "inputairtemp",  "app", "output", "display1", "display2", "display"};
byte regsizes[] = {1, 2, 2, 1, 10, 6, 8, 2, 6, 2, 0, 1, 6, 6, 4, 14, 7, 4, 28, 4, 4, 1};
int regaddresses[] = {200, 203, 207, 221, 400, 300, 1000, 200, 1200, 1100, 0, 500, 600, 610, 1000, 100, 1200, 0, 100, 2002, 2007, 3000};
byte regtypes[] = {8, 8, 8, 8, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 8, 2, 1, 4, 4, 8};
char *regnames[][MAXREGSIZE] = {
     //temp
    {"T0_Controller"},
    //temp1
    {"T3_Exhaust", "T4_Outlet"},
    //temp2
    {"T7_Inlet", "T8_Outdoor"},
    //moist
    {"RH"},
    //alarm
    {"Status", "List_1_ID ", "List_1_Date", "List_1_Time", "List_2_ID ", "List_2_Date", "List_2_Time", "List_3_ID ", "List_3_Date", "List_3_Time"},
    //time
    {"Second", "Minute", "Hour", "Day", "Month", "Year"},
    //control
    {"Type", "RunSet", "ModeSet", "VentSet", "TempSet", "ServiceMode", "ServicePct", "Preset"},
    //speed
    {"ExhaustSpeed", "InletSpeed"},
    //airtemp
    {"CoolSet", "TempMinSum", "TempMinWin", "TempMaxSum", "TempMaxWin", "TempSummer"},
    //airflow
    {"VentSet", "InletAct"},
    //airheat
    {},
     //program
    {"Selectset"},
    //program.user
    {"UserFuncAct", "UserFuncSet", "UserTimeSet", "UserVentSet", "UserTempSet", "UserOffsSet"},
    //program.user2
    {"User2FuncAct", "User2FuncSet", "User2TimeSet", "User2VentSet", "UserTempSet", "UserOffsSet"},
    //actstate
    {"RunAct", "ModeAct", "State", "SecInState"},
    //info
    {"UserFunc", "AirFilter", "DoorOpen", "Smoke", "MotorThermo", "Frost_overht", "AirFlow", "P_Hi", "P_Lo", "Boil", "3WayPos", "DefrostHG", "Defrost", "UserFunc_2"},
    //inputairtemp
    {"IsSummer", "TempInletSet", "TempControl", "TempRoom", "EffPct", "CapSet", "CapAct"},
    //app
    {"Bus.Version", "VersionMajor", "VersionMinor", "VersionRelease"},
    //output
    {"AirFlap", "SmokeFlap", "BypassOpen", "BypassClose", "AirCircPump", "AirHeatAllow", "AirHeat_1", "AirHeat_2", "AirHeat_3", "Compressor", "Compressor_2", "4WayCool", "HotGasHeat", "HotGasCool", "CondOpen", "CondClose", "WaterHeat", "3WayValve", "CenCircPump", "CenHeat_1", "CenHeat_2", "CenHeat_3", "CenHeatExt", "UserFunc", "UserFunc_2", "Defrosting", "AlarmRelay", "PreHeat"},
    //display1
    {"Text_1_2", "Text_3_4", "Text_5_6", "Text_7_8"},
    //display2
    {"Text_9_10", "Text_11_12", "Text_13_14", "Text_15_16"},
    //airbypass
    {"AirBypass/IsOpen"}};
   
 
char *getName(reqtypes type, int address)
{
  if (address >= 0 && address <= regsizes[type])
  {
    return regnames[type][address];
  }
  return NULL;
}

/*--------------------------- Function JSON Requests ------------------------*/
 
JsonObject HandleRequest(JsonDocument& doc)
{
  JsonObject root = doc.to<JsonObject>();
  reqtypes r = reqmax;
  char type = 0;
  if (req[1] != "")
  {
    for (int i = 0; i < reqmax; i++)
    {
      if (groups[i] == req[1])
      {
        r = (reqtypes)i;
      }
    }
  }
  type = regtypes[r];
  if (req[0] == "read")
  {
    int address = 0;
    int nums = 0;
    char result = -1;
    address = regaddresses[r];
    nums = regsizes[r];
 
    result = ReadModbus(address, nums, rsbuffer, type & 1);
    if (result == 0)
    {
      root["status"] = "Modbus connection OK";
      for (int i = 0; i < nums; i++)
      {
        char *name = getName(r, i);
        if (name != NULL && strlen(name) > 0)
        {
          if ((type & 2 && i > 0) || type & 4)
          {
            String str = "";
            str += (char)(rsbuffer[i] & 0x00ff);
            str += (char)(rsbuffer[i] >> 8);
            root[name] = str;
          }
          else if (type & 8)
          {
            root[name] = rsbuffer[i] / 100.0;
          }
          else
          {
            root[name] = rsbuffer[i];
          }
        }
      }
    }
    else {
      root["status"] = "Modbus connection failed";
    }
    root["requestaddress"] = address;
    root["requestnum"] = nums;
  }
  if (req[0] == "set" && req[2] != "" && req[3] != "")
  {
    int address = atoi(req[2].c_str());
    int value = atoi(req[3].c_str());
    char result = WriteModbus(address, value);
    root["result"] = result;
    root["address"] = address;
    root["value"] = value;
  }
  if (req[0] == "help")
  {
    for (int i = 0; i < reqmax; i++)
    {
      root[groups[i]] = 0;
    }
  }
  root["operation"] = req[0];
  root["group"] = req[1];
  return root;
}

/*--------------------------- Function related to wifi setup ------------------------*/

void connectToWiFi()
{

  Serial.println("\n\n\r--------------------------------");
  Serial.println("  Nilan Gateway Open Source");
  Serial.println("--------------------------------\n\n");

  Serial.println();
  Serial.print("Nilan Air Gateway starting up, version ");
  Serial.println(VERSION);
  Serial.print("Connecting to WiFi network: " + String(ssid));
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS)
  {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println(" Failed! ");
  }
  else
  {
    Serial.println("\nWiFi connected successfully and assigned IP: " + WiFi.localIP().toString());

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("RSSI:");
    Serial.println(rssi);

    delay(500);
  }
}

/*--------------------------- Main program setup ------------------------*/

void setup()
{
  
  pinMode(4,  OUTPUT); // user - relay

  uint8_t mac[6];
  WiFi.macAddress(mac);

  char macStr[13] = {0};
  sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  mqttClientWithMac = String(MQTT_CLIENT + macStr);

  std::string s_mqtt_port = MQTT_PORT;
  uint16_t i_mqtt_port = atoi(s_mqtt_port.c_str());

  Serial.println("\nHardware initialized, starting program load");
  
  Serial.begin(SERIAL_BAUD_RATE);

  connectToWiFi();

#ifdef ENABLE_ARDUINO_OTA
ArduinoOTA.setHostname("NILAN-GATEWAY-OTA");
  ArduinoOTA
    .onStart([]() {
      Serial.println("ESP OTA:  update start");
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("ESP OTA:  update complete");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
      
    });

  ArduinoOTA.begin();
  Serial.println("ESP OTA:  Over the Air firmware update ready\n");
  #endif

prepareModbus();
  
  server.begin();
  mqttclient.setServer(mqttserver, i_mqtt_port);
  mqttclient.setCallback(mqttcallback);

  Serial.println("Initialization complete\n");
}

void prepareModbus()
{
  Serial.println("Preparing Modbus\n");
  Serial2.begin(19200, SERIAL_8E1);
  node.begin(30, Serial2);
  node.clearResponseBuffer();
  node.clearTransmitBuffer();
}

void mqttcallback(char *topic, byte *payload, unsigned int length)
{
  if (strcmp(topic, "ventilation/ventset") == 0)
  {
    if (length == 1 && payload[0] >= '0' && payload[0] <= '4')
    {
      int16_t speed = payload[0] - '0';
      WriteModbus(VENTSET, speed);
    }
  }
  if (strcmp(topic, "ventilation/modeset") == 0)
  {
    if (length == 1 && payload[0] >= '0' && payload[0] <= '4')
    {
      int16_t mode = payload[0] - '0';
      WriteModbus(MODESET, mode);
    }
  }
  if (strcmp(topic, "ventilation/runset") == 0)
  {
    if (length == 1 && payload[0] >= '0' && payload[0] <= '1')
    {
      int16_t run = payload[0] - '0';
      WriteModbus(RUNSET, run);
    }
  }
  if (strcmp(topic, "ventilation/userset") == 0)
  {
    if (payload[0] == '1')
    {
      digitalWrite(4, HIGH);
    
      mqttclient.publish("ventilation/userset", "on");
    }
    else if (payload[0] == '0')
    {
      digitalWrite(4, LOW);
      
      mqttclient.publish("ventilation/userset", "off");
    }
  }
  if (strcmp(topic, "ventilation/userfuncset") == 0)
  {
    if (length == 1 && payload[0] >= '0' && payload[0] <= '4')
    {
      int16_t select = payload[0] - '0';
      WriteModbus(USERFUNCSET, select);
    }
  }
  if (strcmp(topic, "ventilation/userventset") == 0)
  {
    if (length == 1 && payload[0] >= '0' && payload[0] <= '4')
    {
      int16_t vent = payload[0] - '0';
      WriteModbus(USERVENTSET, vent);
    }
  }
  if (strcmp(topic, "ventilation/coolvent") == 0)
  {
    if (length == 1 && payload[0] >= '0' && payload[0] <= '4')
    {
      int16_t airflow = payload[0] - '0';
      WriteModbus(COOLVENT, airflow);
    }
  }
  if (strcmp(topic, "ventilation/coolset") == 0)
  {
    if (length == 1 && payload[0] >= '0' && payload[0] <= '8')
    {
      int16_t airtemp = payload[0] - '0';
      WriteModbus(COOLSET, airtemp);
    }
  }
  if (strcmp(topic, "ventilation/usertimeset") == 0)
  {
    if (length == 3 && payload[0] >= '0' && payload[0] <= '8')
    {
      String str;
      for (int i = 0; i < length; i++)
      {
        str += (char)payload[i];
      }
      WriteModbus(USERTIMESET, str.toInt());
    }
  }
  if (strcmp(topic, "ventilation/usertempset") == 0)
  {
    if (length == 2 && payload[0] >= '0' && payload[0] <= '2')
    {
      String str;
      for (int i = 0; i < length; i++)
      {
        str += (char)payload[i];
      }
      WriteModbus(USERTEMPSET, str.toInt());
    }
  }
  if (strcmp(topic, "ventilation/tempset") == 0)
  {
    if (length == 4 && payload[0] >= '0' && payload[0] <= '2')
    {
      String str;
      for (int i = 0; i < length; i++)
      {
        str += (char)payload[i];
      }
      WriteModbus(TEMPSET, str.toInt());
    }
  }
  if (strcmp(topic, "ventilation/selectset") == 0)
  {
    if (length == 1 && payload[0] >= '0' && payload[0] <= '4')
    {
      int16_t select = payload[0] - '0';
      WriteModbus(SELECTSET, select);
    }
  }
  lastMsg = -SENDINTERVAL;
}

bool readRequest(WiFiClient &client)
{
  req[0] = "";
  req[1] = "";
  req[2] = "";
  req[3] = "";

  int n = -1;
  bool readstring = false;
  while (client.connected())
  {
    if (client.available())
    {
      char c = client.read();
      if (c == '\n')
      {
        return false;
      }
      else if (c == '/')
      {
        n++;
      }
      else if (c != ' ' && n >= 0 && n < 4)
      {
        req[n] += c;
      }
      else if (c == ' ' && n >= 0 && n < 4)
      {
        return true;
      }
    }
  }

  return false;
}

void writeResponse(WiFiClient &client, const JsonDocument &doc)
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  serializeJsonPretty(doc, client);
}

/*--------------------------- Function related to modbus ------------------------*/

char ReadModbus(uint16_t addr, uint8_t sizer, int16_t *vals, int type)
{
  char result = 0;
  switch (type)
  {
  case 0:
    result = node.readInputRegisters(addr, sizer);
    break;
  case 1:
    result = node.readHoldingRegisters(addr, sizer);
    break;
  }
  if (result == node.ku8MBSuccess)
  {
    for (int j = 0; j < sizer; j++)
    {
      vals[j] = node.getResponseBuffer(j);
    }
    return result;
  }
  return result;
}
char WriteModbus(uint16_t addr, int16_t val)
{
  node.setTransmitBuffer(0, val);
  char result = 0;
  result = node.writeMultipleRegisters(addr, 1);
  if (result == node.ku8MBSuccess)
  {
    Serial.println("Write OK");
  }
  else
  {
    Serial.println("Write NOT OK");
    Serial.println("Clear bufs");
    node.clearResponseBuffer();
    node.clearTransmitBuffer();
  }
  return result;
}

/*--------------------------- Function for reconnecting MQTT ------------------------*/

void mqttreconnect()
{
  int numretries = 0;
  while (!mqttclient.connected() && numretries < 3)
  {
    if (mqttclient.connect(mqttClientWithMac.c_str(), mqttusername, mqttpassword))
    {
      digitalWrite(13, 1);
      mqttclient.subscribe("ventilation/ventset");
      mqttclient.subscribe("ventilation/modeset");
      mqttclient.subscribe("ventilation/runset");
      mqttclient.subscribe("ventilation/tempset");
      mqttclient.subscribe("ventilation/selectset");
      mqttclient.subscribe("ventilation/userset");
      mqttclient.subscribe("ventilation/userfuncset");
      mqttclient.subscribe("ventilation/userventset");
      mqttclient.subscribe("ventilation/usertimeset");
      mqttclient.subscribe("ventilation/usertempset");
      mqttclient.subscribe("ventilation/coolvent");
      mqttclient.subscribe("ventilation/coolset");
    }
    else
    {
      digitalWrite(13, 0);
      delay(1000);
      numretries++;
    }
  }
}

/*--------------------------- Program loop / main loop ------------------------*/

void loop()
{
#ifdef ENABLE_ARDUINO_OTA
  ArduinoOTA.handle();
#endif  
  if (WiFi.status() != WL_CONNECTED)
  {

    if (WiFi.waitForConnectResult() != WL_CONNECTED)
      return;
  }

  if (WiFi.status() == WL_CONNECTED)
  {

    WiFiClient client = server.available();
    if (client)
    {
      bool success = readRequest(client);
      if (success)
      {
        StaticJsonDocument<500> doc;
        HandleRequest(doc);

        writeResponse(client, doc);
      }
      client.stop();
    }

    if (!mqttclient.connected())
    {
      mqttreconnect();
    }

    if (mqttclient.connected())
    {
      mqttclient.loop();
      long now = millis();
      if (now - lastMsg > SENDINTERVAL)
      {
        reqtypes rr[] = {reqtemp, reqtemp1, reqtemp2, reqmoist, reqcontrol, reqtime, reqoutput, reqspeed, reqairtemp, reqalarm, reqinputairtemp, reqprogram, requser, reqdisplay, reqactstate, reqinfo, reqairflow}; // put another register in this line to subscribe
        for (int i = 0; i < (sizeof(rr) / sizeof(rr[0])); i++)
        {
          reqtypes r = rr[i];
          char result = ReadModbus(regaddresses[r], regsizes[r], rsbuffer, regtypes[r] & 1);
          if (result == 0)
          {
            mqttclient.publish("ventilation/error/modbus/", "0"); //no error when connecting through modbus
            for (int i = 0; i < regsizes[r]; i++)
            {
              char *name = getName(r, i);
              char numstr[8];
              if (name != NULL && strlen(name) > 0)
              {
                String mqname = "temp/";
                switch (r)
                {
                case reqcontrol:
                  mqname = "ventilation/control/"; // Subscribe to the "control" register
                  itoa((rsbuffer[i]), numstr, 10);
                  break;
                case reqtime:
                  mqname = "ventilation/time/"; // Subscribe to the "output" register
                  itoa((rsbuffer[i]), numstr, 10);
                  break;
                case reqoutput:
                  mqname = "ventilation/output/"; // Subscribe to the "output" register
                  itoa((rsbuffer[i]), numstr, 10);
                  break;
                case reqdisplay:
                  mqname = "ventilation/display/"; // Subscribe to the "input display" register
                  itoa((rsbuffer[i]), numstr, 10);
                  break;
                case reqspeed:
                  mqname = "ventilation/speed/"; // Subscribe to the "speed" register
                  itoa((rsbuffer[i]), numstr, 10);
                  break;
                case reqairtemp:
                  mqname = "ventilation/airtemp/"; // Subscribe to the "airtemp" register
                  itoa((rsbuffer[i]), numstr, 10);
                  break;
                case reqalarm:
                  mqname = "ventilation/alarm/"; // Subscribe to the "alarm" register
                  itoa((rsbuffer[i]), numstr, 10);
                  break;
                case reqinputairtemp:
                  mqname = "ventilation/inputairtemp/"; // Subscribe to the "inputairtemp" register
                  itoa((rsbuffer[i]), numstr, 10);
                  break;
                case reqprogram:
                  mqname = "ventilation/program/"; // Subscribe to the "program" register
                  itoa((rsbuffer[i]), numstr, 10);
                  break;
                case requser:
                  mqname = "ventilation/user/"; // Subscribe to the "user" register
                  itoa((rsbuffer[i]), numstr, 10);
                  break;
                case reqactstate:
                  mqname = "ventilation/actstate/"; // Subscribe to the "controlact" register
                  itoa((rsbuffer[i]), numstr, 10);
                  break;
                case reqinfo:
                  mqname = "ventilation/info/"; // Subscribe to the "info" register
                  itoa((rsbuffer[i]), numstr, 10);
                  break;
                case reqairflow:
                  mqname = "ventilation/airflow/"; // Subscribe to the "airflow" register
                  itoa((rsbuffer[i]), numstr, 10);
                  break;
                case reqtemp:
                  mqname = "ventilation/temp/"; // Subscribe to "temp" register
                  dtostrf((rsbuffer[i] / 100.0), 5, 2, numstr);
                  break;
                case reqtemp1:
                  mqname = "ventilation/temp/"; // Subscribe to "temp" register
                  dtostrf((rsbuffer[i] / 100.0), 5, 2, numstr);
                  break;
                case reqtemp2:
                  mqname = "ventilation/temp/"; // Subscribe to "temp" register
                  dtostrf((rsbuffer[i] / 100.0), 5, 2, numstr);
                  break;
                case reqmoist:
                    mqname = "ventilation/moist/"; // Subscribe to moisture-level
                  dtostrf((rsbuffer[i] / 100.0), 5, 2, numstr);
                  break;
                }
                mqname += (char *)name;
                mqttclient.publish(mqname.c_str(), numstr);
              }
            }
          }
          else
          {
            mqttclient.publish("ventilation/error/modbus/", "1"); //error when connecting through modbus
          }
        }

        // Handle text fields
        reqtypes rr2[] = {reqdisplay1, reqdisplay2};             // put another register in this line to subscribe
        for (int i = 0; i < (sizeof(rr2) / sizeof(rr2[0])); i++) // change value "5" to how many registers you want to subscribe to
        {
          reqtypes r = rr2[i];

          char result = ReadModbus(regaddresses[r], regsizes[r], rsbuffer, regtypes[r] & 1);
          if (result == 0)
          {
            String text = "";
            String mqname = "ventilation/text/";

            for (int i = 0; i < regsizes[r]; i++)
            {
              char *name = getName(r, i);

              if ((rsbuffer[i] & 0x00ff) == 0xDF)
              {
                text += (char)0x20; // replace degree sign with space
              }
              else
              {
                text += (char)(rsbuffer[i] & 0x00ff);
              }
              if ((rsbuffer[i] >> 8) == 0xDF)
              {
                text += (char)0x20; // replace degree sign with space
              }
              else
              {
                text += (char)(rsbuffer[i] >> 8);
              }
              mqname += (char *)name;
            }
            mqttclient.publish(mqname.c_str(), text.c_str());
          }
        }
        lastMsg = now;
      }
    }
  }
}
