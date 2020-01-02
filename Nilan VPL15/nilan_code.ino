#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ModbusMaster.h>
#include <PubSubClient.h>
#include "configuration.h"
#if SERIAL == SERIAL_SOFTWARE
#include <SoftwareSerial.h>
#endif

#define SERIAL_SOFTWARE 1
#define SERIAL_HARDWARE 2

#define HOST "NilanGW-%s" // Change this to whatever you like. 
#define MAXREGSIZE 28
#define SENDINTERVAL 30000 // normally set to 180000 milliseconds = 3 minutes. Define as you like
#define VENTSET 1003
#define RUNSET 1001
#define MODESET 1002
#define TEMPSET 1004

#if SERIAL == SERIAL_SOFTWAR
SoftwareSerial SSerial(SERIAL_SOFTWARE_RX, SERIAL_SOFTWARE_TX); // RX, TX
#endif

const char* ssid = WIFISSID;
const char* password = WIFIPASSWORD;
char chipid[12];
const char* mqttserver = MQTTSERVER;
const char* mqttusername = MQTTUSERNAME;
const char* mqttpassword = MQTTPASSWORD;
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
  reqalarm,
  reqtime,
  reqcontrol,
  reqspeed,
  reqairtemp,
  reqairflow,
  reqairheat,
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

String groups[] = {"temp", "alarm", "time", "control", "speed", "airtemp", "airflow", "airheat", "user", "user2", "info", "inputairtemp", "app", "output", "display1", "display2", "display"};
byte regsizes[] = {23, 10, 6, 8, 6, 9, 6, 0, 6, 6, 17, 7, 4, 28, 4, 4, 1};
int regaddresses[] = {200, 400, 300, 1000, 200, 1200, 1100, 0, 600, 610, 100, 1200, 0, 100, 2002, 2007, 3000};
byte regtypes[] = {8, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 2, 1, 4, 4, 8};
char *regnames[][MAXREGSIZE] = {
    //temp - Input register 200
    {"T0_Controller", "T1_Intake", "T2_Inlet", "T3_Exhaust", "T4_Outlet", "T5_Cond", "T6_Evap", "T7_Inlet", "T8_Outdoor", "T9_Heater", "T10_Extern", "T11_Top", "T12_Bottom", "T13_Return", "T14_Supply", "T15_Room", "T16", "T17_PreHeat", "T18_PresPibe", "pSuc", "pDis", "RH", "CO2"},
    //alarm - Input register 400
    {"Status", "List_1_ID ", "List_1_Date", "List_1_Time", "List_2_ID ", "List_2_Date", "List_2_Time", "List_3_ID ", "List_3_Date", "List_3_Time"},
    //time - Holding register 300
    {"Second", "Minute", "Hour", "Day", "Month", "Year"},
    //control - Holding register 1000
    {"Type", "RunSet", "ModeSet", "VentSet", "TempSet", "ServiceMode", "ServicePct", "Preset"},
    //speed - Holding register 200
    {"ExhaustSpeed", "InletSpeed","AirHeatCap","CenHeatCap","CprCap","PreHeatCap"},
    //airtemp - Holding register 1200
    {"CoolSet", "TempMinSum", "TempMinWin", "TempMaxSum", "TempMaxWin", "TempSummer","NightDayLim","NightSet","SensorSelect"},
    //airflow - Holding register 1100
    {"AirExchMode", "CoolVent","TestSelect","LastTestDay","TestState","FiltAlmType"},
    //airheat  - Display register 1400
    {},
    //program.user  - Holding register 600 
    {"UserFuncAct", "UserFuncSet", "UserTimeSet", "UserVentSet", "UserTempSet", "UserOffsSet"},
    //program.user2 - Holding register 610
    {"User2FuncAct", "User2FuncSet", "User2TimeSet", "User2VentSet", "UserTempSet", "UserOffsSet"},
    //info - Input register 100
    {"UserFunc", "AirFilter", "DoorOpen", "Smoke", "MotorThermo", "Frost_overht", "AirFlow", "P_Hi", "P_Lo", "Boil", "3WayPos", "DefrostHG", "Defrost", "UserFunc_2","DamperClosed","DamperOpened","FCorThermoAl"},
    //inputairtemp - Input register 1200
    {"IsSummer", "TempInletSet", "TempControl", "TempRoom", "EffPct", "CapSet", "CapAct"},
    //app - Input register 0
    {"Bus.Version", "VersionMajor", "VersionMinor", "VersionRelease"},
    //output - Holding register 100
    {"AirFlap", "SmokeFlap", "BypassOpen", "BypassClose", "AirCircPump", "AirHeatAllow", "AirHeat_1", "AirHeat_2", "AirHeat_3", "Compressor", "Compressor_2", "4WayCool", "HotGasHeat", "HotGasCool", "CondOpen", "CondClose", "WaterHeat", "3WayValve", "CenCircPump", "CenHeat_1", "CenHeat_2", "CenHeat_3", "CenHeatExt", "UserFunc", "UserFunc_2", "Defrosting","AlarmRelay","PreHeat"},
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

JsonObject &HandleRequest(JsonBuffer &jsonBuffer)
{
  JsonObject &root = jsonBuffer.createObject();
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

void setup()
{
  char host[64];
  sprintf(chipid, "%08X", ESP.getChipId());
  sprintf(host, HOST, chipid);
  delay(500);
  WiFi.hostname(host);
  ArduinoOTA.setHostname(host);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    delay(5000);
    ESP.restart();
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
  server.begin();

  #if SERIAL == SERIAL_SOFTWARE
    #warning Compiling for software serial
    SSerial.begin(19200); // SERIAL_8E1
    node.begin(30, SSerial);
  #elif SERIAL == SERIAL_HARDWARE
    #warning Compiling for hardware serial
    Serial.begin(19200, SERIAL_8E1);
    node.begin(30, Serial);
  #else
    #error hardware og serial serial port?
  #endif

  mqttclient.setServer(mqttserver, 1883);
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

void writeResponse(WiFiClient &client, JsonObject &json)
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  json.prettyPrintTo(client);
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
    if (mqttclient.connect(chipid, mqttusername, mqttpassword))
    {
      mqttclient.subscribe("ventilation/ventset");
      mqttclient.subscribe("ventilation/modeset");
      mqttclient.subscribe("ventilation/runset");
      mqttclient.subscribe("ventilation/tempset");
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
      StaticJsonBuffer<500> jsonBuffer;
      JsonObject &json = HandleRequest(jsonBuffer); //prepareResponse(jsonBuffer);
      writeResponse(client, json);
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
      reqtypes rr[] = {reqtemp, reqcontrol, reqoutput, reqspeed, reqalarm, reqinputairtemp, requser, reqdisplay, reqinfo}; // put another register in this line to subscribe
      for (int i = 0; i < (sizeof(rr)/sizeof(rr[0])); i++)
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
              case requser:
                mqname = "ventilation/user/"; // Subscribe to the "user" register
                itoa((rsbuffer[i]), numstr, 10);
                break;     
              case reqinfo:
                mqname = "ventilation/info/"; // Subscribe to the "info" register
                itoa((rsbuffer[i]), numstr, 10);
                break;         
              case reqtemp:
                if (strncmp("RH", name, 2) == 0) {
                  mqname = "ventilation/moist/"; // Subscribe to moisture-level
                } else {
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
        else {
          mqttclient.publish("ventilation/error/modbus/", "1"); //error when connecting through modbus
        }       
      }

      // Handle text fields
      reqtypes rr2[] = {reqdisplay1, reqdisplay2}; // put another register in this line to subscribe
      for (int i = 0; i < (sizeof(rr2)/sizeof(rr2[0])); i++) // change value "5" to how many registers you want to subscribe to
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

              if ((rsbuffer[i] & 0x00ff) == 0xDF) {
                text += (char)0x20; // replace degree sign with space
              } else {
                text += (char)(rsbuffer[i] & 0x00ff);
              }
              if ((rsbuffer[i] >> 8) == 0xDF) {
                text += (char)0x20; // replace degree sign with space
              } else {
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
