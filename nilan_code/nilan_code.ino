/**
  Nilan Modbus firmware for D1 Mini (ESP8266) together with a TTL to RS485 Converter https://www.aliexpress.com/item/32836213346.html?spm=a2g0s.9042311.0.0.27424c4dqnr5i7
 
  Written by Dan Gunvald
    https://github.com/DanGunvald/NilanModbus
  Modified to use with Home Assistant by Anders Kvist, Kenn Dyrvig, Jacob Scherrebeck & other great people :)
    https://github.com/anderskvist/Nilan_Homeassistant
    https://github.com/jascdk/Nilan_Homeassistant
   
  Read from a Nilan Air Vent System (Danish Brand) using a Wemos D1
  Mini (or other ESP8266-based board) and report the values to an MQTT
  broker. Then use it for your home-automation system like Home Assistant.
 
  External dependencies. Install using the Arduino library manager:
 
     "Arduino JSON V6 by Benoît Blanchon https://github.com/bblanchon/ArduinoJson - IMPORTANT - Use latest V.6 !!! This code won´t compile with V.5
     "ModbusMaster by Doc Walker https://github.com/4-20ma/ModbusMaster
     "PubSubClient" by Nick O'Leary https://github.com/knolleary/pubsubclient
     
  Project inspired by https://github.com/DanGunvald/NilanModbus
  Join this Danish Facebook Page for inspiration :) https://www.facebook.com/groups/667765647316443/
*/

#include <FS.h>          // this needs to be first, or it all crashes and burns...
#include <LittleFS.h>
#include <WiFiManager.h> // https://github.com/tzapu/wm
#include <DNSServer.h>
#include <ESP8266WebServer.h>

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ModbusMaster.h>
#include <PubSubClient.h>

#include <DoubleResetDetector.h>

#include <Ticker.h>
Ticker ticker;

#define MAXREGSIZE 26
#define SENDINTERVAL 30000 // normally set to 180000 milliseconds = 3 minutes. Define as you like
#define VENTSET 1003
#define RUNSET 1001
#define MODESET 1002
#define TEMPSET 1004
#define PROGRAMSET 500

/*------------------------------------------------------------------------------ 
DOUBLE RESET DETECTOR
------------------------------------------------------------------------------*/

// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

char chipid[12];
WiFiServer server(80);
WiFiClient client;
PubSubClient mqttclient(client);
static long lastMsg = -SENDINTERVAL;
static int16_t rsbuffer[MAXREGSIZE];
ModbusMaster node;
char *usersetTopic1 = "ventilation/relay/userset";  // bruger valg
char *usersetTopic2 = "ventilation/relay/userset2"; // bruger valg

String req[4]; //operation, group, address, value
enum reqtypes
{
  reqtemp = 0,
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
  reqinfo,
  reqinputairtemp,
  reqapp,
  reqoutput,
  reqdisplay1,
  reqdisplay2,
  reqdisplay,
  reqmax
};

String groups[] = {"temp", "alarm", "time", "control", "speed", "airtemp", "airflow", "airheat", "program", "user", "user2", "info", "inputairtemp", "app", "output", "display1", "display2", "display"};
byte regsizes[] = {23, 10, 6, 8, 2, 6, 2, 0, 1, 6, 6, 14, 7, 4, 26, 4, 4, 1};
int regaddresses[] = {200, 400, 300, 1000, 200, 1200, 1100, 0, 500, 600, 610, 100, 1200, 0, 100, 2002, 2007, 3000};
byte regtypes[] = {8, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 2, 1, 4, 4, 8};
char *regnames[][MAXREGSIZE] = {
    //temp
    {"T0_Controller", NULL, NULL, "T3_Exhaust", "T4_Outlet", NULL, NULL, "T7_Inlet", "T8_Outdoor", NULL, NULL, NULL, NULL, NULL, NULL, "T15_Room", NULL, NULL, NULL, NULL, NULL, "RH", NULL},
    //alarm
    {"Status", "List_1_ID", "List_1_Date", "List_1_Time", "List_2_ID", "List_2_Date", "List_2_Time", "List_3_ID", "List_3_Date", "List_3_Time"},
    //time
    {"Second", "Minute", "Hour", "Day", "Month", "Year"},
    //control
    {"Type", "RunSet", "ModeSet", "VentSet", "TempSet", "ServiceMode", "ServicePct", "Preset"},
    //speed
    {"ExhaustSpeed", "InletSpeed"},
    //airtemp
    {"CoolSet", "TempMinSum", "TempMinWin", "TempMaxSum", "TempMaxWin", "TempSummer"},
    //airflow
    {"AirExchMode", "CoolVent"},
    //airheat
    {},
    //program
    {"Program"},
    //program.user
    {"UserFuncAct", "UserFuncSet", "UserTimeSet", "UserVentSet", "UserTempSet", "UserOffsSet"},
    //program.user2
    {"User2FuncAct", "User2FuncSet", "User2TimeSet", "User2VentSet", "UserTempSet", "UserOffsSet"},
    //info
    {"UserFunc", "AirFilter", "DoorOpen", "Smoke", "MotorThermo", "Frost_overht", "AirFlow", "P_Hi", "P_Lo", "Boil", "3WayPos", "DefrostHG", "Defrost", "UserFunc_2"},
    //inputairtemp
    {"IsSummer", "TempInletSet", "TempControl", "TempRoom", "EffPct", "CapSet", "CapAct"},
    //app
    {"Bus.Version", "VersionMajor", "VersionMinor", "VersionRelease"},
    //output
    {"AirFlap", "SmokeFlap", "BypassOpen", "BypassClose", "AirCircPump", "AirHeatAllow", "AirHeat_1", "AirHeat_2", "AirHeat_3", "Compressor", "Compressor_2", "4WayCool", "HotGasHeat", "HotGasCool", "CondOpen", "CondClose", "WaterHeat", "3WayValve", "CenCircPump", "CenHeat_1", "CenHeat_2", "CenHeat_3", "CenHeatExt", "UserFunc", "UserFunc_2", "Defrosting"},
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

JsonObject HandleRequest(JsonDocument &doc)
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
    else
    {
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

void tick()
{
  //toggle state
  int state = digitalRead(BUILTIN_LED); // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);    // set pin to the opposite state
}

//gets called when wm enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "1883";
char mqtt_user[40];
char mqtt_pass[40];

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setupLittleFS()
{
  //clean FS, for testing
  //LittleFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (LittleFS.begin())
  {
    Serial.println("mounted file system");
    if (LittleFS.exists("/config.json"))
    {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        //json.printTo(Serial);
        serializeJson(json, Serial);
        //if (json.success()) {
        if (!deserializeError)
        {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_pass, json["mqtt_pass"]);
        }
        else
        {
          Serial.println("failed to load json config");
        }
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
  }
  //end read
}

void setup()
{
  pinMode(D6, OUTPUT); // bruger valg
  digitalWrite(D6, LOW);
  pinMode(D7, OUTPUT); // bruger valg
  digitalWrite(D7, LOW);
  pinMode(LED_BUILTIN, OUTPUT);
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);
  ArduinoOTA.setHostname("NILAN GATEWAY");
  setupLittleFS();

  // wm, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  //wm.resetSettings();
  
  //set config save notify callback
  wm.setSaveConfigCallback(saveConfigCallback);

  //set config save notify callback
  wm.setAPCallback(configModeCallback);

  // invert theme, dark
  wm.setClass("invert");

  // setup custom parameters
  //
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt_user", mqtt_user, 40);
  WiFiManagerParameter custom_mqtt_pass("pass", "mqtt_pass", mqtt_pass, 40);

  //add all your parameters here
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_port);
  wm.addParameter(&custom_mqtt_user);
  wm.addParameter(&custom_mqtt_pass);

  // Detect Double reset to initiate the Captive portal for configuration
  if (drd.detectDoubleReset())
  {
    //Serial.println("Double Reset Detected");
    wm.startConfigPortal("Nilan Gateway");
  }
  else
  {
    //Serial.println("No Double Reset Detected");
    if (!wm.autoConnect("Nilan Gateway"))
    {
      //Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
    ticker.detach();
    //keep LED on
    digitalWrite(LED_BUILTIN, LOW);
  }

  //if you get here you have connected to the WiFi
  //Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pass, custom_mqtt_pass.getValue());
  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    Serial.println("saving config");
    DynamicJsonDocument json(1024);
    //JsonObject json = jsonDocument.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;

    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile)
    {
      Serial.println("failed to open config file for writing");
    }

    //json.printTo(Serial);
    serializeJson(json, Serial);
    //json.printTo(configFile);
    serializeJson(json, configFile);
    configFile.close();
    //end save
    shouldSaveConfig = false;
  }
  ArduinoOTA.onStart([]() {
  });
  ArduinoOTA.onEnd([]() {
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
  });
  ArduinoOTA.onError([](ota_error_t error) {
  });
  ArduinoOTA.begin();

  Serial.begin(19200, SERIAL_8E1);
  node.begin(30, Serial);

  server.begin();
  mqttclient.setServer(mqtt_server, 1883);
  mqttclient.setCallback(mqttcallback);
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
  if (strcmp(topic, "ventilation/relay/userset") == 0) // bruger valg
  {
    if (payload[0] == '1')
    {
      digitalWrite(D6, HIGH);
      mqttclient.publish("ventilation/relay/userset", "on");
    }
    else if (payload[0] == '0')
    {
      digitalWrite(D6, LOW);
      mqttclient.publish("ventilation/relay/userset", "off");
    }
  }
  if (strcmp(topic, "ventilation/relay/userset2") == 0) // bruger valg
  {
    if (payload[0] == '1')
    {
      digitalWrite(D7, HIGH);
      mqttclient.publish("ventilation/relay/userset2", "on");
    }
    else if (payload[0] == '0')
    {
      digitalWrite(D7, LOW);
      mqttclient.publish("ventilation/relay/userset2", "off");
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
  if (strcmp(topic, "ventilation/programset") == 0)
  {
    if (length == 1 && payload[0] >= '0' && payload[0] <= '4')
    {
      int16_t program = payload[0] - '0';
      WriteModbus(PROGRAMSET, program);
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
  return result;
}

void mqttreconnect()
{
  int numretries = 0;
  while (!mqttclient.connected() && numretries < 3)
  {
    if (mqttclient.connect(chipid, mqtt_user, mqtt_pass))
    {
      mqttclient.subscribe("ventilation/ventset");
      mqttclient.subscribe("ventilation/modeset");
      mqttclient.subscribe("ventilation/runset");
      mqttclient.subscribe("ventilation/tempset");
      mqttclient.subscribe("ventilation/programset");
      mqttclient.subscribe("ventilation/relay/userset");  // bruger valg
      mqttclient.subscribe("ventilation/relay/userset2"); // bruger valg
    }
    else
    {
      delay(1000);
    }
    numretries++;
  }
}

void loop()
{

  drd.stop();

#ifdef DEBUG_TELNET
  // handle Telnet connection for debugging
  handleTelnet();
#endif

  ArduinoOTA.handle();
  WiFiClient client = server.available();
  if (client)
  {
    bool success = readRequest(client);
    if (success)
    {
      StaticJsonDocument<1000> doc;
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
      reqtypes rr[] = {reqtemp, reqcontrol, reqtime, reqoutput, reqspeed, reqalarm, reqinputairtemp, reqprogram, requser, reqdisplay, reqinfo}; // put another register in this line to subscribe
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
              case reqalarm:
                mqname = "ventilation/alarm/"; // Subscribe to the "alarm" register
                itoa((rsbuffer[i]), numstr, 10);
                break;
              case reqinputairtemp:
                mqname = "ventilation/inputairtemp/"; // Subscribe to the "inputairtemp" register
                itoa((rsbuffer[i]), numstr, 10);
                break;
              case reqprogram:
                mqname = "ventilation/weekprogram/"; // Subscribe to the "week program" register
                itoa((rsbuffer[i]), numstr, 10);
                break;
              case requser:
                mqname = "ventilation/user/"; // Subscribe to the "user" register
                itoa((rsbuffer[i]), numstr, 10);
                break;
              case reqinfo:
                mqname = "ventilation/info/"; // Subscribe to the "info" register
                itoa((rsbuffer[i]), numstr, 10);
                break;
              case reqtemp:
                if (strncmp("RH", name, 2) == 0)
                {
                  mqname = "ventilation/moist/"; // Subscribe to moisture-level
                }
                else
                {
                  mqname = "ventilation/temp/"; // Subscribe to "temp" register
                }
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

      // Tell the current state of RELAY1
      if (now - lastMsg > SENDINTERVAL)
      {
        if (digitalRead(D6))
        {
          mqttclient.publish("ventilation/relay/relay1/state", "on");
        }
        else
        {
          mqttclient.publish("ventilation/relay/relay1/state", "off");
        }

        // Tell the current state of RELAY2
        if (now - lastMsg > SENDINTERVAL)
        {
          if (digitalRead(D7))
          {
            mqttclient.publish("ventilation/relay/relay2/state", "on");
          }
          else
          {
            mqttclient.publish("ventilation/relay/relay2/state", "off");
          }

          // Handle text fields
          reqtypes rr2[] = {reqdisplay1, reqdisplay2}; // put another register in this line to subscribe
          for (int i = 0; i < (sizeof(rr2) / sizeof(rr2[0])); i++)
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
}
