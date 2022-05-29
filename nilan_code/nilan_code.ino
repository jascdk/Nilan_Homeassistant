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

  External dependencies. Install using the Arduino library manager:

     "Arduino JSON V6 by Benoît Blanchon https://github.com/bblanchon/ArduinoJson - IMPORTANT - Use latest V.6 !!! This code won´t compile with V.5
     "ModbusMaster by Doc Walker https://github.com/4-20ma/ModbusMaster
     "PubSubClient" by Nick O'Leary https://github.com/knolleary/pubsubclient

  Project inspired by https://github.com/DanGunvald/NilanModbus

  Join this Danish Facebook Page for inspiration :) https://www.facebook.com/groups/667765647316443/
*/

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ModbusMaster.h>
#include <PubSubClient.h>
#include "configuration.h"
#define SERIAL_SOFTWARE 1
#define SERIAL_HARDWARE 2
#if SERIAL == SERIAL_SOFTWARE
#include <SoftwareSerial.h>
#endif
#define HOST "NilanGW-%s" // Change this to whatever you like.
#define MAXREGSIZE 26
#define SENDINTERVAL 60000 // normally set to 180000 milliseconds = 3 minutes. Define as you like
#define VENTSET 1003
#define RUNSET 1001
#define MODESET 1002
#define TEMPSET 1004
#define PROGRAMSET 500

#if SERIAL == SERIAL_SOFTWARE
SoftwareSerial SSerial(SERIAL_SOFTWARE_RX, SERIAL_SOFTWARE_TX); // RX, TX
#endif

const char *ssid = WIFISSID;
const char *password = WIFIPASSWORD;
char chipid[12];
const char *mqttserver = MQTTSERVER;
const char *mqttusername = MQTTUSERNAME;
const char *mqttpassword = MQTTPASSWORD;
WiFiServer server(80);
WiFiClient client;
PubSubClient mqttclient(client);
static long lastMsg = -SENDINTERVAL;
static int16_t rsbuffer[MAXREGSIZE];
ModbusMaster node;

int16_t AlarmListNumber[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 70, 71, 90, 91, 92};
String AlarmListText[] = {"NONE", "HARDWARE", "TIMEOUT", "FIRE", "PRESSURE", "DOOR", "DEFROST", "FROST", "FROST", "OVERTEMP", "OVERHEAT", "AIRFLOW", "THERMO", "BOILING", "SENSOR", "ROOM LOW", "SOFTWARE", "WATCHDOG", "CONFIG", "FILTER", "LEGIONEL", "POWER", "T AIR", "T WATER", "T HEAT", "MODEM", "INSTABUS", "T1SHORT", "T1OPEN", "T2SHORT", "T2OPEN", "T3SHORT", "T3OPEN", "T4SHORT", "T4OPEN", "T5SHORT", "T5OPEN", "T6SHORT", "T6OPEN", "T7SHORT", "T7OPEN", "T8SHORT", "T8OPEN", "T9SHORT", "T9OPEN", "T10SHORT", "T10OPEN", "T11SHORT", "T11OPEN", "T12SHORT", "T12OPEN", "T13SHORT", "T13OPEN", "T14SHORT", "T14OPEN", "T15SHORT", "T15OPEN", "T16SHORT", "T16OPEN", "ANODE", "EXCH INFO", "SLAVE IO", "OPT IO", "PRESET", "INSTABUS"};

String req[4]; // operation, group, address, value
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
char const *regnames[][MAXREGSIZE] = {
    // temp
    {"T0_Controller", NULL, NULL, "T3_Exhaust", "T4_Outlet", NULL, NULL, "T7_Inlet", "T8_Outdoor", NULL, NULL, NULL, NULL, NULL, NULL, "T15_Room", NULL, NULL, NULL, NULL, NULL, "RH", NULL},
    // alarm
    {"Status", "List_1_ID", "List_1_Date", "List_1_Time", "List_2_ID", "List_2_Date", "List_2_Time", "List_3_ID", "List_3_Date", "List_3_Time"},
    // time
    {"Second", "Minute", "Hour", "Day", "Month", "Year"},
    // control
    {"Type", "RunSet", "ModeSet", "VentSet", "TempSet", "ServiceMode", "ServicePct", "Preset"},
    // speed
    {"ExhaustSpeed", "InletSpeed"},
    // airtemp
    {"CoolSet", "TempMinSum", "TempMinWin", "TempMaxSum", "TempMaxWin", "TempSummer"},
    // airflow
    {"AirExchMode", "CoolVent"},
    // airheat
    {},
    // program
    {"Program"},
    // program.user
    {"UserFuncAct", "UserFuncSet", "UserTimeSet", "UserVentSet", "UserTempSet", "UserOffsSet"},
    // program.user2 requires the optional print board
    {"User2FuncAct", "User2FuncSet", "User2TimeSet", "User2VentSet", "User2TempSet", "User2OffsSet"},
    // info
    {"UserFunc", "AirFilter", "DoorOpen", "Smoke", "MotorThermo", "Frost_overht", "AirFlow", "P_Hi", "P_Lo", "Boil", "3WayPos", "DefrostHG", "Defrost", "UserFunc_2"},
    // inputairtemp
    {"IsSummer", "TempInletSet", "TempControl", "TempRoom", "EffPct", "CapSet", "CapAct"},
    // app
    {"Bus.Version", "VersionMajor", "VersionMinor", "VersionRelease"},
    // output
    {"AirFlap", "SmokeFlap", "BypassOpen", "BypassClose", "AirCircPump", "AirHeatAllow", "AirHeat_1", "AirHeat_2", "AirHeat_3", "Compressor", "Compressor_2", "4WayCool", "HotGasHeat", "HotGasCool", "CondOpen", "CondClose", "WaterHeat", "3WayValve", "CenCircPump", "CenHeat_1", "CenHeat_2", "CenHeat_3", "CenHeatExt", "UserFunc", "UserFunc_2", "Defrosting"},
    // display1
    {"Text_1_2", "Text_3_4", "Text_5_6", "Text_7_8"},
    // display2
    {"Text_9_10", "Text_11_12", "Text_13_14", "Text_15_16"},
    // airbypass
    {"AirBypass/IsOpen"}};

char const *getName(reqtypes type, int address)
{
  if (address >= 0 && address <= regsizes[type])
  {
    return regnames[type][address];
  }
  return NULL;
}

char WriteModbus(uint16_t addr, int16_t val)
{
  node.setTransmitBuffer(0, val);
  char result = 0;
  result = node.writeMultipleRegisters(addr, 1);
  return result;
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

JsonObject HandleRequest(JsonDocument &doc)
{
  JsonObject root = doc.to<JsonObject>();
  reqtypes r = reqmax;
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
  char type = regtypes[r];
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
        char const *name = getName(r, i);
        if (name != NULL && strlen(name) > 0)
        {
          if ((type == 2 && i > 0) || type == 4)
          {
            String str = "";
            str += (char)(rsbuffer[i] & 0x00ff);
            str = (char)(rsbuffer[i] >> 8) + str;            // Remove leading space from one character string
            str.trim();
            root[name] = str;
          }
          else if (type == 8)
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
  else if (req[0] == "set" && req[2] != "" && req[3] != "")
  {
    int address = atoi(req[2].c_str());
    int value = atoi(req[3].c_str());
    char result = WriteModbus(address, value);
    root["result"] = result;
    root["address"] = address;
    root["value"] = value;
  }
  else if (req[0] == "help")
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
      mqttclient.subscribe("ventilation/programset");
    }
    else
    {
      delay(1000);
    }
    numretries++;
  }
}

void mqttcallback(char *topic, byte *payload, unsigned int length)
{
  // Check if topic is equal to string
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
      for (unsigned int i = 0; i < length; i++)
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
  // Fix: To adhere to RFC2616 section 14.13. Calculate length of data to client
  String response = "";
  serializeJsonPretty(doc, response);
  client.print("Content-Length: ");
  client.println(response.length());
  client.println();
  client.print(response);
}

void setup()
{
  char host[64];
  sprintf(chipid, "%08X", ESP.getChipId());
  sprintf(host, HOST, chipid);
#if USE_WIFI_LED
  pinMode(WIFI_LED, OUTPUT);
  digitalWrite(WIFI_LED, LOW); // Reverse meaning. LOW=LED ON
#endif
  delay(500);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(host);
  ArduinoOTA.setHostname(host);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    delay(5000);
    // Give up and do a reboot
    ESP.restart();
  }
#if USE_WIFI_LED
  digitalWrite(WIFI_LED, HIGH);
#endif
  ArduinoOTA.onStart([]() {});
  ArduinoOTA.onEnd([]() {});
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {});
  ArduinoOTA.onError([](ota_error_t error) {});
  ArduinoOTA.begin();
  server.begin();

#if SERIAL == SERIAL_SOFTWARE
#warning Compiling for software serial
  SSerial.begin(19200, SERIAL_8E1);
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
      for (unsigned int i = 0; i < (sizeof(rr) / sizeof(rr[0])); i++)
      {
        reqtypes r = rr[i];
        char result = ReadModbus(regaddresses[r], regsizes[r], rsbuffer, regtypes[r] & 1);
        if (result == 0)
        {
          mqttclient.publish("ventilation/error/modbus/", "0"); // no error when connecting through modbus
          for (int i = 0; i < regsizes[r]; i++)
          {
            char const *name = getName(r, i);
            char numstr[10];
            if (name != NULL && strlen(name) > 0)
            {
              String mqname;
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

                switch (i)
                {
                case 1: // Alarm.List_1_ID
                case 4: // Alarm.List_2_ID
                case 7: // Alarm.List_3_ID
                  if (rsbuffer[i] > 0)
                  {
                    // itoa((rsbuffer[i]), numstr, 10);
                    sprintf(numstr, "UNKNOWN"); // Preallocate unknown if no match if found
                    for (unsigned int p = 0; p < (sizeof(AlarmListNumber)); p++)
                    {
                      if (AlarmListNumber[p] == rsbuffer[i])
                      {
                        //   memset(numstr, 0, sizeof numstr);
                        //   strcpy (numstr,AlarmListText[p].c_str());
                        sprintf(numstr, AlarmListText[p].c_str());
                        break;
                      }
                    }
                  }
                  else
                  {
                    sprintf(numstr, "None"); // No alarm, output None
                  }
                  break;
                case 2: // Alarm.List_1_Date
                case 5: // Alarm.List_2_Date
                case 8: // Alarm.List_3_Date
                  if (rsbuffer[i] > 0)
                  {
                    sprintf(numstr, "%d", (rsbuffer[i] >> 9) + 1980);
                    sprintf(numstr + strlen(numstr), "-%02d", (rsbuffer[i] & 0x1E0) >> 5);
                    sprintf(numstr + strlen(numstr), "-%02d", (rsbuffer[i] & 0x1F));
                  }
                  else
                  {
                    sprintf(numstr, "N/A"); // No alarm, output N/A
                  }
                  break;
                case 3: // Alarm.List_1_Time
                case 6: // Alarm.List_2_Time
                case 9: // Alarm.List_3_Time
                  if (rsbuffer[i] > 0)
                  {
                    sprintf(numstr, "%02d", rsbuffer[i] >> 11);
                    sprintf(numstr + strlen(numstr), ":%02d", (rsbuffer[i] & 0x7E0) >> 5);
                    sprintf(numstr + strlen(numstr), ":%02d", (rsbuffer[i] & 0x11F) * 2);
                  }
                  else
                  {
                    sprintf(numstr, "N/A"); // No alarm, output N/A
                  }

                  break;
                default: // used for Status bit (case 0)
                  itoa((rsbuffer[i]), numstr, 10);
                }
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
          mqttclient.publish("ventilation/error/modbus/", "1"); // error when connecting through modbus
        }
      }

      // Handle text fields
      reqtypes rr2[] = {reqdisplay1, reqdisplay2}; // put another register in this line to subscribe
      for (unsigned int i = 0; i < (sizeof(rr2) / sizeof(rr2[0])); i++)
      {
        reqtypes r = rr2[i];

        char result = ReadModbus(regaddresses[r], regsizes[r], rsbuffer, regtypes[r] & 1);
        if (result == 0)
        {
          String text = "";
          String mqname = "ventilation/text/";

          for (int i = 0; i < regsizes[r]; i++)
          {
            char const *name = getName(r, i);

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
